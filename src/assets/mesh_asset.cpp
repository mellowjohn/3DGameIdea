#include "engine/assets/mesh_asset.h"

#include "engine/assets/png_decode.h"

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <variant>
#include <vector>

namespace engine { namespace {
EngineError mesh_error(std::string code,std::string message,std::string remedy="Export a triangle glTF/GLB with finite POSITION data and valid indices."){return {std::move(code),Severity::Error,ErrorCategory::AssetImport,"mesh-import",std::move(message),ENGINE_SOURCE_CONTEXT,{},std::move(remedy),make_correlation_id()};}

constexpr std::array<float, 16> identity_matrix4() {
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

std::array<float, 16> matrix_to_column_major(const fastgltf::math::fmat4x4& matrix) {
    std::array<float, 16> out{};
    for (std::size_t col = 0; col < 4; ++col) {
        for (std::size_t row = 0; row < 4; ++row) {
            out[col * 4 + row] = matrix.col(col)[row];
        }
    }
    return out;
}

Result<ImportedSkin> import_skin(const fastgltf::Asset& asset, const fastgltf::Skin& skin) {
    if (skin.joints.empty()) {
        return Result<ImportedSkin>::failure(
            mesh_error("MESH-SKIN-EMPTY", "glTF skin must list at least one joint node",
                "Export a skin with a non-empty joints array."));
    }
    ImportedSkin output;
    output.name = std::string(skin.name);
    output.joint_node_indices.reserve(skin.joints.size());
    output.joint_names.reserve(skin.joints.size());
    for (const auto joint_index : skin.joints) {
        if (joint_index >= asset.nodes.size()) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-JOINT-RANGE",
                "Skin joint index references a missing node",
                "Ensure every skin.joints entry is a valid node index."));
        }
        output.joint_node_indices.push_back(static_cast<std::uint32_t>(joint_index));
        output.joint_names.emplace_back(asset.nodes[joint_index].name);
    }
    if (skin.skeleton) {
        if (*skin.skeleton >= asset.nodes.size()) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-SKELETON-RANGE",
                "Skin skeleton root references a missing node",
                "Point skin.skeleton at a valid node or omit it."));
        }
        output.skeleton_root = static_cast<std::int32_t>(*skin.skeleton);
    }
    if (skin.inverseBindMatrices) {
        if (*skin.inverseBindMatrices >= asset.accessors.size()) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-IBM-MISSING",
                "Skin inverseBindMatrices accessor is missing",
                "Export inverse-bind matrices or omit the property for identity defaults."));
        }
        const auto& accessor = asset.accessors[*skin.inverseBindMatrices];
        if (accessor.type != fastgltf::AccessorType::Mat4 || accessor.componentType != fastgltf::ComponentType::Float) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-IBM-TYPE",
                "inverseBindMatrices must be FLOAT MAT4",
                "Re-export skins with float 4x4 inverse-bind matrices."));
        }
        if (accessor.count != skin.joints.size()) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-IBM-COUNT",
                "inverseBindMatrices count must match skin.joints length",
                "Export one inverse-bind matrix per joint."));
        }
        output.inverse_bind_matrices.reserve(accessor.count);
        bool nonfinite = false;
        fastgltf::iterateAccessor<fastgltf::math::fmat4x4>(asset, accessor, [&](const auto& value) {
            const auto matrix = matrix_to_column_major(value);
            for (float component : matrix) {
                if (!std::isfinite(component)) nonfinite = true;
            }
            output.inverse_bind_matrices.push_back(matrix);
        });
        if (nonfinite) {
            return Result<ImportedSkin>::failure(mesh_error("MESH-SKIN-IBM-NONFINITE",
                "inverseBindMatrices contain NaN or infinity",
                "Export finite inverse-bind matrices."));
        }
    } else {
        output.inverse_bind_matrices.assign(skin.joints.size(), identity_matrix4());
    }
    return Result<ImportedSkin>::success(std::move(output));
}

