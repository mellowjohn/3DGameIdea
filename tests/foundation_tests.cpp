#include "engine/automation/command.h"
#include "engine/automation/editor_screenshot.h"
#include "engine/core/result.h"
#include "engine/assets/asset_registry.h"
#include "engine/automation/scene_commands.h"
#include "engine/world/scene.h"
#include "engine/world/cell_streamer.h"
#include "engine/world/world_partition.h"
#include "engine/world/cell_state.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>

namespace {
int failures = 0;
void check(bool condition, const char* message) {
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main() {
    using namespace engine;
    auto success = Result<int>::success(42);
    check(success && success.value() == 42, "Result preserves success value");

    EngineError sample{"TEST-1", Severity::Error, ErrorCategory::Validation, "test", "bad input",
                       std::nullopt, {"cause"}, "fix it", "abc"};
    auto failure = Result<int>::failure(sample);
    check(!failure && failure.error().code == "TEST-1", "Result preserves error");
    check(sample.to_json().find("\"category\":\"validation\"") != std::string::npos,
          "Error JSON contains stable category");

    const auto project = std::filesystem::temp_directory_path() / "ai-rpg-engine-test";
    std::filesystem::create_directories(project);
    std::ofstream(project / "project.engine.json") << "{\"schemaVersion\":1,\"projectId\":\"test\",\"name\":\"Test\"}";
    CommandRequest request{"validate", project, true, false, {}, "test-correlation"};
    auto response = execute_command(request);
    check(response.exit_code == ExitCode::Success, "Valid project passes validation");
    check(response.to_json().find("\"schemaVersion\":1") != std::string::npos,
          "Command response has schema version");
    check(response.to_json().find("\"assets\":0") != std::string::npos &&
              response.to_json().find("\"entities\":0") != std::string::npos,
          "Project validation exposes structured asset and entity metrics");
    CommandResponse help{ExitCode::Success, command_help(), {}, {}};
    check(help.to_json().find("Engine 0.2.0\\nCommands") != std::string::npos,
          "Command response escapes newlines");
    std::ofstream(project / "project.engine.json", std::ios::trunc) << "{}";
    response = execute_command(request);
    check(response.exit_code == ExitCode::ValidationFailed &&
              response.diagnostics.front().code == "PROJECT-MANIFEST-INVALID",
          "Invalid manifest returns stable validation error");

    std::ofstream(project / "project.engine.json", std::ios::trunc) << "{\"schemaVersion\":1,\"projectId\":\"test\",\"name\":\"Test\"}";
    CommandRequest missing_suite{"test", project, true, false, {}, "test-correlation"};
    response = execute_command(missing_suite);
    check(response.exit_code == ExitCode::InvalidArguments &&
              response.diagnostics.front().code == "CLI-TEST-SUITE-REQUIRED",
          "Test command requires a suite");
    CommandRequest unknown_suite{"test", project, true, false, {"--suite", "unknown"}, "test-correlation"};
    response = execute_command(unknown_suite);
    check(response.exit_code == ExitCode::InvalidArguments &&
              response.diagnostics.front().code == "CLI-TEST-SUITE-UNKNOWN",
          "Test command rejects an unknown suite");
    CommandRequest dry_run_suite{"test", project, true, true, {"--suite", "core"}, "test-correlation"};
    response = execute_command(dry_run_suite);
    check(response.exit_code == ExitCode::Success && response.metadata.at("suite") == "core" &&
              response.metrics.at("testCount") == 1.0,
          "Test command dry run reports the selected suite");

    auto invalid_id = EntityId::parse("not-a-uuid");
    check(!invalid_id && invalid_id.error().code == "WORLD-INVALID-UUID", "Malformed UUID is rejected");
    auto root_id = EntityId::parse("00000000-0000-4000-8000-000000000001");
    auto child_id = EntityId::parse("00000000-0000-4000-8000-000000000002");
    check(root_id && child_id, "Canonical UUIDs parse");

    Scene scene;
    auto root = scene.create_entity("Root", root_id.value());
    auto child = scene.create_entity("Child", child_id.value());
    check(root && child && scene.size() == 2, "Entities are created with stable UUIDs");
    auto duplicate = scene.create_entity("Duplicate", root_id.value());
    check(!duplicate && duplicate.error().code == "WORLD-DUPLICATE-UUID", "Duplicate UUID is rejected");
    check(scene.set_parent(child.value(), root.value()).has_value(), "Valid hierarchy is accepted");
    auto cycle = scene.set_parent(root.value(), child.value());
    check(!cycle && cycle.error().code == "WORLD-HIERARCHY-CYCLE", "Hierarchy cycle is rejected");
    auto self_parent = scene.set_parent(root.value(), root.value());
    check(!self_parent && self_parent.error().code == "WORLD-HIERARCHY-SELF", "Self parenting is rejected");

    TransformComponent invalid_transform;
    invalid_transform.position[0] = std::numeric_limits<float>::quiet_NaN();
    check(!scene.set_transform(root.value(), invalid_transform), "Non-finite transform is rejected");
    invalid_transform = TransformComponent{};
    invalid_transform.scale[1] = 0.0f;
    check(!scene.set_transform(root.value(), invalid_transform), "Zero transform scale is rejected");

    const auto serialized = scene.to_json();
    auto restored = Scene::from_json(serialized);
    check(restored && restored.value().to_json() == serialized, "Scene serialization is deterministic and round trips");
    check(!Scene::from_json("{broken"), "Malformed scene JSON is rejected");

    CommandHistory history;
    check(!history.undo(scene), "Empty undo is rejected");
    check(history.execute(scene, std::make_unique<RenameEntityCommand>(root.value(), "Renamed")).has_value(), "Rename command applies");
    check(scene.name(root.value()) == std::optional<std::string>("Renamed"), "Rename command changes scene");
    check(history.undo(scene) && scene.name(root.value()) == std::optional<std::string>("Root"), "Undo restores name");
    check(history.redo(scene) && scene.name(root.value()) == std::optional<std::string>("Renamed"), "Redo reapplies name");
    check(history.undo(scene).has_value(), "Second undo succeeds");
    check(history.execute(scene, std::make_unique<RenameEntityCommand>(root.value(), "Branch")).has_value(), "New command after undo succeeds");
    check(history.redo_size() == 0 && !history.redo(scene), "New command clears redo branch");
    check(history.execute(scene, std::make_unique<ReparentEntityCommand>(child.value(), std::nullopt)).has_value(), "Reparent command applies");
    check(!scene.parent(child.value()).has_value(), "Reparent command detaches child");
    check(history.undo(scene) && scene.parent(child.value()) == std::optional<EntityId>(root.value()), "Undo restores parent");

    const auto scene_file = project / "worlds" / "test.world.json";
    check(scene.save_atomic(scene_file).has_value(), "Atomic scene save succeeds");
    check(Scene::load(scene_file).has_value(), "Saved scene loads");
    check(scene.save_atomic(scene_file) && std::filesystem::exists(scene_file.string() + ".bak"), "Repeated save preserves backup");
    check(scene.destroy_entity(root.value()).has_value() && !scene.parent(child.value()).has_value(), "Destroying parent safely detaches children");
    check(!scene.destroy_entity(root.value()), "Destroying a missing entity is rejected");

    Scene prefab;
    auto prefab_root = prefab.create_entity("Prefab Root", root_id.value());
    auto prefab_child = prefab.create_entity("Prefab Child", child_id.value());
    check(prefab_root && prefab_child && prefab.set_parent(prefab_child.value(), prefab_root.value()), "Prefab hierarchy is created");
    const auto before_prefab = scene.size();
    auto instance = scene.instantiate_prefab(prefab);
    check(instance && instance.value().size() == 2 && scene.size() == before_prefab + 2, "Prefab instantiates with fresh UUIDs");
    check(instance && scene.parent(instance.value()[1]) == std::optional<EntityId>(instance.value()[0]), "Prefab hierarchy remaps to instance UUIDs");
    check(instance && instance.value()[0] != prefab_root.value(), "Prefab source UUID is not reused");

    std::filesystem::create_directories(project / "assets");
    std::ofstream(project / "assets" / "hero.txt") << "hero";
    std::ofstream(project / "assets" / "hero.txt.meta") << "{\"dependencies\":[\"assets/missing.txt\"]}";
    AssetRegistry registry;
    check(registry.scan(project).has_value(), "Asset registry scan succeeds");
    auto asset_errors = registry.validate();
    check(asset_errors.size() == 1 && asset_errors[0].code == "ASSET-DEPENDENCY-MISSING", "Missing asset dependency is detected");
    std::ofstream(project / "assets" / "missing.txt") << "dependency";
    check(registry.scan(project) && registry.validate().empty(), "Resolved dependency passes validation");
    AssetRegistry previous;
    check(previous.scan(project).has_value(), "Asset snapshot is captured");
    std::ofstream(project / "assets" / "hero.txt", std::ios::trunc) << "changed hero";
    check(registry.scan(project).has_value(), "Changed asset rescans");
    auto changes = registry.diff(previous);
    check(changes.size() == 1 && changes[0].kind == AssetChangeKind::Modified, "Content hash detects modified asset");
    const auto database = project / "out" / "assets" / "registry.json";
    auto first_write = registry.write_database_if_changed(database);
    auto second_write = registry.write_database_if_changed(database);
    check(first_write && first_write.value() && second_write && !second_write.value(), "Asset database rebuild is incremental");
    std::ofstream(project / "assets" / "hero.txt.meta", std::ios::trunc) << "{\"dependencies\":[\"assets/missing.txt\"]}";
    std::ofstream(project / "assets" / "missing.txt.meta", std::ios::trunc) << "{\"dependencies\":[\"assets/hero.txt\"]}";
    check(registry.scan(project).has_value(), "Cyclic dependency metadata scans");
    asset_errors = registry.validate();
    check(std::any_of(asset_errors.begin(), asset_errors.end(), [](const EngineError& error) { return error.code == "ASSET-DEPENDENCY-CYCLE"; }),
          "Circular asset dependency is detected");
    AssetMonitor monitor;
    check(!monitor.poll(project), "Monitor rejects invalid dependency snapshots");
    std::ofstream(project / "assets" / "hero.txt.meta", std::ios::trunc) << "{\"dependencies\":[]}";
    std::ofstream(project / "assets" / "missing.txt.meta", std::ios::trunc) << "{\"dependencies\":[]}";
    auto initial_changes = monitor.poll(project);
    check(initial_changes && initial_changes.value().size() == 2, "Monitor accepts valid initial snapshot");
    std::ofstream(project / "assets" / "hero.txt", std::ios::app) << " again";
    auto hot_reload_changes = monitor.poll(project);
    check(hot_reload_changes && hot_reload_changes.value().size() == 1 &&
              hot_reload_changes.value()[0].kind == AssetChangeKind::Modified,
          "Monitor emits validated hot-reload change set");

    WorldPartition partition;
    auto zero_cell = partition.cell_for({0.0, 0.0, 0.0});
    auto negative_cell = partition.cell_for({-0.01, 0.0, -128.0});
    check(zero_cell && zero_cell.value() == CellCoord{0, 0}, "Origin maps to cell zero");
    check(negative_cell && negative_cell.value() == CellCoord{-1, -1}, "Negative cell coordinates use floor semantics");
    check(!partition.cell_for({2000.01, 0.0, 0.0}), "Out-of-bounds world position is rejected");
    check(!partition.rebase_if_needed({100.0, 0.0, 100.0}), "Nearby focus does not rebase origin");
    check(partition.rebase_if_needed({600.0, 12.0, 0.0}), "Distant focus rebases origin");
    const auto local = partition.to_local({600.0, 12.0, 0.0});
    check(std::abs(local.x) < 128.0f && local.y == 0.0f, "Rebased local position remains bounded");

    CellStreamer streamer([](CellCoord coordinate, const std::atomic_bool& cancelled) -> Result<CellData> {
        if (cancelled.load()) return Result<CellData>::failure(EngineError{"STREAM-CANCELLED", Severity::Info, ErrorCategory::Io,
            "test", "cancelled", std::nullopt, {}, {}, make_correlation_id()});
        return Result<CellData>::success(CellData{coordinate, 1, "cell"});
    });
    check(!streamer.request({0, 0}, 33), "Unsafe streaming radius is rejected");
    check(streamer.request({0, 0}, 1).has_value(), "Streaming request is accepted");
    auto stream_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (streamer.loaded_count() < 9 && std::chrono::steady_clock::now() < stream_deadline) { (void)streamer.update(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    check(streamer.loaded_count() == 9 && streamer.loaded({0, 0}), "Radius-one request commits nine validated cells");
    check(streamer.request({10, 10}, 0).has_value(), "Rapid traversal starts new generation");
    stream_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!streamer.loaded({10, 10}) && std::chrono::steady_clock::now() < stream_deadline) { (void)streamer.update(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    check(streamer.loaded_count() == 1 && streamer.loaded({10, 10}) && !streamer.loaded({0, 0}), "New generation unloads stale cells");

    CellStreamer corrupt([](CellCoord coordinate, const std::atomic_bool&) {
        return Result<CellData>::success(CellData{{coordinate.x + 1, coordinate.z}, 99, "bad"});
    });
    check(corrupt.request({2, 2}, 0).has_value(), "Corrupt-cell test request starts");
    std::vector<EngineError> stream_errors;
    stream_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (stream_errors.empty() && std::chrono::steady_clock::now() < stream_deadline) { stream_errors = corrupt.update(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    check(!stream_errors.empty() && stream_errors[0].code == "STREAM-CELL-INVALID" && corrupt.loaded_count() == 0,
          "Mismatched cell identity and schema never commit");
    TerrainTileMetadata terrain{{0,0},129,-20.0f,80.0f,"assets/terrain/0_0.height",{}};
    check(terrain.validate().has_value(), "Valid terrain metadata passes"); terrain.height_resolution=128;
    check(!terrain.validate(), "Invalid terrain resolution is rejected");
    auto bubble=simulation_bubble({0,0},2); check(bubble&&bubble.value().size()==13, "Simulation bubble is deterministic");
    check(!simulation_bubble({0,0},33), "Oversized simulation bubble is rejected");
    CellStateStore states; states.set({-1,2},"chest.open","true"); auto states2=CellStateStore::from_json(states.to_json());
    check(states2&&states2.value().get({-1,2},"chest.open")==std::optional<std::string>("true"), "Cell state round trips");
    check(!CellStateStore::from_json("{bad"), "Malformed cell state is rejected");
    auto state_path=project/"saves"/"cells.json"; check(states.save_atomic(state_path)&&CellStateStore::load(state_path), "Cell state saves atomically");

    {
        const std::uint8_t rgba[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
        const auto png = write_rgba_png(project, "shot-rgba-test", 2, 2, rgba);
        check(png && std::filesystem::exists(png.value()) && std::filesystem::file_size(png.value()) > 32,
            "write_rgba_png encodes a tiny RGBA buffer");
    }

    std::filesystem::remove_all(project);

    std::cout << (failures == 0 ? "All foundation tests passed\n" : "Foundation tests failed\n");
    return failures == 0 ? 0 : 1;
}
