#include "engine/rendering/render_app.h"

#include "engine/diagnostics/logger.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/prefab_asset.h"
#include "engine/assets/character_asset.h"
#include "engine/assets/camera_asset.h"
#include "engine/assets/play_session_settings.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/character_controller.h"
#include "engine/rendering/debug_camera.h"
#include "engine/rendering/orbit_camera.h"
#include "engine/rendering/pbr_lighting.h"
#include "engine/rendering/viewport_picking.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_field.h"
#include "engine/world/transform_utils.h"
#include "engine/world/prefab_collision.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/combat_volumes.h"
#include "engine/editor/editor_fonts.h"
#include "engine/editor/editor_icons.h"
#include "engine/assets/asset_registry.h"
#include "engine/automation/scene_commands.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/automation_trace.h"
#include "engine/automation/editor_session.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/scripting/script_file_monitor.h"
#include "engine/quest/quest_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/hud_asset.h"
#include "engine/ui/hud_runtime.h"
#include "engine/ui/ui_canvas_editor.h"
#include "engine/ui/world_forge_editor.h"
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
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
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
struct Vertex { float x,y,z,r,g,b; };
struct RenderInstance {
    TransformComponent transform;
    std::string mesh_asset;
    PbrSurfaceParams pbr = PbrSurfaceParams::dielectric_default();
};