Result<std::vector<MeshJointInfluence>> read_vertex_influences(const fastgltf::Asset& asset,
    const fastgltf::Primitive& primitive, std::size_t vertex_count) {
    const auto joints_attr = primitive.findAttribute("JOINTS_0");
    const auto weights_attr = primitive.findAttribute("WEIGHTS_0");
    const bool has_joints = joints_attr != primitive.attributes.end();
    const bool has_weights = weights_attr != primitive.attributes.end();
    if (!has_joints && !has_weights) return Result<std::vector<MeshJointInfluence>>::success({});
    if (has_joints != has_weights) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-ATTR-PAIR",
            "JOINTS_0 and WEIGHTS_0 must both be present when either is authored",
            "Export both JOINTS_0 and WEIGHTS_0, or neither for static meshes."));
    }
    const auto& joints_accessor = asset.accessors[joints_attr->accessorIndex];
    const auto& weights_accessor = asset.accessors[weights_attr->accessorIndex];
    if (joints_accessor.count != vertex_count || weights_accessor.count != vertex_count) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-ATTR-COUNT",
            "JOINTS_0/WEIGHTS_0 counts must match POSITION count",
            "Re-export skinning attributes with one entry per mesh vertex."));
    }
    if (joints_accessor.type != fastgltf::AccessorType::Vec4) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-JOINTS-TYPE",
            "JOINTS_0 must be VEC4", "Export JOINTS_0 as UNSIGNED_BYTE or UNSIGNED_SHORT VEC4."));
    }
    if (joints_accessor.componentType != fastgltf::ComponentType::UnsignedByte
        && joints_accessor.componentType != fastgltf::ComponentType::UnsignedShort) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-JOINTS-TYPE",
            "JOINTS_0 must use UNSIGNED_BYTE or UNSIGNED_SHORT components",
            "Export JOINTS_0 as UNSIGNED_BYTE or UNSIGNED_SHORT VEC4."));
    }
    if (weights_accessor.type != fastgltf::AccessorType::Vec4
        || weights_accessor.componentType != fastgltf::ComponentType::Float) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-WEIGHTS-TYPE",
            "WEIGHTS_0 must be FLOAT VEC4", "Export WEIGHTS_0 as float VEC4."));
    }

    std::vector<fastgltf::math::u16vec4> joint_rows;
    joint_rows.reserve(vertex_count);
    fastgltf::iterateAccessor<fastgltf::math::u16vec4>(asset, joints_accessor, [&](auto value) { joint_rows.push_back(value); });

    std::vector<fastgltf::math::fvec4> weight_rows;
    weight_rows.reserve(vertex_count);
    bool nonfinite = false;
    fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset, weights_accessor, [&](auto value) {
        if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()) || !std::isfinite(value.w())) {
            nonfinite = true;
        }
        weight_rows.push_back(value);
    });
    if (nonfinite) {
        return Result<std::vector<MeshJointInfluence>>::failure(mesh_error("MESH-SKIN-WEIGHTS-NONFINITE",
            "WEIGHTS_0 contain NaN or infinity", "Export finite skinning weights."));
    }

    std::vector<MeshJointInfluence> influences(vertex_count);
    for (std::size_t i = 0; i < vertex_count; ++i) {
        const auto& joints = joint_rows[i];
        const auto& weights = weight_rows[i];
        influences[i].joints = {joints.x(), joints.y(), joints.z(), joints.w()};
        influences[i].weights = {weights.x(), weights.y(), weights.z(), weights.w()};
    }
    return Result<std::vector<MeshJointInfluence>>::success(std::move(influences));
}

Result<void> validate_influence_joint_indices(const ImportedMesh& mesh) {
    if (mesh.influences.empty()) return Result<void>::success();
    if (mesh.skins.empty()) {
        return Result<void>::failure(mesh_error("MESH-SKIN-MISSING",
            "Mesh has JOINTS_0/WEIGHTS_0 but the glTF defines no skins",
            "Add a skins entry that lists the joint nodes for this mesh."));
    }
    std::size_t max_joints = 0;
    for (const auto& skin : mesh.skins) max_joints = std::max(max_joints, skin.joint_node_indices.size());
    for (const auto& influence : mesh.influences) {
        for (std::size_t slot = 0; slot < influence.joints.size(); ++slot) {
            if (influence.weights[slot] == 0.0f) continue;
            if (influence.joints[slot] >= max_joints) {
                return Result<void>::failure(mesh_error("MESH-SKIN-JOINT-INDEX",
                    "Vertex joint index is outside every skin.joints range",
                    "Ensure JOINTS_0 values index into the skin joints array."));
            }
        }
    }
    return Result<void>::success();
}

void expand_bounds(MeshBounds& bounds, float x, float y, float z) {
    if (x < bounds.min_x) bounds.min_x = x;
    if (y < bounds.min_y) bounds.min_y = y;
    if (z < bounds.min_z) bounds.min_z = z;
    if (x > bounds.max_x) bounds.max_x = x;
    if (y > bounds.max_y) bounds.max_y = y;
    if (z > bounds.max_z) bounds.max_z = z;
}

void push_triangle(std::vector<MeshVertex>& vertices, MeshBounds& bounds, MeshVertex a, MeshVertex b, MeshVertex c) {
    vertices.push_back(a);
    vertices.push_back(b);
    vertices.push_back(c);
    expand_bounds(bounds, a.x, a.y, a.z);
    expand_bounds(bounds, b.x, b.y, b.z);
    expand_bounds(bounds, c.x, c.y, c.z);
}

MeshVertex mesh_vertex(float x, float y, float z, float r, float g, float b) { return {x, y, z, r, g, b}; }

void append_ellipsoid(std::vector<MeshVertex>& vertices, MeshBounds& bounds, float cx, float cy, float cz,
    float radius_x, float radius_y, float radius_z, float cr, float cg, float cb) {
    constexpr int segments = 6;
    constexpr int rings = 3;
    constexpr float k_pi = 3.14159265f;
    std::vector<MeshVertex> ring_vertices(static_cast<std::size_t>((rings + 1) * segments));
    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = v * k_pi;
        const float y = std::cos(phi);
        const float ring_radius = std::sin(phi);
        for (int segment = 0; segment < segments; ++segment) {
            const float theta = static_cast<float>(segment) / static_cast<float>(segments) * 2.0f * k_pi;
            ring_vertices[static_cast<std::size_t>(ring * segments + segment)] = mesh_vertex(
                cx + std::cos(theta) * ring_radius * radius_x, cy + y * radius_y,
                cz + std::sin(theta) * ring_radius * radius_z, cr, cg, cb);
        }
    }
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const int next = (segment + 1) % segments;
            const auto& a = ring_vertices[static_cast<std::size_t>(ring * segments + segment)];
            const auto& b_corner = ring_vertices[static_cast<std::size_t>(ring * segments + next)];
            const auto& c = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + next)];
            const auto& d = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + segment)];
            push_triangle(vertices, bounds, a, b_corner, c);
            push_triangle(vertices, bounds, a, c, d);
        }
    }
}

