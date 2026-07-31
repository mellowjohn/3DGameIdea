#pragma once

#include "engine/assets/material_asset.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace engine {

/// Runtime surface inputs for the lit PBR path (TICKET-0040 / 0238 / 0239).
struct PbrSurfaceParams {
    float roughness = 1.0f;
    float metallic = 0.0f;
    std::array<float, 3> emissive{0.0f, 0.0f, 0.0f};
    float emissive_pulse_hz = 0.0f;
    float emissive_pulse_min = 0.35f;
    /// When true, pixel shader discards fragments with alpha below opacity_cutoff.
    bool alpha_clip = false;
    float opacity_cutoff = 0.5f;
    /// Surface alpha when not sampling an albedo texture (vertex/baseColor.a).
    float surface_alpha = 1.0f;

    [[nodiscard]] static PbrSurfaceParams dielectric_default() { return {}; }

    [[nodiscard]] static PbrSurfaceParams from_material(const MaterialAsset& material) {
        PbrSurfaceParams params;
        params.roughness = material.roughness;
        params.metallic = material.metallic;
        params.emissive = material.emissive;
        params.surface_alpha = material.base_color[3];
        if (material.opacity_mode == OpacityMode::Masked) {
            params.alpha_clip = true;
            params.opacity_cutoff = material.opacity_cutoff;
        }
        if (material.shader == MaterialShaderProfile::EmissiveMagic) {
            params.emissive_pulse_hz = material.emissive_pulse_hz;
            params.emissive_pulse_min = material.emissive_pulse_min;
        }
        return params;
    }

    /// Scale authored emissive by pulse envelope (1 when Hz == 0).
    [[nodiscard]] std::array<float, 3> emissive_at_time(float time_seconds) const {
        if (emissive_pulse_hz <= 0.0f) return emissive;
        constexpr float k_pi = 3.14159265f;
        const float wave = 0.5f + 0.5f * std::sin(2.0f * k_pi * emissive_pulse_hz * time_seconds);
        const float scale = emissive_pulse_min + (1.0f - emissive_pulse_min) * wave;
        return {emissive[0] * scale, emissive[1] * scale, emissive[2] * scale};
    }
};

/// Opaque and masked materials draw through the lit PBR path; blended water uses the water pass.
[[nodiscard]] inline bool material_supports_opaque_pbr_runtime(const MaterialAsset& material) noexcept {
    return material.opacity_mode == OpacityMode::Opaque || material.opacity_mode == OpacityMode::Masked;
}

[[nodiscard]] inline bool material_supports_water_runtime(const MaterialAsset& material) noexcept {
    return material.opacity_mode == OpacityMode::Blended &&
           material.physics.surface == "water";
}

/// GGX/Smith/Schlick Cook-Torrance radiance for one light (no 1/pi so existing light strengths stay usable).
[[nodiscard]] inline std::array<float, 3> evaluate_pbr_light(const std::array<float, 3>& albedo,
    const PbrSurfaceParams& surface, const std::array<float, 3>& normal, const std::array<float, 3>& view_dir,
    const std::array<float, 3>& light_dir, const std::array<float, 3>& light_radiance) {
    const auto clamp01 = [](float value) { return std::clamp(value, 0.0f, 1.0f); };
    const auto dot3 = [](const std::array<float, 3>& a, const std::array<float, 3>& b) {
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    };
    const auto normalize3 = [&](std::array<float, 3> value) {
        const float length = std::sqrt(std::max(dot3(value, value), 1.0e-8f));
        return std::array<float, 3>{value[0] / length, value[1] / length, value[2] / length};
    };

    const auto N = normalize3(normal);
    const auto V = normalize3(view_dir);
    const auto L = normalize3(light_dir);
    const float NdotL = clamp01(dot3(N, L));
    if (NdotL <= 0.0f) return {0.0f, 0.0f, 0.0f};

    const auto H = normalize3({L[0] + V[0], L[1] + V[1], L[2] + V[2]});
    const float NdotV = std::max(dot3(N, V), 1.0e-4f);
    const float NdotH = clamp01(dot3(N, H));
    const float VdotH = clamp01(dot3(V, H));

    const float roughness = std::clamp(surface.roughness, 0.04f, 1.0f);
    const float metallic = clamp01(surface.metallic);
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;

    const float denom = (NdotH * NdotH) * (alpha2 - 1.0f) + 1.0f;
    const float D = alpha2 / std::max(3.14159265f * denom * denom, 1.0e-6f);

    const auto geometry_schlick = [alpha](float NdotX) {
        const float k = (alpha + 1.0f) * (alpha + 1.0f) / 8.0f;
        return NdotX / std::max(NdotX * (1.0f - k) + k, 1.0e-4f);
    };
    const float G = geometry_schlick(NdotV) * geometry_schlick(NdotL);

    const std::array<float, 3> f0{std::lerp(0.04f, albedo[0], metallic), std::lerp(0.04f, albedo[1], metallic),
        std::lerp(0.04f, albedo[2], metallic)};
    const float fresnel = std::pow(1.0f - VdotH, 5.0f);
    const std::array<float, 3> F{f0[0] + (1.0f - f0[0]) * fresnel, f0[1] + (1.0f - f0[1]) * fresnel,
        f0[2] + (1.0f - f0[2]) * fresnel};

    const float specular_denom = std::max(4.0f * NdotV * NdotL, 1.0e-4f);
    const std::array<float, 3> specular{(D * G * F[0]) / specular_denom, (D * G * F[1]) / specular_denom,
        (D * G * F[2]) / specular_denom};
    const std::array<float, 3> diffuse{albedo[0] * (1.0f - metallic), albedo[1] * (1.0f - metallic),
        albedo[2] * (1.0f - metallic)};

    return {(diffuse[0] + specular[0]) * light_radiance[0] * NdotL,
        (diffuse[1] + specular[1]) * light_radiance[1] * NdotL,
        (diffuse[2] + specular[2]) * light_radiance[2] * NdotL};
}

} // namespace engine
