#pragma once

#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace engine {

/// Shared directional wind driving foliage gusts, tree canopy sway, and ambient streak particles.
struct WindFieldParams {
    /// Unit direction on XZ (y ignored). Default west→east breeze.
    std::array<float, 3> direction{{1.0f, 0.0f, 0.15f}};
    float speed = 4.5f;
    float ambient_amp = 1.0f;
    float gust_wavelength = 18.0f;
    float gust_strength = 0.55f;
    /// Higher = narrower traveling bands.
    float gust_sharpness = 2.4f;
    float time_seconds = 0.0f;

    void normalize_direction() noexcept {
        const float len = std::sqrt(direction[0] * direction[0] + direction[2] * direction[2]);
        if (len < 1e-5f) {
            direction = {1.0f, 0.0f, 0.0f};
            return;
        }
        direction[0] /= len;
        direction[1] = 0.0f;
        direction[2] /= len;
    }
};

/// Traveling gust envelope in [0,1] at world XZ. Matches foliage / prop VS.
[[nodiscard]] inline float wind_gust_envelope(float world_x, float world_z, const WindFieldParams& wind) noexcept {
    const float wavelength = std::max(wind.gust_wavelength, 0.5f);
    const float freq = 6.28318530718f / wavelength;
    const float phase = world_x * wind.direction[0] * freq + world_z * wind.direction[2] * freq
        - wind.time_seconds * wind.speed * freq * 0.35f;
    const float wave = 0.5f + 0.5f * std::sin(phase);
    const float sharpness = std::max(wind.gust_sharpness, 0.5f);
    float envelope = wave;
    // pow(saturate(wave), sharpness)
    if (envelope < 0.0f) envelope = 0.0f;
    if (envelope > 1.0f) envelope = 1.0f;
    return std::pow(envelope, sharpness);
}

/// True for scene tree meshes that should receive canopy sway (path filter).
[[nodiscard]] inline bool mesh_uses_wind_sway(std::string_view mesh_path) noexcept {
    std::string lower(mesh_path);
    for (char& c : lower) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c == '\\') c = '/';
    }
    if (lower.find("dead-tree") != std::string::npos) return true;
    if (lower.find("/tree.") != std::string::npos) return true;
    if (lower.size() >= 9 && lower.compare(lower.size() - 9, 9, "tree.gltf") == 0) return true;
    if (lower.find("tree.gltf") != std::string::npos) return true;
    return false;
}

/// Pack wind into 8 floats for root constants / CB tails:
/// [0]=dirX [1]=dirZ [2]=speed [3]=ambientAmp [4]=wavelength [5]=gustStrength [6]=sharpness [7]=time
inline void pack_wind_constants(const WindFieldParams& wind, float out[8]) noexcept {
    WindFieldParams copy = wind;
    copy.normalize_direction();
    out[0] = copy.direction[0];
    out[1] = copy.direction[2];
    out[2] = copy.speed;
    out[3] = copy.ambient_amp;
    out[4] = copy.gust_wavelength;
    out[5] = copy.gust_strength;
    out[6] = copy.gust_sharpness;
    out[7] = copy.time_seconds;
}

} // namespace engine