void append_cylinder_stem(std::vector<MeshVertex>& vertices, MeshBounds& bounds, float base_y, float top_y, float radius,
    float cr, float cg, float cb) {
    constexpr int segments = 6;
    constexpr float k_pi = 3.14159265f;
    const auto center_bottom = mesh_vertex(0.0f, base_y, 0.0f, cr, cg, cb);
    const auto center_top = mesh_vertex(0.0f, top_y, 0.0f, cr, cg, cb);
    for (int segment = 0; segment < segments; ++segment) {
        const float angle0 = static_cast<float>(segment) * (2.0f * k_pi / static_cast<float>(segments));
        const float angle1 = static_cast<float>(segment + 1) * (2.0f * k_pi / static_cast<float>(segments));
        const auto bottom0 = mesh_vertex(std::cos(angle0) * radius, base_y, std::sin(angle0) * radius, cr, cg, cb);
        const auto bottom1 = mesh_vertex(std::cos(angle1) * radius, base_y, std::sin(angle1) * radius, cr, cg, cb);
        const auto top0 = mesh_vertex(std::cos(angle0) * radius, top_y, std::sin(angle0) * radius, cr, cg, cb);
        const auto top1 = mesh_vertex(std::cos(angle1) * radius, top_y, std::sin(angle1) * radius, cr, cg, cb);
        push_triangle(vertices, bounds, center_bottom, bottom1, bottom0);
        push_triangle(vertices, bounds, center_top, top0, top1);
        push_triangle(vertices, bounds, bottom0, bottom1, top1);
        push_triangle(vertices, bounds, bottom0, top1, top0);
    }
}

void build_bush_mesh(ImportedMesh& mesh, const std::string& variant, float r, float g, float b) {
    const float stem_r = 0.28f;
    const float stem_g = 0.18f;
    const float stem_b = 0.12f;
    const float accent_r = r * 1.15f;
    const float accent_g = g * 0.85f;
    const float accent_b = b * 1.05f;
    if (variant == "bush_wide") {
        append_cylinder_stem(mesh.vertices, mesh.aabb, 0.02f, 0.28f, 0.08f, stem_r, stem_g, stem_b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.0f, 0.42f, 0.0f, 0.68f, 0.28f, 0.60f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, -0.34f, 0.36f, 0.08f, 0.35f, 0.21f, 0.30f, accent_r, accent_g, accent_b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.30f, 0.34f, -0.10f, 0.31f, 0.20f, 0.29f, r, g, b);
    } else if (variant == "bush_tall") {
        append_cylinder_stem(mesh.vertices, mesh.aabb, 0.02f, 0.38f, 0.07f, stem_r * 0.9f, stem_g * 0.9f, stem_b * 0.9f);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.0f, 0.52f, 0.0f, 0.38f, 0.28f, 0.38f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.04f, 0.78f, -0.03f, 0.31f, 0.24f, 0.31f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, -0.16f, 0.65f, 0.08f, 0.20f, 0.18f, 0.20f, accent_r, accent_g, accent_b);
    } else {
        append_cylinder_stem(mesh.vertices, mesh.aabb, 0.02f, 0.32f, 0.09f, stem_r, stem_g, stem_b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.0f, 0.55f, 0.0f, 0.55f, 0.35f, 0.55f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, -0.28f, 0.48f, 0.10f, 0.33f, 0.25f, 0.33f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.24f, 0.46f, -0.08f, 0.30f, 0.24f, 0.30f, r, g, b);
        append_ellipsoid(mesh.vertices, mesh.aabb, 0.05f, 0.41f, -0.20f, 0.28f, 0.21f, 0.28f, accent_r, accent_g, accent_b);
    }
}

Result<PngImage> decode_png_span(const std::byte* data, std::size_t size) {
    return decode_png_bytes(std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(data), size));
}

