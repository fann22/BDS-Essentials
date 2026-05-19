#pragma once

#include <unordered_set>
#include "mc/world/level/BlockPos.h"

namespace bds_essentials::features::piston_quick_pulse {

struct BlockPosHash {
    size_t operator()(const BlockPos& p) const {
        size_t h = std::hash<int>()(p.x);
        h ^= std::hash<int>()(p.y) + 0x9e3779b9 + (h<<6)+(h>>2);
        h ^= std::hash<int>()(p.z) + 0x9e3779b9 + (h<<6)+(h>>2);
        return h;
    }
};

inline std::unordered_set<BlockPos, BlockPosHash>& getQuickPulseSet() {
    static std::unordered_set<BlockPos, BlockPosHash> s;
    return s;
}

} // namespace bds_essentials