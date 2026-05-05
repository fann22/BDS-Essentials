#pragma once

#include "GlassTypes.h"
#include "TileLookup.h"

#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/definition/BlockDefinitionGroup.h"
#include "mc/world/level/block/definition/BlockDescription.h"
#include "mc/world/level/block/definition/BlockStateDefinition.h"
#include "mc/world/level/Level.h"

#include <string>
#include <unordered_map>

namespace ConnectedGlass {

// Cache: block ID string → network runtime ID
// Populated during registerAll(), used by Hooks to build UpdateBlockPacket
inline std::unordered_map<std::string, uint>& runtimeIdCache() {
    static std::unordered_map<std::string, uint> cache;
    return cache;
}

// Register all connected glass custom blocks.
// One block per (variant, tile) combination — named bdse:cglass_{variant}_{col}_{row}
// Called once at LevelInitEvent.
inline void registerAll(Level& level) {
    BlockDefinitionGroup* defGroup = level.getBlockDefinitions();
    if (!defGroup) return;

    auto& cache = runtimeIdCache();

    for (size_t vi = 0; vi < VARIANT_COUNT; ++vi) {
        auto variant = static_cast<GlassVariant>(vi);

        for (auto const& [key, tilePos] : tileTable()) {
            std::string id = "bdse:cglass_"
                           + variantName(variant)
                           + "_" + std::to_string(tilePos.col)
                           + "_" + std::to_string(tilePos.row);

            BlockDescription desc;
            desc.mIdentifier      = id;
            desc.mIsBaseGameBlock = false;

            auto weakType = defGroup->registerDataDrivenBlock(desc);
            if (!weakType) continue;

            Block const* defaultState = weakType->mDefaultState;
            if (defaultState) {
                cache[id] = defaultState->mNetworkId;
            }
        }
    }
}

// Look up runtime ID for a given variant + masks.
// Returns 0 if not found.
inline uint getRuntimeId(GlassVariant variant, uint8_t cmask, uint8_t icmask) {
    auto [col, row] = lookupTile(cmask, icmask);
    std::string id = "bdse:cglass_"
                   + variantName(variant)
                   + "_" + std::to_string(col)
                   + "_" + std::to_string(row);

    auto& cache = runtimeIdCache();
    auto it = cache.find(id);
    if (it == cache.end()) return 0;
    return it->second;
}

} // namespace ConnectedGlass