/** Resolve and decode an image's pixels regardless of whether it lives inline, in a buffer view, or on disk. */
Result<PngImage> decode_image_source(const fastgltf::Asset& asset, const fastgltf::Image& image,
    const std::filesystem::path& gltf_parent) {
    return std::visit(fastgltf::visitor{
        [&](const fastgltf::sources::Array& array) -> Result<PngImage> {
            return decode_png_span(array.bytes.data(), array.bytes.size_bytes());
        },
        [&](const fastgltf::sources::Vector& vec) -> Result<PngImage> {
            return decode_png_span(vec.bytes.data(), vec.bytes.size());
        },
        [&](const fastgltf::sources::ByteView& view) -> Result<PngImage> {
            return decode_png_span(view.bytes.data(), view.bytes.size_bytes());
        },
        [&](const fastgltf::sources::BufferView& source) -> Result<PngImage> {
            const auto& buffer_view = asset.bufferViews[source.bufferViewIndex];
            const auto& buffer = asset.buffers[buffer_view.bufferIndex];
            return std::visit(fastgltf::visitor{
                [&](const fastgltf::sources::Array& array) -> Result<PngImage> {
                    return decode_png_span(array.bytes.data() + buffer_view.byteOffset, buffer_view.byteLength);
                },
                [&](const fastgltf::sources::Vector& vec) -> Result<PngImage> {
                    return decode_png_span(vec.bytes.data() + buffer_view.byteOffset, buffer_view.byteLength);
                },
                [&](const fastgltf::sources::ByteView& view) -> Result<PngImage> {
                    return decode_png_span(view.bytes.data() + buffer_view.byteOffset, buffer_view.byteLength);
                },
                [&](const auto&) -> Result<PngImage> {
                    return Result<PngImage>::failure(mesh_error("MESH-TEXTURE-SOURCE",
                        "baseColorTexture buffer has no loaded data",
                        "Re-export with embedded or external PNG image data."));
                },
            }, buffer.data);
        },
        [&](const fastgltf::sources::URI& source) -> Result<PngImage> {
            if (source.uri.isDataUri()) {
                return Result<PngImage>::failure(mesh_error("MESH-TEXTURE-DATAURI",
                    "Embedded data-URI image was not decoded by the parser",
                    "Ensure the importer requests LoadExternalImages."));
            }
            return decode_png_file(gltf_parent / std::filesystem::path(source.uri.fspath()));
        },
        [&](const auto&) -> Result<PngImage> {
            return Result<PngImage>::failure(mesh_error("MESH-TEXTURE-SOURCE",
                "Unsupported baseColorTexture image source",
                "Use an embedded or external PNG baseColorTexture."));
        },
    }, image.data);
}

