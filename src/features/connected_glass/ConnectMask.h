#pragma once

#include "GlassTypes.h"

#include "mc/world/level/BlockSource.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/block/Block.h"

namespace ConnectedGlass {

struct ConnectResult {
    uint8_t cardinal;   // N/S/E/W bits
    uint8_t diagonal;   // NE_IC/NW_IC/SE_IC/SW_IC bits — inner corner needed?
};

// Compute both cardinal and diagonal connection masks for a glass block at pos.
//
// Inner corner logic:
//   icmask bit is SET when:
//     - both adjacent cardinals are connected (e.g. N+E for NE corner)
//     - AND the diagonal neighbor (NE) is NOT the same glass variant
//   This means the corner pixel needs to be drawn to avoid a visual gap.
inline ConnectResult computeMask(BlockSource& region, BlockPos const& pos, GlassVariant variant) {
    // ── Cardinal neighbors ────────────────────────────────────────────────────
    Block const& blockN = region.getBlock({pos.x,     pos.y, pos.z - 1});
    Block const& blockS = region.getBlock({pos.x,     pos.y, pos.z + 1});
    Block const& blockE = region.getBlock({pos.x + 1, pos.y, pos.z    });
    Block const& blockW = region.getBlock({pos.x - 1, pos.y, pos.z    });

    auto varN = fromBlock(blockN);
    auto varS = fromBlock(blockS);
    auto varE = fromBlock(blockE);
    auto varW = fromBlock(blockW);

    bool cN = varN.has_value() && canConnect(variant, *varN);
    bool cS = varS.has_value() && canConnect(variant, *varS);
    bool cE = varE.has_value() && canConnect(variant, *varE);
    bool cW = varW.has_value() && canConnect(variant, *varW);

    uint8_t cmask = 0;
    if (cN) cmask |= N;
    if (cS) cmask |= S;
    if (cE) cmask |= E;
    if (cW) cmask |= W;

    // ── Diagonal neighbors (only checked when both adjacent cardinals connect) ──
    uint8_t icmask = 0;

    // NE inner corner: N and E both connected, but NE diagonal is NOT glass
    if (cN && cE) {
        Block const& blockNE = region.getBlock({pos.x + 1, pos.y, pos.z - 1});
        auto varNE = fromBlock(blockNE);
        if (!varNE.has_value() || !canConnect(variant, *varNE)) {
            icmask |= NE_IC;
        }
    }

    // NW inner corner: N and W both connected, but NW diagonal is NOT glass
    if (cN && cW) {
        Block const& blockNW = region.getBlock({pos.x - 1, pos.y, pos.z - 1});
        auto varNW = fromBlock(blockNW);
        if (!varNW.has_value() || !canConnect(variant, *varNW)) {
            icmask |= NW_IC;
        }
    }

    // SE inner corner: S and E both connected, but SE diagonal is NOT glass
    if (cS && cE) {
        Block const& blockSE = region.getBlock({pos.x + 1, pos.y, pos.z + 1});
        auto varSE = fromBlock(blockSE);
        if (!varSE.has_value() || !canConnect(variant, *varSE)) {
            icmask |= SE_IC;
        }
    }

    // SW inner corner: S and W both connected, but SW diagonal is NOT glass
    if (cS && cW) {
        Block const& blockSW = region.getBlock({pos.x - 1, pos.y, pos.z + 1});
        auto varSW = fromBlock(blockSW);
        if (!varSW.has_value() || !canConnect(variant, *varSW)) {
            icmask |= SW_IC;
        }
    }

    return {cmask, icmask};
}

} // namespace ConnectedGlass
