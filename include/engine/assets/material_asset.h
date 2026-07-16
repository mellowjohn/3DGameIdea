#pragma once

#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <string>

namespace engine {

enum class OpacityMode { Opaque, Masked, Blended };

struct PhysicalMaterialProperties {
    float friction = 0.8f;
    float restitution = 0.05f;
    float density = 1000.0f;
    std::string surface = "default";
};

struct MaterialAsset {
    std::uint32_t schema_version = 1;
    std::array<float, 4> base_color{1, 1, 1, 1};
    float roughness = 1.0f;
    float metallic = 0.0f;
    OpacityMode opacity_mode = OpacityMode::Opaque;
    float opacity_cutoff = 0.5f;
    std::array<float, 3> emissive{0, 0, 0};
    bool double_sided = false;
    PhysicalMaterialProperties physics;

    [[nodiscard]] Result<void> validate() const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static Result<MaterialAsset> from_json(const std::string& text);
    [[nodiscard]] static Result<MaterialAsset> load(const std::filesystem::path& path);
    [[nodiscard]] Result<void> save_atomic(const std::filesystem::path& path) const;
    [[nodiscard]] static MaterialAsset make_default();
};

[[nodiscard]] const char* to_string(OpacityMode value) noexcept;

} // namespace engine
