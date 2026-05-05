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
#include "mc/world/level/LevelSettings.h"
#include "mc/world/level/Experiments.h"

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
    for (auto const& nb : ConnectedGlass::CARDINALS)
        updateGlassAt(*this, BlockPos{ pos.x + nb.dx, pos.y, pos.z + nb.dz });
    for (auto const& diag : ConnectedGlass::DIAGONALS)
        updateGlassAt(*this, BlockPos{ pos.x + diag.dx, pos.y, pos.z + diag.dz });

    return result;
}

LL_TYPE_INSTANCE_HOOK(
    LevelChunkPacketHook,
    ll::memory::HookPriority::Low,
    LevelChunkPacket,
    &LevelChunkPacket::$write,
    void,
    BinaryStream& stream
) {
    origin(stream);

    auto levelRef = ll::service::getLevel();
    if (!levelRef) return;

    BlockPos chunkOrigin{ this->mPos, 0 };
    int const cx = chunkOrigin.x;
    int const cz = chunkOrigin.z;

    BlockSource* region = nullptr;
    levelRef->forEachPlayer([&](Player& player) -> bool {
        region = &player.getDimensionBlockSource();
        return false;
    });
    if (!region) return;

    constexpr int S = 16, MINY = -64, MAXY = 319;
    for (int lx = 0; lx < S; ++lx)
        for (int lz = 0; lz < S; ++lz)
            for (int y = MINY; y <= MAXY; ++y)
                updateGlassAt(*region, BlockPos{ cx + lx, y, cz + lz });
}

// Use exact signature from ServerLevel.h
LL_TYPE_INSTANCE_HOOK(
    LevelInitHook,
    ll::memory::HookPriority::Normal,
    ServerLevel,
    &ServerLevel::$initialize,
    bool,
    ::std::string const&   levelName,
    ::LevelSettings const& levelSettings,
    ::Experiments const&   experiments,
    ::std::string const*   levelId,
    ::std::optional<::std::reference_wrapper<
        ::std::unordered_map<::std::string, ::std::unique_ptr<::BiomeJsonDocumentGlueResolvedBiomeData>>>>
        biomeIdToResolvedData
) {
    bool result = origin(levelName, levelSettings, experiments, levelId, biomeIdToResolvedData);
    if (result) ConnectedGlass::registerAll(*this);
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
