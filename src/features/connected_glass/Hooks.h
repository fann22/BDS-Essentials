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

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

namespace ConnectedGlass {

// UpdateBlockPacket inherits ll::PayloadPacket<UpdateBlockPacketPayload>
// ll::PayloadPacket<T> inherits T directly, so cast to payload type to access fields
static void sendFakeBlock(BlockPos const& pos, uint runtimeId) {
    UpdateBlockPacket pkt;
    auto& p          = static_cast<UpdateBlockPacketPayload&>(pkt);
    p.mPos           = pos;
    p.mLayer         = 0;
    p.mUpdateFlags   = 0;
    p.mRuntimeId     = runtimeId;
    pkt.sendToClients();
}

static void updateGlassAt(BlockSource& region, BlockPos const& pos) {
    Block const& block = region.getBlock(pos);
    auto variant = fromBlock(block);
    if (!variant.has_value()) return;

    auto [cmask, icmask] = computeMask(region, pos, *variant);
    auto rid = getRuntimeId(*variant, cmask, icmask);
    if (!rid.has_value()) return;

    sendFakeBlock(pos, *rid);
}

// Hook BlockSource::setBlock — update changed pos + all cardinal + diagonal neighbors
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

    for (auto const& nb : CARDINALS) {
        BlockPos npos{ pos.x + nb.dx, pos.y, pos.z + nb.dz };
        updateGlassAt(*this, npos);
    }

    for (auto const& diag : DIAGONALS) {
        BlockPos dpos{ pos.x + diag.dx, pos.y, pos.z + diag.dz };
        updateGlassAt(*this, dpos);
    }

    return result;
}

// Hook LevelChunkPacket::write — after chunk sent, push UpdateBlockPackets for all glass
LL_TYPE_INSTANCE_HOOK(
    LevelChunkPacketHook,
    ll::memory::HookPriority::Low,
    LevelChunkPacket,
    &LevelChunkPacket::$write,
    void,
    BinaryStream& stream
) {
    origin(stream);

    auto* level = ll::service::getLevel();
    if (!level) return;

    // Convert ChunkPos → block-space origin via BlockPos(ChunkPos, y=0)
    // BlockPos(ChunkPos, y) sets x = chunkX*16, z = chunkZ*16
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
                BlockPos bpos{ cx + lx, y, cz + lz };
                updateGlassAt(*region, bpos);
            }
        }
    }
}

inline void registerHooks() {
    SetBlockHook::hook();
    LevelChunkPacketHook::hook();
}

inline void unregisterHooks() {
    SetBlockHook::unhook();
    LevelChunkPacketHook::unhook();
}

} // namespace ConnectedGlass
