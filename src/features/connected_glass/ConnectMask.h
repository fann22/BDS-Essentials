#pragma once

#include "GlassTypes.h"
#include "TileLookup.h"

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

#include <array>
#include <cstdint>

namespace ConnectedGlass {

constexpr uint8_t CONNECT_NORTH = 0b0001;
constexpr uint8_t CONNECT_SOUTH = 0b0010;
constexpr uint8_t CONNECT_EAST  = 0b0100;
constexpr uint8_t CONNECT_WEST  = 0b1000;

struct CardinalOffset { int dx, dz; uint8_t bit; };
struct DiagonalOffset { int dx, dz; uint8_t ic_bit; uint8_t req1; uint8_t req2; };

inline constexpr std::array<CardinalOffset, 4> CARDINALS = {{
    {  0, -1, CONNECT_NORTH },
    {  0, +1, CONNECT_SOUTH },
    { +1,  0, CONNECT_EAST  },
    { -1,  0, CONNECT_WEST  },
}};

inline constexpr std::array<DiagonalOffset, 4> DIAGONALS = {{
    { +1, -1, IC_NE, CONNECT_NORTH, CONNECT_EAST  },
    { -1, -1, IC_NW, CONNECT_NORTH, CONNECT_WEST  },
    { +1, +1, IC_SE, CONNECT_SOUTH, CONNECT_EAST  },
    { -1, +1, IC_SW, CONNECT_SOUTH, CONNECT_WEST  },
}};

struct MaskResult {
    uint8_t cmask;
    uint8_t icmask;
};

inline MaskResult computeMask(BlockSource& region, BlockPos const& pos, GlassVariant variant) {
    uint8_t cmask  = 0;
    uint8_t icmask = 0;

    for (auto const& nb : CARDINALS) {
        BlockPos npos{ pos.x + nb.dx, pos.y, pos.z + nb.dz };
        auto nv = fromBlock(region.getBlock(npos));
        if (nv.has_value() && canConnect(variant, *nv)) cmask |= nb.bit;
    }

    for (auto const& diag : DIAGONALS) {
        if (!((cmask & diag.req1) && (cmask & diag.req2))) continue;
        BlockPos dpos{ pos.x + diag.dx, pos.y, pos.z + diag.dz };
        auto dv = fromBlock(region.getBlock(dpos));
        if (!dv.has_value() || !canConnect(variant, *dv)) icmask |= diag.ic_bit;
    }

    return { cmask, icmask };
}

} // namespace ConnectedGlass
