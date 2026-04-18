// Original from: GMEssentials (https://github.com/GroupMountain/GMEssentials-Release)
// Adapted for: bds_essentials

#include "features/FastLeafDecay.h"

#include "ll/api/base/Containers.h"
#include "ll/api/memory/Hook.h"
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <ll/api/utils/RandomUtils.h>
#include <mc/util/Random.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/block/Block.h>
#include <mc/world/level/block/LeavesBlock.h>
#include <mc/world/level/block/LogBlock.h>
#include <mc/world/level/block/block_events/BlockRandomTickEvent.h>
#include <mc/world/level/levelgen/structure/BoundingBox.h>

namespace bds_essentials::features::fast_leaf_decay {

static constexpr int DECAY_TICKS_MIN = 2;
static constexpr int DECAY_TICKS_MAX = 10;

// Forward declarations
void addLeavesBlock(BlockSource& region, BlockPos const& pos);
bool isLeaves(Block const& block);

static ll::SmallDenseMap<BlockPos, std::shared_ptr<ll::data::CancellableCallback>> gCallbacks;

LL_TYPE_INSTANCE_HOOK(
    LeavesBlockRemoveHook,
    HookPriority::Normal,
    LeavesBlock,
    &LeavesBlock::$onRemove,
    void,
    BlockSource&    region,
    BlockPos const& pos
) {
    origin(region, pos);
    addLeavesBlock(region, pos);
}

LL_TYPE_INSTANCE_HOOK(
    LogBlockRemoveHook,
    HookPriority::Normal,
    LogBlock,
    &LogBlock::$onRemove,
    void,
    BlockSource&    region,
    BlockPos const& pos
) {
    origin(region, pos);
    addLeavesBlock(region, pos);
}

void addLeavesBlock(BlockSource& region, BlockPos const& pos) try {
    for (auto offset : BoundingBox(-1, 1).forEachPos()) {
        auto newPos = pos.add(offset);
        if (!isLeaves(region.getBlock(newPos)) || gCallbacks.contains(newPos)) continue;

        auto* regionPtr = &region;
        gCallbacks[newPos] = ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [regionPtr, newPos]() -> void {
                if (auto& block = regionPtr->getBlock(newPos); isLeaves(block)) {
                    BlockEvents::BlockRandomTickEvent event(newPos, *regionPtr, Random::mThreadLocalRandom());
                    static_cast<LeavesBlock const&>(*block.mBlockType).randomTick(event);
                }
                gCallbacks.erase(newPos);
            },
            ll::chrono::ticks(ll::random_utils::rand(DECAY_TICKS_MIN, DECAY_TICKS_MAX))
        );
    }
} catch (...) {}

bool isLeaves(Block const& block) {
    return (static_cast<uint64>(block.mBlockType->mProperties) & static_cast<uint64>(BlockProperty::Leaves)) != 0;
}

void enable() {
    LeavesBlockRemoveHook::hook();
    LogBlockRemoveHook::hook();
}

void disable() {
    LeavesBlockRemoveHook::unhook();
    LogBlockRemoveHook::unhook();
    for (auto& cb : gCallbacks)
        if (cb.second) cb.second->cancel();
    gCallbacks.clear();
}

} // namespace bds_essentials::features::fast_leaf_decay