/** Load the first material's PBR base-color texture into engine-owned RGBA pixels, if any. */
Result<void> load_base_color_albedo(const fastgltf::Asset& asset, const fastgltf::Primitive& primitive,
    const std::filesystem::path& gltf_parent, ImportedMesh& output) {
    if (!primitive.materialIndex.has_value() || output.has_albedo()) return Result<void>::success();
    const auto material_index = primitive.materialIndex.value();
    if (material_index >= asset.materials.size()) return Result<void>::success();
    const auto& material = asset.materials[material_index];
    if (!material.pbrData.baseColorTexture.has_value()) return Result<void>::success();
    const auto texture_index = material.pbrData.baseColorTexture.value().textureIndex;
    if (texture_index >= asset.textures.size()) return Result<void>::success();
    const auto& texture = asset.textures[texture_index];
    if (!texture.imageIndex.has_value()) return Result<void>::success();
    const auto image_index = texture.imageIndex.value();
    if (image_index >= asset.images.size()) return Result<void>::success();
    auto decoded = decode_image_source(asset, asset.images[image_index], gltf_parent);
    if (!decoded) return Result<void>::failure(decoded.error());
    output.albedo_rgba = std::move(decoded.value().rgba);
    output.albedo_width = decoded.value().width;
    output.albedo_height = decoded.value().height;
    return Result<void>::success();
}
}
Result<void> ImportedMesh::validate()const{
    if(vertices.empty()||vertices.size()%3!=0)return Result<void>::failure(mesh_error("MESH-TOPOLOGY-INVALID","Expanded mesh must contain a non-empty triangle list"));
    if(vertices.size()>30'000'000)return Result<void>::failure(mesh_error("MESH-VERTEX-LIMIT","Mesh exceeds the 30 million expanded-vertex safety limit"));
    for(const auto& v:vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return Result<void>::failure(mesh_error("MESH-POSITION-NONFINITE","Mesh contains NaN or infinite positions"));
    if(!influences.empty()&&influences.size()!=vertices.size()){
        return Result<void>::failure(mesh_error("MESH-SKIN-INFLUENCE-COUNT","Expanded skinning influences must match expanded vertex count"));
    }
    for(const auto& skin:skins){
        if(skin.joint_node_indices.empty())return Result<void>::failure(mesh_error("MESH-SKIN-EMPTY","Imported skin has no joints"));
        if(skin.joint_names.size()!=skin.joint_node_indices.size()||skin.inverse_bind_matrices.size()!=skin.joint_node_indices.size()){
            return Result<void>::failure(mesh_error("MESH-SKIN-PARALLEL","Skin joint names and inverse-bind matrices must match joint count"));
        }
    }
    return Result<void>::success();
}
Result<ImportedMesh> import_gltf_mesh(const std::filesystem::path& path) {
    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (!data) return Result<ImportedMesh>::failure(mesh_error("MESH-FILE-READ", "Could not read glTF: " + path.generic_string()));
    fastgltf::Parser parser;
    auto parsed = parser.loadGltf(data.get(), path.parent_path(),
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages);
    if (!parsed) {
        auto error = mesh_error("MESH-GLTF-PARSE", "fastgltf rejected " + path.generic_string());
        error.causes.push_back(std::string(fastgltf::getErrorMessage(parsed.error())));
        return Result<ImportedMesh>::failure(std::move(error));
    }
    auto& asset = parsed.get();
    ImportedMesh output;
    for (const auto& skin : asset.skins) {
        auto imported_skin = import_skin(asset, skin);
        if (!imported_skin) return Result<ImportedMesh>::failure(imported_skin.error());
        output.skins.push_back(std::move(imported_skin.value()));
    }
    for (const auto& mesh : asset.meshes) {
        for (const auto& primitive : mesh.primitives) {
            if (primitive.type != fastgltf::PrimitiveType::Triangles)
                return Result<ImportedMesh>::failure(
                    mesh_error("MESH-PRIMITIVE-UNSUPPORTED", "Only triangle primitives are supported in the first importer pass"));
            const auto attribute = primitive.findAttribute("POSITION");
            if (attribute == primitive.attributes.end())
                return Result<ImportedMesh>::failure(mesh_error("MESH-POSITION-MISSING", "Mesh primitive has no POSITION attribute"));
            const auto& accessor = asset.accessors[attribute->accessorIndex];
            std::vector<fastgltf::math::fvec3> positions;
            positions.reserve(accessor.count);
            fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, accessor, [&](auto value) { positions.push_back(value); });
            std::vector<std::array<float, 3>> colors;
            if (const auto color_attribute = primitive.findAttribute("COLOR_0");
                color_attribute != primitive.attributes.end()) {
                const auto& color_accessor = asset.accessors[color_attribute->accessorIndex];
                if (color_accessor.count != positions.size()) {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-COLOR-COUNT",
                        "COLOR_0 count must match POSITION count",
                        "Export COLOR_0 with one color per mesh vertex."));
                }
                colors.resize(positions.size(), {.34f, .22f, .12f});
                bool nonfinite = false;
                if (color_accessor.type == fastgltf::AccessorType::Vec3) {
                    std::size_t i = 0;
                    fastgltf::iterateAccessor<fastgltf::math::fvec3>(asset, color_accessor, [&](auto value) {
                        if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
                            nonfinite = true;
                        colors[i++] = {value.x(), value.y(), value.z()};
                    });
                } else if (color_accessor.type == fastgltf::AccessorType::Vec4) {
                    std::size_t i = 0;
                    fastgltf::iterateAccessor<fastgltf::math::fvec4>(asset, color_accessor, [&](auto value) {
                        if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
                            nonfinite = true;
                        colors[i++] = {value.x(), value.y(), value.z()};
                    });
                } else {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-COLOR-TYPE",
                        "COLOR_0 must be FLOAT VEC3 or VEC4",
                        "Export vertex colors as float COLOR_0."));
                }
                if (nonfinite) {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-COLOR-NONFINITE",
                        "COLOR_0 contain NaN or infinity", "Export finite vertex colors."));
                }
            }
            std::vector<std::array<float, 2>> uvs;
            if (const auto uv_attribute = primitive.findAttribute("TEXCOORD_0");
                uv_attribute != primitive.attributes.end()) {
                const auto& uv_accessor = asset.accessors[uv_attribute->accessorIndex];
                if (uv_accessor.count != positions.size()) {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-UV-COUNT",
                        "TEXCOORD_0 count must match POSITION count",
                        "Export TEXCOORD_0 with one UV per mesh vertex."));
                }
                if (uv_accessor.type != fastgltf::AccessorType::Vec2
                    || uv_accessor.componentType != fastgltf::ComponentType::Float) {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-UV-TYPE",
                        "TEXCOORD_0 must be FLOAT VEC2",
                        "Export texture coordinates as float TEXCOORD_0."));
                }
                uvs.resize(positions.size(), {0.0f, 0.0f});
                bool nonfinite = false;
                std::size_t i = 0;
                fastgltf::iterateAccessor<fastgltf::math::fvec2>(asset, uv_accessor, [&](auto value) {
                    if (!std::isfinite(value.x()) || !std::isfinite(value.y())) nonfinite = true;
                    uvs[i++] = {value.x(), value.y()};
                });
                if (nonfinite) {
                    return Result<ImportedMesh>::failure(mesh_error("MESH-UV-NONFINITE",
                        "TEXCOORD_0 contain NaN or infinity", "Export finite texture coordinates."));
                }
            }
            auto albedo_loaded = load_base_color_albedo(asset, primitive, path.parent_path(), output);
            if (!albedo_loaded) return Result<ImportedMesh>::failure(albedo_loaded.error());
            auto source_influences = read_vertex_influences(asset, primitive, positions.size());
            if (!source_influences) return Result<ImportedMesh>::failure(source_influences.error());
            std::vector<std::uint32_t> indices;
            if (primitive.indicesAccessor) {
                const auto& index_accessor = asset.accessors[*primitive.indicesAccessor];
                indices.reserve(index_accessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(asset, index_accessor, [&](auto value) { indices.push_back(value); });
            } else {
                indices.resize(positions.size());
                for (std::uint32_t i = 0; i < indices.size(); ++i) indices[i] = i;
            }
            if (indices.size() % 3 != 0)
                return Result<ImportedMesh>::failure(
                    mesh_error("MESH-INDEX-COUNT", "Triangle index count must be divisible by three"));
            const bool expand_influences = !source_influences.value().empty();
            if (expand_influences && output.influences.empty() && !output.vertices.empty()) {
                return Result<ImportedMesh>::failure(mesh_error("MESH-SKIN-MIXED",
                    "Cannot mix skinned and unskinned primitives in one imported mesh",
                    "Export separate meshes or add JOINTS_0/WEIGHTS_0 to every primitive."));
            }
            if (!expand_influences && !output.influences.empty()) {
                return Result<ImportedMesh>::failure(mesh_error("MESH-SKIN-MIXED",
                    "Cannot mix skinned and unskinned primitives in one imported mesh",
                    "Export separate meshes or add JOINTS_0/WEIGHTS_0 to every primitive."));
            }
            bool bounds_initialized = !output.vertices.empty();
            for (auto index : indices) {
                if (index >= positions.size())
                    return Result<ImportedMesh>::failure(mesh_error("MESH-INDEX-RANGE", "Mesh index references a missing vertex"));
                const auto& p = positions[index];
                const float x = p.x();
                const float y = p.y();
                const float z = p.z();
                const float r = colors.empty() ? .34f : colors[index][0];
                const float g = colors.empty() ? .22f : colors[index][1];
                const float b = colors.empty() ? .12f : colors[index][2];
                const float u = uvs.empty() ? 0.0f : uvs[index][0];
                const float v = uvs.empty() ? 0.0f : uvs[index][1];
                output.vertices.push_back({x, y, z, r, g, b, u, v});
                if (expand_influences) output.influences.push_back(source_influences.value()[index]);
                if (!bounds_initialized) {
                    output.aabb.min_x = output.aabb.max_x = x;
                    output.aabb.min_y = output.aabb.max_y = y;
                    output.aabb.min_z = output.aabb.max_z = z;
                    bounds_initialized = true;
                } else {
                    if (x < output.aabb.min_x) output.aabb.min_x = x;
                    if (y < output.aabb.min_y) output.aabb.min_y = y;
                    if (z < output.aabb.min_z) output.aabb.min_z = z;
                    if (x > output.aabb.max_x) output.aabb.max_x = x;
                    if (y > output.aabb.max_y) output.aabb.max_y = y;
                    if (z > output.aabb.max_z) output.aabb.max_z = z;
                }
            }
        }
    }
    auto influence_ok = validate_influence_joint_indices(output);
    if (!influence_ok) return Result<ImportedMesh>::failure(influence_ok.error());
    auto valid = output.validate();
    if (!valid) return Result<ImportedMesh>::failure(valid.error());
    return Result<ImportedMesh>::success(std::move(output));
}

