#include "engine/rendering/render_app.h"

#include "engine/diagnostics/logger.h"
#include "engine/diagnostics/gpu_diagnostics.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/character_asset.h"
#include "engine/assets/camera_asset.h"
#include "engine/assets/play_session_settings.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/character_controller.h"
#include "engine/physics/rigidbody_locomotion.h"
#include "engine/rendering/debug_camera.h"
#include "engine/rendering/orbit_camera.h"
#include "engine/rendering/pbr_lighting.h"
#include "engine/rendering/viewport_picking.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_field.h"
#include "engine/world/water_field.h"
#include "engine/world/water_store.h"
#include "engine/automation/water_edit_commands.h"
#include "engine/world/transform_utils.h"
#include "engine/world/prefab_collision.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/combat_volumes.h"
#include "engine/editor/editor_fonts.h"
#include "engine/editor/editor_icons.h"
#include "engine/ui/game_fonts.h"
#include "engine/assets/asset_registry.h"
#include "engine/assets/animator_controller_asset.h"
#include "engine/automation/scene_commands.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/automation_trace.h"
#include "engine/automation/editor_session.h"
#include "engine/automation/editor_screenshot.h"
#include "engine/automation/live_automation_control.h"
#include "engine/automation/project_git_commands.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/scripting/script_file_monitor.h"
#include "engine/quest/quest_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/hud_asset.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_editor.h"
#include "engine/ui/editor_ui_hotspots.h"
#include "engine/ui/editor_chrome.h"
#include "engine/ui/world_forge_editor.h"
#include "engine/assets/world_forge_acts.h"
#include "engine/ui/imgui_png_texture.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_field.h"
#include "engine/world/world_influence.h"
#include "engine/world/scene.h"
#include "engine/world/authored_components.h"

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl3.h>
#include <ImGuizmo.h>
#include <nlohmann/json.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace engine {
namespace {
using Microsoft::WRL::ComPtr;

bool path_ends_with(std::string_view path, std::string_view suffix) {
    return path.size() >= suffix.size() &&
           path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool is_player_hud_canvas_path(const std::filesystem::path& path) {
    const auto name = path.filename().generic_string();
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower == "player.uicanvas.json";
}

void hot_reload_ui_canvas_file(UiCanvasStack& stack, const std::filesystem::path& absolute) {
    if (is_player_hud_canvas_path(absolute)) {
        const double health = stack.hud().get_number("player.health").value_or(100.0);
        const double health_max = stack.hud().get_number("player.healthMax").value_or(100.0);
        if (stack.set_hud(absolute)) stack.hud().set_health(health, health_max);
        return;
    }
    const auto loaded = UiCanvasAsset::load(absolute);
    if (!loaded) return;
    std::string id = loaded.value().id;
    if (id.empty()) {
        id = absolute.stem().generic_string();
        if (id.size() > 9 && id.substr(id.size() - 9) == ".uicanvas") id = id.substr(0, id.size() - 9);
    }
    (void)stack.register_canvas(id, absolute);
}

constexpr UINT frame_count = 2;
struct Vertex { float x,y,z,r,g,b,u=0,v=0; };
struct RenderInstance {
    TransformComponent transform;
    std::string mesh_asset;
    PbrSurfaceParams pbr = PbrSurfaceParams::dielectric_default();
};

std::array<float, 24> pack_object_constants(const std::array<float, 16>& model, const PbrSurfaceParams& pbr,
    float use_albedo = 0.0f) {
    std::array<float, 24> constants{};
    std::memcpy(constants.data(), model.data(), sizeof(model));
    constants[16] = pbr.roughness;
    constants[17] = pbr.metallic;
    constants[18] = use_albedo; // materialParams.z toggles GPU albedo sampling
    constants[19] = 0.0f;
    constants[20] = pbr.emissive[0];
    constants[21] = pbr.emissive[1];
    constants[22] = pbr.emissive[2];
    constants[23] = 0.0f;
    return constants;
}

const char* k_pbr_hlsl_helpers = R"(
            float3 fresnelSchlick(float cosTheta, float3 F0) {
                return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
            }
            float distributionGGX(float NdotH, float roughness) {
                float a = roughness * roughness;
                float a2 = a * a;
                float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
                return a2 / max(3.14159265 * d * d, 0.000001);
            }
            float geometrySchlickGGX(float NdotX, float roughness) {
                float r = roughness + 1.0;
                float k = (r * r) / 8.0;
                return NdotX / max(NdotX * (1.0 - k) + k, 0.0001);
            }
            float3 shadePbr(float3 albedo, float roughness, float metallic, float3 N, float3 V, float3 L, float3 lightRadiance) {
                if (!all(isfinite(N)) || !all(isfinite(V)) || !all(isfinite(L))) return 0.0;
                float NdotL = saturate(dot(N, L));
                if (NdotL <= 0.0) return 0.0;
                float3 H = normalize(L + V);
                float NdotV = max(dot(N, V), 0.0001);
                float NdotH = saturate(dot(N, H));
                float VdotH = saturate(dot(V, H));
                float rough = max(roughness, 0.04);
                float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
                float3 F = fresnelSchlick(VdotH, F0);
                float D = distributionGGX(NdotH, rough);
                float G = geometrySchlickGGX(NdotV, rough) * geometrySchlickGGX(NdotL, rough);
                float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);
                float3 diffuse = albedo * (1.0 - metallic);
                return (diffuse + specular) * lightRadiance * NdotL;
            }
            float3 applyPointLightPbr(float3 worldPos, float3 albedo, float roughness, float metallic, float3 N, float3 V,
                float4 posRadius, float4 colorStrength) {
                if (colorStrength.w <= 0.0) return 0.0;
                float3 toLight = posRadius.xyz - worldPos;
                float lightDist = length(toLight);
                float atten = saturate(1.0 - lightDist / max(posRadius.w, 0.001));
                atten *= atten;
                float3 L = toLight / max(lightDist, 0.001);
                return shadePbr(albedo, roughness, metallic, N, V, L, colorStrength.rgb * colorStrength.w * atten);
            }
)";

struct FoliageGpuDraw {
    std::string mesh_key;
    UINT instance_offset = 0;
    UINT instance_count = 0;
    float bend_strength = 0.35f;
    float bend_radius = 1.2f;
    float blade_height = 0.55f;
    float center_x = 0.0f;
    float center_z = 0.0f;
};

EngineError graphics_error(std::string code, std::string message, HRESULT result = S_OK) {
    std::vector<std::string> causes;
    if (FAILED(result)) {
        std::ostringstream value;
        value << "HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
        causes.push_back(value.str());
    }
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Graphics, "rendering",
                       std::move(message), ENGINE_SOURCE_CONTEXT, std::move(causes),
                       "Check the D3D12 debug output and graphics driver.", make_correlation_id()};
}

std::string normalize_asset_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return path;
}

constexpr std::size_t k_max_point_lights = 2;

// SSAO v1 tuning defaults (world-space depth-based AO); see context/planning/tickets/TICKET-0042.md.
constexpr float k_ssao_radius = 0.65f;
constexpr float k_ssao_bias = 0.035f;
constexpr float k_ssao_intensity = 0.55f;

struct ActivePointLight {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float radius = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float strength = 0.0f;
};

void append_fullscreen_sky_triangle(std::vector<Vertex>& vertices) {
    vertices.push_back({-1.0f, -1.0f, 0.99999f, -1.0f, 0.0f, 0.0f});
    vertices.push_back({3.0f, -1.0f, 0.99999f, -1.0f, 0.0f, 0.0f});
    vertices.push_back({-1.0f, 3.0f, 0.99999f, -1.0f, 0.0f, 0.0f});
}

void append_debug_ground_quad(std::vector<Vertex>& vertices) {
    vertices.push_back({-20, 0, -20, .12f, .18f, .22f});
    vertices.push_back({-20, 0, 20, .12f, .18f, .22f});
    vertices.push_back({20, 0, 20, .12f, .18f, .22f});
    vertices.push_back({-20, 0, -20, .12f, .18f, .22f});
    vertices.push_back({20, 0, 20, .12f, .18f, .22f});
    vertices.push_back({20, 0, -20, .12f, .18f, .22f});
}

void append_physics_cube(std::vector<Vertex>& vertices) {
    const float p[8][3] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
        {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}};
    const int faces[6][6] = {{0, 2, 1, 0, 3, 2}, {4, 5, 6, 4, 6, 7}, {0, 4, 7, 0, 7, 3}, {1, 2, 6, 1, 6, 5},
        {3, 7, 6, 3, 6, 2}, {0, 1, 5, 0, 5, 4}};
    const float colors[6][3] = {{.2f, .8f, 1}, {.8f, .2f, .9f}, {.1f, 1, .45f}, {1, .6f, .1f}, {.4f, .5f, 1}, {1, .25f, .25f}};
    for (int face = 0; face < 6; ++face) {
        for (int vertex = 0; vertex < 6; ++vertex) {
            const auto point = p[faces[face][vertex]];
            vertices.push_back({point[0], point[1], point[2], colors[face][0], colors[face][1], colors[face][2]});
        }
    }
}

void append_imported_mesh_vertices(std::vector<Vertex>& vertices,
    const std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes,
    std::map<std::string, std::pair<UINT, UINT>>& mesh_ranges) {
    for (const auto& imported : imported_meshes) {
        const UINT offset = static_cast<UINT>(vertices.size());
        for (const auto& vertex : imported.second.vertices)
            vertices.push_back({vertex.x, vertex.y, vertex.z, vertex.r, vertex.g, vertex.b, vertex.u, vertex.v});
        mesh_ranges[normalize_asset_path(imported.first)] = {offset, static_cast<UINT>(imported.second.vertices.size())};
    }
}

std::string resolve_prefab_mesh(const std::map<std::string, PrefabAsset>& prefab_catalog, const std::string& prefab_asset) {
    const auto resolved = resolve_prefab_catalog_path(prefab_catalog, prefab_asset);
    const auto found = prefab_catalog.find(resolved);
    if (found == prefab_catalog.end()) return {};
    if (found->second.is_compositional()) return {};
    return normalize_asset_path(found->second.mesh);
}

PbrSurfaceParams resolve_part_pbr(const PrefabMeshSource& mesh, const PrefabAsset::MaterialLookup& lookup_material,
    bool& skip_draw) {
    skip_draw = false;
    if (!mesh.material || !lookup_material) return PbrSurfaceParams::dielectric_default();
    const MaterialAsset* material = lookup_material(*mesh.material);
    if (!material) return PbrSurfaceParams::dielectric_default();
    if (!material_supports_opaque_pbr_runtime(*material)) {
        skip_draw = true;
        return PbrSurfaceParams::dielectric_default();
    }
    return PbrSurfaceParams::from_material(*material);
}

void expand_prefab_render_instances(const PrefabAsset& prefab, const TransformComponent& placement_transform,
    std::vector<RenderInstance>& instances, const PrefabAsset::MaterialLookup& lookup_material = {}) {
    if (!prefab.is_compositional()) {
        if (!prefab.mesh.empty())
            instances.push_back({placement_transform, normalize_asset_path(prefab.mesh),
                PbrSurfaceParams::dielectric_default()});
        return;
    }
    for (const auto& part : prefab.parts) {
        bool skip_draw = false;
        const auto pbr = resolve_part_pbr(part.mesh, lookup_material, skip_draw);
        if (skip_draw) continue;
        const auto mesh_key = prefab.mesh_key_for_part(part, lookup_material);
        if (mesh_key.empty()) continue;
        instances.push_back({engine::multiply_transforms(placement_transform, part.transform), mesh_key, pbr});
    }
}

const PrefabAsset* find_prefab(const std::map<std::string, PrefabAsset>& prefab_catalog, const std::string& prefab_asset) {
    return find_prefab_in_catalog(prefab_catalog, prefab_asset);
}

std::vector<ActivePointLight> collect_point_lights(const std::map<std::string, PrefabAsset>& prefab_catalog,
    const std::vector<std::pair<std::string, TransformComponent>>& placements) {
    std::vector<ActivePointLight> lights;
    lights.reserve(placements.size());
    for (const auto& placement : placements) {
        const auto normalized = resolve_prefab_catalog_path(prefab_catalog, placement.first);
        const auto found = prefab_catalog.find(normalized);
        if (found == prefab_catalog.end() || !found->second.light) continue;
        const auto& spec = *found->second.light;
        const auto& transform = placement.second;
        lights.push_back({transform.position[0] + spec.offset[0] * transform.scale[0],
                          transform.position[1] + spec.offset[1] * transform.scale[1],
                          transform.position[2] + spec.offset[2] * transform.scale[2], spec.radius, spec.color[0],
                          spec.color[1], spec.color[2], spec.strength});
    }
    return lights;
}

void pack_point_lights(std::array<float, 48>& frame_constants, const std::vector<ActivePointLight>& lights,
    const std::array<float, 3>& camera_position) {
    std::vector<ActivePointLight> sorted = lights;
    std::sort(sorted.begin(), sorted.end(), [&](const ActivePointLight& left, const ActivePointLight& right) {
        const float left_dx = left.x - camera_position[0];
        const float left_dy = left.y - camera_position[1];
        const float left_dz = left.z - camera_position[2];
        const float right_dx = right.x - camera_position[0];
        const float right_dy = right.y - camera_position[1];
        const float right_dz = right.z - camera_position[2];
        return (left_dx * left_dx + left_dy * left_dy + left_dz * left_dz) <
               (right_dx * right_dx + right_dy * right_dy + right_dz * right_dz);
    });
    for (std::size_t index = 0; index < k_max_point_lights; ++index) {
        const std::size_t base = 28 + index * 8;
        if (index < sorted.size()) {
            frame_constants[base + 0] = sorted[index].x;
            frame_constants[base + 1] = sorted[index].y;
            frame_constants[base + 2] = sorted[index].z;
            frame_constants[base + 3] = sorted[index].radius;
            frame_constants[base + 4] = sorted[index].r;
            frame_constants[base + 5] = sorted[index].g;
            frame_constants[base + 6] = sorted[index].b;
            frame_constants[base + 7] = sorted[index].strength;
        } else {
            for (std::size_t slot = 0; slot < 8; ++slot) frame_constants[base + slot] = 0.0f;
        }
    }
}

std::string utf8(const wchar_t* value) {
    if (!value || !*value) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return {};
    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, output.data(), size, nullptr, nullptr);
    output.pop_back();
    return output;
}

class Renderer final {
public:
    ~Renderer() { shutdown(); }

    Result<void> initialize(SDL_Window* window, bool debug_layer, bool debug_world, bool editor, bool hidden, const MaterialAsset& terrain_material, const std::vector<std::pair<std::string,ImportedMesh>>& imported_meshes) {
        editor_requested_=editor;
        debug_world_=debug_world;
        const auto properties = SDL_GetWindowProperties(window);
        hwnd_ = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd_) return Result<void>::failure(graphics_error("GFX-WINDOW-HANDLE", "SDL did not provide a Win32 window handle"));

        UINT factory_flags = 0;
        if (debug_layer) {
            ComPtr<ID3D12Debug> debug;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
                debug->EnableDebugLayer();
                factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
            } else {
                Logger::instance().write(Severity::Warning, "rendering", "D3D12 debug layer is unavailable");
            }
        }

        HRESULT hr = CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-FACTORY", "Could not create DXGI factory", hr));

        ComPtr<IDXGIAdapter1> adapter;
        for (UINT index = 0; factory_->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
            DXGI_ADAPTER_DESC1 description{};
            adapter->GetDesc1(&description);
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
                SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)))) {
                adapter_name_ = utf8(description.Description);
                break;
            }
            adapter.Reset();
        }
        if (!device_) {
            hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
            if (FAILED(hr) || FAILED(hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_))))
                return Result<void>::failure(graphics_error("GFX-DEVICE", "No Direct3D 12 device is available", hr));
            adapter_name_ = "Microsoft WARP";
        }
        set_process_gpu_diagnostics(GpuDiagnostics::from_device(adapter.Get(), device_.Get()));

        D3D12_COMMAND_QUEUE_DESC queue_desc{};
        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr = device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-QUEUE", "Could not create command queue", hr));

        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        width_ = static_cast<UINT>(width > 0 ? width : 1);
        height_ = static_cast<UINT>(height > 0 ? height : 1);
        auto swap_result = create_swap_chain();
        if (!swap_result) return swap_result;

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = frame_count + (editor ? 2u : 0u) + 2u; // + lit_color_ + ao_target_ (post-process)
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        hr = device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-RTV-HEAP", "Could not create render-target heap", hr));
        rtv_stride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_DESCRIPTOR_HEAP_DESC dsv_desc{}; dsv_desc.NumDescriptors=1; dsv_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        hr=device_->CreateDescriptorHeap(&dsv_desc,IID_PPV_ARGS(&dsv_heap_));
        if(FAILED(hr)) return Result<void>::failure(graphics_error("GFX-DSV-HEAP","Could not create depth heap",hr));
        srv_stride_=device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        if(editor){D3D12_DESCRIPTOR_HEAP_DESC imgui_desc{};imgui_desc.NumDescriptors=256;imgui_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;imgui_desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr=device_->CreateDescriptorHeap(&imgui_desc,IID_PPV_ARGS(&imgui_heap_));if(FAILED(hr))return Result<void>::failure(graphics_error("EDITOR-DESCRIPTOR-HEAP","Could not create editor descriptor heap",hr));}
        // Post-process descriptor heap: depth, lit, AO, water scene-color copy.
        D3D12_DESCRIPTOR_HEAP_DESC post_desc{};post_desc.NumDescriptors=4;post_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;post_desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        hr=device_->CreateDescriptorHeap(&post_desc,IID_PPV_ARGS(&post_srv_heap_));
        if(FAILED(hr)) return Result<void>::failure(graphics_error("GFX-POST-DESCRIPTOR-HEAP","Could not create post-process descriptor heap",hr));

        for (UINT i = 0; i < frame_count; ++i) {
            hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocators_[i]));
            if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALLOCATOR", "Could not create command allocator", hr));
        }
        hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocators_[0].Get(), nullptr, IID_PPV_ARGS(&command_list_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMMAND-LIST", "Could not create command list", hr));
        command_list_->Close();

        auto targets = recreate_targets();
        if (!targets) return targets;
        auto pipeline = create_pipeline();
        if (!pipeline) return pipeline;
        auto water_pipeline = create_water_pipeline();
        if (!water_pipeline) return water_pipeline;
        auto ssao_pipelines = create_ssao_pipelines();
        if (!ssao_pipelines) return ssao_pipelines;
        auto frame_cb = create_frame_constant_buffer();
        if (!frame_cb) return frame_cb;
        auto post_cb = create_post_constant_buffers();
        if (!post_cb) return post_cb;
        auto geometry = create_geometry(debug_world, terrain_material, imported_meshes);
        if (!geometry) return geometry;

        D3D12_QUERY_HEAP_DESC query_desc{};
        query_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        query_desc.Count = 2;
        hr = device_->CreateQueryHeap(&query_desc, IID_PPV_ARGS(&timestamp_heap_));
        if (SUCCEEDED(hr) && SUCCEEDED(queue_->GetTimestampFrequency(&timestamp_frequency_))) {
            D3D12_HEAP_PROPERTIES readback_heap{};
            readback_heap.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC timestamp_buffer{};
            timestamp_buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            timestamp_buffer.Width = sizeof(UINT64) * 2;
            timestamp_buffer.Height = 1;
            timestamp_buffer.DepthOrArraySize = 1;
            timestamp_buffer.MipLevels = 1;
            timestamp_buffer.SampleDesc.Count = 1;
            timestamp_buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            hr = device_->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &timestamp_buffer,
                                                  D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                  IID_PPV_ARGS(&timestamp_readback_));
        }
        if (FAILED(hr)) Logger::instance().write(Severity::Warning, "rendering", "GPU timestamp queries are unavailable");

        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-FENCE", "Could not create GPU fence", hr));
        fence_event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fence_event_) return Result<void>::failure(graphics_error("GFX-FENCE-EVENT", "Could not create fence event", HRESULT_FROM_WIN32(GetLastError())));
        if(editor){IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;if(hidden)io.IniFilename=nullptr;else{std::filesystem::create_directories("out/editor");io.IniFilename="out/editor/imgui.ini";}ImGui::StyleColorsDark();EditorChrome::apply_style(ImGui::GetStyle());(void)EditorFonts::load(io);if(!ImGui_ImplSDL3_InitForD3D(window)||!ImGui_ImplDX12_Init(device_.Get(),frame_count,DXGI_FORMAT_R8G8B8A8_UNORM,imgui_heap_.Get(),imgui_heap_->GetCPUDescriptorHandleForHeapStart(),imgui_heap_->GetGPUDescriptorHandleForHeapStart()))return Result<void>::failure(graphics_error("EDITOR-IMGUI-INIT","Could not initialize Dear ImGui SDL3/D3D12 backends"));editor_initialized_=true;}
        return Result<void>::success();
    }

    Result<void> resize(UINT width, UINT height) {
        if (!swap_chain_ || width == 0 || height == 0 || (width == width_ && height == height_)) return Result<void>::success();
        wait_for_gpu();
        for (auto& target : targets_) target.Reset();
        viewport_target_.Reset();
        game_viewport_target_.Reset();
        lit_color_.Reset();
        water_scene_color_.Reset();
        ao_target_.Reset();
        const HRESULT hr = swap_chain_->ResizeBuffers(frame_count, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-RESIZE", "Could not resize swap-chain buffers", hr));
        width_ = width;
        height_ = height;
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
        return recreate_targets();
    }

    Result<void> create_frame_constant_buffer() {
        frame_cb_.Reset();
        water_frame_cb_.Reset();
        frame_cb_mapped_ = nullptr;
        water_frame_cb_mapped_ = nullptr;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // 256 bytes covers 48 floats
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frame_cb_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-FRAME-CB", "Could not create frame constant buffer", hr));
        hr = frame_cb_->Map(0, nullptr, &frame_cb_mapped_);
        if (FAILED(hr) || !frame_cb_mapped_)
            return Result<void>::failure(graphics_error("GFX-FRAME-CB-MAP", "Could not map frame constant buffer", hr));
        // Separate upload CB for water: overwriting the world frame CB before ExecuteCommandLists was
        // zeroing fog/lighting for every mesh draw (black terrain/trees, blue sky only).
        hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&water_frame_cb_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-FRAME-CB", "Could not create water frame constant buffer", hr));
        hr = water_frame_cb_->Map(0, nullptr, &water_frame_cb_mapped_);
        if (FAILED(hr) || !water_frame_cb_mapped_)
            return Result<void>::failure(
                graphics_error("GFX-WATER-FRAME-CB-MAP", "Could not map water frame constant buffer", hr));
        return Result<void>::success();
    }

    void bind_frame_constants(const std::array<float, 48>& frame_constants) {
        std::memcpy(frame_cb_mapped_, frame_constants.data(), sizeof(frame_constants));
        command_list_->SetGraphicsRootConstantBufferView(0, frame_cb_->GetGPUVirtualAddress());
    }

    void bind_water_frame_constants(const std::array<float, 48>& frame_constants) {
        std::memcpy(water_frame_cb_mapped_, frame_constants.data(), sizeof(frame_constants));
        command_list_->SetGraphicsRootConstantBufferView(0, water_frame_cb_->GetGPUVirtualAddress());
    }

    Result<void> upload_prop_vertices(const std::vector<Vertex>& vertices) {
        wait_for_gpu();
        vertex_buffer_.Reset();
        vertex_view_ = {};
        if (vertices.empty()) return Result<void>::success();
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vertices.size() * sizeof(Vertex);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&vertex_buffer_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-VERTEX-BUFFER", "Could not create geometry buffer", hr));
        void* mapped = nullptr;
        vertex_buffer_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
        vertex_buffer_->Unmap(0, nullptr);
        vertex_view_ = {vertex_buffer_->GetGPUVirtualAddress(), static_cast<UINT>(vertices.size() * sizeof(Vertex)),
                        sizeof(Vertex)};
        return Result<void>::success();
    }

    Result<void> sync_imported_meshes(const std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes) {
        std::vector<Vertex> vertices;
        if (!debug_world_) append_debug_ground_quad(vertices);
        append_physics_cube(vertices);
        mesh_ranges_.clear();
        append_imported_mesh_vertices(vertices, imported_meshes, mesh_ranges_);
        if (debug_world_) {
            sky_vertex_offset_ = static_cast<UINT>(vertices.size());
            append_fullscreen_sky_triangle(vertices);
            sky_vertex_count_ = static_cast<UINT>(vertices.size()) - sky_vertex_offset_;
        } else {
            sky_vertex_offset_ = 0;
            sky_vertex_count_ = 0;
        }
        auto uploaded = upload_prop_vertices(vertices);
        if (!uploaded) return uploaded;
        return sync_mesh_albedos(imported_meshes);
    }

    // Decode-free RGBA8 upload into a committed texture + SRV. Self-contained fence keeps it usable during
    // initialization (before the frame fence exists) and mid-frame syncs alike.
    Result<void> upload_rgba_texture(const std::uint8_t* pixels, UINT width, UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu, ComPtr<ID3D12Resource>& out_texture) {
        if (!pixels || width == 0 || height == 0)
            return Result<void>::failure(graphics_error("GFX-ALBEDO-ARGS", "Invalid albedo texture upload arguments"));
        D3D12_RESOURCE_DESC tex_desc{};
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Width = width;
        tex_desc.Height = height;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.MipLevels = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        D3D12_HEAP_PROPERTIES default_heap{};
        default_heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        ComPtr<ID3D12Resource> texture;
        HRESULT hr = device_->CreateCommittedResource(&default_heap, D3D12_HEAP_FLAG_NONE, &tex_desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALBEDO-TEXTURE", "Could not create albedo texture", hr));

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT num_rows = 0;
        UINT64 row_size = 0;
        UINT64 total = 0;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, &num_rows, &row_size, &total);

        D3D12_HEAP_PROPERTIES upload_heap{};
        upload_heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC upload_desc{};
        upload_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        upload_desc.Width = total;
        upload_desc.Height = 1;
        upload_desc.DepthOrArraySize = 1;
        upload_desc.MipLevels = 1;
        upload_desc.SampleDesc.Count = 1;
        upload_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> upload;
        hr = device_->CreateCommittedResource(&upload_heap, D3D12_HEAP_FLAG_NONE, &upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALBEDO-UPLOAD", "Could not create albedo upload buffer", hr));

        void* mapped = nullptr;
        if (FAILED(upload->Map(0, nullptr, &mapped)) || !mapped)
            return Result<void>::failure(graphics_error("GFX-ALBEDO-MAP", "Could not map albedo upload buffer"));
        const UINT src_stride = width * 4;
        for (UINT y = 0; y < height; ++y) {
            std::memcpy(static_cast<std::uint8_t*>(mapped) + footprint.Offset + static_cast<std::size_t>(y) * footprint.Footprint.RowPitch,
                pixels + static_cast<std::size_t>(y) * src_stride, src_stride);
        }
        upload->Unmap(0, nullptr);

        ComPtr<ID3D12CommandAllocator> allocator;
        hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALBEDO-ALLOCATOR", "Could not create albedo upload allocator", hr));
        ComPtr<ID3D12GraphicsCommandList> list;
        hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALBEDO-CMDLIST", "Could not create albedo upload command list", hr));

        D3D12_TEXTURE_COPY_LOCATION dst_loc{};
        dst_loc.pResource = texture.Get();
        dst_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst_loc.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = upload.Get();
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src_loc.PlacedFootprint = footprint;
        list->CopyTextureRegion(&dst_loc, 0, 0, 0, &src_loc, nullptr);
        auto barrier = CD3DX12_RESOURCE_BARRIER_placeholder(texture.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        list->ResourceBarrier(1, &barrier);
        list->Close();

        ID3D12CommandList* lists[] = {list.Get()};
        queue_->ExecuteCommandLists(1, lists);
        ComPtr<ID3D12Fence> fence;
        hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ALBEDO-FENCE", "Could not create albedo upload fence", hr));
        hr = queue_->Signal(fence.Get(), 1);
        if (SUCCEEDED(hr) && fence->GetCompletedValue() < 1) {
            HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (event) {
                fence->SetEventOnCompletion(1, event);
                WaitForSingleObject(event, INFINITE);
                CloseHandle(event);
            }
        }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        device_->CreateShaderResourceView(texture.Get(), &srv, srv_cpu);
        out_texture = std::move(texture);
        return Result<void>::success();
    }

    // (Re)build the shader-visible albedo SRV heap: slot 0 = white fallback, one aligned slot per imported mesh.
    Result<void> sync_mesh_albedos(const std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes) {
        wait_for_gpu();
        mesh_albedo_gpu_.clear();
        mesh_albedo_textures_.clear();
        mesh_white_gpu_ = {};
        const UINT descriptor_count = 1u + static_cast<UINT>(imported_meshes.size());
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
        heap_desc.NumDescriptors = descriptor_count;
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        HRESULT hr = device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&mesh_albedo_heap_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-ALBEDO-HEAP", "Could not create mesh albedo descriptor heap", hr));
        mesh_albedo_textures_.reserve(descriptor_count);
        auto cpu = mesh_albedo_heap_->GetCPUDescriptorHandleForHeapStart();
        auto gpu = mesh_albedo_heap_->GetGPUDescriptorHandleForHeapStart();
        const std::uint8_t white[4] = {255, 255, 255, 255};
        ComPtr<ID3D12Resource> white_texture;
        auto white_uploaded = upload_rgba_texture(white, 1, 1, cpu, white_texture);
        if (!white_uploaded) return white_uploaded;
        mesh_white_gpu_ = gpu;
        mesh_albedo_textures_.push_back(std::move(white_texture));
        cpu.ptr += srv_stride_;
        gpu.ptr += srv_stride_;
        for (const auto& imported : imported_meshes) {
            if (imported.second.has_albedo()) {
                ComPtr<ID3D12Resource> texture;
                auto uploaded = upload_rgba_texture(imported.second.albedo_rgba.data(), imported.second.albedo_width,
                    imported.second.albedo_height, cpu, texture);
                if (!uploaded) return uploaded;
                mesh_albedo_gpu_[normalize_asset_path(imported.first)] = gpu;
                mesh_albedo_textures_.push_back(std::move(texture));
            }
            cpu.ptr += srv_stride_;
            gpu.ptr += srv_stride_;
        }
        return Result<void>::success();
    }

    Result<void> upload_terrain_vertices(const std::vector<Vertex>& vertices) {
        wait_for_gpu();
        terrain_vertex_buffer_.Reset();
        terrain_vertex_count_ = static_cast<UINT>(vertices.size());
        terrain_vertex_view_ = {};
        if (vertices.empty()) return Result<void>::success();
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vertices.size() * sizeof(Vertex);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(&terrain_vertex_buffer_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-TERRAIN-BUFFER", "Could not create terrain geometry buffer", hr));
        void* mapped = nullptr;
        terrain_vertex_buffer_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
        terrain_vertex_buffer_->Unmap(0, nullptr);
        terrain_vertex_view_ = {terrain_vertex_buffer_->GetGPUVirtualAddress(),
                                static_cast<UINT>(vertices.size() * sizeof(Vertex)), sizeof(Vertex)};
        return Result<void>::success();
    }

    Result<void> upload_water_vertices(const std::vector<Vertex>& vertices) {
        wait_for_gpu();
        water_vertex_buffer_.Reset();
        water_vertex_count_ = static_cast<UINT>(vertices.size());
        water_vertex_view_ = {};
        if (vertices.empty()) return Result<void>::success();
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = vertices.size() * sizeof(Vertex);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                   nullptr, IID_PPV_ARGS(&water_vertex_buffer_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-BUFFER", "Could not create water geometry buffer", hr));
        void* mapped = nullptr;
        water_vertex_buffer_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, vertices.data(), vertices.size() * sizeof(Vertex));
        water_vertex_buffer_->Unmap(0, nullptr);
        water_vertex_view_ = {water_vertex_buffer_->GetGPUVirtualAddress(),
                              static_cast<UINT>(vertices.size() * sizeof(Vertex)), sizeof(Vertex)};
        return Result<void>::success();
    }

    struct WorldPassParams {
        std::array<float, 16> view_projection{};
        std::array<float, 3> camera_position{};
        WorldPosition body_position{};
        bool draw_physics_body = false;
        const WorldInfluenceBus* influence = nullptr;
        float time_seconds = 0.0f;
        PbrSurfaceParams terrain_pbr = PbrSurfaceParams::dielectric_default();
        std::array<float, 4> water_color{0.06f, 0.18f, 0.30f, 0.96f};
        float water_roughness = 0.05f;
    };

    void draw_world_pass(ID3D12Resource* color_target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const WorldPassParams& params,
        const std::vector<RenderInstance>& placed_objects, const std::vector<ActivePointLight>& point_lights,
        bool record_gpu_timestamp) {
        auto barrier = CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &barrier);
        const float clear[] = {0.32f, 0.48f, 0.68f, 1.0f};
        auto dsv = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        command_list_->ClearRenderTargetView(rtv, clear, 0, nullptr);
        command_list_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        if (record_gpu_timestamp && timestamp_heap_ && timestamp_readback_)
            command_list_->EndQuery(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

        D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor);
        command_list_->SetGraphicsRootSignature(root_signature_.Get());
        if (mesh_albedo_heap_) {
            ID3D12DescriptorHeap* albedo_heaps[] = {mesh_albedo_heap_.Get()};
            command_list_->SetDescriptorHeaps(1, albedo_heaps);
        }
        std::array<float, 48> frame_constants{};
        std::memcpy(frame_constants.data(), params.view_projection.data(), sizeof(params.view_projection));
        frame_constants[16] = params.camera_position[0];
        frame_constants[17] = params.camera_position[1];
        frame_constants[18] = params.camera_position[2];
        frame_constants[19] = 100.0f;
        frame_constants[20] = 0.28f;
        frame_constants[21] = 0.36f;
        frame_constants[22] = 0.48f;
        frame_constants[23] = 640.0f;
        // Daytime sun direction (world → light) and ambient fill.
        frame_constants[24] = -0.40f;
        frame_constants[25] = -0.85f;
        frame_constants[26] = -0.30f;
        frame_constants[27] = 0.42f;
        pack_point_lights(frame_constants, point_lights, params.camera_position);
        frame_constants[44] = static_cast<float>(width_);
        frame_constants[45] = static_cast<float>(height_);
        bind_frame_constants(frame_constants);
        const std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const auto bind_object = [&](const std::array<float, 16>& model, const PbrSurfaceParams& pbr,
            D3D12_GPU_DESCRIPTOR_HANDLE albedo, float use_albedo) {
            const auto constants = pack_object_constants(model, pbr, use_albedo);
            command_list_->SetGraphicsRoot32BitConstants(1, 24, constants.data(), 0);
            if (mesh_albedo_heap_) command_list_->SetGraphicsRootDescriptorTable(2, albedo);
        };
        command_list_->IASetVertexBuffers(0, 1, &vertex_view_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (debug_world_ && sky_vertex_count_ > 0) {
            command_list_->SetPipelineState(sky_pipeline_.Get());
            bind_object(identity, PbrSurfaceParams::dielectric_default(), mesh_white_gpu_, 0.0f);
            command_list_->DrawInstanced(sky_vertex_count_, 1, sky_vertex_offset_, 0);
            command_list_->SetPipelineState(pipeline_.Get());
        }
        bind_object(identity, params.terrain_pbr, mesh_white_gpu_, 0.0f);
        if (terrain_vertex_count_ > 0) {
            command_list_->IASetVertexBuffers(0, 1, &terrain_vertex_view_);
            command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            command_list_->DrawInstanced(terrain_vertex_count_, 1, 0, 0);
        }
        command_list_->IASetVertexBuffers(0, 1, &vertex_view_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (params.draw_physics_body) {
            auto model = identity;
            model[12] = static_cast<float>(params.body_position.x);
            model[13] = static_cast<float>(params.body_position.y);
            model[14] = static_cast<float>(params.body_position.z);
            bind_object(model, PbrSurfaceParams::dielectric_default(), mesh_white_gpu_, 0.0f);
            command_list_->DrawInstanced(36, 1, 0, 0);
        }
        for (const auto& instance : placed_objects) {
            if (instance.mesh_asset.empty()) continue;
            const auto found = mesh_ranges_.find(normalize_asset_path(instance.mesh_asset));
            if (found == mesh_ranges_.end()) continue;
            const auto& transform = instance.transform;
            using namespace DirectX;
            const auto scale = XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]);
            const auto rotation =
                XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data())));
            const auto translation = XMMatrixTranslation(transform.position[0], transform.position[1],
                transform.position[2]);
            std::array<float, 16> placed_model{};
            XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(placed_model.data()), scale * rotation * translation);
            D3D12_GPU_DESCRIPTOR_HANDLE albedo = mesh_white_gpu_;
            float use_albedo = 0.0f;
            const auto albedo_it = mesh_albedo_gpu_.find(normalize_asset_path(instance.mesh_asset));
            if (albedo_it != mesh_albedo_gpu_.end()) {
                albedo = albedo_it->second;
                use_albedo = 1.0f;
            }
            bind_object(placed_model, instance.pbr, albedo, use_albedo);
            command_list_->DrawInstanced(found->second.second, 1, found->second.first, 0);
        }
        draw_foliage_instances(frame_constants, params.influence, params.time_seconds);
        barrier = CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list_->ResourceBarrier(1, &barrier);
    }

    void draw_water_pass(ID3D12Resource* color_target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const WorldPassParams& params) {
        if (water_vertex_count_ == 0 || !water_pipeline_ || !water_scene_color_) return;

        // Snapshot the opaque lit scene before water writes it — sampling lit_color_ while blending
        // into it caused shoreline fuzz / highlight feedback.
        D3D12_RESOURCE_BARRIER pre_copy[2] = {
            CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_COPY_SOURCE),
            CD3DX12_RESOURCE_BARRIER_placeholder(water_scene_color_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_COPY_DEST),
        };
        command_list_->ResourceBarrier(2, pre_copy);
        command_list_->CopyResource(water_scene_color_.Get(), color_target);
        D3D12_RESOURCE_BARRIER post_copy[2] = {
            CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_COPY_SOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET),
            CD3DX12_RESOURCE_BARRIER_placeholder(water_scene_color_.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
        };
        command_list_->ResourceBarrier(2, post_copy);

        auto dsv = dsv_heap_->GetCPUDescriptorHandleForHeapStart();
        command_list_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
        D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor);
        ID3D12DescriptorHeap* post_heaps[] = {post_srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(1, post_heaps);
        command_list_->SetGraphicsRootSignature(water_root_signature_.Get());
        command_list_->SetPipelineState(water_pipeline_.Get());
        std::array<float, 48> frame_constants{};
        std::memcpy(frame_constants.data(), params.view_projection.data(), sizeof(params.view_projection));
        frame_constants[16] = params.camera_position[0];
        frame_constants[17] = params.camera_position[1];
        frame_constants[18] = params.camera_position[2];
        // Match opaque pass sun so water specular/glints share the same light direction.
        frame_constants[24] = -0.40f;
        frame_constants[25] = -0.85f;
        frame_constants[26] = -0.30f;
        frame_constants[27] = 0.42f;
        frame_constants[44] = static_cast<float>(width_);
        frame_constants[45] = static_cast<float>(height_);
        bind_water_frame_constants(frame_constants);
        std::array<float, 8> water_constants{};
        water_constants[0] = params.time_seconds;
        water_constants[1] = params.water_roughness;
        water_constants[2] = 0.06f; // waveAmplitude (keep small so the sheet doesn't read as floating)
        water_constants[3] = 0.22f; // waveFrequency
        water_constants[4] = params.water_color[0];
        water_constants[5] = params.water_color[1];
        water_constants[6] = params.water_color[2];
        water_constants[7] = params.water_color[3];
        command_list_->SetGraphicsRoot32BitConstants(1, 8, water_constants.data(), 0);
        command_list_->SetGraphicsRootDescriptorTable(2, post_water_gpu_);
        command_list_->IASetVertexBuffers(0, 1, &water_vertex_view_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->DrawInstanced(water_vertex_count_, 1, 0, 0);
        auto barrier = CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list_->ResourceBarrier(1, &barrier);
    }

    // SSAO v1: samples depth_ to build a half-res AO term in ao_target_, then composites lit_color_ * AO into
    // destination. lit_color_ must already be in D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE (draw_world_pass leaves
    // it there). destination_returns_to_srv must be true for editor viewport targets (which ImGui samples between
    // frames) and false for the swap-chain backbuffer (which the caller transitions to PRESENT separately).
    void apply_ssao(ID3D12Resource* destination, D3D12_CPU_DESCRIPTOR_HANDLE destination_rtv,
        bool destination_returns_to_srv, const WorldPassParams& params) {
        using namespace DirectX;

        auto depth_to_srv = CD3DX12_RESOURCE_BARRIER_placeholder(depth_.Get(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list_->ResourceBarrier(1, &depth_to_srv);
        auto ao_to_rt = CD3DX12_RESOURCE_BARRIER_placeholder(ao_target_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &ao_to_rt);

        ID3D12DescriptorHeap* post_heaps[] = {post_srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(1, post_heaps);

        // ---- SSAO pass: depth -> half-res AO ----
        const float ao_clear[] = {1.0f, 1.0f, 1.0f, 1.0f};
        command_list_->OMSetRenderTargets(1, &ao_rtv_, FALSE, nullptr);
        command_list_->ClearRenderTargetView(ao_rtv_, ao_clear, 0, nullptr);
        D3D12_VIEWPORT ao_viewport{0.0f, 0.0f, static_cast<float>(ao_width_), static_cast<float>(ao_height_), 0.0f, 1.0f};
        D3D12_RECT ao_scissor{0, 0, static_cast<LONG>(ao_width_), static_cast<LONG>(ao_height_)};
        command_list_->RSSetViewports(1, &ao_viewport);
        command_list_->RSSetScissorRects(1, &ao_scissor);
        command_list_->SetGraphicsRootSignature(ssao_root_signature_.Get());
        command_list_->SetPipelineState(ssao_pipeline_.Get());

        const auto vp = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(params.view_projection.data()));
        XMVECTOR determinant;
        const auto inv_vp = XMMatrixInverse(&determinant, vp);
        std::array<float, 40> ssao_constants{};
        XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(ssao_constants.data()), inv_vp);
        std::memcpy(ssao_constants.data() + 16, params.view_projection.data(), sizeof(float) * 16);
        ssao_constants[32] = params.camera_position[0];
        ssao_constants[33] = params.camera_position[1];
        ssao_constants[34] = params.camera_position[2];
        ssao_constants[35] = k_ssao_radius;
        ssao_constants[36] = k_ssao_bias;
        ssao_constants[37] = k_ssao_intensity;
        // Full-res depth texel size for stable finite-difference normals (not AO half-res).
        ssao_constants[38] = 1.0f / static_cast<float>(width_);
        ssao_constants[39] = 1.0f / static_cast<float>(height_);
        std::memcpy(ssao_cb_mapped_, ssao_constants.data(), sizeof(ssao_constants));
        command_list_->SetGraphicsRootConstantBufferView(0, ssao_cb_->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootDescriptorTable(1, post_depth_gpu_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->DrawInstanced(3, 1, 0, 0);

        auto ao_to_srv = CD3DX12_RESOURCE_BARRIER_placeholder(ao_target_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list_->ResourceBarrier(1, &ao_to_srv);
        auto depth_to_write = CD3DX12_RESOURCE_BARRIER_placeholder(depth_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        command_list_->ResourceBarrier(1, &depth_to_write);

        // ---- Composite pass: lit_color_ * lerp(1, blurred AO, intensity) -> destination ----
        if (destination_returns_to_srv) {
            auto dst_to_rt = CD3DX12_RESOURCE_BARRIER_placeholder(destination, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list_->ResourceBarrier(1, &dst_to_rt);
        }
        command_list_->OMSetRenderTargets(1, &destination_rtv, FALSE, nullptr);
        D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
        D3D12_RECT scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
        command_list_->RSSetViewports(1, &viewport);
        command_list_->RSSetScissorRects(1, &scissor);
        command_list_->SetGraphicsRootSignature(composite_root_signature_.Get());
        command_list_->SetPipelineState(composite_pipeline_.Get());
        const std::array<float, 4> composite_constants{k_ssao_intensity, 1.0f / static_cast<float>(ao_width_),
            1.0f / static_cast<float>(ao_height_), 0.0f};
        std::memcpy(composite_cb_mapped_, composite_constants.data(), sizeof(composite_constants));
        command_list_->SetGraphicsRootConstantBufferView(0, composite_cb_->GetGPUVirtualAddress());
        command_list_->SetGraphicsRootDescriptorTable(1, post_lit_gpu_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        command_list_->DrawInstanced(3, 1, 0, 0);
        if (destination_returns_to_srv) {
            auto dst_to_srv = CD3DX12_RESOURCE_BARRIER_placeholder(destination, D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            command_list_->ResourceBarrier(1, &dst_to_srv);
        }
    }

    Result<void> render(const std::filesystem::path& capture_path, const WorldPassParams& world,
        const std::vector<RenderInstance>& placed_objects, const std::vector<ActivePointLight>& point_lights,
        const WorldPassParams* game_world = nullptr) {
        HRESULT hr = allocators_[frame_index_]->Reset();
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-ALLOCATOR-RESET", "Could not reset command allocator", hr));
        hr = command_list_->Reset(allocators_[frame_index_].Get(), pipeline_.Get());
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-LIST-RESET", "Could not reset command list", hr));

        auto barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &barrier);
        D3D12_CPU_DESCRIPTOR_HANDLE backbuffer_rtv = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        backbuffer_rtv.ptr += static_cast<SIZE_T>(frame_index_) * rtv_stride_;
        const float clear[] = {0.32f, 0.48f, 0.68f, 1.0f};
        command_list_->OMSetRenderTargets(1, &backbuffer_rtv, FALSE, nullptr);
        command_list_->ClearRenderTargetView(backbuffer_rtv, clear, 0, nullptr);
        if (editor_initialized_) {
            draw_world_pass(lit_color_.Get(), lit_rtv_, world, placed_objects, point_lights, true);
            draw_water_pass(lit_color_.Get(), lit_rtv_, world);
            apply_ssao(viewport_target_.Get(), viewport_rtv_, /*destination_returns_to_srv=*/true, world);
            if (game_world) {
                draw_world_pass(lit_color_.Get(), lit_rtv_, *game_world, placed_objects, point_lights, false);
                draw_water_pass(lit_color_.Get(), lit_rtv_, *game_world);
                apply_ssao(game_viewport_target_.Get(), game_viewport_rtv_, /*destination_returns_to_srv=*/true,
                    *game_world);
            }
            // Offscreen passes leave the post-process descriptor heap and viewport RTV/DSV bound; ImGui must draw to
            // the swap-chain target using its own descriptor heap.
            ID3D12DescriptorHeap* imgui_heaps[] = {imgui_heap_.Get()};
            command_list_->SetDescriptorHeaps(1, imgui_heaps);
            command_list_->OMSetRenderTargets(1, &backbuffer_rtv, FALSE, nullptr);
            const D3D12_VIEWPORT imgui_viewport{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_),
                0.0f, 1.0f};
            const D3D12_RECT imgui_scissor{0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
            command_list_->RSSetViewports(1, &imgui_viewport);
            command_list_->RSSetScissorRects(1, &imgui_scissor);
        } else {
            draw_world_pass(lit_color_.Get(), lit_rtv_, world, placed_objects, point_lights, true);
            draw_water_pass(lit_color_.Get(), lit_rtv_, world);
            apply_ssao(targets_[frame_index_].Get(), backbuffer_rtv, /*destination_returns_to_srv=*/false, world);
        }
        if(editor_initialized_){ImGui::Render();ID3D12DescriptorHeap* heaps[]={imgui_heap_.Get()};command_list_->SetDescriptorHeaps(1,heaps);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),command_list_.Get());}
        if (timestamp_heap_ && timestamp_readback_) {
            command_list_->EndQuery(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
            command_list_->ResolveQueryData(timestamp_heap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, timestamp_readback_.Get(), 0);
        }

        const bool capture = !capture_path.empty();
        ComPtr<ID3D12Resource> readback;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 readback_size = 0;
        // Prefer the composited scene viewport (editor) or backbuffer (runtime) so captures are not an empty clear.
        ID3D12Resource* capture_source = targets_[frame_index_].Get();
        D3D12_RESOURCE_STATES capture_state = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (editor_initialized_ && viewport_target_) {
            capture_source = viewport_target_.Get();
            capture_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        }
        if (capture) {
            auto copy_barrier = CD3DX12_RESOURCE_BARRIER_placeholder(capture_source, capture_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
            command_list_->ResourceBarrier(1, &copy_barrier);
            const auto desc = capture_source->GetDesc();
            device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &readback_size);
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_READBACK;
            D3D12_RESOURCE_DESC buffer{};
            buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            buffer.Width = readback_size;
            buffer.Height = 1;
            buffer.DepthOrArraySize = 1;
            buffer.MipLevels = 1;
            buffer.SampleDesc.Count = 1;
            buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
            if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-CAPTURE-BUFFER", "Could not allocate capture readback", hr));
            D3D12_TEXTURE_COPY_LOCATION destination{};
            destination.pResource = readback.Get();
            destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            destination.PlacedFootprint = footprint;
            D3D12_TEXTURE_COPY_LOCATION source{};
            source.pResource = capture_source;
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex = 0;
            command_list_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            if (capture_source == targets_[frame_index_].Get()) {
                barrier = CD3DX12_RESOURCE_BARRIER_placeholder(capture_source, D3D12_RESOURCE_STATE_COPY_SOURCE,
                    D3D12_RESOURCE_STATE_PRESENT);
            } else {
                auto restore = CD3DX12_RESOURCE_BARRIER_placeholder(capture_source, D3D12_RESOURCE_STATE_COPY_SOURCE,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                command_list_->ResourceBarrier(1, &restore);
                barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            }
        } else {
            barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        }
        command_list_->ResourceBarrier(1, &barrier);
        hr = command_list_->Close();
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-LIST-CLOSE", "Could not close command list", hr));
        ID3D12CommandList* lists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, lists);
        const UINT presented_index = frame_index_;
        hr = swap_chain_->Present(1, 0);
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-PRESENT", "Could not present frame", hr));
        wait_for_gpu();
        last_presented_index_ = presented_index;
        has_presented_backbuffer_ = true;

        if (timestamp_readback_ && timestamp_frequency_ > 0) {
            UINT64* timestamps = nullptr;
            D3D12_RANGE query_range{0, sizeof(UINT64) * 2};
            if (SUCCEEDED(timestamp_readback_->Map(0, &query_range, reinterpret_cast<void**>(&timestamps)))) {
                last_gpu_ms_ = timestamps[1] >= timestamps[0]
                    ? static_cast<double>(timestamps[1] - timestamps[0]) * 1000.0 / static_cast<double>(timestamp_frequency_)
                    : 0.0;
                D3D12_RANGE written{0, 0};
                timestamp_readback_->Unmap(0, &written);
            }
        }

        if (capture) {
            void* mapped = nullptr;
            D3D12_RANGE range{0, static_cast<SIZE_T>(readback_size)};
            hr = readback->Map(0, &range, &mapped);
            if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-CAPTURE-MAP", "Could not map capture data", hr));
            if (capture_path.has_parent_path()) std::filesystem::create_directories(capture_path.parent_path());
            std::ofstream output(capture_path, std::ios::binary | std::ios::trunc);
            output << "P6\n" << width_ << ' ' << height_ << "\n255\n";
            const auto* bytes = static_cast<const unsigned char*>(mapped) + footprint.Offset;
            for (UINT y = 0; y < height_; ++y) {
                const auto* row = bytes + static_cast<std::size_t>(y) * footprint.Footprint.RowPitch;
                for (UINT x = 0; x < width_; ++x) output.write(reinterpret_cast<const char*>(row + x * 4), 3);
            }
            readback->Unmap(0, nullptr);
            if (!output) return Result<void>::failure(graphics_error("GFX-CAPTURE-WRITE", "Could not write PPM capture"));
        }
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
        return Result<void>::success();
    }

    const std::string& adapter_name() const { return adapter_name_; }
    double last_gpu_ms() const { return last_gpu_ms_; }
    [[nodiscard]] ImTextureID scene_viewport_texture() const { return static_cast<ImTextureID>(viewport_gpu_.ptr); }
    [[nodiscard]] ImTextureID game_viewport_texture() const { return static_cast<ImTextureID>(game_viewport_gpu_.ptr); }

    void ensure_world_forge_placeholder_textures(WorldForgeEditorSession& session) {
        if (!editor_requested_ || !imgui_heap_ || !device_ || !queue_) return;

        if (!session.concept_placeholder_tex_ready) {
            session.concept_placeholder_tex_ready = true;

            static constexpr const char* keys[] = {"person", "deity", "artifact", "organization", "faction", "region",
                "poi"};
            static constexpr const char* files[] = {"wf_person.png", "wf_deity.png", "wf_artifact.png",
                "wf_organization.png", "wf_faction.png", "wf_region.png", "wf_poi.png"};
            world_forge_placeholder_textures_.resize(7);
            for (int i = 0; i < 7; ++i) {
                const std::filesystem::path relative =
                    std::filesystem::path("assets/world-forge/placeholders") / files[i];
                std::filesystem::path path = relative;
#ifdef ENGINE_REPOSITORY_ROOT
                if (!std::filesystem::exists(path))
                    path = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / relative;
#endif
                ID3D12Resource* raw = nullptr;
                const UINT srv_index = 3u + static_cast<UINT>(i);
                auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(), srv_stride_,
                    srv_index, path, &raw, [this]() { wait_for_gpu(); });
                if (!loaded || !raw) {
                    Logger::instance().write(Severity::Warning, "world-forge",
                        std::string("Concept placeholder PNG failed: ") + files[i]);
                    continue;
                }
                world_forge_placeholder_textures_[static_cast<std::size_t>(i)].Attach(raw);
                session.concept_placeholder_tex[keys[i]] = loaded.value();
            }
        }

        if (!session.cartography_tex_ready) {
            session.cartography_tex_ready = true;
            static constexpr const char* carto_keys[] = {"icon-village", "icon-town", "icon-city", "icon-fortress",
                "icon-ruin", "icon-gate", "icon-shrine", "icon-camp", "icon-landmark", "icon-dock",
                "heraldry-kingdom_tessera", "heraldry-chaotic_imperium", "heraldry-cristallo", "heraldry-arrotrebae",
                "heraldry-orc_warbands", "heraldry-thalassar", "heraldry-underflow"};
            static constexpr const char* carto_files[] = {"icon-village.png", "icon-town.png", "icon-city.png",
                "icon-fortress.png", "icon-ruin.png", "icon-gate.png", "icon-shrine.png", "icon-camp.png",
                "icon-landmark.png", "icon-dock.png", "heraldry-kingdom_tessera.png", "heraldry-chaotic_imperium.png",
                "heraldry-cristallo.png", "heraldry-arrotrebae.png", "heraldry-orc_warbands.png",
                "heraldry-thalassar.png", "heraldry-underflow.png"};
            constexpr int k_carto_count = 17;
            cartography_textures_.resize(static_cast<std::size_t>(k_carto_count));
            for (int i = 0; i < k_carto_count; ++i) {
                const std::filesystem::path relative =
                    std::filesystem::path("samples/open-world-rpg/assets/ui/cartography") / carto_files[i];
                std::filesystem::path path = relative;
#ifdef ENGINE_REPOSITORY_ROOT
                if (!std::filesystem::exists(path))
                    path = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / relative;
                if (!std::filesystem::exists(path))
                    path = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "context/art/cartography" / carto_files[i];
#endif
                if (!std::filesystem::exists(path))
                    path = std::filesystem::path("context/art/cartography") / carto_files[i];
                ID3D12Resource* raw = nullptr;
                const UINT srv_index = 10u + static_cast<UINT>(i);
                auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(), srv_stride_,
                    srv_index, path, &raw, [this]() { wait_for_gpu(); });
                if (!loaded || !raw) {
                    Logger::instance().write(Severity::Warning, "world-forge",
                        std::string("Cartography icon PNG failed: ") + carto_files[i]);
                    continue;
                }
                cartography_textures_[static_cast<std::size_t>(i)].Attach(raw);
                session.cartography_tex[carto_keys[i]] = loaded.value();
            }

            // Discrete world-map zoom layers + frame/fog chrome (SRV 32+).
            {
                auto resolve_carto_dir = [](const char* relative_under_cartography) {
                    std::filesystem::path root =
                        std::filesystem::path("samples/open-world-rpg/assets/ui/cartography") /
                        relative_under_cartography;
#ifdef ENGINE_REPOSITORY_ROOT
                    if (!std::filesystem::exists(root / "manifest.json") &&
                        !std::filesystem::exists(root / "frame-full.png") &&
                        !std::filesystem::exists(root / "fog-veil.png"))
                        root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / root;
                    if (!std::filesystem::exists(root / "manifest.json") &&
                        !std::filesystem::exists(root / "frame-full.png") &&
                        !std::filesystem::exists(root / "fog-veil.png"))
                        root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "context/art/cartography" /
                               relative_under_cartography;
#endif
                    if (!std::filesystem::exists(root / "manifest.json") &&
                        !std::filesystem::exists(root / "frame-full.png") &&
                        !std::filesystem::exists(root / "fog-veil.png"))
                        root = std::filesystem::path("context/art/cartography") / relative_under_cartography;
                    return root;
                };

                UINT srv_index = 32u;
                auto load_carto_png = [&](const std::filesystem::path& path, const char* key) -> bool {
                    if (srv_index >= 256u || !std::filesystem::exists(path)) return false;
                    ID3D12Resource* raw = nullptr;
                    auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(), srv_stride_,
                        srv_index, path, &raw, [this]() { wait_for_gpu(); });
                    if (!loaded || !raw) {
                        Logger::instance().write(Severity::Warning, "world-forge",
                            "Cartography PNG failed: " + path.generic_string());
                        return false;
                    }
                    cartography_textures_.push_back({});
                    cartography_textures_.back().Attach(raw);
                    session.cartography_tex[key] = loaded.value();
                    ++srv_index;
                    return true;
                };

                const auto layers_root = resolve_carto_dir("world-map-layers");
                const auto layers_manifest = layers_root / "manifest.json";
                if (std::filesystem::exists(layers_manifest)) {
                    try {
                        std::ifstream in(layers_manifest);
                        nlohmann::json manifest;
                        in >> manifest;
                        session.map_layer_aspect = manifest.value("aspect", 1.5f);
                        session.map_layer_native_width =
                            manifest.value("nativeWidth", manifest.value("masterWidth", 0));
                        session.map_layer_transition_seconds = manifest.value("transitionSeconds", 0.35f);
                        session.map_layers.clear();
                        session.map_layer_tex.clear();
                        for (const auto& layer_json : manifest.value("layers", nlohmann::json::array())) {
                            WorldForgeEditorSession::WorldMapLayer layer{};
                            layer.id = layer_json.value("id", "");
                            if (layer.id.empty()) continue;
                            layer.u0 = layer_json.value("u0", 0.0f);
                            layer.v0 = layer_json.value("v0", 0.0f);
                            layer.u1 = layer_json.value("u1", 1.0f);
                            layer.v1 = layer_json.value("v1", 1.0f);
                            layer.min_zoom = layer_json.value("minZoom", 0.0f);
                            layer.priority = layer_json.value("priority", 0);
                            layer.width = layer_json.value("width", 0);
                            layer.height = layer_json.value("height", 0);
                            const auto path = layers_root / layer_json.value("file", layer.id + ".png");
                            if (srv_index >= 256u) break;
                            ID3D12Resource* raw = nullptr;
                            auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(),
                                srv_stride_, srv_index, path, &raw, [this]() { wait_for_gpu(); });
                            if (!loaded || !raw) {
                                Logger::instance().write(Severity::Warning, "world-forge",
                                    "World-map layer failed: " + path.generic_string());
                                continue;
                            }
                            cartography_textures_.push_back({});
                            cartography_textures_.back().Attach(raw);
                            session.map_layer_tex[layer.id] = loaded.value();
                            session.cartography_tex["layer-" + layer.id] = loaded.value();
                            if (layer.id == "continent")
                                session.cartography_tex["official-world-map"] = loaded.value();
                            session.map_layers.push_back(std::move(layer));
                            ++srv_index;
                        }
                        session.map_layers_ready = !session.map_layer_tex.empty();
                        if (session.map_layer_active_id.empty() && !session.map_layers.empty())
                            session.map_layer_active_id = session.map_layers.front().id;
                        Logger::instance().write(Severity::Info, "world-forge",
                            "Loaded world-map layers: " + std::to_string(session.map_layer_tex.size()));
                    } catch (const std::exception& ex) {
                        Logger::instance().write(Severity::Warning, "world-forge",
                            std::string("World-map layer manifest parse failed: ") + ex.what());
                    }
                }

                const auto frame_root = resolve_carto_dir("frame");
                load_carto_png(frame_root / "frame-full.png", "map-frame");
                const auto fog_root = resolve_carto_dir("fog");
                load_carto_png(fog_root / "fog-veil.png", "map-fog");
                const auto panel_root = resolve_carto_dir("panel");
                load_carto_png(panel_root / "panel-parchment.png", "panel-parchment");
                load_carto_png(panel_root / "panel-border.png", "panel-border");
                load_carto_png(panel_root / "panel-border-wide.png", "panel-border-wide");

                // Transparent stroke tiles for borders / roads / ferry / river (image-stamp ribbons).
                {
                    auto resolve_strokes_dir = []() {
                        const char* marker = "stroke-road.png";
                        std::filesystem::path root =
                            std::filesystem::path("samples/open-world-rpg/assets/ui/cartography/strokes");
#ifdef ENGINE_REPOSITORY_ROOT
                        if (!std::filesystem::exists(root / marker))
                            root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / root;
                        if (!std::filesystem::exists(root / marker))
                            root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) /
                                   "context/art/cartography/strokes";
#endif
                        if (!std::filesystem::exists(root / marker))
                            root = std::filesystem::path("context/art/cartography/strokes");
                        return root;
                    };
                    const auto strokes_root = resolve_strokes_dir();
                    static constexpr const char* stroke_keys[] = {"stroke-political-border", "stroke-track",
                        "stroke-road", "stroke-highway", "stroke-ferry", "stroke-river"};
                    static constexpr const char* stroke_files[] = {"stroke-political-border.png", "stroke-track.png",
                        "stroke-road.png", "stroke-highway.png", "stroke-ferry.png", "stroke-river.png"};
                    for (int i = 0; i < 6; ++i) {
                        if (!load_carto_png(strokes_root / stroke_files[i], stroke_keys[i])) {
                            Logger::instance().write(Severity::Warning, "world-forge",
                                std::string("Cartography stroke PNG missing: ") + stroke_files[i]);
                        }
                    }
                }

                // Legacy tiled LOD fallback when discrete layers are unavailable.
                if (!session.map_layers_ready) {
                    std::filesystem::path tiles_root =
                        std::filesystem::path("samples/open-world-rpg/assets/ui/cartography/world-map-tiles");
#ifdef ENGINE_REPOSITORY_ROOT
                    if (!std::filesystem::exists(tiles_root / "manifest.json"))
                        tiles_root = std::filesystem::path(ENGINE_REPOSITORY_ROOT) / tiles_root;
                    if (!std::filesystem::exists(tiles_root / "manifest.json"))
                        tiles_root =
                            std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "context/art/cartography/world-map-tiles";
#endif
                    if (!std::filesystem::exists(tiles_root / "manifest.json"))
                        tiles_root = std::filesystem::path("context/art/cartography/world-map-tiles");

                    const auto manifest_path = tiles_root / "manifest.json";
                    if (std::filesystem::exists(manifest_path)) {
                        try {
                            std::ifstream in(manifest_path);
                            nlohmann::json manifest;
                            in >> manifest;
                            session.map_tile_size = manifest.value("tileSize", 512);
                            session.map_tile_max_lod = manifest.value("maxLod", 0);
                            session.map_tile_aspect = manifest.value("aspect", 1.5f);
                            session.map_tile_native_width =
                                manifest.value("nativeWidth", manifest.value("masterWidth", 0));
                            session.map_tile_levels.clear();
                            session.map_tile_tex.clear();
                            for (const auto& level_json : manifest.value("levels", nlohmann::json::array())) {
                                WorldForgeEditorSession::WorldMapTileLevel level{};
                                level.lod = level_json.value("lod", 0);
                                level.cols = level_json.value("cols", 1);
                                level.rows = level_json.value("rows", 1);
                                level.content_width = level_json.value("contentWidth", session.map_tile_size);
                                level.content_height = level_json.value("contentHeight", session.map_tile_size);
                                level.level_width = level_json.value("levelWidth", level.content_width);
                                level.level_height = level_json.value("levelHeight", level.content_height);
                                session.map_tile_levels.push_back(level);
                                for (int ty = 0; ty < level.rows; ++ty) {
                                    for (int tx = 0; tx < level.cols; ++tx) {
                                        if (srv_index >= 256u) {
                                            Logger::instance().write(Severity::Warning, "world-forge",
                                                "World-map tile SRV heap exhausted");
                                            break;
                                        }
                                        const auto path = tiles_root / ("z" + std::to_string(level.lod)) /
                                                          (std::to_string(tx) + "_" + std::to_string(ty) + ".png");
                                        ID3D12Resource* raw = nullptr;
                                        auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(),
                                            srv_stride_, srv_index, path, &raw, [this]() { wait_for_gpu(); });
                                        if (!loaded || !raw) {
                                            Logger::instance().write(Severity::Warning, "world-forge",
                                                "World-map tile failed: " + path.generic_string());
                                            continue;
                                        }
                                        cartography_textures_.push_back({});
                                        cartography_textures_.back().Attach(raw);
                                        const std::uint32_t key = (static_cast<std::uint32_t>(level.lod) << 24) |
                                                                 (static_cast<std::uint32_t>(tx) << 12) |
                                                                 static_cast<std::uint32_t>(ty);
                                        session.map_tile_tex[key] = loaded.value();
                                        ++srv_index;
                                    }
                                }
                            }
                            session.map_tiles_ready = !session.map_tile_tex.empty();
                            const auto z0 = session.map_tile_tex.find(0u);
                            if (z0 != session.map_tile_tex.end())
                                session.cartography_tex["official-world-map"] = z0->second;
                            Logger::instance().write(Severity::Info, "world-forge",
                                "Loaded world-map tiles: " + std::to_string(session.map_tile_tex.size()) +
                                    " (maxLod=" + std::to_string(session.map_tile_max_lod) + ")");
                        } catch (const std::exception& ex) {
                            Logger::instance().write(Severity::Warning, "world-forge",
                                std::string("World-map tile manifest parse failed: ") + ex.what());
                        }
                    } else {
                        std::filesystem::path path = std::filesystem::path("context/story/official-world-map.png");
#ifdef ENGINE_REPOSITORY_ROOT
                        if (!std::filesystem::exists(path))
                            path =
                                std::filesystem::path(ENGINE_REPOSITORY_ROOT) / "context/story/official-world-map.png";
#endif
                        if (load_carto_png(path, "official-world-map")) {
                            // ok
                        } else {
                            Logger::instance().write(Severity::Warning, "world-forge",
                                "Official world map PNG failed to load for Cartography backdrop");
                        }
                    }
                }
            }
        }
    }

private:
    static D3D12_RESOURCE_BARRIER CD3DX12_RESOURCE_BARRIER_placeholder(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER value{};
        value.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        value.Transition.pResource = resource;
        value.Transition.StateBefore = before;
        value.Transition.StateAfter = after;
        value.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        return value;
    }

    Result<void> create_swap_chain() {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = width_;
        desc.Height = height_;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = frame_count;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> initial;
        HRESULT hr = factory_->CreateSwapChainForHwnd(queue_.Get(), hwnd_, &desc, nullptr, nullptr, &initial);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SWAP-CHAIN", "Could not create swap chain", hr));
        factory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);
        hr = initial.As(&swap_chain_);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SWAP-CHAIN-CAST", "Could not acquire swap-chain interface", hr));
        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
        return Result<void>::success();
    }

    Result<void> recreate_targets() {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtv_heap_->GetCPUDescriptorHandleForHeapStart();
        for (UINT i = 0; i < frame_count; ++i) {
            const HRESULT hr = swap_chain_->GetBuffer(i, IID_PPV_ARGS(&targets_[i]));
            if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-BACK-BUFFER", "Could not acquire swap-chain buffer", hr));
            device_->CreateRenderTargetView(targets_[i].Get(), nullptr, handle);
            handle.ptr += rtv_stride_;
        }
        if(editor_requested_){
            const auto create_viewport_target=[&](ComPtr<ID3D12Resource>& target,D3D12_CPU_DESCRIPTOR_HANDLE rtv,D3D12_GPU_DESCRIPTOR_HANDLE& gpu_srv,UINT srv_index){
                D3D12_HEAP_PROPERTIES texture_heap{};texture_heap.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC texture{};texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;texture.Width=width_;texture.Height=height_;texture.DepthOrArraySize=1;texture.MipLevels=1;texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;texture.SampleDesc.Count=1;texture.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;texture.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;D3D12_CLEAR_VALUE color_clear{};color_clear.Format=texture.Format;color_clear.Color[0]=.015f;color_clear.Color[1]=.025f;color_clear.Color[2]=.055f;color_clear.Color[3]=1;
                target.Reset();auto viewport_hr=device_->CreateCommittedResource(&texture_heap,D3D12_HEAP_FLAG_NONE,&texture,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&color_clear,IID_PPV_ARGS(&target));if(FAILED(viewport_hr))return viewport_hr;
                device_->CreateRenderTargetView(target.Get(),nullptr,rtv);auto cpu=imgui_heap_->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=static_cast<SIZE_T>(srv_index)*srv_stride_;auto gpu=imgui_heap_->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=static_cast<SIZE_T>(srv_index)*srv_stride_;D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=texture.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;device_->CreateShaderResourceView(target.Get(),&srv,cpu);gpu_srv=gpu;return S_OK;
            };
            if(FAILED(create_viewport_target(viewport_target_,handle,viewport_gpu_,1)))return Result<void>::failure(graphics_error("EDITOR-VIEWPORT-TARGET","Could not create scene viewport render target",E_FAIL));
            viewport_rtv_=handle;handle.ptr+=rtv_stride_;
            if(FAILED(create_viewport_target(game_viewport_target_,handle,game_viewport_gpu_,2)))return Result<void>::failure(graphics_error("EDITOR-GAME-VIEWPORT-TARGET","Could not create game viewport render target",E_FAIL));
            game_viewport_rtv_=handle;handle.ptr+=rtv_stride_;
        }
        // Full-res lit-color intermediate: the world pass always draws here so SSAO can sample it before composite.
        D3D12_HEAP_PROPERTIES texture_heap{};texture_heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC lit_desc{};lit_desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;lit_desc.Width=width_;lit_desc.Height=height_;lit_desc.DepthOrArraySize=1;lit_desc.MipLevels=1;lit_desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;lit_desc.SampleDesc.Count=1;lit_desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;lit_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE lit_clear{};lit_clear.Format=lit_desc.Format;lit_clear.Color[0]=0.11f;lit_clear.Color[1]=0.16f;lit_clear.Color[2]=0.24f;lit_clear.Color[3]=1.0f;
        lit_color_.Reset();
        HRESULT lit_hr=device_->CreateCommittedResource(&texture_heap,D3D12_HEAP_FLAG_NONE,&lit_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&lit_clear,IID_PPV_ARGS(&lit_color_));
        if(FAILED(lit_hr))return Result<void>::failure(graphics_error("GFX-LIT-TARGET","Could not create lit-color intermediate target",lit_hr));
        lit_rtv_=handle;device_->CreateRenderTargetView(lit_color_.Get(),nullptr,lit_rtv_);handle.ptr+=rtv_stride_;
        // Stable scene-color copy for water refraction (water writes lit_color_ and must not sample it).
        water_scene_color_.Reset();
        HRESULT water_scene_hr=device_->CreateCommittedResource(&texture_heap,D3D12_HEAP_FLAG_NONE,&lit_desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&lit_clear,IID_PPV_ARGS(&water_scene_color_));
        if(FAILED(water_scene_hr))
            return Result<void>::failure(graphics_error("GFX-WATER-SCENE-TARGET",
                "Could not create water scene-color copy target", water_scene_hr));
        // Half-res AO target.
        ao_width_=std::max<UINT>(1,width_/2);ao_height_=std::max<UINT>(1,height_/2);
        D3D12_RESOURCE_DESC ao_desc{};ao_desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;ao_desc.Width=ao_width_;ao_desc.Height=ao_height_;ao_desc.DepthOrArraySize=1;ao_desc.MipLevels=1;ao_desc.Format=DXGI_FORMAT_R8_UNORM;ao_desc.SampleDesc.Count=1;ao_desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;ao_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE ao_clear{};ao_clear.Format=ao_desc.Format;ao_clear.Color[0]=1.0f;ao_clear.Color[1]=1.0f;ao_clear.Color[2]=1.0f;ao_clear.Color[3]=1.0f;
        ao_target_.Reset();
        HRESULT ao_hr=device_->CreateCommittedResource(&texture_heap,D3D12_HEAP_FLAG_NONE,&ao_desc,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&ao_clear,IID_PPV_ARGS(&ao_target_));
        if(FAILED(ao_hr))return Result<void>::failure(graphics_error("GFX-AO-TARGET","Could not create AO target",ao_hr));
        ao_rtv_=handle;device_->CreateRenderTargetView(ao_target_.Get(),nullptr,ao_rtv_);
        // Depth is typeless so it can be bound as a DSV (D32_FLOAT) for the world pass and as an SRV (R32_FLOAT) for SSAO.
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=width_;desc.Height=height_;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32_TYPELESS;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        D3D12_CLEAR_VALUE clear{};clear.Format=DXGI_FORMAT_D32_FLOAT;clear.DepthStencil.Depth=1.0f;
        depth_.Reset();auto hr=device_->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth_));
        if(FAILED(hr))return Result<void>::failure(graphics_error("GFX-DEPTH","Could not create depth target",hr));
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_view{};dsv_view.Format=DXGI_FORMAT_D32_FLOAT;dsv_view.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
        device_->CreateDepthStencilView(depth_.Get(),&dsv_view,dsv_heap_->GetCPUDescriptorHandleForHeapStart());

        // Post-process SRVs: 0 depth, 1 lit, 2 AO, 3 water_scene (refraction source).
        auto post_cpu=post_srv_heap_->GetCPUDescriptorHandleForHeapStart();
        auto post_gpu=post_srv_heap_->GetGPUDescriptorHandleForHeapStart();
        D3D12_SHADER_RESOURCE_VIEW_DESC depth_srv{};depth_srv.Format=DXGI_FORMAT_R32_FLOAT;depth_srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;depth_srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;depth_srv.Texture2D.MipLevels=1;
        device_->CreateShaderResourceView(depth_.Get(),&depth_srv,post_cpu);
        post_depth_gpu_=post_gpu;
        post_cpu.ptr+=srv_stride_;post_gpu.ptr+=srv_stride_;
        D3D12_SHADER_RESOURCE_VIEW_DESC lit_srv{};lit_srv.Format=lit_desc.Format;lit_srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;lit_srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;lit_srv.Texture2D.MipLevels=1;
        device_->CreateShaderResourceView(lit_color_.Get(),&lit_srv,post_cpu);
        post_lit_gpu_=post_gpu;
        post_cpu.ptr+=srv_stride_;post_gpu.ptr+=srv_stride_;
        D3D12_SHADER_RESOURCE_VIEW_DESC ao_srv{};ao_srv.Format=ao_desc.Format;ao_srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;ao_srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;ao_srv.Texture2D.MipLevels=1;
        device_->CreateShaderResourceView(ao_target_.Get(),&ao_srv,post_cpu);
        post_cpu.ptr+=srv_stride_;post_gpu.ptr+=srv_stride_;
        device_->CreateShaderResourceView(water_scene_color_.Get(),&lit_srv,post_cpu);
        post_water_gpu_=post_gpu;
        return Result<void>::success();
    }

    Result<void> create_geometry(bool debug_world, const MaterialAsset&, const std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes){
        debug_world_ = debug_world;
        std::vector<Vertex> vertices;
        if (!debug_world) append_debug_ground_quad(vertices);
        append_physics_cube(vertices);
        mesh_ranges_.clear();
        append_imported_mesh_vertices(vertices, imported_meshes, mesh_ranges_);
        if (debug_world) {
            sky_vertex_offset_ = static_cast<UINT>(vertices.size());
            append_fullscreen_sky_triangle(vertices);
            sky_vertex_count_ = static_cast<UINT>(vertices.size()) - sky_vertex_offset_;
        } else {
            sky_vertex_offset_ = 0;
            sky_vertex_count_ = 0;
        }
        auto uploaded = upload_prop_vertices(vertices);
        if (!uploaded) return uploaded;
        return sync_mesh_albedos(imported_meshes);
    }

    Result<void> create_pipeline() {
        std::string shader = std::string(R"(
            cbuffer Frame : register(b0) {
                float4x4 viewProjection;
                float4 cameraAndFogStart;
                float4 fogColorAndEnd;
                float4 lightAndAmbient;
                float4 pointLight0PosRadius;
                float4 pointLight0ColorStrength;
                float4 pointLight1PosRadius;
                float4 pointLight1ColorStrength;
                float4 viewportSize;
            };
            cbuffer Object : register(b1) {
                float4x4 model;
                float4 materialParams;
                float4 emissive;
            };
            Texture2D<float4> albedoTex : register(t0);
            SamplerState albedoSampler : register(s0);
            struct In { float3 position:POSITION; float3 color:COLOR; float2 uv:TEXCOORD; };
            struct Out { float4 position : SV_POSITION; float3 color : COLOR; float3 worldPos : TEXCOORD0; float2 uv : TEXCOORD1; };
            Out vs(In input) {
                Out o;
                o.uv = input.uv;
                if (input.color.r < -0.5) {
                    o.position = float4(input.position.xy, input.position.z, 1.0);
                    o.worldPos = cameraAndFogStart.xyz;
                    o.color = input.color;
                    return o;
                }
                float4 world = mul(model, float4(input.position, 1.0));
                o.worldPos = world.xyz;
                o.position = mul(viewProjection, world);
                o.color = input.color;
                return o;
            }
)") + k_pbr_hlsl_helpers + R"(
            float4 ps(Out input) : SV_TARGET {
                if (input.color.r < -0.5) {
                    float t = 1.0 - (input.position.y / max(viewportSize.y, 1.0));
                    float3 horizon = float3(0.38, 0.52, 0.70);
                    float3 zenith = float3(0.18, 0.34, 0.58);
                    return float4(lerp(horizon, zenith, saturate(t)), 1.0);
                }
                float3 albedo = (materialParams.z > 0.5) ? albedoTex.Sample(albedoSampler, input.uv).rgb : input.color;
                if (dot(albedo, albedo) < 1e-6) albedo = float3(0.35, 0.45, 0.28);
                float dist = distance(input.worldPos, cameraAndFogStart.xyz);
                float fogRange = max(fogColorAndEnd.w - cameraAndFogStart.w, 0.001);
                float fogFactor = saturate((fogColorAndEnd.w - dist) / fogRange);
                float3 dpdx = ddx(input.worldPos);
                float3 dpdy = ddy(input.worldPos);
                float3 nrm = cross(dpdx, dpdy);
                float3 normal = (dot(nrm, nrm) > 1e-12) ? normalize(nrm) : float3(0, 1, 0);
                float3 V = normalize(cameraAndFogStart.xyz - input.worldPos);
                float3 L = normalize(-lightAndAmbient.xyz);
                if (dot(normal, V) < 0.0) normal = -normal;
                float3 lit = albedo * lightAndAmbient.w + emissive.rgb;
                float3 direct = shadePbr(albedo, materialParams.x, materialParams.y, normal, V, L, float3(1.35, 1.25, 1.1));
                if (all(isfinite(direct))) lit += direct;
                float3 p0 = applyPointLightPbr(input.worldPos, albedo, materialParams.x, materialParams.y, normal, V,
                    pointLight0PosRadius, pointLight0ColorStrength);
                float3 p1 = applyPointLightPbr(input.worldPos, albedo, materialParams.x, materialParams.y, normal, V,
                    pointLight1PosRadius, pointLight1ColorStrength);
                if (all(isfinite(p0))) lit += p0;
                if (all(isfinite(p1))) lit += p1;
                return float4(saturate(lerp(fogColorAndEnd.rgb, lit, fogFactor)), 1.0);
            }
        )";
        ComPtr<ID3DBlob> vs, ps, errors;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompile(shader.c_str(), shader.size(), "debug_triangle", nullptr, nullptr, "vs", "vs_5_1", flags, 0, &vs, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-VERTEX-SHADER", errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Vertex shader compilation failed", hr));
        errors.Reset();
        hr = D3DCompile(shader.c_str(), shader.size(), "debug_triangle", nullptr, nullptr, "ps", "ps_5_1", flags, 0, &ps, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-PIXEL-SHADER", errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Pixel shader compilation failed", hr));

        D3D12_ROOT_PARAMETER parameters[3]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].Descriptor.RegisterSpace = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants.ShaderRegister = 1;
        parameters[1].Constants.RegisterSpace = 0;
        parameters[1].Constants.Num32BitValues = 24;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_DESCRIPTOR_RANGE albedo_range{};
        albedo_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        albedo_range.NumDescriptors = 1;
        albedo_range.BaseShaderRegister = 0; // t0
        albedo_range.RegisterSpace = 0;
        albedo_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[2].DescriptorTable.NumDescriptorRanges = 1;
        parameters[2].DescriptorTable.pDescriptorRanges = &albedo_range;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC albedo_sampler{};
        albedo_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        albedo_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        albedo_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        albedo_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        albedo_sampler.MipLODBias = 0.0f;
        albedo_sampler.MaxAnisotropy = 1;
        albedo_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        albedo_sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        albedo_sampler.MinLOD = 0.0f;
        albedo_sampler.MaxLOD = D3D12_FLOAT32_MAX;
        albedo_sampler.ShaderRegister = 0; // s0
        albedo_sampler.RegisterSpace = 0;
        albedo_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC root{};
        root.NumParameters = 3;
        root.pParameters = parameters;
        root.NumStaticSamplers = 1;
        root.pStaticSamplers = &albedo_sampler;
        root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ROOT-SIGNATURE-SERIALIZE", errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Could not serialize root signature", hr));
        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&root_signature_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-ROOT-SIGNATURE", "Could not create root signature", hr));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
        state.pRootSignature = root_signature_.Get();
        state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};state.InputLayout={input,3};
        state.BlendState.AlphaToCoverageEnable = FALSE;
        state.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC blend{FALSE, FALSE, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL};
        for (auto& target : state.BlendState.RenderTarget) target = blend;
        state.SampleMask = UINT_MAX;
        state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable = TRUE;
        state.DepthStencilState.DepthEnable = TRUE;state.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;state.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;
        state.DepthStencilState.StencilEnable = FALSE;
        state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.NumRenderTargets = 1;
        state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        state.DSVFormat=DXGI_FORMAT_D32_FLOAT;
        state.SampleDesc.Count = 1;
        hr = device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&pipeline_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-PIPELINE", "Could not create debug pipeline", hr));
        state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        hr = device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&sky_pipeline_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SKY-PIPELINE", "Could not create sky pipeline", hr));
        const auto foliage_pipeline = create_foliage_pipeline(ps);
        if (!foliage_pipeline) return Result<void>::failure(foliage_pipeline.error());
        return Result<void>::success();
    }

    Result<void> create_water_pipeline() {
        const char* water_shader = R"(
            cbuffer Frame : register(b0) {
                float4x4 viewProjection;
                float4 cameraAndFogStart;
                float4 fogColorAndEnd;
                float4 lightAndAmbient;
                float4 pointLight0PosRadius;
                float4 pointLight0ColorStrength;
                float4 pointLight1PosRadius;
                float4 pointLight1ColorStrength;
                float4 viewportSize;
            };
            cbuffer WaterParams : register(b1) {
                float timeSeconds;
                float roughness;
                float waveAmplitude;
                float waveFrequency;
                float4 tint;
            };
            Texture2D sceneColor : register(t0);
            SamplerState linearClamp : register(s0);
            struct In { float3 position:POSITION; float3 color:COLOR; float2 uv:TEXCOORD; };
            struct Out { float4 position : SV_POSITION; float3 worldPos : TEXCOORD0; float2 uv : TEXCOORD1; float3 color : COLOR; };
            float waveHeight(float2 xz, float t) {
                float w1 = sin(xz.x * waveFrequency + t * 1.7) * waveAmplitude;
                float w2 = cos(xz.y * waveFrequency * 0.85 + t * 1.3) * waveAmplitude * 0.65;
                float w3 = sin((xz.x + xz.y) * waveFrequency * 0.45 + t * 2.1) * waveAmplitude * 0.35;
                return w1 + w2 + w3;
            }
            float3 waveNormal(float2 xz, float t) {
                float f = waveFrequency;
                float a = waveAmplitude;
                float dhdx = cos(xz.x * f + t * 1.7) * a * f
                    + cos((xz.x + xz.y) * f * 0.45 + t * 2.1) * a * 0.35 * f * 0.45;
                float dhdz = -sin(xz.y * f * 0.85 + t * 1.3) * a * 0.65 * f * 0.85
                    + cos((xz.x + xz.y) * f * 0.45 + t * 2.1) * a * 0.35 * f * 0.45;
                return normalize(float3(-dhdx, 1.0, -dhdz));
            }
            float hash21(float2 p) {
                float3 p3 = frac(float3(p.xyx) * 0.1031);
                p3 += dot(p3, p3.yzx + 33.33);
                return frac((p3.x + p3.y) * p3.z);
            }
            Out vs(In input) {
                Out o;
                o.uv = input.uv;
                o.color = input.color;
                float3 pos = input.position;
                pos.y += waveHeight(pos.xz, timeSeconds);
                float4 world = float4(pos, 1.0);
                o.worldPos = world.xyz;
                o.position = mul(viewProjection, world);
                return o;
            }
            float4 ps(Out input) : SV_TARGET {
                float2 uv = input.position.xy / max(viewportSize.xy, float2(1.0, 1.0));
                // sceneColor is a pre-water copy of the lit buffer (never the live RT).
                float2 refractOffset = float2(sin(input.worldPos.x * 0.15 + timeSeconds) * 0.0025,
                    cos(input.worldPos.z * 0.12 + timeSeconds * 0.9) * 0.0025);
                float3 refracted = sceneColor.Sample(linearClamp, uv + refractOffset).rgb;
                float3 V = normalize(cameraAndFogStart.xyz - input.worldPos);
                float3 N = waveNormal(input.worldPos.xz, timeSeconds);
                float3 L = normalize(-lightAndAmbient.xyz);
                float3 H = normalize(L + V);
                float3 sky = lerp(float3(0.22, 0.30, 0.42), float3(0.16, 0.28, 0.46), saturate(V.y * 0.5 + 0.5));
                float fresnel = pow(1.0 - saturate(dot(N, V)), 3.0);
                float3 waterColor = lerp(tint.rgb, sky, fresnel * 0.55);
                // Vertex uv.x = column depth (m). Strong absorption so bed facets don't read through.
                // Cover already mixes refraction; keep alpha high so RT alpha-blend doesn't double-expose the bed.
                float depthM = max(input.uv.x, 0.0);
                float optical = saturate(1.0 - exp(-depthM * 2.4));
                float cover = lerp(0.55, 0.985, optical);
                float3 color = lerp(refracted, waterColor, cover);
                color = lerp(color, tint.rgb * 0.28, optical * 0.75);

                // Soft tinted sheen + crest glints — same hue family as the water material, lit by sun/ambient.
                float ndotl = saturate(dot(N, L));
                float lighting = saturate(lightAndAmbient.w * 0.85 + ndotl * 0.95);
                float specPower = lerp(48.0, 96.0, saturate(1.0 - roughness * 4.0));
                float sheen = pow(saturate(dot(N, H)), specPower) * ndotl;
                float crest = saturate(waveHeight(input.worldPos.xz, timeSeconds) / max(waveAmplitude, 1e-4) * 0.5 + 0.5);
                float2 sparkleUv = input.worldPos.xz * 2.4 + float2(timeSeconds * 0.28, -timeSeconds * 0.18);
                float sparkleCell = hash21(floor(sparkleUv));
                float sparkle = smoothstep(0.88, 0.985, sparkleCell) * smoothstep(0.6, 0.95, crest);
                float twinkle = 0.75 + 0.25 * sin(timeSeconds * 5.5 + sparkleCell * 28.0);
                // Lift the material tint slightly toward sky/fresnel; never jump to hot white/yellow.
                float3 glistenTint = saturate(lerp(tint.rgb * 1.35, waterColor * 1.15, 0.45));
                float glistenAmt = (sheen * (0.14 + fresnel * 0.18) + sparkle * twinkle * 0.07) * lighting;
                color += glistenTint * glistenAmt;

                float alpha = lerp(0.78, max(tint.a, 0.97), optical);
                return float4(saturate(color), alpha);
            }
        )";
        ComPtr<ID3DBlob> vs, ps, errors;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompile(water_shader, strlen(water_shader), "water_shader", nullptr, nullptr, "vs", "vs_5_1",
            flags, 0, &vs, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-VS",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Water VS failed", hr));
        errors.Reset();
        hr = D3DCompile(water_shader, strlen(water_shader), "water_shader", nullptr, nullptr, "ps", "ps_5_1", flags,
            0, &ps, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-PS",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Water PS failed", hr));

        D3D12_ROOT_PARAMETER parameters[3]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants.ShaderRegister = 1;
        parameters[1].Constants.Num32BitValues = 8;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        // Single SRV: pre-water lit scene copy (t0). Depth stays bound as DSV for testing only.
        D3D12_DESCRIPTOR_RANGE ranges[1]{};
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].NumDescriptors = 1;
        ranges[0].BaseShaderRegister = 0;
        ranges[0].OffsetInDescriptorsFromTableStart = 0;
        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[2].DescriptorTable.NumDescriptorRanges = 1;
        parameters[2].DescriptorTable.pDescriptorRanges = ranges;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_STATIC_SAMPLER_DESC sampler{};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampler.ShaderRegister = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC root{};
        root.NumParameters = 3;
        root.pParameters = parameters;
        root.NumStaticSamplers = 1;
        root.pStaticSamplers = &sampler;
        root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-ROOT",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Water root failed", hr));
        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&water_root_signature_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-ROOT-CREATE", "Could not create water root", hr));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
        state.pRootSignature = water_root_signature_.Get();
        state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        D3D12_INPUT_ELEMENT_DESC input[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                                                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        state.InputLayout = {input, 3};
        D3D12_RENDER_TARGET_BLEND_DESC blend{};
        blend.BlendEnable = TRUE;
        blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blend.BlendOp = D3D12_BLEND_OP_ADD;
        blend.SrcBlendAlpha = D3D12_BLEND_ONE;
        blend.DestBlendAlpha = D3D12_BLEND_ZERO;
        blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        state.BlendState.RenderTarget[0] = blend;
        state.SampleMask = UINT_MAX;
        state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        state.DepthStencilState.DepthEnable = TRUE;
        state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.NumRenderTargets = 1;
        state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        state.SampleDesc.Count = 1;
        hr = device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&water_pipeline_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-WATER-PIPELINE", "Could not create water pipeline", hr));
        return Result<void>::success();
    }

    // SSAO v1: two fullscreen-triangle passes. Root signatures use a root CBV + a single descriptor table
    // (instead of many inline 32-bit constants) to stay well clear of the 64-DWORD root signature limit.
    Result<void> create_ssao_pipelines() {
        const char* fullscreen_vs = R"(
            struct Out { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
            Out vs(uint id : SV_VertexID) {
                Out o;
                float2 pos = float2((id == 1) ? 3.0 : -1.0, (id == 2) ? 3.0 : -1.0);
                o.position = float4(pos, 0.0, 1.0);
                o.uv = float2(pos.x * 0.5 + 0.5, 1.0 - (pos.y * 0.5 + 0.5));
                return o;
            }
        )";
        const char* ssao_ps = R"(
            cbuffer SsaoParams : register(b0) {
                float4x4 invViewProjection;
                float4x4 viewProjection;
                float4 cameraPositionRadius;
                float4 biasIntensityTexel; // x=bias, y=intensity, zw=1/fullResSize for depth normals
            };
            Texture2D depthTex : register(t0);
            SamplerState pointClampSampler : register(s0);
            struct In { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };

            float3 reconstructWorldPos(float2 uv, float depth) {
                float4 clip = float4(uv.x * 2.0 - 1.0, (1.0 - uv.y) * 2.0 - 1.0, depth, 1.0);
                float4 worldH = mul(invViewProjection, clip);
                return worldH.xyz / max(worldH.w, 1e-6);
            }

            float2 projectToUv(float3 worldPos, out bool valid) {
                float4 clip = mul(viewProjection, float4(worldPos, 1.0));
                valid = clip.w > 0.0001;
                float3 ndc = clip.xyz / max(clip.w, 0.0001);
                return float2(ndc.x * 0.5 + 0.5, 1.0 - (ndc.y * 0.5 + 0.5));
            }

            // Interleaved gradient noise — breaks coherent banding better than a simple hash.
            float interleavedGradientNoise(float2 pixel) {
                return frac(52.9829189 * frac(dot(pixel, float2(0.06711056, 0.00583715))));
            }

            float3 reconstructNormal(float2 uv, float centerDepth, float3 centerPos) {
                float2 texel = biasIntensityTexel.zw;
                float dR = depthTex.SampleLevel(pointClampSampler, uv + float2(texel.x, 0), 0).r;
                float dU = depthTex.SampleLevel(pointClampSampler, uv + float2(0, -texel.y), 0).r;
                float3 pR = reconstructWorldPos(uv + float2(texel.x, 0), dR);
                float3 pU = reconstructWorldPos(uv + float2(0, -texel.y), dU);
                float3 normal = normalize(cross(pR - centerPos, pU - centerPos));
                if (dot(normal, cameraPositionRadius.xyz - centerPos) < 0.0) normal = -normal;
                return normal;
            }

            static const float3 kKernel[12] = {
                float3(0.5381, 0.1856, 0.4290), float3(0.1379, 0.2486, 0.4430),
                float3(0.3371, 0.5679, 0.0057), float3(-0.6999, -0.0451, 0.0019),
                float3(0.0689, -0.1598, 0.8547), float3(0.0560, 0.0069, 0.1843),
                float3(-0.0146, 0.1402, 0.0762), float3(0.0100, -0.1924, 0.0344),
                float3(-0.3577, -0.5301, 0.4358), float3(-0.3169, 0.1063, 0.0158),
                float3(0.0103, -0.5869, 0.0046), float3(-0.0897, -0.4940, 0.2557),
            };

            float4 ps(In input) : SV_TARGET {
                float depth = depthTex.SampleLevel(pointClampSampler, input.uv, 0).r;
                if (depth >= 0.999) return float4(1, 1, 1, 1);
                float3 worldPos = reconstructWorldPos(input.uv, depth);
                float3 normal = reconstructNormal(input.uv, depth, worldPos);
                float3 up = (abs(normal.y) < 0.999) ? float3(0, 1, 0) : float3(1, 0, 0);
                float3 tangent = normalize(cross(up, normal));
                float3 bitangent = cross(normal, tangent);
                float angle = interleavedGradientNoise(input.position.xy) * 6.2831853;
                float ca = cos(angle), sa = sin(angle);
                float radius = cameraPositionRadius.w;
                float bias = biasIntensityTexel.x;
                float occlusion = 0.0;
                float weightSum = 0.0;
                [unroll]
                for (int i = 0; i < 12; ++i) {
                    float3 k = kKernel[i];
                    float t = (float)i / 11.0;
                    float scale = lerp(0.15, 1.0, t * t);
                    float2 rotatedXY = float2(k.x * ca - k.y * sa, k.x * sa + k.y * ca);
                    float3 sampleOffset = (tangent * rotatedXY.x + bitangent * rotatedXY.y + normal * abs(k.z)) * scale;
                    float3 samplePos = worldPos + sampleOffset * radius;
                    bool valid;
                    float2 sampleUv = projectToUv(samplePos, valid);
                    if (!valid || any(sampleUv < 0.0) || any(sampleUv > 1.0)) continue;
                    float sampleDepth = depthTex.SampleLevel(pointClampSampler, sampleUv, 0).r;
                    if (sampleDepth >= 0.999) continue;
                    float3 scenePos = reconstructWorldPos(sampleUv, sampleDepth);
                    float sampleDist = distance(cameraPositionRadius.xyz, samplePos);
                    float sceneDist = distance(cameraPositionRadius.xyz, scenePos);
                    float rangeCheck = 1.0 - saturate(abs(sampleDist - sceneDist) / radius);
                    occlusion += ((sceneDist + bias < sampleDist) ? 1.0 : 0.0) * rangeCheck;
                    weightSum += 1.0;
                }
                float ao = 1.0 - (weightSum > 0.0 ? occlusion / weightSum : 0.0);
                ao = saturate(pow(ao, 1.35));
                return float4(ao, ao, ao, 1.0);
            }
        )";
        const char* composite_ps = R"(
            cbuffer CompositeParams : register(b0) {
                float4 intensityTexel; // x = intensity, y = 1/aoWidth, z = 1/aoHeight, w unused
            };
            Texture2D litTex : register(t0);
            Texture2D aoTex : register(t1);
            SamplerState linearClampSampler : register(s0);
            struct In { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
            float4 ps(In input) : SV_TARGET {
                float3 color = litTex.SampleLevel(linearClampSampler, input.uv, 0).rgb;
                float2 texel = intensityTexel.yz;
                // Wide 9-tap blur to kill half-res SSAO striping before composite.
                float ao = 0.0;
                float weight = 0.0;
                [unroll]
                for (int y = -1; y <= 1; ++y) {
                    [unroll]
                    for (int x = -1; x <= 1; ++x) {
                        float w = (x == 0 && y == 0) ? 4.0 : ((x == 0 || y == 0) ? 2.0 : 1.0);
                        ao += aoTex.SampleLevel(linearClampSampler, input.uv + float2(x, y) * 1.5 * texel, 0).r * w;
                        weight += w;
                    }
                }
                ao /= max(weight, 1.0);
                float factor = lerp(1.0, ao, intensityTexel.x);
                return float4(color * factor, 1.0);
            }
        )";
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ComPtr<ID3DBlob> fullscreen_vs_blob, ssao_ps_blob, composite_ps_blob, errors;
        HRESULT hr = D3DCompile(fullscreen_vs, std::strlen(fullscreen_vs), "ssao_fullscreen", nullptr, nullptr, "vs",
            "vs_5_1", flags, 0, &fullscreen_vs_blob, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-VERTEX-SHADER",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "SSAO vertex shader compilation failed", hr));
        errors.Reset();
        hr = D3DCompile(ssao_ps, std::strlen(ssao_ps), "ssao", nullptr, nullptr, "ps", "ps_5_1", flags, 0, &ssao_ps_blob, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-PIXEL-SHADER",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "SSAO pixel shader compilation failed", hr));
        errors.Reset();
        hr = D3DCompile(composite_ps, std::strlen(composite_ps), "ssao_composite", nullptr, nullptr, "ps", "ps_5_1",
            flags, 0, &composite_ps_blob, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMPOSITE-PIXEL-SHADER",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Composite pixel shader compilation failed", hr));

        D3D12_STATIC_SAMPLER_DESC point_sampler{};
        point_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        point_sampler.AddressU = point_sampler.AddressV = point_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        point_sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        point_sampler.ShaderRegister = 0;
        point_sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_DESCRIPTOR_RANGE ssao_range{};
        ssao_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ssao_range.NumDescriptors = 1;
        ssao_range.BaseShaderRegister = 0;
        ssao_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER ssao_parameters[2]{};
        ssao_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        ssao_parameters[0].Descriptor.ShaderRegister = 0;
        ssao_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        ssao_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        ssao_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        ssao_parameters[1].DescriptorTable.pDescriptorRanges = &ssao_range;
        ssao_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC ssao_root{};
        ssao_root.NumParameters = 2;
        ssao_root.pParameters = ssao_parameters;
        ssao_root.NumStaticSamplers = 1;
        ssao_root.pStaticSamplers = &point_sampler;
        ComPtr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&ssao_root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-ROOT-SIGNATURE-SERIALIZE",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Could not serialize SSAO root signature", hr));
        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&ssao_root_signature_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-ROOT-SIGNATURE", "Could not create SSAO root signature", hr));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC ssao_state{};
        ssao_state.pRootSignature = ssao_root_signature_.Get();
        ssao_state.VS = {fullscreen_vs_blob->GetBufferPointer(), fullscreen_vs_blob->GetBufferSize()};
        ssao_state.PS = {ssao_ps_blob->GetBufferPointer(), ssao_ps_blob->GetBufferSize()};
        ssao_state.BlendState.RenderTarget[0] = {FALSE, FALSE, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL};
        ssao_state.SampleMask = UINT_MAX;
        ssao_state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        ssao_state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        ssao_state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        ssao_state.NumRenderTargets = 1;
        ssao_state.RTVFormats[0] = DXGI_FORMAT_R8_UNORM;
        ssao_state.DSVFormat = DXGI_FORMAT_UNKNOWN;
        ssao_state.SampleDesc.Count = 1;
        hr = device_->CreateGraphicsPipelineState(&ssao_state, IID_PPV_ARGS(&ssao_pipeline_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-PIPELINE", "Could not create SSAO pipeline", hr));

        D3D12_STATIC_SAMPLER_DESC linear_sampler = point_sampler;
        linear_sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        D3D12_DESCRIPTOR_RANGE composite_range{};
        composite_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        composite_range.NumDescriptors = 2;
        composite_range.BaseShaderRegister = 0;
        composite_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        D3D12_ROOT_PARAMETER composite_parameters[2]{};
        composite_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        composite_parameters[0].Descriptor.ShaderRegister = 0;
        composite_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        composite_parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        composite_parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        composite_parameters[1].DescriptorTable.pDescriptorRanges = &composite_range;
        composite_parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC composite_root{};
        composite_root.NumParameters = 2;
        composite_root.pParameters = composite_parameters;
        composite_root.NumStaticSamplers = 1;
        composite_root.pStaticSamplers = &linear_sampler;
        signature.Reset(); errors.Reset();
        hr = D3D12SerializeRootSignature(&composite_root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMPOSITE-ROOT-SIGNATURE-SERIALIZE",
            errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Could not serialize composite root signature", hr));
        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&composite_root_signature_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMPOSITE-ROOT-SIGNATURE", "Could not create composite root signature", hr));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC composite_state = ssao_state;
        composite_state.pRootSignature = composite_root_signature_.Get();
        composite_state.PS = {composite_ps_blob->GetBufferPointer(), composite_ps_blob->GetBufferSize()};
        composite_state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        hr = device_->CreateGraphicsPipelineState(&composite_state, IID_PPV_ARGS(&composite_pipeline_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMPOSITE-PIPELINE", "Could not create composite pipeline", hr));
        return Result<void>::success();
    }

    Result<void> create_post_constant_buffers() {
        const auto make_cb = [&](ComPtr<ID3D12Resource>& resource, void*& mapped) -> HRESULT {
            resource.Reset();
            mapped = nullptr;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            HRESULT resource_hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
            if (FAILED(resource_hr)) return resource_hr;
            return resource->Map(0, nullptr, &mapped);
        };
        HRESULT hr = make_cb(ssao_cb_, ssao_cb_mapped_);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-SSAO-CB", "Could not create SSAO constant buffer", hr));
        hr = make_cb(composite_cb_, composite_cb_mapped_);
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-COMPOSITE-CB", "Could not create composite constant buffer", hr));
        return Result<void>::success();
    }

    Result<void> create_foliage_pipeline(ComPtr<ID3DBlob> /*pixel_shader*/) {
        const char* foliage_vs = R"(
            cbuffer Frame : register(b0) {
                float4x4 viewProjection;
                float4 cameraAndFogStart;
                float4 fogColorAndEnd;
                float4 lightAndAmbient;
                float4 pointLight0PosRadius;
                float4 pointLight0ColorStrength;
                float4 pointLight1PosRadius;
                float4 pointLight1ColorStrength;
                float4 viewportSize;
            };
            cbuffer Interaction : register(b2) {
                float4 influencePosRadius;
                float4 influenceVelStrength;
                float4 layerBladeTime;
            };
            StructuredBuffer<float4> instanceRows : register(t0);
            struct In { float3 position:POSITION; float3 color:COLOR; };
            struct Out { float4 position : SV_POSITION; float3 color : COLOR; float3 worldPos : TEXCOORD0; };
            Out vs(In input, uint instanceId : SV_InstanceID) {
                Out o;
                const uint baseIndex = instanceId * 4u;
                float4x4 model = transpose(float4x4(
                    instanceRows[baseIndex + 0u],
                    instanceRows[baseIndex + 1u],
                    instanceRows[baseIndex + 2u],
                    instanceRows[baseIndex + 3u]));
                float4 world = mul(model, float4(input.position, 1.0));
                const float bladeFactor = saturate(input.position.y / max(layerBladeTime.x, 0.001));
                float2 deltaXZ = world.xz - influencePosRadius.xz;
                float dist = length(deltaXZ);
                const float falloff = saturate(1.0 - dist / max(influencePosRadius.w, 0.001));
                const float bend = influenceVelStrength.w * bladeFactor * falloff;
                if (bend > 0.0001) {
                    float2 away = dist > 0.0001 ? deltaXZ / dist : float2(0, 0);
                    world.x += away.x * bend * 0.45;
                    world.z += away.y * bend * 0.45;
                    float3 vel = influenceVelStrength.xyz;
                    vel.y = 0;
                    const float velLen = length(vel);
                    if (velLen > 0.001)
                        world.xyz += (vel / velLen) * velLen * 0.05 * bladeFactor * falloff;
                }
                o.worldPos = world.xyz;
                o.position = mul(viewProjection, world);
                o.color = input.color;
                return o;
            }
        )";
        const char* foliage_ps_prefix = R"(
            cbuffer Frame : register(b0) {
                float4x4 viewProjection;
                float4 cameraAndFogStart;
                float4 fogColorAndEnd;
                float4 lightAndAmbient;
                float4 pointLight0PosRadius;
                float4 pointLight0ColorStrength;
                float4 pointLight1PosRadius;
                float4 pointLight1ColorStrength;
                float4 viewportSize;
            };
            struct In { float4 position : SV_POSITION; float3 color : COLOR; float3 worldPos : TEXCOORD0; };
)";
        const std::string foliage_ps = std::string(foliage_ps_prefix) + k_pbr_hlsl_helpers + R"(
            float4 ps(In input) : SV_TARGET {
                float dist = distance(input.worldPos, cameraAndFogStart.xyz);
                float fogRange = max(fogColorAndEnd.w - cameraAndFogStart.w, 0.001);
                float fogFactor = saturate((fogColorAndEnd.w - dist) / fogRange);
                float3 albedo = input.color;
                if (dot(albedo, albedo) < 1e-6) albedo = float3(0.30, 0.48, 0.22);
                float3 dpdx = ddx(input.worldPos);
                float3 dpdy = ddy(input.worldPos);
                float3 nrm = cross(dpdx, dpdy);
                float3 normal = (dot(nrm, nrm) > 1e-12) ? normalize(nrm) : float3(0, 1, 0);
                float3 V = normalize(cameraAndFogStart.xyz - input.worldPos);
                float3 L = normalize(-lightAndAmbient.xyz);
                if (dot(normal, V) < 0.0) normal = -normal;
                float3 lit = albedo * lightAndAmbient.w;
                float3 direct = shadePbr(albedo, 1.0, 0.0, normal, V, L, float3(1.35, 1.25, 1.1));
                if (all(isfinite(direct))) lit += direct;
                float3 p0 = applyPointLightPbr(input.worldPos, albedo, 1.0, 0.0, normal, V, pointLight0PosRadius,
                    pointLight0ColorStrength);
                float3 p1 = applyPointLightPbr(input.worldPos, albedo, 1.0, 0.0, normal, V, pointLight1PosRadius,
                    pointLight1ColorStrength);
                if (all(isfinite(p0))) lit += p0;
                if (all(isfinite(p1))) lit += p1;
                return float4(saturate(lerp(fogColorAndEnd.rgb, lit, fogFactor)), 1.0);
            }
        )";
        ComPtr<ID3DBlob> vs, ps, errors;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompile(foliage_vs, std::strlen(foliage_vs), "foliage_instanced", nullptr, nullptr, "vs",
            "vs_5_1", flags, 0, &vs, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-FOLIAGE-VERTEX-SHADER",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Foliage vertex shader compilation failed",
                hr));
        errors.Reset();
        hr = D3DCompile(foliage_ps.c_str(), foliage_ps.size(), "foliage_instanced", nullptr, nullptr, "ps", "ps_5_1",
            flags, 0, &ps, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-FOLIAGE-PIXEL-SHADER",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Foliage pixel shader compilation failed",
                hr));

        D3D12_ROOT_PARAMETER parameters[3]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].Descriptor.RegisterSpace = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_DESCRIPTOR_RANGE foliage_srv_range{};
        foliage_srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        foliage_srv_range.NumDescriptors = 1;
        foliage_srv_range.BaseShaderRegister = 0;
        foliage_srv_range.RegisterSpace = 0;
        foliage_srv_range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[1].DescriptorTable.NumDescriptorRanges = 1;
        parameters[1].DescriptorTable.pDescriptorRanges = &foliage_srv_range;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[2].Constants.ShaderRegister = 2;
        parameters[2].Constants.RegisterSpace = 0;
        parameters[2].Constants.Num32BitValues = 12;
        parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        D3D12_ROOT_SIGNATURE_DESC root{};
        root.NumParameters = 3;
        root.pParameters = parameters;
        root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> signature;
        hr = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &errors);
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-FOLIAGE-ROOT-SIGNATURE-SERIALIZE",
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Could not serialize foliage root signature",
                hr));
        hr = device_->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(),
            IID_PPV_ARGS(&foliage_root_signature_));
        if (FAILED(hr))
            return Result<void>::failure(graphics_error("GFX-FOLIAGE-ROOT-SIGNATURE", "Could not create foliage root signature", hr));

        D3D12_GRAPHICS_PIPELINE_STATE_DESC state{};
        state.pRootSignature = foliage_root_signature_.Get();
        state.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        state.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        D3D12_INPUT_ELEMENT_DESC input[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                                                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        state.InputLayout = {input, 3};
        state.BlendState.AlphaToCoverageEnable = FALSE;
        state.BlendState.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC blend{FALSE, FALSE, D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD, D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL};
        for (auto& target : state.BlendState.RenderTarget) target = blend;
        state.SampleMask = UINT_MAX;
        state.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        state.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        state.RasterizerState.DepthClipEnable = TRUE;
        state.DepthStencilState.DepthEnable = TRUE;
        state.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        state.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        state.DepthStencilState.StencilEnable = FALSE;
        state.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        state.NumRenderTargets = 1;
        state.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        state.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        state.SampleDesc.Count = 1;
        hr = device_->CreateGraphicsPipelineState(&state, IID_PPV_ARGS(&foliage_pipeline_));
        if (FAILED(hr)) return Result<void>::failure(graphics_error("GFX-FOLIAGE-PIPELINE", "Could not create foliage pipeline", hr));
        if (!foliage_srv_heap_) {
            D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
            heap_desc.NumDescriptors = 1;
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&foliage_srv_heap_));
            if (FAILED(hr))
                return Result<void>::failure(
                    graphics_error("GFX-FOLIAGE-SRV-HEAP", "Could not create foliage instance descriptor heap", hr));
            foliage_instance_srv_cpu_ = foliage_srv_heap_->GetCPUDescriptorHandleForHeapStart();
            foliage_instance_srv_gpu_ = foliage_srv_heap_->GetGPUDescriptorHandleForHeapStart();
        }
        return Result<void>::success();
    }

    void update_foliage_instance_srv(UINT float4_count) {
        if (!foliage_instance_buffer_ || float4_count == 0 || !foliage_srv_heap_) return;
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements = float4_count;
        srv_desc.Buffer.StructureByteStride = sizeof(float) * 4;
        device_->CreateShaderResourceView(foliage_instance_buffer_.Get(), &srv_desc, foliage_instance_srv_cpu_);
        foliage_instance_float4_count_ = float4_count;
    }

public:
    Result<void> set_foliage_batches(const std::map<std::string, std::vector<FoliageInstance>>& batches,
        const FoliageLayerPalette* palette) {
        foliage_draws_.clear();
        std::vector<float> instance_rows;
        instance_rows.reserve(1024);
        UINT instance_offset = 0;
        for (const auto& batch : batches) {
            if (batch.second.empty()) continue;
            const auto found = mesh_ranges_.find(normalize_asset_path(batch.first));
            if (found == mesh_ranges_.end()) continue;
            FoliageGpuDraw draw{batch.first, instance_offset, static_cast<UINT>(batch.second.size())};
            draw.center_x = batch.second.front().model[12];
            draw.center_z = batch.second.front().model[14];
            if (palette) {
                if (const auto* layer = palette->find_by_index(batch.second.front().layer_index)) {
                    draw.bend_strength = layer->bend_strength;
                    draw.bend_radius = layer->bend_radius;
                    draw.blade_height = layer->blade_height;
                }
            }
            for (const auto& instance : batch.second) {
                for (float value : instance.model) instance_rows.push_back(value);
            }
            instance_offset += draw.instance_count;
            foliage_draws_.push_back(std::move(draw));
        }
        if (instance_rows.empty()) {
            foliage_instance_buffer_.Reset();
            foliage_instance_capacity_ = 0;
            foliage_instance_float4_count_ = 0;
            return Result<void>::success();
        }
        const UINT required_floats = static_cast<UINT>(instance_rows.size());
        if (required_floats > foliage_instance_capacity_) {
            wait_for_gpu();
            foliage_instance_buffer_.Reset();
            foliage_instance_capacity_ = std::max(required_floats, foliage_instance_capacity_ + 4096u);
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            desc.Width = static_cast<UINT64>(foliage_instance_capacity_) * sizeof(float);
            desc.Height = 1;
            desc.DepthOrArraySize = 1;
            desc.MipLevels = 1;
            desc.SampleDesc.Count = 1;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            auto hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&foliage_instance_buffer_));
            if (FAILED(hr))
                return Result<void>::failure(
                    graphics_error("GFX-FOLIAGE-INSTANCE-BUFFER", "Could not create foliage instance buffer", hr));
        }
        void* mapped = nullptr;
        foliage_instance_buffer_->Map(0, nullptr, &mapped);
        std::memcpy(mapped, instance_rows.data(), instance_rows.size() * sizeof(float));
        foliage_instance_buffer_->Unmap(0, nullptr);
        update_foliage_instance_srv(required_floats / 4u);
        return Result<void>::success();
    }

    void draw_foliage_instances(const std::array<float, 48>& frame_constants, const WorldInfluenceBus* influence,
        float time_seconds) {
        if (!foliage_pipeline_ || foliage_draws_.empty() || !foliage_instance_buffer_ || foliage_instance_float4_count_ == 0)
            return;
        command_list_->SetPipelineState(foliage_pipeline_.Get());
        command_list_->SetGraphicsRootSignature(foliage_root_signature_.Get());
        bind_frame_constants(frame_constants);
        ID3D12DescriptorHeap* heaps[] = {foliage_srv_heap_.Get()};
        command_list_->SetDescriptorHeaps(1, heaps);
        command_list_->SetGraphicsRootDescriptorTable(1, foliage_instance_srv_gpu_);
        command_list_->IASetVertexBuffers(0, 1, &vertex_view_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for (const auto& draw : foliage_draws_) {
            const auto found = mesh_ranges_.find(normalize_asset_path(draw.mesh_key));
            if (found == mesh_ranges_.end() || draw.instance_count == 0) continue;
            std::array<float, 12> interaction{};
            if (influence && !influence->empty()) {
                const WorldInfluenceSource dominant = influence->dominant_at(draw.center_x, draw.center_z);
                interaction[0] = dominant.position[0];
                interaction[1] = dominant.position[1];
                interaction[2] = dominant.position[2];
                interaction[3] = std::min(draw.bend_radius, dominant.radius);
                interaction[4] = dominant.velocity[0];
                interaction[5] = dominant.velocity[1];
                interaction[6] = dominant.velocity[2];
                interaction[7] = draw.bend_strength * dominant.strength;
            }
            interaction[8] = draw.blade_height;
            interaction[9] = time_seconds;
            command_list_->SetGraphicsRoot32BitConstants(2, 12, interaction.data(), 0);
            command_list_->DrawInstanced(found->second.second, draw.instance_count, found->second.first, draw.instance_offset);
        }
        command_list_->SetPipelineState(pipeline_.Get());
        command_list_->SetGraphicsRootSignature(root_signature_.Get());
    }

    /// GPU readback of the last presented swap-chain buffer (includes ImGui chrome). Matches rendered colors.
    Result<std::filesystem::path> capture_presented_backbuffer_png(const std::filesystem::path& project_root,
        const std::string& filename_stem) {
        if (!has_presented_backbuffer_ || !device_ || !queue_ || !command_list_) {
            return Result<std::filesystem::path>::failure(
                EngineError{"SHOT-GPU-FRAME", Severity::Error, ErrorCategory::Io, "automation",
                    "No presented backbuffer is available yet", std::nullopt, {},
                    "Wait one frame after the editor starts, then retry."});
        }
        if (last_presented_index_ >= frame_count || !targets_[last_presented_index_]) {
            return Result<std::filesystem::path>::failure(
                EngineError{"SHOT-GPU-TARGET", Severity::Error, ErrorCategory::Io, "automation",
                    "Presented backbuffer target is missing", std::nullopt, {}, "Retry after a successful present."});
        }

        wait_for_gpu();
        ID3D12Resource* source = targets_[last_presented_index_].Get();
        const auto desc = source->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 readback_size = 0;
        device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &readback_size);

        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = readback_size;
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> readback;
        HRESULT hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
        if (FAILED(hr)) {
            return Result<std::filesystem::path>::failure(
                graphics_error("SHOT-GPU-BUFFER", "Could not allocate screenshot readback", hr));
        }

        hr = allocators_[frame_index_]->Reset();
        if (FAILED(hr)) {
            return Result<std::filesystem::path>::failure(
                device_error("SHOT-GPU-ALLOC", "Could not reset screenshot allocator", hr));
        }
        hr = command_list_->Reset(allocators_[frame_index_].Get(), nullptr);
        if (FAILED(hr)) {
            return Result<std::filesystem::path>::failure(
                device_error("SHOT-GPU-LIST", "Could not reset screenshot command list", hr));
        }
        auto to_copy = CD3DX12_RESOURCE_BARRIER_placeholder(source, D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
        command_list_->ResourceBarrier(1, &to_copy);
        D3D12_TEXTURE_COPY_LOCATION destination{};
        destination.pResource = readback.Get();
        destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        destination.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION src_loc{};
        src_loc.pResource = source;
        src_loc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src_loc.SubresourceIndex = 0;
        command_list_->CopyTextureRegion(&destination, 0, 0, 0, &src_loc, nullptr);
        auto to_present = CD3DX12_RESOURCE_BARRIER_placeholder(source, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_PRESENT);
        command_list_->ResourceBarrier(1, &to_present);
        hr = command_list_->Close();
        if (FAILED(hr)) {
            return Result<std::filesystem::path>::failure(
                device_error("SHOT-GPU-CLOSE", "Could not close screenshot command list", hr));
        }
        ID3D12CommandList* lists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, lists);
        wait_for_gpu();

        void* mapped = nullptr;
        D3D12_RANGE range{0, static_cast<SIZE_T>(readback_size)};
        hr = readback->Map(0, &range, &mapped);
        if (FAILED(hr) || !mapped) {
            return Result<std::filesystem::path>::failure(
                graphics_error("SHOT-GPU-MAP", "Could not map screenshot readback", hr));
        }

        const UINT width = static_cast<UINT>(desc.Width);
        const UINT height = desc.Height;
        std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
        const auto* bytes = static_cast<const std::uint8_t*>(mapped) + footprint.Offset;
        for (UINT y = 0; y < height; ++y) {
            const auto* row = bytes + static_cast<std::size_t>(y) * footprint.Footprint.RowPitch;
            std::uint8_t* dst = rgba.data() + static_cast<std::size_t>(y) * width * 4u;
            for (UINT x = 0; x < width; ++x) {
                const auto* px = row + static_cast<std::size_t>(x) * 4u;
                dst[x * 4u + 0] = px[0];
                dst[x * 4u + 1] = px[1];
                dst[x * 4u + 2] = px[2];
                dst[x * 4u + 3] = 255;
            }
        }
        readback->Unmap(0, nullptr);

        return write_rgba_png(project_root, filename_stem, width, height, rgba);
    }

private:
    EngineError device_error(std::string code, std::string message, HRESULT hr) const {
        EngineError error = graphics_error(std::move(code), std::move(message), hr);
        if (device_) {
            const HRESULT removed = device_->GetDeviceRemovedReason();
            if (FAILED(removed)) {
                error.category = ErrorCategory::DeviceRemoval;
                set_process_gpu_device_removal_hresult(removed);
                std::ostringstream value;
                value << "Device removal HRESULT 0x" << std::hex << static_cast<unsigned long>(removed);
                error.causes.push_back(value.str());
            }
        }
        return error;
    }

    void wait_for_gpu() {
        if (!queue_ || !fence_) return;
        const UINT64 target = ++fence_value_;
        if (SUCCEEDED(queue_->Signal(fence_.Get(), target)) && fence_->GetCompletedValue() < target) {
            fence_->SetEventOnCompletion(target, fence_event_);
            WaitForSingleObject(fence_event_, INFINITE);
        }
    }

    void shutdown() {
        wait_for_gpu();
        if (frame_cb_ && frame_cb_mapped_) {
            frame_cb_->Unmap(0, nullptr);
            frame_cb_mapped_ = nullptr;
        }
        if (water_frame_cb_ && water_frame_cb_mapped_) {
            water_frame_cb_->Unmap(0, nullptr);
            water_frame_cb_mapped_ = nullptr;
        }
        if (ssao_cb_ && ssao_cb_mapped_) {
            ssao_cb_->Unmap(0, nullptr);
            ssao_cb_mapped_ = nullptr;
        }
        if (composite_cb_ && composite_cb_mapped_) {
            composite_cb_->Unmap(0, nullptr);
            composite_cb_mapped_ = nullptr;
        }
        if(editor_initialized_){ImGui_ImplDX12_Shutdown();ImGui_ImplSDL3_Shutdown();ImGui::DestroyContext();editor_initialized_=false;}
        if (fence_event_) CloseHandle(fence_event_);
        fence_event_ = nullptr;
    }

    HWND hwnd_ = nullptr;
    UINT width_ = 1, height_ = 1, frame_index_ = 0, rtv_stride_ = 0, srv_stride_ = 0;
    UINT64 fence_value_ = 0;
    UINT64 timestamp_frequency_ = 0;
    double last_gpu_ms_ = 0.0;
    HANDLE fence_event_ = nullptr;
    std::string adapter_name_;
    ComPtr<IDXGIFactory6> factory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<IDXGISwapChain3> swap_chain_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    ComPtr<ID3D12DescriptorHeap> imgui_heap_;
    std::vector<ComPtr<ID3D12Resource>> world_forge_placeholder_textures_;
    std::vector<ComPtr<ID3D12Resource>> cartography_textures_;
    bool editor_initialized_=false;
    bool editor_requested_=false;
    bool debug_world_=false;
    ComPtr<ID3D12Resource> depth_,vertex_buffer_,terrain_vertex_buffer_,viewport_target_,game_viewport_target_;
    D3D12_CPU_DESCRIPTOR_HANDLE viewport_rtv_{};
    D3D12_GPU_DESCRIPTOR_HANDLE viewport_gpu_{};
    D3D12_CPU_DESCRIPTOR_HANDLE game_viewport_rtv_{};
    D3D12_GPU_DESCRIPTOR_HANDLE game_viewport_gpu_{};
    // SSAO v1 post-process resources: world always draws into lit_color_, then apply_ssao() reads depth_ + lit_color_
    // to produce ao_target_ (half-res) and composite the result into the editor viewport(s) or the swap chain.
    // water_scene_color_ is a pre-water copy of lit_color_ so refraction can sample without feedback.
    ComPtr<ID3D12Resource> lit_color_, water_scene_color_, ao_target_;
    D3D12_CPU_DESCRIPTOR_HANDLE lit_rtv_{}, ao_rtv_{};
    UINT ao_width_ = 1, ao_height_ = 1;
    ComPtr<ID3D12DescriptorHeap> post_srv_heap_;
    D3D12_GPU_DESCRIPTOR_HANDLE post_depth_gpu_{}, post_lit_gpu_{}, post_water_gpu_{};
    ComPtr<ID3D12RootSignature> ssao_root_signature_, composite_root_signature_;
    ComPtr<ID3D12PipelineState> ssao_pipeline_, composite_pipeline_;
    ComPtr<ID3D12Resource> ssao_cb_, composite_cb_;
    void* ssao_cb_mapped_ = nullptr;
    void* composite_cb_mapped_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertex_view_{};
    D3D12_VERTEX_BUFFER_VIEW terrain_vertex_view_{};
    UINT terrain_vertex_count_=0;
    ComPtr<ID3D12Resource> water_vertex_buffer_;
    D3D12_VERTEX_BUFFER_VIEW water_vertex_view_{};
    UINT water_vertex_count_=0;
    ComPtr<ID3D12RootSignature> water_root_signature_;
    ComPtr<ID3D12PipelineState> water_pipeline_;
    UINT sky_vertex_offset_=0;
    UINT sky_vertex_count_=0;
    std::map<std::string,std::pair<UINT,UINT>> mesh_ranges_;
    // Per-mesh base-color textures (TICKET-0191). Slot 0 is a 1x1 white fallback; the rest are aligned with
    // imported mesh order. Meshes without a texture stay absent from mesh_albedo_gpu_ and draw with vertex color.
    ComPtr<ID3D12DescriptorHeap> mesh_albedo_heap_;
    std::vector<ComPtr<ID3D12Resource>> mesh_albedo_textures_;
    std::map<std::string, D3D12_GPU_DESCRIPTOR_HANDLE> mesh_albedo_gpu_;
    D3D12_GPU_DESCRIPTOR_HANDLE mesh_white_gpu_{};
    std::array<ComPtr<ID3D12Resource>, frame_count> targets_;
    std::array<ComPtr<ID3D12CommandAllocator>, frame_count> allocators_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12PipelineState> sky_pipeline_;
    ComPtr<ID3D12RootSignature> foliage_root_signature_;
    ComPtr<ID3D12PipelineState> foliage_pipeline_;
    ComPtr<ID3D12Resource> frame_cb_;
    ComPtr<ID3D12Resource> water_frame_cb_;
    void* frame_cb_mapped_ = nullptr;
    void* water_frame_cb_mapped_ = nullptr;
    ComPtr<ID3D12Resource> foliage_instance_buffer_;
    ComPtr<ID3D12DescriptorHeap> foliage_srv_heap_;
    D3D12_CPU_DESCRIPTOR_HANDLE foliage_instance_srv_cpu_{};
    D3D12_GPU_DESCRIPTOR_HANDLE foliage_instance_srv_gpu_{};
    UINT foliage_instance_capacity_ = 0;
    UINT foliage_instance_float4_count_ = 0;
    std::vector<FoliageGpuDraw> foliage_draws_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12QueryHeap> timestamp_heap_;
    ComPtr<ID3D12Resource> timestamp_readback_;
    UINT last_presented_index_ = 0;
    bool has_presented_backbuffer_ = false;
};

constexpr const char* k_prefab_drag_payload = "ENGINE_PREFAB";
constexpr const char* k_asset_file_drag_payload = "ENGINE_ASSET_FILE";

bool editor_icon_button(const char* id_suffix, const char* icon, const char* tooltip, bool enabled = true) {
    if (!enabled) ImGui::BeginDisabled();
    const bool pressed = ImGui::SmallButton((std::string(icon) + "##" + id_suffix).c_str());
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tooltip);
    if (!enabled) ImGui::EndDisabled();
    return pressed && enabled;
}

struct ViewportFrame {
    ImVec2 image_min{};
    ImVec2 image_max{};
    float width = 1.0f;
    float height = 1.0f;
};

std::optional<WorldPosition> viewport_raycast(CollisionWorld* collision, const ViewportFrame& frame,
    const std::array<float, 16>& view, const std::array<float, 16>& projection, ImVec2 mouse) {
    if (!collision) return std::nullopt;
    using namespace DirectX;
    const auto projection_matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(projection.data()));
    const auto view_matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(view.data()));
    const auto identity = XMMatrixIdentity();
    const auto near_point = XMVector3Unproject(
        XMVectorSet(mouse.x - frame.image_min.x, mouse.y - frame.image_min.y, 0.0f, 1.0f), 0.0f, 0.0f, frame.width,
        frame.height, 0.0f, 1.0f, projection_matrix, view_matrix, identity);
    const auto far_point = XMVector3Unproject(
        XMVectorSet(mouse.x - frame.image_min.x, mouse.y - frame.image_min.y, 1.0f, 1.0f), 0.0f, 0.0f, frame.width,
        frame.height, 0.0f, 1.0f, projection_matrix, view_matrix, identity);
    const auto direction = XMVectorSubtract(far_point, near_point);
    auto hit = collision->ray_cast({XMVectorGetX(near_point), XMVectorGetY(near_point), XMVectorGetZ(near_point)},
                                   {XMVectorGetX(direction), XMVectorGetY(direction), XMVectorGetZ(direction)});
    if (hit && hit.value()) return hit.value()->position;
    return std::nullopt;
}

bool project_world_to_screen(const std::array<float, 16>& view_projection, const ViewportFrame& frame, float x, float y,
    float z, float& screen_x, float& screen_y, float& depth) {
    using namespace DirectX;
    const auto matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(view_projection.data()));
    const auto ndc = XMVector3TransformCoord(XMVectorSet(x, y, z, 1.0f), matrix);
    depth = XMVectorGetZ(ndc);
    if (depth < 0.0f || depth > 1.0f) return false;
    screen_x = frame.image_min.x + (XMVectorGetX(ndc) + 1.0f) * 0.5f * frame.width;
    screen_y = frame.image_min.y + (1.0f - XMVectorGetY(ndc)) * 0.5f * frame.height;
    return true;
}

struct PlacementScreenBounds {
    bool visible = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float center_depth = 1.0f;
    std::array<ImVec2, 8> corners{};
    std::array<bool, 8> corner_visible{};
};

constexpr MeshBounds k_proxy_bounds{-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};

struct PlacementBoundsSource {
    MeshBounds local_bounds = k_proxy_bounds;
    float y_translation_offset = 0.0f;
};

PlacementScreenBounds compute_placement_screen_bounds(const TransformComponent& transform, const MeshBounds& local_bounds,
    float y_translation_offset, const std::array<float, 16>& view_projection, const ViewportFrame& frame) {
    PlacementScreenBounds bounds;
    using namespace DirectX;
    const auto matrix = XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(view_projection.data()));
    static const float corner_offsets[8][3] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1},
    };
    const auto model =
        XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
        XMMatrixRotationQuaternion(XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data()))) *
        XMMatrixTranslation(transform.position[0], transform.position[1] + y_translation_offset, transform.position[2]);
    bounds.min_x = frame.image_max.x;
    bounds.min_y = frame.image_max.y;
    bounds.max_x = frame.image_min.x;
    bounds.max_y = frame.image_min.y;
    for (int i = 0; i < 8; ++i) {
        const float x = corner_offsets[i][0] ? local_bounds.max_x : local_bounds.min_x;
        const float y = corner_offsets[i][1] ? local_bounds.max_y : local_bounds.min_y;
        const float z = corner_offsets[i][2] ? local_bounds.max_z : local_bounds.min_z;
        const auto world = XMVector3TransformCoord(XMVectorSet(x, y, z, 1.0f), model);
        const auto ndc = XMVector3TransformCoord(world, matrix);
        const float depth = XMVectorGetZ(ndc);
        if (depth < 0.0f || depth > 1.0f) {
            bounds.corner_visible[static_cast<std::size_t>(i)] = false;
            continue;
        }
        bounds.visible = true;
        bounds.corner_visible[static_cast<std::size_t>(i)] = true;
        const float sx = frame.image_min.x + (XMVectorGetX(ndc) + 1.0f) * 0.5f * frame.width;
        const float sy = frame.image_min.y + (1.0f - XMVectorGetY(ndc)) * 0.5f * frame.height;
        bounds.corners[static_cast<std::size_t>(i)] = {sx, sy};
        bounds.min_x = std::min(bounds.min_x, sx);
        bounds.max_x = std::max(bounds.max_x, sx);
        bounds.min_y = std::min(bounds.min_y, sy);
        bounds.max_y = std::max(bounds.max_y, sy);
        if (i == 0) bounds.center_depth = depth;
    }
    return bounds;
}

void draw_placement_bounds_overlay(ImDrawList* draw_list, const PlacementScreenBounds& bounds, ImU32 color,
    ImU32 fill_color, float thickness) {
    if (!bounds.visible) return;
    static constexpr int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6}, {6, 7}, {7, 4},
                                           {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    draw_list->AddRectFilled({bounds.min_x, bounds.min_y}, {bounds.max_x, bounds.max_y}, fill_color);
    draw_list->AddRect({bounds.min_x, bounds.min_y}, {bounds.max_x, bounds.max_y}, color, 0.0f, 0, thickness);
    for (const auto& edge : edges) {
        if (bounds.corner_visible[static_cast<std::size_t>(edge[0])] &&
            bounds.corner_visible[static_cast<std::size_t>(edge[1])]) {
            draw_list->AddLine(bounds.corners[static_cast<std::size_t>(edge[0])],
                               bounds.corners[static_cast<std::size_t>(edge[1])], color, thickness);
        }
    }
}

struct EditorState {
    enum class TestSessionState : std::uint8_t { Inactive, Running, Paused };
    enum class TestSessionCommand : std::uint8_t { None, Start, Pause, Resume, End };
    enum class ViewportTab : std::uint8_t { Scene, Sculpt, Game, UI, WorldForge, DesignDocs };

    struct DesignDocEntry {
        std::string section;       // features | story | art | design
        std::string relative_path; // context/...
        std::string title;
        std::string status; // from "Status:" line, lowercased token
    };

    struct DomOpenQuestion {
        std::string id;
        std::string priority; // P0 | P1 | P2
        std::string question;
        std::string context; // why / notes / draft columns combined for the form
        std::string answer;
    };

    Scene scene;
    AssetRegistry assets;
    CommandHistory history;
    std::optional<EntityId> selected;
    std::optional<EntityId> hovered;
    std::filesystem::path world_path;
    std::string status = "Editor ready";
    std::array<float, 16> gizmo_matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::optional<EntityId> gizmo_entity;
    bool gizmo_was_using = false;
    ImGuizmo::OPERATION gizmo_operation = ImGuizmo::TRANSLATE;
    std::optional<TransformComponent> gizmo_preview;
    std::optional<WorldPosition> placement_cursor;
    std::optional<TransformComponent> drop_preview;
    std::optional<std::string> drop_preview_prefab;
    bool viewport_hovered = false;
    bool viewport_focused = false;
    bool left_press_active = false;
    bool terrain_drag_active = false;
    ImVec2 left_press_pos{};
    std::optional<EntityId> left_press_pick;
    char rename_buffer[128]{};
    std::optional<EntityId> rename_target;
    std::map<std::string, PrefabAsset> prefab_catalog;
    std::map<std::string, MeshBounds> mesh_bounds;
    std::map<std::string, MeshBounds> prefab_bounds;
    std::optional<std::string> prefab_edit_path;
    std::optional<std::size_t> prefab_edit_part;
    std::array<float, 16> prefab_part_gizmo_matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    bool prefab_part_gizmo_was_using = false;
    std::filesystem::path project_root;
    bool show_collision_debug = false;
    /// Editor-only Scene/Sculpt overlay for World Forge region/POI anchors (TICKET-0190).
    bool show_world_forge_map_markers = true;
    /// Set by UI; applied once in the editor frame after draw_editor.
    bool request_focus_world_forge_marker = false;
    std::vector<WorldPosition> recent_contact_points;
    std::vector<InteractionEvent> recent_interaction_events;
    std::vector<CombatContactEvent> recent_combat_events;
    TestSessionState test_session = TestSessionState::Inactive;
    TestSessionCommand test_session_command = TestSessionCommand::None;
    ViewportTab active_viewport_tab = ViewportTab::Scene;
    PlaySessionSettings play_session;
    CharacterAsset character_asset;
    CameraAsset camera_asset;
    MaterialAsset material_asset;
    std::optional<std::string> inspector_asset_path;
    std::optional<std::string> inspector_component_edit_id;
    char inspector_script_kind_buf[64]{};
    char inspector_script_binding_buf[128]{};
    std::optional<std::string> prefab_component_edit_id;
    char prefab_script_kind_buf[64]{};
    char prefab_script_binding_buf[128]{};
    bool play_session_dirty = false;
    bool character_asset_dirty = false;
    bool camera_asset_dirty = false;
    bool material_asset_dirty = false;
    enum class SculptTool : std::uint8_t { Height, Flatten, Paint, Foliage, Water };

    SculptTool sculpt_tool = SculptTool::Height;
    TerrainEditStore terrain_edits;
    TerrainEditHistory terrain_history;
    bool terrain_edits_dirty = false;
    WaterStore water_store;
    WaterEditHistory water_history;
    bool water_dirty = false;
    /// Bumped when sculpt heights change (World Forge Map Canvas underlay).
    std::uint64_t terrain_height_revision = 1;
    TerrainPaintStore terrain_paint;
    TerrainPaintHistory terrain_paint_history;
    bool terrain_paint_dirty = false;
    FoliageLayerPalette foliage_layers;
    FoliageDensityStore foliage_density;
    FoliageDensityHistory foliage_density_history;
    bool foliage_density_dirty = false;
    std::uint8_t foliage_brush_layer = 0;
    float foliage_brush_strength = 0.25f;
    int foliage_density_preset = 1;
    enum class FoliageBrushMode : std::uint8_t { Paint, Erase, Mixed };

    FoliageBrushMode foliage_brush_mode = FoliageBrushMode::Paint;
    std::string terrain_paint_brush_material;
    float terrain_brush_radius = 4.0f;
    float terrain_brush_strength = 0.12f;
    float terrain_flatten_target = 0.0f;
    bool terrain_flatten_target_valid = false;
    bool terrain_brush_active = false;
    std::map<CellCoord, std::vector<float>> terrain_brush_before;
    std::set<CellCoord> terrain_brush_touched;
    std::map<CellCoord, std::vector<std::uint16_t>> terrain_paint_brush_before;
    std::set<CellCoord> terrain_paint_brush_touched;
    std::map<CellCoord, FoliageCellSnapshot> foliage_brush_before;
    std::set<CellCoord> foliage_brush_touched;
    std::map<CellCoord, WaterCellSnapshot> water_brush_before;
    std::set<CellCoord> water_brush_touched;
    std::optional<EntityId> test_player_spawn_entity;
    std::optional<EntityId> inspector_player_spawn_entity;
    bool show_movement_console = false;
    std::deque<std::string> console_lines;
    std::string asset_browser_folder;
    std::optional<std::string> asset_browser_selected_folder;
    char asset_folder_name_buffer[128]{};
    bool asset_browser_new_folder_popup = false;
    bool asset_browser_rename_folder_popup = false;
    bool asset_browser_new_material_popup = false;
    bool new_material_assign_paint_brush = false;
    std::string new_material_folder;
    char asset_material_name_buffer[128]{};
    std::map<std::string, MaterialAsset> material_cache;
    std::string terrain_material_path = "assets/materials/terrain.material.json";
    bool prefab_meshes_dirty = false;
    std::set<std::string> pending_mesh_reloads;
    bool scene_dirty = false;
    std::unique_ptr<LuaRuntime> lua_runtime;
    std::unique_ptr<QuestRuntime> quest_runtime;
    std::unique_ptr<WorldForgeQuestsAsset> quest_asset;
    std::unique_ptr<StandingRuntime> standing_runtime;
    std::unique_ptr<WorldForgeFactionsAsset> standing_factions_asset;
    std::unique_ptr<WorldForgeRelationshipsAsset> standing_relationships_asset;
    std::unique_ptr<UiCanvasStack> ui_canvas_stack;
    UiCanvasEditorSession ui_canvas_editor;
    WorldForgeEditorSession world_forge_editor;
    std::vector<DesignDocEntry> design_docs;
    int design_docs_selected = -1;
    int design_docs_status_filter = 0; // 0=All, 1=active, 2=planned, 3=complete, 4=other
    std::string design_docs_body;
    std::string design_docs_loaded_relative;
    std::string design_docs_error;
    std::uint32_t design_docs_scan_counter = 0;
    bool design_docs_dom_form_mode = true; // Form vs markdown preview for Dom questionnaire
    bool design_docs_markdown_selectable = true; // Selectable source (copy) vs rendered markdown
    std::string design_docs_copy_status;
    int design_docs_dom_priority_filter = 0; // 0=All, 1=P0, 2=P1, 3=P2
    std::vector<DomOpenQuestion> design_docs_dom_questions;
    std::string design_docs_dom_session_notes;
    std::string design_docs_dom_form_status;
    std::string design_docs_dom_form_source_path;
    ScriptFileMonitor script_monitor;
    std::uint32_t script_reload_counter = 0;
    std::uint32_t bridge_poll_counter = 0;
    // The editor is automation-first: start the project-scoped MCP bridge with each editor session.
    // Authors can still disable it from Diagnostics when they do not want live tooling connected.
    bool live_automation_enabled = true;
    char project_sync_commit_message[256]{};
    std::string project_sync_branch;
    std::string project_sync_summary = "Click Status to refresh";
    std::string project_sync_detail;
    bool project_sync_offer_wf_reload = false;
    bool project_sync_block_scene_reload = false;
    std::size_t lua_dispatched_interactions = 0;
    std::size_t lua_dispatched_combat = 0;
    std::optional<ImVec2> game_viewport_min;
    std::optional<ImVec2> game_viewport_max;
    bool game_viewport_hovered = false;

    [[nodiscard]] bool test_session_active() const { return test_session != TestSessionState::Inactive; }
    [[nodiscard]] bool test_session_running() const { return test_session == TestSessionState::Running; }
    bool force_select_viewport_tab = false;
    bool lock_viewport_tab = false;

    /// Queued MCP UI input (applied after ImGui NewFrame).
    struct InputEvent {
        enum class Kind : std::uint8_t { Move, Button, Wheel, Key, Wait };
        Kind kind = Kind::Wait;
        float x = 0.0f;
        float y = 0.0f;
        int button = 0; // 0=left,1=right,2=middle
        bool down = false;
        float wheel = 0.0f;
        int key = 0; // ImGuiKey or SDL scancode-ish; we map a small set
        int wait_frames = 1;
    };
    std::deque<InputEvent> mcp_input_queue;
    float mcp_cursor_x = -1.0f;
    float mcp_cursor_y = -1.0f;
    bool mcp_draw_cursor = false;
    EditorUiHotspotRegistry ui_hotspots;

    [[nodiscard]] bool game_viewport_active() const { return active_viewport_tab == ViewportTab::Game; }
    [[nodiscard]] bool sculpt_viewport_active() const { return active_viewport_tab == ViewportTab::Sculpt; }
    [[nodiscard]] bool ui_viewport_active() const { return active_viewport_tab == ViewportTab::UI; }
    [[nodiscard]] bool world_forge_viewport_active() const {
        return active_viewport_tab == ViewportTab::WorldForge;
    }
    [[nodiscard]] bool design_docs_viewport_active() const {
        return active_viewport_tab == ViewportTab::DesignDocs;
    }
};

void commit_active_terrain_stroke(EditorState& state);

void enqueue_mcp_click(EditorState& state, float x, float y, int button, int hold_frames = 1) {
    EditorState::InputEvent move;
    move.kind = EditorState::InputEvent::Kind::Move;
    move.x = x;
    move.y = y;
    state.mcp_input_queue.push_back(move);
    EditorState::InputEvent down;
    down.kind = EditorState::InputEvent::Kind::Button;
    down.button = button;
    down.down = true;
    state.mcp_input_queue.push_back(down);
    EditorState::InputEvent wait;
    wait.kind = EditorState::InputEvent::Kind::Wait;
    wait.wait_frames = (std::max)(1, hold_frames);
    state.mcp_input_queue.push_back(wait);
    EditorState::InputEvent up;
    up.kind = EditorState::InputEvent::Kind::Button;
    up.button = button;
    up.down = false;
    state.mcp_input_queue.push_back(up);
}

bool resolve_mcp_client_xy(SDL_Window* window, const nlohmann::json& params, float& out_x, float& out_y,
    std::string* error, const EditorUiHotspotRegistry* hotspots = nullptr) {
    if (params.contains("targetId") && params["targetId"].is_string()) {
        const auto target_id = params["targetId"].get<std::string>();
        if (!hotspots) {
            if (error) *error = "targetId requires live hotspot registry";
            return false;
        }
        const auto* spot = hotspots->find_exact(target_id);
        if (!spot) {
            if (error) *error = "Unknown targetId: " + target_id;
            return false;
        }
        out_x = spot->cx;
        out_y = spot->cy;
        return true;
    }
    int w = 1, h = 1;
    SDL_GetWindowSize(window, &w, &h);
    if (params.contains("nx") || params.contains("ny")) {
        const float nx = params.value("nx", 0.5f);
        const float ny = params.value("ny", 0.5f);
        out_x = nx * static_cast<float>(w);
        out_y = ny * static_cast<float>(h);
        return true;
    }
    if (params.contains("x") || params.contains("y")) {
        out_x = params.value("x", 0.0f);
        out_y = params.value("y", 0.0f);
        return true;
    }
    if (error) *error = "Provide x/y client pixels or nx/ny normalized [0,1]";
    return false;
}

void drain_mcp_input_queue(EditorState& state, SDL_Window* window) {
    if (state.mcp_input_queue.empty()) {
        state.mcp_draw_cursor = state.mcp_cursor_x >= 0.0f && state.mcp_cursor_y >= 0.0f;
        return;
    }
    auto& io = ImGui::GetIO();
    while (!state.mcp_input_queue.empty()) {
        auto& ev = state.mcp_input_queue.front();
        if (ev.kind == EditorState::InputEvent::Kind::Wait) {
            if (ev.wait_frames > 1) {
                --ev.wait_frames;
                break;
            }
            state.mcp_input_queue.pop_front();
            continue;
        }
        if (ev.kind == EditorState::InputEvent::Kind::Move) {
            state.mcp_cursor_x = ev.x;
            state.mcp_cursor_y = ev.y;
            state.mcp_draw_cursor = true;
            SDL_WarpMouseInWindow(window, static_cast<float>(ev.x), static_cast<float>(ev.y));
            io.AddMousePosEvent(ev.x, ev.y);
        } else if (ev.kind == EditorState::InputEvent::Kind::Button) {
            if (state.mcp_cursor_x >= 0.0f) io.AddMousePosEvent(state.mcp_cursor_x, state.mcp_cursor_y);
            io.AddMouseButtonEvent(ev.button, ev.down);
        } else if (ev.kind == EditorState::InputEvent::Kind::Wheel) {
            io.AddMouseWheelEvent(0.0f, ev.wheel);
        } else if (ev.kind == EditorState::InputEvent::Kind::Key) {
            // Small named key set for UI navigation.
            ImGuiKey key = ImGuiKey_None;
            switch (ev.key) {
            case 27: key = ImGuiKey_Escape; break;
            case 13: key = ImGuiKey_Enter; break;
            case 9: key = ImGuiKey_Tab; break;
            case 32: key = ImGuiKey_Space; break;
            case 37: key = ImGuiKey_LeftArrow; break;
            case 38: key = ImGuiKey_UpArrow; break;
            case 39: key = ImGuiKey_RightArrow; break;
            case 40: key = ImGuiKey_DownArrow; break;
            default: break;
            }
            if (key != ImGuiKey_None) io.AddKeyEvent(key, ev.down);
        }
        state.mcp_input_queue.pop_front();
        // One non-wait event per frame keeps ImGui click/hover sequencing stable.
        break;
    }
}

void draw_mcp_cursor_overlay(const EditorState& state) {
    if (!state.mcp_draw_cursor || state.mcp_cursor_x < 0.0f || state.mcp_cursor_y < 0.0f) return;
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 p{state.mcp_cursor_x, state.mcp_cursor_y};
    draw->AddCircleFilled(p, 5.0f, IM_COL32(255, 200, 60, 220), 12);
    draw->AddCircle(p, 8.0f, IM_COL32(20, 16, 8, 255), 12, 2.0f);
    draw->AddLine(ImVec2(p.x - 14.0f, p.y), ImVec2(p.x + 14.0f, p.y), IM_COL32(255, 220, 100, 180), 1.5f);
    draw->AddLine(ImVec2(p.x, p.y - 14.0f), ImVec2(p.x, p.y + 14.0f), IM_COL32(255, 220, 100, 180), 1.5f);
}

void mark_scene_dirty(EditorState& state) { state.scene_dirty = true; }

void apply_project_sync_response(EditorState& state, const EditorBridgeResponse& response) {
    state.project_sync_summary = response.summary;
    if (const auto it = response.metadata.find("branch"); it != response.metadata.end())
        state.project_sync_branch = it->second;
    std::ostringstream detail;
    if (const auto it = response.metadata.find("dirtyPaths"); it != response.metadata.end() && !it->second.empty())
        detail << "Dirty:\n" << it->second << '\n';
    if (const auto it = response.metadata.find("conflictedPaths"); it != response.metadata.end() && !it->second.empty())
        detail << "Conflicts:\n" << it->second << '\n';
    if (const auto it = response.metadata.find("changedPaths"); it != response.metadata.end() && !it->second.empty())
        detail << "Changed:\n" << it->second << '\n';
    if (!response.diagnostics.empty()) detail << response.diagnostics.front().message;
    state.project_sync_detail = detail.str();
    state.status = response.summary;

    const auto meta_flag = [&](const char* key) {
        const auto it = response.metadata.find(key);
        return it != response.metadata.end() && it->second == "true";
    };
    state.project_sync_offer_wf_reload = false;
    state.project_sync_block_scene_reload = false;
    if (response.exit_code == ExitCode::Success && meta_flag("requiresWorldForgeReload")) {
        if (state.scene_dirty || state.terrain_edits_dirty || state.terrain_paint_dirty ||
            state.foliage_density_dirty) {
            if (meta_flag("changedScene")) {
                state.project_sync_block_scene_reload = true;
                state.project_sync_summary +=
                    " — save or discard Scene/Sculpt before applying pulled scene changes";
            }
        }
        if (state.world_forge_editor.dirty) {
            state.project_sync_summary += " — World Forge has unsaved edits; Save or discard before Reload";
        } else {
            state.project_sync_offer_wf_reload = true;
        }
    }
}

void run_project_sync_action(EditorState& state, const std::string& action) {
    nlohmann::json params = {{"action", action}};
    if (action == "commit") {
        params["message"] = state.project_sync_commit_message;
    }
    apply_project_sync_response(state, apply_project_git_operation(state.project_root, params));
}

void draw_project_sync_panel(EditorState& state) {
    ImGui::Separator();
    ImGui::TextUnformatted("Project Sync (git)");
    if (!state.project_sync_branch.empty())
        ImGui::Text("Branch: %s", state.project_sync_branch.c_str());
    else
        ImGui::TextDisabled("Branch: (unknown — Status to refresh)");
    ImGui::TextWrapped("%s", state.project_sync_summary.c_str());
    if (ImGui::Button("Status##ProjectSync")) run_project_sync_action(state, "status");
    ImGui::SameLine();
    if (ImGui::Button("Fetch##ProjectSync")) run_project_sync_action(state, "fetch");
    ImGui::SameLine();
    if (ImGui::Button("Pull##ProjectSync")) run_project_sync_action(state, "pull");
    ImGui::SameLine();
    if (ImGui::Button("Push##ProjectSync")) run_project_sync_action(state, "push");
    ImGui::InputTextWithHint("##ProjectSyncCommitMessage", "Commit message", state.project_sync_commit_message,
        sizeof(state.project_sync_commit_message));
    ImGui::SameLine();
    if (ImGui::Button("Commit##ProjectSync")) run_project_sync_action(state, "commit");
    if (state.project_sync_offer_wf_reload) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "Pulled World Forge changes are on disk.");
        if (ImGui::Button("Reload World Forge##ProjectSyncReload")) {
            if (state.world_forge_editor.dirty) {
                state.status = "Save or discard World Forge edits before reload";
            } else {
                const auto reloaded = state.world_forge_editor.reload(state.project_root);
                if (!reloaded) {
                    state.status = "World Forge reload failed: " + reloaded.error().message;
                    state.project_sync_detail = reloaded.error().message;
                } else {
                    state.status = "World Forge reloaded after pull";
                    state.project_sync_offer_wf_reload = false;
                    state.project_sync_summary = "World Forge reloaded after pull";
                }
            }
        }
    }
    if (state.project_sync_block_scene_reload) {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f),
            "Pulled scene/terrain files while the editor has unsaved Scene/Sculpt work. Save or discard, then restart or reload carefully.");
    }
    if (!state.project_sync_detail.empty()) {
        ImGui::BeginChild("ProjectSyncDetail", ImVec2(0.0f, 96.0f), true);
        ImGui::TextUnformatted(state.project_sync_detail.c_str());
        ImGui::EndChild();
    }
    ImGui::TextDisabled("Uses system git + OS credentials. Save World Forge before Commit.");
}

void open_script_binding(EditorState& state, const std::string& kind, const std::string& binding_id) {
    const auto relative = resolve_script_binding_path(state.project_root, kind, binding_id);
    if (!relative) {
        state.status = relative.error().message;
        Logger::instance().write(relative.error());
        return;
    }
    const std::string& relative_path = relative.value();
    const auto absolute = (state.project_root / relative_path).lexically_normal();
    if (!std::filesystem::exists(absolute)) {
        state.status = "Script file missing: " + normalize_asset_path(relative_path);
        Logger::instance().write(EngineError{"SCRIPT-FILE-MISSING", Severity::Error, ErrorCategory::Io, "editor",
            state.status, ENGINE_SOURCE_CONTEXT, {}, "Create the Lua file or fix the bindings path.",
            make_correlation_id()});
        return;
    }
    const HINSTANCE opened = ShellExecuteW(nullptr, L"open", absolute.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(opened) <= 32) {
        state.status = "Failed to open script: " + normalize_asset_path(relative_path);
        return;
    }
    state.status = "Opened " + normalize_asset_path(relative_path);
}

bool draw_collider_property_fields(PrefabCollisionVolume& volume) {
    bool commit = false;
    int shape_index = volume.shape == PrefabCollisionShape::Box       ? 0
                      : volume.shape == PrefabCollisionShape::Sphere ? 1
                                                                     : 2;
    if (ImGui::Combo("Shape", &shape_index, "box\0sphere\0capsule\0")) {
        volume.shape = shape_index == 0   ? PrefabCollisionShape::Box
                       : shape_index == 1 ? PrefabCollisionShape::Sphere
                                          : PrefabCollisionShape::Capsule;
        commit = true;
    }
    if (volume.shape == PrefabCollisionShape::Box) {
        float half_extent[3] = {volume.half_extent.x, volume.half_extent.y, volume.half_extent.z};
        ImGui::InputFloat3("Half Extent", half_extent);
        volume.half_extent = {half_extent[0], half_extent[1], half_extent[2]};
        if (ImGui::IsItemDeactivatedAfterEdit()) commit = true;
    } else if (volume.shape == PrefabCollisionShape::Capsule) {
        ImGui::InputFloat("Radius", &volume.radius);
        if (ImGui::IsItemDeactivatedAfterEdit()) commit = true;
        ImGui::InputFloat("Half Height", &volume.capsule_half_height);
        if (ImGui::IsItemDeactivatedAfterEdit()) commit = true;
    } else {
        ImGui::InputFloat("Radius", &volume.radius);
        if (ImGui::IsItemDeactivatedAfterEdit()) commit = true;
    }
    float offset[3] = {volume.transform.position[0], volume.transform.position[1], volume.transform.position[2]};
    ImGui::InputFloat3("Offset", offset);
    volume.transform.position = {offset[0], offset[1], offset[2]};
    if (ImGui::IsItemDeactivatedAfterEdit()) commit = true;
    if (ImGui::Checkbox("Trigger", &volume.trigger)) commit = true;
    return commit;
}

bool draw_script_binding_property_fields(ScriptBindingComponentData& script, char* kind_buf, std::size_t kind_buf_size,
    char* binding_buf, std::size_t binding_buf_size) {
    bool commit = false;
    int kind_index = 0;
    const std::string kind_key = script.kind;
    std::string kind_lower = kind_key;
    std::transform(kind_lower.begin(), kind_lower.end(), kind_lower.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (kind_lower == "combathit") kind_index = 1;
    else if (kind_lower == "combathurt") kind_index = 2;
    else if (kind_lower == "handler") kind_index = 3;
    if (ImGui::Combo("Kind", &kind_index, "interaction\0combatHit\0combatHurt\0handler\0")) {
        static constexpr const char* k_kinds[] = {"interaction", "combatHit", "combatHurt", "handler"};
        script.kind = k_kinds[kind_index];
        std::snprintf(kind_buf, kind_buf_size, "%s", script.kind.c_str());
        commit = true;
    }
    ImGui::InputText("Binding Id", binding_buf, binding_buf_size);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        script.binding_id = binding_buf;
        commit = true;
    }
    return commit;
}

void commit_entity_component_edit(EditorState& state, AuthoredComponentEntry entry) {
    const auto result =
        state.history.execute(state.scene, std::make_unique<SetEntityComponentCommand>(*state.selected, std::move(entry)));
    state.status = result ? "Component updated" : result.error().message;
    if (!result) Logger::instance().write(result.error());
    else mark_scene_dirty(state);
}

EditorSessionContext make_editor_session_context(EditorState& state, bool editor_running) {
    EditorSessionContext context;
    context.scene = &state.scene;
    context.history = &state.history;
    context.world_path = state.world_path;
    context.project_root = state.project_root;
    context.selected = &state.selected;
    context.assets = &state.assets;
    context.prefab_catalog = &state.prefab_catalog;
    context.scene_dirty = &state.scene_dirty;
    context.prefab_meshes_dirty = &state.prefab_meshes_dirty;
    context.pending_mesh_reloads = &state.pending_mesh_reloads;
    context.terrain_edits = &state.terrain_edits;
    context.terrain_history = &state.terrain_history;
    context.terrain_edits_dirty = &state.terrain_edits_dirty;
    context.terrain_paint = &state.terrain_paint;
    context.terrain_paint_history = &state.terrain_paint_history;
    context.terrain_paint_dirty = &state.terrain_paint_dirty;
    context.foliage_density = &state.foliage_density;
    context.foliage_density_history = &state.foliage_density_history;
    context.foliage_density_dirty = &state.foliage_density_dirty;
    context.foliage_layers = &state.foliage_layers;
    context.water_store = &state.water_store;
    context.water_history = &state.water_history;
    context.water_dirty = &state.water_dirty;
    context.editor_running = editor_running;
    context.live_automation_enabled = state.live_automation_enabled;
    context.test_session_active = state.test_session_active();
    context.test_session_state = state.test_session_active()
                                     ? (state.test_session_running() ? "running" : "paused")
                                     : "inactive";
    context.lua_runtime = state.lua_runtime.get();
    context.quest_runtime = state.quest_runtime.get();
    context.standing_runtime = state.standing_runtime.get();
    context.hud_runtime = state.ui_canvas_stack ? &state.ui_canvas_stack->hud() : nullptr;
    context.ui_canvas_stack = state.ui_canvas_stack.get();
    if (state.selected) context.selected_entity_id = state.selected->str();
    return context;
}

void dispatch_pending_script_events(EditorState& state) {
    if (!state.lua_runtime) return;
    for (std::size_t index = state.lua_dispatched_interactions; index < state.recent_interaction_events.size(); ++index)
        state.lua_runtime->dispatch_interaction(state.recent_interaction_events[index]);
    for (std::size_t index = state.lua_dispatched_combat; index < state.recent_combat_events.size(); ++index)
        state.lua_runtime->dispatch_combat_hit(state.recent_combat_events[index]);
    state.lua_dispatched_interactions = state.recent_interaction_events.size();
    state.lua_dispatched_combat = state.recent_combat_events.size();
}

void reload_changed_lua_scripts(EditorState& state) {
    if (!state.lua_runtime || state.project_root.empty()) return;
    constexpr std::uint32_t k_reload_interval_frames = 30;
    if (++state.script_reload_counter < k_reload_interval_frames) return;
    state.script_reload_counter = 0;

    const auto scripts_root = state.project_root / "assets" / "scripts";
    for (const auto& changed : state.script_monitor.poll_changes(scripts_root)) {
        if (!std::filesystem::exists(changed)) continue;
        if (const auto reloaded = state.lua_runtime->reload_script(changed); !reloaded)
            Logger::instance().write(reloaded.error());
        else
            state.status = "Reloaded Lua script: " + changed.filename().string();
    }
}

MaterialAsset* editor_material(EditorState& state, const std::string& path);
TerrainPaintMaterialLookup editor_paint_material_lookup(EditorState& state);
void apply_active_terrain_material(EditorState& state, MaterialAsset* terrain_material,
    StreamedTerrainField* streamed_terrain, CollisionWorld* collision);
void reload_loaded_terrain_cells(EditorState& state, StreamedTerrainField* streamed_terrain, CollisionWorld* collision,
    MaterialAsset* terrain_material, bool height_changed = true);
void reload_loaded_water_cells(EditorState& state, StreamedWaterField* streamed_water);
void commit_terrain_brush_stroke(EditorState& state);
void commit_terrain_paint_stroke(EditorState& state);
void commit_foliage_density_stroke(EditorState& state);
void commit_active_terrain_stroke(EditorState& state);

Result<std::string> create_material_asset(EditorState& state, const std::string& raw_name,
    std::optional<std::string_view> folder_override = std::nullopt);

void request_new_material_asset(EditorState& state, bool assign_paint_brush, const std::string& folder = {}) {
    state.asset_material_name_buffer[0] = '\0';
    state.asset_browser_new_material_popup = true;
    state.new_material_assign_paint_brush = assign_paint_brush;
    state.new_material_folder = folder;
}

void draw_new_material_asset_popup(EditorState& state) {
    if (state.asset_browser_new_material_popup) ImGui::OpenPopup("New Material Asset");
    if (!ImGui::BeginPopupModal("New Material Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    ImGui::InputText("Name", state.asset_material_name_buffer, sizeof(state.asset_material_name_buffer));
    const std::string target_folder =
        state.new_material_folder.empty()
            ? (state.asset_browser_folder.empty() ? "materials" : state.asset_browser_folder)
            : state.new_material_folder;
    ImGui::TextDisabled("Saved as <name>.material.json in assets/%s/.", target_folder.c_str());
    if (state.sculpt_viewport_active())
        ImGui::Checkbox("Use as paint brush", &state.new_material_assign_paint_brush);
    if (ImGui::Button("Create")) {
        const std::optional<std::string_view> folder_override =
            state.new_material_folder.empty() ? std::nullopt : std::optional<std::string_view>{state.new_material_folder};
        if (const auto created = create_material_asset(state, state.asset_material_name_buffer, folder_override);
            created) {
            state.inspector_asset_path = created.value();
            state.material_asset = MaterialAsset::make_default();
            state.material_asset_dirty = false;
            if (state.new_material_assign_paint_brush) {
                state.terrain_paint_brush_material = created.value();
                (void)state.terrain_paint.ensure_material_index(state.terrain_paint_brush_material);
            }
            state.status = state.new_material_assign_paint_brush
                               ? "Created material and set paint brush: " + created.value()
                               : "Created material " + created.value();
        } else {
            state.status = created.error().message;
            Logger::instance().write(created.error());
        }
        state.asset_browser_new_material_popup = false;
        state.new_material_assign_paint_brush = false;
        state.new_material_folder.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        state.asset_browser_new_material_popup = false;
        state.new_material_assign_paint_brush = false;
        state.new_material_folder.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void attach_process_console_if_needed() {
    if (GetConsoleWindow() != nullptr) return;
    if (!AllocConsole()) return;
    FILE* stream = nullptr;
    (void)freopen_s(&stream, "CONOUT$", "w", stdout);
    (void)freopen_s(&stream, "CONOUT$", "w", stderr);
    (void)freopen_s(&stream, "CONIN$", "r", stdin);
    std::ios::sync_with_stdio();
}

void append_editor_console(EditorState& state, const std::string& line, bool mirror_to_terminal) {
    state.console_lines.push_back(line);
    constexpr std::size_t k_max_console_lines = 256;
    if (state.console_lines.size() > k_max_console_lines)
        state.console_lines.pop_front();
    if (mirror_to_terminal) {
        std::cerr << line << '\n';
        Logger::instance().write(Severity::Info, "movement", line);
    }
}

void record_movement_debug(EditorState& state, const CharacterController& character, const LocalPosition& wish,
    float frame_delta_seconds, const WorldPosition& position_before) {
    if (!state.show_movement_console) return;
    const float wish_magnitude =
        std::sqrt(wish.x * wish.x + wish.z * wish.z);
    if (wish_magnitude <= 0.0f) return;

    const auto position_after = character.position();
    const double dx = position_after.x - position_before.x;
    const double dz = position_after.z - position_before.z;
    const float measured_speed =
        frame_delta_seconds > 0.0f ? static_cast<float>(std::sqrt(dx * dx + dz * dz) / frame_delta_seconds) : 0.0f;
    const auto velocity = character.linear_velocity();
    const float horizontal_velocity =
        std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);

    std::ostringstream line;
    line.setf(std::ios::fixed);
    line.precision(2);
    line << "wish=(" << wish.x << ',' << wish.z << ") mag=" << wish_magnitude << " max=" << character.config().max_speed
         << " m/s dt=" << std::setprecision(4) << frame_delta_seconds << std::setprecision(2) << " vel=(" << velocity[0]
         << ',' << velocity[2] << ") hvel=" << horizontal_velocity << " meas=" << measured_speed
         << " ground=" << (character.on_ground() ? 1 : 0);
    append_editor_console(state, line.str(), true);
}

Result<void> load_prefab_catalog(EditorState& state, const std::filesystem::path& project_root) {
    state.project_root = project_root;
    if (const char* trace_env = std::getenv("ENGINE_AUTOMATION_TRACE")) {
        if (trace_env[0] == '0' && (trace_env[1] == '\0' || trace_env[1] == '\n')) AutomationTrace::set_enabled(false);
    }
    AutomationTrace::set_log_root(project_root / "out/logs");
    for (const auto& entry : state.assets.records()) {
        const auto relative = normalize_asset_path(entry.second.path);
        if (relative.size() < 12 || relative.compare(relative.size() - 12, 12, ".prefab.json") != 0) continue;
        const auto loaded = PrefabAsset::load(project_root / relative);
        if (!loaded) return Result<void>::failure(loaded.error());
        state.prefab_catalog[relative] = loaded.value();
        if (!state.prefab_catalog[relative].mesh.empty())
            state.prefab_catalog[relative].mesh = normalize_asset_path(state.prefab_catalog[relative].mesh);
        state.prefab_bounds[relative] = state.prefab_catalog[relative].bounds(state.mesh_bounds);
    }
    const auto player_path = normalize_asset_path(state.play_session.character_asset);
    if (state.prefab_catalog.find(player_path) == state.prefab_catalog.end()) {
        if (const auto loaded = CharacterAsset::load(project_root / player_path); loaded) {
            const auto visual = normalize_asset_path(loaded.value().visual_prefab);
            if (state.prefab_catalog.find(visual) == state.prefab_catalog.end()) {
                if (const auto prefab = PrefabAsset::load(project_root / visual); prefab)
                    state.prefab_catalog[visual] = prefab.value();
            }
        }
    }
    return Result<void>::success();
}

Result<void> load_editor_play_session(EditorState& state) {
    state.play_session.character_asset = normalize_asset_path(state.play_session.character_asset);
    state.play_session.camera_asset = normalize_asset_path(state.play_session.camera_asset);
    const auto settings_path = default_play_session_settings_path(state.project_root);
    if (std::filesystem::exists(settings_path)) {
        const auto loaded = PlaySessionSettings::load(settings_path);
        if (!loaded) return Result<void>::failure(loaded.error());
        state.play_session = loaded.value();
        state.play_session.character_asset = normalize_asset_path(state.play_session.character_asset);
        state.play_session.camera_asset = normalize_asset_path(state.play_session.camera_asset);
    }
    const auto character_path = state.project_root / state.play_session.character_asset;
    const auto camera_path = state.project_root / state.play_session.camera_asset;
    const auto character_loaded = CharacterAsset::load(character_path);
    if (!character_loaded)
        Logger::instance().write(Severity::Warning, "editor",
            "Could not load character asset: " + character_loaded.error().message);
    else
        state.character_asset = character_loaded.value();
    const auto camera_loaded = CameraAsset::load(camera_path);
    if (!camera_loaded)
        Logger::instance().write(Severity::Warning, "editor",
            "Could not load camera asset: " + camera_loaded.error().message);
    else
        state.camera_asset = camera_loaded.value();
    state.play_session_dirty = false;
    state.character_asset_dirty = false;
    state.camera_asset_dirty = false;
    const auto visual = normalize_asset_path(state.character_asset.visual_prefab);
    if (state.prefab_catalog.find(visual) == state.prefab_catalog.end()) {
        if (const auto prefab = PrefabAsset::load(state.project_root / visual); prefab) {
            state.prefab_catalog[visual] = prefab.value();
            state.prefab_bounds[visual] = prefab.value().bounds(state.mesh_bounds);
        }
    }
    return Result<void>::success();
}

Result<void> save_text_asset(const std::filesystem::path& path, const std::string& text) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-SAVE", "Could not write asset: " + path.generic_string()));
    output << text;
    if (!output)
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-SAVE", "Could not flush asset: " + path.generic_string()));
    return Result<void>::success();
}

std::vector<std::string> collect_asset_paths(const EditorState& state, std::string_view suffix) {
    std::vector<std::string> paths;
    for (const auto& entry : state.assets.records()) {
        const auto normalized = normalize_asset_path(entry.second.path);
        if (normalized.size() >= suffix.size() &&
            normalized.compare(normalized.size() - suffix.size(), suffix.size(), suffix) == 0)
            paths.push_back(normalized);
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

std::optional<std::string> resolve_character_asset_for_prefab(const EditorState& state, const std::string& prefab_path) {
    const auto normalized = normalize_asset_path(prefab_path);
    if (const auto* prefab = find_prefab(state.prefab_catalog, normalized)) {
        if (prefab->character_asset) return *prefab->character_asset;
    }
    for (const auto& path : collect_asset_paths(state, ".character.json")) {
        const auto loaded = CharacterAsset::load(state.project_root / path);
        if (!loaded) continue;
        if (normalize_asset_path(loaded.value().visual_prefab) == normalized) return path;
    }
    return std::nullopt;
}

void sync_player_placement_tags(EditorState& state) {
    for (const auto& id : state.scene.entity_ids()) {
        const auto placement = state.scene.placement(id);
        if (!placement || placement->character_asset) continue;
        if (const auto character_path = resolve_character_asset_for_prefab(state, placement->prefab_asset))
            (void)state.scene.set_placement_character_asset(id, *character_path);
    }
}

bool draw_asset_path_combo(const char* label, std::string& path, const std::vector<std::string>& options,
    const char* none_label = nullptr) {
    bool changed = false;
    const char* preview = path.empty() && none_label ? none_label : path.c_str();
    if (ImGui::BeginCombo(label, preview)) {
        if (none_label && ImGui::Selectable(none_label, path.empty())) {
            path.clear();
            changed = true;
        }
        for (const auto& option : options) {
            if (ImGui::Selectable(option.c_str(), option == path)) {
                path = option;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

std::vector<std::string> collect_animator_state_names(const EditorState& state, const std::string& controller_path) {
    std::vector<std::string> states;
    if (controller_path.empty() || state.project_root.empty()) return states;
    const auto loaded = AnimatorControllerAsset::load(state.project_root / controller_path);
    if (!loaded) return states;
    for (const auto& layer : loaded.value().layers) {
        for (const auto& layer_state : layer.states) {
            if (layer_state.name.empty()) continue;
            if (std::find(states.begin(), states.end(), layer_state.name) == states.end())
                states.push_back(layer_state.name);
        }
    }
    // Prefer idle-like rest poses at the top of the dropdown.
    std::sort(states.begin(), states.end(), [](const std::string& a, const std::string& b) {
        const bool a_idle = a == "idle" || a == "Idle";
        const bool b_idle = b == "idle" || b == "Idle";
        if (a_idle != b_idle) return a_idle;
        return a < b;
    });
    return states;
}

bool draw_animator_property_fields(EditorState& state, AnimatorComponentData& animator) {
    bool commit = false;
    auto controllers = collect_asset_paths(state, ".animator.json");
    if (!animator.controller.empty() &&
        std::find(controllers.begin(), controllers.end(), animator.controller) == controllers.end())
        controllers.insert(controllers.begin(), animator.controller);
    if (draw_asset_path_combo("Controller", animator.controller, controllers, "(none)")) commit = true;

    auto states = collect_animator_state_names(state, animator.controller);
    if (!animator.default_state.empty() &&
        std::find(states.begin(), states.end(), animator.default_state) == states.end())
        states.insert(states.begin(), animator.default_state);
    if (draw_asset_path_combo("Default State", animator.default_state, states, "(controller default)")) commit = true;
    ImGui::TextDisabled("Convention: controller layer defaultState should be idle (rest pose).");
    return commit;
}

bool draw_rigidbody_property_fields(RigidbodyComponentData& rigidbody) {
    bool commit = false;
    const char* motion_items[] = {"dynamic", "kinematic"};
    int motion_index = rigidbody.motion_type == "kinematic" ? 1 : 0;
    if (ImGui::Combo("Motion Type", &motion_index, motion_items, 2)) {
        rigidbody.motion_type = motion_items[motion_index];
        commit = true;
    }
    if (ImGui::InputFloat("Mass", &rigidbody.mass, 0.1f, 1.0f, "%.3f")) commit = true;
    if (ImGui::InputFloat("Linear Damping", &rigidbody.linear_damping, 0.01f, 0.1f, "%.3f")) commit = true;
    if (ImGui::InputFloat("Angular Damping", &rigidbody.angular_damping, 0.01f, 0.1f, "%.3f")) commit = true;
    if (ImGui::Checkbox("Use Gravity", &rigidbody.use_gravity)) commit = true;
    if (ImGui::Checkbox("Freeze Rotation", &rigidbody.freeze_rotation)) commit = true;
    return commit;
}

void draw_character_asset_inspector(EditorState& state, bool placement_entity = false) {
    const std::string asset_path = state.inspector_asset_path
                                       ? normalize_asset_path(*state.inspector_asset_path)
                                       : normalize_asset_path(state.play_session.character_asset);
    ImGui::TextDisabled(placement_entity ? "Player Character Settings" : "Character Asset");
    if (!placement_entity) ImGui::Text("%s", asset_path.c_str());
    const auto prefab_paths = collect_asset_paths(state, ".prefab.json");
    std::string visual = normalize_asset_path(state.character_asset.visual_prefab);
    if (draw_asset_path_combo("Visual Prefab", visual, prefab_paths)) {
        state.character_asset.visual_prefab = visual;
        state.character_asset_dirty = true;
    }
    if (ImGui::InputFloat("Capsule Radius", &state.character_asset.capsule_radius, 0.01f, 0.05f, "%.3f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Capsule Half Height", &state.character_asset.capsule_half_height, 0.01f, 0.05f, "%.3f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Max Slope Ratio", &state.character_asset.max_slope_ratio, 0.01f, 0.05f, "%.3f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Step Height", &state.character_asset.step_height, 0.01f, 0.05f, "%.3f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Max Speed", &state.character_asset.max_speed, 0.1f, 0.5f, "%.2f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Gravity", &state.character_asset.gravity, 0.05f, 0.25f, "%.2f"))
        state.character_asset_dirty = true;
    if (ImGui::InputFloat("Jump Velocity", &state.character_asset.jump_velocity, 0.1f, 0.5f, "%.2f"))
        state.character_asset_dirty = true;
    ImGui::BeginDisabled(state.test_session_active());
    if (placement_entity && state.selected) {
        if (ImGui::Button("Apply to Placement") && !state.test_session_active()) {
            const auto valid = state.character_asset.validate();
            if (!valid) {
                state.status = valid.error().message;
                Logger::instance().write(valid.error());
            } else {
                const auto result = state.history.execute(
                    state.scene, std::make_unique<SetPlacementCharacterSettingsCommand>(*state.selected, state.character_asset));
                state.status = result ? "Player settings applied to placement" : result.error().message;
                if (!result) Logger::instance().write(result.error());
                else state.character_asset_dirty = false;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset from Asset File") && !state.test_session_active()) {
            if (const auto placement = state.scene.placement(*state.selected); placement && placement->character_asset) {
                if (const auto loaded = CharacterAsset::load(state.project_root / *placement->character_asset); loaded) {
                    state.character_asset = loaded.value();
                    state.character_asset_dirty = false;
                    state.status = "Reloaded character settings from asset file";
                }
            }
        }
        if (state.test_session_active())
            ImGui::TextDisabled("End test session to edit placement character settings.");
        else
            ImGui::TextDisabled("Settings are stored on this placement when you apply. F5 also uses unsaved edits while this spawn is selected.");
    } else if (ImGui::Button("Save Character Asset") && !state.test_session_active()) {
        const auto valid = state.character_asset.validate();
        if (!valid) {
            state.status = valid.error().message;
            Logger::instance().write(valid.error());
        } else {
            const auto saved = save_text_asset(state.project_root / asset_path, state.character_asset.to_json());
            state.status = saved ? "Character asset saved" : saved.error().message;
            if (!saved) Logger::instance().write(saved.error());
            else state.character_asset_dirty = false;
        }
    }
    ImGui::EndDisabled();
    if (!placement_entity && state.test_session_active())
        ImGui::TextDisabled("End test session to save character asset changes.");
}

void draw_camera_asset_inspector(EditorState& state) {
    const std::string asset_path = state.inspector_asset_path ? normalize_asset_path(*state.inspector_asset_path)
                                                               : normalize_asset_path(state.play_session.camera_asset);
    ImGui::TextDisabled("Camera Asset (3rd-person RPG)");
    ImGui::Text("%s", asset_path.c_str());
    if (ImGui::InputFloat("Pivot Height", &state.camera_asset.pivot_height, 0.05f, 0.2f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Shoulder Offset", &state.camera_asset.shoulder_offset, 0.05f, 0.1f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Min Distance", &state.camera_asset.min_distance, 0.05f, 0.25f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Max Distance", &state.camera_asset.max_distance, 0.1f, 0.5f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Default Distance", &state.camera_asset.default_distance, 0.1f, 0.5f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Default Pitch", &state.camera_asset.default_pitch, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Min Pitch", &state.camera_asset.min_pitch, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Max Pitch", &state.camera_asset.max_pitch, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Collision Probe Radius", &state.camera_asset.collision_probe_radius, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Collision Padding", &state.camera_asset.collision_padding, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Look Sensitivity", &state.camera_asset.look_sensitivity, 0.0001f, 0.001f, "%.4f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Zoom Sensitivity (m/notch)", &state.camera_asset.zoom_sensitivity, 0.1f, 0.5f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Vertical FOV (rad)", &state.camera_asset.vertical_fov_radians, 0.01f, 0.1f, "%.4f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Near Plane", &state.camera_asset.near_plane, 0.01f, 0.1f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Far Plane", &state.camera_asset.far_plane, 1.0f, 50.0f, "%.1f"))
        state.camera_asset_dirty = true;
    if (ImGui::Button("Save Camera Asset")) {
        const auto valid = state.camera_asset.validate();
        if (!valid) {
            state.status = valid.error().message;
            Logger::instance().write(valid.error());
        } else {
            const auto saved = save_text_asset(state.project_root / asset_path, state.camera_asset.to_json());
            if (!saved) {
                state.status = saved.error().message;
                Logger::instance().write(saved.error());
            }             else {
                state.camera_asset_dirty = false;
                state.status = "Camera asset saved";
            }
        }
    }
}

void draw_material_asset_inspector(EditorState& state, MaterialAsset* terrain_material,
    StreamedTerrainField* streamed_terrain, CollisionWorld* collision) {
    const std::string asset_path =
        state.inspector_asset_path ? normalize_asset_path(*state.inspector_asset_path) : std::string{};
    ImGui::TextDisabled("Material Asset");
    const auto material_paths = collect_asset_paths(state, ".material.json");
    std::string inspector_material = asset_path;
    if (draw_asset_path_combo("Material##inspector", inspector_material, material_paths)) {
        const auto normalized = normalize_asset_path(inspector_material);
        state.inspector_asset_path = normalized;
        if (const MaterialAsset* loaded = editor_material(state, normalized)) {
            state.material_asset = *loaded;
            state.material_asset_dirty = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ New")) request_new_material_asset(state, state.sculpt_viewport_active(), "materials");
    if (state.sculpt_viewport_active()) {
        const bool is_tint = normalize_asset_path(state.terrain_material_path) == normalize_asset_path(asset_path);
        const bool is_paint_brush =
            normalize_asset_path(state.terrain_paint_brush_material) == normalize_asset_path(asset_path);
        if (!is_paint_brush) {
            if (ImGui::Button("Use as Paint Brush")) {
                state.terrain_paint_brush_material = normalize_asset_path(asset_path);
                (void)state.terrain_paint.ensure_material_index(state.terrain_paint_brush_material);
                state.status = "Paint brush material set";
            }
        } else {
            ImGui::TextDisabled("Current paint brush material");
        }
        if (!is_tint) {
            ImGui::SameLine();
            if (ImGui::Button("Use as Global Tint")) {
                state.terrain_material_path = normalize_asset_path(asset_path);
                apply_active_terrain_material(state, terrain_material, streamed_terrain, collision);
                state.status = "Global terrain tint updated";
            }
        } else {
            ImGui::SameLine();
            ImGui::TextDisabled("Current global tint");
        }
    }
    ImGui::Separator();
    const auto display_name = asset_path.empty() ? std::string{"Material"}
                                                 : std::filesystem::path(asset_path).stem().string();
    ImGui::Text("%s", display_name.c_str());
    ImGui::TextDisabled("%s", asset_path.c_str());
    if (ImGui::ColorEdit4("Base Color", state.material_asset.base_color.data()))
        state.material_asset_dirty = true;
    if (ImGui::InputFloat("Roughness", &state.material_asset.roughness, 0.01f, 0.05f, "%.3f"))
        state.material_asset_dirty = true;
    if (ImGui::InputFloat("Metallic", &state.material_asset.metallic, 0.01f, 0.05f, "%.3f"))
        state.material_asset_dirty = true;
    const char* opacity_modes[] = {"opaque", "masked", "blended"};
    int opacity_index = state.material_asset.opacity_mode == OpacityMode::Opaque
                            ? 0
                            : state.material_asset.opacity_mode == OpacityMode::Masked ? 1 : 2;
    if (ImGui::Combo("Opacity Mode", &opacity_index, opacity_modes, IM_ARRAYSIZE(opacity_modes))) {
        state.material_asset.opacity_mode =
            opacity_index == 0 ? OpacityMode::Opaque : opacity_index == 1 ? OpacityMode::Masked : OpacityMode::Blended;
        state.material_asset_dirty = true;
    }
    if (ImGui::InputFloat("Opacity Cutoff", &state.material_asset.opacity_cutoff, 0.01f, 0.05f, "%.3f"))
        state.material_asset_dirty = true;
    if (ImGui::ColorEdit3("Emissive", state.material_asset.emissive.data()))
        state.material_asset_dirty = true;
    if (ImGui::Checkbox("Double Sided", &state.material_asset.double_sided)) state.material_asset_dirty = true;
    ImGui::Separator();
    ImGui::TextDisabled("Physics");
    if (ImGui::InputFloat("Friction", &state.material_asset.physics.friction, 0.01f, 0.05f, "%.3f"))
        state.material_asset_dirty = true;
    if (ImGui::InputFloat("Restitution", &state.material_asset.physics.restitution, 0.01f, 0.05f, "%.3f"))
        state.material_asset_dirty = true;
    if (ImGui::InputFloat("Density", &state.material_asset.physics.density, 1.0f, 10.0f, "%.1f"))
        state.material_asset_dirty = true;
    char surface_buffer[64];
    std::snprintf(surface_buffer, sizeof(surface_buffer), "%s", state.material_asset.physics.surface.c_str());
    if (ImGui::InputText("Surface", surface_buffer, sizeof(surface_buffer))) {
        state.material_asset.physics.surface = surface_buffer;
        state.material_asset_dirty = true;
    }
    ImGui::BeginDisabled(state.test_session_active());
    if (ImGui::Button("Save Material Asset") && !state.test_session_active()) {
        const auto valid = state.material_asset.validate();
        if (!valid) {
            state.status = valid.error().message;
            Logger::instance().write(valid.error());
        } else {
            const auto saved =
                save_text_asset(state.project_root / asset_path, state.material_asset.to_json());
            state.status = saved ? "Material asset saved" : saved.error().message;
            if (!saved) Logger::instance().write(saved.error());
            else {
                state.material_asset_dirty = false;
                state.material_cache[normalize_asset_path(asset_path)] = state.material_asset;
                if (terrain_material && asset_path == normalize_asset_path(state.terrain_material_path)) {
                    *terrain_material = state.material_asset;
                    if (streamed_terrain && collision) {
                        const auto loaded = streamed_terrain->loaded_cell_coordinates();
                        const auto lookup = editor_paint_material_lookup(state);
                        if (!loaded.empty()) {
                            const auto reloaded = streamed_terrain->reload_cells(
                                *collision, loaded, terrain_material->physics, &state.terrain_edits,
                                &state.terrain_paint, lookup);
                            if (!reloaded) Logger::instance().write(reloaded.error());
                        } else {
                            streamed_terrain->mark_render_data_dirty();
                        }
                    }
                } else if (streamed_terrain) {
                    for (const auto& material_path : state.terrain_paint.materials()) {
                        if (normalize_asset_path(material_path) == normalize_asset_path(asset_path)) {
                            reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
                            break;
                        }
                    }
                }
            }
        }
    }
    ImGui::EndDisabled();
    if (!state.test_session_active() && state.material_asset_dirty)
        ImGui::TextDisabled("Unsaved material changes.");
    if (state.test_session_active()) ImGui::TextDisabled("End test session to save material asset changes.");
}

void draw_play_session_inspector(EditorState& state) {
    ImGui::TextDisabled("No entity selected");
    ImGui::TextWrapped(
        "Select a placed object in the hierarchy or viewport to edit it. Player movement settings appear when a player spawn prefab is selected.");
    ImGui::Separator();
    ImGui::TextDisabled("Default Test Session Assets");
    ImGui::TextWrapped(
        "These bindings are used when no player spawn exists in the scene, and as the source file when resetting a player placement.");
    const auto character_paths = collect_asset_paths(state, ".character.json");
    const auto camera_paths = collect_asset_paths(state, ".camera.json");
    if (draw_asset_path_combo("Character Asset", state.play_session.character_asset, character_paths))
        state.play_session_dirty = true;
    if (draw_asset_path_combo("Camera Asset", state.play_session.camera_asset, camera_paths))
        state.play_session_dirty = true;
    ImGui::BeginDisabled(state.test_session_active());
    if (ImGui::Button("Apply Session Assets") && !state.test_session_active()) {
        const auto loaded = load_editor_play_session(state);
        state.status = loaded ? "Play session assets reloaded" : loaded.error().message;
        if (!loaded) Logger::instance().write(loaded.error());
        else state.play_session_dirty = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save play.session.json") && !state.test_session_active()) {
        const auto saved = state.play_session.save(default_play_session_settings_path(state.project_root));
        state.status = saved ? "Play session settings saved" : saved.error().message;
        if (!saved) Logger::instance().write(saved.error());
        else state.play_session_dirty = false;
    }
    ImGui::EndDisabled();
    if (state.test_session_active()) ImGui::TextDisabled("End test session to change play session bindings.");
}

WorldPosition character_feet_pivot(const CharacterController& character) {
    const auto body = character.debug_body();
    return {body.position.x, body.position.y - static_cast<double>(body.half_extent.x + body.radius), body.position.z};
}

TransformComponent character_visual_transform(const CharacterController& character, const PrefabAsset& player_prefab,
    float facing_yaw) {
    using namespace DirectX;
    const auto body = character.debug_body();
    TransformComponent transform;
    // Blockbench/player.gltf faces −Z; controller walk basis uses yaw 0 → +Z.
    constexpr float k_model_forward_yaw_offset = 3.14159265f;
    const auto facing_q = XMQuaternionRotationRollPitchYaw(0.0f, facing_yaw + k_model_forward_yaw_offset, 0.0f);
    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(transform.rotation.data()), facing_q);
    const bool unit_capsule_proxy = !player_prefab.parts.empty() && player_prefab.parts.front().mesh.primitive.has_value()
        && !player_prefab.parts.front().mesh.asset.has_value();
    if (unit_capsule_proxy) {
        transform.position = {static_cast<float>(body.position.x), static_cast<float>(body.position.y),
                              static_cast<float>(body.position.z)};
        constexpr float k_unit_radius = 0.5f;
        constexpr float k_unit_half_height = 1.0f;
        const float diameter_scale = body.radius / k_unit_radius;
        const float height_scale = (body.half_extent.x + body.radius) / k_unit_half_height;
        transform.scale = {diameter_scale, height_scale, diameter_scale};
        return transform;
    }
    const auto feet = character_feet_pivot(character);
    transform.position = {static_cast<float>(feet.x), static_cast<float>(feet.y), static_cast<float>(feet.z)};
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

void append_character_render_instances(const PrefabAsset& player_prefab, const CharacterController& character,
    float facing_yaw, std::vector<RenderInstance>& instances,
    const PrefabAsset::MaterialLookup& lookup_material = {}) {
    expand_prefab_render_instances(player_prefab, character_visual_transform(character, player_prefab, facing_yaw),
        instances, lookup_material);
}

Result<void> ensure_runtime_player_assets(const std::filesystem::path& project_root, PrefabAsset& player_prefab,
    std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes) {
    PlaySessionSettings session;
    const auto settings_path = default_play_session_settings_path(project_root);
    if (std::filesystem::exists(settings_path)) {
        const auto loaded = PlaySessionSettings::load(settings_path);
        if (loaded) session = loaded.value();
    }
    const auto character_loaded = CharacterAsset::load(project_root / session.character_asset);
    if (!character_loaded) return Result<void>::failure(character_loaded.error());
    const auto prefab_loaded = PrefabAsset::load(project_root / character_loaded.value().visual_prefab);
    if (!prefab_loaded) return Result<void>::failure(prefab_loaded.error());
    player_prefab = prefab_loaded.value();
    if (player_prefab.parts.empty())
        return Result<void>::failure(
            graphics_error("PLAYER-PREFAB-INVALID", "Player prefab has no render parts", E_FAIL));
    const auto& part = player_prefab.parts.front();
    if (part.mesh.asset) {
        const auto key = normalize_asset_path(*part.mesh.asset);
        for (const auto& mesh : imported_meshes) {
            if (normalize_asset_path(mesh.first) == key) return Result<void>::success();
        }
        auto imported = import_project_mesh(project_root / key);
        if (!imported) return Result<void>::failure(imported.error());
        imported_meshes.emplace_back(key, std::move(imported.value()));
        return Result<void>::success();
    }
    if (!part.mesh.primitive) {
        return Result<void>::failure(graphics_error("PLAYER-PREFAB-INVALID",
            "Player prefab part must reference a mesh asset or primitive", E_FAIL));
    }
    const auto key = primitive_mesh_cache_key(*part.mesh.primitive, part.mesh.color);
    for (const auto& mesh : imported_meshes) {
        if (mesh.first == key) return Result<void>::success();
    }
    auto generated = generate_primitive_mesh(*part.mesh.primitive, part.mesh.color);
    if (!generated) return Result<void>::failure(generated.error());
    imported_meshes.emplace_back(key, std::move(generated.value()));
    return Result<void>::success();
}

void ensure_prefab_primitive_meshes(EditorState& state, std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes) {
    const auto lookup_material = make_material_lookup(&state.material_cache);
    ensure_prefab_catalog_meshes(state.project_root, state.prefab_catalog, lookup_material, state.mesh_bounds,
        imported_meshes, &state.pending_mesh_reloads);
    for (auto& entry : state.prefab_catalog) {
        state.prefab_bounds[entry.first] = entry.second.bounds(state.mesh_bounds, lookup_material);
    }
}

PlacementBoundsSource placement_bounds_source(const EditorState& state, const EntityId& id,
    const TransformComponent&) {
    PlacementBoundsSource source{k_proxy_bounds, 0.0f};
    const auto placement = state.scene.placement(id);
    if (!placement) return source;
    const auto prefab = find_prefab(state.prefab_catalog, placement->prefab_asset);
    if (!prefab) return source;
    const auto resolved = resolve_prefab_catalog_path(state.prefab_catalog, placement->prefab_asset);
    const auto lookup_material = make_material_lookup(&state.material_cache);
    const auto bounds = state.prefab_bounds.find(resolved);
    if (bounds != state.prefab_bounds.end()) source.local_bounds = bounds->second;
    else source.local_bounds = prefab->bounds(state.mesh_bounds, lookup_material);
    return source;
}

std::optional<EntityId> pick_placement(const Scene& scene, const EditorState& state,
    const std::array<float, 16>& view, const std::array<float, 16>& projection,
    const std::array<float, 16>& view_projection, const ViewportFrame& frame, ImVec2 mouse) {
    if (const auto ray = build_viewport_ray(frame.image_min.x, frame.image_min.y, frame.width, frame.height, view,
            projection, mouse.x, mouse.y)) {
        const PlacementPickContext context{&scene, &state.prefab_catalog, &state.mesh_bounds, &state.material_cache};
        if (const auto mesh_pick = pick_placement_mesh(context, *ray)) return mesh_pick;
    }
    std::optional<EntityId> best_id;
    float best_depth = 1.0f;
    for (const auto& id : scene.entity_ids()) {
        const auto placement = scene.placement(id);
        const auto transform = scene.transform(id);
        if (!placement || !transform) continue;
        const auto source = placement_bounds_source(state, id, *transform);
        const auto bounds =
            compute_placement_screen_bounds(*transform, source.local_bounds, source.y_translation_offset, view_projection, frame);
        if (!bounds.visible) continue;
        constexpr float pad = 6.0f;
        if (mouse.x >= bounds.min_x - pad && mouse.x <= bounds.max_x + pad && mouse.y >= bounds.min_y - pad &&
            mouse.y <= bounds.max_y + pad && (!best_id || bounds.center_depth < best_depth)) {
            best_id = id;
            best_depth = bounds.center_depth;
        }
    }
    return best_id;
}

bool place_prefab_at(EditorState& state, const std::string& path, const std::optional<WorldPosition>& position) {
    TransformComponent transform;
    if (position) {
        transform.position = {static_cast<float>(position->x), static_cast<float>(position->y),
                              static_cast<float>(position->z)};
    }
    const auto character_asset = resolve_character_asset_for_prefab(state, path);
    const auto id = EntityId::generate();
    std::optional<PrefabAsset> seed;
    if (const auto* prefab = find_prefab(state.prefab_catalog, path)) seed = *prefab;
    const auto result = state.history.execute(
        state.scene,
        std::make_unique<PlaceWorldObjectCommand>(std::filesystem::path(path).stem().stem().string(), path, transform,
            id, character_asset, std::move(seed)));
    if (result) {
        mark_scene_dirty(state);
        state.selected = id;
        state.gizmo_entity.reset();
        state.gizmo_preview.reset();
        state.drop_preview.reset();
        state.drop_preview_prefab.reset();
        state.status = "Placed " + path;
        return true;
    }
    state.status = result.error().message;
    Logger::instance().write(result.error());
    return false;
}

void remove_selected(EditorState& state) {
    if (!state.selected || !state.scene.placement(*state.selected)) return;
    const auto result = state.history.execute(state.scene, std::make_unique<RemoveWorldObjectCommand>(*state.selected));
    if (result) {
        mark_scene_dirty(state);
        state.selected.reset();
        state.gizmo_entity.reset();
        state.gizmo_preview.reset();
        state.status = "Object removed";
    } else {
        state.status = result.error().message;
        Logger::instance().write(result.error());
    }
}

void duplicate_selected(EditorState& state) {
    if (!state.selected) return;
    const auto source_id = *state.selected;
    const auto placement = state.scene.placement(source_id);
    const auto transform = state.scene.transform(source_id);
    const auto name = state.scene.name(source_id);
    if (!placement || !transform || !name) return;
    TransformComponent offset = *transform;
    offset.position[0] += 1.5f;
    const auto id = EntityId::generate();
    std::optional<PrefabAsset> seed;
    if (const auto* prefab = find_prefab(state.prefab_catalog, placement->prefab_asset)) seed = *prefab;
    const auto source_components = state.scene.authored_components(source_id);
    const auto result = state.history.execute(
        state.scene, std::make_unique<PlaceWorldObjectCommand>(*name + " Copy", placement->prefab_asset, offset, id,
            placement->character_asset, seed));
    if (result) {
        if (source_components && !source_components->entries.empty())
            (void)state.scene.set_authored_components(id, *source_components);
        mark_scene_dirty(state);
        state.selected = id;
        state.gizmo_entity.reset();
        state.gizmo_preview.reset();
        state.status = "Duplicated object";
    } else {
        state.status = result.error().message;
        Logger::instance().write(result.error());
    }
}

void clear_editor_manipulation(EditorState& state) {
    state.terrain_drag_active = false;
    state.left_press_active = false;
    state.gizmo_entity.reset();
    state.gizmo_preview.reset();
    state.gizmo_was_using = false;
}

WorldPosition editor_test_spawn_position(const EditorState& state, const DebugCamera& camera) {
    if (state.placement_cursor) {
        const float ground = sample_terrain_height(static_cast<float>(state.placement_cursor->x),
            static_cast<float>(state.placement_cursor->z));
        return {state.placement_cursor->x, ground + 2.0, state.placement_cursor->z};
    }
    const auto pos = camera.position();
    const auto fwd = camera.forward();
    const float x = pos[0] + fwd[0] * 6.0f;
    const float z = pos[2] + fwd[2] * 6.0f;
    const float ground = sample_terrain_height(x, z);
    return {static_cast<double>(x), ground + 2.0, static_cast<double>(z)};
}

struct TestPlayerSpawnResolution {
    WorldPosition position;
    std::optional<EntityId> placement_entity;
    std::optional<std::string> character_asset_path;
};

TestPlayerSpawnResolution resolve_test_player_spawn(const EditorState& state, const DebugCamera& camera) {
    std::vector<EntityId> spawns;
    for (const auto& id : state.scene.entity_ids()) {
        const auto placement = state.scene.placement(id);
        if (placement && placement->character_asset) spawns.push_back(id);
    }
    std::optional<EntityId> chosen;
    if (state.selected) {
        const auto placement = state.scene.placement(*state.selected);
        if (placement && placement->character_asset) chosen = state.selected;
    }
    if (!chosen && spawns.size() == 1) chosen = spawns.front();
    if (chosen) {
        const auto transform = state.scene.transform(*chosen);
        const auto placement = state.scene.placement(*chosen);
        if (transform && placement) {
            return {{transform->position[0], transform->position[1], transform->position[2]}, chosen,
                placement->character_asset};
        }
    }
    return {editor_test_spawn_position(state, camera), std::nullopt, std::nullopt};
}

CharacterAsset resolve_character_for_spawn(const EditorState& editor, const EntityId& spawn_id) {
    if (editor.selected && *editor.selected == spawn_id) return editor.character_asset;
    const auto placement = editor.scene.placement(spawn_id);
    if (!placement) return editor.character_asset;
    if (placement->character_settings) return *placement->character_settings;
    if (placement->character_asset) {
        if (const auto loaded = CharacterAsset::load(editor.project_root / *placement->character_asset); loaded)
            return loaded.value();
    }
    return editor.character_asset;
}

struct EditorTestSessionRestore {
    std::array<float, 3> camera_position{};
    float camera_yaw = 0.0f;
    float camera_pitch = 0.0f;
    std::optional<EntityId> spawn_entity;
    std::optional<TransformComponent> spawn_transform;
};

TerrainPaintMaterialLookup editor_paint_material_lookup(EditorState& state) {
    return [&state](const std::string& normalized_path) -> const MaterialAsset* {
        return editor_material(state, normalized_path);
    };
}

void reload_loaded_terrain_cells(EditorState& state, StreamedTerrainField* streamed_terrain, CollisionWorld* collision,
    MaterialAsset* terrain_material, bool height_changed) {
    if (!streamed_terrain || !terrain_material) return;
    const auto lookup = editor_paint_material_lookup(state);
    const auto loaded = streamed_terrain->loaded_cell_coordinates();
    if (loaded.empty()) {
        streamed_terrain->mark_render_data_dirty();
        return;
    }
    if (collision && height_changed) {
        (void)streamed_terrain->reload_cells(*collision, loaded, terrain_material->physics, &state.terrain_edits,
            &state.terrain_paint, lookup);
    } else {
        (void)streamed_terrain->reload_cell_meshes(loaded, &state.terrain_edits, &state.terrain_paint, lookup);
    }
}

void reload_loaded_water_cells(EditorState& state, StreamedWaterField* streamed_water) {
    if (!streamed_water) return;
    const auto loaded = streamed_water->loaded_cell_coordinates();
    if (loaded.empty()) {
        streamed_water->mark_render_data_dirty();
        return;
    }
    (void)streamed_water->reload_cells(loaded, &state.water_store);
}

std::string sanitize_asset_filename(std::string name) {
    if (name.empty()) return "material";
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    for (char& character : name) {
        const bool allowed = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                             character == '_' || character == '-';
        if (!allowed) character = '_';
    }
    while (!name.empty() && name.front() == '_') name.erase(name.begin());
    while (!name.empty() && name.back() == '_') name.pop_back();
    return name.empty() ? "material" : name;
}

MaterialAsset* editor_material(EditorState& state, const std::string& path) {
    const auto normalized = normalize_asset_path(path);
    const auto found = state.material_cache.find(normalized);
    if (found != state.material_cache.end()) return &found->second;
    const auto loaded = MaterialAsset::load(state.project_root / normalized);
    if (!loaded) return nullptr;
    auto [inserted, _] = state.material_cache.emplace(normalized, loaded.value());
    return &inserted->second;
}

void warm_material_cache(EditorState& state) {
    for (const auto& path : collect_asset_paths(state, ".material.json")) (void)editor_material(state, path);
}

void apply_active_terrain_material(EditorState& state, MaterialAsset* terrain_material,
    StreamedTerrainField* streamed_terrain, CollisionWorld* collision) {
    if (!terrain_material || !streamed_terrain) return;
    const auto loaded_asset = MaterialAsset::load(state.project_root / state.terrain_material_path);
    if (!loaded_asset) {
        state.status = loaded_asset.error().message;
        Logger::instance().write(loaded_asset.error());
        return;
    }
    *terrain_material = loaded_asset.value();
    state.material_cache[normalize_asset_path(state.terrain_material_path)] = *terrain_material;
    reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
}

Result<std::string> create_material_asset(EditorState& state, const std::string& raw_name,
    std::optional<std::string_view> folder_override) {
    const auto filename = sanitize_asset_filename(raw_name) + ".material.json";
    const std::string subfolder = folder_override.has_value() && !folder_override->empty()
                                      ? std::string(*folder_override)
                                      : (state.asset_browser_folder.empty() ? "materials" : state.asset_browser_folder);
    std::string relative = "assets/" + subfolder + "/";
    relative += filename;
    const auto normalized = normalize_asset_path(relative);
    const auto absolute = state.project_root / normalized;
    if (std::filesystem::exists(absolute))
        return Result<std::string>::failure(
            graphics_error("MATERIAL-EXISTS", "Material already exists: " + normalized, E_FAIL));
    const auto saved = MaterialAsset::make_default().save_atomic(absolute);
    if (!saved) return Result<std::string>::failure(saved.error());
    const auto scanned = state.assets.scan(state.project_root);
    if (!scanned) return Result<std::string>::failure(scanned.error());
    state.material_cache[normalized] = MaterialAsset::make_default();
    return Result<std::string>::success(normalized);
}

void commit_terrain_paint_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    std::map<CellCoord, std::vector<std::uint16_t>> after;
    for (const auto& cell : state.terrain_paint_brush_touched)
        after[cell] = state.terrain_paint.cell_indices_or_empty(cell);
    if (!state.terrain_paint_brush_touched.empty()) {
        const auto result = state.terrain_paint_history.execute(state.terrain_paint,
            std::make_unique<TerrainPaintBrushStrokeCommand>(state.terrain_paint_brush_before, std::move(after)));
        if (!result) {
            state.status = result.error().message;
            Logger::instance().write(result.error());
        } else {
            state.status = "Terrain paint stroke committed";
        }
    }
    state.terrain_brush_active = false;
    state.terrain_paint_brush_before.clear();
    state.terrain_paint_brush_touched.clear();
}

void commit_foliage_density_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    std::map<CellCoord, FoliageCellSnapshot> after;
    for (const auto& cell : state.foliage_brush_touched) after[cell] = state.foliage_density.cell_snapshot_or_empty(cell);
    if (!state.foliage_brush_touched.empty()) {
        const auto result = state.foliage_density_history.execute(state.foliage_density,
            std::make_unique<FoliageDensityBrushStrokeCommand>(state.foliage_brush_before, std::move(after)));
        if (!result) {
            state.status = result.error().message;
            Logger::instance().write(result.error());
        } else {
            state.status = "Foliage density stroke committed";
        }
    }
    state.terrain_brush_active = false;
    state.foliage_brush_before.clear();
    state.foliage_brush_touched.clear();
}

void commit_terrain_brush_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    std::map<CellCoord, std::vector<float>> after;
    for (const auto& cell : state.terrain_brush_touched)
        after[cell] = state.terrain_edits.cell_deltas_or_empty(cell);
    if (!state.terrain_brush_touched.empty()) {
        const auto result = state.terrain_history.execute(state.terrain_edits,
            std::make_unique<TerrainBrushStrokeCommand>(state.terrain_brush_before, std::move(after)));
        if (!result) {
            state.status = result.error().message;
            Logger::instance().write(result.error());
        } else {
            state.status = "Terrain brush stroke committed";
        }
    }
    state.terrain_brush_active = false;
    state.terrain_brush_before.clear();
    state.terrain_brush_touched.clear();
}

void commit_water_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    std::map<CellCoord, WaterCellSnapshot> after;
    for (const auto& cell : state.water_brush_touched)
        after[cell] = WaterCellSnapshot{state.water_store.cell_fill_or_empty(cell)};
    if (!state.water_brush_touched.empty()) {
        const auto result = state.water_history.execute(state.water_store,
            std::make_unique<WaterBrushStrokeCommand>(state.water_brush_before, std::move(after)));
        if (!result) {
            state.status = result.error().message;
            Logger::instance().write(result.error());
        } else {
            state.water_dirty = true;
            state.status = "Water stroke committed";
        }
    }
    state.terrain_brush_active = false;
    state.water_brush_before.clear();
    state.water_brush_touched.clear();
}

void commit_active_terrain_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    if (state.sculpt_tool == EditorState::SculptTool::Paint) commit_terrain_paint_stroke(state);
    else if (state.sculpt_tool == EditorState::SculptTool::Foliage) commit_foliage_density_stroke(state);
    else if (state.sculpt_tool == EditorState::SculptTool::Water) commit_water_stroke(state);
    else commit_terrain_brush_stroke(state);
    state.terrain_flatten_target_valid = false;
}

void process_test_session_ui_input(EditorState& state) {
    if (!state.test_session_active() || !state.ui_canvas_stack || ImGui::GetIO().WantTextInput) return;
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) return;

    const bool esc =
        ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight);
    if (esc && !state.ui_canvas_stack->has_modal() && state.ui_canvas_stack->is_registered("pause")) {
        (void)state.ui_canvas_stack->push("pause");
        if (state.test_session == EditorState::TestSessionState::Running)
            state.test_session = EditorState::TestSessionState::Paused;
        state.status = "Pause menu opened";
        return;
    }

    if (!state.ui_canvas_stack->has_modal()) {
        if (ImGui::IsKeyPressed(ImGuiKey_I) && state.ui_canvas_stack->is_registered("inventory")) {
            (void)state.ui_canvas_stack->push("inventory");
            state.status = "Inventory opened";
            return;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && state.ui_canvas_stack->is_registered("dialogue")) {
            (void)state.ui_canvas_stack->push("dialogue");
            state.status = "Dialogue opened";
            return;
        }
        return;
    }

    UiCanvasInputEvent event;
    if (state.game_viewport_min && state.game_viewport_max) {
        event.viewport_min = {state.game_viewport_min->x, state.game_viewport_min->y};
        event.viewport_max = {state.game_viewport_max->x, state.game_viewport_max->y};
    } else {
        event.viewport_min = {0.0f, 0.0f};
        event.viewport_max = {1920.0f, 1080.0f};
    }
    if (esc) event.cancel_pressed = true;

    const auto& io = ImGui::GetIO();
    event.nav_next = ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadDown) ||
        (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Tab));
    event.nav_prev = ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadUp) ||
        (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Tab));
    event.adjust_left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);
    event.adjust_right = ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
    event.activate_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Space) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown);
    if (state.game_viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        event.mouse_clicked = true;
        event.mouse_pos = {io.MousePos.x, io.MousePos.y};
    }

    const auto result = state.ui_canvas_stack->handle_modal_input(event, state.lua_runtime.get());
    if (result.modal_popped) {
        if (result.canvas_id == "pause" && state.test_session == EditorState::TestSessionState::Paused)
            state.test_session = EditorState::TestSessionState::Running;
        state.status = "Closed overlay: " + result.canvas_id;
    }
    if (result.activated_bind) {
        const auto& bind = *result.activated_bind;
        if (bind == "pause.resume" && state.test_session == EditorState::TestSessionState::Paused)
            state.test_session = EditorState::TestSessionState::Running;
        else if (bind == "pause.quit" && state.ui_canvas_stack->top_modal() == "main_menu")
            state.status = "Main menu opened";
        else if (bind == "main_menu.new_game" &&
            state.test_session == EditorState::TestSessionState::Paused)
            state.test_session = EditorState::TestSessionState::Running;
        else if (bind == "main_menu.quit")
            state.test_session_command = EditorState::TestSessionCommand::End;
    }
}

void handle_editor_shortcuts(EditorState& state, bool camera_capture, MaterialAsset* terrain_material,
    StreamedTerrainField* streamed_terrain, StreamedWaterField* streamed_water, CollisionWorld* collision) {
    if (camera_capture || ImGui::GetIO().WantTextInput) return;
    const bool editor_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    if (!editor_focus) return;
    const auto& io = ImGui::GetIO();
    if (state.test_session == EditorState::TestSessionState::Inactive && ImGui::IsKeyPressed(ImGuiKey_F5))
        state.test_session_command = EditorState::TestSessionCommand::Start;
    if (state.test_session == EditorState::TestSessionState::Running && ImGui::IsKeyPressed(ImGuiKey_F6))
        state.test_session_command = EditorState::TestSessionCommand::Pause;
    if (state.test_session == EditorState::TestSessionState::Paused && ImGui::IsKeyPressed(ImGuiKey_F6))
        state.test_session_command = EditorState::TestSessionCommand::Resume;
    if (state.test_session_active() && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_F5))
        state.test_session_command = EditorState::TestSessionCommand::End;
    if (state.test_session_active()) return;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        const auto saved = state.scene.save_atomic(state.world_path);
        if (!saved) {
            state.status = saved.error().message;
            Logger::instance().write(saved.error());
        } else {
            const auto terrain_saved = state.terrain_edits.save_atomic(default_terrain_edits_path(state.project_root));
            const auto paint_saved = state.terrain_paint.save_atomic(default_terrain_paint_path(state.project_root));
            const auto foliage_saved =
                state.foliage_density.save_atomic(default_foliage_density_path(state.project_root));
            if (!terrain_saved) {
                state.status = terrain_saved.error().message;
                Logger::instance().write(terrain_saved.error());
            } else if (!paint_saved) {
                state.status = paint_saved.error().message;
                Logger::instance().write(paint_saved.error());
            } else if (!foliage_saved) {
                state.status = foliage_saved.error().message;
                Logger::instance().write(foliage_saved.error());
            } else {
                const auto water_saved = state.water_store.save_atomic(default_water_surfaces_path(state.project_root));
                if (!water_saved) {
                    state.status = water_saved.error().message;
                    Logger::instance().write(water_saved.error());
                } else {
                state.terrain_edits_dirty = false;
                state.terrain_paint_dirty = false;
                state.foliage_density_dirty = false;
                state.water_dirty = false;
                state.scene_dirty = false;
                state.status = "World, terrain sculpt, paint, foliage, and water saved";
                }
            }
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Water &&
            state.water_history.undo_size() > 0) {
            const auto result = state.water_history.undo(state.water_store);
            state.status = result ? state.water_history.last_summary() : result.error().message;
            if (result) {
                state.water_dirty = true;
                reload_loaded_water_cells(state, streamed_water);
            }
        } else if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Foliage &&
            state.foliage_density_history.undo_size() > 0) {
            const auto result = state.foliage_density_history.undo(state.foliage_density);
            state.status = result ? state.foliage_density_history.last_summary() : result.error().message;
            if (result) state.foliage_density_dirty = true;
        } else if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Paint &&
            state.terrain_paint_history.undo_size() > 0) {
            const auto result = state.terrain_paint_history.undo(state.terrain_paint);
            state.status = result ? state.terrain_paint_history.last_summary() : result.error().message;
            if (result && streamed_terrain && terrain_material) {
                reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
                state.terrain_paint_dirty = true;
            }
        } else if (state.sculpt_viewport_active() && state.terrain_history.undo_size() > 0) {
            const auto result = state.terrain_history.undo(state.terrain_edits);
            state.status = result ? state.terrain_history.last_summary() : result.error().message;
            if (result && streamed_terrain && collision && terrain_material) {
                reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material);
                state.terrain_edits_dirty = true; ++state.terrain_height_revision;
            }
        } else if (state.history.undo_size() > 0) {
            const auto result = state.history.undo(state.scene);
            state.status = result ? state.history.last_summary() : result.error().message;
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Water &&
            state.water_history.redo_size() > 0) {
            const auto result = state.water_history.redo(state.water_store);
            state.status = result ? state.water_history.last_summary() : result.error().message;
            if (result) {
                state.water_dirty = true;
                reload_loaded_water_cells(state, streamed_water);
            }
        } else if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Foliage &&
            state.foliage_density_history.redo_size() > 0) {
            const auto result = state.foliage_density_history.redo(state.foliage_density);
            state.status = result ? state.foliage_density_history.last_summary() : result.error().message;
            if (result) state.foliage_density_dirty = true;
        } else if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Paint &&
            state.terrain_paint_history.redo_size() > 0) {
            const auto result = state.terrain_paint_history.redo(state.terrain_paint);
            state.status = result ? state.terrain_paint_history.last_summary() : result.error().message;
            if (result && streamed_terrain && terrain_material) {
                reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
                state.terrain_paint_dirty = true;
            }
        } else if (state.sculpt_viewport_active() && state.terrain_history.redo_size() > 0) {
            const auto result = state.terrain_history.redo(state.terrain_edits);
            state.status = result ? state.terrain_history.last_summary() : result.error().message;
            if (result && streamed_terrain && collision && terrain_material) {
                reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material);
                state.terrain_edits_dirty = true; ++state.terrain_height_revision;
            }
        } else if (state.history.redo_size() > 0) {
            const auto result = state.history.redo(state.scene);
            state.status = result ? state.history.last_summary() : result.error().message;
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D) && state.selected) duplicate_selected(state);
    if ((ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace)) && state.selected &&
        state.scene.placement(*state.selected))
        remove_selected(state);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        state.selected.reset();
        state.hovered.reset();
        state.inspector_player_spawn_entity.reset();
        state.gizmo_entity.reset();
        state.gizmo_preview.reset();
        state.status = "Selection cleared";
    }
}

TransformComponent editor_transform_for_entity(const EditorState& state, const EntityId& id) {
    if (state.selected && *state.selected == id && state.gizmo_preview &&
        (state.gizmo_was_using || state.terrain_drag_active))
        return *state.gizmo_preview;
    return state.scene.transform(id).value_or(TransformComponent{});
}

bool editor_entity_has_mesh(const EditorState& state, const EntityId& id) {
    const auto placement = state.scene.placement(id);
    if (!placement) return false;
    return state.prefab_bounds.find(normalize_asset_path(placement->prefab_asset)) != state.prefab_bounds.end();
}

PlacementScreenBounds placement_screen_bounds_for_entity(const EditorState& state, const EntityId& id,
    const TransformComponent& transform, const std::array<float, 16>& view_projection, const ViewportFrame& frame) {
    const auto source = placement_bounds_source(state, id, transform);
    return compute_placement_screen_bounds(transform, source.local_bounds, source.y_translation_offset, view_projection,
                                           frame);
}

void draw_viewport_markers(EditorState& state, const ViewportFrame& frame, const std::array<float, 16>& view_projection) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const auto draw_marker = [&](const WorldPosition& position, ImU32 color) {
        float sx = 0.0f;
        float sy = 0.0f;
        float depth = 0.0f;
        if (project_world_to_screen(view_projection, frame, static_cast<float>(position.x), static_cast<float>(position.y),
                                    static_cast<float>(position.z), sx, sy, depth)) {
            draw_list->AddCircleFilled({sx, sy}, 6.0f, color);
            draw_list->AddCircle({sx, sy}, 10.0f, color, 0, 2.0f);
        }
    };
    if (state.placement_cursor) draw_marker(*state.placement_cursor, IM_COL32(255, 196, 64, 220));
    if (state.drop_preview)
        draw_marker({state.drop_preview->position[0], state.drop_preview->position[1], state.drop_preview->position[2]},
                    IM_COL32(96, 220, 255, 220));
}

void draw_viewport_selection_overlays(EditorState& state, const ViewportFrame& frame,
    const std::array<float, 16>& view_projection) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const auto draw_entity_bounds = [&](const EntityId& id, ImU32 color, ImU32 fill_color, float thickness) {
        if (!state.scene.placement(id)) return;
        const auto bounds = placement_screen_bounds_for_entity(state, id, editor_transform_for_entity(state, id),
                                                               view_projection, frame);
        draw_placement_bounds_overlay(draw_list, bounds, color, fill_color, thickness);
    };
    if (state.hovered && (!state.selected || *state.hovered != *state.selected))
        draw_entity_bounds(*state.hovered, IM_COL32(120, 210, 255, 230), IM_COL32(120, 210, 255, 28), 1.75f);
    if (state.selected && state.scene.placement(*state.selected)) {
        const auto transform = editor_transform_for_entity(state, *state.selected);
        const auto bounds = placement_screen_bounds_for_entity(state, *state.selected, transform, view_projection, frame);
        draw_placement_bounds_overlay(draw_list, bounds, IM_COL32(255, 204, 72, 255), IM_COL32(255, 204, 72, 42), 2.5f);
        if (bounds.visible) {
            const auto name = state.scene.name(*state.selected).value_or("Entity");
            draw_list->AddText({bounds.min_x, bounds.min_y - 18.0f}, IM_COL32(255, 220, 120, 255), name.c_str());
        }
    }
    if (state.drop_preview && state.drop_preview_prefab) {
        PlacementBoundsSource source{k_proxy_bounds, 0.0f};
        const auto bounds_it = state.prefab_bounds.find(normalize_asset_path(*state.drop_preview_prefab));
        if (bounds_it != state.prefab_bounds.end()) source.local_bounds = bounds_it->second;
        const auto bounds = compute_placement_screen_bounds(*state.drop_preview, source.local_bounds,
                                                            source.y_translation_offset, view_projection, frame);
        draw_placement_bounds_overlay(draw_list, bounds, IM_COL32(96, 220, 255, 220), IM_COL32(96, 220, 255, 30), 1.75f);
    }
}

ImU32 collision_debug_color(CollisionLayer layer, bool sensor) {
    if (sensor || layer == CollisionLayer::Trigger) return IM_COL32(80, 220, 255, 210);
    if (layer == CollisionLayer::Dynamic) return IM_COL32(255, 160, 64, 210);
    return IM_COL32(96, 200, 120, 210);
}

void draw_debug_aabb(ImDrawList* draw_list, const std::array<float, 16>& view_projection, const ViewportFrame& frame,
    const WorldPosition& center, const LocalPosition& half, ImU32 color, ImU32 fill_color) {
    const float cx = static_cast<float>(center.x);
    const float cy = static_cast<float>(center.y);
    const float cz = static_cast<float>(center.z);
    const float corners[8][3] = {
        {cx - half.x, cy - half.y, cz - half.z}, {cx + half.x, cy - half.y, cz - half.z},
        {cx + half.x, cy - half.y, cz + half.z}, {cx - half.x, cy - half.y, cz + half.z},
        {cx - half.x, cy + half.y, cz - half.z}, {cx + half.x, cy + half.y, cz - half.z},
        {cx + half.x, cy + half.y, cz + half.z}, {cx - half.x, cy + half.y, cz + half.z},
    };
    float sx[8]{};
    float sy[8]{};
    float depth[8]{};
    bool visible[8]{};
    for (int i = 0; i < 8; ++i)
        visible[i] = project_world_to_screen(view_projection, frame, corners[i][0], corners[i][1], corners[i][2], sx[i],
                                             sy[i], depth[i]);
    const auto edge = [&](int a, int b) {
        if (visible[a] && visible[b]) draw_list->AddLine({sx[a], sy[a]}, {sx[b], sy[b]}, color, 1.75f);
    };
    edge(0, 1);
    edge(1, 2);
    edge(2, 3);
    edge(3, 0);
    edge(4, 5);
    edge(5, 6);
    edge(6, 7);
    edge(7, 4);
    edge(0, 4);
    edge(1, 5);
    edge(2, 6);
    edge(3, 7);
    float min_x = frame.image_max.x;
    float min_y = frame.image_max.y;
    float max_x = frame.image_min.x;
    float max_y = frame.image_min.y;
    bool any = false;
    for (int i = 0; i < 8; ++i) {
        if (!visible[i]) continue;
        any = true;
        min_x = std::min(min_x, sx[i]);
        min_y = std::min(min_y, sy[i]);
        max_x = std::max(max_x, sx[i]);
        max_y = std::max(max_y, sy[i]);
    }
    if (any && fill_color)
        draw_list->AddRectFilled({min_x, min_y}, {max_x, max_y}, fill_color);
}

void draw_debug_sphere(ImDrawList* draw_list, const std::array<float, 16>& view_projection, const ViewportFrame& frame,
    const WorldPosition& center, float radius, ImU32 color, ImU32 fill_color = IM_COL32(0, 0, 0, 0)) {
    float sx = 0.0f;
    float sy = 0.0f;
    float depth = 0.0f;
    if (!project_world_to_screen(view_projection, frame, static_cast<float>(center.x), static_cast<float>(center.y),
            static_cast<float>(center.z), sx, sy, depth))
        return;
    float edge_x = 0.0f;
    float edge_y = 0.0f;
    float edge_depth = 0.0f;
    const float screen_radius_raw =
        project_world_to_screen(view_projection, frame, static_cast<float>(center.x) + radius,
            static_cast<float>(center.y), static_cast<float>(center.z), edge_x, edge_y, edge_depth)
            ? std::hypot(edge_x - sx, edge_y - sy)
            : 8.0f;
    const float screen_radius_hi = std::max(4.0f, std::max(frame.width, frame.height) * 0.45f);
    const float screen_radius = std::clamp(screen_radius_raw, 4.0f, screen_radius_hi);
    draw_list->AddCircle({sx, sy}, screen_radius, color, 0, 2.0f);
    if (fill_color) draw_list->AddCircleFilled({sx, sy}, screen_radius, fill_color);
}

LocalPosition scale_debug_half_extent(const LocalPosition& half, const std::array<float, 3>& scale) {
    return {half.x * std::abs(scale[0]), half.y * std::abs(scale[1]), half.z * std::abs(scale[2])};
}

float scale_debug_radius(float radius, const std::array<float, 3>& scale) {
    return radius * std::max({std::abs(scale[0]), std::abs(scale[1]), std::abs(scale[2])});
}

void draw_authored_collider_volume(ImDrawList* draw_list, const std::array<float, 16>& view_projection,
    const ViewportFrame& frame, const TransformComponent& placement, const PrefabCollisionVolume& volume, ImU32 color,
    ImU32 fill_color) {
    const auto world_transform = multiply_transforms(placement, volume.transform);
    const WorldPosition position{world_transform.position[0], world_transform.position[1], world_transform.position[2]};
    if (volume.shape == PrefabCollisionShape::Sphere) {
        draw_debug_sphere(draw_list, view_projection, frame, position,
            scale_debug_radius(volume.radius, world_transform.scale), color, fill_color);
    } else {
        draw_debug_aabb(draw_list, view_projection, frame, position,
            scale_debug_half_extent(volume.half_extent, world_transform.scale), color, fill_color);
    }
}

void draw_authored_collider_overlays(EditorState& state, const ViewportFrame& frame,
    const std::array<float, 16>& view_projection) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    constexpr ImU32 k_green = IM_COL32(64, 220, 96, 235);
    constexpr ImU32 k_green_fill = IM_COL32(64, 220, 96, 28);

    if (state.selected && state.scene.contains(*state.selected)) {
        const auto transform = state.scene.transform(*state.selected);
        if (transform) {
            TransformComponent draw_transform = *transform;
            if (state.gizmo_preview && (state.gizmo_was_using || state.terrain_drag_active))
                draw_transform = *state.gizmo_preview;
            const PrefabAsset* prefab = nullptr;
            if (const auto placement = state.scene.placement(*state.selected))
                prefab = find_prefab(state.prefab_catalog, placement->prefab_asset);
            const auto authored = state.scene.authored_components(*state.selected);
            const auto volumes = effective_collision_volumes(authored ? &*authored : nullptr, prefab);
            for (const auto& volume : volumes)
                draw_authored_collider_volume(draw_list, view_projection, frame, draw_transform, volume, k_green,
                    k_green_fill);
        }
    }

    if (state.prefab_edit_path) {
        const auto normalized = normalize_asset_path(*state.prefab_edit_path);
        const auto found = state.prefab_catalog.find(normalized);
        if (found != state.prefab_catalog.end()) {
            TransformComponent preview_root;
            preview_root.position = {0.0f, 3.0f, 0.0f};
            for (const auto& volume : found->second.collision)
                draw_authored_collider_volume(draw_list, view_projection, frame, preview_root, volume, k_green,
                    k_green_fill);
        }
    }
}

void draw_collision_debug_overlays(CollisionWorld* collision, EditorState& state, const ViewportFrame& frame,
    const std::array<float, 16>& view_projection, const CharacterController* character = nullptr,
    const InteractionVolumeRegistry* interactions = nullptr, const CombatVolumeRegistry* combat = nullptr) {
    if (!collision || !state.show_collision_debug) return;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    for (const auto& body : collision->debug_bodies()) {
        const bool is_interaction = interactions && interactions->is_interaction_body(body.body);
        const bool is_combat_hit = combat && combat->is_hit_body(body.body);
        const bool is_combat_hurt = combat && combat->is_hurt_body(body.body);
        ImU32 color = collision_debug_color(body.layer, body.sensor);
        ImU32 fill = body.sensor ? IM_COL32(80, 220, 255, 24) : IM_COL32(0, 0, 0, 0);
        if (is_interaction) {
            color = IM_COL32(255, 220, 80, 230);
            fill = IM_COL32(255, 220, 80, 28);
        } else if (is_combat_hit) {
            color = IM_COL32(255, 90, 90, 230);
            fill = IM_COL32(255, 90, 90, 28);
        } else if (is_combat_hurt) {
            color = IM_COL32(255, 90, 220, 230);
            fill = IM_COL32(255, 90, 220, 28);
        }
        if (body.shape == CollisionDebugShape::Sphere) {
            draw_debug_sphere(draw_list, view_projection, frame, body.position, body.radius, color, fill);
        } else if (body.shape == CollisionDebugShape::Capsule) {
            const LocalPosition half{body.radius, body.half_extent.x + body.radius, body.radius};
            draw_debug_aabb(draw_list, view_projection, frame, body.position, half, color, fill);
        } else {
            draw_debug_aabb(draw_list, view_projection, frame, body.position, body.half_extent, color, fill);
        }
    }
    for (const auto& point : state.recent_contact_points) {
        float sx = 0.0f;
        float sy = 0.0f;
        float depth = 0.0f;
        if (!project_world_to_screen(view_projection, frame, static_cast<float>(point.x), static_cast<float>(point.y),
                                     static_cast<float>(point.z), sx, sy, depth))
            continue;
        draw_list->AddLine({sx - 5.0f, sy}, {sx + 5.0f, sy}, IM_COL32(255, 96, 96, 230), 2.0f);
        draw_list->AddLine({sx, sy - 5.0f}, {sx, sy + 5.0f}, IM_COL32(255, 96, 96, 230), 2.0f);
    }
    if (character) {
        const auto body = character->debug_body();
        const ImU32 color = collision_debug_color(body.layer, body.sensor);
        const LocalPosition half{body.radius, body.half_extent.x + body.radius, body.radius};
        draw_debug_aabb(draw_list, view_projection, frame, body.position, half, color, IM_COL32(0, 0, 0, 0));
    }
}

void ensure_world_forge_map_loaded_for_overlay(EditorState& state) {
    auto& session = state.world_forge_editor;
    if (session.loaded || state.project_root.empty()) return;
    if (const auto result = session.reload(state.project_root); !result)
        state.status = "World Forge load for markers failed: " + result.error().message;
}

std::optional<std::array<float, 3>> selected_world_forge_marker_world(const WorldForgeEditorSession& session) {
    if (session.selected_id.empty()) return std::nullopt;
    for (const auto& region : session.map.regions) {
        if (region.id != session.selected_id || !region.anchor) continue;
        return std::array<float, 3>{region.anchor->x, region.anchor->y, region.anchor->z};
    }
    for (const auto& poi : session.map.pois) {
        if (poi.id != session.selected_id || !poi.anchor) continue;
        return std::array<float, 3>{poi.anchor->x, poi.anchor->y, poi.anchor->z};
    }
    return std::nullopt;
}

void draw_world_forge_map_marker_overlays(EditorState& state, const ViewportFrame& frame,
    const std::array<float, 16>& view_projection) {
    if (!state.show_world_forge_map_markers) return;
    if (state.active_viewport_tab != EditorState::ViewportTab::Scene &&
        state.active_viewport_tab != EditorState::ViewportTab::Sculpt)
        return;
    ensure_world_forge_map_loaded_for_overlay(state);
    const auto& session = state.world_forge_editor;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    constexpr float k_pole_height = 4.0f;

    auto draw_marker = [&](float x, float y, float z, bool is_poi, bool selected, const std::string& label) {
        float base_sx = 0.0f;
        float base_sy = 0.0f;
        float top_sx = 0.0f;
        float top_sy = 0.0f;
        float depth = 0.0f;
        const bool base_ok =
            project_world_to_screen(view_projection, frame, x, y, z, base_sx, base_sy, depth);
        const bool top_ok = project_world_to_screen(view_projection, frame, x, y + k_pole_height, z, top_sx, top_sy,
            depth);
        if (!base_ok && !top_ok) return;
        const ImU32 fill = selected ? IM_COL32(245, 185, 55, 255)
                                    : (is_poi ? IM_COL32(210, 150, 70, 255) : IM_COL32(70, 170, 120, 255));
        const ImU32 outline = IM_COL32(255, 255, 255, 220);
        if (base_ok && top_ok) {
            draw_list->AddLine({base_sx, base_sy}, {top_sx, top_sy}, IM_COL32(0, 0, 0, 140), 4.0f);
            draw_list->AddLine({base_sx, base_sy}, {top_sx, top_sy}, fill, 2.0f);
        }
        const float hx = top_ok ? top_sx : base_sx;
        const float hy = top_ok ? top_sy : base_sy;
        if (is_poi) {
            draw_list->AddCircleFilled({hx, hy}, selected ? 8.0f : 6.5f, IM_COL32(0, 0, 0, 120));
            draw_list->AddCircleFilled({hx, hy}, selected ? 7.0f : 5.5f, fill);
            draw_list->AddCircle({hx, hy}, selected ? 7.0f : 5.5f, outline, 0, 1.5f);
        } else {
            const float half = selected ? 7.0f : 5.5f;
            draw_list->AddRectFilled({hx - half + 1.0f, hy - half + 2.0f}, {hx + half + 1.0f, hy + half + 2.0f},
                IM_COL32(0, 0, 0, 120), 2.0f);
            draw_list->AddRectFilled({hx - half, hy - half}, {hx + half, hy + half}, fill, 2.0f);
            draw_list->AddRect({hx - half, hy - half}, {hx + half, hy + half}, outline, 2.0f, 0, 1.5f);
        }
        if (!label.empty()) {
            const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
            const ImVec2 text_pos{hx - text_size.x * 0.5f, hy - text_size.y - 12.0f};
            draw_list->AddRectFilled({text_pos.x - 3.0f, text_pos.y - 1.0f},
                {text_pos.x + text_size.x + 3.0f, text_pos.y + text_size.y + 1.0f}, IM_COL32(8, 10, 14, 200), 3.0f);
            draw_list->AddText(text_pos, IM_COL32(245, 245, 248, 255), label.c_str());
        }
    };

    for (const auto& region : session.map.regions) {
        if (!region.anchor) continue;
        if (!matches_world_forge_act_filter(region.acts, region.tags, session.act_filter)) continue;
        const std::string label = region.display_name.empty() ? region.id : region.display_name;
        draw_marker(region.anchor->x, region.anchor->y, region.anchor->z, false, session.selected_id == region.id,
            label);
    }
    for (const auto& poi : session.map.pois) {
        if (!poi.anchor) continue;
        if (!matches_world_forge_act_filter(poi.acts, poi.tags, session.act_filter)) continue;
        const std::string label = poi.display_name.empty() ? poi.id : poi.display_name;
        draw_marker(poi.anchor->x, poi.anchor->y, poi.anchor->z, true, session.selected_id == poi.id, label);
    }
}

void apply_pending_world_forge_marker_focus(EditorState& state, DebugCamera& camera) {
    if (!state.request_focus_world_forge_marker) return;
    state.request_focus_world_forge_marker = false;
    ensure_world_forge_map_loaded_for_overlay(state);
    const auto target = selected_world_forge_marker_world(state.world_forge_editor);
    if (!target) {
        state.status = "Select an anchored World Forge region/POI to focus";
        return;
    }
    constexpr float k_distance = 28.0f;
    constexpr float k_height = 16.0f;
    const float tx = (*target)[0];
    const float ty = (*target)[1];
    const float tz = (*target)[2];
    const float cam_x = tx;
    const float cam_y = ty + k_height;
    const float cam_z = tz - k_distance;
    const float dx = tx - cam_x;
    const float dy = ty - cam_y;
    const float dz = tz - cam_z;
    const float yaw = std::atan2(dx, dz);
    const float horiz = std::sqrt(dx * dx + dz * dz);
    const float pitch = std::atan2(dy, (std::max)(horiz, 0.001f));
    camera.set_pose({cam_x, cam_y, cam_z}, yaw, pitch);
    state.status = "Focused Scene camera on World Forge marker";
}

std::string asset_browser_prefix(const std::string& folder) {
    return folder.empty() ? "assets/" : "assets/" + normalize_asset_path(folder) + "/";
}

bool is_direct_asset_child(const std::string& asset_path, const std::string& folder) {
    const auto normalized = normalize_asset_path(asset_path);
    const auto prefix = asset_browser_prefix(folder);
    if (normalized.size() <= prefix.size() || normalized.rfind(prefix, 0) != 0) return false;
    return normalized.find('/', prefix.size()) == std::string::npos;
}

std::string asset_entry_name(const std::string& path) {
    const auto normalized = normalize_asset_path(path);
    const auto slash = normalized.rfind('/');
    return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

std::string asset_folder_display_name(const std::string& folder) {
    const auto normalized = normalize_asset_path(folder);
    const auto slash = normalized.rfind('/');
    return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

bool is_valid_asset_folder_name(const std::string& name) {
    if (name.empty() || name == "." || name == "..") return false;
    static constexpr std::string_view invalid = "<>:\"/\\|?*";
    for (const char character : name) {
        if (invalid.find(character) != std::string_view::npos) return false;
        if (static_cast<unsigned char>(character) < 32) return false;
    }
    return true;
}

ImU32 asset_thumbnail_color(const std::array<float, 3>& color, int alpha = 255) {
    return IM_COL32(static_cast<int>(std::clamp(color[0], 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(color[1], 0.0f, 1.0f) * 255.0f),
                    static_cast<int>(std::clamp(color[2], 0.0f, 1.0f) * 255.0f), alpha);
}

void draw_prefab_asset_thumbnail(const EditorState& state, const std::string& prefab_path) {
    constexpr ImVec2 size{28.0f, 28.0f};
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max{min.x + size.x, min.y + size.y};
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::Dummy(size);

    draw_list->AddRectFilled(min, max, IM_COL32(28, 31, 38, 255), 4.0f);
    draw_list->AddRect(min, max, IM_COL32(84, 92, 108, 255), 4.0f);

    const auto normalized = normalize_asset_path(prefab_path);
    const auto prefab_it = state.prefab_catalog.find(normalized);
    if (prefab_it == state.prefab_catalog.end()) {
        draw_list->AddRectFilled({min.x + 8.0f, min.y + 9.0f}, {min.x + 20.0f, min.y + 20.0f},
                                 IM_COL32(145, 154, 172, 230), 2.0f);
        draw_list->AddLine({min.x + 8.0f, min.y + 9.0f}, {min.x + 14.0f, min.y + 5.0f},
                           IM_COL32(180, 188, 204, 230), 1.0f);
        draw_list->AddLine({min.x + 20.0f, min.y + 9.0f}, {min.x + 14.0f, min.y + 5.0f},
                           IM_COL32(180, 188, 204, 230), 1.0f);
        return;
    }

    const auto& prefab = prefab_it->second;
    if (!prefab.is_compositional()) {
        draw_list->AddRectFilled({min.x + 8.0f, min.y + 8.0f}, {min.x + 20.0f, min.y + 20.0f},
                                 IM_COL32(106, 140, 210, 230), 2.0f);
        draw_list->AddLine({min.x + 8.0f, min.y + 8.0f}, {min.x + 14.0f, min.y + 4.0f},
                           IM_COL32(150, 180, 235, 230), 1.0f);
        draw_list->AddLine({min.x + 20.0f, min.y + 8.0f}, {min.x + 14.0f, min.y + 4.0f},
                           IM_COL32(150, 180, 235, 230), 1.0f);
        return;
    }

    MeshBounds bounds = k_proxy_bounds;
    if (const auto found = state.prefab_bounds.find(normalized); found != state.prefab_bounds.end()) bounds = found->second;
    const float span_x = std::max(0.1f, bounds.max_x - bounds.min_x);
    const float span_y = std::max(0.1f, bounds.max_y - bounds.min_y);
    const float scale = 19.0f / std::max(span_x, span_y);
    const ImVec2 origin{min.x + size.x * 0.5f - (bounds.min_x + bounds.max_x) * 0.5f * scale,
                        max.y - 4.0f + bounds.min_y * scale};

    std::size_t drawn_parts = 0;
    for (const auto& part : prefab.parts) {
        if (++drawn_parts > 18) break;
        const auto color = asset_thumbnail_color(part.mesh.color, 230);
        const float x = origin.x + part.transform.position[0] * scale;
        const float y = origin.y - part.transform.position[1] * scale;
        const float w = std::max(3.0f, std::abs(part.transform.scale[0]) * scale * 0.5f);
        const float h = std::max(3.0f, std::abs(part.transform.scale[1]) * scale * 0.5f);
        const std::string primitive = part.mesh.primitive.value_or("");
        if (primitive == "sphere") {
            draw_list->AddCircleFilled({x, y}, std::max(w, h) * 0.65f, color, 16);
        } else if (primitive == "pyramid") {
            draw_list->AddTriangleFilled({x, y - h}, {x - w, y + h * 0.8f}, {x + w, y + h * 0.8f}, color);
        } else {
            draw_list->AddRectFilled({x - w, y - h}, {x + w, y + h}, color, 2.0f);
        }
    }
}

void draw_folder_asset_icon() {
    constexpr ImVec2 size{22.0f, 18.0f};
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max{min.x + size.x, min.y + size.y};
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::Dummy({28.0f, 22.0f});

    const ImU32 tab = IM_COL32(225, 178, 70, 255);
    const ImU32 body = IM_COL32(210, 154, 48, 255);
    const ImU32 edge = IM_COL32(110, 82, 36, 255);
    draw_list->AddRectFilled({min.x + 2.0f, min.y + 3.0f}, {min.x + 10.0f, min.y + 8.0f}, tab, 2.0f);
    draw_list->AddRectFilled({min.x + 2.0f, min.y + 6.0f}, {max.x, max.y}, body, 3.0f);
    draw_list->AddRect({min.x + 2.0f, min.y + 6.0f}, {max.x, max.y}, edge, 3.0f);
}

void draw_file_asset_icon(ImU32 accent) {
    constexpr ImVec2 size{22.0f, 22.0f};
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max{min.x + size.x, min.y + size.y};
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImGui::Dummy({28.0f, 24.0f});

    draw_list->AddRectFilled({min.x + 4.0f, min.y + 2.0f}, {max.x - 2.0f, max.y - 2.0f},
                             IM_COL32(36, 40, 48, 255), 2.0f);
    draw_list->AddTriangleFilled({max.x - 8.0f, min.y + 2.0f}, {max.x - 2.0f, min.y + 2.0f},
                                 {max.x - 2.0f, min.y + 8.0f}, IM_COL32(66, 74, 88, 255));
    draw_list->AddRect({min.x + 4.0f, min.y + 2.0f}, {max.x - 2.0f, max.y - 2.0f},
                       IM_COL32(88, 98, 116, 255), 2.0f);
    draw_list->AddCircleFilled({min.x + 13.0f, min.y + 14.0f}, 4.0f, accent, 12);
}

std::vector<std::string> list_immediate_asset_subfolders(const std::filesystem::path& project_root,
    const std::string& folder) {
    std::vector<std::string> subfolders;
    const auto directory = folder.empty() ? project_root / "assets" : project_root / "assets" / folder;
    if (!std::filesystem::exists(directory)) return subfolders;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_directory()) continue;
        const auto name = entry.path().filename().string();
        subfolders.push_back(folder.empty() ? name : folder + "/" + name);
    }
    std::sort(subfolders.begin(), subfolders.end());
    return subfolders;
}

template <typename Value>
void remap_string_map_keys(std::map<std::string, Value>& entries, const std::string& old_prefix,
    const std::string& new_prefix) {
    auto remap_path = [](std::string path, const std::string& old_path, const std::string& new_path) {
        const auto normalized = normalize_asset_path(path);
        const auto old_norm = normalize_asset_path(old_path);
        const auto new_norm = normalize_asset_path(new_path);
        if (normalized == old_norm) return new_norm;
        const std::string old_slash = old_norm + "/";
        if (normalized.rfind(old_slash, 0) == 0) return new_norm + normalized.substr(old_norm.size());
        return path;
    };
    std::map<std::string, Value> remapped;
    for (auto& entry : entries)
        remapped[remap_path(entry.first, old_prefix, new_prefix)] = std::move(entry.second);
    entries = std::move(remapped);
}

Result<void> refresh_editor_assets(EditorState& state) {
    if (const auto scanned = state.assets.scan(state.project_root); !scanned) return scanned;
    state.prefab_catalog.clear();
    state.prefab_bounds.clear();
    return load_prefab_catalog(state, state.project_root);
}

void remap_editor_asset_references(EditorState& state, const std::string& old_prefix, const std::string& new_prefix) {
    auto remap_path = [](std::string path, const std::string& old_path, const std::string& new_path) {
        const auto normalized = normalize_asset_path(path);
        const auto old_norm = normalize_asset_path(old_path);
        const auto new_norm = normalize_asset_path(new_path);
        if (normalized == old_norm) return new_norm;
        const std::string old_slash = old_norm + "/";
        if (normalized.rfind(old_slash, 0) == 0) return new_norm + normalized.substr(old_norm.size());
        return path;
    };
    (void)state.scene.remap_asset_path_prefix(old_prefix, new_prefix);
    state.play_session.character_asset = remap_path(state.play_session.character_asset, old_prefix, new_prefix);
    state.play_session.camera_asset = remap_path(state.play_session.camera_asset, old_prefix, new_prefix);
    state.play_session_dirty = true;
    if (state.inspector_asset_path)
        state.inspector_asset_path = remap_path(*state.inspector_asset_path, old_prefix, new_prefix);
    if (state.prefab_edit_path)
        state.prefab_edit_path = remap_path(*state.prefab_edit_path, old_prefix, new_prefix);
    state.character_asset.visual_prefab = remap_path(state.character_asset.visual_prefab, old_prefix, new_prefix);
    remap_string_map_keys(state.prefab_catalog, old_prefix, new_prefix);
    remap_string_map_keys(state.prefab_bounds, old_prefix, new_prefix);
    remap_string_map_keys(state.mesh_bounds, old_prefix, new_prefix);
}

Result<void> create_asset_subfolder(EditorState& state, const std::string& name) {
    if (!is_valid_asset_folder_name(name))
        return Result<void>::failure(graphics_error("EDITOR-ASSET-FOLDER-INVALID", "Invalid folder name: " + name));
    const auto relative = state.asset_browser_folder.empty() ? name : state.asset_browser_folder + "/" + name;
    const auto absolute = state.project_root / "assets" / relative;
    if (std::filesystem::exists(absolute))
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-FOLDER-EXISTS", "Folder already exists: assets/" + relative));
    std::error_code error;
    if (!std::filesystem::create_directories(absolute, error))
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-FOLDER-CREATE", "Could not create folder: assets/" + relative));
    return refresh_editor_assets(state);
}

Result<void> rename_asset_folder(EditorState& state, const std::string& folder, const std::string& new_name) {
    if (!is_valid_asset_folder_name(new_name))
        return Result<void>::failure(graphics_error("EDITOR-ASSET-FOLDER-INVALID", "Invalid folder name: " + new_name));
    const auto parent = folder.find('/') == std::string::npos
                            ? std::string{}
                            : folder.substr(0, folder.rfind('/'));
    const auto renamed_relative = parent.empty() ? new_name : parent + "/" + new_name;
    if (folder == renamed_relative)
        return Result<void>::success();
    const auto old_absolute = state.project_root / "assets" / folder;
    const auto new_absolute = state.project_root / "assets" / renamed_relative;
    if (!std::filesystem::exists(old_absolute))
        return Result<void>::failure(graphics_error("EDITOR-ASSET-FOLDER-MISSING", "Folder not found: assets/" + folder));
    if (std::filesystem::exists(new_absolute))
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-FOLDER-EXISTS", "Folder already exists: assets/" + renamed_relative));
    std::error_code error;
    std::filesystem::rename(old_absolute, new_absolute, error);
    if (error)
        return Result<void>::failure(
            graphics_error("EDITOR-ASSET-FOLDER-RENAME", "Could not rename folder: " + error.message()));
    const auto old_prefix = "assets/" + normalize_asset_path(folder);
    const auto new_prefix = "assets/" + normalize_asset_path(renamed_relative);
    remap_editor_asset_references(state, old_prefix, new_prefix);
    if (state.asset_browser_folder == folder)
        state.asset_browser_folder = renamed_relative;
    else if (state.asset_browser_folder.rfind(folder + "/", 0) == 0)
        state.asset_browser_folder = renamed_relative + state.asset_browser_folder.substr(folder.size());
    if (state.asset_browser_selected_folder) {
        if (*state.asset_browser_selected_folder == folder)
            state.asset_browser_selected_folder = renamed_relative;
        else if (state.asset_browser_selected_folder->rfind(folder + "/", 0) == 0)
            state.asset_browser_selected_folder =
                renamed_relative + state.asset_browser_selected_folder->substr(folder.size());
    }
    if (const auto refreshed = refresh_editor_assets(state); !refreshed) return refreshed;
    return Result<void>::success();
}

Result<std::string> move_asset_file_to_folder(EditorState& state, const std::string& asset_path,
    const std::string& destination_folder) {
    const auto normalized = normalize_asset_path(asset_path);
    if (normalized.rfind("assets/", 0) != 0 || normalized.find('/', std::string("assets/").size()) == std::string::npos)
        return Result<std::string>::failure(
            graphics_error("EDITOR-ASSET-MOVE-INVALID", "Asset moves require a file path under assets/: " + asset_path));
    const auto name = asset_entry_name(normalized);
    const auto destination = normalize_asset_path(destination_folder);
    const auto destination_prefix = asset_browser_prefix(destination);
    if (normalized.rfind(destination_prefix, 0) == 0 &&
        normalized.find('/', destination_prefix.size()) == std::string::npos)
        return Result<std::string>::success(normalized);
    const auto moved_path = destination_prefix + name;
    if (moved_path == normalized) return Result<std::string>::success(normalized);

    const auto source_absolute = state.project_root / normalized;
    const auto destination_absolute = state.project_root / moved_path;
    if (!std::filesystem::exists(source_absolute))
        return Result<std::string>::failure(
            graphics_error("EDITOR-ASSET-MOVE-MISSING", "Asset not found: " + normalized));
    if (std::filesystem::exists(destination_absolute))
        return Result<std::string>::failure(
            graphics_error("EDITOR-ASSET-MOVE-EXISTS", "Asset already exists: " + moved_path));

    std::error_code error;
    std::filesystem::create_directories(destination_absolute.parent_path(), error);
    if (error)
        return Result<std::string>::failure(
            graphics_error("EDITOR-ASSET-MOVE-DIRECTORY", "Could not create destination folder: " + error.message()));
    std::filesystem::rename(source_absolute, destination_absolute, error);
    if (error)
        return Result<std::string>::failure(
            graphics_error("EDITOR-ASSET-MOVE", "Could not move asset: " + error.message()));

    const auto source_meta = source_absolute.string() + ".meta";
    const auto destination_meta = destination_absolute.string() + ".meta";
    if (std::filesystem::exists(source_meta)) {
        std::filesystem::rename(source_meta, destination_meta, error);
        if (error)
            return Result<std::string>::failure(
                graphics_error("EDITOR-ASSET-MOVE-META", "Asset moved but sidecar metadata could not move: " + error.message()));
    }

    remap_editor_asset_references(state, normalized, moved_path);
    if (const auto refreshed = refresh_editor_assets(state); !refreshed) return Result<std::string>::failure(refreshed.error());
    return Result<std::string>::success(moved_path);
}

void handle_asset_drop_to_folder(EditorState& state, const std::string& destination_folder) {
    const auto accept_payload = [&](const char* payload_type) -> const ImGuiPayload* {
        return ImGui::AcceptDragDropPayload(payload_type);
    };
    const ImGuiPayload* payload = accept_payload(k_asset_file_drag_payload);
    if (!payload) payload = accept_payload(k_prefab_drag_payload);
    if (!payload || !payload->Data) return;
    const std::string source(static_cast<const char*>(payload->Data));
    const auto moved = move_asset_file_to_folder(state, source, destination_folder);
    if (moved) {
        state.status = "Moved " + asset_entry_name(source) + " to " + asset_browser_prefix(destination_folder);
    } else {
        state.status = moved.error().message;
        Logger::instance().write(moved.error());
    }
}

void begin_asset_file_drag_source(const std::string& normalized_path, const std::string& display_name, bool prefab_payload) {
    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;
    if (prefab_payload)
        ImGui::SetDragDropPayload(k_prefab_drag_payload, normalized_path.c_str(), normalized_path.size() + 1);
    else
        ImGui::SetDragDropPayload(k_asset_file_drag_payload, normalized_path.c_str(), normalized_path.size() + 1);
    ImGui::Text("Move %s", display_name.c_str());
    ImGui::TextDisabled("%s", normalized_path.c_str());
    ImGui::EndDragDropSource();
}

void draw_sculpt_toolbar(EditorState& state, StreamedTerrainField* streamed_terrain, StreamedFoliageField* streamed_foliage,
    StreamedWaterField* streamed_water, CollisionWorld* collision, MaterialAsset* terrain_material,
    const std::array<float, 3>& camera_position) {
    if (ImGui::RadioButton("Sculpt##tool", state.sculpt_tool == EditorState::SculptTool::Height)) {
        commit_active_terrain_stroke(state);
        state.sculpt_tool = EditorState::SculptTool::Height;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Flatten##tool", state.sculpt_tool == EditorState::SculptTool::Flatten)) {
        commit_active_terrain_stroke(state);
        state.sculpt_tool = EditorState::SculptTool::Flatten;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Paint##tool", state.sculpt_tool == EditorState::SculptTool::Paint)) {
        commit_active_terrain_stroke(state);
        state.sculpt_tool = EditorState::SculptTool::Paint;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Foliage##tool", state.sculpt_tool == EditorState::SculptTool::Foliage)) {
        commit_active_terrain_stroke(state);
        state.sculpt_tool = EditorState::SculptTool::Foliage;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Water##tool", state.sculpt_tool == EditorState::SculptTool::Water)) {
        commit_active_terrain_stroke(state);
        state.sculpt_tool = EditorState::SculptTool::Water;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(88.0f);
    if (ImGui::InputFloat("Radius##sculpt", &state.terrain_brush_radius, 0.1f, 0.5f, "%.1f m"))
        state.terrain_brush_radius = std::max(0.25f, state.terrain_brush_radius);

    if (state.sculpt_tool == EditorState::SculptTool::Height ||
        state.sculpt_tool == EditorState::SculptTool::Flatten) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88.0f);
        if (ImGui::InputFloat("Strength##sculpt", &state.terrain_brush_strength, 0.01f, 0.05f, "%.3f m"))
            state.terrain_brush_strength = std::max(0.01f, state.terrain_brush_strength);
        ImGui::SameLine();
        if (state.sculpt_tool == EditorState::SculptTool::Flatten)
            ImGui::TextDisabled("Flatten: drag (locks height on click)");
        else
            ImGui::TextDisabled("Raise: drag | Lower: Shift+drag");
        ImGui::SameLine();
        ImGui::BeginDisabled(state.terrain_history.undo_size() == 0);
        if (editor_icon_button("sculpt_undo", ICON_FA_UNDO, "Undo stroke (Ctrl+Z)")) {
            const auto result = state.terrain_history.undo(state.terrain_edits);
            state.status = result ? state.terrain_history.last_summary() : result.error().message;
            if (result) reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state.terrain_history.redo_size() == 0);
        if (editor_icon_button("sculpt_redo", ICON_FA_REDO, "Redo stroke (Ctrl+Y)")) {
            const auto result = state.terrain_history.redo(state.terrain_edits);
            state.status = result ? state.terrain_history.last_summary() : result.error().message;
            if (result) reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material);
        }
        ImGui::EndDisabled();
        if (state.terrain_edits_dirty) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Unsaved sculpt (Ctrl+S)");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| Sculpt cells: %zu", state.terrain_edits.cell_coordinates().size());
    } else if (state.sculpt_tool == EditorState::SculptTool::Paint) {
        const auto material_paths = collect_asset_paths(state, ".material.json");
        if (state.terrain_paint_brush_material.empty() && !material_paths.empty())
            state.terrain_paint_brush_material = material_paths.front();
        ImGui::SameLine();
        if (draw_asset_path_combo("Paint Material##brush", state.terrain_paint_brush_material, material_paths)) {
            (void)state.terrain_paint.ensure_material_index(state.terrain_paint_brush_material);
        }
        if (const MaterialAsset* brush_material = editor_material(state, state.terrain_paint_brush_material)) {
            ImGui::SameLine();
            ImGui::ColorButton("##paint_brush_preview",
                ImVec4{brush_material->base_color[0], brush_material->base_color[1], brush_material->base_color[2],
                    brush_material->base_color[3]});
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Edit##paint_brush_mat")) {
            state.inspector_asset_path = normalize_asset_path(state.terrain_paint_brush_material);
            if (const MaterialAsset* brush_material = editor_material(state, state.terrain_paint_brush_material)) {
                state.material_asset = *brush_material;
                state.material_asset_dirty = false;
            }
            state.selected.reset();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Paint: drag on terrain");
        ImGui::SameLine();
        ImGui::BeginDisabled(state.terrain_paint_history.undo_size() == 0);
        if (editor_icon_button("paint_undo", ICON_FA_UNDO, "Undo paint stroke (Ctrl+Z)")) {
            const auto result = state.terrain_paint_history.undo(state.terrain_paint);
            state.status = result ? state.terrain_paint_history.last_summary() : result.error().message;
            if (result) reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state.terrain_paint_history.redo_size() == 0);
        if (editor_icon_button("paint_redo", ICON_FA_REDO, "Redo paint stroke (Ctrl+Y)")) {
            const auto result = state.terrain_paint_history.redo(state.terrain_paint);
            state.status = result ? state.terrain_paint_history.last_summary() : result.error().message;
            if (result) reload_loaded_terrain_cells(state, streamed_terrain, collision, terrain_material, false);
        }
        ImGui::EndDisabled();
        if (state.terrain_paint_dirty) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Unsaved paint (Ctrl+S)");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| Painted cells: %zu", state.terrain_paint.cell_coordinates().size());
    } else if (state.sculpt_tool == EditorState::SculptTool::Water) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88.0f);
        float sea_level = state.water_store.sea_level();
        if (ImGui::InputFloat("Sea Level##water", &sea_level, 0.1f, 0.5f, "%.2f m")) {
            state.water_store.set_sea_level(sea_level);
            state.water_dirty = true;
            reload_loaded_water_cells(state, streamed_water);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Place: drag | Erase: Shift+drag");
        ImGui::SameLine();
        ImGui::BeginDisabled(state.water_history.undo_size() == 0);
        if (editor_icon_button("water_undo", ICON_FA_UNDO, "Undo water stroke (Ctrl+Z)")) {
            const auto result = state.water_history.undo(state.water_store);
            state.status = result ? state.water_history.last_summary() : result.error().message;
            if (result) {
                state.water_dirty = true;
                reload_loaded_water_cells(state, streamed_water);
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state.water_history.redo_size() == 0);
        if (editor_icon_button("water_redo", ICON_FA_REDO, "Redo water stroke (Ctrl+Y)")) {
            const auto result = state.water_history.redo(state.water_store);
            state.status = result ? state.water_history.last_summary() : result.error().message;
            if (result) {
                state.water_dirty = true;
                reload_loaded_water_cells(state, streamed_water);
            }
        }
        ImGui::EndDisabled();
        if (state.water_dirty) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Unsaved water (Ctrl+S)");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| Water cells: %zu", state.water_store.cell_coordinates().size());
    } else {
        if (ImGui::RadioButton("Paint##foliage_mode", state.foliage_brush_mode == EditorState::FoliageBrushMode::Paint))
            state.foliage_brush_mode = EditorState::FoliageBrushMode::Paint;
        ImGui::SameLine();
        if (ImGui::RadioButton("Erase##foliage_mode", state.foliage_brush_mode == EditorState::FoliageBrushMode::Erase))
            state.foliage_brush_mode = EditorState::FoliageBrushMode::Erase;
        ImGui::SameLine();
        if (ImGui::RadioButton("Mixed##foliage_mode", state.foliage_brush_mode == EditorState::FoliageBrushMode::Mixed))
            state.foliage_brush_mode = EditorState::FoliageBrushMode::Mixed;
        ImGui::SameLine();
        if (!state.foliage_layers.layers.empty() &&
            state.foliage_brush_layer >= state.foliage_layers.layers.size())
            state.foliage_brush_layer = 0;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(88.0f);
        if (ImGui::InputFloat("Strength##foliage", &state.foliage_brush_strength, 0.01f, 0.05f, "%.3f"))
            state.foliage_brush_strength = std::clamp(state.foliage_brush_strength, 0.01f, 1.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(96.0f);
        if (ImGui::BeginCombo("Density##foliage", state.foliage_density_preset == 0 ? "Sparse"
                : state.foliage_density_preset == 2 ? "Dense" : "Medium")) {
            if (ImGui::Selectable("Sparse", state.foliage_density_preset == 0)) {
                state.foliage_density_preset = 0;
                state.foliage_brush_strength = 0.12f;
            }
            if (ImGui::Selectable("Medium", state.foliage_density_preset == 1)) {
                state.foliage_density_preset = 1;
                state.foliage_brush_strength = 0.28f;
            }
            if (ImGui::Selectable("Dense", state.foliage_density_preset == 2)) {
                state.foliage_density_preset = 2;
                state.foliage_brush_strength = 0.55f;
            }
            ImGui::EndCombo();
        }
        if (!state.foliage_layers.layers.empty() &&
            state.foliage_brush_mode == EditorState::FoliageBrushMode::Paint) {
            if (ImGui::BeginCombo("Layer##foliage",
                    state.foliage_layers.layers[state.foliage_brush_layer].label.c_str())) {
                for (std::uint8_t index = 0; index < state.foliage_layers.layers.size(); ++index) {
                    const bool selected = state.foliage_brush_layer == index;
                    if (ImGui::Selectable(state.foliage_layers.layers[index].label.c_str(), selected))
                        state.foliage_brush_layer = index;
                }
                ImGui::EndCombo();
            }
            const auto& layer = state.foliage_layers.layers[state.foliage_brush_layer];
            ImGui::SameLine();
            ImGui::ColorButton("##foliage_layer_preview",
                ImVec4{layer.color[0], layer.color[1], layer.color[2], 1.0f});
        } else if (state.foliage_brush_mode == EditorState::FoliageBrushMode::Mixed) {
            ImGui::TextDisabled("Mix: grass + flowers + bushes");
        } else {
            ImGui::TextDisabled("Removes all foliage layers");
        }
        if (state.foliage_layers.layers.empty()) {
            ImGui::TextDisabled("No foliage layers loaded");
        }
        ImGui::SameLine();
        if (!state.foliage_layers.layers.empty()) {
            if (state.foliage_brush_mode == EditorState::FoliageBrushMode::Erase)
                ImGui::TextDisabled("Drag to remove foliage");
            else if (state.foliage_brush_mode == EditorState::FoliageBrushMode::Mixed)
                ImGui::TextDisabled("Natural meadow mix | Shift+drag erases");
            else if (state.foliage_layers.layers[state.foliage_brush_layer].scatter_mode == FoliageScatterMode::Discrete)
                ImGui::TextDisabled("Discrete layer | Shift+drag erases");
            else
                ImGui::TextDisabled("Drag to paint | Shift+drag erases | Bend in play test");
        } else {
            ImGui::TextDisabled("Drag to paint | Shift+drag erases");
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(state.foliage_density_history.undo_size() == 0);
        if (editor_icon_button("foliage_undo", ICON_FA_UNDO, "Undo foliage stroke (Ctrl+Z)")) {
            const auto result = state.foliage_density_history.undo(state.foliage_density);
            state.status = result ? state.foliage_density_history.last_summary() : result.error().message;
            if (result && streamed_foliage && streamed_terrain) {
                (void)streamed_foliage->rebuild_cells(streamed_terrain->loaded_cell_coordinates(), camera_position);
                state.foliage_density_dirty = true;
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(state.foliage_density_history.redo_size() == 0);
        if (editor_icon_button("foliage_redo", ICON_FA_REDO, "Redo foliage stroke (Ctrl+Y)")) {
            const auto result = state.foliage_density_history.redo(state.foliage_density);
            state.status = result ? state.foliage_density_history.last_summary() : result.error().message;
            if (result && streamed_foliage && streamed_terrain) {
                (void)streamed_foliage->rebuild_cells(streamed_terrain->loaded_cell_coordinates(), camera_position);
                state.foliage_density_dirty = true;
            }
        }
        ImGui::EndDisabled();
        if (state.foliage_density_dirty) {
            ImGui::SameLine();
            ImGui::TextColored({1.0f, 0.8f, 0.2f, 1.0f}, "Unsaved foliage (Ctrl+S)");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("| Foliage cells: %zu", state.foliage_density.cell_coordinates().size());
    }

    ImGui::Separator();
    ImGui::TextDisabled("Global Terrain Tint");
    const auto material_paths = collect_asset_paths(state, ".material.json");
    if (draw_asset_path_combo("Material##terrain", state.terrain_material_path, material_paths)) {
        apply_active_terrain_material(state, terrain_material, streamed_terrain, collision);
        state.status = "Terrain tint material updated";
    }
    ImGui::SameLine();
    if (terrain_material) {
        ImGui::ColorButton("##terrain_mat_preview", ImVec4{terrain_material->base_color[0],
            terrain_material->base_color[1], terrain_material->base_color[2], terrain_material->base_color[3]});
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Edit Tint##terrain")) {
        state.inspector_asset_path = normalize_asset_path(state.terrain_material_path);
        if (const auto loaded = MaterialAsset::load(state.project_root / state.terrain_material_path)) {
            state.material_asset = loaded.value();
            state.material_asset_dirty = false;
        }
        state.selected.reset();
        state.status = "Inspecting terrain tint material";
    }
    ImGui::TextDisabled("Tint multiplies procedural terrain colors only. Painted samples keep their material colors.");
}

void draw_asset_browser(EditorState& state) {
    ImGui::TextDisabled("Drag prefabs into the viewport, or click terrain then Place.");
    if (state.placement_cursor)
        ImGui::Text("Place at: %.1f, %.1f, %.1f", state.placement_cursor->x, state.placement_cursor->y,
                    state.placement_cursor->z);
    else
        ImGui::TextDisabled("Click terrain to set placement point");

    ImGui::BeginDisabled(state.asset_browser_folder.empty());
    if (ImGui::SmallButton("Up") && !state.asset_browser_folder.empty()) {
        const auto slash = state.asset_browser_folder.rfind('/');
        state.asset_browser_folder = slash == std::string::npos ? std::string{} : state.asset_browser_folder.substr(0, slash);
        state.asset_browser_selected_folder.reset();
    }
    ImGui::EndDisabled();
    if (!state.asset_browser_folder.empty()) ImGui::SameLine();
    ImGui::TextDisabled("assets");
    if (!state.asset_browser_folder.empty()) {
        std::string cumulative;
        std::size_t start = 0;
        while (start < state.asset_browser_folder.size()) {
            const auto slash = state.asset_browser_folder.find('/', start);
            const auto segment = slash == std::string::npos
                                     ? state.asset_browser_folder.substr(start)
                                     : state.asset_browser_folder.substr(start, slash - start);
            cumulative = cumulative.empty() ? segment : cumulative + "/" + segment;
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            if (ImGui::SmallButton((segment + "##crumb").c_str())) {
                state.asset_browser_folder = cumulative;
                state.asset_browser_selected_folder.reset();
            }
            if (slash == std::string::npos) break;
            start = slash + 1;
        }
    }

    auto open_new_folder_popup = [&]() {
        state.asset_folder_name_buffer[0] = '\0';
        state.asset_browser_new_folder_popup = true;
    };

    if (ImGui::SmallButton("+ New Folder")) open_new_folder_popup();
    ImGui::SameLine();
    if (ImGui::SmallButton("+ New Material")) request_new_material_asset(state, false);
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.asset_browser_selected_folder);
    if (ImGui::SmallButton("Rename Folder")) {
        if (state.asset_browser_selected_folder) {
            const auto display = asset_folder_display_name(*state.asset_browser_selected_folder);
            std::snprintf(state.asset_folder_name_buffer, sizeof(state.asset_folder_name_buffer), "%s", display.c_str());
            state.asset_browser_rename_folder_popup = true;
        }
    }
    ImGui::EndDisabled();

    if (state.asset_browser_new_folder_popup) ImGui::OpenPopup("New Asset Folder");
    if (ImGui::BeginPopupModal("New Asset Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", state.asset_folder_name_buffer, sizeof(state.asset_folder_name_buffer));
        if (ImGui::Button("Create")) {
            if (const auto created = create_asset_subfolder(state, state.asset_folder_name_buffer); created)
                state.status = "Created folder assets/" + (state.asset_browser_folder.empty()
                                                                ? std::string(state.asset_folder_name_buffer)
                                                                : state.asset_browser_folder + "/" +
                                                                      state.asset_folder_name_buffer);
            else {
                state.status = created.error().message;
                Logger::instance().write(created.error());
            }
            state.asset_browser_new_folder_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.asset_browser_new_folder_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (state.asset_browser_rename_folder_popup) ImGui::OpenPopup("Rename Asset Folder");
    if (ImGui::BeginPopupModal("Rename Asset Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Name", state.asset_folder_name_buffer, sizeof(state.asset_folder_name_buffer));
        if (ImGui::Button("Rename")) {
            if (state.asset_browser_selected_folder) {
                if (const auto renamed =
                        rename_asset_folder(state, *state.asset_browser_selected_folder, state.asset_folder_name_buffer);
                    renamed)
                    state.status = std::string("Renamed folder to ") + state.asset_folder_name_buffer;
                else {
                    state.status = renamed.error().message;
                    Logger::instance().write(renamed.error());
                }
            }
            state.asset_browser_rename_folder_popup = false;
            state.asset_browser_selected_folder.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            state.asset_browser_rename_folder_popup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::BeginChild("AssetBrowserList", {0.0f, 0.0f}, false)) {
        if (ImGui::Selectable("+ New Folder...##asset-browser-new-folder-row", false)) open_new_folder_popup();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create a folder in assets/%s", state.asset_browser_folder.c_str());
        if (ImGui::Selectable("+ New Material...##asset-browser-new-material-row", false)) {
            request_new_material_asset(state, false);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Create a .material.json asset in the current folder");

        for (const auto& subfolder : list_immediate_asset_subfolders(state.project_root, state.asset_browser_folder)) {
            draw_folder_asset_icon();
            ImGui::SameLine();
            const auto label = asset_folder_display_name(subfolder) + "/##folder-" + subfolder;
            const bool selected =
                state.asset_browser_selected_folder && *state.asset_browser_selected_folder == subfolder;
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                state.asset_browser_selected_folder = subfolder;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    state.asset_browser_folder = subfolder;
                    state.asset_browser_selected_folder.reset();
                }
            }
            if (ImGui::BeginDragDropTarget()) {
                handle_asset_drop_to_folder(state, subfolder);
                ImGui::EndDragDropTarget();
            }
        }

        for (const auto& entry : state.assets.records()) {
            const auto& path = entry.second.path;
            if (!is_direct_asset_child(path, state.asset_browser_folder)) continue;
            const auto normalized = normalize_asset_path(path);
            const auto display_name = asset_entry_name(path);
            const bool is_prefab = path_ends_with(normalized, ".prefab.json");
            const bool is_character = path_ends_with(normalized, ".character.json");
            const bool is_camera = path_ends_with(normalized, ".camera.json");
            const bool is_material = path_ends_with(normalized, ".material.json");
            if (is_prefab) {
                draw_prefab_asset_thumbnail(state, normalized);
                ImGui::SameLine();
                ImGui::Selectable((display_name + "##asset-" + entry.first).c_str(), false,
                                  ImGuiSelectableFlags_AllowDoubleClick);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    place_prefab_at(state, path, state.placement_cursor);
                begin_asset_file_drag_source(normalized, display_name, true);
                ImGui::SameLine();
                if (ImGui::SmallButton(("Place##" + entry.first).c_str()))
                    place_prefab_at(state, path, state.placement_cursor);
                ImGui::SameLine();
                if (ImGui::SmallButton(("Edit##" + entry.first).c_str())) {
                    state.prefab_edit_path = path;
                    state.prefab_edit_part = 0;
                    state.prefab_part_gizmo_was_using = false;
                    state.status = "Editing prefab " + path;
                }
            } else if (is_character || is_camera || is_material) {
                const ImU32 icon_color = is_character ? IM_COL32(125, 175, 255, 230)
                    : is_camera ? IM_COL32(170, 145, 255, 230) : IM_COL32(145, 210, 150, 230);
                draw_file_asset_icon(icon_color);
                ImGui::SameLine();
                const bool selected =
                    state.inspector_asset_path && normalize_asset_path(*state.inspector_asset_path) == normalized;
                if (ImGui::Selectable((display_name + "##asset-" + entry.first).c_str(), selected)) {
                    state.inspector_asset_path = normalized;
                    state.selected.reset();
                    state.rename_target.reset();
                    state.inspector_player_spawn_entity.reset();
                    bool loaded_ok = true;
                    if (is_character) {
                        const auto loaded = CharacterAsset::load(state.project_root / normalized);
                        if (loaded) state.character_asset = loaded.value();
                        else {
                            loaded_ok = false;
                            state.status = loaded.error().message;
                            Logger::instance().write(loaded.error());
                        }
                    } else if (is_camera) {
                        const auto loaded = CameraAsset::load(state.project_root / normalized);
                        if (loaded) {
                            state.camera_asset = loaded.value();
                            state.camera_asset_dirty = false;
                        } else {
                            loaded_ok = false;
                            state.status = loaded.error().message;
                            Logger::instance().write(loaded.error());
                        }
                    } else {
                        const auto loaded = MaterialAsset::load(state.project_root / normalized);
                        if (loaded) {
                            state.material_asset = loaded.value();
                            state.material_asset_dirty = false;
                        } else {
                            loaded_ok = false;
                            state.status = loaded.error().message;
                            Logger::instance().write(loaded.error());
                        }
                    }
                    if (loaded_ok) state.status = "Inspecting " + normalized;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nClick to edit in Inspector", path.c_str());
                begin_asset_file_drag_source(normalized, display_name, false);
            } else {
                draw_file_asset_icon(IM_COL32(145, 154, 172, 230));
                ImGui::SameLine();
                ImGui::TextUnformatted(display_name.c_str());
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
                begin_asset_file_drag_source(normalized, display_name, false);
            }
        }
    }
    ImGui::EndChild();
}

std::filesystem::path repository_root_path() {
#ifdef ENGINE_REPOSITORY_ROOT
    return std::filesystem::path(ENGINE_REPOSITORY_ROOT);
#else
    return {};
#endif
}

std::string extract_markdown_title(const std::string& text, const std::string& fallback) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("# ", 0) == 0) {
            auto title = line.substr(2);
            while (!title.empty() && (title.back() == '\r' || title.back() == ' ')) title.pop_back();
            if (!title.empty()) return title;
        }
    }
    return fallback;
}

std::string extract_markdown_status(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("Status:", 0) != 0) continue;
        auto value = line.substr(7);
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        std::string token;
        for (char ch : value) {
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_')
                token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            else if (!token.empty())
                break;
        }
        return token.empty() ? "unknown" : token;
    }
    return "unknown";
}

void scan_design_docs_folder(const std::filesystem::path& repo_root, const char* section,
    std::vector<EditorState::DesignDocEntry>& out) {
    const auto folder = repo_root / "context" / section;
    if (!std::filesystem::exists(folder)) return;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".md") continue;
        std::ifstream input(entry.path());
        if (!input) continue;
        const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        EditorState::DesignDocEntry doc;
        doc.section = section;
        doc.relative_path = ("context/" + std::string(section) + "/" + entry.path().filename().generic_string());
        doc.title = extract_markdown_title(text, entry.path().stem().generic_string());
        doc.status = extract_markdown_status(text);
        out.push_back(std::move(doc));
    }
}

void refresh_design_docs_catalog(EditorState& state) {
    state.design_docs.clear();
    state.design_docs_error.clear();
    const auto repo = repository_root_path();
    if (repo.empty() || !std::filesystem::exists(repo / "context")) {
        state.design_docs_error = "Engine repository root / context/ not found (ENGINE_REPOSITORY_ROOT).";
        return;
    }
    scan_design_docs_folder(repo, "features", state.design_docs);
    scan_design_docs_folder(repo, "story", state.design_docs);
    scan_design_docs_folder(repo, "art", state.design_docs);
    scan_design_docs_folder(repo, "design", state.design_docs);
    std::sort(state.design_docs.begin(), state.design_docs.end(),
        [](const EditorState::DesignDocEntry& a, const EditorState::DesignDocEntry& b) {
            if (a.section != b.section) return a.section < b.section;
            return a.title < b.title;
        });
}

bool design_doc_matches_filter(const EditorState::DesignDocEntry& doc, int filter) {
    if (filter == 0) return true;
    if (filter == 1) return doc.status == "active";
    if (filter == 2) return doc.status == "planned" || doc.status == "proposed";
    if (filter == 3) return doc.status == "complete" || doc.status == "done";
    return doc.status != "active" && doc.status != "planned" && doc.status != "proposed" && doc.status != "complete"
        && doc.status != "done";
}

void load_design_doc_body(EditorState& state, const EditorState::DesignDocEntry& doc) {
    state.design_docs_loaded_relative = doc.relative_path;
    state.design_docs_body.clear();
    state.design_docs_error.clear();
    const auto path = repository_root_path() / doc.relative_path;
    std::ifstream input(path);
    if (!input) {
        state.design_docs_error = "Could not read " + doc.relative_path;
        return;
    }
    state.design_docs_body.assign((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

bool is_markdown_table_separator(const std::string& line);
std::vector<std::string> split_markdown_table_row(const std::string& line);
std::string cleanup_markdown_inline(std::string text);

[[nodiscard]] bool is_dom_open_questions_doc(const EditorState::DesignDocEntry& doc) {
    return doc.relative_path.find("dom-open-questions.md") != std::string::npos;
}

std::string markdown_cell_escape(std::string value) {
    for (char& ch : value) {
        if (ch == '|') ch = '/';
        if (ch == '\n' || ch == '\r') ch = ' ';
    }
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
    return value;
}

std::string local_date_yyyy_mm_dd() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d");
    return out.str();
}

void parse_dom_open_questions(const std::string& markdown, std::vector<EditorState::DomOpenQuestion>& out) {
    out.clear();
    std::istringstream stream(markdown);
    std::string line;
    std::string current_priority;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("## P0", 0) == 0) current_priority = "P0";
        else if (line.rfind("## P1", 0) == 0) current_priority = "P1";
        else if (line.rfind("## P2", 0) == 0) current_priority = "P2";
        else if (line.rfind("## ", 0) == 0) current_priority.clear();

        if (current_priority.empty()) continue;
        if (line.find('|') == std::string::npos) continue;
        if (is_markdown_table_separator(line)) continue;
        auto cells = split_markdown_table_row(line);
        if (cells.empty()) continue;
        const std::string& id = cells.front();
        if (id.rfind("D-P", 0) != 0) continue;

        EditorState::DomOpenQuestion q;
        q.id = id;
        q.priority = current_priority;
        if (cells.size() >= 2) q.question = cleanup_markdown_inline(cells[1]);
        if (current_priority == "P2") {
            if (cells.size() >= 3) q.context = cleanup_markdown_inline(cells[2]);
            if (cells.size() >= 4) q.answer = cells[3];
        } else {
            std::string why = cells.size() >= 3 ? cleanup_markdown_inline(cells[2]) : std::string{};
            std::string draft = cells.size() >= 4 ? cleanup_markdown_inline(cells[3]) : std::string{};
            if (!why.empty() && !draft.empty()) q.context = why + "\n\nDraft: " + draft;
            else if (!why.empty()) q.context = why;
            else q.context = draft;
            if (cells.size() >= 5) q.answer = cells[4];
        }
        out.push_back(std::move(q));
    }
}

bool write_text_file_atomic(const std::filesystem::path& absolute_path, const std::string& source,
    std::string& error_out) {
    error_out.clear();
    const auto parent = absolute_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }
    const auto temp = absolute_path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) {
            error_out = "Failed to open temp file for write.";
            return false;
        }
        out << source;
        if (!source.empty() && source.back() != '\n') out << '\n';
        if (!out) {
            error_out = "Failed while writing temp file.";
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, absolute_path, ec);
    if (ec) {
        error_out = "Failed to replace document: " + ec.message();
        std::filesystem::remove(temp);
        return false;
    }
    return true;
}

bool apply_dom_answers_to_markdown(std::string& markdown,
    const std::vector<EditorState::DomOpenQuestion>& questions, const std::string& session_notes,
    std::string& error_out) {
    error_out.clear();
    std::map<std::string, std::string> answers;
    std::vector<std::string> answered_ids;
    for (const auto& q : questions) {
        answers[q.id] = markdown_cell_escape(q.answer);
        if (!q.answer.empty()) answered_ids.push_back(q.id);
    }

    std::istringstream stream(markdown);
    std::ostringstream rebuilt;
    std::string line;
    std::string current_priority;
    bool in_session_log = false;
    bool inserted_session_row = false;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("## Session answer log", 0) == 0) {
            in_session_log = true;
            current_priority.clear();
        } else if (line.rfind("## ", 0) == 0) {
            in_session_log = false;
            if (line.rfind("## P0", 0) == 0) current_priority = "P0";
            else if (line.rfind("## P1", 0) == 0) current_priority = "P1";
            else if (line.rfind("## P2", 0) == 0) current_priority = "P2";
            else current_priority.clear();
        }

        if (!current_priority.empty() && line.find('|') != std::string::npos && !is_markdown_table_separator(line)) {
            auto cells = split_markdown_table_row(line);
            if (!cells.empty() && cells.front().rfind("D-P", 0) == 0) {
                const auto it = answers.find(cells.front());
                if (it != answers.end()) {
                    const std::size_t answer_index = (current_priority == "P2") ? 3u : 4u;
                    while (cells.size() <= answer_index) cells.emplace_back();
                    cells[answer_index] = it->second;
                    rebuilt << '|';
                    for (const auto& cell : cells) rebuilt << ' ' << cell << " |";
                    rebuilt << '\n';
                    continue;
                }
            }
        }

        // After session-log header + separator, insert a new row for this submit.
        if (in_session_log && !inserted_session_row && is_markdown_table_separator(line)) {
            rebuilt << line << '\n';
            if (!answered_ids.empty() || !session_notes.empty()) {
                std::ostringstream ids;
                for (std::size_t i = 0; i < answered_ids.size(); ++i) {
                    if (i) ids << ", ";
                    ids << answered_ids[i];
                }
                const std::string notes = markdown_cell_escape(
                    session_notes.empty() ? std::string("In-editor Design Docs form submit") : session_notes);
                const std::string ids_cell =
                    markdown_cell_escape(ids.str().empty() ? std::string("(no filled answers)") : ids.str());
                rebuilt << "| " << local_date_yyyy_mm_dd() << " | " << notes << " | " << ids_cell << " |\n";
            }
            inserted_session_row = true;
            continue;
        }

        rebuilt << line << '\n';
    }

    markdown = rebuilt.str();
    if (!answered_ids.empty() && !inserted_session_row) {
        error_out = "Session answer log table not found; answers were still written to question rows.";
    }
    return true;
}

void ensure_dom_form_loaded(EditorState& state, const EditorState::DesignDocEntry& doc) {
    if (!is_dom_open_questions_doc(doc)) return;
    if (state.design_docs_dom_form_source_path == doc.relative_path && !state.design_docs_dom_questions.empty())
        return;
    parse_dom_open_questions(state.design_docs_body, state.design_docs_dom_questions);
    state.design_docs_dom_form_source_path = doc.relative_path;
    if (state.design_docs_dom_session_notes.empty())
        state.design_docs_dom_session_notes = "In-editor Design Docs form submit";
    state.design_docs_dom_form_status.clear();
}

bool draw_design_docs_string_multiline(const char* id, std::string& value, const ImVec2& size) {
    std::vector<char> buffer((std::max)(static_cast<std::size_t>(1024), value.size() + 1024), '\0');
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    if (ImGui::InputTextMultiline(id, buffer.data(), buffer.size(), size, ImGuiInputTextFlags_AllowTabInput)) {
        value = buffer.data();
        return true;
    }
    return false;
}

void draw_dom_open_questions_form(EditorState& state, const EditorState::DesignDocEntry& doc) {
    ensure_dom_form_loaded(state, doc);
    ImGui::TextWrapped(
        "Fill answers below, then Submit to write the Answer column (and session log) back to the repo markdown.");
    ImGui::Spacing();

    const char* pri_filters[] = {"All", "P0", "P1", "P2"};
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("Priority filter", &state.design_docs_dom_priority_filter, pri_filters, IM_ARRAYSIZE(pri_filters));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu questions", state.design_docs_dom_questions.size());

    ImGui::TextUnformatted("Session notes (logged on submit)");
    draw_design_docs_string_multiline("##dom_session_notes", state.design_docs_dom_session_notes, ImVec2(-FLT_MIN, 54.0f));

    if (ImGui::Button("Submit answers to doc")) {
        std::string markdown = state.design_docs_body;
        std::string apply_error;
        if (!apply_dom_answers_to_markdown(markdown, state.design_docs_dom_questions, state.design_docs_dom_session_notes,
                apply_error)) {
            state.design_docs_dom_form_status = apply_error.empty() ? "Failed to apply answers." : apply_error;
        } else {
            const auto path = repository_root_path() / doc.relative_path;
            std::string write_error;
            if (!write_text_file_atomic(path, markdown, write_error)) {
                state.design_docs_dom_form_status = write_error;
            } else {
                state.design_docs_body = std::move(markdown);
                parse_dom_open_questions(state.design_docs_body, state.design_docs_dom_questions);
                state.design_docs_dom_form_source_path = doc.relative_path;
                state.design_docs_dom_form_status = apply_error.empty()
                    ? ("Saved answers to " + doc.relative_path)
                    : ("Saved with warning: " + apply_error);
                state.status = "Dom open questions updated";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload from disk")) {
        load_design_doc_body(state, doc);
        state.design_docs_dom_form_source_path.clear();
        ensure_dom_form_loaded(state, doc);
        state.design_docs_dom_form_status = "Reloaded from disk.";
    }

    if (!state.design_docs_dom_form_status.empty()) {
        const bool err = state.design_docs_dom_form_status.find("Failed") != std::string::npos;
        ImGui::TextColored(err ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f) : ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "%s",
            state.design_docs_dom_form_status.c_str());
    }

    ImGui::Separator();
    ImGui::BeginChild("DomOpenQuestionsFormList", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    const float form_width = (std::max)(ImGui::GetContentRegionAvail().x, 720.0f);
    ImGui::PushItemWidth(form_width);

    int shown = 0;
    for (int i = 0; i < static_cast<int>(state.design_docs_dom_questions.size()); ++i) {
        auto& q = state.design_docs_dom_questions[static_cast<std::size_t>(i)];
        if (state.design_docs_dom_priority_filter == 1 && q.priority != "P0") continue;
        if (state.design_docs_dom_priority_filter == 2 && q.priority != "P1") continue;
        if (state.design_docs_dom_priority_filter == 3 && q.priority != "P2") continue;
        ++shown;

        ImGui::PushID(i);
        ImGui::Spacing();
        if (GameFonts::display()) ImGui::PushFont(GameFonts::display());
        ImGui::TextColored(ImVec4(0.95f, 0.90f, 0.72f, 1.0f), "%s", q.id.c_str());
        if (GameFonts::display()) ImGui::PopFont();
        ImGui::SameLine();
        ImGui::TextDisabled("[%s]", q.priority.c_str());
        ImGui::TextWrapped("%s", q.question.c_str());
        if (!q.context.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.70f, 0.74f, 0.80f, 1.0f));
            ImGui::TextWrapped("%s", q.context.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::TextUnformatted("Answer");
        draw_design_docs_string_multiline("##answer", q.answer, ImVec2(form_width, 64.0f));
        ImGui::Separator();
        ImGui::PopID();
    }
    if (shown == 0) ImGui::TextDisabled("No questions match this priority filter.");
    ImGui::PopItemWidth();
    ImGui::EndChild();
}

std::string trim_markdown_whitespace(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r'))
        value.erase(value.begin());
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
        value.pop_back();
    return value;
}

bool is_markdown_table_separator(const std::string& line) {
    if (line.find('|') == std::string::npos) return false;
    for (char ch : line) {
        if (ch != '|' && ch != '-' && ch != ':' && ch != ' ' && ch != '\t') return false;
    }
    return line.find('-') != std::string::npos;
}

std::vector<std::string> split_markdown_table_row(const std::string& line) {
    std::vector<std::string> cells;
    std::string current;
    bool started = false;
    for (char ch : line) {
        if (ch == '|') {
            if (started) cells.push_back(trim_markdown_whitespace(current));
            current.clear();
            started = true;
            continue;
        }
        if (!started && (ch == ' ' || ch == '\t')) continue;
        started = true;
        current.push_back(ch);
    }
    if (started && !trim_markdown_whitespace(current).empty())
        cells.push_back(trim_markdown_whitespace(current));
    // Drop empty leading/trailing cells from edge pipes.
    while (!cells.empty() && cells.front().empty()) cells.erase(cells.begin());
    while (!cells.empty() && cells.back().empty()) cells.pop_back();
    return cells;
}

std::string cleanup_markdown_inline(std::string text) {
    // [label](url) -> label
    for (;;) {
        const auto open = text.find('[');
        if (open == std::string::npos) break;
        const auto mid = text.find("](", open);
        if (mid == std::string::npos) break;
        const auto close = text.find(')', mid + 2);
        if (close == std::string::npos) break;
        const auto label = text.substr(open + 1, mid - open - 1);
        text.replace(open, close - open + 1, label);
    }
    // `code` -> code
    for (;;) {
        const auto a = text.find('`');
        if (a == std::string::npos) break;
        const auto b = text.find('`', a + 1);
        if (b == std::string::npos) break;
        text.replace(a, b - a + 1, text.substr(a + 1, b - a - 1));
    }
    // **bold** / __bold__
    auto strip_wrap = [&](const char* marker) {
        const std::size_t n = std::strlen(marker);
        for (;;) {
            const auto a = text.find(marker);
            if (a == std::string::npos) break;
            const auto b = text.find(marker, a + n);
            if (b == std::string::npos) break;
            text.replace(a, b - a + n, text.substr(a + n, b - a - n));
        }
    };
    strip_wrap("**");
    strip_wrap("__");
    // *italic* / _italic_ (single) — avoid eating underscores in snake_case ids by requiring spaces/boundaries
    for (;;) {
        const auto a = text.find('*');
        if (a == std::string::npos) break;
        const auto b = text.find('*', a + 1);
        if (b == std::string::npos || b == a + 1) break;
        text.replace(a, b - a + 1, text.substr(a + 1, b - a - 1));
    }
    // Checkbox markers
    if (text.rfind("[ ] ", 0) == 0) text.replace(0, 4, "☐ ");
    else if (text.rfind("[x] ", 0) == 0 || text.rfind("[X] ", 0) == 0) text.replace(0, 4, "☑ ");
    return text;
}

void draw_markdown_text_line(const std::string& raw, const ImVec4* color = nullptr) {
    const std::string cleaned = cleanup_markdown_inline(raw);
    if (cleaned.empty()) {
        ImGui::Spacing();
        return;
    }
    if (color) ImGui::PushStyleColor(ImGuiCol_Text, *color);
    ImGui::TextWrapped("%s", cleaned.c_str());
    if (color) ImGui::PopStyleColor();
}

void draw_markdown_table(const std::vector<std::vector<std::string>>& rows) {
    if (rows.empty() || rows.front().empty()) return;
    const int columns = static_cast<int>(rows.front().size());
    if (columns <= 0) return;
    ImGui::Spacing();
    constexpr float kMinColWidth = 140.0f;
    constexpr float kDefaultColWidth = 220.0f;
    // Unique id per table instance so resize widths don't collide across docs/sections.
    ImGui::PushID(static_cast<int>(rows.size() * 31 + columns));
    if (ImGui::BeginTable("##md_table", columns,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit
                | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
                | ImGuiTableFlags_NoSavedSettings)) {
        std::vector<std::string> headers;
        headers.reserve(static_cast<std::size_t>(columns));
        for (int c = 0; c < columns; ++c)
            headers.push_back(cleanup_markdown_inline(rows.front()[static_cast<std::size_t>(c)]));
        for (int c = 0; c < columns; ++c) {
            ImGui::TableSetupColumn(headers[static_cast<std::size_t>(c)].c_str(),
                ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, kDefaultColWidth);
        }
        ImGui::TableHeadersRow();
        for (std::size_t r = 1; r < rows.size(); ++r) {
            ImGui::TableNextRow();
            for (int c = 0; c < columns; ++c) {
                ImGui::TableSetColumnIndex(c);
                const std::string cell =
                    c < static_cast<int>(rows[r].size()) ? cleanup_markdown_inline(rows[r][static_cast<std::size_t>(c)])
                                                         : std::string{};
                // Wrap within the current (user-resizable) column width.
                const float wrap_width = (std::max)(kMinColWidth, ImGui::GetContentRegionAvail().x);
                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
                ImGui::TextUnformatted(cell.c_str());
                ImGui::PopTextWrapPos();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopID();
    ImGui::Spacing();
}

void draw_markdown_readonly(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    bool in_code_fence = false;
    std::string code_block;
    bool skipped_title = false;
    std::vector<std::vector<std::string>> table_rows;

    auto flush_table = [&]() {
        if (!table_rows.empty()) {
            draw_markdown_table(table_rows);
            table_rows.clear();
        }
    };

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (line.rfind("```", 0) == 0) {
            flush_table();
            if (!in_code_fence) {
                in_code_fence = true;
                code_block.clear();
            } else {
                in_code_fence = false;
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.11f, 0.14f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.86f, 0.92f, 1.0f));
                ImGui::BeginChild("##md_code_block", ImVec2(-FLT_MIN, 0.0f),
                    ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
                if (ImFont* mono = GameFonts::mono()) ImGui::PushFont(mono);
                ImGui::TextUnformatted(code_block.c_str());
                if (GameFonts::mono()) ImGui::PopFont();
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
                ImGui::Spacing();
                code_block.clear();
            }
            continue;
        }
        if (in_code_fence) {
            if (!code_block.empty()) code_block.push_back('\n');
            code_block += line;
            continue;
        }

        // Markdown tables
        if (!line.empty() && line.find('|') != std::string::npos
            && (line.front() == '|' || line.find('|') != line.size() - 1)) {
            if (is_markdown_table_separator(line)) continue;
            auto cells = split_markdown_table_row(line);
            if (!cells.empty()) {
                table_rows.push_back(std::move(cells));
                continue;
            }
        } else {
            flush_table();
        }

        if (line.empty()) {
            ImGui::Dummy(ImVec2(0.0f, 6.0f));
            continue;
        }
        if (line == "---" || line == "***" || line == "___") {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            continue;
        }

        int heading = 0;
        std::string heading_text;
        if (line.rfind("#### ", 0) == 0) {
            heading = 4;
            heading_text = line.substr(5);
        } else if (line.rfind("### ", 0) == 0) {
            heading = 3;
            heading_text = line.substr(4);
        } else if (line.rfind("## ", 0) == 0) {
            heading = 2;
            heading_text = line.substr(3);
        } else if (line.rfind("# ", 0) == 0) {
            heading = 1;
            heading_text = line.substr(2);
        }

        if (heading > 0) {
            heading_text = cleanup_markdown_inline(heading_text);
            if (heading == 1 && !skipped_title) {
                skipped_title = true; // title already shown in the pane header
                continue;
            }
            ImGui::Dummy(ImVec2(0.0f, heading == 2 ? 10.0f : 6.0f));
            const ImVec4 colors[] = {
                ImVec4(0.95f, 0.90f, 0.72f, 1.0f),
                ImVec4(0.82f, 0.88f, 1.0f, 1.0f),
                ImVec4(0.75f, 0.92f, 0.82f, 1.0f),
                ImVec4(0.85f, 0.85f, 0.90f, 1.0f),
            };
            const ImVec4 color = colors[static_cast<std::size_t>(std::clamp(heading, 1, 4) - 1)];
            if (heading <= 2 && GameFonts::display()) ImGui::PushFont(GameFonts::display());
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", heading_text.c_str());
            ImGui::PopStyleColor();
            if (heading <= 2 && GameFonts::display()) ImGui::PopFont();
            if (heading <= 2) {
                ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(color.x, color.y, color.z, 0.35f));
                ImGui::Separator();
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, 2.0f));
            continue;
        }

        // Block quote
        if (line.rfind("> ", 0) == 0) {
            const ImVec4 quote_color(0.70f, 0.74f, 0.82f, 1.0f);
            ImGui::Indent(10.0f);
            draw_markdown_text_line(line.substr(2), &quote_color);
            ImGui::Unindent(10.0f);
            continue;
        }

        // Lists (unordered / ordered / task)
        auto list_body = std::string{};
        bool is_list = false;
        if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0 || line.rfind("+ ", 0) == 0) {
            list_body = line.substr(2);
            is_list = true;
        } else if (line.size() >= 3 && std::isdigit(static_cast<unsigned char>(line[0]))) {
            const auto dot = line.find(". ");
            if (dot != std::string::npos && dot < 4) {
                list_body = line.substr(dot + 2);
                is_list = true;
            }
        }
        if (is_list) {
            ImGui::Bullet();
            ImGui::SameLine();
            draw_markdown_text_line(list_body);
            continue;
        }

        draw_markdown_text_line(line);
    }
    flush_table();
    if (in_code_fence && !code_block.empty()) {
        if (ImFont* mono = GameFonts::mono()) ImGui::PushFont(mono);
        ImGui::TextUnformatted(code_block.c_str());
        if (GameFonts::mono()) ImGui::PopFont();
    }
}

void draw_markdown_selectable_source(const std::string& text) {
    ImGui::TextDisabled("Select text and Ctrl+C, or use Copy all.");
    std::vector<char> buffer(text.size() + 1, '\0');
    if (!text.empty()) std::memcpy(buffer.data(), text.data(), text.size());
    if (ImFont* mono = GameFonts::mono()) ImGui::PushFont(mono);
    ImGui::InputTextMultiline("##design_docs_selectable_md", buffer.data(), buffer.size(), ImVec2(-FLT_MIN, -FLT_MIN),
        ImGuiInputTextFlags_ReadOnly | ImGuiInputTextFlags_AllowTabInput);
    if (GameFonts::mono()) ImGui::PopFont();
}

void draw_design_docs_viewport(EditorState& state) {
    if (state.design_docs.empty() && state.design_docs_error.empty()) refresh_design_docs_catalog(state);
    if (++state.design_docs_scan_counter >= 120) {
        state.design_docs_scan_counter = 0;
        const int previous = state.design_docs_selected;
        std::string previous_path =
            (previous >= 0 && previous < static_cast<int>(state.design_docs.size()))
                ? state.design_docs[static_cast<std::size_t>(previous)].relative_path
                : state.design_docs_loaded_relative;
        refresh_design_docs_catalog(state);
        state.design_docs_selected = -1;
        for (int i = 0; i < static_cast<int>(state.design_docs.size()); ++i) {
            if (state.design_docs[static_cast<std::size_t>(i)].relative_path == previous_path) {
                state.design_docs_selected = i;
                break;
            }
        }
        if (state.design_docs_selected >= 0)
            load_design_doc_body(state, state.design_docs[static_cast<std::size_t>(state.design_docs_selected)]);
    }

    ImGui::TextUnformatted("Design Docs");
    ImGui::SameLine();
    ImGui::TextDisabled("(repo context/ markdown)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Refresh")) {
        refresh_design_docs_catalog(state);
        if (state.design_docs_selected >= 0
            && state.design_docs_selected < static_cast<int>(state.design_docs.size())) {
            load_design_doc_body(state, state.design_docs[static_cast<std::size_t>(state.design_docs_selected)]);
            state.design_docs_dom_form_source_path.clear();
        }
    }
    ImGui::TextWrapped(
        "Most docs are read-only. Dom Open Questions supports an in-editor answer form that writes back to the markdown.");

    const char* filters[] = {"All", "active", "planned", "complete", "other"};
    ImGui::SetNextItemWidth(140.0f);
    ImGui::Combo("Status filter", &state.design_docs_status_filter, filters, IM_ARRAYSIZE(filters));

    if (!state.design_docs_error.empty() && state.design_docs.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", state.design_docs_error.c_str());
        return;
    }

    const float list_width = std::clamp(ImGui::GetContentRegionAvail().x * 0.34f, 220.0f, 360.0f);
    ImGui::BeginChild("DesignDocsList", ImVec2(list_width, 0.0f), true);
    std::string last_section;
    for (int i = 0; i < static_cast<int>(state.design_docs.size()); ++i) {
        const auto& doc = state.design_docs[static_cast<std::size_t>(i)];
        if (!design_doc_matches_filter(doc, state.design_docs_status_filter)) continue;
        if (doc.section != last_section) {
            last_section = doc.section;
            ImGui::SeparatorText(doc.section.c_str());
        }
        const bool selected = state.design_docs_selected == i;
        ImGui::PushID(i);
        if (ImGui::Selectable(doc.title.c_str(), selected)) {
            state.design_docs_selected = i;
            load_design_doc_body(state, doc);
            state.design_docs_dom_form_source_path.clear();
            if (is_dom_open_questions_doc(doc)) state.design_docs_dom_form_mode = true;
        }
        register_ui_hotspot_last_item(&state.ui_hotspots, "DesignDocs.Doc." + std::to_string(i), doc.title);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nstatus: %s", doc.relative_path.c_str(), doc.status.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("%s", doc.status.c_str());
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("DesignDocsBody", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (state.design_docs_selected < 0 || state.design_docs_selected >= static_cast<int>(state.design_docs.size())) {
        ImGui::TextDisabled("Select a document from the list.");
    } else {
        const auto& doc = state.design_docs[static_cast<std::size_t>(state.design_docs_selected)];
        if (GameFonts::display()) ImGui::PushFont(GameFonts::display());
        ImGui::TextColored(ImVec4(0.95f, 0.90f, 0.72f, 1.0f), "%s", doc.title.c_str());
        if (GameFonts::display()) ImGui::PopFont();
        ImGui::TextDisabled("%s", doc.relative_path.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("·");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.78f, 0.95f, 1.0f), "%s", doc.status.c_str());

        const bool dom_doc = is_dom_open_questions_doc(doc);
        if (dom_doc) {
            ImGui::SameLine();
            if (ImGui::RadioButton("Form", state.design_docs_dom_form_mode)) state.design_docs_dom_form_mode = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Markdown", !state.design_docs_dom_form_mode)) state.design_docs_dom_form_mode = false;
        }

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        if (!state.design_docs_error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", state.design_docs_error.c_str());
        } else if (dom_doc && state.design_docs_dom_form_mode) {
            draw_dom_open_questions_form(state, doc);
        } else {
            if (ImGui::SmallButton("Copy all")) {
                ImGui::SetClipboardText(state.design_docs_body.c_str());
                state.design_docs_copy_status = "Copied document to clipboard.";
            }
            ImGui::SameLine();
            if (ImGui::RadioButton("Selectable", state.design_docs_markdown_selectable))
                state.design_docs_markdown_selectable = true;
            ImGui::SameLine();
            if (ImGui::RadioButton("Rendered", !state.design_docs_markdown_selectable))
                state.design_docs_markdown_selectable = false;
            if (!state.design_docs_copy_status.empty()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.0f), "%s", state.design_docs_copy_status.c_str());
            }
            ImGui::Dummy(ImVec2(0.0f, 2.0f));

            if (state.design_docs_markdown_selectable) {
                draw_markdown_selectable_source(state.design_docs_body);
            } else {
                // Keep wide tables readable: inner pane can exceed viewport width; outer child scrolls X.
                const float md_min_width = 1100.0f;
                const float md_width = (std::max)(ImGui::GetContentRegionAvail().x, md_min_width);
                ImGui::BeginChild("DesignDocsMarkdownPane", ImVec2(md_width, 0.0f), ImGuiChildFlags_AutoResizeY);
                draw_markdown_readonly(state.design_docs_body);
                ImGui::EndChild();
            }
        }
    }
    ImGui::EndChild();
}

void draw_editor(EditorState& state, CollisionWorld* collision, bool camera_capture, ImTextureID scene_texture,
    ImTextureID game_texture, const std::array<float, 16>& view, const std::array<float, 16>& projection,
    const std::array<float, 16>& view_projection, const std::array<float, 3>& camera_position,
    StreamedTerrainField* streamed_terrain = nullptr, StreamedFoliageField* streamed_foliage = nullptr,
    StreamedWaterField* streamed_water = nullptr, MaterialAsset* terrain_material = nullptr,
    const CharacterController* character = nullptr, const InteractionVolumeRegistry* interactions = nullptr,
    const CombatVolumeRegistry* combat = nullptr) {
    state.ui_hotspots.clear();
    state.ui_hotspots.set_window_size(static_cast<int>(ImGui::GetIO().DisplaySize.x),
        static_cast<int>(ImGui::GetIO().DisplaySize.y));

    const auto* main = ImGui::GetMainViewport();
    constexpr ImGuiWindowFlags locked =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem(ICON_FA_SAVE " Save", "Ctrl+S")) {
                const auto saved = state.scene.save_atomic(state.world_path);
                if (!saved) {
                    state.status = saved.error().message;
                    Logger::instance().write(saved.error());
                } else {
                    const auto terrain_saved =
                        state.terrain_edits.save_atomic(default_terrain_edits_path(state.project_root));
                    const auto paint_saved =
                        state.terrain_paint.save_atomic(default_terrain_paint_path(state.project_root));
                    const auto foliage_saved =
                        state.foliage_density.save_atomic(default_foliage_density_path(state.project_root));
                    if (!terrain_saved) {
                        state.status = terrain_saved.error().message;
                        Logger::instance().write(terrain_saved.error());
                    } else if (!paint_saved) {
                        state.status = paint_saved.error().message;
                        Logger::instance().write(paint_saved.error());
                    } else if (!foliage_saved) {
                        state.status = foliage_saved.error().message;
                        Logger::instance().write(foliage_saved.error());
                    } else {
                        const auto water_saved =
                            state.water_store.save_atomic(default_water_surfaces_path(state.project_root));
                        if (!water_saved) {
                            state.status = water_saved.error().message;
                            Logger::instance().write(water_saved.error());
                        } else {
                        state.terrain_edits_dirty = false;
                        state.terrain_paint_dirty = false;
                        state.foliage_density_dirty = false;
                        state.water_dirty = false;
                        state.status = "World, terrain sculpt, terrain paint, foliage, and water saved";
                        }
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem(ICON_FA_UNDO " Undo", "Ctrl+Z", false, state.history.undo_size() > 0)) {
                const auto result = state.history.undo(state.scene);
                state.status = result ? state.history.last_summary() : result.error().message;
            }
            if (ImGui::MenuItem(ICON_FA_REDO " Redo", "Ctrl+Y", false, state.history.redo_size() > 0)) {
                const auto result = state.history.redo(state.scene);
                state.status = result ? state.history.last_summary() : result.error().message;
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, state.selected.has_value())) duplicate_selected(state);
            if (ImGui::MenuItem("Delete", "Del", false, state.selected && state.scene.placement(*state.selected)))
                remove_selected(state);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            const bool inactive = state.test_session == EditorState::TestSessionState::Inactive;
            const bool running = state.test_session == EditorState::TestSessionState::Running;
            const bool paused = state.test_session == EditorState::TestSessionState::Paused;
            if (ImGui::MenuItem(ICON_FA_PLAY " Start Test Session", "F5", false, inactive))
                state.test_session_command = EditorState::TestSessionCommand::Start;
            if (ImGui::MenuItem(ICON_FA_PAUSE " Pause Test Session", "F6", false, running))
                state.test_session_command = EditorState::TestSessionCommand::Pause;
            if (ImGui::MenuItem(ICON_FA_PLAY " Resume Test Session", "F6", false, paused))
                state.test_session_command = EditorState::TestSessionCommand::Resume;
            if (ImGui::MenuItem(ICON_FA_STOP " End Test Session", "Shift+F5", false, !inactive))
                state.test_session_command = EditorState::TestSessionCommand::End;
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    const char* active_area = "Scene";
    switch (state.active_viewport_tab) {
    case EditorState::ViewportTab::Sculpt: active_area = "Sculpt"; break;
    case EditorState::ViewportTab::Game: active_area = "Game"; break;
    case EditorState::ViewportTab::UI: active_area = "UI Canvas"; break;
    case EditorState::ViewportTab::WorldForge: active_area = "World Forge"; break;
    case EditorState::ViewportTab::DesignDocs: active_area = "Design Docs"; break;
    case EditorState::ViewportTab::Scene: default: active_area = "Scene"; break;
    }
    bool chrome_save = false;
    const bool any_dirty = state.scene_dirty || state.terrain_edits_dirty || state.terrain_paint_dirty ||
        state.foliage_density_dirty || state.water_dirty || state.world_forge_editor.dirty;
    EditorChrome::draw_app_header(state.project_root, active_area, any_dirty, state.world_forge_editor.dirty,
        state.status, &chrome_save, &state.ui_hotspots);
    if (chrome_save) {
        const auto saved = state.scene.save_atomic(state.world_path);
        if (!saved) {
            state.status = saved.error().message;
            Logger::instance().write(saved.error());
        } else {
            const auto terrain_saved =
                state.terrain_edits.save_atomic(default_terrain_edits_path(state.project_root));
            const auto paint_saved =
                state.terrain_paint.save_atomic(default_terrain_paint_path(state.project_root));
            const auto foliage_saved =
                state.foliage_density.save_atomic(default_foliage_density_path(state.project_root));
            if (!terrain_saved) {
                state.status = terrain_saved.error().message;
                Logger::instance().write(terrain_saved.error());
            } else if (!paint_saved) {
                state.status = paint_saved.error().message;
                Logger::instance().write(paint_saved.error());
            } else if (!foliage_saved) {
                state.status = foliage_saved.error().message;
                Logger::instance().write(foliage_saved.error());
            } else {
                const auto water_saved =
                    state.water_store.save_atomic(default_water_surfaces_path(state.project_root));
                if (!water_saved) {
                    state.status = water_saved.error().message;
                    Logger::instance().write(water_saved.error());
                } else {
                    state.terrain_edits_dirty = false;
                    state.terrain_paint_dirty = false;
                    state.foliage_density_dirty = false;
                    state.water_dirty = false;
                    state.scene_dirty = false;
                    if (state.world_forge_editor.dirty) {
                        if (const auto wf = state.world_forge_editor.save(state.project_root); !wf) {
                            state.status = "World Forge save failed: " + wf.error().message;
                            Logger::instance().write(wf.error());
                        } else {
                            state.status = "Saved";
                        }
                    } else {
                        state.status = "Saved";
                    }
                }
            }
        }
    }

    const ImVec2 origin{main->WorkPos.x, main->WorkPos.y + EditorChrome::kHeaderHeight};
    const ImVec2 extent{main->WorkSize.x, std::max(1.0f, main->WorkSize.y - EditorChrome::kHeaderHeight)};
    const float left = std::min(extent.x * 0.20f, 310.0f);
    const float right = std::min(extent.x * 0.25f, 390.0f);
    const float center = std::max(1.0f, extent.x - left - right);
    const float top = extent.y * 0.63f;
    const float bottom = extent.y - top;

    handle_editor_shortcuts(state, camera_capture, terrain_material, streamed_terrain, streamed_water, collision);

    const bool edit_mode = !state.test_session_active();
    ImGui::SetNextWindowPos({origin.x + left, origin.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({center, top}, ImGuiCond_Always);
    ImGui::Begin("Viewports", nullptr, locked);
    state.viewport_hovered = false;
    state.viewport_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (ImGui::BeginTabBar("EditorViewportTabs", ImGuiTabBarFlags_NoCloseWithMiddleMouseButton)) {
        const auto tab_flags = [&](EditorState::ViewportTab tab) -> ImGuiTabItemFlags {
            if (!state.force_select_viewport_tab || state.active_viewport_tab != tab) return 0;
            return ImGuiTabItemFlags_SetSelected;
        };
        const bool scene_open =
            ImGui::BeginTabItem(ICON_FA_CUBE " Scene##ViewportScene", nullptr, tab_flags(EditorState::ViewportTab::Scene));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.Scene", "Scene");
        if (scene_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Scene;
            ImGui::EndTabItem();
        }
        ImGui::BeginDisabled(state.test_session_active());
        const bool sculpt_open = ImGui::BeginTabItem(ICON_FA_MOUNTAIN " Sculpt##ViewportSculpt", nullptr,
            tab_flags(EditorState::ViewportTab::Sculpt));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.Sculpt", "Sculpt");
        if (sculpt_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Sculpt;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        const bool game_open =
            ImGui::BeginTabItem(ICON_FA_GAMEPAD " Game##ViewportGame", nullptr, tab_flags(EditorState::ViewportTab::Game));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.Game", "Game");
        if (game_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Game;
            ImGui::EndTabItem();
        }
        ImGui::BeginDisabled(state.test_session_active());
        const bool ui_open =
            ImGui::BeginTabItem(ICON_FA_DESKTOP " UI##ViewportUI", nullptr, tab_flags(EditorState::ViewportTab::UI));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.UI", "UI");
        if (ui_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::UI;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.test_session_active());
        const bool world_forge_open = ImGui::BeginTabItem(ICON_FA_GLOBE " World Forge##ViewportWorldForge", nullptr,
            tab_flags(EditorState::ViewportTab::WorldForge));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.WorldForge", "World Forge");
        if (world_forge_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::WorldForge;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.test_session_active());
        const bool design_docs_open = ImGui::BeginTabItem(ICON_FA_BOOK " Design Docs##ViewportDesignDocs", nullptr,
            tab_flags(EditorState::ViewportTab::DesignDocs));
        register_ui_hotspot_last_item(&state.ui_hotspots, "Viewport.DesignDocs", "Design Docs");
        if (design_docs_open) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::DesignDocs;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        ImGui::EndTabBar();
        if (!state.lock_viewport_tab) state.force_select_viewport_tab = false;
    }
    const bool scene_tab = state.active_viewport_tab == EditorState::ViewportTab::Scene;
    const bool sculpt_tab = state.active_viewport_tab == EditorState::ViewportTab::Sculpt;
    const bool game_tab = state.active_viewport_tab == EditorState::ViewportTab::Game;
    const bool ui_tab = state.active_viewport_tab == EditorState::ViewportTab::UI;
    const bool world_forge_tab = state.active_viewport_tab == EditorState::ViewportTab::WorldForge;
    const bool design_docs_tab = state.active_viewport_tab == EditorState::ViewportTab::DesignDocs;
    if (scene_tab && edit_mode && state.viewport_focused && !camera_capture && !ImGui::GetIO().WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_1)) state.gizmo_operation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_2)) state.gizmo_operation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_3)) state.gizmo_operation = ImGuizmo::SCALE;
    }
    ImGui::Text("Status: %s", state.status.c_str());
    if (state.test_session == EditorState::TestSessionState::Running) {
        ImGui::SameLine();
        ImGui::TextColored({0.45f, 1.0f, 0.55f, 1.0f}, "| Test session: running");
    } else if (state.test_session == EditorState::TestSessionState::Paused) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.85f, 0.35f, 1.0f}, "| Test session: paused");
    }
    if (streamed_terrain) {
        const auto focus = streamed_terrain->focus_cell();
        ImGui::SameLine();
        ImGui::Text("| Terrain %u cells @ (%d,%d)", static_cast<unsigned>(streamed_terrain->loaded_cell_count()),
                    focus.x, focus.z);
    }
    ImGui::SameLine();
    if (edit_mode && scene_tab) {
        if (editor_icon_button("gizmo_move", ICON_FA_ARROWS_ALT, "Move [1]"))
            state.gizmo_operation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (editor_icon_button("gizmo_rotate", ICON_FA_SYNC_ALT, "Rotate [2]"))
            state.gizmo_operation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (editor_icon_button("gizmo_scale", ICON_FA_EXPAND_ALT, "Scale [3]"))
            state.gizmo_operation = ImGuizmo::SCALE;
    }
    if (edit_mode && sculpt_tab) {
        draw_sculpt_toolbar(state, streamed_terrain, streamed_foliage, streamed_water, collision, terrain_material,
            camera_position);
    }
    if (state.test_session == EditorState::TestSessionState::Inactive) {
        ImGui::SameLine();
        if (editor_icon_button("test_start", ICON_FA_PLAY, "Start Test (F5)"))
            state.test_session_command = EditorState::TestSessionCommand::Start;
    } else {
        if (state.test_session == EditorState::TestSessionState::Running) {
            ImGui::SameLine();
            if (editor_icon_button("test_pause", ICON_FA_PAUSE, "Pause (F6)"))
                state.test_session_command = EditorState::TestSessionCommand::Pause;
        } else {
            ImGui::SameLine();
            if (editor_icon_button("test_resume", ICON_FA_PLAY, "Resume (F6)"))
                state.test_session_command = EditorState::TestSessionCommand::Resume;
        }
        ImGui::SameLine();
        if (editor_icon_button("test_end", ICON_FA_STOP, "End Test (Shift+F5)"))
            state.test_session_command = EditorState::TestSessionCommand::End;
    }
    const auto available = ImGui::GetContentRegionAvail();
    const ImTextureID active_texture = game_tab ? game_texture : scene_texture;
    if (ui_tab) {
        draw_ui_canvas_viewport(state.ui_canvas_editor, state.project_root,
            [&state](const std::filesystem::path& saved_path) {
                if (!state.ui_canvas_stack || saved_path.empty()) return;
                const auto& editor = state.ui_canvas_editor;
                if (editor.apply_as_hud) {
                    const double health = state.ui_canvas_stack->hud().get_number("player.health").value_or(100.0);
                    const double health_max =
                        state.ui_canvas_stack->hud().get_number("player.healthMax").value_or(100.0);
                    if (state.ui_canvas_stack->set_hud(saved_path))
                        state.ui_canvas_stack->hud().set_health(health, health_max);
                    state.status = "UI HUD canvas saved";
                } else {
                    const std::string id = editor.canvas.id.empty() ? saved_path.stem().generic_string() : editor.canvas.id;
                    // stem of foo.uicanvas.json is often "foo.uicanvas" — prefer asset id.
                    std::string stack_id = id;
                    if (stack_id.size() > 9 && stack_id.substr(stack_id.size() - 9) == ".uicanvas")
                        stack_id = stack_id.substr(0, stack_id.size() - 9);
                    (void)state.ui_canvas_stack->register_canvas(stack_id, saved_path);
                    state.status = "UI modal canvas saved (" + stack_id + ")";
                }
                if (state.lua_runtime) {
                    state.lua_runtime->set_hud_runtime(&state.ui_canvas_stack->hud());
                    state.lua_runtime->set_ui_canvas_stack(state.ui_canvas_stack.get());
                    state.lua_runtime->set_quest_runtime(state.quest_runtime.get());
                    state.lua_runtime->set_standing_runtime(state.standing_runtime.get());
                }
            });
    } else if (world_forge_tab) {
        WorldForgeViewportDrawContext wf_ctx;
        wf_ctx.terrain_edits = &state.terrain_edits;
        wf_ctx.terrain_revision = state.terrain_height_revision;
        wf_ctx.hotspots = &state.ui_hotspots;
        draw_world_forge_viewport(state.world_forge_editor, state.project_root, wf_ctx);
    } else if (design_docs_tab) {
        draw_design_docs_viewport(state);
    } else if (available.x > 1.0f && available.y > 1.0f) {
        ImGui::Image(active_texture, available);
        state.viewport_hovered = ImGui::IsItemHovered();
        const ImVec2 image_min = ImGui::GetItemRectMin();
        const ImVec2 image_max = ImGui::GetItemRectMax();
        const ViewportFrame frame{image_min, image_max, image_max.x - image_min.x, image_max.y - image_min.y};
        const ImVec2 mouse = ImGui::GetMousePos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (game_tab && !state.test_session_active()) {
            const char* hint = "Start a test session (F5) to preview the game view";
            const ImVec2 text_size = ImGui::CalcTextSize(hint);
            const ImVec2 text_pos{image_min.x + (frame.width - text_size.x) * 0.5f,
                image_min.y + (frame.height - text_size.y) * 0.5f};
            draw_list->AddRectFilled(image_min, image_max, IM_COL32(8, 12, 20, 180));
            draw_list->AddText(text_pos, IM_COL32(220, 230, 245, 255), hint);
        } else if (game_tab && state.test_session_active() && state.ui_canvas_stack) {
            state.game_viewport_min = image_min;
            state.game_viewport_max = image_max;
            state.game_viewport_hovered = state.viewport_hovered;
            state.ui_canvas_stack->draw_overlay(draw_list, image_min, image_max);
        } else if (game_tab) {
            state.game_viewport_min.reset();
            state.game_viewport_max.reset();
            state.game_viewport_hovered = false;
        }

        if (scene_tab) {
            if (edit_mode && state.viewport_hovered && !ImGuizmo::IsUsing() && !state.terrain_drag_active &&
                !ImGui::GetDragDropPayload())
                state.hovered = pick_placement(state.scene, state, view, projection, view_projection, frame, mouse);
            else if (!state.viewport_hovered)
                state.hovered.reset();

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(image_min.x, image_min.y, frame.width, frame.height);
            if (edit_mode && state.selected && state.scene.placement(*state.selected)) {
                if (!state.gizmo_entity || *state.gizmo_entity != *state.selected) {
                    const auto transform = state.scene.transform(*state.selected).value();
                    using namespace DirectX;
                    const auto model =
                        XMMatrixScaling(transform.scale[0], transform.scale[1], transform.scale[2]) *
                        XMMatrixRotationQuaternion(
                            XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(transform.rotation.data()))) *
                        XMMatrixTranslation(transform.position[0], transform.position[1], transform.position[2]);
                    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(state.gizmo_matrix.data()), model);
                    state.gizmo_entity = state.selected;
                    state.gizmo_was_using = false;
                    if (!state.terrain_drag_active) state.gizmo_preview = transform;
                }
                if (!state.terrain_drag_active) {
                    ImGuizmo::Manipulate(view.data(), projection.data(), state.gizmo_operation, ImGuizmo::LOCAL,
                        state.gizmo_matrix.data());
                }
                const bool using_now = ImGuizmo::IsUsing();
                if (using_now) {
                    float position[3];
                    float rotation[3];
                    float scale[3];
                    ImGuizmo::DecomposeMatrixToComponents(state.gizmo_matrix.data(), position, rotation, scale);
                    auto preview = state.scene.transform(*state.selected).value();
                    std::copy(position, position + 3, preview.position.begin());
                    std::copy(scale, scale + 3, preview.scale.begin());
                    using namespace DirectX;
                    const auto q = XMQuaternionRotationRollPitchYaw(XMConvertToRadians(rotation[0]),
                        XMConvertToRadians(rotation[1]), XMConvertToRadians(rotation[2]));
                    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(preview.rotation.data()), q);
                    state.gizmo_preview = preview;
                    state.status = "Gizmo preview (release to commit)";
                }
                if (state.gizmo_was_using && !using_now && state.gizmo_preview) {
                    const auto result = state.history.execute(state.scene,
                        std::make_unique<MoveWorldObjectCommand>(*state.selected, *state.gizmo_preview));
                    state.status = result ? "Gizmo transform committed" : result.error().message;
                    if (!result) Logger::instance().write(result.error());
                }
                state.gizmo_was_using = using_now;
            }

            if (edit_mode && state.viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGuizmo::IsOver()) {
                state.left_press_active = true;
                state.left_press_pos = mouse;
                state.left_press_pick =
                    pick_placement(state.scene, state, view, projection, view_projection, frame, mouse);
            }
            if (edit_mode && state.left_press_active && ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                state.viewport_hovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                if (!state.terrain_drag_active && state.left_press_pick && state.selected &&
                    *state.left_press_pick == *state.selected &&
                    std::hypot(mouse.x - state.left_press_pos.x, mouse.y - state.left_press_pos.y) > 4.0f) {
                    state.terrain_drag_active = true;
                    state.gizmo_entity.reset();
                }
                if (state.terrain_drag_active && state.selected) {
                    if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                        auto preview = state.scene.transform(*state.selected).value();
                        preview.position = {static_cast<float>(hit->x), static_cast<float>(hit->y),
                            static_cast<float>(hit->z)};
                        state.gizmo_preview = preview;
                        state.status = "Dragging on terrain (release to commit)";
                    }
                }
            }
            if (edit_mode && state.left_press_active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                if (state.terrain_drag_active && state.selected && state.gizmo_preview) {
                    const auto result = state.history.execute(state.scene,
                        std::make_unique<MoveWorldObjectCommand>(*state.selected, *state.gizmo_preview));
                    state.status = result ? "Object moved on terrain" : result.error().message;
                    if (!result) Logger::instance().write(result.error());
                } else if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                    state.selected = state.left_press_pick;
                    state.inspector_asset_path.reset();
                    state.inspector_player_spawn_entity.reset();
                    state.gizmo_entity.reset();
                    state.gizmo_preview.reset();
                    if (state.left_press_pick)
                        state.status = "Object selected from viewport";
                    else if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                        state.placement_cursor = hit;
                        state.status = "Placement cursor set on terrain";
                    } else
                        state.status = "Nothing selected";
                }
                state.left_press_active = false;
                state.terrain_drag_active = false;
            }

            if (edit_mode && ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(k_prefab_drag_payload)) {
                    const std::string path(static_cast<const char*>(payload->Data));
                    const auto hit = viewport_raycast(collision, frame, view, projection, mouse);
                    place_prefab_at(state, path, hit ? std::optional<WorldPosition>{*hit} : state.placement_cursor);
                }
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload()) {
                    if (payload->IsDelivery() == false && std::strcmp(payload->DataType, k_prefab_drag_payload) == 0) {
                        const std::string path(static_cast<const char*>(payload->Data));
                        if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                            TransformComponent preview;
                            preview.position = {static_cast<float>(hit->x), static_cast<float>(hit->y),
                                static_cast<float>(hit->z)};
                            state.drop_preview = preview;
                            state.drop_preview_prefab = path;
                            state.placement_cursor = hit;
                        }
                    } else {
                        state.drop_preview.reset();
                        state.drop_preview_prefab.reset();
                    }
                }
                ImGui::EndDragDropTarget();
            } else if (edit_mode) {
                state.drop_preview.reset();
                state.drop_preview_prefab.reset();
            }
        }

        if (sculpt_tab && edit_mode) {
            state.hovered.reset();

            const auto apply_terrain_height_brush_at = [&](const WorldPosition& hit, bool lower) {
                const auto touched = state.terrain_edits.apply_brush(static_cast<float>(hit.x),
                    static_cast<float>(hit.z), state.terrain_brush_radius, state.terrain_brush_strength, lower);
                if (!touched) {
                    state.status = touched.error().message;
                    Logger::instance().write(touched.error());
                    return;
                }
                for (const auto& cell : touched.value()) {
                    if (state.terrain_brush_before.find(cell) == state.terrain_brush_before.end())
                        state.terrain_brush_before[cell] = state.terrain_edits.cell_deltas_or_empty(cell);
                    state.terrain_brush_touched.insert(cell);
                }
                state.terrain_edits_dirty = true; ++state.terrain_height_revision;
                if (streamed_terrain && collision && terrain_material) {
                    std::set<CellCoord> reload;
                    const auto loaded = streamed_terrain->loaded_cell_coordinates();
                    for (const auto& cell : touched.value())
                        if (loaded.find(cell) != loaded.end()) reload.insert(cell);
                    if (!reload.empty()) {
                        const auto lookup = editor_paint_material_lookup(state);
                        const auto reloaded = streamed_terrain->reload_cells(*collision, reload,
                            terrain_material->physics, &state.terrain_edits, &state.terrain_paint, lookup);
                        if (!reloaded) Logger::instance().write(reloaded.error());
                    }
                }
                state.status = lower ? "Lowering terrain" : "Raising terrain";
            };

            const auto apply_terrain_flatten_brush_at = [&](const WorldPosition& hit) {
                if (!state.terrain_flatten_target_valid) {
                    state.terrain_flatten_target = sample_terrain_height(static_cast<float>(hit.x), static_cast<float>(hit.z));
                    state.terrain_flatten_target_valid = true;
                }
                const auto touched = state.terrain_edits.apply_flatten_brush(static_cast<float>(hit.x),
                    static_cast<float>(hit.z), state.terrain_brush_radius, state.terrain_brush_strength,
                    state.terrain_flatten_target);
                if (!touched) {
                    state.status = touched.error().message;
                    Logger::instance().write(touched.error());
                    return;
                }
                for (const auto& cell : touched.value()) {
                    if (state.terrain_brush_before.find(cell) == state.terrain_brush_before.end())
                        state.terrain_brush_before[cell] = state.terrain_edits.cell_deltas_or_empty(cell);
                    state.terrain_brush_touched.insert(cell);
                }
                state.terrain_edits_dirty = true; ++state.terrain_height_revision;
                if (streamed_terrain && collision && terrain_material) {
                    std::set<CellCoord> reload;
                    const auto loaded = streamed_terrain->loaded_cell_coordinates();
                    for (const auto& cell : touched.value())
                        if (loaded.find(cell) != loaded.end()) reload.insert(cell);
                    if (!reload.empty()) {
                        const auto lookup = editor_paint_material_lookup(state);
                        const auto reloaded = streamed_terrain->reload_cells(*collision, reload,
                            terrain_material->physics, &state.terrain_edits, &state.terrain_paint, lookup);
                        if (!reloaded) Logger::instance().write(reloaded.error());
                    }
                }
                state.status = "Flattening terrain";
            };

            const auto apply_terrain_paint_brush_at = [&](const WorldPosition& hit) {
                if (state.terrain_paint_brush_material.empty()) {
                    state.status = "Choose a paint material first";
                    return;
                }
                const std::uint16_t material_index =
                    state.terrain_paint.ensure_material_index(state.terrain_paint_brush_material);
                const auto touched = state.terrain_paint.apply_material_brush(static_cast<float>(hit.x),
                    static_cast<float>(hit.z), state.terrain_brush_radius, material_index);
                if (!touched) {
                    state.status = touched.error().message;
                    Logger::instance().write(touched.error());
                    return;
                }
                for (const auto& cell : touched.value()) {
                    if (state.terrain_paint_brush_before.find(cell) == state.terrain_paint_brush_before.end())
                        state.terrain_paint_brush_before[cell] = state.terrain_paint.cell_indices_or_empty(cell);
                    state.terrain_paint_brush_touched.insert(cell);
                }
                state.terrain_paint_dirty = true;
                if (streamed_terrain && terrain_material) {
                    std::set<CellCoord> reload;
                    const auto loaded = streamed_terrain->loaded_cell_coordinates();
                    for (const auto& cell : touched.value())
                        if (loaded.find(cell) != loaded.end()) reload.insert(cell);
                    if (!reload.empty()) {
                        const auto lookup = editor_paint_material_lookup(state);
                        const auto reloaded = streamed_terrain->reload_cell_meshes(
                            reload, &state.terrain_edits, &state.terrain_paint, lookup);
                        if (!reloaded) Logger::instance().write(reloaded.error());
                    }
                }
                state.status = "Painting terrain";
            };

            const auto apply_foliage_brush_at = [&](const WorldPosition& hit, bool shift_held) {
                if (state.foliage_layers.layers.empty()) {
                    state.status = "Load foliage layers before painting";
                    return;
                }
                const bool erase = state.foliage_brush_mode == EditorState::FoliageBrushMode::Erase ||
                    (shift_held && state.foliage_brush_mode != EditorState::FoliageBrushMode::Erase);
                const auto touched = [&]() -> Result<std::set<CellCoord>> {
                    if (state.foliage_brush_mode == EditorState::FoliageBrushMode::Mixed) {
                        const auto mix = default_meadow_mix_weights(state.foliage_layers);
                        return state.foliage_density.apply_foliage_mixed_brush(static_cast<float>(hit.x),
                            static_cast<float>(hit.z), state.terrain_brush_radius, state.foliage_brush_strength, mix,
                            erase);
                    }
                    return state.foliage_density.apply_foliage_brush(static_cast<float>(hit.x), static_cast<float>(hit.z),
                        state.terrain_brush_radius, state.foliage_brush_strength, state.foliage_brush_layer, erase);
                }();
                if (!touched) {
                    state.status = touched.error().message;
                    Logger::instance().write(touched.error());
                    return;
                }
                for (const auto& cell : touched.value()) {
                    if (state.foliage_brush_before.find(cell) == state.foliage_brush_before.end())
                        state.foliage_brush_before[cell] = state.foliage_density.cell_snapshot_or_empty(cell);
                    state.foliage_brush_touched.insert(cell);
                }
                state.foliage_density_dirty = true;
                if (streamed_foliage && streamed_terrain) {
                    std::set<CellCoord> reload;
                    const auto loaded = streamed_terrain->loaded_cell_coordinates();
                    for (const auto& cell : touched.value())
                        if (loaded.find(cell) != loaded.end()) reload.insert(cell);
                    if (!reload.empty()) {
                        streamed_foliage->set_palette(&state.foliage_layers);
                        streamed_foliage->set_density(&state.foliage_density);
                        (void)streamed_foliage->rebuild_cells(reload, camera_position);
                    }
                }
                state.status = erase ? "Erasing foliage" : "Painting foliage";
            };

            const auto apply_water_brush_at = [&](const WorldPosition& hit, bool shift_held) {
                const bool erase = shift_held;
                const auto touched = erase
                    ? state.water_store.apply_erase_brush(static_cast<float>(hit.x), static_cast<float>(hit.z),
                          state.terrain_brush_radius, state.terrain_brush_strength)
                    : state.water_store.apply_place_brush(static_cast<float>(hit.x), static_cast<float>(hit.z),
                          state.terrain_brush_radius, state.terrain_brush_strength);
                if (!touched) {
                    state.status = touched.error().message;
                    Logger::instance().write(touched.error());
                    return;
                }
                for (const auto& cell : touched.value()) {
                    if (state.water_brush_before.find(cell) == state.water_brush_before.end())
                        state.water_brush_before[cell] = WaterCellSnapshot{state.water_store.cell_fill_or_empty(cell)};
                    state.water_brush_touched.insert(cell);
                }
                state.water_dirty = true;
                if (streamed_water) {
                    std::set<CellCoord> reload;
                    const auto loaded = streamed_water->loaded_cell_coordinates();
                    for (const auto& cell : touched.value())
                        if (loaded.find(cell) != loaded.end()) reload.insert(cell);
                    if (!reload.empty()) {
                        const auto reloaded = streamed_water->reload_cells(reload, &state.water_store);
                        if (!reloaded) Logger::instance().write(reloaded.error());
                    }
                }
                state.status = erase ? "Erasing water" : "Placing water";
            };

            if (state.viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                state.terrain_brush_active = true;
                state.terrain_brush_before.clear();
                state.terrain_brush_touched.clear();
                state.terrain_paint_brush_before.clear();
                state.terrain_paint_brush_touched.clear();
                state.foliage_brush_before.clear();
                state.foliage_brush_touched.clear();
                state.water_brush_before.clear();
                state.water_brush_touched.clear();
                state.terrain_flatten_target_valid = false;
                if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                    if (state.sculpt_tool == EditorState::SculptTool::Paint)
                        apply_terrain_paint_brush_at(*hit);
                    else if (state.sculpt_tool == EditorState::SculptTool::Foliage)
                        apply_foliage_brush_at(*hit, ImGui::GetIO().KeyShift);
                    else if (state.sculpt_tool == EditorState::SculptTool::Water)
                        apply_water_brush_at(*hit, ImGui::GetIO().KeyShift);
                    else if (state.sculpt_tool == EditorState::SculptTool::Flatten)
                        apply_terrain_flatten_brush_at(*hit);
                    else
                        apply_terrain_height_brush_at(*hit, ImGui::GetIO().KeyShift);
                }
            }
            if (state.terrain_brush_active && ImGui::IsMouseDown(ImGuiMouseButton_Left) && state.viewport_hovered) {
                if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                    if (state.sculpt_tool == EditorState::SculptTool::Paint)
                        apply_terrain_paint_brush_at(*hit);
                    else if (state.sculpt_tool == EditorState::SculptTool::Foliage)
                        apply_foliage_brush_at(*hit, ImGui::GetIO().KeyShift);
                    else if (state.sculpt_tool == EditorState::SculptTool::Water)
                        apply_water_brush_at(*hit, ImGui::GetIO().KeyShift);
                    else if (state.sculpt_tool == EditorState::SculptTool::Flatten)
                        apply_terrain_flatten_brush_at(*hit);
                    else
                        apply_terrain_height_brush_at(*hit, ImGui::GetIO().KeyShift);
                }
            }
            if (state.terrain_brush_active && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
                commit_active_terrain_stroke(state);

            if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                float sx = 0.0f;
                float sy = 0.0f;
                float depth = 0.0f;
                if (project_world_to_screen(view_projection, frame, static_cast<float>(hit->x),
                        static_cast<float>(hit->y), static_cast<float>(hit->z), sx, sy, depth)) {
                    float edge_x = 0.0f;
                    float edge_y = 0.0f;
                    float edge_depth = 0.0f;
                    float radius_px = 8.0f;
                    if (project_world_to_screen(view_projection, frame,
                            static_cast<float>(hit->x) + state.terrain_brush_radius,
                            static_cast<float>(hit->y), static_cast<float>(hit->z), edge_x, edge_y, edge_depth))
                        radius_px = std::max(4.0f, std::hypot(edge_x - sx, edge_y - sy));
                    radius_px = std::min(radius_px, std::max(frame.width, frame.height) * 0.45f);
                    const ImU32 brush_color = state.sculpt_tool == EditorState::SculptTool::Paint
                                                  ? IM_COL32(140, 210, 255, 180)
                                                  : state.sculpt_tool == EditorState::SculptTool::Foliage
                                                        ? IM_COL32(120, 220, 140, 180)
                                                        : state.sculpt_tool == EditorState::SculptTool::Water
                                                              ? IM_COL32(90, 170, 255, 180)
                                                              : state.sculpt_tool == EditorState::SculptTool::Flatten
                                                                    ? IM_COL32(180, 160, 255, 180)
                                                                    : IM_COL32(255, 210, 120, 180);
                    draw_list->AddCircle({sx, sy}, radius_px, brush_color, 0, 2.0f);
                    draw_list->AddCircleFilled({sx, sy}, 2.0f, brush_color);
                }
            }
        } else if (state.terrain_brush_active) {
            commit_active_terrain_stroke(state);
        }

            draw_viewport_markers(state, frame, view_projection);
            if (!(sculpt_tab && edit_mode))
                draw_viewport_selection_overlays(state, frame, view_projection);
            draw_collision_debug_overlays(collision, state, frame, view_projection, character, interactions, combat);
            draw_authored_collider_overlays(state, frame, view_projection);
            draw_world_forge_map_marker_overlays(state, frame, view_projection);
            if (state.drop_preview_prefab)
                ImGui::SetTooltip("Drop %s here", state.drop_preview_prefab->c_str());
    }
    ImGui::End();
    process_test_session_ui_input(state);

    ImGui::SetNextWindowPos(origin, ImGuiCond_Always);
    ImGui::SetNextWindowSize({left, top * 0.48f}, ImGuiCond_Always);
    ImGui::Begin("Scene Hierarchy", nullptr, locked);
    for (const auto& id : state.scene.entity_ids()) {
        const auto name = state.scene.name(id).value_or("Entity");
        const bool is_selected = state.selected && *state.selected == id;
        const bool is_hovered = state.hovered && *state.hovered == id;
        if (is_hovered && !is_selected) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.40f, 0.50f, 0.55f));
        if (ImGui::Selectable((name + "##" + id.str()).c_str(), is_selected)) {
            state.selected = id;
            state.inspector_asset_path.reset();
            state.inspector_player_spawn_entity.reset();
            state.gizmo_entity.reset();
            state.gizmo_preview.reset();
        }
        if (ImGui::IsItemHovered()) state.hovered = id;
        if (is_hovered && !is_selected) ImGui::PopStyleColor();
    }
    ImGui::End();

    ImGui::SetNextWindowPos({origin.x + left + center, origin.y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({right, extent.y}, ImGuiCond_Always);
    ImGui::Begin("Inspector", nullptr, locked);
    if (state.inspector_asset_path) {
        const auto normalized = normalize_asset_path(*state.inspector_asset_path);
        if (ImGui::Button("Back")) state.inspector_asset_path.reset();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", normalized.c_str());
        if (path_ends_with(normalized, ".character.json"))
            draw_character_asset_inspector(state);
        else if (path_ends_with(normalized, ".camera.json"))
            draw_camera_asset_inspector(state);
        else if (path_ends_with(normalized, ".material.json"))
            draw_material_asset_inspector(state, terrain_material, streamed_terrain, collision);
        else
            ImGui::TextDisabled("Unsupported asset type for inspector editing.");
    } else if (state.selected && state.scene.contains(*state.selected)) {
        if (!state.rename_target || *state.rename_target != *state.selected) {
            state.rename_target = state.selected;
            const auto current_name = state.scene.name(*state.selected).value_or("Entity");
            std::snprintf(state.rename_buffer, sizeof(state.rename_buffer), "%s", current_name.c_str());
        }
        ImGui::TextDisabled("%s", state.selected->str().c_str());
        ImGui::InputText("Name", state.rename_buffer, sizeof(state.rename_buffer));
        ImGui::SameLine();
        if (ImGui::Button("Rename")) {
            const auto result = state.history.execute(
                state.scene, std::make_unique<RenameEntityCommand>(*state.selected, state.rename_buffer));
            state.status = result ? "Entity renamed" : result.error().message;
            if (!result) Logger::instance().write(result.error());
        }
        auto transform = state.scene.transform(*state.selected).value();
        ImGui::InputFloat3("Position", transform.position.data());
        ImGui::InputFloat4("Rotation", transform.rotation.data());
        ImGui::InputFloat3("Scale", transform.scale.data());
        if (ImGui::Button("Apply Transform")) {
            const Result<void> result = state.scene.placement(*state.selected)
                                            ? state.history.execute(state.scene, std::make_unique<MoveWorldObjectCommand>(
                                                                                  *state.selected, transform))
                                            : state.scene.set_transform(*state.selected, transform);
            state.status = result ? "Transform applied" : result.error().message;
            if (!result) Logger::instance().write(result.error());
            state.gizmo_entity.reset();
            state.gizmo_preview.reset();
        }
        if (auto placement = state.scene.placement(*state.selected)) {
            ImGui::Separator();
            ImGui::Text("Prefab: %s", placement->prefab_asset.c_str());
            ImGui::Text("Cell: %d, %d", placement->cell.x, placement->cell.z);
            std::optional<std::string> character_path = placement->character_asset;
            if (!character_path)
                character_path = resolve_character_asset_for_prefab(state, placement->prefab_asset);
            if (character_path && !placement->character_asset)
                (void)state.scene.set_placement_character_asset(*state.selected, *character_path);
            if (character_path) {
                const auto char_path = normalize_asset_path(*character_path);
                const bool entity_changed =
                    !state.inspector_player_spawn_entity || *state.inspector_player_spawn_entity != *state.selected;
                if (entity_changed) {
                    state.inspector_player_spawn_entity = *state.selected;
                    if (placement->character_settings)
                        state.character_asset = *placement->character_settings;
                    else if (const auto loaded = CharacterAsset::load(state.project_root / char_path); loaded)
                        state.character_asset = loaded.value();
                    state.character_asset_dirty = false;
                }
                ImGui::Separator();
                ImGui::TextDisabled("Player Spawn");
                ImGui::TextDisabled("Asset: %s", char_path.c_str());
                draw_character_asset_inspector(state, true);
            }
            if (const auto* prefab = find_prefab(state.prefab_catalog, placement->prefab_asset)) {
                if (prefab->is_compositional()) {
                    ImGui::Text("Parts: %zu", prefab->parts.size());
                    for (std::size_t index = 0; index < prefab->parts.size(); ++index) {
                        const auto& part = prefab->parts[index];
                        const char* mesh_label = part.mesh.primitive ? part.mesh.primitive->c_str()
                                                                     : part.mesh.asset ? part.mesh.asset->c_str() : "?";
                        ImGui::BulletText("%s (%s)", part.name.c_str(), mesh_label);
                    }
                }
                if (const auto seeded = state.scene.ensure_authored_components_seeded(*state.selected, *prefab);
                    seeded && seeded.value())
                    mark_scene_dirty(state);
            }
            ImGui::Separator();
            ImGui::Text("Components");
            if (auto components = state.scene.authored_components(*state.selected)) {
                for (const auto& entry : components->entries) {
                    ImGui::PushID(entry.id.c_str());
                    const std::string type_label = authored_component_type_name(entry.type);
                    const std::string node_label = entry.id + " [" + type_label + "]";
                    const bool open = ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s%s", entry.from_prefab ? "prefab" : "instance",
                        entry.overridden ? " override" : "");
                    if (open) {
                        AuthoredComponentEntry draft = entry;
                        bool commit = false;
                        if (draft.type == AuthoredComponentType::Collider) {
                            commit = draw_collider_property_fields(draft.collider);
                        } else if (draft.type == AuthoredComponentType::Animator) {
                            state.inspector_component_edit_id = entry.id;
                            commit = draw_animator_property_fields(state, draft.animator);
                        } else if (draft.type == AuthoredComponentType::Rigidbody) {
                            commit = draw_rigidbody_property_fields(draft.rigidbody);
                        } else {
                            if (!state.inspector_component_edit_id ||
                                *state.inspector_component_edit_id != entry.id) {
                                state.inspector_component_edit_id = entry.id;
                                std::snprintf(state.inspector_script_kind_buf,
                                    sizeof(state.inspector_script_kind_buf), "%s", entry.script.kind.c_str());
                                std::snprintf(state.inspector_script_binding_buf,
                                    sizeof(state.inspector_script_binding_buf), "%s",
                                    entry.script.binding_id.c_str());
                            }
                            commit = draw_script_binding_property_fields(draft.script,
                                state.inspector_script_kind_buf, sizeof(state.inspector_script_kind_buf),
                                state.inspector_script_binding_buf, sizeof(state.inspector_script_binding_buf));
                            draft.script.kind = state.inspector_script_kind_buf;
                            draft.script.binding_id = state.inspector_script_binding_buf;
                            if (ImGui::SmallButton("Open Script"))
                                open_script_binding(state, draft.script.kind, draft.script.binding_id);
                        }
                        if (ImGui::Button("Apply")) commit = true;
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Remove")) {
                            const auto result = state.history.execute(state.scene,
                                std::make_unique<RemoveEntityComponentCommand>(*state.selected, entry.id));
                            state.status = result ? "Component removed" : result.error().message;
                            if (!result) Logger::instance().write(result.error());
                            else mark_scene_dirty(state);
                            commit = false;
                        }
                        if (commit) commit_entity_component_edit(state, std::move(draft));
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            } else {
                ImGui::TextDisabled("No components");
            }
            if (ImGui::Button("Add Collider")) {
                AuthoredComponentEntry entry;
                entry.type = AuthoredComponentType::Collider;
                entry.id = "collider-" + std::to_string(state.scene.authored_components(*state.selected)
                        ? state.scene.authored_components(*state.selected)->entries.size()
                        : 0);
                entry.collider.id = entry.id;
                entry.collider.shape = PrefabCollisionShape::Box;
                entry.collider.half_extent = {0.5f, 0.5f, 0.5f};
                const auto result = state.history.execute(state.scene,
                    std::make_unique<AddEntityComponentCommand>(*state.selected, std::move(entry)));
                state.status = result ? "Collider added" : result.error().message;
                if (!result) Logger::instance().write(result.error());
                else mark_scene_dirty(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Script Binding")) {
                AuthoredComponentEntry entry;
                entry.type = AuthoredComponentType::ScriptBinding;
                entry.id = "script-" + std::to_string(state.scene.authored_components(*state.selected)
                        ? state.scene.authored_components(*state.selected)->entries.size()
                        : 0);
                entry.script.kind = "handler";
                entry.script.binding_id = "new_handler";
                const auto result = state.history.execute(state.scene,
                    std::make_unique<AddEntityComponentCommand>(*state.selected, std::move(entry)));
                state.status = result ? "Script binding added" : result.error().message;
                if (!result) Logger::instance().write(result.error());
                else mark_scene_dirty(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Animator")) {
                AuthoredComponentEntry entry;
                entry.type = AuthoredComponentType::Animator;
                entry.id = "animator-" + std::to_string(state.scene.authored_components(*state.selected)
                        ? state.scene.authored_components(*state.selected)->entries.size()
                        : 0);
                const auto controllers = collect_asset_paths(state, ".animator.json");
                entry.animator.controller = controllers.empty() ? std::string{} : controllers.front();
                // Prefer idle as the instance override when that state exists on the controller.
                const auto states = collect_animator_state_names(state, entry.animator.controller);
                const bool has_idle =
                    std::find(states.begin(), states.end(), "idle") != states.end() ||
                    std::find(states.begin(), states.end(), "Idle") != states.end();
                if (has_idle)
                    entry.animator.default_state =
                        std::find(states.begin(), states.end(), "idle") != states.end() ? "idle" : "Idle";
                const auto result = state.history.execute(state.scene,
                    std::make_unique<AddEntityComponentCommand>(*state.selected, std::move(entry)));
                state.status = result ? "Animator added" : result.error().message;
                if (!result) Logger::instance().write(result.error());
                else mark_scene_dirty(state);
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Rigidbody")) {
                AuthoredComponentEntry entry;
                entry.type = AuthoredComponentType::Rigidbody;
                entry.id = "rigidbody-" + std::to_string(state.scene.authored_components(*state.selected)
                        ? state.scene.authored_components(*state.selected)->entries.size()
                        : 0);
                entry.rigidbody = RigidbodyComponentData{};
                const auto result = state.history.execute(state.scene,
                    std::make_unique<AddEntityComponentCommand>(*state.selected, std::move(entry)));
                state.status = result ? "Rigidbody added" : result.error().message;
                if (!result) Logger::instance().write(result.error());
                else mark_scene_dirty(state);
            }
            if (ImGui::Button("Snap To Terrain") && collision) {
                const auto current = state.scene.transform(*state.selected).value();
                if (const auto hit = collision->ray_cast(
                        {current.position[0], current.position[1] + 20.0, current.position[2]}, {0.0, -40.0, 0.0})) {
                    if (hit.value()) {
                        auto snapped = current;
                        snapped.position = {static_cast<float>(hit.value()->position.x),
                                            static_cast<float>(hit.value()->position.y),
                                            static_cast<float>(hit.value()->position.z)};
                        const auto result = state.history.execute(
                            state.scene, std::make_unique<MoveWorldObjectCommand>(*state.selected, snapped));
                        state.status = result ? "Snapped to terrain" : result.error().message;
                        if (!result) Logger::instance().write(result.error());
                        state.gizmo_entity.reset();
                        state.gizmo_preview.reset();
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Duplicate")) duplicate_selected(state);
            if (ImGui::Button("Remove Object")) remove_selected(state);
        }
    } else if (state.prefab_edit_path)
        ImGui::TextDisabled("Editing prefab: %s", state.prefab_edit_path->c_str());
    else
        draw_play_session_inspector(state);
    ImGui::End();

    if (state.prefab_edit_path) {
        ImGui::SetNextWindowPos({origin.x + left + center, origin.y + top * 0.55f}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({right, top * 0.45f}, ImGuiCond_Always);
        ImGui::Begin("Prefab Editor", nullptr, locked);
        const auto normalized = normalize_asset_path(*state.prefab_edit_path);
        auto prefab_it = state.prefab_catalog.find(normalized);
        if (prefab_it == state.prefab_catalog.end()) {
            ImGui::TextDisabled("Prefab not loaded.");
        } else {
            auto& prefab = prefab_it->second;
            ImGui::Text("%s", state.prefab_edit_path->c_str());
            ImGui::Text("Parts: %zu", prefab.parts.size());
            if (!state.prefab_edit_part && !prefab.parts.empty()) state.prefab_edit_part = 0;
            for (std::size_t index = 0; index < prefab.parts.size(); ++index) {
                const auto& part = prefab.parts[index];
                const bool selected = state.prefab_edit_part && *state.prefab_edit_part == index;
                const char* mesh_label = part.mesh.primitive ? part.mesh.primitive->c_str()
                                                             : part.mesh.asset ? part.mesh.asset->c_str() : "?";
                if (ImGui::Selectable((part.name + "##part" + std::to_string(index)).c_str(), selected)) {
                    state.prefab_edit_part = index;
                    state.prefab_part_gizmo_was_using = false;
                }
                if (selected) ImGui::SetTooltip("%s", mesh_label);
            }
            if (state.prefab_edit_part && *state.prefab_edit_part < prefab.parts.size()) {
                auto& part = prefab.parts[*state.prefab_edit_part];
                ImGui::Separator();
                char name_buffer[128];
                std::snprintf(name_buffer, sizeof(name_buffer), "%s", part.name.c_str());
                if (ImGui::InputText("Part Name", name_buffer, sizeof(name_buffer))) part.name = name_buffer;
                ImGui::InputFloat3("Local Position", part.transform.position.data());
                ImGui::InputFloat4("Local Rotation", part.transform.rotation.data());
                ImGui::InputFloat3("Local Scale", part.transform.scale.data());
                const char* mesh_label = part.mesh.primitive ? part.mesh.primitive->c_str()
                                                             : part.mesh.asset ? part.mesh.asset->c_str() : "?";
                ImGui::TextDisabled("Mesh: %s", mesh_label);
                const auto material_paths = collect_asset_paths(state, ".material.json");
                std::string material_path = part.mesh.material ? normalize_asset_path(*part.mesh.material) : std::string{};
                if (draw_asset_path_combo("Material", material_path, material_paths, "(none)")) {
                    if (material_path.empty()) part.mesh.material.reset();
                    else part.mesh.material = material_path;
                    state.prefab_meshes_dirty = true;
                }
                if (part.mesh.material) {
                    if (const MaterialAsset* material = editor_material(state, *part.mesh.material)) {
                        ImGui::ColorButton("##part_mat_preview",
                            ImVec4{material->base_color[0], material->base_color[1], material->base_color[2],
                                material->base_color[3]});
                        ImGui::SameLine();
                        ImGui::TextDisabled("Roughness %.2f | Metallic %.2f", material->roughness, material->metallic);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Edit##part_mat")) {
                            state.inspector_asset_path = *part.mesh.material;
                            state.material_asset = *material;
                            state.material_asset_dirty = false;
                        }
                    }
                } else if (ImGui::ColorEdit3("Part Color", part.mesh.color.data())) {
                    state.prefab_meshes_dirty = true;
                }
            }
            const auto lookup_material = make_material_lookup(&state.material_cache);
            state.prefab_bounds[normalized] = prefab.bounds(state.mesh_bounds, lookup_material);
            ImGui::Separator();
            ImGui::Text("Collision / Components");
            ImGui::Text("Colliders: %zu | Scripts: %zu | Animators: %zu | Rigidbodies: %zu", prefab.collision.size(),
                prefab.script_bindings.size(), prefab.animators.size(), prefab.rigidbodies.size());
            for (std::size_t index = 0; index < prefab.collision.size(); ++index) {
                auto& volume = prefab.collision[index];
                ImGui::PushID(static_cast<int>(index));
                const char* shape_label = volume.shape == PrefabCollisionShape::Box       ? "box"
                                          : volume.shape == PrefabCollisionShape::Sphere ? "sphere"
                                                                                         : "capsule";
                const bool open =
                    ImGui::TreeNodeEx((volume.id + " (" + shape_label + ")").c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (open) {
                    (void)draw_collider_property_fields(volume);
                    if (ImGui::SmallButton("Remove##col")) {
                        prefab.collision.erase(prefab.collision.begin() + static_cast<std::ptrdiff_t>(index));
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            for (std::size_t index = 0; index < prefab.script_bindings.size(); ++index) {
                auto& binding = prefab.script_bindings[index];
                ImGui::PushID(static_cast<int>(1000 + index));
                const std::string node_label =
                    binding.id + " script:" + binding.kind + "/" + binding.binding_id;
                const bool open = ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (open) {
                    const std::string edit_key = normalized + "#" + binding.id;
                    if (!state.prefab_component_edit_id || *state.prefab_component_edit_id != edit_key) {
                        state.prefab_component_edit_id = edit_key;
                        std::snprintf(state.prefab_script_kind_buf, sizeof(state.prefab_script_kind_buf), "%s",
                            binding.kind.c_str());
                        std::snprintf(state.prefab_script_binding_buf, sizeof(state.prefab_script_binding_buf), "%s",
                            binding.binding_id.c_str());
                    }
                    ScriptBindingComponentData script;
                    script.kind = binding.kind;
                    script.binding_id = binding.binding_id;
                    const bool commit = draw_script_binding_property_fields(script, state.prefab_script_kind_buf,
                        sizeof(state.prefab_script_kind_buf), state.prefab_script_binding_buf,
                        sizeof(state.prefab_script_binding_buf));
                    binding.kind = state.prefab_script_kind_buf;
                    binding.binding_id = state.prefab_script_binding_buf;
                    if (commit) {
                        binding.kind = script.kind;
                        binding.binding_id = script.binding_id;
                    }
                    if (ImGui::SmallButton("Open Script"))
                        open_script_binding(state, binding.kind, binding.binding_id);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Remove##script")) {
                        prefab.script_bindings.erase(
                            prefab.script_bindings.begin() + static_cast<std::ptrdiff_t>(index));
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            for (std::size_t index = 0; index < prefab.rigidbodies.size(); ++index) {
                auto& rigidbody = prefab.rigidbodies[index];
                ImGui::PushID(static_cast<int>(2000 + index));
                const std::string node_label = rigidbody.id + " [" + rigidbody.motion_type + "]";
                const bool open = ImGui::TreeNodeEx(node_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
                if (open) {
                    RigidbodyComponentData draft;
                    draft.motion_type = rigidbody.motion_type;
                    draft.mass = rigidbody.mass;
                    draft.linear_damping = rigidbody.linear_damping;
                    draft.angular_damping = rigidbody.angular_damping;
                    draft.use_gravity = rigidbody.use_gravity;
                    draft.freeze_rotation = rigidbody.freeze_rotation;
                    if (draw_rigidbody_property_fields(draft)) {
                        rigidbody.motion_type = draft.motion_type;
                        rigidbody.mass = draft.mass;
                        rigidbody.linear_damping = draft.linear_damping;
                        rigidbody.angular_damping = draft.angular_damping;
                        rigidbody.use_gravity = draft.use_gravity;
                        rigidbody.freeze_rotation = draft.freeze_rotation;
                    }
                    if (ImGui::SmallButton("Remove##rb")) {
                        prefab.rigidbodies.erase(prefab.rigidbodies.begin() + static_cast<std::ptrdiff_t>(index));
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Prefab Collider")) {
                PrefabCollisionVolume volume;
                volume.id = "collision-" + std::to_string(prefab.collision.size());
                volume.shape = PrefabCollisionShape::Box;
                volume.half_extent = {0.5f, 0.5f, 0.5f};
                prefab.collision.push_back(std::move(volume));
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Prefab Script")) {
                PrefabScriptBinding binding;
                binding.id = "script-" + std::to_string(prefab.script_bindings.size());
                binding.kind = "handler";
                binding.binding_id = "new_handler";
                prefab.script_bindings.push_back(std::move(binding));
            }
            ImGui::SameLine();
            if (ImGui::Button("Add Prefab Rigidbody")) {
                PrefabRigidbody rigidbody;
                rigidbody.id = "rigidbody-" + std::to_string(prefab.rigidbodies.size());
                prefab.rigidbodies.push_back(std::move(rigidbody));
            }
            if (ImGui::Button("Save Prefab")) {
                const auto saved = prefab.save(state.project_root / normalized);
                state.status = saved ? "Prefab saved" : saved.error().message;
                if (!saved) Logger::instance().write(saved.error());
                else {
                    state.prefab_bounds[normalized] = prefab.bounds(state.mesh_bounds, lookup_material);
                    state.prefab_meshes_dirty = true;
                    const auto propagated = state.scene.propagate_prefab_components(normalized, prefab);
                    if (propagated > 0) {
                        mark_scene_dirty(state);
                        state.status = "Prefab saved; propagated to " + std::to_string(propagated) + " instance(s)";
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                state.prefab_edit_path.reset();
                state.prefab_edit_part.reset();
                state.prefab_part_gizmo_was_using = false;
                state.status = "Prefab edit closed";
            }
        }
        ImGui::End();
    }

    ImGui::SetNextWindowPos({origin.x, origin.y + top * 0.48f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({left, extent.y - top * 0.48f}, ImGuiCond_Always);
    ImGui::Begin("Asset Browser", nullptr, locked);
    draw_asset_browser(state);
    ImGui::End();

    ImGui::SetNextWindowPos({origin.x + left, origin.y + top}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({center, bottom}, ImGuiCond_Always);
    ImGui::Begin("Diagnostics", nullptr, locked);
    ImGui::Checkbox("Show collision debug", &state.show_collision_debug);
    ImGui::Checkbox("Show World Forge map markers", &state.show_world_forge_map_markers);
    ImGui::BeginDisabled(!state.show_world_forge_map_markers ||
                         !selected_world_forge_marker_world(state.world_forge_editor).has_value());
    if (ImGui::Button("Focus selected WF marker##FocusWorldForgeMarker"))
        state.request_focus_world_forge_marker = true;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Select an anchored region/POI in World Forge Map, then focus the Scene camera on it");
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::Text("Live automation (MCP)");
    if (ImGui::Checkbox("Enable MCP connection", &state.live_automation_enabled)) {
        state.status = state.live_automation_enabled ? "Enabling live automation..."
                                                     : "Disabling live automation...";
        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "ui_toggle",
            {{"enabled", state.live_automation_enabled ? "true" : "false"}});
    }
    if (state.live_automation_enabled) {
        if (!state.project_root.empty()) {
            ImGui::TextWrapped("Pipe: %s", editor_bridge_pipe_name(state.project_root).c_str());
            if (AutomationTrace::enabled()) {
                ImGui::TextDisabled("Trace: %s",
                    AutomationTrace::log_path(AutomationTraceChannel::EditorBridge).generic_string().c_str());
            }
        }
        ImGui::TextDisabled("Connect Cursor MCP (engine mcp --project <dir>) after enabling this.");
    } else {
        ImGui::TextDisabled("Disabled for this session. Agents: engine_editor_live enable — or check this box.");
    }
    draw_project_sync_panel(state);
    if (character) {
        ImGui::Separator();
        ImGui::Text("Active player controller");
        ImGui::Text("Max speed: %.2f m/s", character->config().max_speed);
        ImGui::Text("Step height: %.2f m", character->config().step_height);
        const auto velocity = character->linear_velocity();
        const float horizontal_velocity =
            std::sqrt(velocity[0] * velocity[0] + velocity[2] * velocity[2]);
        ImGui::Text("Velocity: %.2f, %.2f, %.2f m/s", velocity[0], velocity[1], velocity[2]);
        ImGui::Text("Horizontal speed: %.2f m/s", horizontal_velocity);
        ImGui::Text("On ground: %s", character->on_ground() ? "yes" : "no");
    }
    ImGui::Separator();
    ImGui::Checkbox("Log movement to console", &state.show_movement_console);
    if (ImGui::Button("Clear console")) state.console_lines.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu lines", state.console_lines.size());
    if (!state.console_lines.empty()) {
        const float console_height = std::min(180.0f, ImGui::GetContentRegionAvail().y - 8.0f);
        if (ImGui::BeginChild("MovementConsole", {0.0f, console_height}, true)) {
            for (const auto& line : state.console_lines) ImGui::TextUnformatted(line.c_str());
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    } else if (state.show_movement_console) {
        ImGui::TextDisabled("Hold WASD on the Game tab during a test to stream movement lines.");
    }
    if (!state.recent_interaction_events.empty()) {
        ImGui::Separator();
        ImGui::Text("Recent interactions");
        for (const auto& event : state.recent_interaction_events) {
            ImGui::BulletText("%s %s (%s)", event.type == InteractionEventType::Enter ? "enter" : "exit",
                event.interaction_id.c_str(), event.placement_entity_id.c_str());
        }
    }
    if (!state.recent_combat_events.empty()) {
        ImGui::Separator();
        ImGui::Text("Recent combat hits");
        for (const auto& event : state.recent_combat_events) {
            ImGui::BulletText("%s -> %s (%s)", event.attacker_id.c_str(), event.hurt_combat_id.c_str(),
                event.hurt_placement_entity_id.c_str());
        }
    }
    ImGui::Text("Errors this session: %llu", static_cast<unsigned long long>(Logger::instance().error_count()));
    for (const auto& error : Logger::instance().recent_errors()) {
        const ImVec4 color =
            error.severity == Severity::Fatal ? ImVec4(1.0f, 0.15f, 0.15f, 1.0f) : ImVec4(1.0f, 0.4f, 0.2f, 1.0f);
        ImGui::TextColored(color, "[%s][%s] %s: %s", to_string(error.severity), to_string(error.priority),
                           error.code.c_str(), error.message.c_str());
    }
    ImGui::End();
    draw_new_material_asset_popup(state);
}
}

Result<RenderStats> run_render_app(const RenderOptions& options) {
    SDL_SetMainReady();
    if (options.attach_console) attach_process_console_if_needed();
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        return Result<RenderStats>::failure(graphics_error("PLATFORM-SDL-INIT", std::string("SDL initialization failed: ") + SDL_GetError()));
    }
    const SDL_WindowFlags flags = options.hidden ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
    SDL_Window* window = SDL_CreateWindow("AI RPG Engine - D3D12", static_cast<int>(options.width), static_cast<int>(options.height), flags);
    if (!window) {
        const auto error = graphics_error("PLATFORM-WINDOW", std::string("Window creation failed: ") + SDL_GetError());
        SDL_Quit();
        return Result<RenderStats>::failure(error);
    }

    MaterialAsset terrain_material;
    TerrainEditStore runtime_terrain_edits;
    WaterStore runtime_water;
    std::array<float, 4> water_color{0.08f, 0.22f, 0.35f, 0.88f};
    float water_roughness = 0.05f;
    if(options.debug_world&&!options.project_root.empty()){
        const auto path=options.project_root/"assets/materials/terrain.material.json";
        auto loaded=MaterialAsset::load(path);
        if(!loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(loaded.error());}
        terrain_material=loaded.value();
        const auto edits_path=default_terrain_edits_path(options.project_root);
        if(std::filesystem::exists(edits_path)){const auto edits_loaded=TerrainEditStore::load(edits_path);if(!edits_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(edits_loaded.error());}runtime_terrain_edits=std::move(edits_loaded.value());}
        const auto water_path=default_water_surfaces_path(options.project_root);
        if(std::filesystem::exists(water_path)){const auto water_loaded=WaterStore::load(water_path);if(!water_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(water_loaded.error());}runtime_water=std::move(water_loaded.value());}
        if(const auto map_loaded=WorldForgeMapAsset::load(default_world_forge_map_path(options.project_root));map_loaded){std::vector<WaterSeaRegion> sea_regions;for(const auto& region:map_loaded.value().hydrology_regions){if(region.kind!=WorldForgeHydrologyKind::Sea)continue;sea_regions.push_back(WaterSeaRegion{region.id,region.min_x,region.max_x,region.min_z,region.max_z});}runtime_water.set_sea_regions(std::move(sea_regions));}
        const auto water_material_path=options.project_root/"assets/materials/water.material.json";
        if(const auto water_material=MaterialAsset::load(water_material_path);water_material){water_color=water_material.value().base_color;water_roughness=water_material.value().roughness;}
        if(!options.editor){set_active_terrain_edits(&runtime_terrain_edits);set_active_water_store(&runtime_water);}
    }
    std::optional<EditorState> editor;
    if(options.editor){auto scene=Scene::load(options.world_path);if(!scene){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(scene.error());}EditorState value;value.scene=std::move(scene.value());const auto ids=value.scene.entity_ids();if(!ids.empty())value.selected=ids.front();value.world_path=options.world_path;auto scanned=value.assets.scan(options.project_root);if(!scanned){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(scanned.error());}const auto prefabs=load_prefab_catalog(value,options.project_root);if(!prefabs){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(prefabs.error());}const auto play=load_editor_play_session(value);if(!play){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(play.error());}sync_player_placement_tags(value);(void)value.scene.repair_prefab_paths(value.prefab_catalog);if(value.scene.seed_missing_authored_components(value.prefab_catalog)>0)value.scene_dirty=true;value.character_asset.visual_prefab=resolve_prefab_catalog_path(value.prefab_catalog,value.character_asset.visual_prefab);const auto edits_path=default_terrain_edits_path(options.project_root);if(std::filesystem::exists(edits_path)){const auto edits_loaded=TerrainEditStore::load(edits_path);if(!edits_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(edits_loaded.error());}value.terrain_edits=std::move(edits_loaded.value());}const auto paint_path=default_terrain_paint_path(options.project_root);if(std::filesystem::exists(paint_path)){const auto paint_loaded=TerrainPaintStore::load(paint_path);if(!paint_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(paint_loaded.error());}value.terrain_paint=std::move(paint_loaded.value());}const auto foliage_layers_path=default_foliage_layers_path(options.project_root);if(std::filesystem::exists(foliage_layers_path)){const auto layers_loaded=FoliageLayerPalette::load(foliage_layers_path);if(!layers_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(layers_loaded.error());}value.foliage_layers=std::move(layers_loaded.value());}else{value.foliage_layers.schema_version=1;value.foliage_layers.layers={{"grass","Grass","grass_blade",{0.14f,0.22f,0.10f},0.55f,1.0f,0.15f,0.55f,0.35f,1.2f,0.55f,"grass_walk"},{"flower","Flower","flower_clump",{0.62f,0.28f,0.48f},0.45f,0.85f,0.03f,0.45f,0.1f,0.9f,0.45f,"",FoliageScatterMode::GroundCover,64},{"bush","Bush","bush",{0.14f,0.22f,0.10f},0.85f,1.15f,1.0f,0.5f,0.08f,1.4f,0.95f,"",FoliageScatterMode::Discrete,72}};}const auto foliage_density_path=default_foliage_density_path(options.project_root);if(std::filesystem::exists(foliage_density_path)){const auto foliage_loaded=FoliageDensityStore::load(foliage_density_path);if(!foliage_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(foliage_loaded.error());}value.foliage_density=std::move(foliage_loaded.value());}const auto water_path=default_water_surfaces_path(options.project_root);if(std::filesystem::exists(water_path)){const auto water_loaded=WaterStore::load(water_path);if(!water_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(water_loaded.error());}value.water_store=std::move(water_loaded.value());}if(const auto map_loaded=WorldForgeMapAsset::load(default_world_forge_map_path(options.project_root));map_loaded){std::vector<WaterSeaRegion> sea_regions;for(const auto& region:map_loaded.value().hydrology_regions){if(region.kind!=WorldForgeHydrologyKind::Sea)continue;sea_regions.push_back(WaterSeaRegion{region.id,region.min_x,region.max_x,region.min_z,region.max_z});}value.water_store.set_sea_regions(std::move(sea_regions));}value.terrain_paint_brush_material=value.terrain_material_path;warm_material_cache(value);if(!options.initial_viewport.empty()){const auto&v=options.initial_viewport;if(v=="sculpt")value.active_viewport_tab=EditorState::ViewportTab::Sculpt;else if(v=="game")value.active_viewport_tab=EditorState::ViewportTab::Game;else if(v=="ui")value.active_viewport_tab=EditorState::ViewportTab::UI;else if(v=="world-forge"||v=="world_forge"||v=="worldforge")value.active_viewport_tab=EditorState::ViewportTab::WorldForge;else value.active_viewport_tab=EditorState::ViewportTab::Scene;value.force_select_viewport_tab=true;value.lock_viewport_tab=true;}editor=std::move(value);set_active_terrain_edits(&editor->terrain_edits);set_active_water_store(&editor->water_store);SDL_SetWindowTitle(window,"AI RPG Engine Editor");}
    std::vector<std::pair<std::string, ImportedMesh>> imported_meshes;
    std::optional<PrefabAsset> runtime_player_prefab;
    if (editor) {
        for (const auto& entry : editor->assets.records()) {
            const auto relative = normalize_asset_path(entry.second.path);
            const auto file_extension = std::filesystem::path(relative).extension().string();
            if (file_extension == ".gltf" || file_extension == ".glb") {
                auto imported = import_project_mesh(options.project_root / relative);
                if (!imported) {
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return Result<RenderStats>::failure(imported.error());
                }
                editor->mesh_bounds[relative] = imported.value().aabb;
                imported_meshes.emplace_back(relative, std::move(imported.value()));
            }
        }
        ensure_prefab_primitive_meshes(*editor, imported_meshes);
        for (const auto& foliage_mesh : build_foliage_layer_meshes(editor->foliage_layers)) {
            const auto normalized = normalize_asset_path(foliage_mesh.first);
            bool exists = false;
            for (const auto& mesh : imported_meshes) {
                if (normalize_asset_path(mesh.first) == normalized) {
                    exists = true;
                    break;
                }
            }
            if (!exists) imported_meshes.emplace_back(foliage_mesh);
        }
        if (const auto loaded = MaterialAsset::load(options.project_root / editor->terrain_material_path))
            terrain_material = loaded.value();
    } else if (options.debug_world) {
        PrefabAsset player_prefab;
        const auto player_assets = ensure_runtime_player_assets(options.project_root, player_prefab, imported_meshes);
        if (player_assets) runtime_player_prefab = std::move(player_prefab);
        else Logger::instance().write(Severity::Warning, "rendering", "Runtime player prefab unavailable; using debug proxy");
    }
    Renderer renderer;
    auto initialized = renderer.initialize(window, options.enable_debug_layer, options.debug_world, options.editor, options.hidden, terrain_material, imported_meshes);
    if (!initialized) {
        SDL_DestroyWindow(window); SDL_Quit();
        return Result<RenderStats>::failure(initialized.error());
    }
    if (editor) {
        editor->lua_runtime = std::make_unique<LuaRuntime>();
        editor->quest_runtime = std::make_unique<QuestRuntime>();
        editor->quest_asset = std::make_unique<WorldForgeQuestsAsset>();
        if (const auto quests_loaded = WorldForgeQuestsAsset::load(default_world_forge_quests_path(options.project_root));
            quests_loaded) {
            *editor->quest_asset = std::move(quests_loaded.value());
            if (const auto bound = editor->quest_runtime->bind(editor->quest_asset.get()); !bound) {
                Logger::instance().write(Severity::Warning, "quest",
                    "QuestRuntime bind failed: " + bound.error().message);
            }
        } else {
            Logger::instance().write(Severity::Warning, "quest",
                "Quests asset not loaded: " + quests_loaded.error().message);
        }
        editor->standing_runtime = std::make_unique<StandingRuntime>();
        editor->standing_factions_asset = std::make_unique<WorldForgeFactionsAsset>();
        editor->standing_relationships_asset = std::make_unique<WorldForgeRelationshipsAsset>();
        if (const auto factions_loaded =
                WorldForgeFactionsAsset::load(default_world_forge_factions_path(options.project_root));
            factions_loaded) {
            *editor->standing_factions_asset = std::move(factions_loaded.value());
        } else {
            Logger::instance().write(Severity::Warning, "standing",
                "Factions asset not loaded for StandingRuntime: " + factions_loaded.error().message);
        }
        if (const auto relationships_loaded =
                WorldForgeRelationshipsAsset::load(default_world_forge_relationships_path(options.project_root));
            relationships_loaded) {
            *editor->standing_relationships_asset = std::move(relationships_loaded.value());
        } else {
            Logger::instance().write(Severity::Warning, "standing",
                "Relationships asset not loaded for StandingRuntime: " + relationships_loaded.error().message);
        }
        if (const auto bound = editor->standing_runtime->bind(editor->standing_factions_asset.get(),
                editor->standing_relationships_asset.get());
            !bound) {
            Logger::instance().write(Severity::Warning, "standing",
                "StandingRuntime bind failed: " + bound.error().message);
        }
        editor->ui_canvas_stack = std::make_unique<UiCanvasStack>();
        if (const auto hud_loaded = editor->ui_canvas_stack->set_hud(default_player_hud_path(options.project_root));
            hud_loaded) {
            editor->ui_canvas_stack->hud().reset_player_health(100.0, 100.0);
        } else {
            Logger::instance().write(Severity::Warning, "hud",
                "Player HUD not loaded: " + hud_loaded.error().message);
        }
        const auto pause_path = options.project_root / "assets" / "ui" / "pause.uicanvas.json";
        if (const auto pause_loaded = editor->ui_canvas_stack->register_canvas("pause", pause_path); !pause_loaded) {
            Logger::instance().write(Severity::Warning, "ui_canvas",
                "Pause canvas not registered: " + pause_loaded.error().message);
        }
        const auto main_menu_path = options.project_root / "assets" / "ui" / "main_menu.uicanvas.json";
        if (const auto menu_loaded = editor->ui_canvas_stack->register_canvas("main_menu", main_menu_path);
            !menu_loaded) {
            Logger::instance().write(Severity::Warning, "ui_canvas",
                "Main menu canvas not registered: " + menu_loaded.error().message);
        }
        const auto settings_path = options.project_root / "assets" / "ui" / "settings.uicanvas.json";
        if (const auto settings_loaded = editor->ui_canvas_stack->register_canvas("settings", settings_path);
            !settings_loaded) {
            Logger::instance().write(Severity::Warning, "ui_canvas",
                "Settings canvas not registered: " + settings_loaded.error().message);
        }
        const auto inventory_path = options.project_root / "assets" / "ui" / "inventory.uicanvas.json";
        if (const auto inventory_loaded = editor->ui_canvas_stack->register_canvas("inventory", inventory_path);
            !inventory_loaded) {
            Logger::instance().write(Severity::Warning, "ui_canvas",
                "Inventory canvas not registered: " + inventory_loaded.error().message);
        }
        const auto dialogue_path = options.project_root / "assets" / "ui" / "dialogue.uicanvas.json";
        if (const auto dialogue_loaded = editor->ui_canvas_stack->register_canvas("dialogue", dialogue_path);
            !dialogue_loaded) {
            Logger::instance().write(Severity::Warning, "ui_canvas",
                "Dialogue canvas not registered: " + dialogue_loaded.error().message);
        }
        editor->lua_runtime->set_hud_runtime(&editor->ui_canvas_stack->hud());
        editor->lua_runtime->set_ui_canvas_stack(editor->ui_canvas_stack.get());
        editor->lua_runtime->set_quest_runtime(editor->quest_runtime.get());
        editor->lua_runtime->set_standing_runtime(editor->standing_runtime.get());
        editor->ui_canvas_stack->hud().set_text("quest.objectiveText", "");
        if (const auto bindings = editor->lua_runtime->load_bindings(options.project_root,
                default_script_bindings_path(options.project_root));
            !bindings) {
            editor->status = "Lua bindings failed: " + bindings.error().message;
            Logger::instance().write(bindings.error());
        }
    }

    const auto started = std::chrono::steady_clock::now();
    std::unique_ptr<CollisionWorld> debug_world;
    std::optional<CharacterController> debug_character;
    std::optional<RigidbodyLocomotion> player_locomotion;
    std::optional<StreamedTerrainField> streamed_terrain;
    std::optional<StreamedFoliageField> streamed_foliage;
    std::optional<StreamedWaterField> streamed_water;
    std::uint64_t water_terrain_revision_seen = 0;
    std::optional<PlacementCollisionTracker> placement_collision;
    InteractionVolumeRegistry debug_interaction_registry;
    InteractionOverlapTracker debug_interaction_tracker;
    CombatVolumeRegistry debug_combat_registry;
    DebugCamera camera;
    if (editor) {
        // Open Scene camera on the authored player spawn when exactly one exists.
        std::optional<EntityId> spawn_id;
        for (const auto& id : editor->scene.entity_ids()) {
            const auto placement = editor->scene.placement(id);
            if (!placement || !placement->character_asset) continue;
            if (spawn_id) {
                spawn_id.reset();
                break;
            }
            spawn_id = id;
        }
        if (spawn_id) {
            if (const auto transform = editor->scene.transform(*spawn_id)) {
                constexpr float k_distance = 28.0f;
                constexpr float k_height = 16.0f;
                const float tx = transform->position[0];
                const float ty = transform->position[1];
                const float tz = transform->position[2];
                const float cam_x = tx;
                const float cam_y = ty + k_height;
                const float cam_z = tz - k_distance;
                const float dx = tx - cam_x;
                const float dy = ty - cam_y;
                const float dz = tz - cam_z;
                const float yaw = std::atan2(dx, dz);
                const float horiz = std::sqrt(dx * dx + dz * dz);
                const float pitch = std::atan2(dy, (std::max)(horiz, 0.001f));
                camera.set_pose({cam_x, cam_y, cam_z}, yaw, pitch);
            }
        }
    }
    std::optional<OrbitCamera> orbit_camera;
    float player_facing_yaw = 0.0f;
    float mouse_x=0,mouse_y=0;
    bool camera_look_active=false;
    float pending_orbit_zoom = 0.0f;
    std::optional<EditorTestSessionRestore> editor_test_restore;
    std::unique_ptr<EditorBridgeServer> editor_bridge;
    if(options.debug_world){
        debug_world=std::make_unique<CollisionWorld>();
        streamed_terrain.emplace();
        streamed_foliage.emplace();
        streamed_water.emplace();
        if (editor) {
            streamed_foliage->set_palette(&editor->foliage_layers);
            streamed_foliage->set_density(&editor->foliage_density);
        }
        if(options.editor)placement_collision.emplace();
        if(!options.editor){
            const float spawn_y=sample_terrain_height(0.0f,0.0f)+2.0f;
            auto created=CharacterController::create(*debug_world,{0.0,spawn_y,0.0});
            if(created)debug_character=std::move(created.value());
            orbit_camera.emplace();
            const float camp_x=4.0f;
            const float camp_y=sample_terrain_height(camp_x,0.0f)+0.55f;
            auto interaction_body=debug_world->add_sphere({camp_x,camp_y,0.0},0.85f,CollisionLayer::Trigger,false,CellCoord{0,0});
            if(interaction_body)debug_interaction_registry.bind(interaction_body.value(),{"debug-campfire",0,"use_campfire"});
            const float enemy_x=-4.0f;
            const float enemy_y=sample_terrain_height(enemy_x,0.0f)+1.0f;
            auto hurt_body=debug_world->add_sphere({enemy_x,enemy_y,0.0},0.9f,CollisionLayer::Trigger,false,CellCoord{0,0});
            if(hurt_body)debug_combat_registry.bind(hurt_body.value(),{"debug-enemy",0,CombatVolumeRole::Hurt,"body"});
            SDL_SetWindowTitle(window,"AI RPG Engine - Low-Poly Dark Fantasy Terrain");
        }
    }
    std::uint64_t frames = 0;
    double gpu_ms_total = 0.0;
    bool running = true;
    bool space_key_was_down = false;
    auto previous_frame_time = std::chrono::steady_clock::now();
    float frame_delta_seconds = 1.0f / 60.0f;
    while (running) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if(options.editor)ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
            if(options.debug_world&&event.type==SDL_EVENT_MOUSE_BUTTON_DOWN&&event.button.button==SDL_BUTTON_RIGHT&&(!editor||editor->viewport_hovered||(editor->test_session_active()&&editor->game_viewport_active()))){camera_look_active=true;SDL_SetWindowRelativeMouseMode(window,true);}
            if(options.debug_world&&event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_RIGHT){camera_look_active=false;SDL_SetWindowRelativeMouseMode(window,false);}
            if(options.debug_world&&camera_look_active&&event.type==SDL_EVENT_MOUSE_MOTION){mouse_x+=event.motion.xrel;mouse_y+=event.motion.yrel;}
            // Accumulate wheel notches; meters-per-notch comes from the camera asset.
            if (options.debug_world && event.type == SDL_EVENT_MOUSE_WHEEL) {
                float dy = event.wheel.y;
                if (event.wheel.integer_y != 0) dy = static_cast<float>(event.wheel.integer_y);
                if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) dy = -dy;
                pending_orbit_zoom += dy;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                int width = 0, height = 0;
                SDL_GetWindowSizeInPixels(window, &width, &height);
                auto resized = renderer.resize(static_cast<UINT>(width), static_cast<UINT>(height));
                if (!resized) { SDL_DestroyWindow(window); SDL_Quit(); return Result<RenderStats>::failure(resized.error()); }
            }
        }
        if (!running) break;
        const auto frame_time = std::chrono::steady_clock::now();
        frame_delta_seconds =
            std::chrono::duration<float>(frame_time - previous_frame_time).count();
        previous_frame_time = frame_time;
        frame_delta_seconds = std::min(frame_delta_seconds, 0.25f);
        if (editor) {
            if (const auto requested = consume_live_automation_request(editor->project_root)) {
                if (*requested != editor->live_automation_enabled) {
                    editor->live_automation_enabled = *requested;
                    editor->status = *requested ? "Live automation enable requested by MCP"
                                               : "Live automation disable requested by MCP";
                    AutomationTrace::log(AutomationTraceChannel::EditorBridge, "live_request_applied",
                        {{"enabled", *requested ? "true" : "false"}});
                }
            }
            if (editor_bridge && editor->live_automation_enabled) {
                editor_bridge->poll_pending([&](const EditorBridgeRequest& request) {
                auto context = make_editor_session_context(*editor, true);
                if (streamed_terrain) {
                    context.reload_terrain = [&](bool height_changed) {
                        reload_loaded_terrain_cells(*editor, &*streamed_terrain, debug_world.get(), &terrain_material,
                            height_changed);
                    };
                }
                if (streamed_foliage && streamed_terrain) {
                    context.reload_foliage = [&]() {
                        streamed_foliage->set_palette(&editor->foliage_layers);
                        streamed_foliage->set_density(&editor->foliage_density);
                        const auto loaded = streamed_terrain->loaded_cell_coordinates();
                        if (!loaded.empty()) (void)streamed_foliage->rebuild_cells(loaded, camera.position());
                    };
                }
                if (streamed_water) {
                    context.reload_water = [&]() { reload_loaded_water_cells(*editor, &*streamed_water); };
                }

                auto make_bridge_ok = [&](std::string summary, std::map<std::string, std::string> metadata = {}) {
                    EditorBridgeResponse response;
                    response.request_id = request.request_id;
                    response.exit_code = ExitCode::Success;
                    response.summary = std::move(summary);
                    response.metadata = std::move(metadata);
                    return response;
                };
                auto make_bridge_err = [&](ExitCode code, std::string summary, EngineError err) {
                    EditorBridgeResponse response;
                    response.request_id = request.request_id;
                    response.exit_code = code;
                    response.summary = std::move(summary);
                    response.diagnostics.push_back(std::move(err));
                    return response;
                };

                if (request.operation == "world_forge_map_view") {
                    nlohmann::json params = nlohmann::json::object();
                    try {
                        if (!request.params_json.empty()) params = nlohmann::json::parse(request.params_json);
                    } catch (...) {
                        return make_bridge_err(ExitCode::InvalidArguments, "Invalid JSON params",
                            EngineError{"WF-MAP-JSON", Severity::Error, ErrorCategory::Validation, "automation",
                                "params_json parse failed", std::nullopt, {}, "Send valid JSON arguments."});
                    }
                    editor->active_viewport_tab = EditorState::ViewportTab::WorldForge;
                    editor->force_select_viewport_tab = true;
                    if (params.value("lockTab", true)) editor->lock_viewport_tab = true;
                    auto& wf = editor->world_forge_editor;
                    if (!wf.loaded) {
                        const auto reloaded = wf.reload(editor->project_root);
                        if (!reloaded) {
                            return make_bridge_err(ExitCode::ValidationFailed, reloaded.error().message, reloaded.error());
                        }
                    }
                    wf.pane = WorldForgeEditorPane::Map;
                    wf.force_select_pane = true;
                    if (params.value("lockTab", true)) wf.lock_pane_tab = true;
                    wf.map_canvas_mode = true;
                    wf.list_kind = WorldForgeEditorSession::ListKind::MapCanvas;
                    if (params.contains("topDown") && params["topDown"].is_boolean() && params["topDown"].get<bool>()) {
                        wf.map_cartography_mode = false;
                    }
                    if (params.contains("cartography") && params["cartography"].is_boolean()) {
                        wf.map_cartography_mode = params["cartography"].get<bool>();
                    }
                    if (params.contains("worldMap") && params["worldMap"].is_boolean()) {
                        wf.map_show_official_backdrop = params["worldMap"].get<bool>();
                    }
                    if (params.contains("frame") && params["frame"].is_boolean()) {
                        wf.map_show_frame = params["frame"].get<bool>();
                    }
                    if (params.value("fit", false)) wf.map_camera_fit_requested = true;
                    if (params.contains("zoom") && params["zoom"].is_number()) {
                        wf.map_camera.zoom = params["zoom"].get<float>();
                    }
                    if (params.contains("panX") && params["panX"].is_number()) {
                        wf.map_camera.pan[0] = params["panX"].get<float>();
                    }
                    if (params.contains("panZ") && params["panZ"].is_number()) {
                        wf.map_camera.pan[1] = params["panZ"].get<float>();
                    }
                    if (params.contains("layerId") && params["layerId"].is_string()) {
                        const auto layer_id = params["layerId"].get<std::string>();
                        bool known = layer_id.empty();
                        for (const auto& layer : wf.map_layers) {
                            if (layer.id == layer_id) {
                                known = true;
                                break;
                            }
                        }
                        if (!known && wf.map_layers_ready) {
                            return make_bridge_err(ExitCode::InvalidArguments, "Unknown layerId",
                                EngineError{"WF-MAP-LAYER", Severity::Error, ErrorCategory::Validation, "automation",
                                    "layerId not in loaded world-map-layers manifest", std::nullopt, {},
                                    "Use continent|theater_nw|theater_ne|theater_sw|theater_se|theater_interior_sea|local_calrenoth."});
                        }
                        if (params.value("forceTransition", true) && !layer_id.empty() &&
                            layer_id != wf.map_layer_active_id) {
                            wf.map_layer_pending_id = layer_id;
                            wf.map_layer_transition_t = 1e-4f;
                        } else {
                            wf.map_layer_active_id = layer_id;
                            wf.map_layer_pending_id.clear();
                            wf.map_layer_transition_t = 0.0f;
                        }
                    }
                    editor->status = "MCP: World Forge Map Canvas view applied";
                    return make_bridge_ok("World Forge map view updated",
                        {{"viewportTab", "world_forge"}, {"pane", "map"}, {"canvas", "true"},
                            {"cartography", wf.map_cartography_mode ? "true" : "false"},
                            {"frame", wf.map_show_frame ? "true" : "false"},
                            {"worldMap", wf.map_show_official_backdrop ? "true" : "false"},
                            {"layerId", wf.map_layer_active_id}, {"pendingLayerId", wf.map_layer_pending_id},
                            {"zoom", std::to_string(wf.map_camera.zoom)}});
                }

                if (request.operation == "editor_ui_query") {
                    nlohmann::json params = nlohmann::json::object();
                    try {
                        if (!request.params_json.empty()) params = nlohmann::json::parse(request.params_json);
                    } catch (...) {
                        return make_bridge_err(ExitCode::InvalidArguments, "Invalid JSON params",
                            EngineError{"UIQ-JSON", Severity::Error, ErrorCategory::Validation, "automation",
                                "params_json parse failed", std::nullopt, {}, "Send valid JSON arguments."});
                    }
                    const auto id_prefix = params.value("idPrefix", std::string{});
                    const auto contains = params.value("contains", std::string{});
                    const auto exact_id = params.value("id", std::string{});
                    const int ww = editor->ui_hotspots.window_w;
                    const int wh = editor->ui_hotspots.window_h;
                    auto hotspot_json = [&](const EditorUiHotspot& spot) {
                        nlohmann::json j;
                        j["id"] = spot.id;
                        j["label"] = spot.label;
                        j["minX"] = spot.min_x;
                        j["minY"] = spot.min_y;
                        j["maxX"] = spot.max_x;
                        j["maxY"] = spot.max_y;
                        j["cx"] = spot.cx;
                        j["cy"] = spot.cy;
                        j["nx"] = spot.cx / static_cast<float>((std::max)(1, ww));
                        j["ny"] = spot.cy / static_cast<float>((std::max)(1, wh));
                        j["width"] = spot.max_x - spot.min_x;
                        j["height"] = spot.max_y - spot.min_y;
                        return j;
                    };
                    nlohmann::json matches = nlohmann::json::array();
                    if (!exact_id.empty()) {
                        if (const auto* spot = editor->ui_hotspots.find_exact(exact_id)) {
                            matches.push_back(hotspot_json(*spot));
                        }
                    } else {
                        for (const auto* spot : editor->ui_hotspots.find_filter(id_prefix, contains)) {
                            matches.push_back(hotspot_json(*spot));
                        }
                    }
                    std::map<std::string, std::string> meta{
                        {"count", std::to_string(matches.size())},
                        {"windowW", std::to_string(ww)},
                        {"windowH", std::to_string(wh)},
                        {"hotspotsJson", matches.dump()},
                    };
                    if (!exact_id.empty()) meta["id"] = exact_id;
                    if (!id_prefix.empty()) meta["idPrefix"] = id_prefix;
                    if (!contains.empty()) meta["contains"] = contains;
                    editor->status = "MCP ui_query: " + std::to_string(matches.size()) + " hotspot(s)";
                    return make_bridge_ok("UI hotspots queried", std::move(meta));
                }

                if (request.operation == "editor_screenshot") {
                    nlohmann::json params = nlohmann::json::object();
                    try {
                        if (!request.params_json.empty()) params = nlohmann::json::parse(request.params_json);
                    } catch (...) {
                        return make_bridge_err(ExitCode::InvalidArguments, "Invalid JSON params",
                            EngineError{"SHOT-JSON", Severity::Error, ErrorCategory::Validation, "automation",
                                "params_json parse failed", std::nullopt, {}, "Send valid JSON arguments."});
                    }
                    const auto filename = params.value("filename", std::string{"editor-screenshot"});
                    const bool client_only = params.value("clientAreaOnly", true);
                    // Prefer GPU readback of the presented swap-chain (exact rendered colors + ImGui).
                    // Fall back to composed desktop BitBlt / PrintWindow if no frame has presented yet.
                    auto captured = renderer.capture_presented_backbuffer_png(editor->project_root, filename);
                    std::string capture_path_kind = "gpu_backbuffer";
                    if (!captured) {
                        const auto properties = SDL_GetWindowProperties(window);
                        void* hwnd =
                            SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
                        captured = capture_window_png(hwnd, editor->project_root, filename, client_only);
                        capture_path_kind = "gdi_fallback";
                    }
                    if (!captured) {
                        return make_bridge_err(ExitCode::InternalError, captured.error().message, captured.error());
                    }
                    editor->status = "MCP screenshot: " + captured.value().filename().string();
                    return make_bridge_ok("Screenshot saved",
                        {{"path", captured.value().generic_string()},
                            {"relativePath",
                                std::filesystem::relative(captured.value(), editor->project_root).generic_string()},
                            {"capturePath", capture_path_kind}});
                }

                if (request.operation == "editor_input") {
                    nlohmann::json params = nlohmann::json::object();
                    try {
                        if (!request.params_json.empty()) params = nlohmann::json::parse(request.params_json);
                    } catch (...) {
                        return make_bridge_err(ExitCode::InvalidArguments, "Invalid JSON params",
                            EngineError{"INPUT-JSON", Severity::Error, ErrorCategory::Validation, "automation",
                                "params_json parse failed", std::nullopt, {}, "Send valid JSON arguments."});
                    }
                    const auto action = params.value("action", std::string{"click"});
                    auto button_index = [&](const nlohmann::json& p) {
                        const auto name = p.value("button", std::string{"left"});
                        if (name == "right") return 1;
                        if (name == "middle") return 2;
                        return 0;
                    };
                    auto enqueue_one = [&](const nlohmann::json& step) -> std::optional<std::string> {
                        const auto step_action = step.value("action", action);
                        if (step_action == "wait") {
                            EditorState::InputEvent wait;
                            wait.kind = EditorState::InputEvent::Kind::Wait;
                            wait.wait_frames = (std::max)(1, step.value("frames", 1));
                            editor->mcp_input_queue.push_back(wait);
                            return std::nullopt;
                        }
                        if (step_action == "move" || step_action == "click" || step_action == "drag") {
                            float x = 0.0f, y = 0.0f;
                            std::string err;
                            if (!resolve_mcp_client_xy(window, step, x, y, &err, &editor->ui_hotspots)) return err;
                            if (step_action == "move") {
                                EditorState::InputEvent move;
                                move.kind = EditorState::InputEvent::Kind::Move;
                                move.x = x;
                                move.y = y;
                                editor->mcp_input_queue.push_back(move);
                                return std::nullopt;
                            }
                            if (step_action == "click") {
                                enqueue_mcp_click(*editor, x, y, button_index(step), step.value("holdFrames", 1));
                                return std::nullopt;
                            }
                            // drag: move to start, down, move to end, up
                            float x2 = x, y2 = y;
                            nlohmann::json end = step.contains("to") ? step["to"] : step;
                            if (step.contains("toX") || step.contains("toY") || step.contains("toNx") ||
                                step.contains("toNy") || step.contains("toTargetId")) {
                                end = nlohmann::json::object();
                                if (step.contains("toX")) end["x"] = step["toX"];
                                if (step.contains("toY")) end["y"] = step["toY"];
                                if (step.contains("toNx")) end["nx"] = step["toNx"];
                                if (step.contains("toNy")) end["ny"] = step["toNy"];
                                if (step.contains("toTargetId")) end["targetId"] = step["toTargetId"];
                            }
                            if (!resolve_mcp_client_xy(window, end, x2, y2, &err, &editor->ui_hotspots)) return err;
                            EditorState::InputEvent move1;
                            move1.kind = EditorState::InputEvent::Kind::Move;
                            move1.x = x;
                            move1.y = y;
                            editor->mcp_input_queue.push_back(move1);
                            EditorState::InputEvent down;
                            down.kind = EditorState::InputEvent::Kind::Button;
                            down.button = button_index(step);
                            down.down = true;
                            editor->mcp_input_queue.push_back(down);
                            EditorState::InputEvent wait;
                            wait.kind = EditorState::InputEvent::Kind::Wait;
                            wait.wait_frames = (std::max)(1, step.value("holdFrames", 1));
                            editor->mcp_input_queue.push_back(wait);
                            EditorState::InputEvent move2;
                            move2.kind = EditorState::InputEvent::Kind::Move;
                            move2.x = x2;
                            move2.y = y2;
                            editor->mcp_input_queue.push_back(move2);
                            EditorState::InputEvent wait2;
                            wait2.kind = EditorState::InputEvent::Kind::Wait;
                            wait2.wait_frames = (std::max)(1, step.value("dragFrames", 2));
                            editor->mcp_input_queue.push_back(wait2);
                            EditorState::InputEvent up;
                            up.kind = EditorState::InputEvent::Kind::Button;
                            up.button = button_index(step);
                            up.down = false;
                            editor->mcp_input_queue.push_back(up);
                            return std::nullopt;
                        }
                        if (step_action == "scroll") {
                            float x = 0.0f, y = 0.0f;
                            std::string err;
                            if (step.contains("targetId") || step.contains("nx") || step.contains("ny") ||
                                step.contains("x") || step.contains("y")) {
                                if (!resolve_mcp_client_xy(window, step, x, y, &err, &editor->ui_hotspots))
                                    return err;
                                EditorState::InputEvent move;
                                move.kind = EditorState::InputEvent::Kind::Move;
                                move.x = x;
                                move.y = y;
                                editor->mcp_input_queue.push_back(move);
                            }
                            EditorState::InputEvent wheel;
                            wheel.kind = EditorState::InputEvent::Kind::Wheel;
                            wheel.wheel = step.value("delta", step.value("wheel", 1.0f));
                            editor->mcp_input_queue.push_back(wheel);
                            return std::nullopt;
                        }
                        if (step_action == "key") {
                            EditorState::InputEvent down;
                            down.kind = EditorState::InputEvent::Kind::Key;
                            down.key = step.value("keyCode", 0);
                            if (down.key == 0) {
                                const auto name = step.value("key", std::string{});
                                if (name == "escape") down.key = 27;
                                else if (name == "enter") down.key = 13;
                                else if (name == "tab") down.key = 9;
                                else if (name == "space") down.key = 32;
                                else if (name == "left") down.key = 37;
                                else if (name == "up") down.key = 38;
                                else if (name == "right") down.key = 39;
                                else if (name == "down") down.key = 40;
                            }
                            if (down.key == 0) return std::string{"key/keyCode required"};
                            down.down = true;
                            editor->mcp_input_queue.push_back(down);
                            EditorState::InputEvent wait;
                            wait.kind = EditorState::InputEvent::Kind::Wait;
                            wait.wait_frames = (std::max)(1, step.value("holdFrames", 1));
                            editor->mcp_input_queue.push_back(wait);
                            EditorState::InputEvent up = down;
                            up.down = false;
                            editor->mcp_input_queue.push_back(up);
                            return std::nullopt;
                        }
                        if (step_action == "clear") {
                            editor->mcp_input_queue.clear();
                            editor->mcp_draw_cursor = false;
                            return std::nullopt;
                        }
                        if (step_action == "unlock_tab") {
                            editor->lock_viewport_tab = false;
                            editor->force_select_viewport_tab = false;
                            editor->world_forge_editor.lock_pane_tab = false;
                            editor->world_forge_editor.force_select_pane = false;
                            return std::nullopt;
                        }
                        return std::string{"Unknown action (move|click|drag|scroll|key|wait|clear|unlock_tab)"};
                    };

                    std::size_t queued = 0;
                    if (params.contains("steps") && params["steps"].is_array()) {
                        for (const auto& step : params["steps"]) {
                            if (const auto err = enqueue_one(step)) {
                                return make_bridge_err(ExitCode::InvalidArguments, *err,
                                    EngineError{"INPUT-STEP", Severity::Error, ErrorCategory::Validation, "automation",
                                        *err, std::nullopt, {}, "Fix the step and retry."});
                            }
                        }
                        queued = editor->mcp_input_queue.size();
                    } else {
                        if (const auto err = enqueue_one(params)) {
                            return make_bridge_err(ExitCode::InvalidArguments, *err,
                                EngineError{"INPUT-ACTION", Severity::Error, ErrorCategory::Validation, "automation",
                                    *err, std::nullopt, {}, "Fix action params and retry."});
                        }
                        queued = editor->mcp_input_queue.size();
                    }
                    editor->status = "MCP input queued (" + std::to_string(queued) + " events)";
                    return make_bridge_ok("Editor input queued",
                        {{"queuedEvents", std::to_string(queued)},
                            {"cursorX", std::to_string(editor->mcp_cursor_x)},
                            {"cursorY", std::to_string(editor->mcp_cursor_y)}});
                }

                auto response = execute_editor_operation(context, request.operation, request.params_json);
                if (request.operation == "editor_status" && response.exit_code == ExitCode::Success) {
                    const char* tab = "scene";
                    switch (editor->active_viewport_tab) {
                    case EditorState::ViewportTab::Sculpt: tab = "sculpt"; break;
                    case EditorState::ViewportTab::Game: tab = "game"; break;
                    case EditorState::ViewportTab::UI: tab = "ui"; break;
                    case EditorState::ViewportTab::WorldForge: tab = "world_forge"; break;
                    case EditorState::ViewportTab::DesignDocs: tab = "design_docs"; break;
                    default: break;
                    }
                    response.metadata["viewportTab"] = tab;
                    const auto& wf = editor->world_forge_editor;
                    response.metadata["worldForgePane"] = std::to_string(static_cast<int>(wf.pane));
                    response.metadata["mapCanvasMode"] = wf.map_canvas_mode ? "true" : "false";
                    response.metadata["mapCartography"] = wf.map_cartography_mode ? "true" : "false";
                    response.metadata["mapShowFrame"] = wf.map_show_frame ? "true" : "false";
                    response.metadata["mapShowWorldMap"] = wf.map_show_official_backdrop ? "true" : "false";
                    response.metadata["mapLayerId"] = wf.map_layer_draw_id.empty() ? wf.map_layer_active_id : wf.map_layer_draw_id;
                    response.metadata["mapLayerPendingId"] = wf.map_layer_pending_id;
                    response.metadata["mapLayerTransition"] = std::to_string(wf.map_layer_transition_t);
                    response.metadata["mapZoom"] = std::to_string(wf.map_camera.zoom);
                    response.metadata["mapLayersReady"] = wf.map_layers_ready ? "true" : "false";
                }
                if (response.exit_code == ExitCode::Success && request.operation == "lua_apply") {
                    try {
                        const auto params = nlohmann::json::parse(request.params_json);
                        const auto relative = params.value("path", std::string{});
                        if (!relative.empty() && editor->lua_runtime)
                            (void)editor->lua_runtime->reload_script(editor->project_root / relative);
                    } catch (...) {
                    }
                }
                if (response.exit_code == ExitCode::Success &&
                    (request.operation == "hud_apply" || request.operation == "ui_canvas_mutate")) {
                    try {
                        const auto params = nlohmann::json::parse(request.params_json);
                        const auto relative = params.value("path", std::string{});
                        if (!relative.empty() && editor->ui_canvas_stack) {
                            const auto absolute = editor->project_root / relative;
                            hot_reload_ui_canvas_file(*editor->ui_canvas_stack, absolute);
                            if (editor->lua_runtime) {
                                editor->lua_runtime->set_hud_runtime(&editor->ui_canvas_stack->hud());
                                editor->lua_runtime->set_ui_canvas_stack(editor->ui_canvas_stack.get());
                                editor->lua_runtime->set_quest_runtime(editor->quest_runtime.get());
                                editor->lua_runtime->set_standing_runtime(editor->standing_runtime.get());
                            }
                            (void)editor->ui_canvas_editor.load(absolute);
                        }
                    } catch (...) {
                    }
                }
                if (response.exit_code == ExitCode::Success && request.operation == "prefab_apply")
                    editor->prefab_meshes_dirty = true;
                return response;
            });
            }
            if (editor->live_automation_enabled) {
                if (!editor_bridge) {
                    editor_bridge = std::make_unique<EditorBridgeServer>(editor->project_root);
                    if (!editor_bridge->start()) {
                        Logger::instance().write(Severity::Warning, "automation",
                            "Editor live bridge failed to start");
                        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "ui_start_failed",
                            "EditorBridgeServer::start returned false");
                        editor->status = "Live automation failed to start";
                        editor->live_automation_enabled = false;
                        editor_bridge.reset();
                    } else {
                        editor->status = "Live automation enabled; MCP tools can connect";
                        AutomationTrace::log(AutomationTraceChannel::EditorBridge, "ui_started");
                    }
                }
            } else if (editor_bridge) {
                AutomationTrace::log(AutomationTraceChannel::EditorBridge, "ui_stopping");
                editor_bridge->stop();
                editor_bridge.reset();
                editor->status = "Live automation disabled";
            }
            reload_changed_lua_scripts(*editor);
        }
        if (editor && debug_world) {
            const auto command = editor->test_session_command;
            editor->test_session_command = EditorState::TestSessionCommand::None;
            if (command == EditorState::TestSessionCommand::Start &&
                editor->test_session == EditorState::TestSessionState::Inactive) {
                clear_editor_manipulation(*editor);
                editor->drop_preview.reset();
                editor->drop_preview_prefab.reset();
                editor_test_restore = EditorTestSessionRestore{camera.position(), camera.yaw(), camera.pitch()};
                const auto spawn_resolution = resolve_test_player_spawn(*editor, camera);
                CharacterAsset spawn_character = editor->character_asset;
                if (spawn_resolution.placement_entity)
                    spawn_character = resolve_character_for_spawn(*editor, *spawn_resolution.placement_entity);
                player_locomotion.reset();
                debug_character.reset();
                bool started_rigidbody = false;
                if (spawn_resolution.placement_entity && placement_collision) {
                    const auto placement = editor->scene.placement(*spawn_resolution.placement_entity);
                    if (placement) {
                        const auto resolved = resolve_prefab_catalog_path(editor->prefab_catalog, placement->prefab_asset);
                        const auto found = editor->prefab_catalog.find(resolved);
                        if (found != editor->prefab_catalog.end()) {
                            (void)editor->scene.ensure_authored_components_seeded(
                                *spawn_resolution.placement_entity, found->second);
                            (void)editor->scene.propagate_prefab_components(resolved, found->second);
                        }
                        const auto synced = placement_collision->sync(
                            *debug_world, editor->scene, editor->prefab_catalog, true);
                        if (synced) {
                            if (const auto body =
                                    placement_collision->motion_body_for(*spawn_resolution.placement_entity)) {
                                float radius = spawn_character.capsule_radius;
                                float half_height = spawn_character.capsule_half_height;
                                if (const auto authored =
                                        editor->scene.authored_components(*spawn_resolution.placement_entity)) {
                                    const PrefabAsset* prefab =
                                        found != editor->prefab_catalog.end() ? &found->second : nullptr;
                                    for (const auto& volume :
                                        effective_collision_volumes(&*authored, prefab)) {
                                        if (volume.trigger || volume.is_interaction() || volume.is_combat_sensor())
                                            continue;
                                        if (volume.shape == PrefabCollisionShape::Capsule) {
                                            radius = volume.radius;
                                            half_height = volume.capsule_half_height;
                                        }
                                        break;
                                    }
                                }
                                player_locomotion.emplace(*debug_world, *body, spawn_character.controller_config(),
                                    radius, half_height);
                                started_rigidbody = true;
                            }
                        }
                    }
                }
                if (!started_rigidbody) {
                    auto created = CharacterController::create(*debug_world, spawn_resolution.position,
                        spawn_character.controller_config());
                    if (!created) {
                        editor->status = created.error().message;
                        Logger::instance().write(created.error());
                        editor_test_restore.reset();
                    } else {
                        debug_character.emplace(std::move(created.value()));
                    }
                }
                if (started_rigidbody || debug_character) {
                    editor->character_asset = spawn_character;
                    editor->test_player_spawn_entity = spawn_resolution.placement_entity;
                    if (spawn_resolution.placement_entity) {
                        editor_test_restore->spawn_entity = spawn_resolution.placement_entity;
                        if (const auto transform = editor->scene.transform(*spawn_resolution.placement_entity))
                            editor_test_restore->spawn_transform = *transform;
                    }
                    orbit_camera.emplace(editor->camera_asset.orbit_config());
                    orbit_camera->set_sensitivity(editor->camera_asset.look_sensitivity);
                    // Keep yaw from edit camera; start with authored RPG look-down pitch.
                    orbit_camera->set_orientation(camera.yaw(), editor->camera_asset.default_pitch);
                    player_facing_yaw = orbit_camera->yaw();
                    editor->test_session = EditorState::TestSessionState::Running;
                    editor->active_viewport_tab = EditorState::ViewportTab::Game;
                    if (editor->ui_canvas_stack) {
                        editor->ui_canvas_stack->clear_modals();
                        (void)editor->ui_canvas_stack->set_hud(default_player_hud_path(editor->project_root));
                        editor->ui_canvas_stack->hud().reset_player_health(100.0, 100.0);
                        if (editor->lua_runtime) {
                            editor->lua_runtime->set_hud_runtime(&editor->ui_canvas_stack->hud());
                            editor->lua_runtime->set_ui_canvas_stack(editor->ui_canvas_stack.get());
                            editor->lua_runtime->set_quest_runtime(editor->quest_runtime.get());
                            editor->lua_runtime->set_standing_runtime(editor->standing_runtime.get());
                        }
                    }
                    editor->status = started_rigidbody
                        ? ("Test session started (Rigidbody loco, max speed " +
                              std::to_string(static_cast<int>(spawn_character.max_speed)) + " m/s)")
                        : ("Test session started (max speed " +
                              std::to_string(static_cast<int>(spawn_character.max_speed)) + " m/s)");
                    append_editor_console(*editor,
                        std::string("Test session started: ") + (started_rigidbody ? "rigidbody" : "character") +
                            " max_speed=" + std::to_string(spawn_character.max_speed) + " m/s",
                        editor->show_movement_console);
                    debug_interaction_tracker.reset("player");
                }
            } else if (command == EditorState::TestSessionCommand::Pause &&
                       editor->test_session == EditorState::TestSessionState::Running) {
                editor->test_session = EditorState::TestSessionState::Paused;
                editor->status = "Test session paused";
            } else if (command == EditorState::TestSessionCommand::Resume &&
                       editor->test_session == EditorState::TestSessionState::Paused) {
                editor->test_session = EditorState::TestSessionState::Running;
                editor->status = "Test session resumed";
            } else if (command == EditorState::TestSessionCommand::End && editor->test_session_active()) {
                if (editor->ui_canvas_stack) editor->ui_canvas_stack->clear_modals();
                if (editor_test_restore && editor_test_restore->spawn_entity && editor_test_restore->spawn_transform)
                    (void)editor->scene.set_transform(*editor_test_restore->spawn_entity, *editor_test_restore->spawn_transform);
                debug_character.reset();
                player_locomotion.reset();
                orbit_camera.reset();
                if (editor_test_restore) {
                    camera.set_pose(editor_test_restore->camera_position, editor_test_restore->camera_yaw,
                        editor_test_restore->camera_pitch);
                    editor_test_restore.reset();
                }
                camera_look_active = false;
                SDL_SetWindowRelativeMouseMode(window, false);
                editor->test_session = EditorState::TestSessionState::Inactive;
                editor->test_player_spawn_entity.reset();
                editor->status = "Test session ended";
                clear_editor_manipulation(*editor);
            }
        }
        if (options.editor) {
            ImGui_ImplDX12_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            if (editor) {
                drain_mcp_input_queue(*editor, window);
                draw_mcp_cursor_overlay(*editor);
            }
        }
        const auto capture = (!options.capture_path.empty() &&
                              (options.frame_limit == 0 ? frames == 0 : (frames + 1 == options.frame_limit)))
                                 ? options.capture_path
                                 : std::filesystem::path{};
        WorldPosition body_position{0, 1, 0};
        std::array<float, 16> view_projection_matrix = camera.view_projection();
        std::array<float, 3> camera_position = camera.position();
        int pixel_w = 1, pixel_h = 1;
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        if (orbit_camera) {
            if (editor && editor->test_session_active()) {
                // Keep orbit limits/framing in sync with inspector edits (zoom already applied live).
                orbit_camera->set_config(editor->camera_asset.orbit_config());
                orbit_camera->set_sensitivity(editor->camera_asset.look_sensitivity);
            }
            const float fov = editor && editor->test_session_active() ? editor->camera_asset.vertical_fov_radians : 1.04719755f;
            const float near_plane = editor && editor->test_session_active() ? editor->camera_asset.near_plane : 0.1f;
            const float far_plane = editor && editor->test_session_active() ? editor->camera_asset.far_plane : 2000.0f;
            float aspect = static_cast<float>(pixel_w) / static_cast<float>(std::max(pixel_h, 1));
            if (editor && editor->game_viewport_min && editor->game_viewport_max) {
                const float gw = editor->game_viewport_max->x - editor->game_viewport_min->x;
                const float gh = editor->game_viewport_max->y - editor->game_viewport_min->y;
                if (gw > 1.0f && gh > 1.0f) aspect = gw / gh;
            }
            (void)orbit_camera->set_perspective(fov, aspect, near_plane, far_plane);
            if (pending_orbit_zoom != 0.0f) {
                const float meters_per_notch =
                    editor ? std::max(0.05f, editor->camera_asset.zoom_sensitivity) : 1.5f;
                orbit_camera->adjust_distance(pending_orbit_zoom * meters_per_notch);
                pending_orbit_zoom = 0.0f;
            }
        } else {
            pending_orbit_zoom = 0.0f;
            (void)camera.set_perspective(1.04719755f, static_cast<float>(pixel_w) / static_cast<float>(std::max(pixel_h, 1)),
                0.1f, 2000.0f);
        }
        if (debug_world) {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const bool editor_mode = editor.has_value();
            const bool test_active = editor_mode && editor->test_session_active();
            const bool test_running = editor_mode && editor->test_session_running();
            const bool game_tab = editor_mode && editor->game_viewport_active();

            if (orbit_camera && (debug_character || player_locomotion) && test_active) {
                if (camera_look_active && game_tab) orbit_camera->apply_look(mouse_x, mouse_y);
                LocalPosition wish{};
                if (test_running && game_tab) {
                    wish.x = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
                    wish.z = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
                }
                if (test_running && game_tab && debug_character) {
                    const bool space_down = keys[SDL_SCANCODE_SPACE];
                    const bool space_pressed = space_down && !space_key_was_down;
                    if (space_pressed) (void)debug_character->jump();
                    const auto position_before = debug_character->position();
                    (void)debug_character->move(wish, orbit_camera->yaw(), frame_delta_seconds);
                    if (editor) record_movement_debug(*editor, *debug_character, wish, frame_delta_seconds, position_before);
                }
                if (test_running && game_tab && editor->ui_canvas_stack && debug_character) {
                    const float pending_damage = debug_character->pending_swim_damage();
                    if (pending_damage > 0.0f) {
                        auto& hud = editor->ui_canvas_stack->hud();
                        const double current = hud.get_number("player.health").value_or(100.0);
                        const double max = hud.get_number("player.healthMax").value_or(100.0);
                        hud.set_health(current - static_cast<double>(pending_damage), max);
                        debug_character->clear_pending_swim_damage();
                    }
                }
                if (player_locomotion) {
                    body_position = player_locomotion->feet_position();
                    (void)orbit_camera->update(body_position, *debug_world);
                } else {
                    body_position = debug_character->position();
                    (void)orbit_camera->update(character_feet_pivot(*debug_character), *debug_world);
                }
                // Face along the lens look (not orbit yaw alone) so OTS shoulder offset does not skew the mesh.
                {
                    const auto eye = orbit_camera->position();
                    const float target_x = static_cast<float>(body_position.x);
                    const float target_z = static_cast<float>(body_position.z);
                    player_facing_yaw =
                        character_facing_yaw_from_camera_look(eye[0], eye[2], target_x, target_z, player_facing_yaw);
                }

                const WorldPosition probe{body_position.x, body_position.y + 1.2, body_position.z};
                const InteractionVolumeRegistry& interact_reg =
                    placement_collision ? placement_collision->interaction_registry() : debug_interaction_registry;
                const CombatVolumeRegistry& combat_reg =
                    placement_collision ? placement_collision->combat_registry() : debug_combat_registry;
                if (test_running) {
                    (void)debug_interaction_tracker.update("player", probe, 0.65f, *debug_world, interact_reg);
                    const float yaw = orbit_camera->yaw();
                    const WorldPosition attack_probe{body_position.x + std::sin(yaw), body_position.y + 1.2f,
                        body_position.z + std::cos(yaw)};
                    (void)query_combat_hits("player_attack", attack_probe, 1.0f, *debug_world, combat_reg);
                }
                if (!game_tab) {
                    const bool camera_keyboard = camera_look_active && editor->viewport_focused;
                    CameraInput input{
                        camera_keyboard * ((keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f)),
                        camera_keyboard * ((keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f)),
                        camera_keyboard * ((keys[SDL_SCANCODE_SPACE] ? 1.0f : 0.0f) -
                            (keys[SDL_SCANCODE_LCTRL] ? 1.0f : 0.0f)),
                        mouse_x, mouse_y, camera_keyboard && keys[SDL_SCANCODE_LSHIFT]};
                    camera.apply(input, frame_delta_seconds);
                }
                mouse_x = mouse_y = 0;
            } else if (orbit_camera && debug_character && !editor_mode) {
                if (camera_look_active) orbit_camera->apply_look(mouse_x, mouse_y);
                mouse_x = mouse_y = 0;
                LocalPosition wish{};
                wish.x = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
                wish.z = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
                const bool space_down = keys[SDL_SCANCODE_SPACE];
                const bool space_pressed = space_down && !space_key_was_down;
                if (space_pressed) (void)debug_character->jump();
                (void)debug_character->move(wish, orbit_camera->yaw(), frame_delta_seconds);
                body_position = debug_character->position();
                (void)orbit_camera->update(character_feet_pivot(*debug_character), *debug_world);
                {
                    const auto eye = orbit_camera->position();
                    player_facing_yaw = character_facing_yaw_from_camera_look(eye[0], eye[2],
                        static_cast<float>(body_position.x), static_cast<float>(body_position.z), player_facing_yaw);
                }
                const WorldPosition probe{body_position.x, body_position.y + 1.2, body_position.z};
                (void)debug_interaction_tracker.update("player", probe, 0.65f, *debug_world, debug_interaction_registry);
                const float yaw = orbit_camera->yaw();
                const WorldPosition attack_probe{body_position.x + std::sin(yaw), body_position.y + 1.2f,
                    body_position.z + std::cos(yaw)};
                (void)query_combat_hits("player_attack", attack_probe, 1.0f, *debug_world, debug_combat_registry);
                view_projection_matrix = orbit_camera->view_projection();
                camera_position = orbit_camera->position();
            } else {
                const bool camera_keyboard =
                    editor_mode ? (camera_look_active && editor->viewport_focused) : camera_look_active;
                CameraInput input{
                    camera_keyboard * ((keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f)),
                    camera_keyboard * ((keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f)),
                    camera_keyboard * ((keys[SDL_SCANCODE_SPACE] ? 1.0f : 0.0f) -
                        (keys[SDL_SCANCODE_LCTRL] ? 1.0f : 0.0f)),
                    mouse_x, mouse_y, camera_keyboard && keys[SDL_SCANCODE_LSHIFT]};
                camera.apply(input, frame_delta_seconds);
                mouse_x = mouse_y = 0;
                view_projection_matrix = camera.view_projection();
                camera_position = camera.position();
            }
        }
        if (streamed_terrain && debug_world) {
            const std::array<float, 3> stream_focus = editor ? camera.position() : camera_position;
            const TerrainEditStore* edits = editor ? &editor->terrain_edits : nullptr;
            const TerrainPaintStore* paint = editor ? &editor->terrain_paint : nullptr;
            TerrainPaintMaterialLookup paint_lookup;
            if (editor) paint_lookup = editor_paint_material_lookup(*editor);
            const auto updated = streamed_terrain->update(*debug_world, stream_focus, terrain_material.physics,
                StreamedTerrainField::k_default_radius, edits, paint, paint_lookup);
            if (!updated) {
                SDL_DestroyWindow(window);
                SDL_Quit();
                return Result<RenderStats>::failure(updated.error());
            }
            if (streamed_foliage && editor) {
                streamed_foliage->set_palette(&editor->foliage_layers);
                streamed_foliage->set_density(&editor->foliage_density);
                const auto foliage_synced = streamed_foliage->sync(*streamed_terrain, stream_focus);
                if (!foliage_synced) {
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return Result<RenderStats>::failure(foliage_synced.error());
                }
            }
            if (streamed_terrain->render_data_dirty()) {
                const auto terrain_vertices = streamed_terrain->build_render_vertices(terrain_material.base_color);
                std::vector<Vertex> upload;
                upload.reserve(terrain_vertices.size());
                for (const auto& vertex : terrain_vertices)
                    upload.push_back({vertex.x, vertex.y, vertex.z, vertex.r, vertex.g, vertex.b});
                const auto uploaded = renderer.upload_terrain_vertices(upload);
                if (!uploaded) {
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return Result<RenderStats>::failure(uploaded.error());
                }
                streamed_terrain->clear_render_data_dirty();
            }
        }
        if (streamed_water && debug_world) {
            const std::array<float, 3> stream_focus = editor ? camera.position() : camera_position;
            const WaterStore* water = editor ? &editor->water_store : &runtime_water;
            const auto updated = streamed_water->update(stream_focus, StreamedWaterField::k_default_radius, water);
            if (!updated) {
                SDL_DestroyWindow(window);
                SDL_Quit();
                return Result<RenderStats>::failure(updated.error());
            }
            // Terrain sculpt changes bed height; rebuild water so sheets clip/skirt to the new basin.
            if (editor && editor->terrain_height_revision != water_terrain_revision_seen) {
                water_terrain_revision_seen = editor->terrain_height_revision;
                reload_loaded_water_cells(*editor, &*streamed_water);
            }
            if (streamed_water->render_data_dirty()) {
                const auto water_vertices = streamed_water->build_render_vertices();
                std::vector<Vertex> upload;
                upload.reserve(water_vertices.size());
                for (const auto& vertex : water_vertices)
                    upload.push_back(
                        {vertex.x, vertex.y, vertex.z, vertex.r, vertex.g, vertex.b, vertex.depth, 0.0f});
                const auto uploaded = renderer.upload_water_vertices(upload);
                if (!uploaded) {
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return Result<RenderStats>::failure(uploaded.error());
                }
                streamed_water->clear_render_data_dirty();
            }
        }
        if (placement_collision && debug_world && editor) {
            const bool simulate_dynamics = editor->test_session_running();
            const auto synced =
                placement_collision->sync(*debug_world, editor->scene, editor->prefab_catalog, simulate_dynamics);
            if (!synced) {
                SDL_DestroyWindow(window);
                SDL_Quit();
                return Result<RenderStats>::failure(synced.error());
            }
            if (simulate_dynamics && player_locomotion && editor->test_player_spawn_entity &&
                editor->game_viewport_active()) {
                if (const auto body = placement_collision->motion_body_for(*editor->test_player_spawn_entity)) {
                    if (player_locomotion->body().value != body->value) {
                        player_locomotion.emplace(*debug_world, *body, player_locomotion->config(),
                            player_locomotion->capsule_radius(), player_locomotion->capsule_half_height());
                    }
                    const bool* loco_keys = SDL_GetKeyboardState(nullptr);
                    LocalPosition wish{};
                    wish.x = (loco_keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (loco_keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
                    wish.z = (loco_keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (loco_keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
                    const bool space_down = loco_keys[SDL_SCANCODE_SPACE];
                    if (space_down && !space_key_was_down) (void)player_locomotion->jump();
                    const float yaw = orbit_camera ? orbit_camera->yaw() : camera.yaw();
                    (void)player_locomotion->move(wish, yaw, frame_delta_seconds);
                }
            }
            const bool editor_physics_step = !editor->test_session_active() || editor->test_session_running();
            if (editor_physics_step) (void)debug_world->step(frame_delta_seconds);
            if (editor_physics_step && simulate_dynamics)
                placement_collision->write_back_transforms(editor->scene, *debug_world);
            if (editor_physics_step)
                for (const auto& contact_event : debug_world->drain_contact_events()) {
                    if (contact_event.type == ContactEventType::Enter && contact_event.contact_point)
                        editor->recent_contact_points.push_back(*contact_event.contact_point);
                    const bool a_trigger = contact_event.layer_a == CollisionLayer::Trigger;
                    const bool b_trigger = contact_event.layer_b == CollisionLayer::Trigger;
                    if (a_trigger == b_trigger) continue;
                    const auto trigger_body = a_trigger ? contact_event.body_a : contact_event.body_b;
                    const auto other_body = a_trigger ? contact_event.body_b : contact_event.body_a;
                    const auto binding = placement_collision->interaction_registry().find(trigger_body);
                    if (!binding) continue;
                    InteractionEvent mapped;
                    mapped.type = contact_event.type == ContactEventType::Enter ? InteractionEventType::Enter
                                                                               : InteractionEventType::Exit;
                    mapped.placement_entity_id = binding->placement_entity_id;
                    mapped.interaction_id = binding->interaction_id;
                    mapped.volume_index = binding->volume_index;
                    mapped.interactor_id = std::to_string(other_body.value);
                    mapped.contact_point = contact_event.contact_point;
                    editor->recent_interaction_events.push_back(mapped);
                }
            if (editor_physics_step)
                for (const auto& hit_body :
                    placement_collision->combat_registry().bodies_for_role(CombatVolumeRole::Hit)) {
                    const auto binding = placement_collision->combat_registry().find(hit_body);
                    if (!binding) continue;
                    for (const auto& contact : query_combat_hits_from_body(binding->combat_id, hit_body, *debug_world,
                             placement_collision->combat_registry()))
                        editor->recent_combat_events.push_back(contact);
                }
            if (editor->recent_contact_points.size() > 64)
                editor->recent_contact_points.erase(editor->recent_contact_points.begin(),
                    editor->recent_contact_points.end() - 64);
            if (editor->recent_interaction_events.size() > 32)
                editor->recent_interaction_events.erase(editor->recent_interaction_events.begin(),
                    editor->recent_interaction_events.end() - 32);
            if (editor->recent_combat_events.size() > 32)
                editor->recent_combat_events.erase(editor->recent_combat_events.begin(),
                    editor->recent_combat_events.end() - 32);
            dispatch_pending_script_events(*editor);
        }
        if (debug_world) {
            const bool* keys_after = SDL_GetKeyboardState(nullptr);
            space_key_was_down = keys_after[SDL_SCANCODE_SPACE];
        }
        if (options.editor) {
            if (editor->active_viewport_tab == EditorState::ViewportTab::WorldForge)
                renderer.ensure_world_forge_placeholder_textures(editor->world_forge_editor);
            draw_editor(*editor, debug_world.get(), camera_look_active, renderer.scene_viewport_texture(),
                renderer.game_viewport_texture(), camera.view_matrix(), camera.projection_matrix(),
                camera.view_projection(), camera.position(), streamed_terrain ? &*streamed_terrain : nullptr,
                streamed_foliage ? &*streamed_foliage : nullptr, streamed_water ? &*streamed_water : nullptr,
                &terrain_material, debug_character ? &*debug_character : nullptr,
                placement_collision ? &placement_collision->interaction_registry() : nullptr,
                placement_collision ? &placement_collision->combat_registry() : nullptr);
            apply_pending_world_forge_marker_focus(*editor, camera);
        }
        if (streamed_foliage && editor && streamed_foliage->dirty()) {
            const auto uploaded = renderer.set_foliage_batches(streamed_foliage->batches(), &editor->foliage_layers);
            if (!uploaded) {
                SDL_DestroyWindow(window);
                SDL_Quit();
                return Result<RenderStats>::failure(uploaded.error());
            }
            streamed_foliage->clear_dirty();
        }
        if (editor && editor->test_player_spawn_entity && editor->test_session_active()) {
            if (auto transform = editor->scene.transform(*editor->test_player_spawn_entity)) {
                if (debug_character) {
                    const PrefabAsset* spawn_prefab = find_prefab(editor->prefab_catalog,
                        normalize_asset_path(editor->character_asset.visual_prefab));
                    if (spawn_prefab) {
                        *transform = character_visual_transform(*debug_character, *spawn_prefab, player_facing_yaw);
                    } else {
                        *transform = character_visual_transform(*debug_character, PrefabAsset{}, player_facing_yaw);
                    }
                    (void)editor->scene.set_transform(*editor->test_player_spawn_entity, *transform);
                } else if (player_locomotion) {
                    using namespace DirectX;
                    body_position = player_locomotion->feet_position();
                    if (orbit_camera) {
                        (void)orbit_camera->update(body_position, *debug_world);
                        const auto eye = orbit_camera->position();
                        player_facing_yaw = character_facing_yaw_from_camera_look(eye[0], eye[2],
                            static_cast<float>(body_position.x), static_cast<float>(body_position.z),
                            player_facing_yaw);
                    }
                    transform->position = {static_cast<float>(body_position.x), static_cast<float>(body_position.y),
                        static_cast<float>(body_position.z)};
                    constexpr float k_model_forward_yaw_offset = 3.14159265f;
                    const auto facing_q =
                        XMQuaternionRotationRollPitchYaw(0.0f, player_facing_yaw + k_model_forward_yaw_offset, 0.0f);
                    XMStoreFloat4(reinterpret_cast<XMFLOAT4*>(transform->rotation.data()), facing_q);
                    (void)editor->scene.set_transform(*editor->test_player_spawn_entity, *transform);
                }
            }
        }
        std::vector<RenderInstance> placed_objects;
        std::vector<std::pair<std::string, TransformComponent>> light_placements;
        const PrefabAsset::MaterialLookup editor_material_lookup =
            editor ? make_material_lookup(&editor->material_cache) : PrefabAsset::MaterialLookup{};
        if (editor && editor->prefab_meshes_dirty) {
            ensure_prefab_primitive_meshes(*editor, imported_meshes);
            const auto synced = renderer.sync_imported_meshes(imported_meshes);
            if (!synced) {
                SDL_DestroyWindow(window);
                SDL_Quit();
                return Result<RenderStats>::failure(synced.error());
            }
            editor->prefab_meshes_dirty = false;
        }
        if(editor){for(const auto& id:editor->scene.entity_ids()){auto placement=editor->scene.placement(id);auto transform=editor->scene.transform(id);if(placement&&transform){const bool previewing=editor->selected&&*editor->selected==id&&editor->gizmo_preview&&(editor->gizmo_was_using||editor->terrain_drag_active);const auto& draw_transform=previewing?*editor->gizmo_preview:*transform;if(const auto* prefab=find_prefab(editor->prefab_catalog,placement->prefab_asset))expand_prefab_render_instances(*prefab,draw_transform,placed_objects,editor_material_lookup);light_placements.push_back({normalize_asset_path(placement->prefab_asset),draw_transform});}}if(editor->drop_preview&&editor->drop_preview_prefab){if(const auto* prefab=find_prefab(editor->prefab_catalog,*editor->drop_preview_prefab))expand_prefab_render_instances(*prefab,*editor->drop_preview,placed_objects,editor_material_lookup);light_placements.push_back({normalize_asset_path(*editor->drop_preview_prefab),*editor->drop_preview});}if(editor->prefab_edit_path){const auto normalized=normalize_asset_path(*editor->prefab_edit_path);const auto found=editor->prefab_catalog.find(normalized);if(found!=editor->prefab_catalog.end()){TransformComponent preview_root;preview_root.position={0.0f,3.0f,0.0f};expand_prefab_render_instances(found->second,preview_root,placed_objects,editor_material_lookup);}}}
        bool draw_player_visual = false;
        const bool editor_spawn_visual =
            editor && editor->test_session_active() && editor->test_player_spawn_entity.has_value();
        if (debug_character && !editor_spawn_visual) {
            const PrefabAsset* player_prefab = nullptr;
            if (editor && editor->test_session_active()) {
                player_prefab =
                    find_prefab(editor->prefab_catalog, normalize_asset_path(editor->character_asset.visual_prefab));
            } else if (!editor && runtime_player_prefab) {
                player_prefab = &*runtime_player_prefab;
            }
            if (player_prefab) {
                append_character_render_instances(*player_prefab, *debug_character, player_facing_yaw, placed_objects,
                    editor_material_lookup);
                draw_player_visual = true;
            }
        }
        const auto point_lights=editor?collect_point_lights(editor->prefab_catalog,light_placements):std::vector<ActivePointLight>{};
        WorldInfluenceBus influence_bus;
        if (debug_character) {
            const auto pos = debug_character->position();
            const auto vel = debug_character->linear_velocity();
            WorldInfluenceSource source;
            source.position = {static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)};
            source.velocity = vel;
            source.radius = 1.25f;
            source.strength = 1.0f;
            source.kind = "character";
            influence_bus.add(std::move(source));
        } else if (player_locomotion) {
            const auto pos = player_locomotion->feet_position();
            const auto vel = player_locomotion->linear_velocity();
            WorldInfluenceSource source;
            source.position = {static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)};
            source.velocity = vel;
            source.radius = 1.25f;
            source.strength = 1.0f;
            source.kind = "character";
            influence_bus.add(std::move(source));
        }
        static float foliage_time_seconds = 0.0f;
        foliage_time_seconds += frame_delta_seconds;
        const PbrSurfaceParams terrain_pbr = material_supports_opaque_pbr_runtime(terrain_material)
                                                 ? PbrSurfaceParams::from_material(terrain_material)
                                                 : PbrSurfaceParams::dielectric_default();
        Renderer::WorldPassParams scene_pass;
        scene_pass.view_projection = camera.view_projection();
        scene_pass.camera_position = camera.position();
        scene_pass.body_position = body_position;
        scene_pass.draw_physics_body = (debug_character.has_value() || player_locomotion.has_value()) &&
            !draw_player_visual && (!editor || editor->test_session_active());
        scene_pass.influence = influence_bus.empty() ? nullptr : &influence_bus;
        scene_pass.time_seconds = foliage_time_seconds;
        scene_pass.terrain_pbr = terrain_pbr;
        scene_pass.water_color = water_color;
        scene_pass.water_roughness = water_roughness;
        Renderer::WorldPassParams game_pass = scene_pass;
        game_pass.draw_physics_body = false;
        if (orbit_camera && (debug_character || player_locomotion) && editor && editor->test_session_active()) {
            game_pass.view_projection = orbit_camera->view_projection();
            game_pass.camera_position = orbit_camera->position();
            game_pass.body_position = body_position;
        }
        Renderer::WorldPassParams runtime_pass;
        runtime_pass.view_projection = view_projection_matrix;
        runtime_pass.camera_position = camera_position;
        runtime_pass.body_position = body_position;
        runtime_pass.draw_physics_body = (debug_character.has_value() || player_locomotion.has_value()) && !draw_player_visual;
        runtime_pass.influence = influence_bus.empty() ? nullptr : &influence_bus;
        runtime_pass.time_seconds = foliage_time_seconds;
        runtime_pass.terrain_pbr = terrain_pbr;
        runtime_pass.water_color = water_color;
        runtime_pass.water_roughness = water_roughness;
        auto rendered = editor ? renderer.render(capture, scene_pass, placed_objects, point_lights, &game_pass)
                                 : renderer.render(capture, runtime_pass, placed_objects, point_lights);
        if (!rendered) { SDL_DestroyWindow(window); SDL_Quit(); return Result<RenderStats>::failure(rendered.error()); }
        ++frames;
        gpu_ms_total += renderer.last_gpu_ms();
        if (options.frame_limit > 0 && frames >= options.frame_limit) running = false;
    }
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    RenderStats stats{frames, elapsed, frames ? elapsed * 1000.0 / static_cast<double>(frames) : 0.0,
                      frames ? gpu_ms_total / static_cast<double>(frames) : 0.0,
                      elapsed > 0.0 ? static_cast<double>(frames) / elapsed : 0.0, renderer.adapter_name()};
    SDL_DestroyWindow(window);
    SDL_Quit();
    return Result<RenderStats>::success(std::move(stats));
}

} // namespace engine
