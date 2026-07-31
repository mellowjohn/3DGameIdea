#include "engine/assets/material_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace engine {
namespace {
EngineError material_error(std::string code, ErrorCategory category, std::string message, std::string remediation) {
    return {std::move(code), Severity::Error, category, "materials", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, std::move(remediation), make_correlation_id()};
}
bool unit(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }

Result<MaterialShaderProfile> parse_shader_profile(const std::string& name) {
    if (name == "stylized_opaque") return Result<MaterialShaderProfile>::success(MaterialShaderProfile::StylizedOpaque);
    if (name == "emissive_magic") return Result<MaterialShaderProfile>::success(MaterialShaderProfile::EmissiveMagic);
    return Result<MaterialShaderProfile>::failure(material_error("MATERIAL-SHADER-UNKNOWN", ErrorCategory::Validation,
        "Unknown material shader profile: " + name, "Use stylized_opaque or emissive_magic."));
}

Result<void> validate_optional_map_field(const std::string& field_name, const std::string& relative) {
    if (relative.empty()) return Result<void>::success();
    if (!is_valid_material_map_path(relative))
        return Result<void>::failure(material_error("MATERIAL-MAP-PATH-INVALID", ErrorCategory::Validation,
            field_name + " must be a project-relative .png path without '..'",
            "Use paths like assets/vfx/leaf_mask.png."));
    return Result<void>::success();
}

Result<void> require_map_exists(const std::filesystem::path& project_root, const std::string& field_name,
    const std::string& relative) {
    if (relative.empty()) return Result<void>::success();
    const auto absolute = project_root / relative;
    if (!std::filesystem::exists(absolute))
        return Result<void>::failure(material_error("MATERIAL-MAP-MISSING", ErrorCategory::Validation,
            field_name + " file not found: " + relative, "Add the PNG under the project or clear the map field."));
    return Result<void>::success();
}
} // namespace

bool is_valid_material_map_path(const std::string& relative) noexcept {
    if (relative.empty()) return false;
    if (relative.find("..") != std::string::npos) return false;
    if (relative.size() >= 2 && std::isalpha(static_cast<unsigned char>(relative[0])) && relative[1] == ':')
        return false;
    if (!relative.empty() && (relative[0] == '/' || relative[0] == '\\')) return false;
    const auto slash = relative.find_last_of("/\\");
    const std::string file = slash == std::string::npos ? relative : relative.substr(slash + 1);
    if (file.size() < 5) return false;
    std::string ext = file.substr(file.size() - 4);
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext == ".png";
}

const char* to_string(OpacityMode value) noexcept {
    switch (value) { case OpacityMode::Opaque: return "opaque"; case OpacityMode::Masked: return "masked"; case OpacityMode::Blended: return "blended"; }
    return "opaque";
}

const char* to_string(MaterialShaderProfile value) noexcept {
    switch (value) {
    case MaterialShaderProfile::StylizedOpaque: return "stylized_opaque";
    case MaterialShaderProfile::EmissiveMagic: return "emissive_magic";
    }
    return "stylized_opaque";
}

Result<void> MaterialAsset::validate() const {
    if (schema_version != 1) return Result<void>::failure(material_error("MATERIAL-SCHEMA-UNSUPPORTED", ErrorCategory::Validation, "Only material schema version 1 is supported", "Migrate the material to schemaVersion 1."));
    if (!std::all_of(base_color.begin(), base_color.end(), unit)) return Result<void>::failure(material_error("MATERIAL-COLOR-INVALID", ErrorCategory::Validation, "Base color channels must be finite values in [0, 1]", "Clamp baseColor RGBA channels to [0, 1]."));
    if (!unit(roughness) || !unit(metallic) || !unit(opacity_cutoff)) return Result<void>::failure(material_error("MATERIAL-PBR-INVALID", ErrorCategory::Validation, "Roughness, metallic, and opacity cutoff must be in [0, 1]", "Correct the material scalar values."));
    if (!std::all_of(emissive.begin(), emissive.end(), [](float value){ return std::isfinite(value) && value >= 0.0f && value <= 64.0f; })) return Result<void>::failure(material_error("MATERIAL-EMISSIVE-INVALID", ErrorCategory::Validation, "Emissive channels must be finite values in [0, 64]", "Correct the emissive RGB values."));
    if (!std::isfinite(emissive_pulse_hz) || emissive_pulse_hz < 0.0f || emissive_pulse_hz > 60.0f)
        return Result<void>::failure(material_error("MATERIAL-PULSE-HZ-INVALID", ErrorCategory::Validation,
            "emissivePulseHz must be finite in [0, 60]", "Use 0 to disable pulse or a modest Hz for magic glow."));
    if (!unit(emissive_pulse_min))
        return Result<void>::failure(material_error("MATERIAL-PULSE-MIN-INVALID", ErrorCategory::Validation,
            "emissivePulseMin must be in [0, 1]", "Clamp emissivePulseMin to [0, 1]."));
    if (shader == MaterialShaderProfile::EmissiveMagic && emissive_pulse_hz > 0.0f &&
        emissive[0] <= 0.0f && emissive[1] <= 0.0f && emissive[2] <= 0.0f)
        return Result<void>::failure(material_error("MATERIAL-PULSE-NO-EMISSIVE", ErrorCategory::Validation,
            "emissive_magic pulse requires non-zero emissive RGB", "Set emissive or disable emissivePulseHz."));
    if (const auto albedo = validate_optional_map_field("albedoMap", albedo_map); !albedo) return albedo;
    if (const auto emissive_path = validate_optional_map_field("emissiveMap", emissive_map); !emissive_path)
        return emissive_path;
    if (!std::isfinite(physics.friction) || physics.friction < 0.0f || physics.friction > 2.0f || !unit(physics.restitution) || !std::isfinite(physics.density) || physics.density <= 0.0f || physics.density > 100000.0f || physics.surface.empty()) return Result<void>::failure(material_error("MATERIAL-PHYSICS-INVALID", ErrorCategory::Validation, "Physical material values or surface name are invalid", "Use friction [0,2], restitution [0,1], positive density, and a non-empty surface."));
    if (opacity_mode == OpacityMode::Opaque && base_color[3] < 1.0f) return Result<void>::failure(material_error("MATERIAL-OPAQUE-ALPHA", ErrorCategory::Validation, "Opaque materials must use alpha 1", "Set baseColor alpha to 1 or choose masked/blended opacity."));
    return Result<void>::success();
}