std::array<float, 24> pack_object_constants(const std::array<float, 16>& model, const PbrSurfaceParams& pbr) {
    std::array<float, 24> constants{};
    std::memcpy(constants.data(), model.data(), sizeof(model));
    constants[16] = pbr.roughness;
    constants[17] = pbr.metallic;
    constants[18] = 0.0f;
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
            vertices.push_back({vertex.x, vertex.y, vertex.z, vertex.r, vertex.g, vertex.b});
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
        if(editor){D3D12_DESCRIPTOR_HEAP_DESC imgui_desc{};imgui_desc.NumDescriptors=10;imgui_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;imgui_desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr=device_->CreateDescriptorHeap(&imgui_desc,IID_PPV_ARGS(&imgui_heap_));if(FAILED(hr))return Result<void>::failure(graphics_error("EDITOR-DESCRIPTOR-HEAP","Could not create editor descriptor heap",hr));}
        // Post-process descriptor heap (depth, lit color, AO) is shader-visible and created even outside the editor.
        D3D12_DESCRIPTOR_HEAP_DESC post_desc{};post_desc.NumDescriptors=3;post_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;post_desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
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
        if(editor){IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;if(hidden)io.IniFilename=nullptr;else{std::filesystem::create_directories("out/editor");io.IniFilename="out/editor/imgui.ini";}ImGui::StyleColorsDark();(void)EditorFonts::load(io);if(!ImGui_ImplSDL3_InitForD3D(window)||!ImGui_ImplDX12_Init(device_.Get(),frame_count,DXGI_FORMAT_R8G8B8A8_UNORM,imgui_heap_.Get(),imgui_heap_->GetCPUDescriptorHandleForHeapStart(),imgui_heap_->GetGPUDescriptorHandleForHeapStart()))return Result<void>::failure(graphics_error("EDITOR-IMGUI-INIT","Could not initialize Dear ImGui SDL3/D3D12 backends"));editor_initialized_=true;}
        return Result<void>::success();
    }

    Result<void> resize(UINT width, UINT height) {
        if (!swap_chain_ || width == 0 || height == 0 || (width == width_ && height == height_)) return Result<void>::success();
        wait_for_gpu();
        for (auto& target : targets_) target.Reset();
        viewport_target_.Reset();
        game_viewport_target_.Reset();
        lit_color_.Reset();
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
        frame_cb_mapped_ = nullptr;
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
        return Result<void>::success();
    }

    void bind_frame_constants(const std::array<float, 48>& frame_constants) {
        std::memcpy(frame_cb_mapped_, frame_constants.data(), sizeof(frame_constants));
        command_list_->SetGraphicsRootConstantBufferView(0, frame_cb_->GetGPUVirtualAddress());
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
        return upload_prop_vertices(vertices);
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

    struct WorldPassParams {
        std::array<float, 16> view_projection{};
        std::array<float, 3> camera_position{};
        WorldPosition body_position{};
        bool draw_physics_body = false;
        const WorldInfluenceBus* influence = nullptr;
        float time_seconds = 0.0f;
        PbrSurfaceParams terrain_pbr = PbrSurfaceParams::dielectric_default();
    };

    void draw_world_pass(ID3D12Resource* color_target, D3D12_CPU_DESCRIPTOR_HANDLE rtv, const WorldPassParams& params,
        const std::vector<RenderInstance>& placed_objects, const std::vector<ActivePointLight>& point_lights,
        bool record_gpu_timestamp) {
        auto barrier = CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);
        command_list_->ResourceBarrier(1, &barrier);
        const float clear[] = {0.11f, 0.16f, 0.24f, 1.0f};
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
        std::array<float, 48> frame_constants{};
        std::memcpy(frame_constants.data(), params.view_projection.data(), sizeof(params.view_projection));
        frame_constants[16] = params.camera_position[0];
        frame_constants[17] = params.camera_position[1];
        frame_constants[18] = params.camera_position[2];
        frame_constants[19] = 90.0f;
        frame_constants[20] = 0.10f;
        frame_constants[21] = 0.14f;
        frame_constants[22] = 0.20f;
        frame_constants[23] = 560.0f;
        frame_constants[24] = -0.35f;
        frame_constants[25] = -0.85f;
        frame_constants[26] = -0.25f;
        frame_constants[27] = 0.58f;
        pack_point_lights(frame_constants, point_lights, params.camera_position);
        frame_constants[44] = static_cast<float>(width_);
        frame_constants[45] = static_cast<float>(height_);
        bind_frame_constants(frame_constants);
        const std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        const auto bind_object = [&](const std::array<float, 16>& model, const PbrSurfaceParams& pbr) {
            const auto constants = pack_object_constants(model, pbr);
            command_list_->SetGraphicsRoot32BitConstants(1, 24, constants.data(), 0);
        };
        command_list_->IASetVertexBuffers(0, 1, &vertex_view_);
        command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        if (debug_world_ && sky_vertex_count_ > 0) {
            command_list_->SetPipelineState(sky_pipeline_.Get());
            bind_object(identity, PbrSurfaceParams::dielectric_default());
            command_list_->DrawInstanced(sky_vertex_count_, 1, sky_vertex_offset_, 0);
            command_list_->SetPipelineState(pipeline_.Get());
        }
        bind_object(identity, params.terrain_pbr);
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
            bind_object(model, PbrSurfaceParams::dielectric_default());
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
            bind_object(placed_model, instance.pbr);
            command_list_->DrawInstanced(found->second.second, 1, found->second.first, 0);
        }
        draw_foliage_instances(frame_constants, params.influence, params.time_seconds);
        barrier = CD3DX12_RESOURCE_BARRIER_placeholder(color_target, D3D12_RESOURCE_STATE_RENDER_TARGET,
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
        const float clear[] = {0.11f, 0.16f, 0.24f, 1.0f};
        command_list_->OMSetRenderTargets(1, &backbuffer_rtv, FALSE, nullptr);
        command_list_->ClearRenderTargetView(backbuffer_rtv, clear, 0, nullptr);
        if (editor_initialized_) {
            draw_world_pass(lit_color_.Get(), lit_rtv_, world, placed_objects, point_lights, true);
            apply_ssao(viewport_target_.Get(), viewport_rtv_, /*destination_returns_to_srv=*/true, world);
            if (game_world) {
                draw_world_pass(lit_color_.Get(), lit_rtv_, *game_world, placed_objects, point_lights, false);
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
        if (capture) {
            auto copy_barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
            command_list_->ResourceBarrier(1, &copy_barrier);
            const auto desc = targets_[frame_index_]->GetDesc();
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
            source.pResource = targets_[frame_index_].Get();
            source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            source.SubresourceIndex = 0;
            command_list_->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
            barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
        } else {
            barrier = CD3DX12_RESOURCE_BARRIER_placeholder(targets_[frame_index_].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        }
        command_list_->ResourceBarrier(1, &barrier);
        hr = command_list_->Close();
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-LIST-CLOSE", "Could not close command list", hr));
        ID3D12CommandList* lists[] = {command_list_.Get()};
        queue_->ExecuteCommandLists(1, lists);
        hr = swap_chain_->Present(1, 0);
        if (FAILED(hr)) return Result<void>::failure(device_error("GFX-PRESENT", "Could not present frame", hr));
        wait_for_gpu();

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
        if (session.concept_placeholder_tex_ready || !editor_requested_ || !imgui_heap_ || !device_ || !queue_)
            return;
        session.concept_placeholder_tex_ready = true;

        static constexpr const char* keys[] = {"person", "deity", "artifact", "organization", "faction", "region", "poi"};
        static constexpr const char* files[] = {"wf_person.png", "wf_deity.png", "wf_artifact.png", "wf_organization.png",
            "wf_faction.png", "wf_region.png", "wf_poi.png"};
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
            auto loaded = load_png_imgui_srv(device_.Get(), queue_.Get(), imgui_heap_.Get(), srv_stride_, srv_index,
                path, &raw, [this]() { wait_for_gpu(); });
            if (!loaded || !raw) {
                Logger::instance().write(Severity::Warning, "world-forge",
                    std::string("Concept placeholder PNG failed: ") + files[i]);
                continue;
            }
            world_forge_placeholder_textures_[static_cast<std::size_t>(i)].Attach(raw);
            session.concept_placeholder_tex[keys[i]] = loaded.value();
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

        // Post-process shader-visible descriptors: slot 0 = depth SRV, slot 1 = lit SRV, slot 2 = AO SRV.
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
        return upload_prop_vertices(vertices);
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
            struct In { float3 position:POSITION; float3 color:COLOR; };
            struct Out { float4 position : SV_POSITION; float3 color : COLOR; float3 worldPos : TEXCOORD0; };
            Out vs(In input) {
                Out o;
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
                    float3 horizon = float3(0.10, 0.14, 0.20);
                    float3 zenith = float3(0.20, 0.26, 0.36);
                    return float4(lerp(horizon, zenith, saturate(t)), 1.0);
                }
                float dist = distance(input.worldPos, cameraAndFogStart.xyz);
                float fogRange = max(fogColorAndEnd.w - cameraAndFogStart.w, 0.001);
                float fogFactor = saturate((fogColorAndEnd.w - dist) / fogRange);
                float3 dpdx = ddx(input.worldPos);
                float3 dpdy = ddy(input.worldPos);
                float3 normal = normalize(cross(dpdx, dpdy));
                float3 V = normalize(cameraAndFogStart.xyz - input.worldPos);
                float3 L = normalize(-lightAndAmbient.xyz);
                float3 lit = input.color * lightAndAmbient.w;
                lit += shadePbr(input.color, materialParams.x, materialParams.y, normal, V, L, float3(1.0, 1.0, 1.0));
                lit += applyPointLightPbr(input.worldPos, input.color, materialParams.x, materialParams.y, normal, V,
                    pointLight0PosRadius, pointLight0ColorStrength);
                lit += applyPointLightPbr(input.worldPos, input.color, materialParams.x, materialParams.y, normal, V,
                    pointLight1PosRadius, pointLight1ColorStrength);
                lit += emissive.rgb;
                return float4(lerp(fogColorAndEnd.rgb, lit, fogFactor), 1.0);
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

        D3D12_ROOT_PARAMETER parameters[2]{};
        parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        parameters[0].Descriptor.ShaderRegister = 0;
        parameters[0].Descriptor.RegisterSpace = 0;
        parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameters[1].Constants.ShaderRegister = 1;
        parameters[1].Constants.RegisterSpace = 0;
        parameters[1].Constants.Num32BitValues = 24;
        parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_ROOT_SIGNATURE_DESC root{};
        root.NumParameters = 2;
        root.pParameters = parameters;
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
        D3D12_INPUT_ELEMENT_DESC input[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};state.InputLayout={input,2};
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
                float3 dpdx = ddx(input.worldPos);
                float3 dpdy = ddy(input.worldPos);
                float3 normal = normalize(cross(dpdx, dpdy));
                float3 V = normalize(cameraAndFogStart.xyz - input.worldPos);
                float3 L = normalize(-lightAndAmbient.xyz);
                float3 lit = input.color * lightAndAmbient.w;
                lit += shadePbr(input.color, 1.0, 0.0, normal, V, L, float3(1.0, 1.0, 1.0));
                lit += applyPointLightPbr(input.worldPos, input.color, 1.0, 0.0, normal, V, pointLight0PosRadius,
                    pointLight0ColorStrength);
                lit += applyPointLightPbr(input.worldPos, input.color, 1.0, 0.0, normal, V, pointLight1PosRadius,
                    pointLight1ColorStrength);
                return float4(lerp(fogColorAndEnd.rgb, lit, fogFactor), 1.0);
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
            {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
        state.InputLayout = {input, 2};
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

private:
    EngineError device_error(std::string code, std::string message, HRESULT hr) const {
        EngineError error = graphics_error(std::move(code), std::move(message), hr);
        if (device_) {
            const HRESULT removed = device_->GetDeviceRemovedReason();
            if (FAILED(removed)) {
                error.category = ErrorCategory::DeviceRemoval;
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
    ComPtr<ID3D12Resource> lit_color_, ao_target_;
    D3D12_CPU_DESCRIPTOR_HANDLE lit_rtv_{}, ao_rtv_{};
    UINT ao_width_ = 1, ao_height_ = 1;
    ComPtr<ID3D12DescriptorHeap> post_srv_heap_;
    D3D12_GPU_DESCRIPTOR_HANDLE post_depth_gpu_{}, post_lit_gpu_{};
    ComPtr<ID3D12RootSignature> ssao_root_signature_, composite_root_signature_;
    ComPtr<ID3D12PipelineState> ssao_pipeline_, composite_pipeline_;
    ComPtr<ID3D12Resource> ssao_cb_, composite_cb_;
    void* ssao_cb_mapped_ = nullptr;
    void* composite_cb_mapped_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertex_view_{};
    D3D12_VERTEX_BUFFER_VIEW terrain_vertex_view_{};
    UINT terrain_vertex_count_=0;
    UINT sky_vertex_offset_=0;
    UINT sky_vertex_count_=0;
    std::map<std::string,std::pair<UINT,UINT>> mesh_ranges_;
    std::array<ComPtr<ID3D12Resource>, frame_count> targets_;
    std::array<ComPtr<ID3D12CommandAllocator>, frame_count> allocators_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    ComPtr<ID3D12RootSignature> root_signature_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12PipelineState> sky_pipeline_;
    ComPtr<ID3D12RootSignature> foliage_root_signature_;
    ComPtr<ID3D12PipelineState> foliage_pipeline_;
    ComPtr<ID3D12Resource> frame_cb_;
    void* frame_cb_mapped_ = nullptr;
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
    enum class ViewportTab : std::uint8_t { Scene, Sculpt, Game, UI, WorldForge };

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
    enum class SculptTool : std::uint8_t { Height, Flatten, Paint, Foliage };

    SculptTool sculpt_tool = SculptTool::Height;
    TerrainEditStore terrain_edits;
    TerrainEditHistory terrain_history;
    bool terrain_edits_dirty = false;
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
    ScriptFileMonitor script_monitor;
    std::uint32_t script_reload_counter = 0;
    std::uint32_t bridge_poll_counter = 0;
    bool live_automation_enabled = false;
    std::size_t lua_dispatched_interactions = 0;
    std::size_t lua_dispatched_combat = 0;
    std::optional<ImVec2> game_viewport_min;
    std::optional<ImVec2> game_viewport_max;
    bool game_viewport_hovered = false;

    [[nodiscard]] bool test_session_active() const { return test_session != TestSessionState::Inactive; }
    [[nodiscard]] bool test_session_running() const { return test_session == TestSessionState::Running; }
    bool force_select_viewport_tab = false;
    bool lock_viewport_tab = false;
    [[nodiscard]] bool game_viewport_active() const { return active_viewport_tab == ViewportTab::Game; }
    [[nodiscard]] bool sculpt_viewport_active() const { return active_viewport_tab == ViewportTab::Sculpt; }
    [[nodiscard]] bool ui_viewport_active() const { return active_viewport_tab == ViewportTab::UI; }
    [[nodiscard]] bool world_forge_viewport_active() const {
        return active_viewport_tab == ViewportTab::WorldForge;
    }
};

void commit_active_terrain_stroke(EditorState& state);

void mark_scene_dirty(EditorState& state) { state.scene_dirty = true; }

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
    int shape_index = volume.shape == PrefabCollisionShape::Box ? 0 : 1;
    if (ImGui::Combo("Shape", &shape_index, "box\0sphere\0")) {
        volume.shape = shape_index == 0 ? PrefabCollisionShape::Box : PrefabCollisionShape::Sphere;
        commit = true;
    }
    if (volume.shape == PrefabCollisionShape::Box) {
        float half_extent[3] = {volume.half_extent.x, volume.half_extent.y, volume.half_extent.z};
        ImGui::InputFloat3("Half Extent", half_extent);
        volume.half_extent = {half_extent[0], half_extent[1], half_extent[2]};
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
    ImGui::TextDisabled("Camera Asset");
    ImGui::Text("%s", asset_path.c_str());
    if (ImGui::InputFloat("Pivot Height", &state.camera_asset.pivot_height, 0.05f, 0.2f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Min Distance", &state.camera_asset.min_distance, 0.05f, 0.25f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Max Distance", &state.camera_asset.max_distance, 0.1f, 0.5f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Default Distance", &state.camera_asset.default_distance, 0.1f, 0.5f, "%.2f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Collision Probe Radius", &state.camera_asset.collision_probe_radius, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Collision Padding", &state.camera_asset.collision_padding, 0.01f, 0.05f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Look Sensitivity", &state.camera_asset.look_sensitivity, 0.0001f, 0.001f, "%.4f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Vertical FOV (rad)", &state.camera_asset.vertical_fov_radians, 0.01f, 0.1f, "%.4f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Near Plane", &state.camera_asset.near_plane, 0.01f, 0.1f, "%.3f"))
        state.camera_asset_dirty = true;
    if (ImGui::InputFloat("Far Plane", &state.camera_asset.far_plane, 1.0f, 50.0f, "%.1f"))
        state.camera_asset_dirty = true;
    ImGui::BeginDisabled(state.test_session_active());
    if (ImGui::Button("Save Camera Asset") && !state.test_session_active()) {
        const auto valid = state.camera_asset.validate();
        if (!valid) {
            state.status = valid.error().message;
            Logger::instance().write(valid.error());
        } else {
            const auto saved = save_text_asset(state.project_root / asset_path, state.camera_asset.to_json());
            state.status = saved ? "Camera asset saved" : saved.error().message;
            if (!saved) Logger::instance().write(saved.error());
            else state.camera_asset_dirty = false;
        }
    }
    ImGui::EndDisabled();
    if (state.test_session_active()) ImGui::TextDisabled("End test session to save camera asset changes.");
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

TransformComponent character_visual_transform(const CharacterController& character) {
    const auto body = character.debug_body();
    TransformComponent transform;
    transform.position = {static_cast<float>(body.position.x), static_cast<float>(body.position.y),
                          static_cast<float>(body.position.z)};
    constexpr float k_unit_radius = 0.5f;
    constexpr float k_unit_half_height = 1.0f;
    const float diameter_scale = body.radius / k_unit_radius;
    const float height_scale = (body.half_extent.x + body.radius) / k_unit_half_height;
    transform.scale = {diameter_scale, height_scale, diameter_scale};
    return transform;
}

WorldPosition character_feet_pivot(const CharacterController& character) {
    const auto body = character.debug_body();
    return {body.position.x, body.position.y - static_cast<double>(body.half_extent.x + body.radius), body.position.z};
}

void append_character_render_instances(const PrefabAsset& player_prefab, const CharacterController& character,
    std::vector<RenderInstance>& instances, const PrefabAsset::MaterialLookup& lookup_material = {}) {
    expand_prefab_render_instances(player_prefab, character_visual_transform(character), instances, lookup_material);
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
    const auto key = primitive_mesh_cache_key(*player_prefab.parts.front().mesh.primitive, player_prefab.parts.front().mesh.color);
    for (const auto& mesh : imported_meshes) {
        if (mesh.first == key) return Result<void>::success();
    }
    auto generated = generate_primitive_mesh(*player_prefab.parts.front().mesh.primitive, player_prefab.parts.front().mesh.color);
    if (!generated) return Result<void>::failure(generated.error());
    imported_meshes.emplace_back(key, std::move(generated.value()));
    return Result<void>::success();
}

void ensure_prefab_primitive_meshes(EditorState& state, std::vector<std::pair<std::string, ImportedMesh>>& imported_meshes) {
    const auto lookup_material = make_material_lookup(&state.material_cache);
    std::set<std::string> existing;
    for (const auto& mesh : imported_meshes) existing.insert(mesh.first);
    for (const auto& entry : state.prefab_catalog) {
        for (const auto& key : entry.second.required_mesh_keys(lookup_material)) {
            if (existing.find(key) != existing.end()) continue;
            if (key.rfind("__primitive/", 0) != 0) continue;
            const auto rest = key.substr(12);
            const auto slash = rest.find('/');
            if (slash == std::string::npos) continue;
            const auto primitive = rest.substr(0, slash);
            const auto color_token = rest.substr(slash + 1);
            std::array<float, 3> color{};
            if (std::sscanf(color_token.c_str(), "%f_%f_%f", &color[0], &color[1], &color[2]) != 3) continue;
            auto generated = generate_primitive_mesh(primitive, color);
            if (!generated) continue;
            state.mesh_bounds[key] = generated.value().aabb;
            imported_meshes.emplace_back(key, std::move(generated.value()));
            existing.insert(key);
        }
    }
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

void commit_active_terrain_stroke(EditorState& state) {
    if (!state.terrain_brush_active) return;
    if (state.sculpt_tool == EditorState::SculptTool::Paint) commit_terrain_paint_stroke(state);
    else if (state.sculpt_tool == EditorState::SculptTool::Foliage) commit_foliage_density_stroke(state);
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
    StreamedTerrainField* streamed_terrain, CollisionWorld* collision) {
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
                state.terrain_edits_dirty = false;
                state.terrain_paint_dirty = false;
                state.foliage_density_dirty = false;
                state.scene_dirty = false;
                state.status = "World, terrain sculpt, terrain paint, and foliage saved";
            }
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Foliage &&
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
                state.terrain_edits_dirty = true;
            }
        } else if (state.history.undo_size() > 0) {
            const auto result = state.history.undo(state.scene);
            state.status = result ? state.history.last_summary() : result.error().message;
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        if (state.sculpt_viewport_active() && state.sculpt_tool == EditorState::SculptTool::Foliage &&
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
                state.terrain_edits_dirty = true;
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
    const float screen_radius = std::clamp(screen_radius_raw, 4.0f, std::max(frame.width, frame.height) * 0.45f);
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
    CollisionWorld* collision, MaterialAsset* terrain_material, const std::array<float, 3>& camera_position) {
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
                ImGui::Selectable((display_name + "##asset-" + entry.first).c_str(),
                    state.inspector_asset_path && normalize_asset_path(*state.inspector_asset_path) == normalized);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
                begin_asset_file_drag_source(normalized, display_name, false);
                ImGui::SameLine();
                if (ImGui::SmallButton(("Inspect##" + entry.first).c_str())) {
                    state.inspector_asset_path = normalized;
                    state.selected.reset();
                    state.rename_target.reset();
                    if (is_character) {
                        if (const auto loaded = CharacterAsset::load(state.project_root / normalized))
                            state.character_asset = loaded.value();
                    } else if (is_camera) {
                        if (const auto loaded = CameraAsset::load(state.project_root / normalized))
                            state.camera_asset = loaded.value();
                    } else if (const auto loaded = MaterialAsset::load(state.project_root / normalized)) {
                        state.material_asset = loaded.value();
                        state.material_asset_dirty = false;
                    }
                    state.status = "Inspecting " + normalized;
                }
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

void draw_editor(EditorState& state, CollisionWorld* collision, bool camera_capture, ImTextureID scene_texture,
    ImTextureID game_texture, const std::array<float, 16>& view, const std::array<float, 16>& projection,
    const std::array<float, 16>& view_projection, const std::array<float, 3>& camera_position,
    StreamedTerrainField* streamed_terrain = nullptr, StreamedFoliageField* streamed_foliage = nullptr,
    MaterialAsset* terrain_material = nullptr, const CharacterController* character = nullptr,
    const InteractionVolumeRegistry* interactions = nullptr, const CombatVolumeRegistry* combat = nullptr) {
    const auto* main = ImGui::GetMainViewport();
    const ImVec2 origin{main->WorkPos.x, main->WorkPos.y + 20.0f};
    const ImVec2 extent{main->WorkSize.x, std::max(1.0f, main->WorkSize.y - 20.0f)};
    const float left = std::min(extent.x * 0.20f, 310.0f);
    const float right = std::min(extent.x * 0.25f, 390.0f);
    const float center = std::max(1.0f, extent.x - left - right);
    const float top = extent.y * 0.63f;
    const float bottom = extent.y - top;
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
                        state.terrain_edits_dirty = false;
                        state.terrain_paint_dirty = false;
                        state.foliage_density_dirty = false;
                        state.status = "World, terrain sculpt, terrain paint, and foliage saved";
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
    handle_editor_shortcuts(state, camera_capture, terrain_material, streamed_terrain, collision);

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
        if (ImGui::BeginTabItem(ICON_FA_CUBE " Scene##ViewportScene", nullptr, tab_flags(EditorState::ViewportTab::Scene))) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Scene;
            ImGui::EndTabItem();
        }
        ImGui::BeginDisabled(state.test_session_active());
        if (ImGui::BeginTabItem(ICON_FA_MOUNTAIN " Sculpt##ViewportSculpt", nullptr, tab_flags(EditorState::ViewportTab::Sculpt))) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Sculpt;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        if (ImGui::BeginTabItem(ICON_FA_GAMEPAD " Game##ViewportGame", nullptr, tab_flags(EditorState::ViewportTab::Game))) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::Game;
            ImGui::EndTabItem();
        }
        ImGui::BeginDisabled(state.test_session_active());
        if (ImGui::BeginTabItem(ICON_FA_DESKTOP " UI##ViewportUI", nullptr, tab_flags(EditorState::ViewportTab::UI))) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::UI;
            ImGui::EndTabItem();
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(state.test_session_active());
        if (ImGui::BeginTabItem(ICON_FA_GLOBE " World Forge##ViewportWorldForge", nullptr,
                tab_flags(EditorState::ViewportTab::WorldForge))) {
            if (!state.lock_viewport_tab) state.active_viewport_tab = EditorState::ViewportTab::WorldForge;
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
    if (edit_mode && sculpt_tab) draw_sculpt_toolbar(state, streamed_terrain, streamed_foliage, collision, terrain_material, camera_position);
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
        draw_world_forge_viewport(state.world_forge_editor, state.project_root);
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
                state.terrain_edits_dirty = true;
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
                state.terrain_edits_dirty = true;
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

            if (state.viewport_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                state.terrain_brush_active = true;
                state.terrain_brush_before.clear();
                state.terrain_brush_touched.clear();
                state.terrain_paint_brush_before.clear();
                state.terrain_paint_brush_touched.clear();
                state.foliage_brush_before.clear();
                state.foliage_brush_touched.clear();
                state.terrain_flatten_target_valid = false;
                if (const auto hit = viewport_raycast(collision, frame, view, projection, mouse)) {
                    if (state.sculpt_tool == EditorState::SculptTool::Paint)
                        apply_terrain_paint_brush_at(*hit);
                    else if (state.sculpt_tool == EditorState::SculptTool::Foliage)
                        apply_foliage_brush_at(*hit, ImGui::GetIO().KeyShift);
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
            ImGui::Text("Colliders: %zu | Script bindings: %zu", prefab.collision.size(), prefab.script_bindings.size());
            for (std::size_t index = 0; index < prefab.collision.size(); ++index) {
                auto& volume = prefab.collision[index];
                ImGui::PushID(static_cast<int>(index));
                const char* shape_label = volume.shape == PrefabCollisionShape::Box ? "box" : "sphere";
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
        ImGui::TextDisabled("Off by default. Enable before Cursor MCP tools can edit this session.");
    }
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
    if(options.debug_world&&!options.project_root.empty()){
        const auto path=options.project_root/"assets/materials/terrain.material.json";
        auto loaded=MaterialAsset::load(path);
        if(!loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(loaded.error());}
        terrain_material=loaded.value();
        const auto edits_path=default_terrain_edits_path(options.project_root);
        if(std::filesystem::exists(edits_path)){const auto edits_loaded=TerrainEditStore::load(edits_path);if(!edits_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(edits_loaded.error());}runtime_terrain_edits=std::move(edits_loaded.value());}
        if(!options.editor)set_active_terrain_edits(&runtime_terrain_edits);
    }
    std::optional<EditorState> editor;
    if(options.editor){auto scene=Scene::load(options.world_path);if(!scene){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(scene.error());}EditorState value;value.scene=std::move(scene.value());const auto ids=value.scene.entity_ids();if(!ids.empty())value.selected=ids.front();value.world_path=options.world_path;auto scanned=value.assets.scan(options.project_root);if(!scanned){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(scanned.error());}const auto prefabs=load_prefab_catalog(value,options.project_root);if(!prefabs){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(prefabs.error());}const auto play=load_editor_play_session(value);if(!play){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(play.error());}sync_player_placement_tags(value);(void)value.scene.repair_prefab_paths(value.prefab_catalog);if(value.scene.seed_missing_authored_components(value.prefab_catalog)>0)value.scene_dirty=true;value.character_asset.visual_prefab=resolve_prefab_catalog_path(value.prefab_catalog,value.character_asset.visual_prefab);const auto edits_path=default_terrain_edits_path(options.project_root);if(std::filesystem::exists(edits_path)){const auto edits_loaded=TerrainEditStore::load(edits_path);if(!edits_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(edits_loaded.error());}value.terrain_edits=std::move(edits_loaded.value());}const auto paint_path=default_terrain_paint_path(options.project_root);if(std::filesystem::exists(paint_path)){const auto paint_loaded=TerrainPaintStore::load(paint_path);if(!paint_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(paint_loaded.error());}value.terrain_paint=std::move(paint_loaded.value());}const auto foliage_layers_path=default_foliage_layers_path(options.project_root);if(std::filesystem::exists(foliage_layers_path)){const auto layers_loaded=FoliageLayerPalette::load(foliage_layers_path);if(!layers_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(layers_loaded.error());}value.foliage_layers=std::move(layers_loaded.value());}else{value.foliage_layers.schema_version=1;value.foliage_layers.layers={{"grass","Grass","grass_blade",{0.14f,0.22f,0.10f},0.55f,1.0f,0.15f,0.55f,0.35f,1.2f,0.55f,"grass_walk"},{"flower","Flower","flower_clump",{0.62f,0.28f,0.48f},0.45f,0.85f,0.03f,0.45f,0.1f,0.9f,0.45f,"",FoliageScatterMode::GroundCover,64},{"bush","Bush","bush",{0.14f,0.22f,0.10f},0.85f,1.15f,1.0f,0.5f,0.08f,1.4f,0.95f,"",FoliageScatterMode::Discrete,72}};}const auto foliage_density_path=default_foliage_density_path(options.project_root);if(std::filesystem::exists(foliage_density_path)){const auto foliage_loaded=FoliageDensityStore::load(foliage_density_path);if(!foliage_loaded){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(foliage_loaded.error());}value.foliage_density=std::move(foliage_loaded.value());}value.terrain_paint_brush_material=value.terrain_material_path;warm_material_cache(value);if(!options.initial_viewport.empty()){const auto&v=options.initial_viewport;if(v=="sculpt")value.active_viewport_tab=EditorState::ViewportTab::Sculpt;else if(v=="game")value.active_viewport_tab=EditorState::ViewportTab::Game;else if(v=="ui")value.active_viewport_tab=EditorState::ViewportTab::UI;else if(v=="world-forge"||v=="world_forge"||v=="worldforge")value.active_viewport_tab=EditorState::ViewportTab::WorldForge;else value.active_viewport_tab=EditorState::ViewportTab::Scene;value.force_select_viewport_tab=true;value.lock_viewport_tab=true;}editor=std::move(value);set_active_terrain_edits(&editor->terrain_edits);SDL_SetWindowTitle(window,"AI RPG Engine Editor");}
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
    std::optional<StreamedTerrainField> streamed_terrain;
    std::optional<StreamedFoliageField> streamed_foliage;
    std::optional<PlacementCollisionTracker> placement_collision;
    InteractionVolumeRegistry debug_interaction_registry;
    InteractionOverlapTracker debug_interaction_tracker;
    CombatVolumeRegistry debug_combat_registry;
    DebugCamera camera;
    std::optional<OrbitCamera> orbit_camera;
    float mouse_x=0,mouse_y=0;
    bool camera_look_active=false;
    std::optional<EditorTestSessionRestore> editor_test_restore;
    std::unique_ptr<EditorBridgeServer> editor_bridge;
    if(options.debug_world){
        debug_world=std::make_unique<CollisionWorld>();
        streamed_terrain.emplace();
        streamed_foliage.emplace();
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
                auto response = execute_editor_operation(context, request.operation, request.params_json);
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
                auto created = CharacterController::create(*debug_world, spawn_resolution.position,
                    spawn_character.controller_config());
                if (created) {
                    debug_character.emplace(std::move(created.value()));
                    editor->character_asset = spawn_character;
                    editor->test_player_spawn_entity = spawn_resolution.placement_entity;
                    if (spawn_resolution.placement_entity) {
                        editor_test_restore->spawn_entity = spawn_resolution.placement_entity;
                        if (const auto transform = editor->scene.transform(*spawn_resolution.placement_entity))
                            editor_test_restore->spawn_transform = *transform;
                    }
                    orbit_camera.emplace(editor->camera_asset.orbit_config());
                    orbit_camera->set_sensitivity(editor->camera_asset.look_sensitivity);
                    orbit_camera->set_orientation(camera.yaw(), camera.pitch());
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
                    editor->status = "Test session started (max speed " +
                        std::to_string(static_cast<int>(spawn_character.max_speed)) + " m/s)";
                    append_editor_console(*editor,
                        "Test session started: max_speed=" + std::to_string(spawn_character.max_speed) + " m/s",
                        editor->show_movement_console);
                    debug_interaction_tracker.reset("player");
                } else {
                    editor->status = created.error().message;
                    Logger::instance().write(created.error());
                    editor_test_restore.reset();
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
        }
        const auto capture = (!options.capture_path.empty() &&
                              (options.frame_limit == 0 ? frames == 0 : (frames + 1 == options.frame_limit)))
                                 ? options.capture_path
                                 : std::filesystem::path{};
        WorldPosition body_position{0, 1, 0};
        std::array<float, 16> view_projection_matrix = camera.view_projection();
        std::array<float, 3> camera_position = camera.position();
        float movement_yaw = camera.yaw();
        int pixel_w = 1, pixel_h = 1;
        SDL_GetWindowSizeInPixels(window, &pixel_w, &pixel_h);
        if (orbit_camera) {
            const float fov = editor && editor->test_session_active() ? editor->camera_asset.vertical_fov_radians : 1.04719755f;
            const float near_plane = editor && editor->test_session_active() ? editor->camera_asset.near_plane : 0.1f;
            const float far_plane = editor && editor->test_session_active() ? editor->camera_asset.far_plane : 2000.0f;
            (void)orbit_camera->set_perspective(fov, static_cast<float>(pixel_w) / static_cast<float>(std::max(pixel_h, 1)),
                near_plane, far_plane);
        } else
            (void)camera.set_perspective(1.04719755f, static_cast<float>(pixel_w) / static_cast<float>(std::max(pixel_h, 1)),
                0.1f, 2000.0f);
        if (debug_world) {
            const bool* keys = SDL_GetKeyboardState(nullptr);
            const bool editor_mode = editor.has_value();
            const bool test_active = editor_mode && editor->test_session_active();
            const bool test_running = editor_mode && editor->test_session_running();
            const bool game_tab = editor_mode && editor->game_viewport_active();

            if (orbit_camera && debug_character && test_active) {
                if (camera_look_active && game_tab) orbit_camera->apply_look(mouse_x, mouse_y);
                LocalPosition wish{};
                if (test_running && game_tab) {
                    wish.x = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
                    wish.z = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) - (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
                }
                if (test_running && game_tab) {
                    const bool space_down = keys[SDL_SCANCODE_SPACE];
                    const bool space_pressed = space_down && !space_key_was_down;
                    if (space_pressed) (void)debug_character->jump();
                    const auto position_before = debug_character->position();
                    (void)debug_character->move(wish, orbit_camera->yaw(), frame_delta_seconds);
                    if (editor) record_movement_debug(*editor, *debug_character, wish, frame_delta_seconds, position_before);
                }
                body_position = debug_character->position();
                (void)orbit_camera->update(character_feet_pivot(*debug_character), *debug_world);
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
                const WorldPosition probe{body_position.x, body_position.y + 1.2, body_position.z};
                (void)debug_interaction_tracker.update("player", probe, 0.65f, *debug_world, debug_interaction_registry);
                const float yaw = orbit_camera->yaw();
                const WorldPosition attack_probe{body_position.x + std::sin(yaw), body_position.y + 1.2f,
                    body_position.z + std::cos(yaw)};
                (void)query_combat_hits("player_attack", attack_probe, 1.0f, *debug_world, debug_combat_registry);
                view_projection_matrix = orbit_camera->view_projection();
                camera_position = orbit_camera->position();
                movement_yaw = orbit_camera->yaw();
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
                movement_yaw = camera.yaw();
            }
            space_key_was_down = keys[SDL_SCANCODE_SPACE];
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
        if(placement_collision&&debug_world&&editor){const auto synced=placement_collision->sync(*debug_world,editor->scene,editor->prefab_catalog);if(!synced){SDL_DestroyWindow(window);SDL_Quit();return Result<RenderStats>::failure(synced.error());}const bool editor_physics_step=!editor->test_session_active()||editor->test_session_running();if(editor_physics_step)(void)debug_world->step(frame_delta_seconds);if(editor_physics_step)for(const auto& contact_event:debug_world->drain_contact_events()){if(contact_event.type==ContactEventType::Enter&&contact_event.contact_point)editor->recent_contact_points.push_back(*contact_event.contact_point);const bool a_trigger=contact_event.layer_a==CollisionLayer::Trigger;const bool b_trigger=contact_event.layer_b==CollisionLayer::Trigger;if(a_trigger==b_trigger)continue;const auto trigger_body=a_trigger?contact_event.body_a:contact_event.body_b;const auto other_body=a_trigger?contact_event.body_b:contact_event.body_a;const auto binding=placement_collision->interaction_registry().find(trigger_body);if(!binding)continue;InteractionEvent mapped;mapped.type=contact_event.type==ContactEventType::Enter?InteractionEventType::Enter:InteractionEventType::Exit;mapped.placement_entity_id=binding->placement_entity_id;mapped.interaction_id=binding->interaction_id;mapped.volume_index=binding->volume_index;mapped.interactor_id=std::to_string(other_body.value);mapped.contact_point=contact_event.contact_point;editor->recent_interaction_events.push_back(mapped);}if(editor_physics_step)for(const auto& hit_body:placement_collision->combat_registry().bodies_for_role(CombatVolumeRole::Hit)){const auto binding=placement_collision->combat_registry().find(hit_body);if(!binding)continue;for(const auto& contact:query_combat_hits_from_body(binding->combat_id,hit_body,*debug_world,placement_collision->combat_registry()))editor->recent_combat_events.push_back(contact);}if(editor->recent_contact_points.size()>64)editor->recent_contact_points.erase(editor->recent_contact_points.begin(),editor->recent_contact_points.end()-64);if(editor->recent_interaction_events.size()>32)editor->recent_interaction_events.erase(editor->recent_interaction_events.begin(),editor->recent_interaction_events.end()-32);        if(editor->recent_combat_events.size()>32)editor->recent_combat_events.erase(editor->recent_combat_events.begin(),editor->recent_combat_events.end()-32);dispatch_pending_script_events(*editor);}
        if (options.editor) {
            if (editor->active_viewport_tab == EditorState::ViewportTab::WorldForge)
                renderer.ensure_world_forge_placeholder_textures(editor->world_forge_editor);
            draw_editor(*editor, debug_world.get(), camera_look_active, renderer.scene_viewport_texture(),
                renderer.game_viewport_texture(), camera.view_matrix(), camera.projection_matrix(),
                camera.view_projection(), camera.position(), streamed_terrain ? &*streamed_terrain : nullptr,
                streamed_foliage ? &*streamed_foliage : nullptr, &terrain_material,
                debug_character ? &*debug_character : nullptr,
                placement_collision ? &placement_collision->interaction_registry() : nullptr,
                placement_collision ? &placement_collision->combat_registry() : nullptr);
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
        if (editor && editor->test_player_spawn_entity && debug_character && editor->test_session_active()) {
            if (auto transform = editor->scene.transform(*editor->test_player_spawn_entity)) {
                const auto body = debug_character->position();
                transform->position = {static_cast<float>(body.x), static_cast<float>(body.y), static_cast<float>(body.z)};
                (void)editor->scene.set_transform(*editor->test_player_spawn_entity, *transform);
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
                append_character_render_instances(*player_prefab, *debug_character, placed_objects, editor_material_lookup);
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
        scene_pass.draw_physics_body = debug_character.has_value() && !draw_player_visual && (!editor || editor->test_session_active());
        scene_pass.influence = influence_bus.empty() ? nullptr : &influence_bus;
        scene_pass.time_seconds = foliage_time_seconds;
        scene_pass.terrain_pbr = terrain_pbr;
        Renderer::WorldPassParams game_pass = scene_pass;
        game_pass.draw_physics_body = false;
        if (orbit_camera && debug_character && editor && editor->test_session_active()) {
            game_pass.view_projection = orbit_camera->view_projection();
            game_pass.camera_position = orbit_camera->position();
            game_pass.body_position = body_position;
        }
        Renderer::WorldPassParams runtime_pass;
        runtime_pass.view_projection = view_projection_matrix;
        runtime_pass.camera_position = camera_position;
        runtime_pass.body_position = body_position;
        runtime_pass.draw_physics_body = debug_character.has_value() && !draw_player_visual;
        runtime_pass.influence = influence_bus.empty() ? nullptr : &influence_bus;
        runtime_pass.time_seconds = foliage_time_seconds;
        runtime_pass.terrain_pbr = terrain_pbr;
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
