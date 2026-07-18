cmake_minimum_required(VERSION 3.25)
project(AIRpgEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENGINE_BUILD_TESTS "Build engine tests" ON)
option(ENGINE_BUILD_GPU_TESTS "Build Direct3D 12 smoke tests" ON)
find_package(SDL3 CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(Lua REQUIRED)

add_library(engine_core
    src/core/error.cpp
    src/core/id_slug.cpp
    src/diagnostics/logger.cpp
    src/diagnostics/crash_bundle.cpp
    src/automation/automation_trace.cpp
    src/automation/command.cpp
    src/automation/editor_bridge.cpp
    src/automation/editor_session.cpp
    src/automation/mcp_server.cpp
    src/automation/world_forge_commands.cpp
    src/automation/project_git_commands.cpp
    src/rendering/render_app.cpp
    src/rendering/viewport_picking.cpp
    src/world/entity_id.cpp
    src/world/scene.cpp
    src/automation/scene_commands.cpp
    src/automation/terrain_edit_commands.cpp
    src/assets/asset_registry.cpp
    src/assets/material_asset.cpp
    src/assets/character_asset.cpp
    src/assets/camera_asset.cpp
    src/assets/play_session_settings.cpp
    src/assets/mesh_asset.cpp
    src/assets/png_decode.cpp
    src/assets/animation_clip_asset.cpp
    src/assets/animator_controller_asset.cpp
    src/animation/animator_runtime.cpp
    src/assets/prefab_asset.cpp
    src/assets/script_bindings_asset.cpp
    src/assets/hud_asset.cpp
    src/assets/ui_canvas_asset.cpp
    src/assets/ui_canvas_mutate.cpp
    src/assets/world_forge_factions_asset.cpp
    src/assets/world_forge_pantheon_asset.cpp
    src/assets/world_forge_archetypes_asset.cpp
    src/assets/world_forge_resources_asset.cpp
    src/assets/world_forge_relationships_asset.cpp
    src/assets/world_forge_map_asset.cpp
    src/assets/world_forge_quests_asset.cpp
    src/assets/world_forge_dialogues_asset.cpp
    src/dialogue/dialogue_runtime.cpp
    src/dialogue/dialogue_graph_edit.cpp
    src/dialogue/twee_import.cpp
    src/quest/quest_runtime.cpp
    src/standing/standing_runtime.cpp
    src/ui/hud_runtime.cpp
    src/ui/ui_canvas_stack.cpp
    src/ui/ui_canvas_editor.cpp
    src/ui/world_forge_editor.cpp
    src/ui/world_forge_graph_camera.cpp
    src/ui/imgui_png_texture.cpp
    src/world/world_partition.cpp
    src/world/cell_streamer.cpp
    src/world/cell_state.cpp
    src/world/terrain.cpp
    src/world/terrain_edits.cpp
    src/world/terrain_paint.cpp
    src/world/foliage_layers.cpp
    src/world/foliage_density.cpp
    src/world/foliage_scatter.cpp
    src/world/foliage_field.cpp
    src/world/world_influence.cpp
    src/world/terrain_field.cpp
    src/world/transform_utils.cpp
    src/world/prefab_collision.cpp
    src/world/authored_components.cpp
    src/world/interaction_volumes.cpp
    src/world/combat_volumes.cpp
    src/world/navigation_grid.cpp
    src/physics/collision_world.cpp
    src/physics/character_controller.cpp
    src/physics/rigidbody_locomotion.cpp
    src/rendering/debug_camera.cpp
    src/rendering/orbit_camera.cpp
    src/rendering/editor_fonts.cpp
    src/ui/game_fonts.cpp
    src/scripting/lua_runtime.cpp
    src/scripting/script_file_monitor.cpp
)
target_include_directories(engine_core PUBLIC include)
target_compile_definitions(engine_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN SDL_MAIN_HANDLED
    "ENGINE_REPOSITORY_ROOT=\"${CMAKE_SOURCE_DIR}\"")
target_link_libraries(engine_core PUBLIC SDL3::SDL3 EnTT::EnTT nlohmann_json::nlohmann_json Jolt::Jolt imgui::imgui imguizmo::imguizmo fastgltf::fastgltf ${LUA_LIBRARIES})
target_include_directories(engine_core PUBLIC ${LUA_INCLUDE_DIR})
if(MSVC)
    target_compile_options(engine_core PRIVATE /W4 /permissive- /EHsc /utf-8 /FS)
    target_link_libraries(engine_core PRIVATE dbghelp d3d12 dxgi dxguid d3dcompiler shell32 windowscodecs)
endif()

add_executable(engine src/app/main.cpp)
target_link_libraries(engine PRIVATE engine_core)

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_executable(engine_tests tests/foundation_tests.cpp)
    target_link_libraries(engine_tests PRIVATE engine_core)
    add_test(NAME regression_all COMMAND engine_tests)
    add_executable(engine_suite_tests tests/suite_tests.cpp)
    target_link_libraries(engine_suite_tests PRIVATE engine_core)
    foreach(suite IN ITEMS core world world_influence assets world_forge streaming terrain foliage collision navigation character interaction combat camera diagnostics scripting automation hud animator)
        add_test(NAME ${suite} COMMAND engine_suite_tests --suite ${suite} --json)
        set_tests_properties(${suite} PROPERTIES LABELS "${suite};unit")
    endforeach()
    add_test(NAME project_validation COMMAND engine validate --project ${CMAKE_SOURCE_DIR}/samples/open-world-rpg --json)
    set_tests_properties(project_validation PROPERTIES LABELS "cli;integration")
    if(ENGINE_BUILD_GPU_TESTS AND WIN32)
        add_test(NAME renderer_smoke COMMAND engine run
            --project ${CMAKE_SOURCE_DIR}/samples/open-world-rpg
            --frames 2 --hidden true --json)
        set_tests_properties(renderer_smoke PROPERTIES TIMEOUT 30)
        set_tests_properties(renderer_smoke PROPERTIES LABELS "gpu;integration")
        add_test(NAME debug_world_smoke COMMAND engine run --project ${CMAKE_SOURCE_DIR}/samples/open-world-rpg --debug-world --frames 30 --hidden true --json)
        set_tests_properties(debug_world_smoke PROPERTIES TIMEOUT 30 LABELS "gpu;physics;integration")
        add_test(NAME editor_smoke COMMAND engine editor --project ${CMAKE_SOURCE_DIR}/samples/open-world-rpg --frames 2 --hidden true --json)
        set_tests_properties(editor_smoke PROPERTIES TIMEOUT 30 LABELS "gpu;editor;integration")
        add_test(NAME editor_responsive_smoke COMMAND engine editor --project ${CMAKE_SOURCE_DIR}/samples/open-world-rpg --width 900 --height 600 --frames 2 --hidden true --json)
        set_tests_properties(editor_responsive_smoke PROPERTIES TIMEOUT 30 LABELS "gpu;editor;integration")
    endif()
endif()

install(TARGETS engine RUNTIME DESTINATION bin)
