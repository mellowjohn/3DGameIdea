#include "engine/automation/editor_session.h"

#include "engine/automation/water_edit_commands.h"
#include "engine/world/terrain.h"
#include "engine/world/water_store.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace engine {
namespace {

EngineError session_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "automation", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EditorBridgeResponse make_response(ExitCode exit_code, std::string summary,
    std::vector<std::string> changed = {}, std::vector<EngineError> diagnostics = {},
    std::map<std::string, std::string> metadata = {}) {
    EditorBridgeResponse response;
    response.exit_code = exit_code;
    response.summary = std::move(summary);
    response.changed_object_ids = std::move(changed);
    response.diagnostics = std::move(diagnostics);
    response.metadata = std::move(metadata);
    return response;
}

} // namespace

EditorBridgeResponse apply_water_operation(EditorSessionContext& context, const nlohmann::json& params) {
    const auto action = params.value("action", std::string{});

    if (action == "sample" || action == "sample_water") {
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, "sample requires x and z", {},
                {session_error("WATER-SAMPLE-ARGS", "sample requires x and z world coordinates.",
                    "Provide numeric x and z.")});
        }
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        if (!std::isfinite(x) || !std::isfinite(z)) {
            return make_response(ExitCode::InvalidArguments, "sample coordinates must be finite", {},
                {session_error("WATER-SAMPLE-FINITE", "sample x/z must be finite.", "Use finite world coordinates.")});
        }
        const WaterStore* store = context.water_store ? context.water_store : active_water_store();
        const WaterStore* previous = active_water_store();
        if (store) set_active_water_store(store);
        const auto surface = sample_water_surface_y(x, z);
        const float depth = water_depth(x, z);
        const bool deep = is_deep_water(x, z);
        if (store && store != previous) set_active_water_store(previous);
        std::map<std::string, std::string> metadata{
            {"x", std::to_string(x)},
            {"z", std::to_string(z)},
            {"hasWater", surface ? "true" : "false"},
            {"depth", std::to_string(depth)},
            {"deepWater", deep ? "true" : "false"},
        };
        if (surface) metadata["surfaceY"] = std::to_string(*surface);
        if (store) metadata["seaLevel"] = std::to_string(store->sea_level());
        return make_response(ExitCode::Success, surface ? "Water surface sampled" : "No water at sample point", {}, {},
            std::move(metadata));
    }

    if (context.test_session_active) {
        return make_response(ExitCode::Unavailable, "Water edits blocked during play test", {},
            {session_error("WATER-PLAY-BLOCKED", "Water apply is blocked while a play-test session is active.",
                "End the test session, then retry.")});
    }

    auto require_water = [&]() -> std::optional<EditorBridgeResponse> {
        if (!context.water_store || !context.water_history) {
            return make_response(ExitCode::Unavailable, "Water edit stores unavailable", {},
                {session_error("WATER-STORE-MISSING", "Water stores are not bound to the editor session.",
                    "Enable MCP connection in a running editor session.")});
        }
        return std::nullopt;
    };

    auto reload_now = [&]() {
        if (context.water_dirty) *context.water_dirty = true;
        if (context.reload_water) context.reload_water();
    };

    auto probe_cells = [](float x, float z, float radius, float cell_size) {
        std::set<CellCoord> probe;
        const int cell_extent = static_cast<int>(std::ceil(radius / cell_size)) + 1;
        const auto center = terrain_cell_for_position(x, z, cell_size);
        for (int dz = -cell_extent; dz <= cell_extent; ++dz)
            for (int dx = -cell_extent; dx <= cell_extent; ++dx)
                probe.insert({center.x + dx, center.z + dz});
        return probe;
    };

    auto snapshot_water = [&](const std::set<CellCoord>& cells) {
        std::map<CellCoord, WaterCellSnapshot> before;
        for (const auto& cell : cells) before[cell] = WaterCellSnapshot{context.water_store->cell_fill_or_empty(cell)};
        return before;
    };

    auto commit_water = [&](std::map<CellCoord, WaterCellSnapshot> before, bool do_reload) -> EditorBridgeResponse {
        std::map<CellCoord, WaterCellSnapshot> after;
        for (const auto& entry : before)
            after[entry.first] = WaterCellSnapshot{context.water_store->cell_fill_or_empty(entry.first)};
        bool changed = false;
        for (const auto& entry : after) {
            const auto found = before.find(entry.first);
            if (found == before.end() || found->second.fill != entry.second.fill) {
                changed = true;
                break;
            }
        }
        if (!changed)
            return make_response(ExitCode::Success, "Water brush touched no samples", {}, {}, {{"touchedCells", "0"}});
        const auto result = context.water_history->execute(*context.water_store,
            std::make_unique<WaterBrushStrokeCommand>(std::move(before), std::move(after)));
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        if (context.water_dirty) *context.water_dirty = true;
        if (do_reload && context.reload_water) context.reload_water();
        return make_response(ExitCode::Success, "Water stroke applied", {}, {}, {});
    };

    auto apply_water_op = [&](const nlohmann::json& op) -> Result<std::set<CellCoord>> {
        if (!op.contains("x") || !op.contains("z")) {
            return Result<std::set<CellCoord>>::failure(
                session_error("WATER-BRUSH-ARGS", "Water brush requires x and z.", "Provide world x/z."));
        }
        const float x = op["x"].get<float>();
        const float z = op["z"].get<float>();
        const float radius = op.value("radius", 4.0f);
        const float strength = op.value("strength", 0.28f);
        if (!std::isfinite(x) || !std::isfinite(z)) {
            return Result<std::set<CellCoord>>::failure(
                session_error("WATER-BRUSH-FINITE", "Brush x/z must be finite.", "Use finite world coordinates."));
        }
        const auto op_action = op.value("action", std::string{});
        const bool erase = op_action == "erase" || op.value("erase", false);
        if (erase) return context.water_store->apply_erase_brush(x, z, radius, strength);
        return context.water_store->apply_place_brush(x, z, radius, strength);
    };

    auto save_water = [&]() -> EditorBridgeResponse {
        if (!context.water_store) {
            return make_response(ExitCode::Unavailable, "No water store to save", {},
                {session_error("WATER-SAVE-MISSING", "Water store is not bound.",
                    "Enable MCP connection in a running editor session.")});
        }
        const auto path = default_water_surfaces_path(context.project_root);
        const auto result = context.water_store->save_atomic(path);
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        if (context.water_dirty) *context.water_dirty = false;
        return make_response(ExitCode::Success, "Water surfaces saved", {path.generic_string()});
    };

    constexpr std::size_t k_max_water_batch_ops = 200;
    if (action == "batch") {
        if (auto missing = require_water()) return *missing;
        if (!params.contains("ops") || !params["ops"].is_array()) {
            return make_response(ExitCode::InvalidArguments, "batch requires ops array", {},
                {session_error("WATER-BATCH-OPS", "batch requires an ops array.",
                    "Provide ops:[{action,x,z,...},...]")});
        }
        const auto& ops = params["ops"];
        if (ops.empty()) {
            return make_response(ExitCode::InvalidArguments, "batch ops is empty", {},
                {session_error("WATER-BATCH-EMPTY", "batch requires at least one operation.",
                    "Provide one or more water operations.")});
        }
        if (ops.size() > k_max_water_batch_ops) {
            return make_response(ExitCode::InvalidArguments, "batch exceeds operation limit", {},
                {session_error("WATER-BATCH-LIMIT",
                    "batch supports at most " + std::to_string(k_max_water_batch_ops) + " operations.",
                    "Split the request into smaller batches.")});
        }

        std::set<CellCoord> probe;
        for (const auto& op : ops) {
            if (!op.is_object()) continue;
            const float x = op.value("x", 0.0f);
            const float z = op.value("z", 0.0f);
            const float radius = op.value("radius", 4.0f);
            const auto cells = probe_cells(x, z, radius, WaterStore::k_cell_size);
            probe.insert(cells.begin(), cells.end());
        }
        auto before = snapshot_water(probe);
        std::size_t applied = 0;
        for (const auto& op : ops) {
            if (!op.is_object()) continue;
            const auto touched = apply_water_op(op);
            if (!touched) return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
            if (!touched.value().empty()) ++applied;
        }
        auto response = commit_water(std::move(before), false);
        if (response.exit_code != ExitCode::Success) return response;
        reload_now();
        response.summary = "Water batch applied";
        response.metadata["appliedCount"] = std::to_string(applied);
        response.metadata["waterChanged"] = applied > 0 ? "true" : "false";
        if (params.value("save", false)) {
            const auto saved = save_water();
            if (saved.exit_code != ExitCode::Success) return saved;
        }
        return response;
    }

    if (action == "place" || action == "erase") {
        if (auto missing = require_water()) return *missing;
        if (!params.contains("x") || !params.contains("z")) {
            return make_response(ExitCode::InvalidArguments, action + " requires x and z", {},
                {session_error("WATER-BRUSH-ARGS", "Water brush requires x and z.", "Provide world x/z.")});
        }
        const float x = params["x"].get<float>();
        const float z = params["z"].get<float>();
        const float radius = params.value("radius", 4.0f);
        auto before = snapshot_water(probe_cells(x, z, radius, WaterStore::k_cell_size));
        const auto touched = apply_water_op(params);
        if (!touched) return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
        auto response = commit_water(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.metadata["touchedCells"] = std::to_string(touched.value().size());
            std::vector<std::string> changed;
            for (const auto& cell : touched.value())
                changed.push_back(std::to_string(cell.x) + "," + std::to_string(cell.z));
            response.changed_object_ids = std::move(changed);
            if (touched.value().empty()) response.summary = "Water brush touched no samples";
        }
        return response;
    }

    if (action == "place_along") {
        if (auto missing = require_water()) return *missing;
        if (!params.contains("points") || !params["points"].is_array() || params["points"].empty()) {
            return make_response(ExitCode::InvalidArguments, "place_along requires points", {},
                {session_error("WATER-POINTS-ARGS", "place_along requires points:[{x,z},...].",
                    "Provide a polyline of world samples.")});
        }
        std::vector<std::array<float, 2>> controls;
        for (const auto& point : params["points"]) {
            if (!point.is_object() || !point.contains("x") || !point.contains("z")) {
                return make_response(ExitCode::InvalidArguments, "Invalid water point", {},
                    {session_error("WATER-POINT-INVALID", "Each point requires x and z.", "Use {x,z} objects.")});
            }
            const float px = point["x"].get<float>();
            const float pz = point["z"].get<float>();
            if (!std::isfinite(px) || !std::isfinite(pz)) {
                return make_response(ExitCode::InvalidArguments, "Water point must be finite", {},
                    {session_error("WATER-POINT-FINITE", "Point coordinates must be finite.", "Use finite x/z.")});
            }
            controls.push_back({px, pz});
        }
        const float step = std::max(0.5f, params.value("step", 2.5f));
        const float radius = params.value("radius", 3.8f);
        const float strength = params.value("strength", 1.0f);
        std::vector<std::array<float, 2>> pts;
        for (std::size_t i = 0; i + 1 < controls.size(); ++i) {
            const float x0 = controls[i][0];
            const float z0 = controls[i][1];
            const float x1 = controls[i + 1][0];
            const float z1 = controls[i + 1][1];
            const float seg = std::hypot(x1 - x0, z1 - z0);
            const int n = std::max(1, static_cast<int>(std::ceil(seg / step)));
            for (int k = 0; k < n; ++k) {
                const float t = static_cast<float>(k) / static_cast<float>(n);
                pts.push_back({x0 + (x1 - x0) * t, z0 + (z1 - z0) * t});
            }
        }
        pts.push_back(controls.back());

        std::set<CellCoord> probe;
        for (const auto& p : pts) {
            auto local = probe_cells(p[0], p[1], radius, WaterStore::k_cell_size);
            probe.insert(local.begin(), local.end());
        }
        auto before = snapshot_water(probe);
        std::set<CellCoord> touched_all;
        for (const auto& p : pts) {
            const auto touched = context.water_store->apply_place_brush(p[0], p[1], radius, strength);
            if (!touched)
                return make_response(ExitCode::ValidationFailed, touched.error().message, {}, {touched.error()});
            touched_all.insert(touched.value().begin(), touched.value().end());
        }
        auto response = commit_water(std::move(before), true);
        if (response.exit_code == ExitCode::Success) {
            response.summary = "Water placed along polyline";
            response.metadata["touchedCells"] = std::to_string(touched_all.size());
            response.metadata["pointCount"] = std::to_string(pts.size());
            if (params.value("save", false)) {
                const auto saved = save_water();
                if (saved.exit_code != ExitCode::Success) return saved;
            }
        }
        return response;
    }

    if (action == "undo") {
        if (auto missing = require_water()) return *missing;
        if (context.water_history->undo_size() == 0) {
            return make_response(ExitCode::ValidationFailed, "No water undo", {},
                {session_error("WATER-UNDO-EMPTY", "No water strokes to undo.", "Apply a water stroke first.")});
        }
        const auto result = context.water_history->undo(*context.water_store);
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        reload_now();
        return make_response(ExitCode::Success, context.water_history->last_summary());
    }

    if (action == "redo") {
        if (auto missing = require_water()) return *missing;
        if (context.water_history->redo_size() == 0) {
            return make_response(ExitCode::ValidationFailed, "No water redo", {},
                {session_error("WATER-REDO-EMPTY", "No water strokes to redo.", "Undo a water stroke first.")});
        }
        const auto result = context.water_history->redo(*context.water_store);
        if (!result) return make_response(ExitCode::ValidationFailed, result.error().message, {}, {result.error()});
        reload_now();
        return make_response(ExitCode::Success, context.water_history->last_summary());
    }

    if (action == "save") {
        if (auto missing = require_water()) return *missing;
        return save_water();
    }

    return make_response(ExitCode::InvalidArguments, "Unknown water action: " + action, {},
        {session_error("WATER-ACTION-UNKNOWN", "Unsupported water action: " + action,
            "Use place, erase, place_along, sample, undo, redo, save, or batch.")});
}

} // namespace engine