Result<ImportedMesh> generate_primitive_mesh(const std::string& primitive_name, const std::array<float, 3>& color) {
    const std::string primitive = [&]() {
        std::string lowered = primitive_name;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
            [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
        return lowered;
    }();
    ImportedMesh mesh;
    mesh.aabb = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest()};
    const float r = color[0];
    const float g = color[1];
    const float b = color[2];
    if (primitive == "cube") {
        const float corners[8][3] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                                     {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
        const int faces[6][6] = {{0, 2, 1, 0, 3, 2}, {4, 5, 6, 4, 6, 7}, {0, 4, 7, 0, 7, 3},
                                 {1, 2, 6, 1, 6, 5}, {3, 7, 6, 3, 6, 2}, {0, 1, 5, 0, 5, 4}};
        for (const auto& face : faces) {
            const auto& a = corners[face[0]];
            const auto& b_corner = corners[face[1]];
            const auto& c = corners[face[2]];
            const auto& d = corners[face[3]];
            const auto& e = corners[face[4]];
            const auto& f_corner = corners[face[5]];
            push_triangle(mesh.vertices, mesh.aabb, mesh_vertex(a[0], a[1], a[2], r, g, b),
                          mesh_vertex(b_corner[0], b_corner[1], b_corner[2], r, g, b),
                          mesh_vertex(c[0], c[1], c[2], r, g, b));
            push_triangle(mesh.vertices, mesh.aabb, mesh_vertex(d[0], d[1], d[2], r, g, b),
                          mesh_vertex(e[0], e[1], e[2], r, g, b),
                          mesh_vertex(f_corner[0], f_corner[1], f_corner[2], r, g, b));
        }
    } else if (primitive == "pyramid") {
        const auto base0 = mesh_vertex(-0.5f, 0.0f, -0.5f, r, g, b);
        const auto base1 = mesh_vertex(0.5f, 0.0f, -0.5f, r, g, b);
        const auto base2 = mesh_vertex(0.5f, 0.0f, 0.5f, r, g, b);
        const auto base3 = mesh_vertex(-0.5f, 0.0f, 0.5f, r, g, b);
        const auto apex = mesh_vertex(0.0f, 1.0f, 0.0f, r, g, b);
        push_triangle(mesh.vertices, mesh.aabb, base0, base2, base1);
        push_triangle(mesh.vertices, mesh.aabb, base0, base3, base2);
        push_triangle(mesh.vertices, mesh.aabb, apex, base0, base1);
        push_triangle(mesh.vertices, mesh.aabb, apex, base1, base2);
        push_triangle(mesh.vertices, mesh.aabb, apex, base2, base3);
        push_triangle(mesh.vertices, mesh.aabb, apex, base3, base0);
    } else if (primitive == "cylinder") {
        constexpr int segments = 8;
        constexpr float k_pi = 3.14159265f;
        const auto center_bottom = mesh_vertex(0.0f, 0.0f, 0.0f, r, g, b);
        const auto center_top = mesh_vertex(0.0f, 1.0f, 0.0f, r, g, b);
        for (int segment = 0; segment < segments; ++segment) {
            const float angle0 = static_cast<float>(segment) * (2.0f * k_pi / static_cast<float>(segments));
            const float angle1 = static_cast<float>(segment + 1) * (2.0f * k_pi / static_cast<float>(segments));
            const auto bottom0 = mesh_vertex(std::cos(angle0) * 0.5f, 0.0f, std::sin(angle0) * 0.5f, r, g, b);
            const auto bottom1 = mesh_vertex(std::cos(angle1) * 0.5f, 0.0f, std::sin(angle1) * 0.5f, r, g, b);
            const auto top0 = mesh_vertex(std::cos(angle0) * 0.5f, 1.0f, std::sin(angle0) * 0.5f, r, g, b);
            const auto top1 = mesh_vertex(std::cos(angle1) * 0.5f, 1.0f, std::sin(angle1) * 0.5f, r, g, b);
            push_triangle(mesh.vertices, mesh.aabb, center_bottom, bottom1, bottom0);
            push_triangle(mesh.vertices, mesh.aabb, center_top, top0, top1);
            push_triangle(mesh.vertices, mesh.aabb, bottom0, bottom1, top1);
            push_triangle(mesh.vertices, mesh.aabb, bottom0, top1, top0);
        }
    } else if (primitive == "sphere") {
        constexpr int segments = 8;
        constexpr int rings = 4;
        constexpr float k_pi = 3.14159265f;
        std::vector<MeshVertex> ring_vertices(static_cast<std::size_t>((rings + 1) * segments));
        for (int ring = 0; ring <= rings; ++ring) {
            const float v = static_cast<float>(ring) / static_cast<float>(rings);
            const float phi = v * k_pi;
            const float y = std::cos(phi) * 0.5f;
            const float ring_radius = std::sin(phi) * 0.5f;
            for (int segment = 0; segment < segments; ++segment) {
                const float u = static_cast<float>(segment) / static_cast<float>(segments);
                const float theta = u * 2.0f * k_pi;
                ring_vertices[static_cast<std::size_t>(ring * segments + segment)] =
                    mesh_vertex(std::cos(theta) * ring_radius, y, std::sin(theta) * ring_radius, r, g, b);
            }
        }
        for (int ring = 0; ring < rings; ++ring) {
            for (int segment = 0; segment < segments; ++segment) {
                const int next = (segment + 1) % segments;
                const auto& a = ring_vertices[static_cast<std::size_t>(ring * segments + segment)];
                const auto& b_corner = ring_vertices[static_cast<std::size_t>(ring * segments + next)];
                const auto& c = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + next)];
                const auto& d = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + segment)];
                push_triangle(mesh.vertices, mesh.aabb, a, b_corner, c);
                push_triangle(mesh.vertices, mesh.aabb, a, c, d);
            }
        }
    } else if (primitive == "capsule") {
        constexpr int segments = 8;
        constexpr int cap_rings = 3;
        constexpr float k_pi = 3.14159265f;
        const float radius = 0.5f;
        const float cylinder_half = 0.5f;
        for (int segment = 0; segment < segments; ++segment) {
            const float angle0 = static_cast<float>(segment) * (2.0f * k_pi / static_cast<float>(segments));
            const float angle1 = static_cast<float>(segment + 1) * (2.0f * k_pi / static_cast<float>(segments));
            const auto bottom0 = mesh_vertex(std::cos(angle0) * radius, -cylinder_half, std::sin(angle0) * radius, r, g, b);
            const auto bottom1 = mesh_vertex(std::cos(angle1) * radius, -cylinder_half, std::sin(angle1) * radius, r, g, b);
            const auto top0 = mesh_vertex(std::cos(angle0) * radius, cylinder_half, std::sin(angle0) * radius, r, g, b);
            const auto top1 = mesh_vertex(std::cos(angle1) * radius, cylinder_half, std::sin(angle1) * radius, r, g, b);
            push_triangle(mesh.vertices, mesh.aabb, bottom0, bottom1, top1);
            push_triangle(mesh.vertices, mesh.aabb, bottom0, top1, top0);
        }
        const auto emit_hemisphere = [&](float center_y, bool upper) {
            std::vector<MeshVertex> ring_vertices(static_cast<std::size_t>((cap_rings + 1) * segments));
            for (int ring = 0; ring <= cap_rings; ++ring) {
                const float v = static_cast<float>(ring) / static_cast<float>(cap_rings);
                const float phi = upper ? v * (k_pi * 0.5f) : k_pi - v * (k_pi * 0.5f);
                const float local_y = std::cos(phi) * radius;
                const float ring_radius = std::sin(phi) * radius;
                for (int segment = 0; segment < segments; ++segment) {
                    const float theta = static_cast<float>(segment) * (2.0f * k_pi / static_cast<float>(segments));
                    ring_vertices[static_cast<std::size_t>(ring * segments + segment)] = mesh_vertex(
                        std::cos(theta) * ring_radius, center_y + local_y, std::sin(theta) * ring_radius, r, g, b);
                }
            }
            for (int ring = 0; ring < cap_rings; ++ring) {
                for (int segment = 0; segment < segments; ++segment) {
                    const int next = (segment + 1) % segments;
                    const auto& a = ring_vertices[static_cast<std::size_t>(ring * segments + segment)];
                    const auto& b_corner = ring_vertices[static_cast<std::size_t>(ring * segments + next)];
                    const auto& c = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + next)];
                    const auto& d = ring_vertices[static_cast<std::size_t>((ring + 1) * segments + segment)];
                    if (upper) {
                        push_triangle(mesh.vertices, mesh.aabb, a, b_corner, c);
                        push_triangle(mesh.vertices, mesh.aabb, a, c, d);
                    } else {
                        push_triangle(mesh.vertices, mesh.aabb, a, c, b_corner);
                        push_triangle(mesh.vertices, mesh.aabb, a, d, c);
                    }
                }
            }
        };
        emit_hemisphere(-cylinder_half, false);
        emit_hemisphere(cylinder_half, true);
    } else if (primitive == "grass_blade") {
        const auto base = mesh_vertex(0.0f, 0.0f, 0.0f, r, g, b);
        const auto tip = mesh_vertex(0.04f, 0.58f, 0.12f, r * 1.05f, g * 1.05f, b * 0.9f);
        const auto side = mesh_vertex(-0.03f, 0.28f, 0.02f, r, g, b);
        push_triangle(mesh.vertices, mesh.aabb, base, side, tip);
        push_triangle(mesh.vertices, mesh.aabb, base, tip, mesh_vertex(-side.x, side.y, -side.z, r, g, b));
    } else if (primitive == "grass_clump") {
        const auto blade = [&](float yaw, float lean) {
            const float cos_yaw = std::cos(yaw);
            const float sin_yaw = std::sin(yaw);
            const auto base = mesh_vertex(0.0f, 0.0f, 0.0f, r, g, b);
            const auto tip = mesh_vertex(cos_yaw * lean, 0.55f, sin_yaw * lean, r * 1.05f, g * 1.05f, b * 0.9f);
            const auto side = mesh_vertex(cos_yaw * lean * 0.35f + sin_yaw * 0.08f, 0.25f, sin_yaw * lean * 0.35f - cos_yaw * 0.08f,
                r, g, b);
            push_triangle(mesh.vertices, mesh.aabb, base, side, tip);
            push_triangle(mesh.vertices, mesh.aabb, base, tip, mesh_vertex(-side.x, side.y, -side.z, r, g, b));
        };
        blade(0.0f, 0.22f);
        blade(2.1f, 0.20f);
        blade(4.2f, 0.24f);
        blade(5.4f, 0.18f);
    } else if (primitive == "flower_clump") {
        const auto stem = mesh_vertex(0.0f, 0.0f, 0.0f, r * 0.7f, g * 0.9f, b * 0.6f);
        const auto stem_top = mesh_vertex(0.0f, 0.42f, 0.0f, r * 0.7f, g * 0.9f, b * 0.6f);
        const auto stem_side = mesh_vertex(0.05f, 0.18f, 0.0f, r * 0.7f, g * 0.9f, b * 0.6f);
        push_triangle(mesh.vertices, mesh.aabb, stem, stem_side, stem_top);
        push_triangle(mesh.vertices, mesh.aabb, stem, stem_top, mesh_vertex(-stem_side.x, stem_side.y, stem_side.z, r * 0.7f, g * 0.9f, b * 0.6f));
        constexpr int petals = 5;
        constexpr float k_pi = 3.14159265f;
        for (int petal = 0; petal < petals; ++petal) {
            const float angle = static_cast<float>(petal) * (2.0f * k_pi / static_cast<float>(petals));
            const float px = std::cos(angle) * 0.12f;
            const float pz = std::sin(angle) * 0.12f;
            const auto center = mesh_vertex(px, 0.45f, pz, r * 1.2f, g * 0.8f, b * 1.1f);
            const auto edge = mesh_vertex(px * 1.5f, 0.50f, pz * 1.5f, r * 1.3f, g * 0.75f, b * 1.15f);
            const auto edge2 = mesh_vertex(px * 1.5f + 0.03f, 0.43f, pz * 1.5f, r * 1.3f, g * 0.75f, b * 1.15f);
            push_triangle(mesh.vertices, mesh.aabb, center, edge, edge2);
            push_triangle(mesh.vertices, mesh.aabb, center, edge2, mesh_vertex(px + 0.02f, 0.47f, pz, r * 1.2f, g * 0.8f, b * 1.1f));
        }
        const auto head = mesh_vertex(0.0f, 0.46f, 0.0f, r * 1.1f, g * 0.85f, b * 1.05f);
        const auto head_edge = mesh_vertex(0.05f, 0.50f, 0.0f, r * 1.1f, g * 0.85f, b * 1.05f);
        const auto head_edge2 = mesh_vertex(0.0f, 0.50f, 0.05f, r * 1.1f, g * 0.85f, b * 1.05f);
        push_triangle(mesh.vertices, mesh.aabb, head, head_edge, head_edge2);
        push_triangle(mesh.vertices, mesh.aabb, head, head_edge2, mesh_vertex(-0.04f, 0.49f, 0.0f, r * 1.1f, g * 0.85f, b * 1.05f));
    } else if (primitive == "bush" || primitive == "bush_wide" || primitive == "bush_tall") {
        build_bush_mesh(mesh, primitive, r, g, b);
    } else {
        return Result<ImportedMesh>::failure(mesh_error("MESH-PRIMITIVE-UNSUPPORTED", "Unsupported primitive: " + primitive));
    }
    auto valid = mesh.validate();
    if (!valid) return Result<ImportedMesh>::failure(valid.error());
    return Result<ImportedMesh>::success(std::move(mesh));
}

Result<ImportedMesh> import_project_mesh(const std::filesystem::path& path) {
    return import_gltf_mesh(path);
}
}
