#pragma once

#include "engine/core/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace engine {

enum class ParticleEmitterShape : std::uint8_t { Box, Sphere, Cylinder, Disc };
enum class ParticleEmitterShapeStyle : std::uint8_t { Volume, Surface };
enum class ParticleOrientation : std::uint8_t {
    FacingCamera,
    FacingCameraWorldUp,
    VelocityParallel,
    VelocityPerpendicular
};
enum class ParticleBlendMode : std::uint8_t { Alpha, Additive, SoftLight };
enum class ParticleFlipbookLayout : std::uint8_t { None, Grid2x2, Grid4x4, Grid8x8 };
enum class ParticleFlipbookMode : std::uint8_t { OneShot, Loop, PingPong, Random };

struct ParticleNumberKeypoint {
    float t = 0.0f; // 0..1 over particle lifetime
    float value = 0.0f;
};

struct ParticleColorKeypoint {
    float t = 0.0f;
    std::array<float, 3> color{{1.0f, 1.0f, 1.0f}};
};

struct ParticleNumberSequence {
    std::vector<ParticleNumberKeypoint> keypoints;

    [[nodiscard]] float sample(float t) const;
    [[nodiscard]] static ParticleNumberSequence constant(float value);
};

struct ParticleColorSequence {
    std::vector<ParticleColorKeypoint> keypoints;

    [[nodiscard]] std::array<float, 3> sample(float t) const;
    [[nodiscard]] static ParticleColorSequence constant(std::array<float, 3> color);
};

struct ParticleRange {
    float min = 0.0f;
    float max = 0.0f;

    [[nodiscard]] float sample(float u01) const;
};

/// Roblox-shaped particle emitter definition (`*.particle.json`).
/// See https://create.roblox.com/docs/effects/particle-emitters
struct ParticleEmitterAsset {
    int schema_version = 1;
    std::string id;
    /// Optional project-relative PNG. Empty uses the built-in soft disc texture.
    std::string texture;
    ParticleColorSequence color = ParticleColorSequence::constant({1.0f, 0.75f, 0.35f});
    ParticleNumberSequence size = ParticleNumberSequence::constant(0.35f);
    /// 0 = opaque, 1 = fully clear (Roblox Transparency).
    ParticleNumberSequence transparency = ParticleNumberSequence::constant(0.2f);
    ParticleRange lifetime{0.6f, 1.2f};
    ParticleRange speed{0.4f, 1.2f};
    float rate = 24.0f;
    std::array<float, 2> spread_angle_deg{{18.0f, 18.0f}};
    ParticleEmitterShape shape = ParticleEmitterShape::Disc;
    ParticleEmitterShapeStyle shape_style = ParticleEmitterShapeStyle::Volume;
    /// Box/disc half-extents or sphere/cylinder radius (x) / half-height (y for cylinder).
    std::array<float, 3> shape_size{{0.25f, 0.05f, 0.25f}};
    ParticleOrientation orientation = ParticleOrientation::FacingCamera;
    /// 0 = alpha blend, 1 = full additive (Roblox LightEmission).
    float light_emission = 0.85f;
    ParticleBlendMode blend = ParticleBlendMode::SoftLight;
    std::array<float, 3> acceleration{{0.0f, 1.6f, 0.0f}};
    float drag = 0.35f;
    std::uint32_t max_particles = 128;
    bool enabled = true;
    /// Local emission axis (normalized at load). Default +Y (up), Roblox EmissionDirection Top.
    std::array<float, 3> emission_direction{{0.0f, 1.0f, 0.0f}};
    /// Billboard width / height. `size` is height; width = size * aspectRatio (1 = square).
    float aspect_ratio = 1.0f;
    /// Lifetime rotation in degrees (billboard roll / yaw depending on orientation).
    ParticleNumberSequence rotation = ParticleNumberSequence::constant(0.0f);
    /// Randomize starting rotation offset on spawn (0..360°).
    bool rotation_start_random = true;
    /// Draw a second quad rotated 90° around world Y (classic low-poly volume).
    bool crossed_billboards = false;
    /// Minimum on-screen height in pixels (0 = off). Scales world size up with distance so
    /// landmarks like campfire flames stay readable far away without growing up close.
    float min_screen_size = 0.0f;
    /// When true (default), billboards fade against scene depth (soft particles / hide-behind).
    /// Impact flashes on hurt meshes set this false so the burst is not swallowed by the dummy.
    bool soft_occlusion = true;
    /// Grid atlas flipbook (Roblox FlipbookLayout). Requires a non-empty `texture`.
    ParticleFlipbookLayout flipbook_layout = ParticleFlipbookLayout::None;
    ParticleFlipbookMode flipbook_mode = ParticleFlipbookMode::Loop;
    float flipbook_framerate = 16.0f;
    bool flipbook_start_random = true;

    [[nodiscard]] std::uint32_t flipbook_columns() const noexcept;
    [[nodiscard]] std::uint32_t flipbook_frame_count() const noexcept;

    [[nodiscard]] static Result<ParticleEmitterAsset> load(const std::filesystem::path& path);
    [[nodiscard]] static Result<ParticleEmitterAsset> parse(const std::string& text,
        const std::string& source_name = "particle.json");
    /// Fail-closed when `texture` is non-empty: project-relative `.png` must exist.
    [[nodiscard]] Result<void> validate_texture(const std::filesystem::path& project_root) const;
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] std::string to_json() const;
};

[[nodiscard]] bool is_valid_particle_texture_path(const std::string& relative) noexcept;

} // namespace engine
