#include "engine/ui/combat_text.h"

#include "engine/ui/game_fonts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace engine {
namespace {

constexpr float kImpactDuration = 0.09f;

ImU32 to_col(const std::array<float, 4>& rgba, float alpha_mul) {
    const float a = std::clamp(rgba[3] * alpha_mul, 0.0f, 255.0f);
    return IM_COL32(static_cast<int>(std::lround(std::clamp(rgba[0], 0.0f, 255.0f))),
        static_cast<int>(std::lround(std::clamp(rgba[1], 0.0f, 255.0f))),
        static_cast<int>(std::lround(std::clamp(rgba[2], 0.0f, 255.0f))),
        static_cast<int>(std::lround(a)));
}

float ease_out_cubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float ease_out_back(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    const float inv = t - 1.0f;
    return 1.0f + c3 * inv * inv * inv + c1 * inv * inv;
}

float floater_scale(const CombatTextFloater& f) {
    if (f.age <= kImpactDuration) {
        const float u = ease_out_back(f.age / kImpactDuration);
        return 0.55f + (f.impact_peak - 0.55f) * u;
    }
    const float settle_t = std::clamp((f.age - kImpactDuration) / 0.22f, 0.0f, 1.0f);
    return f.impact_peak + (f.rest_scale - f.impact_peak) * ease_out_cubic(settle_t);
}

float floater_alpha(const CombatTextFloater& f) {
    const float life = std::max(0.01f, f.lifetime);
    const float t = std::clamp(f.age / life, 0.0f, 1.0f);
    if (t < 0.42f) return 1.0f;
    return 1.0f - ease_out_cubic((t - 0.42f) / 0.58f);
}

float floater_rise(const CombatTextFloater& f) {
    const float life = std::max(0.01f, f.lifetime);
    return f.rise_height * ease_out_cubic(std::clamp(f.age / life, 0.0f, 1.0f));
}

std::string format_amount(double amount, CombatTextKind kind) {
    if (!std::isfinite(amount)) amount = 0.0;
    const double rounded = std::round(std::fabs(amount));
    char buf[40];
    const char* prefix = (kind == CombatTextKind::Bleed || kind == CombatTextKind::Poison ||
                             kind == CombatTextKind::Burn || kind == CombatTextKind::Hit ||
                             kind == CombatTextKind::Crit)
        ? "-"
        : (kind == CombatTextKind::Heal ? "+" : "");
    if (kind == CombatTextKind::Hit || kind == CombatTextKind::Crit) {
        // Direct hits keep unsigned numbers; DoTs use minus.
        prefix = "";
    }
    if (std::fabs(amount - std::round(amount)) < 0.05) {
        std::snprintf(buf, sizeof(buf), "%s%d", prefix, static_cast<int>(rounded));
    } else {
        std::snprintf(buf, sizeof(buf), "%s%.1f", prefix, std::fabs(amount));
    }
    return buf;
}

void style_for_kind(CombatTextFloater& floater) {
    switch (floater.kind) {
    case CombatTextKind::Crit:
        floater.lifetime = 1.05f;
        floater.rise_height = 0.95f;
        floater.rest_scale = 1.22f;
        floater.impact_peak = 1.85f;
        floater.base_font = 36.0f;
        floater.color = {255.0f, 224.0f, 138.0f, 255.0f};
        break;
    case CombatTextKind::Bleed:
        floater.lifetime = 0.85f;
        floater.rise_height = 0.55f;
        floater.rest_scale = 0.95f;
        floater.impact_peak = 1.25f;
        floater.base_font = 24.0f;
        floater.color = {220.0f, 72.0f, 72.0f, 255.0f};
        break;
    case CombatTextKind::Poison:
        floater.lifetime = 0.85f;
        floater.rise_height = 0.55f;
        floater.rest_scale = 0.95f;
        floater.impact_peak = 1.25f;
        floater.base_font = 24.0f;
        floater.color = {110.0f, 190.0f, 90.0f, 255.0f};
        break;
    case CombatTextKind::Burn:
        floater.lifetime = 0.85f;
        floater.rise_height = 0.55f;
        floater.rest_scale = 0.95f;
        floater.impact_peak = 1.25f;
        floater.base_font = 24.0f;
        floater.color = {255.0f, 140.0f, 48.0f, 255.0f};
        break;
    case CombatTextKind::Heal:
        floater.lifetime = 0.9f;
        floater.rise_height = 0.7f;
        floater.rest_scale = 1.0f;
        floater.impact_peak = 1.35f;
        floater.base_font = 28.0f;
        floater.color = {90.0f, 200.0f, 120.0f, 255.0f};
        break;
    case CombatTextKind::Hit:
    default:
        floater.lifetime = 0.9f;
        floater.rise_height = 0.72f;
        floater.rest_scale = 1.0f;
        floater.impact_peak = 1.42f;
        floater.base_font = 28.0f;
        floater.color = {241.0f, 238.0f, 232.0f, 255.0f};
        break;
    }
}

} // namespace

