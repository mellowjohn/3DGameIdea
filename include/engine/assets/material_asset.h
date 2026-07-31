#pragma once

#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <string>

namespace engine {

enum class OpacityMode { Opaque, Masked, Blended };

/// Engine-owned master shader selected from material JSON (DEC-0049 / TICKET-0238).
enum class MaterialShaderProfile { StylizedOpaque, EmissiveMagic };

struct PhysicalMaterialProperties {
    float friction = 0.8f;
    float restitution = 0.05f;
    float density = 1000.0f;
    std::string surface = "default";
};

struct MaterialAsset {
    std::uint32_t schema_version = 1;
    MaterialShaderProfile shader = MaterialShaderProfile::StylizedOpaque;
    std::array<float, 4> base_color{1, 1, 1, 1};
    float roughness = 1.0f;
    float metallic = 0.0f;
    OpacityMode opacity_mode = OpacityMode::Opaque;
    float opacity_cutoff = 0.5f;
    std::array<float, 3> emissive{0, 0, 0};
    /// When > 0, emissive intensity pulses with sin(2π · hz · t). Used by `emissive_magic`.
    float emissive_pulse_hz = 0.0f;
    /// Floor scale for pulse (1 = full emissive at peaks). Clamped [0, 1].
    float emissive_pulse_min = 0.35f;
    /// Optional project-relative PNG sampled as albedo (and alpha for masked). Empty = mesh/vertex path.
    std::string albedo_map;
    /// Optional project-relative PNG; v1 multiplies authored emissive by the texture's average RGB.
    std::string emissive_map;
    bool double_sided = false;
    PhysicalMaterialProperties physics;

    [[nodiscard]] Result<void> validate() const;
    /// Fail closed if albedoMap/emissiveMap are set but missing under project_root.
    [[nodiscard]] Result<void> validate_texture_maps(const std::filesystem::path& project_root) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<MaterialAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<MaterialAsset> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static MaterialAsset make_default();
};

[[nodiscard]] const char* to_string(OpacityMode value) noexcept;
[[nodiscard]] const char* to_string(MaterialShaderProfile value) noexcept;

/// True when path is a safe project-relative PNG (no `..`, not absolute).
[[nodiscard]] bool is_valid_material_map_path(const std::string& relative) noexcept;

} // namespace engine
