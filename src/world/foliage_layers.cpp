#include "engine/world/foliage_layers.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>

namespace engine {
namespace {

EngineError foliage_layers_error(std::string code, std::string message) {
    return {std::move(code), Severity::Error, ErrorCategory::Validation, "foliage-layers", std::move(message),
            ENGINE_SOURCE_CONTEXT, {}, "Correct the foliage layer palette JSON.", make_correlation_id()};
}

bool positive(float value) { return std::isfinite(value) && value > 0.0f; }

FoliageScatterMode parse_scatter_mode(const std::string& value) {
    if (value == "discrete") return FoliageScatterMode::Discrete;
    return FoliageScatterMode::GroundCover;
}

std::string scatter_mode_to_string(FoliageScatterMode mode) {
    return mode == FoliageScatterMode::Discrete ? "discrete" : "ground_cover";
}

float default_bend_strength(const FoliageLayerDefinition& layer) {
    if (layer.scatter_mode == FoliageScatterMode::Discrete) return 0.08f;
    return layer.id == "flower" ? 0.1f : 0.35f;
}

float default_blade_height(const FoliageLayerDefinition& layer) {
    if (layer.scatter_mode == FoliageScatterMode::Discrete) return 0.95f;
    return 0.55f;
}

std::string foliage_mesh_key(const std::string& mesh_kind) {
    return "__engine/foliage/" + mesh_kind;
}

} // namespace

std::filesystem::path default_foliage_layers_path(const std::filesystem::path& project_root) {
    return project_root / "assets/foliage/ground-cover.layers.json";
}

Result<void> FoliageLayerPalette::validate() const {
    if (schema_version != 1)
        return Result<void>::failure(
            foliage_layers_error("FOLIAGE-LAYER-SCHEMA", "Only foliage layer schema version 1 is supported"));
    if (layers.empty())
        return Result<void>::failure(
            foliage_layers_error("FOLIAGE-LAYER-EMPTY", "Foliage layer palette must define at least one layer"));
    std::set<std::string> ids;
    for (const auto& layer : layers) {
        if (layer.id.empty() || layer.mesh_kind.empty())
            return Result<void>::failure(
                foliage_layers_error("FOLIAGE-LAYER-INVALID", "Each foliage layer requires id and meshKind"));
        if (!ids.insert(layer.id).second)
            return Result<void>::failure(
                foliage_layers_error("FOLIAGE-LAYER-DUPLICATE", "Foliage layer ids must be unique: " + layer.id));
        if (!positive(layer.scale_min) || !positive(layer.scale_max) || layer.scale_min > layer.scale_max)
            return Result<void>::failure(
                foliage_layers_error("FOLIAGE-LAYER-SCALE", "Foliage layer scale range is invalid"));
        if (!positive(layer.density_multiplier) || !positive(layer.max_slope_ratio))
            return Result<void>::failure(
                foliage_layers_error("FOLIAGE-LAYER-TUNING", "Foliage layer density and slope values must be positive"));
        if (!positive(layer.bend_radius) || !positive(layer.blade_height) || layer.bend_strength < 0.0f)
            return Result<void>::failure(
                foliage_layers_error("FOLIAGE-LAYER-BEND", "Foliage layer bend and blade height values are invalid"));
        if (layer.scatter_mode == FoliageScatterMode::Discrete && layer.discrete_min_density == 0)
            return Result<void>::failure(foliage_layers_error("FOLIAGE-LAYER-DISCRETE",
                "Discrete foliage layers require discreteMinDensity between 1 and 255"));
    }
    return Result<void>::success();
}

std::string FoliageLayerPalette::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", schema_version}, {"layers", nlohmann::ordered_json::array()}};
    for (const auto& layer : layers) {
        root["layers"].push_back({{"id", layer.id},
            {"label", layer.label.empty() ? layer.id : layer.label},
            {"meshKind", layer.mesh_kind},
            {"color", {layer.color[0], layer.color[1], layer.color[2]}},
            {"scaleMin", layer.scale_min},
            {"scaleMax", layer.scale_max},
            {"densityMultiplier", layer.density_multiplier},
            {"maxSlopeRatio", layer.max_slope_ratio},
            {"bendStrength", layer.bend_strength},
            {"bendRadius", layer.bend_radius},
            {"bladeHeight", layer.blade_height},
            {"scatterMode", scatter_mode_to_string(layer.scatter_mode)}});
        if (layer.scatter_mode == FoliageScatterMode::Discrete)
            root["layers"].back()["discreteMinDensity"] = layer.discrete_min_density;
        if (!layer.disturb_vfx_id.empty()) root["layers"].back()["disturbVfxId"] = layer.disturb_vfx_id;
    }
    return root.dump(2) + "\n";
}

