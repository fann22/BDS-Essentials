#pragma once

#include "GlassTypes.h"
#include "TileLookup.h"
#include "BlockRegistrar.h"
#include "ConnectMask.h"

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/network/packet/UpdateBlockPacketPayload.h"
#include "mc/network/packet/LevelChunkPacket.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/server/ServerLevel.h"

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

namespace ConnectedGlass {

static void sendFakeBlock(BlockPos const& pos, uint runtimeId) {
    UpdateBlockPacket pkt;
    auto& p        = static_cast<UpdateBlockPacketPayload&>(pkt);
    p.mPos         = pos;
    p.mLayer       = 0;
    p.mUpdateFlags = 0;
    p.mRuntimeId   = runtimeId;
    pkt.sendToClients();
}

static void updateGlassAt(BlockSource& region, BlockPos const& pos) {
    Block const& block = region.getBlock(pos);
    auto variant = ConnectedGlass::fromBlock(block);
    if (!variant.has_value()) return;

    auto [cmask, icmask] = ConnectedGlass::computeMask(region, pos, *variant);

    auto rid = ConnectedGlass::lookupRuntimeId(*variant, cmask, icmask);
    if (!rid.has_value()) return;

    sendFakeBlock(pos, *rid);
}

// Hook BlockSource::setBlock
LL_TYPE_INSTANCE_HOOK(
    SetBlockHook,
    ll::memory::HookPriority::Low,
    BlockSource,
    &BlockSource::$setBlock,
    bool,
    BlockPos const&              pos,
    Block const&                 block,
    int                          updateFlags,
    ActorBlockSyncMessage const* syncMsg,
    BlockChangeContext const&    changeSourceContext
) {
    bool result = origin(pos, block, updateFlags, syncMsg, changeSourceContext);
    if (!result) return result;

    updateGlassAt(*this, pos);

    for (auto const& nb : ConnectedGlass::CARDINALS) {
        updateGlassAt(*this, BlockPos{ pos.x + nb.dx, pos.y, pos.z + nb.dz });
    }
    for (auto const& diag : ConnectedGlass::DIAGONALS) {
        updateGlassAt(*this, BlockPos{ pos.x + diag.dx, pos.y, pos.z + diag.dz });
    }

    return result;
}

// Hook LevelChunkPacket::write — scan chunk column after send
LL_TYPE_INSTANCE_HOOK(
    LevelChunkPacketHook,
    ll::memory::HookPriority::Low,
    LevelChunkPacket,
    &LevelChunkPacket::$write,
    void,
    BinaryStream& stream
) {
    origin(stream);

    // optional_ref<Level> — check with has_value(), get pointer via ->
    auto levelRef = ll::service::getLevel();
    if (!levelRef.has_value()) return;
    Level* level = levelRef.as_ptr();
    if (!level) return;

    // BlockPos(ChunkPos, y) → x=chunkX*16, z=chunkZ*16
    BlockPos chunkOrigin{ this->mPos, 0 };
    int const cx = chunkOrigin.x;
    int const cz = chunkOrigin.z;

    BlockSource* region = nullptr;
    level->forEachPlayer([&](Player& player) -> bool {
        region = &player.getDimensionBlockSource();
        return false;
    });
    if (!region) return;

    constexpr int CHUNK_SIZE  = 16;
    constexpr int WORLD_MIN_Y = -64;
    constexpr int WORLD_MAX_Y = 319;

    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            for (int y = WORLD_MIN_Y; y <= WORLD_MAX_Y; ++y) {
                updateGlassAt(*region, BlockPos{ cx + lx, y, cz + lz });
            }
        }
    }
}

// Hook ServerLevel::initialize — fires once when level is fully loaded
// Replaces ll::event::LevelInitEvent which is not available in LL 26.x
LL_TYPE_INSTANCE_HOOK(
    LevelInitHook,
    ll::memory::HookPriority::Normal,
    ServerLevel,
    &ServerLevel::$initialize,
    bool,
    std::string const&   levelName,
    LevelSettings const& levelSettings,
    Experiments const&   experiments,
    std::string const*   levelId,
    std::optional<std::reference_wrapper<class StorageVersion>> storageVersion
) {
    bool result = origin(levelName, levelSettings, experiments, levelId, storageVersion);
    if (result) {
        ConnectedGlass::registerAll(*this);
    }
    return result;
}

inline void registerHooks() {
    SetBlockHook::hook();
    LevelChunkPacketHook::hook();
    LevelInitHook::hook();
}

inline void unregisterHooks() {
    SetBlockHook::unhook();
    LevelChunkPacketHook::unhook();
    LevelInitHook::unhook();
}

} // namespace ConnectedGlass
