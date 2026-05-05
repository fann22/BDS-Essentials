#pragma once

#include "GlassTypes.h"

#include <array>
#include <string>
#include <cstdint>

namespace ConnectedGlass {

constexpr uint8_t IC_NE = 0b0001;
constexpr uint8_t IC_NW = 0b0010;
constexpr uint8_t IC_SE = 0b0100;
constexpr uint8_t IC_SW = 0b1000;

struct TilePos {
    int col;
    int row;
};

struct TileDef {
    uint8_t cmask;
    uint8_t icmask;
    int     col;
    int     row;
};

// clang-format off
inline constexpr std::array<TileDef, 43> TILE_TABLE = {{
    { 0b0000, 0b0000, 0, 0 },
    { 0b0001, 0b0000, 1, 0 }, { 0b0010, 0b0000, 2, 0 },
    { 0b0100, 0b0000, 3, 0 }, { 0b1000, 0b0000, 4, 0 },
    { 0b0011, 0b0000, 0, 1 }, { 0b1100, 0b0000, 1, 1 },
    { 0b0101, 0b0000, 0, 2 }, { 0b1001, 0b0000, 1, 2 },
    { 0b0110, 0b0000, 2, 2 }, { 0b1010, 0b0000, 3, 2 },
    { 0b0101, IC_NE,          0, 3 }, { 0b1001, IC_NW,          1, 3 },
    { 0b0110, IC_SE,          2, 3 }, { 0b1010, IC_SW,          3, 3 },
    { 0b1101, 0b0000,         0, 4 }, { 0b1110, 0b0000,         1, 4 },
    { 0b0111, 0b0000,         2, 4 }, { 0b1011, 0b0000,         3, 4 },
    { 0b1101, IC_NE|IC_NW,    0, 5 }, { 0b1110, IC_SE|IC_SW,    1, 5 },
    { 0b0111, IC_NE|IC_SE,    2, 5 }, { 0b1011, IC_NW|IC_SW,    3, 5 },
    { 0b1101, IC_NE,          4, 5 }, { 0b1101, IC_NW,          5, 5 },
    { 0b1110, IC_SE,          6, 5 }, { 0b1110, IC_SW,          7, 5 },
    { 0b1111, 0b0000,                   0, 6 },
    { 0b1111, IC_NE,                    1, 6 }, { 0b1111, IC_NW,         2, 6 },
    { 0b1111, IC_SE,                    3, 6 }, { 0b1111, IC_SW,         4, 6 },
    { 0b1111, IC_NE|IC_SW,              5, 6 }, { 0b1111, IC_NW|IC_SE,  6, 6 },
    { 0b1111, IC_NE|IC_NW,              7, 6 },
    { 0b1111, IC_SE|IC_SW,              0, 7 }, { 0b1111, IC_NE|IC_SE,  1, 7 },
    { 0b1111, IC_NW|IC_SW,              2, 7 },
    { 0b1111, IC_NE|IC_NW|IC_SE,        3, 7 },
    { 0b1111, IC_NE|IC_NW|IC_SW,        4, 7 },
    { 0b1111, IC_NE|IC_SE|IC_SW,        5, 7 },
    { 0b1111, IC_NW|IC_SE|IC_SW,        6, 7 },
    { 0b1111, IC_NE|IC_NW|IC_SE|IC_SW,  7, 7 },
}};
// clang-format on

inline TilePos lookupTile(uint8_t cmask, uint8_t icmask) {
    for (auto const& def : TILE_TABLE) {
        if (def.cmask == cmask && def.icmask == icmask) {
            return { def.col, def.row };
        }
    }
    return { 0, 0 };
}

inline std::string blockId(GlassVariant variant, uint8_t cmask, uint8_t icmask) {
    auto [col, row] = lookupTile(cmask, icmask);
    return "bdse:cglass_" + variantName(variant)
         + "_" + std::to_string(col)
         + "_" + std::to_string(row);
}

inline std::string blockId(GlassVariant variant, int col, int row) {
    return "bdse:cglass_" + variantName(variant)
         + "_" + std::to_string(col)
         + "_" + std::to_string(row);
}

} // namespace ConnectedGlass
