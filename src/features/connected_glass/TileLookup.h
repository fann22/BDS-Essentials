#pragma once

#include "GlassTypes.h"

#include <array>
#include <optional>
#include <string>
#include <unordered_map>

namespace ConnectedGlass {

// ── Tile entry ────────────────────────────────────────────────────────────────

struct TilePos {
    int col;
    int row;
};

// ── Lookup table (mirrors TILE_DEFS in gen_glass_atlas_smart.py) ─────────────
// Key: (cardinal_mask << 4) | inner_corner_mask  → packed uint8
// Value: TilePos

inline uint8_t tileKey(uint8_t cmask, uint8_t icmask) {
    return (cmask & 0xF) | ((icmask & 0xF) << 4);
}

// clang-format off
inline const std::unordered_map<uint8_t, TilePos>& tileTable() {
    static const std::unordered_map<uint8_t, TilePos> TABLE = {
        // ── Row 0: isolated + single side ────────────────────────────────────
        { tileKey(0,           0          ), {0, 0} },  // isolated
        { tileKey(N,           0          ), {1, 0} },  // N
        { tileKey(S,           0          ), {2, 0} },  // S
        { tileKey(E,           0          ), {3, 0} },  // E
        { tileKey(W,           0          ), {4, 0} },  // W

        // ── Row 1: straight through ───────────────────────────────────────────
        { tileKey(N|S,         0          ), {0, 1} },  // N+S
        { tileKey(E|W,         0          ), {1, 1} },  // E+W

        // ── Row 2: L-shape, no inner corner ──────────────────────────────────
        { tileKey(N|E,         0          ), {0, 2} },  // N+E outer
        { tileKey(N|W,         0          ), {1, 2} },  // N+W outer
        { tileKey(S|E,         0          ), {2, 2} },  // S+E outer
        { tileKey(S|W,         0          ), {3, 2} },  // S+W outer

        // ── Row 3: L-shape WITH inner corner ─────────────────────────────────
        { tileKey(N|E,         NE_IC      ), {0, 3} },  // N+E ic_NE
        { tileKey(N|W,         NW_IC      ), {1, 3} },  // N+W ic_NW
        { tileKey(S|E,         SE_IC      ), {2, 3} },  // S+E ic_SE
        { tileKey(S|W,         SW_IC      ), {3, 3} },  // S+W ic_SW

        // ── Row 4: T-shape, no inner corners ─────────────────────────────────
        { tileKey(N|E|W,       0          ), {0, 4} },  // N+E+W outer
        { tileKey(S|E|W,       0          ), {1, 4} },  // S+E+W outer
        { tileKey(N|S|E,       0          ), {2, 4} },  // N+S+E outer
        { tileKey(N|S|W,       0          ), {3, 4} },  // N+S+W outer

        // ── Row 5: T-shape WITH inner corners ────────────────────────────────
        { tileKey(N|E|W,       NE_IC|NW_IC), {0, 5} },  // N+E+W ic_NE+NW
        { tileKey(S|E|W,       SE_IC|SW_IC), {1, 5} },  // S+E+W ic_SE+SW
        { tileKey(N|S|E,       NE_IC|SE_IC), {2, 5} },  // N+S+E ic_NE+SE
        { tileKey(N|S|W,       NW_IC|SW_IC), {3, 5} },  // N+S+W ic_NW+SW
        { tileKey(N|E|W,       NE_IC      ), {4, 5} },  // N+E+W ic_NE
        { tileKey(N|E|W,       NW_IC      ), {5, 5} },  // N+E+W ic_NW
        { tileKey(S|E|W,       SE_IC      ), {6, 5} },  // S+E+W ic_SE
        { tileKey(S|E|W,       SW_IC      ), {7, 5} },  // S+E+W ic_SW

        // ── Row 6: cross/plus, varying inner corners ──────────────────────────
        { tileKey(N|S|E|W,     0                        ), {0, 6} },
        { tileKey(N|S|E|W,     NE_IC                    ), {1, 6} },
        { tileKey(N|S|E|W,     NW_IC                    ), {2, 6} },
        { tileKey(N|S|E|W,     SE_IC                    ), {3, 6} },
        { tileKey(N|S|E|W,     SW_IC                    ), {4, 6} },
        { tileKey(N|S|E|W,     NE_IC|SW_IC              ), {5, 6} },
        { tileKey(N|S|E|W,     NW_IC|SE_IC              ), {6, 6} },
        { tileKey(N|S|E|W,     NE_IC|NW_IC              ), {7, 6} },

        // ── Row 7: cross continued ────────────────────────────────────────────
        { tileKey(N|S|E|W,     SE_IC|SW_IC              ), {0, 7} },
        { tileKey(N|S|E|W,     NE_IC|SE_IC              ), {1, 7} },
        { tileKey(N|S|E|W,     NW_IC|SW_IC              ), {2, 7} },
        { tileKey(N|S|E|W,     NE_IC|NW_IC|SE_IC        ), {3, 7} },
        { tileKey(N|S|E|W,     NE_IC|NW_IC|SW_IC        ), {4, 7} },
        { tileKey(N|S|E|W,     NE_IC|SE_IC|SW_IC        ), {5, 7} },
        { tileKey(N|S|E|W,     NW_IC|SE_IC|SW_IC        ), {6, 7} },
        { tileKey(N|S|E|W,     NE_IC|NW_IC|SE_IC|SW_IC  ), {7, 7} },  // + shape
    };
    return TABLE;
}
// clang-format on

// Look up tile position from masks. Falls back to isolated {0,0} if not found.
inline TilePos lookupTile(uint8_t cmask, uint8_t icmask) {
    auto const& table = tileTable();
    auto it = table.find(tileKey(cmask, icmask));
    if (it != table.end()) return it->second;
    return {0, 0}; // fallback: isolated
}

// Build the full custom block identifier string
inline std::string blockId(GlassVariant variant, uint8_t cmask, uint8_t icmask) {
    auto [col, row] = lookupTile(cmask, icmask);
    return "bdse:cglass_" + variantName(variant) + "_" + std::to_string(col) + "_" + std::to_string(row);
}

} // namespace ConnectedGlass