Result<FoliageLayerPalette> FoliageLayerPalette::from_json(const std::string& text) {
    try {
        const auto root = nlohmann::json::parse(text);
        FoliageLayerPalette palette;
        palette.schema_version = root.value("schemaVersion", 0);
        if (!root.contains("layers") || !root["layers"].is_array())
            return Result<FoliageLayerPalette>::failure(
                foliage_layers_error("FOLIAGE-LAYER-SCHEMA", "Foliage layer palette requires a layers array"));
        for (const auto& entry : root["layers"]) {
            FoliageLayerDefinition layer;
            layer.id = entry.at("id").get<std::string>();
            layer.label = entry.value("label", layer.id);
            layer.mesh_kind = entry.at("meshKind").get<std::string>();
            if (entry.contains("color") && entry["color"].is_array() && entry["color"].size() >= 3) {
                layer.color[0] = entry["color"][0].get<float>();
                layer.color[1] = entry["color"][1].get<float>();
                layer.color[2] = entry["color"][2].get<float>();
            }
            layer.scale_min = entry.value("scaleMin", 0.6f);
            layer.scale_max = entry.value("scaleMax", 1.1f);
            layer.density_multiplier = entry.value("densityMultiplier", 0.04f);
            layer.max_slope_ratio = entry.value("maxSlopeRatio", 0.55f);
            layer.scatter_mode = parse_scatter_mode(entry.value("scatterMode", std::string{"ground_cover"}));
            layer.discrete_min_density = entry.value("discreteMinDensity", static_cast<std::uint8_t>(64));
            layer.bend_strength = entry.value("bendStrength", default_bend_strength(layer));
            layer.bend_radius = entry.value("bendRadius", 1.2f);
            layer.blade_height = entry.value("bladeHeight", default_blade_height(layer));
            layer.disturb_vfx_id = entry.value("disturbVfxId", std::string{});
            palette.layers.push_back(std::move(layer));
        }
        if (const auto valid = palette.validate(); !valid) return Result<FoliageLayerPalette>::failure(valid.error());
        return Result<FoliageLayerPalette>::success(std::move(palette));
    } catch (const std::exception& exception) {
        auto error = foliage_layers_error("FOLIAGE-LAYER-PARSE", "Foliage layer palette JSON is malformed");
        error.causes.push_back(exception.what());
        return Result<FoliageLayerPalette>::failure(std::move(error));
    }
}

Result<FoliageLayerPalette> FoliageLayerPalette::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return Result<FoliageLayerPalette>::failure(
            foliage_layers_error("FOLIAGE-LAYER-READ", "Could not read foliage layers: " + path.generic_string()));
    std::ostringstream text;
    text << input.rdbuf();
    return from_json(text.str());
}

Result<void> FoliageLayerPalette::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return Result<void>::success();
}

const FoliageLayerDefinition* FoliageLayerPalette::find_by_index(std::uint8_t index) const {
    if (index >= layers.size()) return nullptr;
    return &layers[static_cast<std::size_t>(index)];
}

std::string FoliageLayerPalette::mesh_key_for_layer(std::uint8_t index) const {
    const auto* layer = find_by_index(index);
    return layer ? foliage_mesh_key(layer->mesh_kind) : std::string{};
}

std::vector<std::pair<std::string, ImportedMesh>> build_foliage_layer_meshes(const FoliageLayerPalette& palette) {
    std::vector<std::pair<std::string, ImportedMesh>> meshes;
    std::set<std::string> seen;
    for (const auto& layer : palette.layers) {
        const auto key = foliage_mesh_key(layer.mesh_kind);
        if (!seen.insert(key).second) continue;
        if (const auto generated = generate_primitive_mesh(layer.mesh_kind, layer.color); generated)
            meshes.emplace_back(key, generated.value());
    }
    return meshes;
}

} // namespace engine