const char* combat_text_kind_id(CombatTextKind kind) noexcept {
    switch (kind) {
    case CombatTextKind::Crit:
        return "crit";
    case CombatTextKind::Bleed:
        return "bleed";
    case CombatTextKind::Poison:
        return "poison";
    case CombatTextKind::Burn:
        return "burn";
    case CombatTextKind::Heal:
        return "heal";
    case CombatTextKind::Hit:
    default:
        return "hit";
    }
}

CombatTextKind combat_text_kind_from_id(const std::string& id) noexcept {
    if (id == "crit") return CombatTextKind::Crit;
    if (id == "bleed") return CombatTextKind::Bleed;
    if (id == "poison") return CombatTextKind::Poison;
    if (id == "burn") return CombatTextKind::Burn;
    if (id == "heal") return CombatTextKind::Heal;
    return CombatTextKind::Hit;
}

void CombatTextRuntime::clear() { floaters_.clear(); }

void CombatTextRuntime::spawn(float x, float y, float z, double amount, CombatTextKind kind) {
    spawn_text(x, y, z, format_amount(amount, kind), kind);
}

void CombatTextRuntime::spawn(float x, float y, float z, double amount, bool crit) {
    spawn(x, y, z, amount, crit ? CombatTextKind::Crit : CombatTextKind::Hit);
}

void CombatTextRuntime::spawn_text(float x, float y, float z, std::string text, bool crit) {
    spawn_text(x, y, z, std::move(text), crit ? CombatTextKind::Crit : CombatTextKind::Hit);
}

void CombatTextRuntime::spawn_text(float x, float y, float z, std::string text, CombatTextKind kind) {
    if (text.empty()) return;
    CombatTextFloater floater;
    floater.world_x = x;
    floater.world_y = y;
    floater.world_z = z;
    const std::uint32_t salt = spawn_salt_++;
    floater.world_x += (static_cast<float>(salt % 5u) - 2.0f) * 0.08f;
    floater.world_z += (static_cast<float>(salt % 3u) - 1.0f) * 0.05f;
    floater.text = std::move(text);
    floater.kind = kind;
    style_for_kind(floater);
    floaters_.push_back(std::move(floater));
}

void CombatTextRuntime::tick(float dt_seconds) {
    const float dt = std::clamp(dt_seconds, 0.0f, 0.1f);
    if (dt <= 0.0f) return;
    for (auto& floater : floaters_) {
        floater.age += dt;
    }
    floaters_.erase(std::remove_if(floaters_.begin(), floaters_.end(),
                        [](const CombatTextFloater& f) { return f.age >= f.lifetime; }),
        floaters_.end());
}

void CombatTextRuntime::draw(ImDrawList* draw_list, const std::array<float, 16>& view_projection,
    const ViewportRect& viewport) const {
    if (!draw_list || floaters_.empty()) return;

    ImFont* font = GameFonts::display() ? GameFonts::display() : ImGui::GetFont();
    for (const auto& floater : floaters_) {
        float sx = 0.0f;
        float sy = 0.0f;
        float depth = 0.0f;
        const float draw_y = floater.world_y + floater_rise(floater);
        if (!project_world_to_screen(view_projection, viewport, floater.world_x, draw_y, floater.world_z, sx, sy,
                depth) ||
            depth <= 0.0f) {
            continue;
        }

        const float alpha = floater_alpha(floater);
        if (alpha <= 0.02f) continue;
        const float scale = floater_scale(floater);
        const float font_sz = floater.base_font * scale;
        const ImVec2 text_size = font->CalcTextSizeA(font_sz, FLT_MAX, 0.0f, floater.text.c_str());
        const float text_x = sx - text_size.x * 0.5f;
        const float text_y = sy - text_size.y * 0.5f;

        const ImU32 outline = IM_COL32(18, 14, 10, static_cast<int>(std::lround(210.0f * alpha)));
        const ImU32 fill = to_col(floater.color, alpha);
        constexpr float kOutline = 1.6f;
        for (int ox = -1; ox <= 1; ++ox) {
            for (int oy = -1; oy <= 1; ++oy) {
                if (ox == 0 && oy == 0) continue;
                draw_list->AddText(font, font_sz,
                    ImVec2{text_x + static_cast<float>(ox) * kOutline, text_y + static_cast<float>(oy) * kOutline},
                    outline, floater.text.c_str());
            }
        }
        draw_list->AddText(font, font_sz, ImVec2{text_x, text_y}, fill, floater.text.c_str());
    }
}

} // namespace engine
