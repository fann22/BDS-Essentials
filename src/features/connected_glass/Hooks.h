#pragma once

#include "GlassTypes.h"
#include "BlockRegistrar.h"
#include "ConnectMask.h"

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"
#include "mc/network/packet/UpdateBlockPacket.h"
#include "mc/network/packet/UpdateBlockPacketPayload.h"
#include "mc/network/packet/LevelChunkPacket.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/BlockChangedEventTarget.h"

#include "ll/api/memory/Hook.h"

namespace ConnectedGlass {

// Send a spoofed UpdateBlockPacket to all clients for a single position.
// runtimeId is the custom block's network ID.
static void sendFakeBlock(BlockPos const& pos, uint runtimeId) {
    auto pkt             = std::make_unique<UpdateBlockPacket>();
    pkt->mPayload.mPos        = pos;
    pkt->mPayload.mLayer      = 0;
    pkt->mPayload.mUpdateFlags = 0;
    pkt->mPayload.mRuntimeId  = runtimeId;
    pkt->sendToClients();
}

// Update a single glass position: compute mask, look up custom block ID, broadcast.
static void updateGlassAt(BlockSource& region, BlockPos const& pos) {
    Block const& block = region.getBlock(pos);
    auto variant = fromBlock(block);
    if (!variant.has_value()) return;

    auto [cmask, icmask] = computeMask(region, pos, *variant);

    uint rid = getRuntimeId(*variant, cmask, icmask);
    if (rid == 0) return;

    sendFakeBlock(pos, rid);
}

// Hook BlockSource::setBlock (virtual slot 0 override).
// After a block change we update the changed pos + all 4 horizontal neighbors.
LL_TYPE_INSTANCE_HOOK(
    SetBlockHook,
    ll::memory::HookPriority::Low,   // run after vanilla logic
    BlockSource,
    &BlockSource::$setBlock,         // virtual thunk
    bool,
    BlockPos const&              pos,
    Block const&                 block,
    int                          updateFlags,
    ActorBlockSyncMessage const* syncMsg,
    BlockChangeContext const&    changeSourceContext
) {
    bool result = origin(pos, block, updateFlags, syncMsg, changeSourceContext);
    if (!result) return result;

    // Check the new block and its 4 horizontal neighbors
    // (placing glass A changes the mask of adjacent glass B, C, etc.)
    updateGlassAt(*this, pos);

    for (auto const& nb : HORIZONTAL_NEIGHBORS) {
        BlockPos neighborPos{pos.x + nb.dx, pos.y, pos.z + nb.dz};
        updateGlassAt(*this, neighborPos);
    }

    return result;
}

// Hook LevelChunkPacket::write — patch serialized chunk data before it hits the wire.
// This handles the initial chunk send when a player enters range.
//
// NOTE: mSerializedChunk is a raw NBT/LevelDB blob that encodes block runtime IDs.
// Directly patching binary chunk data is complex and version-sensitive.
// The recommended approach here is to intercept the packet, deserialize,
// swap all glass runtime IDs → custom block runtime IDs, then re-serialize.
//
// For now we hook write() and queue per-block UpdateBlockPackets for every glass
// block in that chunk after it's sent — simpler and more robust than binary patching.
LL_TYPE_INSTANCE_HOOK(
    LevelChunkPacketHook,
    ll::memory::HookPriority::Low,
    LevelChunkPacket,
    &LevelChunkPacket::$write,
    void,
    BinaryStream& stream
) {
    // Let vanilla write the real chunk data first
    origin(stream);

    // After the chunk packet is sent, send UpdateBlockPackets for every
    // glass block in the chunk column so clients see the correct connected texture.
    //
    // We need the Level to iterate a BlockSource for this chunk's dimension.
    // Get level via ll::service
    auto* level = ll::service::getLevel();
    if (!level) return;

    // mPos is ChunkPos (chunk-space). Block-space origin = (cx*16, miny, cz*16)
    int const cx = this->mPos.x;
    int const cz = this->mPos.z;

    // Iterate over all players; find one in the same dimension to borrow its BlockSource
    // (We only need to touch blocks, not send per-player — sendToClients() handles broadcast)
    BlockSource* region = nullptr;
    level->forEachPlayer([&](Player& player) -> bool {
        // Use the first available player's BlockSource
        region = &player.getDimensionBlockSource();
        return false; // stop iterating
    });

    if (!region) return;

    // Walk all blocks in the 16x256x16 column
    // (BDS world height range: typically -64 to 319, but use BlockSource bounds)
    constexpr int CHUNK_SIZE = 16;
    constexpr int WORLD_MIN_Y = -64;
    constexpr int WORLD_MAX_Y = 319;

    for (int lx = 0; lx < CHUNK_SIZE; ++lx) {
        for (int lz = 0; lz < CHUNK_SIZE; ++lz) {
            for (int y = WORLD_MIN_Y; y <= WORLD_MAX_Y; ++y) {
                BlockPos pos{cx * CHUNK_SIZE + lx, y, cz * CHUNK_SIZE + lz};
                Block const& block = region->getBlock(pos);
                auto variant = fromBlock(block);
                if (!variant.has_value()) continue;

                auto [cmask, icmask] = computeMask(*region, pos, *variant);
                uint rid = getRuntimeId(*variant, cmask, icmask);
                if (rid == 0) continue;

                sendFakeBlock(pos, rid);
            }
        }
    }
}

// Register all hooks
inline void registerHooks() {
    SetBlockHook::hook();
    LevelChunkPacketHook::hook();
}

inline void unregisterHooks() {
    SetBlockHook::unhook();
    LevelChunkPacketHook::unhook();
}

} // namespace ConnectedGlass
