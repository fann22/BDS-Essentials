#pragma once

#include "GlassTypes.h"
#include "TileLookup.h"

#include "mc/world/level/block/definition/BlockDefinitionGroup.h"
#include "mc/world/level/block/definition/BlockDescription.h"
#include "mc/world/level/block/registry/BlockTypeRegistry.h"
#include "mc/world/level/Level.h"

#include <unordered_map>
#include <optional>
#include <string>

namespace ConnectedGlass {

// Runtime ID cache: blockId string → network runtime ID
inline std::unordered_map<std::string, uint> gRuntimeIdCache;

inline void registerAll(Level& level) {
    BlockDefinitionGroup* defGroup = level.getBlockDefinitions();
    if (!defGroup) return;

    for (size_t vi = 0; vi < VARIANT_COUNT; ++vi) {
        auto variant = static_cast<GlassVariant>(vi);
        for (auto const& def : TILE_TABLE) {
            std::string id = blockId(variant, def.col, def.row);

            BlockDescription desc;
            desc.mIdentifier      = id;
            desc.mIsBaseGameBlock = false;

            auto weakType = defGroup->registerDataDrivenBlock(desc);
            if (!weakType) continue;

            Block const* defaultState = weakType->mDefaultState;
            if (defaultState) {
                // TypedStorage<4,4,uint> — access via implicit conversion
                gRuntimeIdCache[id] = static_cast<uint>(defaultState->mNetworkId);
            }
        }
    }
}

// Separate name from Block's own methods to avoid any ADL confusion
inline std::optional<uint> lookupRuntimeId(GlassVariant variant, uint8_t cmask, uint8_t icmask) {
    std::string id = blockId(variant, cmask, icmask);
    auto it = gRuntimeIdCache.find(id);
    if (it == gRuntimeIdCache.end()) return std::nullopt;
    return it->second;
}

} // namespace ConnectedGlass
