#pragma once

#include "GlassTypes.h"
#include "TileLookup.h"

#include "mc/world/level/block/definition/BlockDefinitionGroup.h"
#include "mc/world/level/block/definition/BlockDescription.h"
#include "mc/world/level/Level.h"

#include <unordered_map>
#include <optional>
#include <string>

namespace ConnectedGlass {

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

            Block const* ds = weakType->mDefaultState;
            if (ds) gRuntimeIdCache[id] = static_cast<uint>(ds->mNetworkId);
        }
    }
}

inline std::optional<uint> lookupRuntimeId(GlassVariant variant, uint8_t cmask, uint8_t icmask) {
    auto it = gRuntimeIdCache.find(blockId(variant, cmask, icmask));
    if (it == gRuntimeIdCache.end()) return std::nullopt;
    return it->second;
}

} // namespace ConnectedGlass