Result<void> MaterialAsset::validate_texture_maps(const std::filesystem::path& project_root) const {
    if (const auto base = validate(); !base) return base;
    if (const auto albedo = require_map_exists(project_root, "albedoMap", albedo_map); !albedo) return albedo;
    if (const auto emissive_path = require_map_exists(project_root, "emissiveMap", emissive_map); !emissive_path)
        return emissive_path;
    return Result<void>::success();
}

std::string MaterialAsset::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion",schema_version},{"shader",to_string(shader)},{"baseColor",base_color},{"roughness",roughness},{"metallic",metallic},{"opacityMode",to_string(opacity_mode)},{"opacityCutoff",opacity_cutoff},{"emissive",emissive},{"emissivePulseHz",emissive_pulse_hz},{"emissivePulseMin",emissive_pulse_min},{"albedoMap",albedo_map},{"emissiveMap",emissive_map},{"doubleSided",double_sided},{"physics",{{"friction",physics.friction},{"restitution",physics.restitution},{"density",physics.density},{"surface",physics.surface}}}};
    return root.dump(2) + "\n";
}

Result<MaterialAsset> MaterialAsset::from_json(const std::string& text) {
    try {
        const auto root=nlohmann::json::parse(text); MaterialAsset value;
        value.schema_version=root.at("schemaVersion").get<std::uint32_t>(); value.base_color=root.at("baseColor").get<std::array<float,4>>(); value.roughness=root.at("roughness").get<float>(); value.metallic=root.at("metallic").get<float>();
        const auto mode=root.at("opacityMode").get<std::string>(); if(mode=="opaque")value.opacity_mode=OpacityMode::Opaque;else if(mode=="masked")value.opacity_mode=OpacityMode::Masked;else if(mode=="blended")value.opacity_mode=OpacityMode::Blended;else return Result<MaterialAsset>::failure(material_error("MATERIAL-OPACITY-MODE",ErrorCategory::Validation,"Unknown opacity mode: "+mode,"Use opaque, masked, or blended."));
        value.opacity_cutoff=root.value("opacityCutoff",0.5f); value.emissive=root.value("emissive",std::array<float,3>{0,0,0}); value.double_sided=root.value("doubleSided",false);
        value.emissive_pulse_hz=root.value("emissivePulseHz",0.0f); value.emissive_pulse_min=root.value("emissivePulseMin",0.35f);
        value.albedo_map=root.value("albedoMap",std::string{}); value.emissive_map=root.value("emissiveMap",std::string{});
        if (root.contains("shader")) {
            auto parsed = parse_shader_profile(root.at("shader").get<std::string>());
            if (!parsed) return Result<MaterialAsset>::failure(parsed.error());
            value.shader = parsed.value();
        } else {
            value.shader = MaterialShaderProfile::StylizedOpaque;
        }
        const auto& physics=root.at("physics"); value.physics.friction=physics.at("friction").get<float>(); value.physics.restitution=physics.at("restitution").get<float>(); value.physics.density=physics.value("density",1000.0f); value.physics.surface=physics.at("surface").get<std::string>();
        auto valid=value.validate();if(!valid)return Result<MaterialAsset>::failure(valid.error());return Result<MaterialAsset>::success(std::move(value));
    } catch(const std::exception& exception) { auto error=material_error("MATERIAL-PARSE-FAILED",ErrorCategory::Serialization,"Material JSON is malformed or missing required fields","Correct the material JSON structure.");error.causes.push_back(exception.what());return Result<MaterialAsset>::failure(std::move(error)); }
}

Result<MaterialAsset> MaterialAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input)return Result<MaterialAsset>::failure(material_error("MATERIAL-READ-FAILED",ErrorCategory::Io,"Could not read material: "+path.generic_string(),"Check the material path and file permissions."));std::ostringstream text;text<<input.rdbuf();return from_json(text.str());
}

MaterialAsset MaterialAsset::make_default() {
    MaterialAsset material;
    material.physics.surface = "default";
    return material;
}

Result<void> MaterialAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output)
        return Result<void>::failure(
            material_error("MATERIAL-WRITE-FAILED", ErrorCategory::Io, "Could not write material: " + path.generic_string(),
                "Check file permissions and disk space."));
    output << to_json();
    output.close();
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::rename(temporary, path, ignored);
    if (ignored)
        return Result<void>::failure(material_error("MATERIAL-WRITE-FAILED", ErrorCategory::Io,
            "Could not replace material: " + path.generic_string(), "Check file permissions and disk space."));
    return Result<void>::success();
}
} // namespace engine
