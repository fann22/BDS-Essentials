#pragma once

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/level/block/VanillaBlockTypeIds.h"
#include "mc/world/level/block/Block.h"

#include <optional>
#include <string>

namespace ConnectedGlass {

// ── Glass variants (panes excluded for now) ───────────────────────────────────

enum class GlassVariant : uint8_t {
    Glass = 0,
    WhiteStained,
    OrangeStained,
    MagentaStained,
    LightBlueStained,
    YellowStained,
    LimeStained,
    PinkStained,
    GrayStained,
    LightGrayStained,
    CyanStained,
    PurpleStained,
    BlueStained,
    BrownStained,
    GreenStained,
    RedStained,
    BlackStained,

    Count
};

constexpr size_t VARIANT_COUNT = static_cast<size_t>(GlassVariant::Count);

// ── Cardinal connection bits ──────────────────────────────────────────────────

constexpr uint8_t N = 0b0001;
constexpr uint8_t S = 0b0010;
constexpr uint8_t E = 0b0100;
constexpr uint8_t W = 0b1000;

// ── Inner corner bits (diagonal) ─────────────────────────────────────────────
// Active when both adjacent cardinals are connected but the diagonal neighbor is NOT glass

constexpr uint8_t NE_IC = 0b0001;
constexpr uint8_t NW_IC = 0b0010;
constexpr uint8_t SE_IC = 0b0100;
constexpr uint8_t SW_IC = 0b1000;

// ── Variant → string name (used in block IDs and resource pack) ───────────────

inline std::string variantName(GlassVariant variant) {
    switch (variant) {
        case GlassVariant::Glass:           return "glass";
        case GlassVariant::WhiteStained:    return "stained_white";
        case GlassVariant::OrangeStained:   return "stained_orange";
        case GlassVariant::MagentaStained:  return "stained_magenta";
        case GlassVariant::LightBlueStained:return "stained_light_blue";
        case GlassVariant::YellowStained:   return "stained_yellow";
        case GlassVariant::LimeStained:     return "stained_lime";
        case GlassVariant::PinkStained:     return "stained_pink";
        case GlassVariant::GrayStained:     return "stained_gray";
        case GlassVariant::LightGrayStained:return "stained_light_gray";
        case GlassVariant::CyanStained:     return "stained_cyan";
        case GlassVariant::PurpleStained:   return "stained_purple";
        case GlassVariant::BlueStained:     return "stained_blue";
        case GlassVariant::BrownStained:    return "stained_brown";
        case GlassVariant::GreenStained:    return "stained_green";
        case GlassVariant::RedStained:      return "stained_red";
        case GlassVariant::BlackStained:    return "stained_black";
        default:                            return "glass";
    }
}

// ── Variant → vanilla HashedString ───────────────────────────────────────────

inline HashedString const& vanillaId(GlassVariant variant) {
    using namespace VanillaBlockTypeIds;
    switch (variant) {
        case GlassVariant::Glass:           return Glass();
        case GlassVariant::WhiteStained:    return WhiteStainedGlass();
        case GlassVariant::OrangeStained:   return OrangeStainedGlass();
        case GlassVariant::MagentaStained:  return MagentaStainedGlass();
        case GlassVariant::LightBlueStained:return LightBlueStainedGlass();
        case GlassVariant::YellowStained:   return YellowStainedGlass();
        case GlassVariant::LimeStained:     return LimeStainedGlass();
        case GlassVariant::PinkStained:     return PinkStainedGlass();
        case GlassVariant::GrayStained:     return GrayStainedGlass();
        case GlassVariant::LightGrayStained:return LightGrayStainedGlass();
        case GlassVariant::CyanStained:     return CyanStainedGlass();
        case GlassVariant::PurpleStained:   return PurpleStainedGlass();
        case GlassVariant::BlueStained:     return BlueStainedGlass();
        case GlassVariant::BrownStained:    return BrownStainedGlass();
        case GlassVariant::GreenStained:    return GreenStainedGlass();
        case GlassVariant::RedStained:      return RedStainedGlass();
        case GlassVariant::BlackStained:    return BlackStainedGlass();
        default:                            return Glass();
    }
}

// ── Block → variant detection ─────────────────────────────────────────────────

inline std::optional<GlassVariant> fromBlock(Block const& block) {
    std::string const& name = block.getTypeName();
    for (size_t i = 0; i < VARIANT_COUNT; ++i) {
        auto v = static_cast<GlassVariant>(i);
        if (name == vanillaId(v).getString()) return v;
    }
    return std::nullopt;
}

inline bool isGlass(Block const& block) {
    return fromBlock(block).has_value();
}

// Two variants connect if they are the same (stained connects only to same color)
inline bool canConnect(GlassVariant a, GlassVariant b) {
    return a == b;
}

} // namespace ConnectedGlass
