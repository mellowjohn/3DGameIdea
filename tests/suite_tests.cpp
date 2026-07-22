#include "engine/assets/asset_registry.h"
#include "engine/assets/material_asset.h"
#include "engine/assets/character_asset.h"
#include "engine/assets/camera_asset.h"
#include "engine/assets/rig_asset.h"
#include "engine/assets/play_session_settings.h"
#include "engine/assets/mesh_asset.h"
#include "engine/assets/animation_clip_asset.h"
#include "engine/assets/prefab_asset.h"
#include <map>
#include "engine/automation/command.h"
#include "engine/automation/editor_bridge.h"
#include "engine/automation/editor_session.h"
#include "engine/automation/scene_commands.h"
#include "engine/assets/script_bindings_asset.h"
#include "engine/animation/animator_runtime.h"
#include "engine/animation/root_motion.h"
#include "engine/assets/animator_controller_asset.h"
#include "engine/physics/character_controller.h"
#include "engine/physics/rigidbody_locomotion.h"
#include "engine/scripting/lua_runtime.h"
#include "engine/assets/hud_asset.h"
#include "engine/assets/ui_canvas_asset.h"
#include "engine/assets/ui_canvas_mutate.h"
#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_acts.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_pantheon_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/dialogue/dialogue_runtime.h"
#include "engine/quest/quest_runtime.h"
#include "engine/standing/standing_runtime.h"
#include "engine/dialogue/dialogue_graph_edit.h"
#include "engine/dialogue/twee_import.h"
#include "engine/ui/world_forge_editor.h"
#include "engine/ui/world_forge_graph_camera.h"
#include "engine/ui/cartography_strokes.h"
#include "engine/core/id_slug.h"
#include "engine/automation/world_forge_commands.h"
#include "engine/automation/project_git_commands.h"
#include "engine/ui/ui_canvas_stack.h"
#include "engine/ui/hud_runtime.h"
#include "engine/world/cell_state.h"
#include "engine/world/cell_streamer.h"
#include "engine/world/terrain.h"
#include "engine/world/terrain_edits.h"
#include "engine/world/terrain_paint.h"
#include "engine/world/terrain_field.h"
#include "engine/world/foliage_layers.h"
#include "engine/world/foliage_density.h"
#include "engine/world/foliage_scatter.h"
#include "engine/world/foliage_field.h"
#include "engine/world/world_influence.h"
#include "engine/automation/terrain_edit_commands.h"
#include "engine/automation/water_edit_commands.h"
#include "engine/world/water_store.h"
#include "engine/world/water_field.h"
#include "engine/world/navigation_grid.h"
#include "engine/world/world_partition.h"
#include "engine/world/scene.h"
#include "engine/world/prefab_collision.h"
#include "engine/world/interaction_volumes.h"
#include "engine/world/combat_volumes.h"
#include "engine/rendering/viewport_picking.h"
#include "engine/rendering/pbr_lighting.h"
#include "engine/physics/collision_world.h"
#include "engine/physics/character_controller.h"
#include "engine/rendering/debug_camera.h"
#include "engine/rendering/orbit_camera.h"
#include "engine/diagnostics/crash_bundle.h"
#include "engine/diagnostics/gpu_diagnostics.h"
#include "engine/diagnostics/logger.h"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <unordered_set>
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cmath>

namespace {
struct Runner {
    std::string suite; int assertions=0, failures=0;
    void check(bool value,const char* name){++assertions;if(!value){++failures;std::cerr<<"FAIL ["<<suite<<"] "<<name<<'\n';}}
};
}
int main(int argc,char**argv){
    std::string suite; for(int i=1;i+1<argc;++i)if(std::string(argv[i])=="--suite")suite=argv[i+1];
    Runner r{suite};
    if(suite=="core"){
        auto id=engine::EntityId::parse("bad"); r.check(!id,"malformed UUID rejected");
        engine::EngineError e{"T",engine::Severity::Error,engine::ErrorCategory::Validation,"test","message",std::nullopt,{},"fix","id"};
        r.check(e.to_json().find("\"code\":\"T\"")!=std::string::npos,"error JSON stable");
    }else if(suite=="world"){
        engine::Scene s; auto a=s.create_entity("a"); auto b=s.create_entity("b"); r.check(a&&b,"entities created");
        r.check(s.set_parent(b.value(),a.value()).has_value(),"parent accepted"); r.check(!s.set_parent(a.value(),b.value()),"cycle rejected");
        auto copy=engine::Scene::from_json(s.to_json()); r.check(copy&&copy.value().to_json()==s.to_json(),"scene round trip");
        auto placed_id=engine::EntityId::parse("00000000-0000-4000-8000-000000000099");engine::TransformComponent transform;transform.position={10,0,10};engine::CommandHistory history;
        r.check(placed_id&&history.execute(s,std::make_unique<engine::PlaceWorldObjectCommand>("Tree","assets/prefabs/tree.prefab.json",transform,placed_id.value())),"placed object command applies");r.check(history.last_summary()=="place-world-object"&&history.last_changed_object_ids()==std::vector<std::string>{placed_id.value().str()},"placement reports deterministic changed IDs");r.check(s.placement(placed_id.value())&&s.placement(placed_id.value())->cell==engine::CellCoord{0,0},"placement receives derived cell");
        auto serialized=s.to_json();auto placed_copy=engine::Scene::from_json(serialized);r.check(placed_copy&&placed_copy.value().to_json()==serialized,"placement round trips deterministically");
        auto world_document=engine::Scene::from_json("{\"schemaVersion\":1,\"worldId\":\"world-1\",\"name\":\"Test World\",\"partition\":{\"worldSizeMeters\":[4000,4000],\"cellSizeMeters\":128},\"entities\":[]}");r.check(world_document&&world_document.value().to_json().find("\"worldId\": \"world-1\"")!=std::string::npos&&world_document.value().to_json().find("\"cellSizeMeters\": 128.0")!=std::string::npos,"world metadata preserved for editor save");
        auto moved=transform;moved.position[0]=129;        r.check(history.execute(s,std::make_unique<engine::MoveWorldObjectCommand>(placed_id.value(),moved)).has_value(),"move command applies");r.check(s.placement(placed_id.value())->cell==engine::CellCoord{1,0},"move reassigns cell");r.check(history.undo(s)&&s.placement(placed_id.value())->cell==engine::CellCoord{0,0},"undo restores transform and cell");r.check(history.redo(s)&&s.placement(placed_id.value())->cell==engine::CellCoord{1,0},"redo reapplies transform and cell");
        auto batch_id_a=engine::EntityId::parse("00000000-0000-4000-8000-0000000000b1");auto batch_id_b=engine::EntityId::parse("00000000-0000-4000-8000-0000000000b2");engine::TransformComponent batch_transform;batch_transform.position={20,0,20};std::vector<std::unique_ptr<engine::SceneCommand>> batch_ops;
        batch_ops.push_back(std::make_unique<engine::PlaceWorldObjectCommand>("Batch A","assets/prefabs/tree.prefab.json",batch_transform,batch_id_a.value()));batch_transform.position={22,0,22};
        batch_ops.push_back(std::make_unique<engine::PlaceWorldObjectCommand>("Batch B","assets/prefabs/tree.prefab.json",batch_transform,batch_id_b.value()));
        r.check(batch_id_a&&batch_id_b&&history.execute(s,std::make_unique<engine::CompositeSceneCommand>("scatter-trees",std::move(batch_ops))).has_value(),"composite scene command applies");
        r.check(s.contains(batch_id_a.value())&&s.contains(batch_id_b.value()),"composite command places multiple objects");r.check(history.last_summary()=="scatter-trees","composite command preserves label");
        r.check(history.undo(s)&&!s.contains(batch_id_a.value())&&!s.contains(batch_id_b.value()),"single undo reverts composite batch");
        r.check(history.execute(s,std::make_unique<engine::RemoveWorldObjectCommand>(placed_id.value())).has_value(),"remove command applies");r.check(!s.contains(placed_id.value()),"placed object removed");r.check(history.undo(s)&&s.contains(placed_id.value())&&s.placement(placed_id.value())->cell==engine::CellCoord{1,0},"undo restores removed placement");        r.check(!s.place_world_object("bad","outside.prefab",transform),"invalid prefab path rejected");
        auto player_id=engine::EntityId::parse("00000000-0000-4000-8000-0000000000aa");
        r.check(player_id&&history.execute(s,std::make_unique<engine::PlaceWorldObjectCommand>("Player","assets/prefabs/player.prefab.json",transform,player_id.value(),std::optional<std::string>(std::string{"assets/characters/player.character.json"}))).has_value(),"player spawn placement applies");
        r.check(s.placement(player_id.value())&&s.placement(player_id.value())->character_asset=="assets/characters/player.character.json","player spawn stores character asset");
        auto player_serialized=s.to_json();auto player_copy=engine::Scene::from_json(player_serialized);r.check(player_copy&&player_copy.value().to_json()==player_serialized,"player spawn placement round trips");
        r.check(history.execute(s,std::make_unique<engine::RemoveWorldObjectCommand>(player_id.value())).has_value(),"player spawn remove applies");
        r.check(history.undo(s)&&s.placement(player_id.value())->character_asset=="assets/characters/player.character.json","undo restores player spawn character asset");
        engine::CharacterAsset custom_settings;custom_settings.max_speed=12.0f;
        r.check(s.set_placement_character_settings(player_id.value(),custom_settings).has_value(),"player spawn character settings stored");
        r.check(s.placement(player_id.value())->character_settings->max_speed==12.0f,"player spawn character settings readable");
        auto settings_serialized=s.to_json();auto settings_copy=engine::Scene::from_json(settings_serialized);r.check(settings_copy&&settings_copy.value().to_json()==settings_serialized,"player spawn character settings round trip");
        r.check(s.remap_asset_path_prefix("assets/prefabs","assets/props").has_value(),"asset folder remap applies");
        r.check(s.placement(player_id.value())->prefab_asset=="assets/props/player.prefab.json","prefab path remapped after folder rename");
        r.check(s.placement(player_id.value())->character_asset=="assets/characters/player.character.json","unrelated character asset path preserved");
        engine::ViewportRay ray{{0.0,0.0,-5.0},{0.0,0.0,1.0}};
        engine::WorldBounds box{-1.0f,-1.0f,0.0f,1.0f,1.0f,2.0f};
        r.check(engine::ray_aabb_intersection(ray,box).has_value(),"ray hits mesh bounds");
        r.check(!engine::ray_aabb_intersection(ray,engine::WorldBounds{5,5,5,6,6,6}),"ray misses distant bounds");
        engine::PrefabAsset tree_prefab;tree_prefab.schema_version=2;engine::PrefabPart trunk;trunk.name="Trunk";trunk.mesh.primitive="cylinder";trunk.mesh.color={0.3f,0.2f,0.1f};
        engine::PrefabPart canopy;canopy.name="Canopy";canopy.transform.position={0.0f,1.6f,0.0f};canopy.mesh.primitive="sphere";canopy.mesh.color={0.1f,0.2f,0.1f};
        tree_prefab.parts={trunk,canopy};
        std::map<std::string,engine::MeshBounds> mesh_bounds;
        mesh_bounds[tree_prefab.mesh_key_for_part(trunk)]={-0.5f,0.0f,-0.5f,0.5f,1.0f,0.5f};
        mesh_bounds[tree_prefab.mesh_key_for_part(canopy)]={-0.6f,-0.6f,-0.6f,0.6f,0.6f,0.6f};
        engine::TransformComponent tree_transform;tree_transform.position={0.0f,0.0f,0.0f};
        const auto part_bounds=engine::placement_mesh_bounds(tree_prefab,tree_transform,mesh_bounds);
        r.check(part_bounds.size()==2,"compositional prefab exposes per-part pick bounds");
        engine::PrefabAsset component_prefab;component_prefab.schema_version=2;
        engine::PrefabCollisionVolume seed_box;seed_box.id="collision-0";seed_box.shape=engine::PrefabCollisionShape::Box;seed_box.half_extent={0.5f,0.5f,0.5f};
        component_prefab.collision={seed_box};
        engine::PrefabScriptBinding seed_script;seed_script.id="script-0";seed_script.kind="handler";seed_script.binding_id="demo";
        component_prefab.script_bindings={seed_script};
        auto component_entity=engine::EntityId::parse("00000000-0000-4000-8000-0000000000c1");
        engine::TransformComponent component_transform;component_transform.position={50,0,50};
        r.check(component_entity&&history.execute(s,std::make_unique<engine::PlaceWorldObjectCommand>("CompObj","assets/prefabs/tree.prefab.json",component_transform,component_entity.value(),std::nullopt,component_prefab)).has_value(),"place seeds authored components");
        r.check(s.authored_components(component_entity.value())&&s.authored_components(component_entity.value())->entries.size()==2,"seeded collider and script components");
        engine::AuthoredComponentEntry override_entry=s.authored_components(component_entity.value())->entries.front();
        override_entry.collider.half_extent={1.0f,1.0f,1.0f};
        override_entry.collider.transform.position={0.25f,0.5f,0.0f};
        override_entry.collider.shape=engine::PrefabCollisionShape::Sphere;
        override_entry.collider.radius=0.75f;
        r.check(history.execute(s,std::make_unique<engine::SetEntityComponentCommand>(component_entity.value(),override_entry)).has_value(),"set component marks override");
        r.check(s.authored_components(component_entity.value())->entries.front().overridden,"component override flag set");
        r.check(s.authored_components(component_entity.value())->entries.front().collider.shape==engine::PrefabCollisionShape::Sphere,"set component updates collider shape");
        r.check(s.authored_components(component_entity.value())->entries.front().collider.radius==0.75f,"set component updates sphere radius");
        r.check(s.authored_components(component_entity.value())->entries.front().collider.transform.position[1]==0.5f,"set component updates collider offset");
        r.check(history.undo(s)&&s.authored_components(component_entity.value())->entries.front().collider.shape==engine::PrefabCollisionShape::Box,"undo restores collider fields");
        r.check(history.redo(s)&&s.authored_components(component_entity.value())->entries.front().collider.radius==0.75f,"redo reapplies collider fields");
        engine::AuthoredComponentEntry script_entry=s.authored_components(component_entity.value())->entries.back();
        script_entry.script.kind="interaction";
        script_entry.script.binding_id="use_campfire";
        r.check(history.execute(s,std::make_unique<engine::SetEntityComponentCommand>(component_entity.value(),script_entry)).has_value(),"set script binding fields");
        r.check(s.authored_components(component_entity.value())->entries.back().script.binding_id=="use_campfire","script binding id updated");
        component_prefab.collision[0].half_extent={2.0f,2.0f,2.0f};
        r.check(s.propagate_prefab_components("assets/prefabs/tree.prefab.json",component_prefab)>=1,"prefab propagate updates non-overridden entries");
        r.check(s.authored_components(component_entity.value())->entries.front().collider.half_extent.x==1.0f,"overridden collider not replaced by propagate");
        auto component_json=s.to_json();auto component_round=engine::Scene::from_json(component_json);
        r.check(component_round&&component_round.value().authored_components(component_entity.value())&&component_round.value().authored_components(component_entity.value())->entries.size()==2,"entity components round trip in world JSON");
        engine::Scene legacy;engine::TransformComponent legacy_transform;legacy_transform.position={60,0,60};
        auto legacy_id=engine::EntityId::parse("00000000-0000-4000-8000-0000000000c2");
        r.check(legacy_id&&legacy.place_world_object("Legacy","assets/prefabs/tree.prefab.json",legacy_transform,legacy_id.value()).has_value(),"legacy place without seed");
        r.check(!legacy.authored_components(legacy_id.value()),"legacy placement has no components until seeded");
        std::map<std::string,engine::PrefabAsset> legacy_catalog;legacy_catalog["assets/prefabs/tree.prefab.json"]=component_prefab;
        r.check(legacy.seed_missing_authored_components(legacy_catalog)==1,"seed_missing exposes prefab colliders as components");
        r.check(legacy.authored_components(legacy_id.value())&&legacy.authored_components(legacy_id.value())->entries.size()==2,"seeded collider and script visible on entity");
        r.check(legacy.seed_missing_authored_components(legacy_catalog)==0,"seed_missing is idempotent");
    }else if(suite=="assets"){
        auto root=std::filesystem::temp_directory_path()/"engine-suite-assets"; std::filesystem::create_directories(root/"assets");
        std::ofstream(root/"assets/a.txt")<<"a"; engine::AssetRegistry assets; r.check(assets.scan(root).has_value(),"scan succeeds");
        r.check(assets.records().size()==1,"one asset found"); r.check(assets.validate().empty(),"dependencies valid");
        engine::MaterialAsset material;material.base_color={0.2f,0.3f,0.4f,1.0f};material.roughness=0.8f;material.physics.surface="stone";r.check(material.validate().has_value(),"material valid");auto parsed=engine::MaterialAsset::from_json(material.to_json());r.check(parsed&&parsed.value().to_json()==material.to_json(),"material deterministic round trip");
        material.metallic=1.5f;r.check(!material.validate(),"invalid metallic rejected");material.metallic=0;material.base_color[3]=0.5f;r.check(!material.validate(),"opaque alpha mismatch rejected");r.check(!engine::MaterialAsset::from_json("{}"),"malformed material rejected");
        const auto gltf=root/"assets/sample.gltf";std::ofstream(gltf)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":96,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";auto mesh=engine::import_gltf_mesh(gltf);r.check(mesh&&mesh.value().vertices.size()==18,"glTF triangle mesh imported");r.check(mesh&&!mesh.value().has_skinning(),"static glTF has no skinning");
        const auto colored=root/"assets/colored.gltf";std::ofstream(colored)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":144,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":48},{"buffer":0,"byteOffset":108,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2}]}]})";
        auto bad_color=engine::import_gltf_mesh(colored);r.check(!bad_color&&bad_color.error().code=="MESH-COLOR-COUNT","COLOR_0 count mismatch rejected");
        std::ofstream(colored,std::ios::trunc)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":156,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AACAPwAAgD8AAIA/AAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":60},{"buffer":0,"byteOffset":120,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"COLOR_0":1},"indices":2}]}]})";
        auto color_mesh=engine::import_gltf_mesh(colored);r.check(color_mesh&&color_mesh.value().vertices.size()==18,"COLOR_0 glTF imported");r.check(color_mesh&&color_mesh.value().vertices.front().r>0.9f&&color_mesh.value().vertices.front().g>0.9f&&color_mesh.value().vertices.front().b>0.9f,"COLOR_0 applied to vertices");
        const auto textured=root/"assets/textured.gltf";std::ofstream(textured)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAABAAIA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"images":[{"uri":"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAFElEQVR4nGP4z8DwHwRZGME0AwMAPv8GAPpcE7AAAAAASUVORK5CYII="}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2,"material":0}]}]})";
        auto tex_mesh=engine::import_gltf_mesh(textured);
        r.check(tex_mesh&&tex_mesh.value().vertices.size()==3,"TEXCOORD glTF imported");
        r.check(tex_mesh&&std::abs(tex_mesh.value().vertices[1].u-1.0f)<1e-5f&&std::abs(tex_mesh.value().vertices[2].v-1.0f)<1e-5f,"TEXCOORD_0 applied to vertex UVs");
        r.check(tex_mesh&&tex_mesh.value().has_albedo()&&tex_mesh.value().albedo_width==2&&tex_mesh.value().albedo_height==2,"baseColorTexture decoded to 2x2 RGBA albedo");
        r.check(tex_mesh&&tex_mesh.value().albedo_rgba.size()==16&&tex_mesh.value().albedo_rgba[0]==255&&tex_mesh.value().albedo_rgba[1]==0&&tex_mesh.value().albedo_rgba[2]==0,"albedo top-left texel is red");
        const auto uv_mismatch=root/"assets/uv-mismatch.gltf";std::ofstream(uv_mismatch)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":50,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAEAAgA="}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},{"buffer":0,"byteOffset":44,"byteLength":6}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5126,"count":1,"type":"VEC2"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":1},"indices":2}]}]})";
        auto uv_bad=engine::import_gltf_mesh(uv_mismatch);r.check(!uv_bad&&uv_bad.error().code=="MESH-UV-COUNT","TEXCOORD_0 count mismatch rejected");
        std::ofstream(gltf,std::ios::trunc)<<"{}";r.check(!engine::import_gltf_mesh(gltf),"malformed glTF rejected");
        const auto skinned=root/"assets/skinned.gltf";
        std::ofstream(skinned)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Root","children":[1,2]},{"name":"Hip"},{"name":"Spine","translation":[0,1,0]},{"name":"Mesh","mesh":0,"skin":0}],
"skins":[{"name":"Body","joints":[1,2],"inverseBindMatrices":2,"skeleton":1}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":3,"WEIGHTS_0":4},"indices":1}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5126,"count":2,"type":"MAT4"},
{"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
{"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":36},
{"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":44,"byteLength":128},
{"buffer":0,"byteOffset":172,"byteLength":24},
{"buffer":0,"byteOffset":196,"byteLength":48}
],
"buffers":[{"byteLength":244,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAQAAAAAAAAABAAAAAAABAAAAAAAAAAAAQD8AAIA+AAAAAAAAAAAAAAA/AAAAPwAAAAAAAAAAzczMPWZmZj8AAAAAAAAAAA=="}]
})";
        auto skinned_mesh=engine::import_gltf_mesh(skinned);
        r.check(skinned_mesh&&skinned_mesh.value().vertices.size()==3,"skinned glTF triangle imported");
        r.check(skinned_mesh&&skinned_mesh.value().skins.size()==1&&skinned_mesh.value().skins[0].joint_node_indices.size()==2,"skinned glTF skin joints imported");
        r.check(skinned_mesh&&skinned_mesh.value().skins[0].joint_names[0]=="Hip"&&skinned_mesh.value().skins[0].skeleton_root==1,"skinned glTF joint names and skeleton root");
        r.check(skinned_mesh&&skinned_mesh.value().influences.size()==3&&skinned_mesh.value().influences[0].joints[0]==0&&skinned_mesh.value().influences[0].weights[0]==0.75f,"skinned glTF vertex influences imported");
        const auto joints_only=root/"assets/joints-only.gltf";
        std::ofstream(joints_only)<<R"({"asset":{"version":"2.0"},"nodes":[{"name":"J0"},{"name":"J1"},{"mesh":0,"skin":0}],"skins":[{"joints":[0,1]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":1},"indices":2}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":3,"type":"VEC4"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":24},{"buffer":0,"byteOffset":60,"byteLength":6}],"buffers":[{"byteLength":66,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAAAAAAAAAEAAAAAAAEAAAAAAAAAAAABAAIA"}]})";
        auto joints_only_mesh=engine::import_gltf_mesh(joints_only);
        r.check(!joints_only_mesh&&joints_only_mesh.error().code=="MESH-SKIN-ATTR-PAIR","JOINTS_0 without WEIGHTS_0 rejected");
        const auto bad_joint=root/"assets/bad-joint.gltf";
        std::ofstream(bad_joint)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"J0"},{"name":"J1"},{"mesh":0,"skin":0}],
"skins":[{"joints":[0,1],"inverseBindMatrices":2}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"JOINTS_0":3,"WEIGHTS_0":4},"indices":1}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
{"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
{"bufferView":2,"componentType":5126,"count":2,"type":"MAT4"},
{"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
{"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":36},
{"buffer":0,"byteOffset":36,"byteLength":6},
{"buffer":0,"byteOffset":44,"byteLength":128},
{"buffer":0,"byteOffset":172,"byteLength":24},
{"buffer":0,"byteOffset":196,"byteLength":48}
],
"buffers":[{"byteLength":244,"uri":"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwgAAAAAAAAAAAABAAAAAAABAAAAAAAAAAAAQD8AAIA+AAAAAAAAAAAAAAA/AAAAPwAAAAAAAAAAzczMPWZmZj8AAAAAAAAAAA=="}]
})";
        auto bad_joint_mesh=engine::import_gltf_mesh(bad_joint);
        r.check(!bad_joint_mesh&&bad_joint_mesh.error().code=="MESH-SKIN-JOINT-INDEX","out-of-range skin joint index rejected");
        const auto bad_ibm=root/"assets/bad-ibm.gltf";
        std::ofstream(bad_ibm)<<R"({"asset":{"version":"2.0"},"nodes":[{"name":"J0"},{"name":"J1"}],"skins":[{"joints":[0,1],"inverseBindMatrices":0}],"meshes":[{"primitives":[{"attributes":{"POSITION":1},"indices":2}]}],"accessors":[{"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},{"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":64},{"buffer":0,"byteOffset":64,"byteLength":36},{"buffer":0,"byteOffset":100,"byteLength":6}],"buffers":[{"byteLength":106,"uri":"data:application/octet-stream;base64,AACAPwAAAAAAAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAQACAA=="}]})";
        auto bad_ibm_mesh=engine::import_gltf_mesh(bad_ibm);
        r.check(!bad_ibm_mesh&&bad_ibm_mesh.error().code=="MESH-SKIN-IBM-COUNT","inverseBindMatrices count mismatch rejected");

        const auto animated=root/"assets/animated.gltf";
        std::ofstream(animated)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"},{"name":"Spine"}],
"animations":[{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":8},
{"buffer":0,"byteOffset":8,"byteLength":24}
],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
        auto anim_set=engine::import_gltf_animation_clips(animated);
        r.check(anim_set&&anim_set.value().clips.size()==1&&anim_set.value().clips[0].name=="Idle","glTF animation clip imported");
        r.check(anim_set&&anim_set.value().clips[0].duration_seconds==1.0f&&anim_set.value().clips[0].channels.size()==1,"glTF clip duration and channel count");
        r.check(anim_set&&anim_set.value().clips[0].channels[0].target_node_name=="Hip"&&anim_set.value().clips[0].channels[0].times.size()==2,"glTF clip targets Hip with two keys");
        auto mid=engine::sample_translation_channel(anim_set.value().clips[0].channels[0],0.5f);
        r.check(mid&&std::abs(mid.value()[1]-0.5f)<1e-5f,"glTF clip translation samples with linear lerp");
        auto empty_anim=engine::import_gltf_animation_clips(skinned);
        r.check(empty_anim&&empty_anim.value().empty(),"skinned glTF without animations imports empty set");
        const auto bad_anim_node=root/"assets/bad-anim-node.gltf";
        std::ofstream(bad_anim_node)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"samplers":[{"input":0,"output":1}],"channels":[{"sampler":0,"target":{"node":9,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
        auto bad_anim=engine::import_gltf_animation_clips(bad_anim_node);
        r.check(!bad_anim&&bad_anim.error().code=="ANIM-CLIP-TARGET-RANGE","out-of-range animation target rejected");
        const auto cubic=root/"assets/cubic-anim.gltf";
        std::ofstream(cubic)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"samplers":[{"input":0,"output":1,"interpolation":"CUBICSPLINE"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR"},
{"bufferView":1,"componentType":5126,"count":6,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":72}],
"buffers":[{"byteLength":80,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"}]
})";
        auto cubic_anim=engine::import_gltf_animation_clips(cubic);
        r.check(!cubic_anim&&cubic_anim.error().code=="ANIM-CLIP-INTERP-UNSUPPORTED","CUBICSPLINE animation rejected");
        engine::AnimationClipLibrary clip_library;
        auto loaded=clip_library.load(animated);
        r.check(loaded&&loaded.value()->clips.size()==1&&loaded.value()->clips[0].duration_seconds==1.0f,"animation clip library load");
        // Hot reload: rewrite with duration 2.0 (times 0 and 2)
        std::ofstream(animated,std::ios::trunc)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"},{"name":"Spine"}],
"animations":[{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[2.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[
{"buffer":0,"byteOffset":0,"byteLength":8},
{"buffer":0,"byteOffset":8,"byteLength":24}
],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
        // Ensure write time advances on Windows
        {
            auto now=std::filesystem::file_time_type::clock::now();
            std::filesystem::last_write_time(animated, now+std::chrono::seconds(2));
        }
        auto changed=clip_library.poll_changed();
        r.check(changed.size()==1,"animation clip library detects write-time change");
        auto reloaded=clip_library.reload(animated);
        r.check(reloaded&&reloaded.value()->clips[0].duration_seconds==2.0f,"animation clip hot reload applies new duration");
        std::ofstream(animated,std::ios::trunc)<<"{}";
        std::filesystem::last_write_time(animated, std::filesystem::file_time_type::clock::now()+std::chrono::seconds(4));
        auto failed_reload=clip_library.reload(animated);
        auto kept=clip_library.get(animated);
        r.check(!failed_reload&&kept&&kept.value()->clips[0].duration_seconds==2.0f,"failed animation reload keeps previous clips");

        const auto prefab=root/"assets/prefabs/campfire.prefab.json";std::filesystem::create_directories(prefab.parent_path());std::ofstream(prefab)<<R"({"schemaVersion":1,"mesh":"assets/models/campfire.gltf","light":{"color":[1.0,0.62,0.28],"radius":20.0,"strength":1.35,"offset":[0.0,0.35,0.0]},"entities":[]})";auto campfire=engine::PrefabAsset::load(prefab);r.check(campfire&&campfire.value().light.has_value()&&campfire.value().light->radius==20.0f,"prefab point light parsed");
        const auto compositional=root/"assets/prefabs/tree.prefab.json";std::ofstream(compositional)<<R"({"schemaVersion":2,"entities":[{"name":"Trunk","transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,2,1]},"parent":null,"mesh":{"primitive":"cylinder","color":[0.3,0.2,0.1]}},{"name":"Canopy","transform":{"position":[0,1.6,0],"rotation":[0,0,0,1],"scale":[1.2,0.8,1.2]},"parent":null,"mesh":{"primitive":"sphere","color":[0.1,0.2,0.1]}}]})";
        auto tree_prefab=engine::PrefabAsset::load(compositional);r.check(tree_prefab&&tree_prefab.value().is_compositional()&&tree_prefab.value().parts.size()==2,"compositional prefab parts parsed");
        auto cube=engine::generate_primitive_mesh("cube",{0.2f,0.3f,0.4f});auto cylinder=engine::generate_primitive_mesh("cylinder",{0.2f,0.3f,0.4f});r.check(cube&&cylinder&&cube.value().vertices.size()!=cylinder.value().vertices.size(),"primitive meshes differ");
        auto capsule=engine::generate_primitive_mesh("capsule",{0.35f,0.55f,0.95f});r.check(capsule&&capsule.value().vertices.size()>cylinder.value().vertices.size(),"capsule primitive generated");
        const auto player_prefab=root/"assets/prefabs/player.prefab.json";std::filesystem::create_directories(player_prefab.parent_path());std::ofstream(player_prefab)<<R"({"schemaVersion":2,"characterAsset":"assets/characters/player.character.json","entities":[{"name":"Body","transform":{"position":[0,0,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"parent":null,"mesh":{"primitive":"capsule","color":[0.35,0.55,0.95]}}]})";
        auto player_asset=engine::PrefabAsset::load(player_prefab);r.check(player_asset&&player_asset.value().parts.size()==1&&player_asset.value().parts[0].mesh.primitive=="capsule"&&player_asset.value().character_asset=="assets/characters/player.character.json","player capsule prefab parsed");
        const auto character_asset=root/"assets/characters/player.character.json";std::filesystem::create_directories(character_asset.parent_path());std::ofstream(character_asset)<<R"({"schemaVersion":1,"visualPrefab":"assets/prefabs/player.prefab.json","capsuleRadius":0.35,"capsuleHalfHeight":0.85,"maxSlopeRatio":0.45,"stepHeight":0.35,"maxSpeed":6.0,"gravity":9.81,"jumpVelocity":5.0})";
        auto character=engine::CharacterAsset::load(character_asset);r.check(character&&character.value().controller_config().max_speed==6.0f&&character.value().controller_config().jump_velocity==5.0f,"character asset parsed");
        engine::RigAsset rig_sample;rig_sample.id="fixture";
        engine::RigIkHook hand;hand.id="left_hand";hand.tip_joint="LeftHand";hand.root_joint="LeftUpperArm";hand.chain_length=3;
        engine::RigBoneRole hips;hips.role="hips";hips.joint_name="Hips";
        rig_sample.ik_hooks={hand};rig_sample.bone_roles={hips};
        r.check(rig_sample.validate().has_value(),"rig metadata validates");
        const auto rig_path=root/"assets/characters/fixture.rig.json";std::filesystem::create_directories(rig_path.parent_path());
        r.check(rig_sample.save(rig_path).has_value(),"rig asset save");
        auto loaded_rig=engine::RigAsset::load(rig_path);
        r.check(loaded_rig&&loaded_rig.value().ik_hooks.size()==1&&loaded_rig.value().bone_roles[0].role=="hips","rig asset round trip");
        r.check(loaded_rig&&loaded_rig.value().validate_against_joint_names({"Hips","LeftHand","LeftUpperArm"}).has_value(),
            "rig joints match skin names");
        r.check(loaded_rig&&!loaded_rig.value().validate_against_joint_names({"Hips"}).has_value(),"rig rejects unknown tip joint");
        engine::RigIkHook dup=hand;rig_sample.ik_hooks.push_back(dup);
        r.check(!rig_sample.validate()&&rig_sample.validate().error().code=="RIG-IK-ID-DUPLICATE","duplicate ik hook id rejected");
        const auto player_rig=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/"samples/open-world-rpg/assets/characters/player.rig.json";
        auto sample_rig=engine::RigAsset::load(player_rig);
        r.check(sample_rig&&sample_rig.value().ik_hooks.size()==4&&sample_rig.value().bone_roles.size()>=10,"sample player.rig.json loads");
        auto char_with_rig=engine::CharacterAsset::from_json(R"({"schemaVersion":1,"visualPrefab":"assets/prefabs/player.prefab.json","rig":"assets/characters/player.rig.json","capsuleRadius":0.35,"capsuleHalfHeight":0.85,"maxSlopeRatio":0.45,"stepHeight":0.35,"maxSpeed":6,"gravity":9.81,"jumpVelocity":5})");
        r.check(char_with_rig&&char_with_rig.value().rig=="assets/characters/player.rig.json","character optional rig path parsed");
        const auto camera_asset=root/"assets/cameras/game.camera.json";std::filesystem::create_directories(camera_asset.parent_path());std::ofstream(camera_asset)<<R"({"schemaVersion":1,"pivotHeight":1.6,"minDistance":1.5,"maxDistance":8.0,"defaultDistance":5.0,"collisionProbeRadius":0.2,"collisionPadding":0.15,"lookSensitivity":0.0025,"verticalFovRadians":1.04719755,"nearPlane":0.1,"farPlane":2000.0})";
        auto camera=engine::CameraAsset::load(camera_asset);r.check(camera&&camera.value().orbit_config().default_distance==5.0f,"camera asset parsed");
        engine::PlaySessionSettings session;session.character_asset="assets/characters/player.character.json";session.camera_asset="assets/cameras/game.camera.json";
        const auto session_path=root/"play.session.json";r.check(session.save(session_path).has_value(),"play session settings saved");
        auto loaded_session=engine::PlaySessionSettings::load(session_path);r.check(loaded_session&&loaded_session.value().character_asset==session.character_asset,"play session settings round trip");
        std::map<std::string,engine::MeshBounds> bounds;for(const auto& part:tree_prefab.value().parts){auto generated=engine::generate_primitive_mesh(*part.mesh.primitive,part.mesh.color);r.check(generated.has_value(),"tree part primitive generated");bounds[tree_prefab.value().mesh_key_for_part(part)]=generated.value().aabb;}        r.check(tree_prefab&&tree_prefab.value().bounds(bounds).max_y>1.0f,"compositional prefab bounds union");
        const auto trigger_prefab=root/"assets/prefabs/trigger.prefab.json";std::ofstream(trigger_prefab)<<R"({"schemaVersion":2,"entities":[],"collision":[{"shape":"sphere","layer":"trigger","trigger":true,"transform":{"position":[0,0.5,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"radius":1.0}]})";
        auto trigger_asset=engine::PrefabAsset::load(trigger_prefab);r.check(trigger_asset&&trigger_asset.value().collision.size()==1&&trigger_asset.value().collision[0].trigger,"prefab collision volume parsed");
        const auto interact_prefab=root/"assets/prefabs/interact.prefab.json";std::ofstream(interact_prefab)<<R"({"schemaVersion":2,"entities":[],"collision":[{"shape":"sphere","interaction":"open_door","transform":{"position":[0,1,0],"rotation":[0,0,0,1],"scale":[1,1,1]},"radius":1.0}]})";
        auto interact_asset=engine::PrefabAsset::load(interact_prefab);r.check(interact_asset&&interact_asset.value().collision[0].is_interaction()&&interact_asset.value().collision[0].trigger,"prefab interaction volume parsed");
        std::map<std::string,engine::PrefabAsset> catalog;catalog["assets/prefabs/scene assets/tree.prefab.json"]=tree_prefab.value();r.check(engine::resolve_prefab_catalog_path(catalog,"assets/prefabs/tree.prefab.json")=="assets/prefabs/scene assets/tree.prefab.json","prefab path resolves by unique filename");
        engine::PrefabMeshSource mesh_with_material;mesh_with_material.primitive="cube";mesh_with_material.material="assets/materials/test.material.json";
        engine::MaterialAsset test_material;test_material.base_color={0.2f,0.4f,0.6f,1.0f};
        std::map<std::string,engine::MaterialAsset> materials{{"assets/materials/test.material.json",test_material}};
        const auto lookup=engine::make_material_lookup(&materials);
        const auto resolved_color=engine::resolved_prefab_mesh_color(mesh_with_material,lookup);
        r.check(resolved_color[0]==0.2f&&resolved_color[1]==0.4f&&resolved_color[2]==0.6f,"prefab material resolves render color");
        // Live catalog mesh ensure: newly referenced glTF must load without restart (MCP authoring).
        {
            const auto live_mesh=root/"assets/models/live_prop.gltf";
            std::filesystem::create_directories(live_mesh.parent_path());
            std::ofstream(live_mesh)<<R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":96,"uri":"data:application/octet-stream;base64,AAAAvwAAAAAAAAC/AAAAPwAAAAAAAAC/AAAAPwAAAAAAAAA/AAAAvwAAAAAAAAA/AAAAAAAAAEAAAAAAAAABAAQAAQACAAQAAgADAAQAAwAAAAQAAAADAAIAAAACAAEA"}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":60},{"buffer":0,"byteOffset":60,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":5,"type":"VEC3"},{"bufferView":1,"componentType":5123,"count":18,"type":"SCALAR"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1}]}]})";
            engine::PrefabAsset live_prefab;live_prefab.schema_version=2;
            engine::PrefabPart live_part;live_part.name="Body";live_part.mesh.asset="assets/models/live_prop.gltf";
            live_prefab.parts={live_part};
            std::map<std::string,engine::PrefabAsset> live_catalog{{"assets/prefabs/live_prop.prefab.json",live_prefab}};
            std::map<std::string,engine::MeshBounds> live_bounds;
            std::vector<std::pair<std::string,engine::ImportedMesh>> live_meshes;
            engine::ensure_prefab_catalog_meshes(root,live_catalog,{},live_bounds,live_meshes);
            r.check(live_meshes.size()==1&&live_meshes[0].second.vertices.size()==18,"catalog ensure imports missing glTF mesh");
            r.check(live_bounds.count("assets/models/live_prop.gltf")==1,"catalog ensure records mesh bounds");
            const auto before_count=live_meshes[0].second.vertices.size();
            std::set<std::string> reload{"assets/models/live_prop.gltf"};
            engine::ensure_prefab_catalog_meshes(root,live_catalog,{},live_bounds,live_meshes,&reload);
            r.check(reload.empty()&&live_meshes.size()==1&&live_meshes[0].second.vertices.size()==before_count,
                "catalog ensure reloads invalidated glTF mesh keys");
        }
        test_material.roughness=0.25f;test_material.metallic=0.8f;test_material.emissive={2.0f,0.5f,0.1f};
        r.check(engine::material_supports_opaque_pbr_runtime(test_material),"opaque material supports PBR runtime");
        const auto pbr=engine::PbrSurfaceParams::from_material(test_material);
        r.check(pbr.roughness==0.25f&&pbr.metallic==0.8f&&pbr.emissive[0]==2.0f,"material PBR params resolve");
        const auto lit=engine::evaluate_pbr_light({0.2f,0.4f,0.6f},pbr,{0,1,0},{0,0,1},{0,1,0},{1,1,1});
        r.check(lit[0]>0.0f&&lit[1]>0.0f&&lit[2]>0.0f,"metallic PBR returns positive radiance under headlight");
        const auto rough_dielectric=engine::evaluate_pbr_light({0.5f,0.5f,0.5f},engine::PbrSurfaceParams::dielectric_default(),
            {0,1,0},{0,0,1},{0,1,0},{1,1,1});
        r.check(rough_dielectric[0]>0.0f,"dielectric rough PBR returns positive radiance");
        test_material.opacity_mode=engine::OpacityMode::Masked;
        r.check(!engine::material_supports_opaque_pbr_runtime(test_material),"masked material fails closed for PBR runtime");
        test_material.opacity_mode=engine::OpacityMode::Blended;
        r.check(!engine::material_supports_opaque_pbr_runtime(test_material),"blended material fails closed for PBR runtime");
        test_material.opacity_mode=engine::OpacityMode::Opaque;
        const auto material_path=root/"assets/materials/saved.material.json";std::filesystem::create_directories(material_path.parent_path());
        r.check(engine::MaterialAsset::make_default().save_atomic(material_path).has_value(),"material asset save atomic");
        auto reloaded_material=engine::MaterialAsset::load(material_path);r.check(reloaded_material&&reloaded_material.value().physics.surface=="default","material asset round trip");
        std::filesystem::remove_all(root);
    }else if(suite=="world_forge"){
        const auto project=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/"samples"/"open-world-rpg";
        const auto path=engine::default_world_forge_factions_path(project);
        r.check(path.filename()=="factions.worldforge.json","default factions path filename");
        auto loaded=engine::WorldForgeFactionsAsset::load(path);
        r.check(loaded.has_value(),"sample factions.worldforge.json loads");
        r.check(loaded&&loaded.value().schema_version==1&&loaded.value().id=="tessera_factions","sample schema and id");
        r.check(loaded&&loaded.value().entities.size()==5,"sample seeds five faction entities");
        if(loaded){
            const auto& entities=loaded.value().entities;
            r.check(entities[0].id=="kingdom_tessera"&&entities[0].political_role&&
                *entities[0].political_role==engine::WorldForgePoliticalRole::Unknown,
                "kingdom_tessera draft with politicalRole unknown");
            r.check(entities[1].id=="chaotic_imperium"&&entities[1].canon_status==engine::WorldForgeCanonStatus::Established,
                "chaotic_imperium established");
            r.check(entities[4].id=="orc_warbands"&&
                std::find(entities[4].tags.begin(),entities[4].tags.end(),"multi-warband")!=entities[4].tags.end(),
                "orc_warbands is multi-warband container");
            const auto round_trip=engine::WorldForgeFactionsAsset::parse(loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==loaded.value().to_json(),"factions to_json round trip");
        }
        const auto empty_id=engine::WorldForgeFactionsAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"","kind":"faction","canonStatus":"draft"}]})");
        r.check(!empty_id&&empty_id.error().code=="WORLD-FORGE-FACTION-ID","empty entity id rejected");
        const auto dup_id=engine::WorldForgeFactionsAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft"},{"id":"a","kind":"faction","canonStatus":"draft"}]})");
        r.check(!dup_id&&dup_id.error().code=="WORLD-FORGE-FACTION-ID-DUP","duplicate entity id rejected");
        const auto bad_kind=engine::WorldForgeFactionsAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"empire","canonStatus":"draft"}]})");
        r.check(!bad_kind&&bad_kind.error().code=="WORLD-FORGE-FACTION-KIND","bad kind rejected");
        const auto bad_parent=engine::WorldForgeFactionsAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft","parentId":"missing"}]})");
        r.check(!bad_parent&&bad_parent.error().code=="WORLD-FORGE-FACTION-PARENT","unknown parentId rejected");
        r.check(engine::WorldForgeFactionsAsset::validate_file(path).has_value(),"sample factions validate_file succeeds");

        const auto pantheon_path=engine::default_world_forge_pantheon_path(project);
        r.check(pantheon_path.filename()=="pantheon.worldforge.json","default pantheon path filename");
        auto pantheon_loaded=engine::WorldForgePantheonAsset::load(pantheon_path);
        r.check(pantheon_loaded.has_value(),"sample pantheon.worldforge.json loads");
        r.check(pantheon_loaded&&pantheon_loaded.value().schema_version==1&&pantheon_loaded.value().id=="tessera_pantheon",
            "pantheon sample schema and id");
        r.check(pantheon_loaded&&pantheon_loaded.value().entities.size()==2,"pantheon seeds frangitur+creotar");
        if(pantheon_loaded){
            const auto round_trip=engine::WorldForgePantheonAsset::parse(pantheon_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==pantheon_loaded.value().to_json(),
                "pantheon to_json round trip");
        }
        const auto pantheon_bad_parent=engine::WorldForgePantheonAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"deity","canonStatus":"draft","parentId":"missing"}]})");
        r.check(!pantheon_bad_parent&&pantheon_bad_parent.error().code=="WORLD-FORGE-PANTHEON-PARENT",
            "pantheon unknown parentId rejected");
        const auto pantheon_cycle=engine::WorldForgePantheonAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"deity","canonStatus":"draft","parentId":"b"},{"id":"b","kind":"deity","canonStatus":"draft","parentId":"a"}]})");
        r.check(!pantheon_cycle&&pantheon_cycle.error().code=="WORLD-FORGE-PANTHEON-PARENT-CYCLE",
            "pantheon parentId cycle rejected");
        r.check(engine::WorldForgePantheonAsset::validate_file(pantheon_path).has_value(),
            "sample pantheon validate_file succeeds");
        const auto wf_pantheon=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","validate"},{"kind","pantheon"}});
        r.check(wf_pantheon.exit_code==engine::ExitCode::Success,"world_forge_apply validates pantheon");

        const auto archetypes_path=engine::default_world_forge_archetypes_path(project);
        r.check(archetypes_path.filename()=="archetypes.worldforge.json","default archetypes path filename");
        auto archetypes_loaded=engine::WorldForgeArchetypesAsset::load(archetypes_path);
        r.check(archetypes_loaded.has_value(),"sample archetypes.worldforge.json loads");
        r.check(archetypes_loaded&&archetypes_loaded.value().schema_version==1&&
                archetypes_loaded.value().id=="tessera_archetypes",
            "archetypes sample schema and id");
        r.check(archetypes_loaded&&archetypes_loaded.value().entities.size()==3,
            "archetypes seeds squire+archer+acolyte");
        if(archetypes_loaded){
            const auto& entities=archetypes_loaded.value().entities;
            r.check(entities[0].id=="squire"&&entities[0].kind==engine::WorldForgeArchetypeKind::Starting,
                "squire starting archetype");
            r.check(entities[1].id=="archer"&&entities[2].id=="acolyte","archer and acolyte seeded");
            const auto round_trip=engine::WorldForgeArchetypesAsset::parse(archetypes_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==archetypes_loaded.value().to_json(),
                "archetypes to_json round trip");
        }
        const auto archetype_bad_kind=engine::WorldForgeArchetypesAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"hero"}]})");
        r.check(!archetype_bad_kind&&archetype_bad_kind.error().code=="WORLD-FORGE-ARCHETYPE-KIND",
            "archetype bad kind rejected");
        const auto archetype_bad_faction=engine::WorldForgeArchetypesAsset::parse(
            R"({"schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"advanced","unlock":{"factionId":"missing_faction"}}]})");
        r.check(archetype_bad_faction.has_value(),"archetype with unknown faction parses before cross-ref");
        if(archetype_bad_faction){
            std::unordered_set<std::string> known{"kingdom_tessera"};
            const auto refs=archetype_bad_faction.value().validate_faction_refs(known);
            r.check(!refs&&refs.error().code=="WORLD-FORGE-ARCHETYPE-FACTION",
                "archetype unknown unlock.factionId rejected");
        }
        r.check(engine::WorldForgeArchetypesAsset::validate_file(archetypes_path).has_value(),
            "sample archetypes validate_file succeeds");
        const auto wf_archetypes=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","validate"},{"kind","archetypes"}});
        r.check(wf_archetypes.exit_code==engine::ExitCode::Success,"world_forge_apply validates archetypes");

        const auto rel_path=engine::default_world_forge_relationships_path(project);
        r.check(rel_path.filename()=="relationships.worldforge.json","default relationships path filename");
        auto rel_loaded=engine::WorldForgeRelationshipsAsset::load(rel_path);
        r.check(rel_loaded.has_value(),"sample relationships.worldforge.json loads");
        r.check(rel_loaded&&rel_loaded.value().schema_version==1&&rel_loaded.value().id=="tessera_relationships",
            "relationships sample schema and id");
        r.check(rel_loaded&&rel_loaded.value().nodes.size()==10&&rel_loaded.value().edges.size()==9,
            "relationships sample seed counts");
        if(rel_loaded){
            const auto round_trip=engine::WorldForgeRelationshipsAsset::parse(rel_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==rel_loaded.value().to_json(),
                "relationships to_json round trip");
            std::unordered_set<std::string> faction_ids;
            if(loaded) for(const auto& entity:loaded.value().entities) faction_ids.insert(entity.id);
            r.check(rel_loaded.value().validate_faction_refs(faction_ids).has_value(),
                "sample faction endpoints resolve against factions asset");
        }
        const auto empty_node=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"","kind":"person","canonStatus":"draft"}],"edges":[]})");
        r.check(!empty_node&&empty_node.error().code=="WORLD-FORGE-REL-NODE-ID","empty node id rejected");
        const auto bad_node_ref=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"missing"},"kind":"related","canonStatus":"draft"}]})");
        r.check(!bad_node_ref&&bad_node_ref.error().code=="WORLD-FORGE-REL-REF","unknown node endpoint rejected");
        const auto self_loop=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"a"},"kind":"related","canonStatus":"draft"}]})");
        r.check(!self_loop&&self_loop.error().code=="WORLD-FORGE-REL-SELF","self-loop rejected");
        const auto bad_edge_kind=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"},{"id":"b","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"node","id":"b"},"kind":"enemigos","canonStatus":"draft"}]})");
        r.check(!bad_edge_kind&&bad_edge_kind.error().code=="WORLD-FORGE-REL-EDGE-KIND","bad edge kind rejected");
        const auto bad_node_parent=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft","parentId":"missing"}],"edges":[]})");
        r.check(!bad_node_parent&&bad_node_parent.error().code=="WORLD-FORGE-REL-PARENT",
            "unknown node parentId rejected");
        const auto node_parent_cycle=engine::WorldForgeRelationshipsAsset::parse(
            R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft","parentId":"b"},{"id":"b","kind":"person","canonStatus":"draft","parentId":"a"}],"edges":[]})");
        r.check(!node_parent_cycle&&node_parent_cycle.error().code=="WORLD-FORGE-REL-PARENT-CYCLE",
            "node parentId cycle rejected");
        {
            auto ok_graph=engine::WorldForgeRelationshipsAsset::parse(
                R"({"schemaVersion":1,"id":"t","nodes":[{"id":"a","kind":"person","canonStatus":"draft"}],"edges":[{"id":"e1","from":{"target":"node","id":"a"},"to":{"target":"faction","id":"missing_faction"},"kind":"member_of","canonStatus":"draft"}]})");
            r.check(ok_graph.has_value(),"faction endpoint allowed without known faction set");
            std::unordered_set<std::string> known{"kingdom_tessera"};
            r.check(ok_graph&&!ok_graph.value().validate_faction_refs(known)&&
                ok_graph.value().validate_faction_refs(known).error().code=="WORLD-FORGE-REL-FACTION-REF",
                "unknown faction endpoint rejected when known set provided");
        }
        r.check(engine::WorldForgeRelationshipsAsset::validate_file(rel_path).has_value(),
            "sample relationships validate_file succeeds");
        const auto map_path=engine::default_world_forge_map_path(project);
        r.check(map_path.filename()=="map.worldforge.json","default map path filename");
        auto map_loaded=engine::WorldForgeMapAsset::load(map_path);
        r.check(map_loaded.has_value(),"sample map.worldforge.json loads");
        r.check(map_loaded&&map_loaded.value().schema_version==1&&map_loaded.value().id=="tessera_map",
            "map sample schema and id");
        r.check(map_loaded&&map_loaded.value().regions.size()==5&&map_loaded.value().pois.size()==6&&
            map_loaded.value().links.size()==6&&map_loaded.value().hydrology_regions.size()==2&&
            map_loaded.value().ferry_routes.size()==1,"map sample seed counts");
        r.check(map_loaded&&map_loaded.value().cartography_plate&&
            map_loaded.value().cartography_plate->width_meters==4000.0f,
            "map sample has 4 km cartographyPlate");
        if(map_loaded){
            const auto round_trip=engine::WorldForgeMapAsset::parse(map_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==map_loaded.value().to_json(),
                "map to_json round trip");
            std::unordered_set<std::string> faction_ids;
            if(loaded) for(const auto& entity:loaded.value().entities) faction_ids.insert(entity.id);
            r.check(map_loaded.value().validate_faction_refs(faction_ids).has_value(),
                "sample region factionIds resolve against factions asset");
        }
        {
            auto plated=engine::WorldForgeMapAsset::parse(
                R"({"schemaVersion":1,"id":"t","cartographyPlate":{"centerX":10,"centerZ":20,"widthMeters":4000,"heightMeters":2250},"regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":0,"y":1,"z":0}}],"pois":[],"links":[]})");
            r.check(plated&&plated.value().cartography_plate&&
                plated.value().cartography_plate->center_x==10.0f&&
                plated.value().cartography_plate->width_meters==4000.0f,"cartographyPlate parse");
            const auto bad_plate=engine::WorldForgeMapAsset::parse(
                R"({"schemaVersion":1,"id":"t","cartographyPlate":{"centerX":0,"centerZ":0,"widthMeters":0,"heightMeters":100},"regions":[],"pois":[],"links":[]})");
            r.check(!bad_plate&&bad_plate.error().code=="WORLD-FORGE-MAP-PLATE","zero-width plate rejected");
            auto scale_map=engine::WorldForgeMapAsset::parse(
                R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":100,"y":2,"z":0}},{"id":"r2","kind":"region","canonStatus":"draft","anchor":{"x":200,"y":2,"z":0}}],"pois":[],"links":[]})");
            r.check(scale_map.has_value(),"scale map parse");
            if(scale_map){
                const float applied=engine::apply_cartography_plate_and_rescale(scale_map.value(),4000.0f,1.0f,1.0f);
                r.check(applied>1.0f&&scale_map.value().cartography_plate&&
                    scale_map.value().cartography_plate->width_meters==4000.0f,"apply plate writes 4 km plate");
                const auto& a=*scale_map.value().regions[0].anchor;
                const auto& b=*scale_map.value().regions[1].anchor;
                const float dist=std::fabs(b.x-a.x);
                r.check(dist>3900.0f&&dist<4100.0f&&a.y==2.0f,"rescale spreads anchors; Y unchanged");
            }
        }
        const auto empty_region=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[{"id":"","kind":"region","canonStatus":"draft"}],"pois":[],"links":[]})");
        r.check(!empty_region&&empty_region.error().code=="WORLD-FORGE-MAP-REGION-ID","empty region id rejected");
        const auto bad_poi_region=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"missing"}],"links":[]})");
        r.check(!bad_poi_region&&bad_poi_region.error().code=="WORLD-FORGE-MAP-POI-REGION","unknown POI regionId rejected");
        const auto bad_link=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"region","toId":"missing","canonStatus":"draft"}]})");
        r.check(!bad_link&&bad_link.error().code=="WORLD-FORGE-MAP-REF","unknown link endpoint rejected");
        const auto self_link=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"region","toId":"r1","canonStatus":"draft"}]})");
        r.check(!self_link&&self_link.error().code=="WORLD-FORGE-MAP-SELF","self-link rejected");
        const auto bad_ferry_poi=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft"}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"r1"}],"links":[],"ferryRoutes":[{"id":"f1","fromPoiId":"p1","toPoiId":"missing","points":[]}]})");
        r.check(!bad_ferry_poi&&bad_ferry_poi.error().code=="WORLD-FORGE-MAP-FERRY-POI",
            "unknown ferry toPoiId rejected");
        const auto hydro_round=engine::WorldForgeMapAsset::parse(
            R"({"schemaVersion":1,"id":"t","regions":[],"pois":[],"links":[],"hydrologyRegions":[{"id":"pond","kind":"lake","minX":-5,"maxX":5,"minZ":-3,"maxZ":3,"acts":["act1"],"summary":"test"}]})");
        r.check(hydro_round.has_value()&&hydro_round.value().hydrology_regions.size()==1&&
            hydro_round.value().hydrology_regions[0].kind==engine::WorldForgeHydrologyKind::Lake,
            "hydrologyRegions parse");
        {
            auto ok_map=engine::WorldForgeMapAsset::parse(
                R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","factionIds":["missing_faction"]}],"pois":[],"links":[]})");
            r.check(ok_map.has_value(),"unknown factionId allowed without known set");
            std::unordered_set<std::string> known{"kingdom_tessera"};
            r.check(ok_map&&!ok_map.value().validate_faction_refs(known)&&
                ok_map.value().validate_faction_refs(known).error().code=="WORLD-FORGE-MAP-FACTION-REF",
                "unknown region factionId rejected when known set provided");
        }
        r.check(engine::WorldForgeMapAsset::validate_file(map_path).has_value(),"sample map validate_file succeeds");
        {
            auto anchored=engine::WorldForgeMapAsset::parse(
                R"({"schemaVersion":1,"id":"t","regions":[{"id":"r1","kind":"region","canonStatus":"draft","anchor":{"x":10,"y":1,"z":20}}],"pois":[{"id":"p1","kind":"landmark","canonStatus":"draft","regionId":"r1","anchor":{"x":12,"y":1,"z":22}}],"links":[{"id":"l1","kind":"travel","fromKind":"region","fromId":"r1","toKind":"poi","toId":"p1","canonStatus":"draft"}]})");
            r.check(anchored.has_value(),"map with anchors parses");
            if(anchored){
                const auto* ra=engine::resolve_map_endpoint_anchor(anchored.value(),
                    engine::WorldForgeMapEndpointKind::Region,"r1");
                const auto* pa=engine::resolve_map_endpoint_anchor(anchored.value(),
                    engine::WorldForgeMapEndpointKind::Poi,"p1");
                const auto* missing=engine::resolve_map_endpoint_anchor(anchored.value(),
                    engine::WorldForgeMapEndpointKind::Region,"missing");
                r.check(ra&&ra->x==10.0f&&ra->z==20.0f,"resolve region endpoint anchor");
                r.check(pa&&pa->x==12.0f&&pa->z==22.0f,"resolve poi endpoint anchor");
                r.check(missing==nullptr,"missing endpoint anchor is null");
                r.check(engine::map_region_marker_key("r1")=="region:r1","region marker key");
                r.check(engine::map_poi_marker_key("p1")=="poi:p1","poi marker key");
                const auto world=engine::graph_screen_to_world(engine::WorldForgeGraphCamera{},100.0f,50.0f);
                const auto back=engine::graph_world_to_screen_local(engine::WorldForgeGraphCamera{},world[0],world[1]);
                r.check(std::fabs(back[0]-100.0f)<0.01f&&std::fabs(back[1]-50.0f)<0.01f,
                    "map canvas screen/world round trip via graph camera");
            }
        }
        {
            using engine::CartographyStrokeStyle;
            r.check(std::string(engine::cartography_stroke_style_id(CartographyStrokeStyle::PoliticalBorder))==
                    "political_border","stroke style id political_border");
            r.check(engine::cartography_stroke_style_from_id("highway")==CartographyStrokeStyle::Highway,
                "stroke style from id highway");
            r.check(engine::cartography_stroke_style_from_id("stroke-ferry")==CartographyStrokeStyle::Ferry,
                "stroke style from texture-like id");
            const auto& road=engine::cartography_stroke_style_info(CartographyStrokeStyle::Road);
            r.check(std::string(road.texture_key)=="stroke-road"&&road.repeat_px==256.0f,"road stroke info");
            // Empty / single-point paths produce no stamps.
            r.check(engine::build_cartography_stroke_stamps({},4.0f,256.0f).empty(),"empty polyline no stamps");
            r.check(engine::build_cartography_stroke_stamps({{0,0}},4.0f,256.0f).empty(),"single point no stamps");
            // Degenerate zero-length segment skipped.
            engine::CartographyStrokeBuildStats stats{};
            auto skipped=engine::build_cartography_stroke_stamps({{0,0},{0,0},{10,0}},3.0f,256.0f,&stats);
            r.check(skipped.size()==1&&stats.segment_count==1&&stats.stamp_count==1,
                "zero-length segment skipped");
            // Continuous UV along a two-segment path.
            auto stamps=engine::build_cartography_stroke_stamps({{0,0},{100,0},{100,50}},4.0f,100.0f,&stats);
            r.check(stamps.size()==2&&stats.total_length_px>149.0f&&stats.total_length_px<151.0f,
                "two-segment stroke length");
            r.check(stamps[0].u0==0.0f&&std::fabs(stamps[0].u1-1.0f)<0.01f,"first stamp UV spans one repeat");
            r.check(std::fabs(stamps[1].u0-1.0f)<0.01f&&stamps[1].u1>stamps[1].u0,"second stamp continues UV");
            // Hit-testing remains against the authored polyline (not the ribbon thickness).
            const float on_line=engine::cartography_stroke_point_polyline_distance(50.0f,0.0f,{{0,0},{100,0}});
            const float off_line=engine::cartography_stroke_point_polyline_distance(50.0f,20.0f,{{0,0},{100,0}});
            r.check(on_line<0.5f&&off_line>19.0f,"polyline hit distance");
            // Stroke tile files exist with alpha (runtime + context).
            const auto stroke_runtime=project/"assets/ui/cartography/strokes/stroke-road.png";
            const auto stroke_context=std::filesystem::path("context/art/cartography/strokes/stroke-road.png");
#ifdef ENGINE_REPOSITORY_ROOT
            const auto stroke_context_abs=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/stroke_context;
            r.check(std::filesystem::exists(stroke_runtime)||std::filesystem::exists(stroke_context)||
                    std::filesystem::exists(stroke_context_abs),"stroke-road.png present");
#else
            r.check(std::filesystem::exists(stroke_runtime)||std::filesystem::exists(stroke_context),
                "stroke-road.png present");
#endif
        }
        const auto quest_path=engine::default_world_forge_quests_path(project);
        r.check(quest_path.filename()=="quests.worldforge.json","default quests path filename");
        auto quest_loaded=engine::WorldForgeQuestsAsset::load(quest_path);
        r.check(quest_loaded.has_value(),"sample quests.worldforge.json loads");
        r.check(quest_loaded&&quest_loaded.value().schema_version==1&&quest_loaded.value().id=="tessera_quests",
            "quests sample schema and id");
        r.check(quest_loaded&&quest_loaded.value().quests.size()==3,"quests sample seeds three quests");
        if(quest_loaded){
            const auto& mq=quest_loaded.value().quests[0];
            r.check(mq.id=="mq_act0_calrenoth"&&mq.kind==engine::WorldForgeQuestKind::Main&&
                mq.dialogue.start_id=="dlg_act0_wrathful_conquest",
                "mq_act0 is main quest hooked to Twine dialogue");
            r.check(!mq.acts.empty()&&mq.acts[0]=="act0","mq_act0 has acts=[act0]");
            r.check(engine::matches_world_forge_act_filter(mq.acts, mq.tags, "act0"),
                "act0 filter matches mq_act0");
            r.check(!engine::matches_world_forge_act_filter(mq.acts, mq.tags, "act1"),
                "act1 filter excludes mq_act0");
            r.check(engine::matches_world_forge_act_filter({}, {}, "act2"),
                "empty acts is campaign-wide under act filter");
            const auto& q0=quest_loaded.value().quests[1];
            r.check(q0.id=="sq_01_cart_again"&&q0.objectives.size()==4&&q0.dialogue.start_id.empty(),
                "sq_01 keeps soft-empty dialogue hooks until authored");
            r.check(q0.objectives[0].dialogue_id.empty()&&q0.forks.size()==1&&q0.forks[0].dialogue_id.empty(),
                "sq_01 objective and fork dialogue ids soft-empty");
            const auto round_trip=engine::WorldForgeQuestsAsset::parse(quest_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==quest_loaded.value().to_json(),
                "quests to_json round trip");
            std::unordered_set<std::string> region_ids;
            if(map_loaded) for(const auto& region:map_loaded.value().regions) region_ids.insert(region.id);
            r.check(quest_loaded.value().validate_region_refs(region_ids).has_value(),
                "sample quest regionIds resolve against map asset");
        }
        const auto empty_quest=engine::WorldForgeQuestsAsset::parse(
            R"({"schemaVersion":1,"id":"t","quests":[{"id":"","kind":"side","canonStatus":"draft","objectives":[],"forks":[]}]})");
        r.check(!empty_quest&&empty_quest.error().code=="WORLD-FORGE-QUEST-ID","empty quest id rejected");
        const auto bad_act=engine::WorldForgeQuestsAsset::parse(
            R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","acts":["act99"],"objectives":[],"forks":[]}]})");
        r.check(!bad_act&&bad_act.error().code=="WORLD-FORGE-ACT-ID","bad act id rejected");
        const auto dup_objective=engine::WorldForgeQuestsAsset::parse(
            R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","objectives":[{"id":"a","summary":""},{"id":"a","summary":""}],"forks":[]}]})");
        r.check(!dup_objective&&dup_objective.error().code=="WORLD-FORGE-QUEST-OBJECTIVE-ID-DUP",
            "duplicate objective id rejected");
        const auto bad_quest_kind=engine::WorldForgeQuestsAsset::parse(
            R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"epic","canonStatus":"draft","objectives":[],"forks":[]}]})");
        r.check(!bad_quest_kind&&bad_quest_kind.error().code=="WORLD-FORGE-QUEST-KIND","bad quest kind rejected");
        {
            auto ok_quest=engine::WorldForgeQuestsAsset::parse(
                R"({"schemaVersion":1,"id":"t","quests":[{"id":"q1","kind":"side","canonStatus":"draft","regionId":"missing","objectives":[],"forks":[]}]})");
            r.check(ok_quest.has_value(),"unknown regionId allowed without known set");
            std::unordered_set<std::string> known{"tessera_overland"};
            r.check(ok_quest&&!ok_quest.value().validate_region_refs(known)&&
                ok_quest.value().validate_region_refs(known).error().code=="WORLD-FORGE-QUEST-REGION-REF",
                "unknown quest regionId rejected when known set provided");
        }
        r.check(engine::WorldForgeQuestsAsset::validate_file(quest_path).has_value(),
            "sample quests validate_file succeeds");
        const auto dlg_path=engine::default_world_forge_dialogues_path(project);
        r.check(dlg_path.filename()=="dialogues.worldforge.json","default dialogues path filename");
        auto dlg_loaded=engine::WorldForgeDialoguesAsset::load(dlg_path);
        r.check(dlg_loaded.has_value(),"sample dialogues.worldforge.json loads");
        r.check(dlg_loaded&&dlg_loaded.value().schema_version==1&&dlg_loaded.value().id=="tessera_dialogues",
            "dialogues sample schema and id");
        r.check(dlg_loaded&&dlg_loaded.value().trees.size()==1&&
            dlg_loaded.value().trees[0].id=="dlg_act0_wrathful_conquest",
            "dialogues sample seeds Twine Act 0 tree");
        if(dlg_loaded){
            const auto& tree=dlg_loaded.value().trees[0];
            r.check(tree.parent_quest_id=="mq_act0_calrenoth"&&tree.entry_node_id=="prologue"&&
                tree.nodes.size()>=40,"Twine tree parents to mq_act0 with prologue entry");
            const auto round_trip=engine::WorldForgeDialoguesAsset::parse(dlg_loaded.value().to_json());
            r.check(round_trip&&round_trip.value().to_json()==dlg_loaded.value().to_json(),
                "dialogues to_json round trip");
            std::unordered_set<std::string> quest_ids;
            if(quest_loaded) for(const auto& quest:quest_loaded.value().quests) quest_ids.insert(quest.id);
            r.check(dlg_loaded.value().validate_quest_refs(quest_ids).has_value(),
                "sample dialogue parentQuestId resolves against quests asset");
            engine::DialogueRuntime runtime;
            r.check(runtime.bind(&dlg_loaded.value()).has_value(),"DialogueRuntime binds sample asset");
            r.check(runtime.start("dlg_act0_wrathful_conquest").has_value(),"DialogueRuntime starts Twine tree");
            auto present=runtime.present();
            r.check(present&&present.value().node_id=="prologue"&&!present.value().choices.empty(),
                "DialogueRuntime presents prologue with choices");
            if(present&&!present.value().choices.empty()){
                r.check(runtime.choose(present.value().choices.front().id).has_value(),
                    "DialogueRuntime chooses first prologue branch");
                auto next=runtime.present();
                r.check(next&&next.value().node_id=="tutorial","DialogueRuntime advances to tutorial");
            }
            {
                engine::QuestRuntime unbound;
                r.check(!unbound.start("sq_01_cart_again").has_value(),"QuestRuntime start fails when unbound");
                r.check(!unbound.bind(nullptr).has_value(),"QuestRuntime rejects null bind");
            }
            if(quest_loaded){
                engine::QuestRuntime quest_rt;
                r.check(quest_rt.bind(&quest_loaded.value()).has_value(),"QuestRuntime binds sample quests");
                r.check(!quest_rt.start("missing_quest").has_value(),"QuestRuntime rejects unknown quest");
                r.check(quest_rt.start("sq_01_cart_again").has_value(),"QuestRuntime starts sq_01");
                auto st=quest_rt.status("sq_01_cart_again");
                r.check(st&&st.value().status==engine::QuestInstanceStatus::Active&&
                    st.value().current_objective_id=="find_pellin","QuestRuntime current is first objective");
                r.check(!quest_rt.complete_objective("sq_01_cart_again","lift_cart").has_value(),
                    "QuestRuntime rejects out-of-order objective");
                r.check(quest_rt.complete_objective("sq_01_cart_again","find_pellin").has_value(),
                    "QuestRuntime completes first objective");
                st=quest_rt.status("sq_01_cart_again");
                r.check(st&&st.value().current_objective_id=="clear_scavengers","QuestRuntime advances objective");
                r.check(quest_rt.list_active().size()==1,"QuestRuntime lists one active quest");
                r.check(quest_rt.complete_objective("sq_01_cart_again","clear_scavengers").has_value(),
                    "QuestRuntime completes scavengers");
                r.check(quest_rt.complete_objective("sq_01_cart_again","lift_cart").has_value(),
                    "QuestRuntime completes lift");
                r.check(quest_rt.complete_objective("sq_01_cart_again","escort_gate").has_value(),
                    "QuestRuntime completes escort");
                st=quest_rt.status("sq_01_cart_again");
                r.check(st&&st.value().status==engine::QuestInstanceStatus::Completed&&
                    quest_rt.list_active().empty(),"QuestRuntime completes quest");
                r.check(quest_rt.start("mq_act0_calrenoth").has_value(),"QuestRuntime starts main quest");
                auto hook=quest_rt.dialogue_for_stage("mq_act0_calrenoth",engine::QuestDialogueStage::Start);
                r.check(hook&&hook.value()=="dlg_act0_wrathful_conquest","QuestRuntime dialogue start hook");
                r.check(quest_rt.abandon("mq_act0_calrenoth").has_value(),"QuestRuntime abandons active quest");
                st=quest_rt.status("mq_act0_calrenoth");
                r.check(st&&st.value().status==engine::QuestInstanceStatus::Abandoned,"QuestRuntime abandoned status");
                r.check(quest_rt.primary_objective_text().empty(),"QuestRuntime primary objective text empty when idle");
            }
            {
                const auto standing_factions=engine::WorldForgeFactionsAsset::parse(R"({
                    "schemaVersion":1,"id":"standing_test","entities":[
                      {"id":"alpha","kind":"faction","canonStatus":"draft","standing":{
                        "tracksPlayer":true,"min":-100,"max":100,
                        "ranks":[{"id":"hostile","minScore":-100,"displayName":"Hostile"},
                                 {"id":"neutral","minScore":0,"displayName":"Neutral"},
                                 {"id":"friendly","minScore":25,"displayName":"Friendly"},
                                 {"id":"allied","minScore":75,"displayName":"Allied"}],
                        "lockIn":{"threshold":80,"exclusiveFactionIds":["beta"]}
                      }},
                      {"id":"beta","kind":"faction","canonStatus":"draft","standing":{
                        "tracksPlayer":true,"min":-100,"max":100,
                        "ranks":[{"id":"hostile","minScore":-100,"displayName":"Hostile"},
                                 {"id":"neutral","minScore":0,"displayName":"Neutral"}],
                        "lockIn":{"threshold":80,"exclusiveFactionIds":["alpha"]}
                      }}
                    ]})");
                const auto standing_rels=engine::WorldForgeRelationshipsAsset::parse(R"({
                    "schemaVersion":1,"id":"standing_rel","nodes":[],"edges":[
                      {"id":"a_vs_b","from":{"target":"faction","id":"alpha"},
                       "to":{"target":"faction","id":"beta"},"kind":"rival","canonStatus":"draft",
                       "standingTransfer":0.5}
                    ]})");
                r.check(standing_factions.has_value()&&standing_rels.has_value(),"standing fixtures parse");
                engine::StandingRuntime standing_rt;
                r.check(!standing_rt.bind(nullptr,nullptr).has_value(),"StandingRuntime rejects null bind");
                if(standing_factions&&standing_rels){
                    r.check(standing_rt.bind(&standing_factions.value(),&standing_rels.value()).has_value(),
                        "StandingRuntime binds fixtures");
                    r.check(standing_rt.adjust("alpha",40.0).has_value(),"StandingRuntime adjust alpha");
                    auto alpha=standing_rt.get("alpha");
                    auto beta=standing_rt.get("beta");
                    r.check(alpha.has_value()&&alpha.value()==40.0,"StandingRuntime primary score");
                    r.check(beta.has_value()&&beta.value()==-20.0,"StandingRuntime hostility fallout 0.5");
                    r.check(standing_rt.adjust("alpha",100.0).has_value(),"StandingRuntime clamp adjust");
                    alpha=standing_rt.get("alpha");
                    r.check(alpha.has_value()&&alpha.value()==100.0,"StandingRuntime clamps to max");
                    auto rank=standing_rt.rank("alpha");
                    r.check(rank.has_value()&&rank.value()=="allied","StandingRuntime rank allied");
                    engine::WorldForgeQuestStandingRequirement req;
                    req.faction_id="alpha";
                    req.min_rank_id="friendly";
                    auto meets=standing_rt.meets_requirement(req);
                    r.check(meets.has_value()&&meets.value(),"StandingRuntime meets rank gate");
                    auto locked=standing_rt.lock_in_faction();
                    r.check(locked.has_value()&&locked.value()=="alpha","StandingRuntime lock-in at threshold");
                    r.check(standing_rt.list_tracked().size()==2,"StandingRuntime lists tracked");
                }
                const auto bad_order=engine::WorldForgeFactionsAsset::parse(R"({
                    "schemaVersion":1,"id":"t","entities":[{"id":"a","kind":"faction","canonStatus":"draft",
                    "standing":{"tracksPlayer":true,"ranks":[
                      {"id":"high","minScore":50,"displayName":"H"},
                      {"id":"low","minScore":0,"displayName":"L"}]}}]})");
                r.check(!bad_order&&bad_order.error().code=="WORLD-FORGE-FACTION-STANDING-RANK-ORDER",
                    "standing ranks out of order rejected");
                const auto bad_transfer=engine::WorldForgeRelationshipsAsset::parse(R"({
                    "schemaVersion":1,"id":"t","nodes":[],"edges":[
                      {"id":"e1","from":{"target":"faction","id":"a"},"to":{"target":"faction","id":"b"},
                       "kind":"rival","canonStatus":"draft","standingTransfer":-1}]})");
                r.check(!bad_transfer&&bad_transfer.error().code=="WORLD-FORGE-REL-STANDING-TRANSFER",
                    "negative standingTransfer rejected");
            }
            const auto reachable=engine::dialogue_reachable_node_ids(tree);
            r.check(reachable.size()>=10&&reachable.front()=="prologue",
                "dialogue reachability walks from prologue");
            engine::DialogueGraphPositions layout;
            engine::layout_dialogue_graph(tree,layout,false);
            r.check(layout.count("prologue")==1&&layout.count("tutorial")==1,
                "dialogue layered layout places entry and child");
            auto edit_tree=tree;
            const auto new_id=engine::unique_dialogue_node_id(edit_tree,"graph_test_node");
            r.check(engine::add_dialogue_node(edit_tree,new_id,"narrator","test").has_value(),
                "headless add dialogue node");
            const auto choice_id=engine::unique_dialogue_choice_id(edit_tree,"graph_test_choice");
            r.check(engine::add_dialogue_choice(edit_tree,"prologue",choice_id,"test link",new_id).has_value(),
                "headless add dialogue choice edge");
            r.check(engine::remove_dialogue_choice(edit_tree,"prologue",choice_id).has_value(),
                "headless remove dialogue choice");
            r.check(engine::remove_dialogue_node(edit_tree,new_id).has_value(),
                "headless remove dialogue node");
            {
                auto dup_tree=tree;
                const auto dup=engine::duplicate_dialogue_node(dup_tree,"prologue");
                r.check(dup.has_value()&&dup.value()!="prologue","duplicate dialogue node yields new id");
                r.check(engine::infer_dialogue_node_kind(tree.nodes.front())==engine::DialogueGraphNodeKind::Dialogue||
                    engine::infer_dialogue_node_kind(tree.nodes.front())==engine::DialogueGraphNodeKind::End,
                    "infer dialogue node kind");
                const auto compact=engine::dialogue_node_card_size(engine::DialogueGraphNodeDisplayMode::Compact);
                const auto standard=engine::dialogue_node_card_size(engine::DialogueGraphNodeDisplayMode::Standard);
                const auto expanded=engine::dialogue_node_card_size(engine::DialogueGraphNodeDisplayMode::Expanded);
                r.check(compact[1]<standard[1]&&standard[1]<expanded[1],"display mode card heights ordered");
                r.check(engine::dialogue_node_matches_query(tree.nodes.front(),"prologue")||
                    !engine::dialogue_search_node_ids(tree,"narrator").empty(),
                    "dialogue search matches speaker or id");
                engine::WorldForgeGraphCamera cam;
                cam.min_zoom=0.2f;
                cam.max_zoom=2.0f;
                engine::DialogueGraphPositions positions;
                engine::layout_dialogue_graph(tree,positions,false);
                const auto bounds=engine::compute_graph_bounds(positions);
                engine::fit_graph_camera_to_bounds(cam,800.0f,600.0f,bounds);
                r.check(bounds.valid&&cam.zoom>=cam.min_zoom&&cam.zoom<=cam.max_zoom,
                    "shared graph camera fits dialogue bounds");
                engine::center_graph_camera_on(cam,800.0f,600.0f,{100.0f,50.0f},1.0f);
                const auto world=engine::graph_screen_to_world(cam,400.0f,300.0f);
                r.check(std::fabs(world[0]-100.0f)<0.01f&&std::fabs(world[1]-50.0f)<0.01f,
                    "graph camera centers on world point");
            }
            r.check(engine::slugify_id("Go Get the flowers")=="go_get_the_flowers",
                "slugify_id lowercases and underscores titles");
            r.check(engine::slugify_id("  Hello--World!! ")=="hello_world","slugify_id collapses punctuation");
            engine::WorldForgeDialoguesAsset edited_asset;
            edited_asset.schema_version=1;
            edited_asset.id="edited";
            edited_asset.trees.push_back(edit_tree);
            r.check(edited_asset.validate().has_value(),"edited dialogue tree still validates");
        }
        const auto empty_tree=engine::WorldForgeDialoguesAsset::parse(
            R"({"schemaVersion":1,"id":"t","trees":[{"id":"","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
        r.check(!empty_tree&&empty_tree.error().code=="WORLD-FORGE-DLG-TREE-ID","empty dialogue tree id rejected");
        const auto bad_entry=engine::WorldForgeDialoguesAsset::parse(
            R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","canonStatus":"draft","entryNodeId":"missing","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
        r.check(!bad_entry&&bad_entry.error().code=="WORLD-FORGE-DLG-ENTRY","missing entry node rejected");
        const auto bad_next=engine::WorldForgeDialoguesAsset::parse(
            R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[{"id":"c1","text":"x","nextNodeId":"gone","setFlags":[]}]}]}]})");
        r.check(!bad_next&&bad_next.error().code=="WORLD-FORGE-DLG-NEXT","unknown nextNodeId rejected");
        {
            auto ok_dlg=engine::WorldForgeDialoguesAsset::parse(
                R"({"schemaVersion":1,"id":"t","trees":[{"id":"d1","parentQuestId":"missing","canonStatus":"draft","entryNodeId":"n","nodes":[{"id":"n","speakerId":"","line":"","choices":[]}]}]})");
            r.check(ok_dlg.has_value(),"unknown parentQuestId allowed without known set");
            std::unordered_set<std::string> known{"mq_act0_calrenoth"};
            r.check(ok_dlg&&!ok_dlg.value().validate_quest_refs(known)&&
                ok_dlg.value().validate_quest_refs(known).error().code=="WORLD-FORGE-DLG-QUEST-REF",
                "unknown parentQuestId rejected when known set provided");
        }
        r.check(engine::WorldForgeDialoguesAsset::validate_file(dlg_path).has_value(),
            "sample dialogues validate_file succeeds");
        const auto wf_quests=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","validate"},{"kind","quests"}});
        r.check(wf_quests.exit_code==engine::ExitCode::Success,"world_forge_apply validates quests");
        const auto wf_dialogues=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","validate"},{"kind","dialogues"}});
        r.check(wf_dialogues.exit_code==engine::ExitCode::Success,"world_forge_apply validates dialogues");
        {
            const auto repo=std::filesystem::path(ENGINE_REPOSITORY_ROOT);
            const auto twee_path=repo/"context"/"story"/"sources"/"wrathful-conquest-act0.twee";
            engine::TweeImportOptions opts;
            opts.tree_id="dlg_act0_import_probe";
            opts.display_name="Act 0 import probe";
            opts.parent_quest_id="mq_act0_calrenoth";
            opts.entry_node_id="prologue";
            const auto imported=engine::import_twee_dialogue_tree_file(twee_path,opts);
            r.check(imported.has_value(),"twee importer parses Act 0 source");
            r.check(imported&&imported.value().nodes.size()>=40&&imported.value().entry_node_id=="prologue",
                "twee importer yields prologue entry and many nodes");
            const auto temp=std::filesystem::temp_directory_path()/"engine_twee_import_test";
            std::filesystem::remove_all(temp);
            const auto dlg_dir=temp/"assets"/"world-forge";
            std::filesystem::create_directories(dlg_dir);
            std::filesystem::copy_file(dlg_path,dlg_dir/"dialogues.worldforge.json");
            const auto quests_src=engine::default_world_forge_quests_path(project);
            if(std::filesystem::exists(quests_src))
                std::filesystem::copy_file(quests_src,dlg_dir/"quests.worldforge.json");
            const auto wf_import=engine::apply_world_forge_operation(temp,
                nlohmann::json{{"action","import_twee"},{"kind","dialogues"},
                    {"tweePath",twee_path.string()},
                    {"treeId","dlg_act0_import_probe"},
                    {"displayName","Act 0 import probe"},
                    {"parentQuestId","mq_act0_calrenoth"},
                    {"entryNodeId","prologue"}});
            r.check(wf_import.exit_code==engine::ExitCode::Success,"world_forge import_twee succeeds");
            auto probe=engine::WorldForgeDialoguesAsset::load(dlg_dir/"dialogues.worldforge.json");
            r.check(probe&&probe.value().trees.size()==2,"import_twee upserts a second tree without wiping sample");
            bool found_probe=false;
            if(probe) for(const auto& tree:probe.value().trees)
                if(tree.id=="dlg_act0_import_probe"){found_probe=tree.nodes.size()>=40;break;}
            r.check(found_probe,"imported probe tree persists with Act 0 node count");
            std::filesystem::remove_all(temp);
        }
        r.check(engine::validate_project_at(project).exit_code==engine::ExitCode::Success,
            "sample project still validates with world forge factions");
    }else if(suite=="streaming"){
        engine::WorldPartition p; auto c=p.cell_for({-0.1,0,-0.1}); r.check(c&&c.value()==engine::CellCoord{-1,-1},"negative floor mapping");
        engine::CellStreamer s([](engine::CellCoord c,const std::atomic_bool&){return engine::Result<engine::CellData>::success({c,1,"ok"});});
        r.check(s.request({0,0},0).has_value(),"request accepted");
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
        while(!s.loaded({0,0})&&std::chrono::steady_clock::now()<deadline){(void)s.update();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        r.check(s.loaded({0,0}),"cell committed");
    }else if(suite=="terrain"){
        engine::TerrainTileMetadata t{{0,0},129,-1,2,"height",{}}; r.check(t.validate().has_value(),"metadata valid"); t.height_resolution=128;r.check(!t.validate(),"bad resolution rejected");
        auto bubble=engine::simulation_bubble({0,0},2);r.check(bubble&&bubble.value().size()==13,"bubble deterministic");
        auto terrain=engine::generate_stylized_terrain({0,0},33,40);r.check(terrain.has_value(),"stylized terrain generated");
        r.check(terrain&&terrain.value().heights.size()==1089&&terrain.value().triangles.size()==6144,"terrain topology is complete");
        auto repeat=engine::generate_stylized_terrain({0,0},33,40);r.check(repeat&&terrain&&repeat.value().heights==terrain.value().heights,"terrain generation deterministic");
        auto east=engine::generate_stylized_terrain({1,0},33,40);bool seam=terrain&&east;for(std::uint32_t z=0;seam&&z<33;++z)seam=std::abs(terrain.value().sample(32,z)-east.value().sample(0,z))<0.00001f;r.check(seam,"adjacent terrain borders match");
        r.check(!engine::generate_stylized_terrain({0,0},16,40),"invalid mesh resolution rejected");r.check(!engine::generate_stylized_terrain({0,0},33,0),"invalid cell size rejected");
        r.check(engine::terrain_cell_for_position(0,0,40)==engine::CellCoord{0,0},"terrain cell at origin");
        r.check(engine::terrain_cell_for_position(45,0,40)==engine::CellCoord{1,0},"terrain cell follows positive X");
        r.check(engine::terrain_cells_in_radius({0,0},1).size()==9,"terrain radius one loads nine cells");
        r.check(engine::terrain_cells_in_radius({0,0},2).size()==25,"terrain radius two loads twenty-five cells");
        bool distinct_colors=false;std::array<float,3> first{};bool first_set=false;for(const auto& tri:terrain.value().triangles){if(!first_set){first={tri.r,tri.g,tri.b};first_set=true;continue;}if(std::abs(tri.r-first[0])>0.01f||std::abs(tri.g-first[1])>0.01f||std::abs(tri.b-first[2])>0.01f){distinct_colors=true;break;}}r.check(distinct_colors,"terrain material regions vary surface color");
        engine::StreamedTerrainField field;engine::CollisionWorld world;
        r.check(field.update(world,{0,5,0},{}).has_value()&&field.loaded_cell_count()==25,"streamed terrain loads a neighborhood");
        r.check(field.update(world,{45,5,0},{}).has_value()&&field.focus_cell()==engine::CellCoord{1,0},"streamed terrain recenters on camera cell");
        r.check(field.contains({1,0})&&field.contains({0,0}),"streamed terrain keeps overlapping cells while moving");
        engine::WorldPartition partition;
        const auto half_extent=static_cast<float>(partition.config().world_half_extent);
        std::size_t peak_cells=0;std::set<engine::CellCoord> visited;
        for(float z=-half_extent;z<=half_extent;z+=80.0f){for(float x=-half_extent;x<=half_extent;x+=80.0f){r.check(field.update(world,{x,12.0f,z},{},2).has_value(),"16 km2 stress update succeeds");peak_cells=std::max(peak_cells,field.loaded_cell_count());visited.insert(field.focus_cell());}}
        r.check(peak_cells<=25,"16 km2 stress keeps bounded resident terrain cells");
        r.check(visited.size()>=24,"16 km2 stress path covers many terrain cells");
        r.check(half_extent>=1900.0f&&partition.config().world_half_extent*partition.config().world_half_extent*4.0e-6>=15.0,"world partition spans roughly sixteen square kilometers");
        engine::TerrainEditStore edits;
        const auto brushed=edits.apply_brush(0.0f,0.0f,3.0f,0.5f,false);
        r.check(brushed&&brushed.value().count({0,0})==1,"terrain brush touches origin cell");
        auto edited=engine::generate_stylized_terrain({0,0},33,40,&edits);
        auto baseline=engine::generate_stylized_terrain({0,0},33,40);
        r.check(edited&&baseline&&edited.value().heights!=baseline.value().heights,"terrain edits change generated heights");
        engine::set_active_terrain_edits(&edits);
        const float sampled=engine::sample_terrain_height(0.0f,0.0f);
        const float procedural=engine::procedural_terrain_height(0.0f,0.0f);
        r.check(sampled>procedural,"sample_terrain_height includes edits");
        engine::set_active_terrain_edits(nullptr);
        const auto round_trip=engine::TerrainEditStore::from_json(edits.to_json());
        r.check(round_trip.has_value(),"terrain edits round trip");
        r.check(!engine::TerrainEditStore::from_json("{\"schemaVersion\":2}"),"invalid terrain edit schema rejected");
        engine::TerrainEditHistory history;
        std::map<engine::CellCoord,std::vector<float>> before{{ {0,0}, {} }};
        std::map<engine::CellCoord,std::vector<float>> after;
        after[{0,0}]=edits.cell_deltas_or_empty({0,0});
        r.check(history.execute(edits,std::make_unique<engine::TerrainBrushStrokeCommand>(before,after)).has_value(),"terrain brush command executes");
        r.check(history.undo(edits).has_value(),"terrain brush command undoes");
        engine::TerrainPaintStore paint;
        const std::uint16_t grass_index=paint.ensure_material_index("assets/materials/grass.material.json");
        const auto painted=paint.apply_material_brush(0.0f,0.0f,3.0f,grass_index);
        r.check(painted&&painted.value().count({0,0})==1,"terrain paint brush touches origin cell");
        engine::MaterialAsset grass_material;grass_material.base_color={0.2f,0.8f,0.1f,1.0f};
        std::map<std::string,engine::MaterialAsset> paint_materials{{"assets/materials/grass.material.json",grass_material}};
        const auto paint_lookup=[&paint_materials](const std::string& path)->const engine::MaterialAsset*{
            const auto found=paint_materials.find(path);return found==paint_materials.end()?nullptr:&found->second;};
        auto painted_mesh=engine::generate_stylized_terrain({0,0},33,40,nullptr,&paint,paint_lookup);
        auto procedural_mesh=engine::generate_stylized_terrain({0,0},33,40);
        r.check(painted_mesh&&procedural_mesh,"terrain paint mesh generation succeeds");
        r.check(painted_mesh&&!painted_mesh.value().triangles.empty(),"terrain paint mesh is non-empty");
        r.check(std::any_of(painted_mesh.value().triangles.begin(),painted_mesh.value().triangles.end(),[](const engine::TerrainVertex& v){return v.painted&&v.g>0.6f;}),"terrain paint changes vertex colors");
        r.check(std::any_of(painted_mesh.value().triangles.begin(),painted_mesh.value().triangles.end(),[](const engine::TerrainVertex& v){return v.painted;}),"painted terrain vertices are flagged");
        const auto paint_round_trip=engine::TerrainPaintStore::from_json(paint.to_json());
        r.check(paint_round_trip.has_value(),"terrain paint round trip");
        engine::TerrainEditStore flatten_edits;
        const float flatten_target=engine::procedural_terrain_height(0.0f,0.0f)+1.0f;
        (void)flatten_edits.apply_brush(0.0f,0.0f,4.0f,2.0f,false);
        const auto flattened=flatten_edits.apply_flatten_brush(0.0f,0.0f,4.0f,2.0f,flatten_target);
        r.check(flattened&&!flattened.value().empty(),"terrain flatten brush touches cells");
        engine::set_active_terrain_edits(&flatten_edits);
        r.check(std::abs(engine::sample_terrain_height(0.0f,0.0f)-flatten_target)<0.05f,"terrain flatten approaches target height");
        engine::set_active_terrain_edits(nullptr);
    }else if(suite=="water"){
        engine::WaterStore store;
        store.set_sea_level(-0.35f);
        const auto brushed=store.apply_place_brush(0.0f,0.0f,6.0f,0.8f);
        r.check(brushed&&brushed.value().count({0,0})==1,"water place brush touches origin cell");
        const auto round_trip=engine::WaterStore::from_json(store.to_json());
        r.check(round_trip.has_value(),"water store round trip");
        r.check(round_trip.value().sea_level()==store.sea_level(),"water store round trip keeps sea level");
        engine::set_active_water_store(&store);
        const auto surface=engine::sample_water_surface_y(0.0f,0.0f);
        r.check(surface&&std::abs(*surface-store.sea_level())<0.001f,"sample_water_surface_y returns sea level over authored fill");
        engine::WaterStore apply_store;
        apply_store.set_sea_level(-0.35f);
        engine::WaterEditHistory history;
        bool dirty=false;
        engine::EditorSessionContext context;
        context.project_root=std::filesystem::path("samples/open-world-rpg");
        context.water_store=&apply_store;
        context.water_history=&history;
        context.water_dirty=&dirty;
        engine::set_active_water_store(&apply_store);
        // Brush inside the origin hollow where procedural terrain is below seaLevel (-0.35).
        r.check(!engine::sample_water_surface_y(0.0f,0.0f),"water sample empty before brush stroke");
        const auto placed=engine::execute_editor_operation(context,"water_apply",
            R"({"action":"place","x":0,"z":0,"radius":3,"strength":0.6})");
        r.check(placed.exit_code==engine::ExitCode::Success,"water_apply place succeeds");
        r.check(history.undo_size()>=1,"water_apply place commits undo entry");
        r.check(engine::sample_water_surface_y(0.0f,0.0f).has_value(),"water_apply place writes surface sample");
        const auto undone=engine::execute_editor_operation(context,"water_apply",R"({"action":"undo"})");
        r.check(undone.exit_code==engine::ExitCode::Success,"water_apply undo succeeds");
        r.check(!engine::sample_water_surface_y(0.0f,0.0f),"water_apply undo restores prior fill");
        apply_store.set_sea_level(-3.0f);
        (void)apply_store.apply_place_brush(0.0f,0.0f,3.0f,1.0f);
        r.check(!engine::sample_water_surface_y(0.0f,0.0f),
            "authored fill above terrain (sea below bed) does not report a floating surface");
        engine::StreamedWaterField field;
        r.check(field.update({0.0f,5.0f,0.0f},2,&store).has_value(),"streamed water loads a neighborhood");
        r.check(field.loaded_cell_count()>0,"streamed water builds cell meshes");
        const auto water_verts=field.build_render_vertices();
        r.check(!water_verts.empty(),"streamed water mesh emits shoreline-aware vertices");
        float min_depth=1.0e9f,max_depth=0.0f;
        for(const auto& v:water_verts){min_depth=std::min(min_depth,v.depth);max_depth=std::max(max_depth,v.depth);}
        r.check(max_depth>min_depth+0.05f,"water mesh encodes varying column depth for absorption");
        r.check(max_depth>0.2f,"water mesh depth reaches deeper basin samples");
        apply_store.set_sea_level(-0.35f);
        engine::set_active_water_store(&apply_store);
        const auto place_along=engine::execute_editor_operation(context,"water_apply",
            R"({"action":"place_along","points":[{"x":0,"z":0},{"x":4,"z":-2}],"radius":2.5,"strength":1,"step":2})");
        r.check(place_along.exit_code==engine::ExitCode::Success,"water_apply place_along succeeds");
        engine::TerrainEditStore height_store;
        engine::TerrainEditHistory height_history;
        bool height_dirty=false;
        engine::EditorSessionContext terrain_ctx;
        terrain_ctx.project_root=std::filesystem::path("samples/open-world-rpg");
        terrain_ctx.terrain_edits=&height_store;
        terrain_ctx.terrain_history=&height_history;
        terrain_ctx.terrain_edits_dirty=&height_dirty;
        terrain_ctx.water_store=&apply_store;
        engine::set_active_terrain_edits(&height_store);
        const auto set_h=engine::execute_editor_operation(terrain_ctx,"terrain_apply",
            R"({"action":"set_height","x":0,"z":0,"radius":3,"strength":1,"targetHeight":1.25})");
        r.check(set_h.exit_code==engine::ExitCode::Success,"terrain_apply set_height succeeds");
        r.check(std::abs(engine::sample_terrain_height(0.0f,0.0f)-1.25f)<0.15f,
            "set_height reaches target near brush center");
        const auto carved=engine::execute_editor_operation(terrain_ctx,"terrain_apply",
            R"({"action":"carve_channel","points":[{"x":-4,"z":2},{"x":0,"z":0},{"x":4,"z":-2}],"halfWidth":2.5,"bedDepth":1.2,"bankClearance":1.0,"step":2,"strength":1})");
        r.check(carved.exit_code==engine::ExitCode::Success,"terrain_apply carve_channel succeeds");
        engine::set_active_terrain_edits(nullptr);
        engine::set_active_water_store(nullptr);
    }else if(suite=="world_influence"){
        engine::WorldInfluenceBus bus;
        const auto empty_dominant=bus.dominant_at(0.0f,0.0f);
        r.check(empty_dominant.position[0]==0.0f&&empty_dominant.position[2]==0.0f,"empty influence bus returns zero dominant");
        engine::WorldInfluenceSource near_source;
        near_source.position={1.0f,0.0f,1.0f};
        near_source.velocity={0.5f,0.0f,0.0f};
        near_source.radius=2.0f;
        near_source.strength=0.8f;
        near_source.kind="character";
        bus.add(std::move(near_source));
        engine::WorldInfluenceSource far_source;
        far_source.position={40.0f,0.0f,40.0f};
        far_source.velocity={0.0f,0.0f,0.0f};
        far_source.radius=3.0f;
        far_source.strength=0.2f;
        far_source.kind="character";
        bus.add(std::move(far_source));
        const auto dominant=bus.dominant_at(1.5f,1.5f);
        r.check(dominant.position[0]==1.0f&&dominant.position[2]==1.0f,"dominant influence picks nearest source on XZ");
        r.check(dominant.strength==0.8f,"dominant influence preserves source strength");
        bus.clear();
        r.check(bus.empty(),"influence bus clear removes sources");
    }else if(suite=="foliage"){
        engine::FoliageLayerPalette palette;
        palette.schema_version=1;
        palette.layers={{"grass","Grass","grass_blade",{0.14f,0.22f,0.10f},0.55f,1.0f,0.15f,0.55f,0.35f,1.2f,0.55f,"grass_walk"},
            {"flower","Flower","flower_clump",{0.62f,0.28f,0.48f},0.45f,0.85f,0.03f,0.45f,0.1f,0.9f,0.45f,""}};
        r.check(palette.validate().has_value(),"foliage layer palette validates");
        const auto palette_round_trip=engine::FoliageLayerPalette::from_json(palette.to_json());
        r.check(palette_round_trip.has_value(),"foliage layer palette round trip");
        r.check(palette_round_trip.value().layers[0].mesh_kind=="grass_blade","foliage palette round trip keeps grass_blade mesh");
        r.check(palette_round_trip.value().layers[0].bend_strength==0.35f,"foliage palette round trip keeps bendStrength");
        r.check(palette_round_trip.value().layers[0].disturb_vfx_id=="grass_walk","foliage palette round trip keeps disturbVfxId");
        r.check(!engine::FoliageLayerPalette::from_json("{\"schemaVersion\":2}"),"invalid foliage layer schema rejected");
        engine::FoliageDensityStore density;
        const auto brushed=density.apply_foliage_brush(0.0f,0.0f,6.0f,0.2f,0,false);
        r.check(brushed&&brushed.value().count({0,0})==1,"foliage brush touches origin cell");
        const auto density_round_trip=engine::FoliageDensityStore::from_json(density.to_json());
        r.check(density_round_trip.has_value(),"foliage density round trip");
        r.check(!engine::FoliageDensityStore::from_json("{\"schemaVersion\":2}"),"invalid foliage density schema rejected");
        engine::FoliageDensityHistory history;
        const auto before=density.cell_snapshot_or_empty({0,0});
        const auto erased=density.apply_foliage_brush(0.0f,0.0f,6.0f,1.0f,0,true);
        r.check(erased&&erased.value().count({0,0})<=1,"foliage erase brush succeeds");
        std::map<engine::CellCoord,engine::FoliageCellSnapshot> before_map;
        before_map[{0,0}]=before;
        std::map<engine::CellCoord,engine::FoliageCellSnapshot> after;
        after[{0,0}]=density.cell_snapshot_or_empty({0,0});
        r.check(history.execute(density,std::make_unique<engine::FoliageDensityBrushStrokeCommand>(std::move(before_map),std::move(after))).has_value(),"foliage brush command executes");
        r.check(history.undo(density).has_value(),"foliage brush command undoes");
        const std::set<engine::CellCoord> cells{{0,0}};
        const std::array<float,3> camera{0.0f,8.0f,0.0f};
        const auto first=engine::scatter_foliage_cells(cells,density,palette,{},camera);
        const auto second=engine::scatter_foliage_cells(cells,density,palette,{},camera);
        r.check(first.size()==second.size(),"foliage scatter map size stable");
        bool scatter_matches=true;
        for(const auto& entry:first){
            const auto found=second.find(entry.first);
            if(found==second.end()||found->second.size()!=entry.second.size()){scatter_matches=false;break;}
            for(std::size_t index=0;index<entry.second.size();++index){
                if(entry.second[index].model!=found->second[index].model){scatter_matches=false;break;}
            }
            if(!scatter_matches) break;
        }
        r.check(scatter_matches,"foliage scatter is deterministic");
        engine::FoliageDensityStore heavy;
        (void)heavy.apply_foliage_brush(0.0f,0.0f,40.0f,1.0f,0,false);
        const auto heavy_instances=engine::scatter_foliage_cell({0,0},heavy,palette,{},camera);
        r.check(heavy_instances.size()<=engine::FoliageScatterConfig::k_max_instances_per_cell,"foliage scatter respects per-cell budget");
        engine::FoliageLayerPalette steep_palette=palette;
        steep_palette.layers[0].max_slope_ratio=0.0001f;
        const auto steep=engine::scatter_foliage_cell({0,0},density,steep_palette,{},camera);
        const auto permissive=engine::scatter_foliage_cell({0,0},density,palette,{},camera);
        r.check(steep.size()<=permissive.size(),"foliage scatter rejects steep slopes");
        engine::CollisionWorld world;
        engine::StreamedTerrainField terrain_field;
        r.check(terrain_field.update(world,{0.0f,8.0f,0.0f},{}).has_value(),"foliage stream terrain update");
        engine::StreamedFoliageField foliage_field;
        foliage_field.set_palette(&palette);
        foliage_field.set_density(&density);
        r.check(foliage_field.sync(terrain_field,camera).has_value(),"streamed foliage syncs with terrain");
        r.check(!foliage_field.batches().empty(),"streamed foliage builds instance batches");
        const auto meshes=engine::build_foliage_layer_meshes(palette);
        r.check(meshes.size()==2,"foliage layer meshes generated for grass and flower");
        const auto blade_mesh=std::find_if(meshes.begin(),meshes.end(),[](const auto& entry){return entry.first.find("grass_blade")!=std::string::npos;});
        r.check(blade_mesh!=meshes.end()&&blade_mesh->second.vertices.size()>=3,"grass_blade mesh is non-empty");
        const auto sample_palette=engine::FoliageLayerPalette::load("samples/open-world-rpg/assets/foliage/ground-cover.layers.json");
        r.check(sample_palette.has_value(),"sample ground-cover.layers.json loads");
        r.check(sample_palette.value().layers[0].mesh_kind=="grass_blade","sample grass layer uses grass_blade");
        r.check(sample_palette.value().layers.size()>=5,"sample palette includes bush layers");
        const auto bush_layer=std::find_if(sample_palette.value().layers.begin(),sample_palette.value().layers.end(),
            [](const engine::FoliageLayerDefinition& layer){return layer.id=="bush";});
        r.check(bush_layer!=sample_palette.value().layers.end()&&bush_layer->scatter_mode==engine::FoliageScatterMode::Discrete,
            "sample bush layer uses discrete scatter");
        engine::FoliageLayerPalette discrete_palette;
        discrete_palette.layers={{"bush","Bush","bush",{0.14f,0.22f,0.10f},0.85f,1.15f,1.0f,0.5f,0.08f,1.4f,0.95f,"",
            engine::FoliageScatterMode::Discrete,static_cast<std::uint8_t>(56)}};
        engine::FoliageDensityStore light_bush;
        (void)light_bush.apply_foliage_brush(0.0f,0.0f,8.0f,0.2f,0,false);
        engine::FoliageDensityStore strong_bush;
        (void)strong_bush.apply_foliage_brush(0.0f,0.0f,8.0f,0.9f,0,false);
        const auto light_instances=engine::scatter_foliage_cell({0,0},light_bush,discrete_palette,{},camera);
        const auto strong_instances=engine::scatter_foliage_cell({0,0},strong_bush,discrete_palette,{},camera);
        r.check(light_instances.empty(),"light discrete bush paint below threshold produces no bushes");
        r.check(!strong_instances.empty(),"strong discrete bush paint above threshold produces bushes");
        const auto bush_mesh=engine::generate_primitive_mesh("bush",{0.14f,0.22f,0.10f});
        r.check(bush_mesh.has_value()&&bush_mesh.value().vertices.size()>=12,"bush primitive mesh is non-empty");
        engine::FoliageLayerPalette mix_palette;
        mix_palette.layers={{"grass","Grass","grass_blade",{0.14f,0.22f,0.10f},0.55f,1.0f,0.15f,0.55f},
            {"flower","Flower","flower_clump",{0.62f,0.28f,0.48f},0.45f,0.85f,0.03f,0.45f},
            {"bush","Bush","bush",{0.14f,0.22f,0.10f},0.85f,1.15f,1.0f,0.5f,0.08f,1.4f,0.95f,"",
                engine::FoliageScatterMode::Discrete,static_cast<std::uint8_t>(72)}};
        const auto mix_weights=engine::default_meadow_mix_weights(mix_palette);
        r.check(mix_weights.size()>=3,"meadow mix resolves grass flower and bush weights");
        engine::FoliageDensityStore mixed_density;
        (void)mixed_density.apply_foliage_mixed_brush(0.0f,0.0f,10.0f,0.55f,mix_weights,false);
        const auto* mixed_cell=mixed_density.find_cell({0,0});
        r.check(mixed_cell,"mixed foliage brush touches origin cell");
        std::set<std::uint8_t> mixed_layers;
        if(mixed_cell){
            for(std::size_t index=0;index<mixed_cell->density.size();++index){
                if(mixed_cell->density[index]>0) mixed_layers.insert(mixed_cell->layer[index]);
            }
        }
        r.check(mixed_layers.size()>=2,"mixed foliage brush assigns multiple layer types");
        engine::FoliageDensityStore erase_density=mixed_density;
        (void)erase_density.apply_foliage_brush(0.0f,0.0f,10.0f,0.55f,0,true);
        const auto* erased_cell=erase_density.find_cell({0,0});
        r.check(!erased_cell||std::all_of(erased_cell->density.begin(),erased_cell->density.end(),
            [](std::uint8_t value){return value==0;}),"foliage erase brush clears painted density");
    }else if(suite=="collision"){
        engine::CollisionWorld world;
        r.check(!world.add_box({0,0,0},{0,1,1},engine::CollisionLayer::StaticWorld,false),"invalid box rejected");
        auto floor=world.add_box({0,-1,0},{10,1,10},engine::CollisionLayer::StaticWorld,false,engine::CellCoord{0,0});
        r.check(floor&&world.body_count()==1,"static body created");
        auto hit=world.ray_cast({0,5,0},{0,-10,0});
        r.check(hit&&hit.value().has_value(),"ray hits floor");
        auto sphere=world.add_sphere({0,5,0},0.5f,engine::CollisionLayer::Dynamic,true);
        r.check(sphere.has_value(),"dynamic sphere created");
        auto before=world.position(sphere.value());
        for(int i=0;i<30;++i) r.check(world.step(1.0f/60.0f).has_value(),"physics step succeeds");
        auto after=world.position(sphere.value());
        r.check(before&&after&&after.value().y<before.value().y,"dynamic body falls");
        auto overlap=world.overlap_sphere({0,0,0},2.0f);
        r.check(overlap&&overlap.value().size()>=1,"overlap sphere hits floor");
        auto miss=world.overlap_sphere({0,50,0},0.5f);
        r.check(miss&&miss.value().empty(),"overlap sphere misses when empty");
        engine::CollisionQueryFilter static_filter; static_filter.layer=engine::CollisionLayer::StaticWorld;
        auto filtered=world.overlap_sphere({0,0,0},15.0f,static_filter);
        r.check(filtered&&std::any_of(filtered.value().begin(),filtered.value().end(),[](const engine::OverlapHit& h){return h.layer==engine::CollisionLayer::StaticWorld;}),"overlap layer filter keeps static floor");
        engine::CollisionQueryFilter trigger_filter; trigger_filter.layer=engine::CollisionLayer::Trigger;
        auto trigger_miss=world.overlap_sphere({0,0,0},15.0f,trigger_filter);
        r.check(trigger_miss&&trigger_miss.value().empty(),"overlap layer filter excludes non-trigger bodies");
        r.check(!world.overlap_sphere({0,0,0},0.0f),"invalid overlap radius rejected");
        auto box_overlap=world.overlap_box({0,0,0},{2,2,2});
        r.check(box_overlap&&box_overlap.value().size()>=1,"overlap box hits floor");
        auto box_miss=world.overlap_box({0,50,0},{0.5f,0.5f,0.5f});
        r.check(box_miss&&box_miss.value().empty(),"overlap box misses when empty");
        auto box_filtered=world.overlap_box({0,0,0},{15,15,15},static_filter);
        r.check(box_filtered&&std::any_of(box_filtered.value().begin(),box_filtered.value().end(),[](const engine::OverlapHit& h){return h.layer==engine::CollisionLayer::StaticWorld;}),"overlap box layer filter keeps static floor");
        r.check(!world.overlap_box({0,0,0},{0,1,1}),"invalid overlap box rejected");
        auto sweep=world.sweep_sphere({0,5,0},{0,-10,0},0.25f);
        r.check(sweep&&sweep.value().has_value()&&sweep.value()->fraction>0.0f&&sweep.value()->fraction<=1.0f,"swept sphere hits floor");
        auto sweep_miss=world.sweep_sphere({0,50,0},{0,10,0},0.25f);
        r.check(sweep_miss&&!sweep_miss.value().has_value(),"swept sphere misses upward cast");
        auto sweep_filtered=world.sweep_sphere({0,5,0},{0,-10,0},0.25f,trigger_filter);
        r.check(sweep_filtered&&!sweep_filtered.value().has_value(),"swept sphere layer filter excludes static floor");
        r.check(!world.sweep_sphere({0,0,0},{0,-1,0},0.0f),"invalid swept sphere radius rejected");
        r.check(world.remove(sphere.value()).has_value(),"dynamic body removed");
        auto trigger=world.add_box({0,2,0},{2,2,2},engine::CollisionLayer::Trigger,false,engine::CellCoord{1,0});
        r.check(trigger.has_value(),"trigger volume created");
        auto mover=world.add_sphere({0,10,0},0.5f,engine::CollisionLayer::Dynamic,true);
        r.check(mover.has_value(),"trigger test mover created");
        bool saw_enter=false,saw_exit=false;
        for(int i=0;i<240;++i){
            r.check(world.step(1.0f/60.0f).has_value(),"trigger test physics step");
            for(const auto& event:world.drain_contact_events()){
                if(event.layer_a!=engine::CollisionLayer::Trigger&&event.layer_b!=engine::CollisionLayer::Trigger) continue;
                if(event.type==engine::ContactEventType::Enter) saw_enter=true;
                if(event.type==engine::ContactEventType::Exit) saw_exit=true;
            }
        }
        r.check(saw_enter,"object enters trigger");
        r.check(saw_exit,"object exits trigger");
        auto unload_trigger=world.add_box({20,2,0},{1,1,1},engine::CollisionLayer::Trigger,false,engine::CellCoord{2,0});
        auto inside=world.add_sphere({20,2,0},0.25f,engine::CollisionLayer::Dynamic,true);
        for(int i=0;i<10;++i){r.check(world.step(1.0f/60.0f).has_value(),"trigger unload warm-up step");(void)world.drain_contact_events();}
        world.unload_cell({2,0});
        auto unload_events=world.drain_contact_events();
        r.check(std::any_of(unload_events.begin(),unload_events.end(),[](const engine::ContactEvent& e){return e.type==engine::ContactEventType::Exit;}),"trigger unloads with cell");
        (void)world.remove(inside.value());
        (void)world.remove(mover.value());
        world.unload_cell({1,0});
        world.unload_cell({0,0});
        r.check(world.body_count()==0,"cell unload removes bodies");
        r.check(!world.step(0),"invalid step rejected");
        auto terrain=engine::generate_stylized_terrain({0,0},33,40);
        auto terrain_body=terrain?world.add_heightfield(terrain.value(),{},engine::CellCoord{0,0}):engine::Result<engine::CollisionBody>::failure(engine::EngineError{});
        r.check(terrain_body.has_value(),"heightfield collision created");
        auto terrain_hit=world.ray_cast({5,20,5},{0,-40,0});
        r.check(terrain_hit&&terrain_hit.value().has_value(),"ray hits generated terrain");
        world.unload_cell({0,0});
        r.check(world.body_count()==0,"heightfield unloads with cell");
        engine::PhysicalMaterialProperties invalid; invalid.friction=-1;
        r.check(terrain&&!world.add_heightfield(terrain.value(),invalid),"invalid physical material rejected");
        (void)world.add_box({0,-1,0},{1,1,1},engine::CollisionLayer::StaticWorld,false);
        engine::PrefabAsset prefab_trigger;
        prefab_trigger.schema_version=2;
        engine::PrefabCollisionVolume trigger_volume;
        trigger_volume.shape=engine::PrefabCollisionShape::Sphere;
        trigger_volume.trigger=true;
        trigger_volume.layer=engine::CollisionLayer::Trigger;
        trigger_volume.radius=1.0f;
        trigger_volume.transform.position={0.0f,0.5f,0.0f};
        prefab_trigger.collision.push_back(trigger_volume);
        engine::TransformComponent placement;
        placement.position={10.0f,2.0f,10.0f};
        auto prefab_bodies=engine::spawn_prefab_collision(world,prefab_trigger,placement,engine::CellCoord{0,0});
        r.check(prefab_bodies&&prefab_bodies.value().size()==1,"prefab trigger collision spawns");
        auto prefab_mover=world.add_sphere({10.0f,10.0f,10.0f},0.5f,engine::CollisionLayer::Dynamic,true);
        r.check(prefab_mover.has_value(),"prefab trigger test mover created");
        bool prefab_enter=false,prefab_exit=false;
        for(int i=0;i<240;++i){
            r.check(world.step(1.0f/60.0f).has_value(),"prefab trigger physics step");
            for(const auto& event:world.drain_contact_events()){
                if(event.layer_a!=engine::CollisionLayer::Trigger&&event.layer_b!=engine::CollisionLayer::Trigger) continue;
                if(event.type==engine::ContactEventType::Enter) prefab_enter=true;
                if(event.type==engine::ContactEventType::Exit) prefab_exit=true;
            }
        }
        r.check(prefab_enter,"prefab trigger reports enter");
        r.check(prefab_exit,"prefab trigger reports exit");
        (void)world.remove(prefab_mover.value());
        auto prefab_inside=world.add_sphere({10.0f,2.5f,10.0f},0.25f,engine::CollisionLayer::Dynamic,true);
        r.check(prefab_inside.has_value(),"prefab trigger unload probe created");
        for(int i=0;i<10;++i){r.check(world.step(1.0f/60.0f).has_value(),"prefab trigger unload warm-up step");(void)world.drain_contact_events();}
        world.unload_cell({0,0});
        auto prefab_unload_events=world.drain_contact_events();
        r.check(std::any_of(prefab_unload_events.begin(),prefab_unload_events.end(),[](const engine::ContactEvent& e){return e.type==engine::ContactEventType::Exit;}),"prefab trigger unloads with cell");
        (void)world.remove(prefab_inside.value());
        r.check(!world.debug_bodies().empty(),"collision debug bodies available");

        // TICKET-0197: CollisionBodySettings + PlacementCollisionTracker Rigidbody path
        {
            engine::CollisionWorld settings_world;
            auto settings_floor=settings_world.add_box({0,-1,0},{20,1,20},engine::CollisionLayer::StaticWorld,false);
            r.check(settings_floor.has_value(),"0197 settings floor");
            auto dyn_settings=engine::CollisionBodySettings::make_dynamic();
            dyn_settings.mass=2.0f;
            dyn_settings.linear_damping=0.1f;
            auto falling=settings_world.add_box({0,8,0},{0.5f,0.5f,0.5f},engine::CollisionLayer::Dynamic,dyn_settings);
            r.check(falling.has_value(),"0197 dynamic settings body");
            const auto y0=settings_world.position(falling.value()).value().y;
            for(int i=0;i<120;++i) r.check(settings_world.step(1.0f/60.0f).has_value(),"0197 fall step");
            const auto y1=settings_world.position(falling.value()).value().y;
            r.check(y1<y0-1.0,"0197 dynamic settings body falls");
            r.check(y1>-0.5,"0197 dynamic settings body rests above floor");
            auto kin_settings=engine::CollisionBodySettings::make_kinematic();
            auto kinematic=settings_world.add_box({5,8,0},{0.5f,0.5f,0.5f},engine::CollisionLayer::Dynamic,kin_settings);
            r.check(kinematic.has_value(),"0197 kinematic settings body");
            const auto ky0=settings_world.position(kinematic.value()).value().y;
            for(int i=0;i<60;++i) r.check(settings_world.step(1.0f/60.0f).has_value(),"0197 kinematic step");
            const auto ky1=settings_world.position(kinematic.value()).value().y;
            r.check(std::abs(ky1-ky0)<0.01,"0197 kinematic does not fall");
            auto no_mass=engine::CollisionBodySettings::make_dynamic();
            no_mass.mass=0.0f;
            r.check(!settings_world.add_box({0,1,0},{0.5f,0.5f,0.5f},engine::CollisionLayer::Dynamic,no_mass),
                "0197 invalid mass rejected");
        }
        {
            engine::CollisionWorld track_world;
            auto track_floor=track_world.add_box({0,-1,0},{20,1,20},engine::CollisionLayer::StaticWorld,false,engine::CellCoord{0,0});
            r.check(track_floor.has_value(),"0197 tracker floor");
            engine::PrefabAsset crate;
            crate.schema_version=2;
            engine::PrefabCollisionVolume box;
            box.shape=engine::PrefabCollisionShape::Box;
            box.layer=engine::CollisionLayer::StaticWorld;
            box.half_extent={0.5f,0.5f,0.5f};
            crate.collision.push_back(box);
            engine::PrefabRigidbody rb;
            rb.id="rigidbody-0";
            rb.motion_type="dynamic";
            rb.mass=1.0f;
            crate.rigidbodies.push_back(rb);
            engine::Scene scene;
            engine::TransformComponent at;
            at.position={0.0f,6.0f,0.0f};
            auto placed=scene.place_world_object("crate","assets/prefabs/crate.prefab.json",at,std::nullopt,std::nullopt,&crate);
            r.check(placed.has_value(),"0197 place rigidbody prefab");
            std::map<std::string,engine::PrefabAsset> catalog;
            catalog["assets/prefabs/crate.prefab.json"]=crate;
            engine::PlacementCollisionTracker tracker;
            r.check(tracker.sync(track_world,scene,catalog,true).has_value(),"0197 sync dynamic placement");
            r.check(track_world.body_count()>=2,"0197 motion + floor bodies");
            const auto y_before=scene.transform(placed.value())->position[1];
            for(int i=0;i<120;++i){
                r.check(track_world.step(1.0f/60.0f).has_value(),"0197 tracker physics step");
                tracker.write_back_transforms(scene,track_world);
                r.check(tracker.sync(track_world,scene,catalog,true).has_value(),"0197 sync after write-back");
            }
            const auto y_after=scene.transform(placed.value())->position[1];
            r.check(y_after<y_before-1.0f,"0197 write-back lowers entity");
            r.check(y_after>-0.6f,"0197 crate rests near floor");
            engine::PrefabAsset static_prefab;
            static_prefab.schema_version=2;
            static_prefab.collision.push_back(box);
            engine::TransformComponent static_at;
            static_at.position={10.0f,6.0f,0.0f};
            auto static_placed=scene.place_world_object("static","assets/prefabs/static.prefab.json",static_at,std::nullopt,std::nullopt,&static_prefab);
            catalog["assets/prefabs/static.prefab.json"]=static_prefab;
            r.check(static_placed.has_value()&&tracker.sync(track_world,scene,catalog,true).has_value(),"0197 sync static placement");
            const auto static_y0=scene.transform(static_placed.value())->position[1];
            for(int i=0;i<60;++i){
                r.check(track_world.step(1.0f/60.0f).has_value(),"0197 static step");
                tracker.write_back_transforms(scene,track_world);
            }
            r.check(std::abs(scene.transform(static_placed.value())->position[1]-static_y0)<0.001f,
                "0197 no-Rigidbody placement stays put");
            const auto count_before_unload=track_world.body_count();
            track_world.unload_cell({0,0});
            r.check(track_world.body_count()<count_before_unload,"0197 unload removes cell bodies");
        }
    }else if(suite=="navigation"){
        auto grid=engine::build_navigation_grid({0,0});
        r.check(grid.has_value()&&grid.value().resolution==33&&grid.value().cell_size==128.0f,"navigation grid builds for partition cell");
        std::size_t walkable=0;for(const auto sample:grid.value().walkable) if(sample) ++walkable;
        r.check(walkable>0,"navigation grid marks walkable samples");
        auto nearest=grid.value().nearest_walkable(32.0f,32.0f,20.0f);
        r.check(nearest.has_value(),"nearest walkable point found");
        r.check(grid.value().line_of_walk(16.0f,16.0f,48.0f,48.0f),"line-of-walk succeeds on local flat path");
        r.check(!engine::build_navigation_grid({0,0},128.0f,16),"invalid navigation resolution rejected");
        engine::StreamedNavigationField field;
        r.check(field.update({64.0f,0.0f,64.0f},1).has_value()&&field.loaded_cell_count()==9,"streamed navigation loads neighborhood");
        r.check(field.contains({0,0}),"streamed navigation keeps origin cell");
        r.check(field.update({200.0f,0.0f,64.0f},1).has_value(),"streamed navigation recenters");
        r.check(!field.contains({0,0})||field.focus_cell().x!=0,"streamed navigation unloads distant cells");
        auto walk=field.line_of_walk({64.0,0.0,64.0},{80.0,0.0,80.0});
        r.check(walk.has_value()&&walk.value(),"loaded cell line-of-walk query succeeds");
        auto cross=field.line_of_walk({64.0,0.0,64.0},{150.0,0.0,64.0});
        r.check(cross.has_value(),"cross-cell line-of-walk succeeds across loaded cells");
        r.check(!field.line_of_walk({64.0,0.0,64.0},{300.0,0.0,300.0}),"line-of-walk fails when cells are not loaded");
        r.check(!field.nearest_walkable_point({9999.0,0.0,9999.0},10.0f),"out-of-world nearest walkable rejected");
    }else if(suite=="character"){
        engine::CollisionWorld world;
        engine::CharacterControllerConfig config;
        config.max_speed=4.0f;
        auto terrain=engine::generate_stylized_terrain({0,0},33,40);
        r.check(terrain.has_value(),"character test terrain generated");
        auto terrain_body=terrain?world.add_heightfield(terrain.value(),{},engine::CellCoord{0,0}):engine::Result<engine::CollisionBody>::failure(engine::EngineError{});
        r.check(terrain_body.has_value(),"character test terrain collision loaded");
        const float ground=engine::sample_terrain_height(8.0f,8.0f);
        auto created=engine::CharacterController::create(world,{8.0,ground+5.0,8.0},config);
        r.check(created.has_value(),"character controller created");
        auto character=std::move(created.value());
        bool landed=false;
        for(int i=0;i<180;++i){
            r.check(character.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"character gravity step");
            if(character.on_ground()){landed=true;break;}
        }
        r.check(landed,"character lands on terrain");
        const auto start=character.position();
        for(int i=0;i<120;++i) r.check(character.move({0,0,config.max_speed},0.0f,1.0f/60.0f).has_value(),"character walk step");
        const auto moved=character.position();
        r.check(std::abs(moved.z-start.z)>0.5,"character walks forward on terrain");
        engine::WorldPartition partition;
        const auto owner=character.owner_cell(partition);
        r.check(owner.x==0&&owner.z==0,"character owner cell derived from position");
        auto steep=world.add_box({12,ground+0.5,12},{2,0.5f,2},engine::CollisionLayer::StaticWorld,false,engine::CellCoord{0,0});
        r.check(steep.has_value(),"steep ramp created for slope test");
        character.set_position({12.0,ground+3.0,10.0});
        const auto before_ramp=character.position();
        for(int i=0;i<120;++i) r.check(character.move({0,0,config.max_speed},0.0f,1.0f/60.0f).has_value(),"character ramp push step");
        r.check(character.on_steep_ground()||character.position().y-before_ramp.y<1.5,"character slope limit blocks steep climb");
        engine::CharacterControllerConfig invalid_config; invalid_config.capsule_radius=0.0f;
        r.check(!engine::CharacterController::create(world,{0,0,0},invalid_config),"invalid character config rejected");
        engine::StreamedTerrainField field;
        r.check(field.update(world,{8.0f,12.0f,8.0f},{}).has_value(),"character terrain stream update");
        r.check(character.move({0,0,1.0f},0.0f,1.0f/60.0f).has_value(),"character moves with streamed terrain resident");
        const auto normalized_start=character.position();
        for(int i=0;i<120;++i) r.check(character.move({0,0,1.0f},0.0f,1.0f/60.0f).has_value(),"normalized wish uses max speed");
        const auto normalized_end=character.position();
        r.check(std::abs(normalized_end.z-normalized_start.z)>1.5,"normalized wish reaches configured max speed");
        const auto diagonal_start=character.position();
        for(int i=0;i<90;++i) r.check(character.move({1.0f,0,1.0f},0.0f,1.0f/60.0f).has_value(),"diagonal keyboard step");
        const auto diagonal_end=character.position();
        const float diagonal_distance=std::sqrt((diagonal_end.x-diagonal_start.x)*(diagonal_end.x-diagonal_start.x)+
            (diagonal_end.z-diagonal_start.z)*(diagonal_end.z-diagonal_start.z));
        const auto forward_start=character.position();
        for(int i=0;i<90;++i) r.check(character.move({0,0,1.0f},0.0f,1.0f/60.0f).has_value(),"forward keyboard step");
        const auto forward_end=character.position();
        const float forward_distance=std::abs(forward_end.z-forward_start.z);
        r.check(std::abs(diagonal_distance-forward_distance)<0.35f,"diagonal keyboard matches forward speed");
        world.unload_cell({5,5});
        r.check(character.move({0,0,1.0f},0.0f,1.0f/60.0f).has_value(),"character move survives unrelated cell unload");
        r.check(character.debug_body().shape==engine::CollisionDebugShape::Capsule,"character debug body reports capsule");
        r.check(std::abs(engine::character_facing_yaw_from_velocity(0.0f, 3.0f, 1.5f) - 0.0f) < 1e-5f,
            "facing forward uses +Z yaw 0");
        r.check(std::abs(engine::character_facing_yaw_from_velocity(3.0f, 0.0f, 0.0f) - 1.5707963f) < 1e-4f,
            "facing right uses +X yaw pi/2");
        r.check(std::abs(engine::character_facing_yaw_from_velocity(0.0f, 0.0f, 0.75f) - 0.75f) < 1e-5f,
            "facing keeps previous when idle");
        r.check(std::abs(engine::character_facing_yaw_from_wish(0.0f, 1.0f, 0.4f, 1.5f) - 0.4f) < 1e-5f,
            "wish forward faces camera yaw");
        r.check(std::abs(engine::character_facing_yaw_from_wish(1.0f, 0.0f, 0.0f, 0.0f) - 1.5707963f) < 1e-4f,
            "wish strafe-right faces +X");
        r.check(std::abs(engine::character_facing_yaw_from_wish(0.0f, 0.0f, 0.4f, 0.9f) - 0.9f) < 1e-5f,
            "wish idle keeps previous facing");
        r.check(std::abs(engine::character_facing_yaw_from_camera_look(0.45f, -10.0f, 0.0f, 0.0f, 0.0f) +
                             std::atan2(0.45f, 10.0f)) < 1e-4f,
            "look facing counters shoulder offset");
        // Build horizontal speed then release input — friction should stop the capsule quickly.
        for(int i=0;i<60;++i) r.check(character.move({0,0,1.0f},0.0f,1.0f/60.0f).has_value(),"friction wind-up step");
        for(int i=0;i<45;++i) r.check(character.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"friction brake step");
        {
            const auto stopped=character.linear_velocity();
            const float horiz=std::sqrt(stopped[0]*stopped[0]+stopped[2]*stopped[2]);
            r.check(horiz<0.35f,"ground friction stops idle slide");
        }
        for(int i=0;i<30;++i) r.check(character.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"character settle before jump");
        r.check(character.on_ground(),"character on ground before jump");
        const float before_jump=character.position().y;
        auto jump_result=character.jump();
        r.check(jump_result.has_value()&&jump_result.value(),"grounded jump accepted");
        r.check(character.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"jump applies on next move");
        const auto jump_velocity=character.linear_velocity();
        r.check(jump_velocity[1]>=config.jump_velocity-0.01f,"jump sets upward velocity");
        bool airborne=false;
        bool repeat_rejected=false;
        float peak_y=before_jump;
        for(int i=0;i<90;++i){
            peak_y=std::max(peak_y,static_cast<float>(character.position().y));
            if(!character.on_ground()){
                airborne=true;
                if(!repeat_rejected){
                    auto repeat_jump=character.jump();
                    r.check(repeat_jump.has_value()&&!repeat_jump.value(),"airborne jump rejected");
                    repeat_rejected=true;
                }
            }
            r.check(character.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"jump airborne step");
        }
        r.check(airborne,"jump leaves ground");
        r.check(repeat_rejected,"airborne jump rejection tested");
        r.check(peak_y>before_jump+0.1f,"jump increases height");
        engine::CharacterAsset invalid_jump_asset;
        invalid_jump_asset.jump_velocity=0.0f;
        r.check(!invalid_jump_asset.validate(),"invalid jump velocity rejected");
        engine::CharacterControllerConfig invalid_jump_config;
        invalid_jump_config.jump_velocity=0.0f;
        r.check(!engine::CharacterController::create(world,{8.0,ground+1.0,8.0},invalid_jump_config),"invalid jump config rejected");

        // TICKET-0198: RigidbodyLocomotion on dynamic capsule
        {
            engine::CollisionWorld loco_world;
            auto floor=loco_world.add_box({0,-1,0},{20,1,20},engine::CollisionLayer::StaticWorld,false);
            r.check(floor.has_value(),"0198 loco floor");
            auto settings=engine::CollisionBodySettings::make_dynamic();
            settings.mass=70.0f;
            settings.freeze_rotation=true;
            const float radius=0.35f;
            const float half=0.85f;
            const float center_y=half+radius+3.0f;
            auto capsule=loco_world.add_capsule({0.0,center_y,0.0},radius,half,engine::CollisionLayer::Dynamic,settings);
            r.check(capsule.has_value(),"0198 capsule body");
            engine::CharacterControllerConfig loco_cfg;
            loco_cfg.max_speed=6.0f;
            engine::RigidbodyLocomotion loco(loco_world,capsule.value(),loco_cfg,radius,half);
            bool landed=false;
            for(int i=0;i<240;++i){
                // Let gravity integrate without overwriting velocity every frame while airborne.
                r.check(loco_world.step(1.0f/60.0f).has_value(),"0198 land step");
                if(loco.on_ground()||loco.feet_position().y<0.15){landed=true;break;}
            }
            r.check(landed,"0198 locomotion lands");
            for(int i=0;i<10;++i){
                (void)loco.move({0,0,0},0.0f,1.0f/60.0f);
                r.check(loco_world.step(1.0f/60.0f).has_value(),"0198 settle step");
            }
            r.check(loco.on_ground(),"0198 locomotion reports ground");
            const auto feet_before=loco.feet_position();
            for(int i=0;i<90;++i){
                r.check(loco.move({0,0,1},0.0f,1.0f/60.0f).has_value(),"0198 walk move");
                r.check(loco_world.step(1.0f/60.0f).has_value(),"0198 walk step");
            }
            const auto feet_after=loco.feet_position();
            r.check(feet_after.z>feet_before.z+0.5,"0198 wish walks forward");
            for(int i=0;i<60;++i){
                r.check(loco.move({0,0,0},0.0f,1.0f/60.0f).has_value(),"0198 idle move");
                r.check(loco_world.step(1.0f/60.0f).has_value(),"0198 idle step");
            }
            const auto vel=loco.linear_velocity();
            const float hspeed=std::sqrt(vel[0]*vel[0]+vel[2]*vel[2]);
            r.check(hspeed<0.35f,"0198 idle stops without ice slide");
            const auto jumped=loco.jump();
            r.check(jumped&&jumped.value(),"0198 grounded jump queued");
            (void)loco.move({0,0,0},0.0f,1.0f/60.0f);
            r.check(loco_world.step(1.0f/60.0f).has_value(),"0198 jump step");
            r.check(loco.linear_velocity()[1]>0.5f,"0198 jump lifts");
        }
    }else if(suite=="camera"){
        engine::DebugCamera camera;r.check(!camera.set_perspective(0,1,0.1f,100),"invalid perspective rejected");r.check(camera.set_perspective(1.0f,16.0f/9.0f,0.1f,2000).has_value(),"perspective accepted");
        auto start=camera.position();camera.apply({1,0,0,0,0,false},1.0f/60.0f);r.check(camera.position()[2]>start[2],"forward input moves camera");
        camera.apply({0,0,0,0,100000,false},1.0f/60.0f);r.check(camera.pitch()>=-1.55334f,"pitch clamps");auto matrix=camera.view_projection();r.check(std::all_of(matrix.begin(),matrix.end(),[](float v){return std::isfinite(v);}),"matrix finite");
        engine::CollisionWorld world;
        (void)world.add_box({0,-1,0},{10,1,10},engine::CollisionLayer::StaticWorld,false);
        engine::OrbitCameraConfig orbit_config;
        orbit_config.default_distance=5.0f;
        orbit_config.pivot_height=1.6f;
        orbit_config.shoulder_offset=0.0f;
        orbit_config.default_pitch=0.0f;
        orbit_config.min_pitch=-0.2f;
        orbit_config.max_pitch=1.2f;
        engine::OrbitCamera orbit(orbit_config);
        r.check(orbit.set_perspective(1.0f,16.0f/9.0f,0.1f,2000).has_value(),"orbit perspective accepted");
        r.check(orbit.update({0,0,0},world).has_value(),"orbit update succeeds");
        r.check(std::abs(orbit.resolved_distance()-5.0f)<0.01f,"orbit keeps desired distance in open space");
        const auto open_pos=orbit.position();
        const float open_dist=std::sqrt(open_pos[0]*open_pos[0]+(open_pos[1]-1.6f)*(open_pos[1]-1.6f)+open_pos[2]*open_pos[2]);
        r.check(std::abs(open_dist-5.0f)<0.25f,"orbit eye sits near configured distance from pivot");
        {
            engine::OrbitCamera zoom(orbit_config);
            zoom.adjust_distance(1.5f);
            r.check(std::abs(zoom.desired_distance()-3.5f)<0.01f,"scroll zoom pulls desired distance closer");
            zoom.adjust_distance(-100.0f);
            r.check(std::abs(zoom.desired_distance()-zoom.config().max_distance)<0.01f,"scroll zoom clamps at max distance");
            zoom.adjust_distance(100.0f);
            r.check(std::abs(zoom.desired_distance()-zoom.config().min_distance)<0.01f,"scroll zoom clamps at min distance");
        }
        (void)world.add_box({0,2,-2.5f},{6,4,0.3f},engine::CollisionLayer::StaticWorld,false);
        r.check(orbit.update({0,0,0},world).has_value(),"orbit update with blocker succeeds");
        r.check(orbit.collision_shortened(),"orbit shortens when geometry blocks the ray");
        r.check(orbit.resolved_distance()<orbit.desired_distance(),"orbit resolved distance moves closer under collision");
        orbit.apply_look(100,0);
        r.check(orbit.yaw()!=0.0f,"orbit look input changes yaw");
        r.check(orbit.update({0,0,0},world).has_value(),"orbit update after look succeeds");
        const auto aim=orbit.forward();
        const auto eye=orbit.position();
        const float to_pivot_x=-eye[0];
        const float to_pivot_y=1.6f-eye[1];
        const float to_pivot_z=-eye[2];
        const float aim_len=std::sqrt(to_pivot_x*to_pivot_x+to_pivot_y*to_pivot_y+to_pivot_z*to_pivot_z);
        r.check(aim_len>0.001f,"orbit eye offset from pivot");
        const float dot=(aim[0]*to_pivot_x+aim[1]*to_pivot_y+aim[2]*to_pivot_z)/aim_len;
        r.check(dot>0.999f,"orbit forward aims at pivot center after yaw");
        // RPG over-the-shoulder: eye shifts right while still aiming at the character.
        engine::CollisionWorld open_world;
        (void)open_world.add_box({0,-1,0},{10,1,10},engine::CollisionLayer::StaticWorld,false);
        engine::OrbitCameraConfig shoulder_cfg=orbit_config;
        shoulder_cfg.shoulder_offset=0.5f;
        engine::OrbitCamera shoulder_cam(shoulder_cfg);
        r.check(shoulder_cam.set_perspective(1.0f,16.0f/9.0f,0.1f,2000).has_value(),"shoulder orbit perspective");
        r.check(shoulder_cam.update({0,0,0},open_world).has_value(),"shoulder orbit update");
        r.check(std::abs(shoulder_cam.position()[0]-0.5f)<0.05f,"shoulder offset shifts eye to camera-right");
        const auto shoulder_aim=shoulder_cam.forward();
        const auto shoulder_eye=shoulder_cam.position();
        const float sx=-shoulder_eye[0],sy=1.6f-shoulder_eye[1],sz=-shoulder_eye[2];
        const float sl=std::sqrt(sx*sx+sy*sy+sz*sz);
        r.check(sl>0.001f&&(shoulder_aim[0]*sx+shoulder_aim[1]*sy+shoulder_aim[2]*sz)/sl>0.999f,
            "shoulder camera still aims at pivot");
        shoulder_cam.set_orientation(0.0f,2.0f);
        r.check(shoulder_cam.pitch()<=shoulder_cfg.max_pitch+0.001f,"pitch clamps to maxPitch");
        shoulder_cam.set_orientation(0.0f,-2.0f);
        r.check(shoulder_cam.pitch()>=shoulder_cfg.min_pitch-0.001f,"pitch clamps to minPitch");
        auto orbit_matrix=orbit.view_projection();
        r.check(std::all_of(orbit_matrix.begin(),orbit_matrix.end(),[](float v){return std::isfinite(v);}),"orbit matrix finite");
        // New optional RPG fields round-trip through camera assets.
        engine::CameraAsset authored;
        authored.default_distance=10.5f;
        authored.min_distance=2.5f;
        authored.max_distance=18.0f;
        authored.shoulder_offset=0.45f;
        authored.default_pitch=0.32f;
        authored.min_pitch=-0.15f;
        authored.max_pitch=1.25f;
        auto reparsed=engine::CameraAsset::from_json(authored.to_json());
        r.check(reparsed&&std::abs(reparsed.value().shoulder_offset-0.45f)<0.001f&&
            std::abs(reparsed.value().default_pitch-0.32f)<0.001f,"camera asset RPG fields round-trip");
    }else if(suite=="interaction"){
        engine::InteractionVolumeRegistry registry;
        engine::InteractionOverlapTracker tracker;
        engine::CollisionWorld world;
        (void)world.add_box({0,-1,0},{10,1,10},engine::CollisionLayer::StaticWorld,false);
        auto plain=world.add_sphere({5,2,0},1.0f,engine::CollisionLayer::Trigger,false);
        auto interact=world.add_sphere({0,2,0},1.2f,engine::CollisionLayer::Trigger,false);
        r.check(plain&&interact,"interaction test volumes created");
        registry.bind(interact.value(),{"placement-1",0,"use_campfire"});
        auto outside=tracker.update("player",{0,2,8},0.5f,world,registry);
        r.check(outside.empty(),"outside probe has no interaction events");
        auto enter=tracker.update("player",{0,2,0},0.5f,world,registry);
        r.check(enter.size()==1&&enter[0].type==engine::InteractionEventType::Enter&&enter[0].interaction_id=="use_campfire","player enters interaction volume");
        auto steady=tracker.update("player",{0,2,0},0.5f,world,registry);
        r.check(steady.empty(),"steady overlap emits no duplicate enter");
        auto exit=tracker.update("player",{0,2,8},0.5f,world,registry);
        r.check(exit.size()==1&&exit[0].type==engine::InteractionEventType::Exit,"player exits interaction volume");
        engine::PrefabAsset prefab;prefab.schema_version=2;engine::PrefabCollisionVolume volume;volume.shape=engine::PrefabCollisionShape::Sphere;volume.interaction_id="use_chest";volume.radius=1.0f;volume.transform.position={0,1,0};prefab.collision.push_back(volume);
        engine::TransformComponent placement;placement.position={2,0,2};
        const auto spawned=engine::spawn_prefab_collision(world,prefab,placement,engine::CellCoord{0,0});
        r.check(spawned&&spawned.value().size()==1,"prefab interaction collision spawns");
        registry.clear();tracker.reset("player");
        registry.bind(spawned.value()[0],{"chest-placement",0,"use_chest"});
        auto chest_enter=tracker.update("player",{2,1.2,2},1.0f,world,registry);
        r.check(chest_enter.size()==1&&chest_enter[0].interaction_id=="use_chest","authored prefab interaction registers enter");
    }else if(suite=="combat"){
        engine::CombatVolumeRegistry registry;
        engine::CollisionWorld world;
        (void)world.add_box({0,-1,0},{10,1,10},engine::CollisionLayer::StaticWorld,false);
        auto hurt=world.add_sphere({0,2,0},1.0f,engine::CollisionLayer::Trigger,false);
        auto hit=world.add_sphere({0.5,2,0},0.8f,engine::CollisionLayer::Trigger,false);
        auto plain=world.add_sphere({5,2,0},1.0f,engine::CollisionLayer::Trigger,false);
        r.check(hurt&&hit&&plain,"combat test volumes created");
        registry.bind(hurt.value(),{"enemy-1",0,engine::CombatVolumeRole::Hurt,"body"});
        registry.bind(hit.value(),{"weapon-1",0,engine::CombatVolumeRole::Hit,"sword_slash"});
        auto miss=engine::query_combat_hits("player_attack",{0,2,8},0.5f,world,registry);
        r.check(miss.empty(),"attack probe misses distant hurt volume");
        auto contacts=engine::query_combat_hits("player_attack",{0,2,0},0.5f,world,registry);
        r.check(contacts.size()==1&&contacts[0].hurt_combat_id=="body","attack probe hits hurt volume");
        r.check(contacts[0].attacker_id=="player_attack","contact records attacker id");
        auto from_hit=engine::query_combat_hits_from_body("sword_slash",hit.value(),world,registry);
        r.check(from_hit.size()==1&&from_hit[0].hurt_combat_id=="body","hit body query finds overlapping hurt volume");
        auto non_hit=engine::query_combat_hits_from_body("sword_slash",plain.value(),world,registry);
        r.check(non_hit.empty(),"unregistered body query returns no contacts");
        engine::PrefabAsset prefab;prefab.schema_version=2;engine::PrefabCollisionVolume hurt_volume;hurt_volume.shape=engine::PrefabCollisionShape::Sphere;hurt_volume.combat_hurt_id="body";hurt_volume.radius=1.0f;hurt_volume.transform.position={0,1,0};prefab.collision.push_back(hurt_volume);
        engine::TransformComponent placement;placement.position={4,0,4};
        const auto spawned=engine::spawn_prefab_collision(world,prefab,placement,engine::CellCoord{0,0});
        r.check(spawned&&spawned.value().size()==1,"prefab hurt collision spawns");
        registry.clear();
        registry.bind(spawned.value()[0],{"dummy-enemy",0,engine::CombatVolumeRole::Hurt,"body"});
        auto prefab_hit=engine::query_combat_hits("player_attack",{4,1.2,4},0.5f,world,registry);
        r.check(prefab_hit.size()==1&&prefab_hit[0].hurt_placement_entity_id=="dummy-enemy","authored prefab hurt registers hit query");
    }else if(suite=="scripting"){
        const auto project=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/"samples"/"open-world-rpg";
        const auto bindings=engine::ScriptBindingsAsset::load(engine::default_script_bindings_path(project));
        r.check(bindings&&bindings.value().interactions.size()>=1,"script bindings load");
        const auto campfire=bindings.value().resolve_script_path("interaction","use_campfire");
        r.check(campfire&&campfire.value()=="assets/scripts/campfire_interaction.lua","resolve interaction binding to lua path");
        const auto hurt=bindings.value().resolve_script_path("combatHurt","body");
        r.check(hurt&&hurt.value()=="assets/scripts/combat_hurt.lua","resolve combatHurt binding to lua path");
        const auto handler=engine::resolve_script_binding_path(project,"handler","use_campfire");
        r.check(handler&&handler.value()=="assets/scripts/campfire_interaction.lua","handler kind searches all binding lists");
        r.check(!bindings.value().resolve_script_path("interaction","missing_id"),"missing binding id rejected");
        r.check(!bindings.value().resolve_script_path("bogus","use_campfire"),"unknown binding kind rejected");
        engine::LuaRuntime runtime;
        r.check(runtime.load_bindings(project,engine::default_script_bindings_path(project)).has_value(),"lua bindings load into runtime");
        const auto script=project/"assets"/"scripts"/"campfire_interaction.lua";
        r.check(runtime.validate_script(script).has_value(),"sample lua script validates");
        r.check(runtime.load_script(script).has_value(),"sample lua script loads");
        const auto invalid=std::filesystem::temp_directory_path()/"bad-script.lua";
        {
            std::ofstream out(invalid);
            out << "function bad(";
            out.close();
        }
        r.check(!runtime.validate_script(invalid),"invalid lua script rejected");
        engine::InteractionEvent event;event.type=engine::InteractionEventType::Enter;event.interaction_id="use_campfire";event.placement_entity_id="campfire-1";event.interactor_id="player";
        runtime.clear_recent_errors();
        runtime.blackboard_clear();
        runtime.dispatch_interaction(event);
        r.check(runtime.recent_errors().empty(),"interaction dispatch succeeds for bound handler");
        const auto last_id=runtime.blackboard_get("interaction.lastId");
        r.check(last_id&&last_id->type==engine::ScriptBlackboardType::String&&last_id->string_value=="use_campfire",
            "interaction handler sets blackboard lastId");
        const auto campfire_active=runtime.blackboard_get("interaction.campfireActive");
        r.check(campfire_active&&campfire_active->type==engine::ScriptBlackboardType::Bool&&campfire_active->bool_value,
            "interaction enter sets campfireActive true");

        engine::CombatContactEvent contact;contact.attacker_id="player";contact.hurt_placement_entity_id="target-1";contact.hurt_combat_id="body";
        runtime.clear_recent_errors();
        runtime.dispatch_combat_hit(contact);
        r.check(runtime.recent_errors().empty(),"combat hurt dispatch succeeds for bound handler");
        const auto last_hurt=runtime.blackboard_get("combat.lastHurtId");
        r.check(last_hurt&&last_hurt->type==engine::ScriptBlackboardType::String&&last_hurt->string_value=="body",
            "combat hurt handler sets blackboard lastHurtId");
        const auto hit_count=runtime.blackboard_get("combat.hitCount");
        r.check(hit_count&&hit_count->type==engine::ScriptBlackboardType::Number&&hit_count->number_value==1.0,
            "combat hurt handler increments hitCount");

        const auto host_probe=std::filesystem::temp_directory_path()/"engine-host-api-probe.lua";
        {
            std::ofstream out(host_probe);
            out << "function probe_host(payload_json)\n"
                   "  local ok, err = engine.json_decode(payload_json)\n"
                   "  if ok then\n"
                   "    engine.blackboard_set('probe.jsonOk', true)\n"
                   "    engine.blackboard_set('probe.id', tostring(ok.id))\n"
                   "  else\n"
                   "    engine.blackboard_set('probe.jsonOk', false)\n"
                   "    engine.blackboard_set('probe.err', tostring(err))\n"
                   "  end\n"
                   "  engine.log('not-a-level', 'should stay in sandbox')\n"
                   "  engine.blackboard_set('probe.logged', true)\n"
                   "end\n";
        }
        r.check(runtime.load_script(host_probe).has_value(),"host api probe script loads");
        runtime.clear_recent_errors();
        r.check(runtime.call_handler("probe_host", "{\"id\":\"alpha\"}").has_value(),"json_decode happy path via probe");
        const auto json_ok=runtime.blackboard_get("probe.jsonOk");
        r.check(json_ok&&json_ok->bool_value,"json_decode returns table");
        const auto probe_id=runtime.blackboard_get("probe.id");
        r.check(probe_id&&probe_id->string_value=="alpha","json_decode exposes object fields");
        r.check(runtime.call_handler("probe_host", "{bad").has_value(),"bad JSON fails closed without aborting VM");
        const auto json_bad=runtime.blackboard_get("probe.jsonOk");
        r.check(json_bad&&!json_bad->bool_value,"bad JSON sets jsonOk false");
        const auto logged=runtime.blackboard_get("probe.logged");
        r.check(logged&&logged->bool_value,"invalid log level does not abort handler");
        r.check(runtime.recent_errors().empty(),"host api probe leaves no scripting errors");

        engine::UiCanvasStack stack;
        r.check(stack.set_hud(engine::default_player_hud_path(project)).has_value(),"sample player hud loads for scripting suite");
        r.check(stack.register_canvas("pause",project/"assets"/"ui"/"pause.uicanvas.json").has_value(),
            "scripting suite registers pause");
        stack.hud().reset_player_health(100.0, 100.0);
        runtime.set_hud_runtime(&stack.hud());
        runtime.set_ui_canvas_stack(&stack);
        const auto health_probe=std::filesystem::temp_directory_path()/"engine-health-probe.lua";
        {
            std::ofstream out(health_probe);
            out << "function probe_health(payload_json)\n"
                   "  engine.set_health(80, 100)\n"
                   "  local c, m = engine.get_health()\n"
                   "  engine.blackboard_set('probe.hp', c)\n"
                   "  engine.blackboard_set('probe.max', m)\n"
                   "  engine.hud_set_number('player.health', 70)\n"
                   "  engine.hud_set_text('player.healthText', '70 / 100')\n"
                   "  engine.hud_set_visible('player_health', true)\n"
                   "  engine.hud_set_enabled('player_health', true)\n"
                   "  engine.hud_set_bool('settings.music', true)\n"
                   "  engine.blackboard_set('probe.music', engine.hud_get_bool('settings.music'))\n"
                   "end\n";
        }
        r.check(runtime.load_script(health_probe).has_value(),"health probe script loads");
        r.check(runtime.call_handler("probe_health", "{}").has_value(),"set_health / get_health succeed");
        const auto hp=runtime.blackboard_get("probe.hp");
        r.check(hp&&hp->number_value==80.0,"get_health returns current");
        r.check(stack.hud().get_number("player.health")&&*stack.hud().get_number("player.health")==70.0,"hud_set_number updates runtime");
        r.check(stack.hud().get_text("player.healthText")&&*stack.hud().get_text("player.healthText")=="70 / 100","hud_set_text updates runtime");
        const auto music=runtime.blackboard_get("probe.music");
        r.check(music&&music->bool_value,"hud_set_bool / hud_get_bool round-trip");
        r.check(stack.hud().get_bool("settings.music")&&*stack.hud().get_bool("settings.music"),
            "hud_set_bool updates runtime");
        const auto ui_probe=std::filesystem::temp_directory_path()/"engine-ui-stack-probe.lua";
        {
            std::ofstream out(ui_probe);
            out << "function probe_ui_stack(payload_json)\n"
                   "  engine.ui_push('pause')\n"
                   "  engine.blackboard_set('probe.top', engine.ui_top())\n"
                   "  engine.ui_pop()\n"
                   "  engine.blackboard_set('probe.empty', engine.ui_top() == nil)\n"
                   "end\n";
        }
        r.check(runtime.load_script(ui_probe).has_value(),"ui stack probe script loads");
        r.check(runtime.call_handler("probe_ui_stack", "{}").has_value(),"ui_push/ui_pop/ui_top succeed");
        const auto top=runtime.blackboard_get("probe.top");
        r.check(top&&top->string_value=="pause","ui_top returns pause after push");
        const auto empty=runtime.blackboard_get("probe.empty");
        r.check(empty&&empty->bool_value,"ui_top is nil after pop");
        runtime.set_hud_runtime(nullptr);
        runtime.set_ui_canvas_stack(nullptr);
    }else if(suite=="hud"){
        const auto project=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/"samples"/"open-world-rpg";
        const auto canvas=engine::UiCanvasAsset::load(engine::default_player_ui_canvas_path(project));
        r.check(canvas.has_value(),"sample player.uicanvas.json loads");
        r.check(canvas&&canvas.value().widgets.size()>=2,"sample canvas has widgets");
        r.check(canvas&&canvas.value().design_resolution[0]==1920.0f&&canvas.value().design_resolution[1]==1080.0f,
            "sample canvas designResolution is 1920x1080");
        const auto bad_design=engine::UiCanvasAsset::parse(
            R"({"schemaVersion":1,"id":"x","designResolution":[0,1080],"widgets":[]})", "bad.uicanvas.json");
        r.check(!bad_design,"zero designResolution rejected");
        const auto letterbox_full=engine::compute_ui_canvas_letterbox(0.0f,0.0f,1920.0f,1080.0f,1920.0f,1080.0f);
        r.check(std::abs(letterbox_full.scale-1.0f)<0.0001f,"letterbox scale is 1 at design size");
        r.check(std::abs(letterbox_full.content_min_x)<0.0001f&&std::abs(letterbox_full.content_min_y)<0.0001f,
            "letterbox fills viewport at design size");
        const auto letterbox_half=engine::compute_ui_canvas_letterbox(0.0f,0.0f,960.0f,540.0f,1920.0f,1080.0f);
        r.check(std::abs(letterbox_half.scale-0.5f)<0.0001f,"letterbox scale is 0.5 at half viewport");
        const auto letterbox_wide=engine::compute_ui_canvas_letterbox(0.0f,0.0f,1920.0f,500.0f,1920.0f,1080.0f);
        r.check(letterbox_wide.content_min_x>0.0f,"wide short viewport pillarboxes horizontally");
        const auto fill_wide=engine::compute_ui_canvas_layout(0.0f,0.0f,1920.0f,500.0f,1920.0f,1080.0f,
            engine::UiCanvasScaleMode::FillEdges);
        r.check(std::abs(fill_wide.scale-(1920.0f/1920.0f))<0.0001f||fill_wide.scale>=1.0f,
            "fill_edges uses cover scale on wide short viewport");
        r.check(fill_wide.content_min_y<0.0f||fill_wide.content_max_y>500.0f,
            "fill_edges content extends past short viewport");
        const auto scale_mode_canvas=engine::UiCanvasAsset::parse(
            R"({"schemaVersion":1,"id":"x","designResolution":[1920,1080],"scaleMode":"fill_edges","widgets":[]})",
            "scale.uicanvas.json");
        r.check(scale_mode_canvas.has_value()&&
            scale_mode_canvas.value().scale_mode==engine::UiCanvasScaleMode::FillEdges,
            "scaleMode fill_edges parses");
        const auto bad=engine::HudAsset::parse(R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"bar","size":[10,10]}]})", "bad.hud.json");
        r.check(!bad,"bar without bind rejected");
        const auto unknown=engine::HudAsset::parse(R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"gadget","size":[10,10]}]})", "bad.hud.json");
        r.check(!unknown,"unknown widget type rejected");
        const auto button=engine::HudAsset::parse(R"({"schemaVersion":1,"id":"x","widgets":[{"id":"b","type":"button","size":[10,10],"bind":"a"}]})", "ok.hud.json");
        r.check(button.has_value(),"button widget type loads");
        const auto toggle=engine::HudAsset::parse(
            R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"toggle","size":[80,24],"bind":"opt"}]})",
            "ok.hud.json");
        r.check(toggle.has_value(),"toggle widget type loads");
        const auto slider=engine::HudAsset::parse(
            R"({"schemaVersion":1,"id":"x","widgets":[{"id":"s","type":"slider","size":[100,16],"bind":"vol","maxBind":"volMax"}]})",
            "ok.hud.json");
        r.check(slider.has_value(),"slider widget type loads");
        const auto image_widget=engine::HudAsset::parse(
            R"({"schemaVersion":1,"id":"x","widgets":[{"id":"i","type":"image","size":[64,64],"image":"assets/ui/textures/x.png","imageMode":"contain"}]})",
            "ok.hud.json");
        r.check(image_widget.has_value()&&image_widget.value().widgets[0].image=="assets/ui/textures/x.png",
            "image widget parses path");
        r.check(image_widget&&image_widget.value().widgets[0].image_mode==engine::HudImageMode::Contain,
            "imageMode contain parses");
        const auto toggle_nobind=engine::HudAsset::parse(
            R"({"schemaVersion":1,"id":"x","widgets":[{"id":"t","type":"toggle","size":[80,24]}]})", "bad.hud.json");
        r.check(!toggle_nobind,"toggle without bind rejected");
        engine::HudRuntime runtime;
        r.check(runtime.load(engine::default_player_hud_path(project)).has_value(),"hud runtime loads sample canvas");
        r.check(runtime.asset().design_resolution[0]==1920.0f,"runtime keeps design resolution");
        runtime.set_health(40.0, 100.0);
        r.check(runtime.get_number("player.health")&&*runtime.get_number("player.health")==40.0,"set_health clamps current");
        runtime.set_health(200.0, 100.0);
        r.check(runtime.get_number("player.health")&&*runtime.get_number("player.health")==100.0,"set_health clamps to max");
        runtime.set_visible("player_health", false);
        r.check(!runtime.is_visible("player_health"),"hud_set_visible hides widget");
        runtime.set_bool("settings.music", true);
        r.check(runtime.get_bool("settings.music")&&*runtime.get_bool("settings.music"),"set_bool / get_bool");
        const auto written=engine::write_ui_canvas_json_atomic(
            std::filesystem::temp_directory_path()/"player-copy.uicanvas.json", canvas.value().to_json());
        r.check(written.has_value(),"ui canvas json writes atomically after validate");
        const auto legacy=engine::HudAsset::load(project/"assets"/"ui"/"player.hud.json");
        r.check(legacy.has_value(),"legacy player.hud.json shim still loads");

        engine::UiCanvasStack stack;
        r.check(stack.set_hud(engine::default_player_ui_canvas_path(project)).has_value(),"stack sets hud canvas");
        r.check(stack.register_canvas("pause",project/"assets"/"ui"/"pause.uicanvas.json").has_value(),
            "stack registers pause canvas");
        r.check(stack.register_canvas("settings",project/"assets"/"ui"/"settings.uicanvas.json").has_value(),
            "stack registers settings canvas");
        r.check(stack.register_canvas("inventory",project/"assets"/"ui"/"inventory.uicanvas.json").has_value(),
            "stack registers inventory canvas");
        r.check(stack.register_canvas("dialogue",project/"assets"/"ui"/"dialogue.uicanvas.json").has_value(),
            "stack registers dialogue canvas");
        r.check(stack.push("pause").has_value(),"stack push pause");
        r.check(stack.top_modal()&&*stack.top_modal()=="pause","stack top is pause");
        r.check(stack.push("pause").has_value(),"stack re-push pause succeeds");
        r.check(stack.modal_ids().size()==1,"duplicate push does not deepen stack");
        r.check(stack.pop().has_value(),"stack pop pause");
        r.check(!stack.has_modal(),"stack empty after pop");
        r.check(!stack.pop().has_value(),"pop empty stack fails");
        r.check(stack.show("pause").has_value(),"stack show pause");
        r.check(stack.hide("pause").has_value(),"stack hide pause");
        r.check(!stack.has_modal(),"stack empty after hide");
        r.check(stack.push("inventory").has_value(),"stack push inventory");
        r.check(stack.top_modal()&&*stack.top_modal()=="inventory","stack top is inventory");
        r.check(stack.pop().has_value(),"stack pop inventory");
        r.check(stack.push("dialogue").has_value(),"stack push dialogue");
        r.check(stack.top_modal()&&*stack.top_modal()=="dialogue","stack top is dialogue");
        r.check(stack.pop().has_value(),"stack pop dialogue");
        r.check(stack.push("settings").has_value(),"stack push settings");
        if(auto* settings=stack.find_canvas("settings")){
            const auto focus=settings->focusable_widget_ids();
            r.check(focus.size()==4,"settings has toggle+slider+back focusables");
            r.check(settings->get_bool("settings.music")&&!*settings->get_bool("settings.music"),
                "settings music toggle seeds false");
            r.check(settings->get_number("settings.volume")&&*settings->get_number("settings.volume")==0.0,
                "settings volume slider seeds 0");
            engine::UiCanvasInputEvent activate{};
            activate.viewport_min={0.0f,0.0f};
            activate.viewport_max={1920.0f,1080.0f};
            activate.activate_pressed=true;
            (void)stack.handle_modal_input(activate,nullptr);
            r.check(settings->get_bool("settings.music")&&*settings->get_bool("settings.music"),
                "activate flips settings music toggle");
            stack.reset_modal_focus();
            // Focus volume slider (third focusable after two toggles).
            engine::UiCanvasInputEvent nav_slider{};
            nav_slider.viewport_min={0.0f,0.0f};
            nav_slider.viewport_max={1920.0f,1080.0f};
            nav_slider.nav_next=true;
            (void)stack.handle_modal_input(nav_slider,nullptr);
            (void)stack.handle_modal_input(nav_slider,nullptr);
            engine::UiCanvasInputEvent adjust{};
            adjust.viewport_min={0.0f,0.0f};
            adjust.viewport_max={1920.0f,1080.0f};
            adjust.adjust_right=true;
            (void)stack.handle_modal_input(adjust,nullptr);
            r.check(settings->get_number("settings.volume")&&
                std::abs(*settings->get_number("settings.volume")-0.05)<0.001,
                "adjust_right nudges slider by 5%");
        }
        r.check(stack.pop().has_value(),"stack pop settings");

        auto mutate_canvas=engine::UiCanvasAsset::load(engine::default_player_ui_canvas_path(project));
        r.check(mutate_canvas.has_value(),"mutate suite loads canvas");
        if(mutate_canvas){
            const auto moved=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"move",
                R"({"id":"player_health","delta":[10,0]})");
            r.check(moved.has_value(),"mutate move succeeds");
            if(moved){
                bool found=false;
                for(const auto& widget:moved.value().widgets){
                    if(widget.id=="player_health"){
                        found=true;
                        r.check(std::abs(widget.offset[0]-58.0f)<0.01f,"mutate move offsets widget");
                    }
                }
                r.check(found,"mutate move targets player_health");
            }
            const auto styled=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"style",
                R"({"id":"player_health_text","color":[255,240,200,255],"fontSize":36,"opacity":0.5,"enabled":false,"text":"HP","textAlign":"center","textVAlign":"middle"})");
            r.check(styled.has_value(),"mutate style succeeds");
            if(styled){
                for(const auto& widget:styled.value().widgets){
                    if(widget.id=="player_health_text"){
                        r.check(widget.has_color()&&widget.color[0]==255.0f,"mutate style sets color");
                        r.check(widget.font_size==36.0f,"mutate style sets fontSize");
                        r.check(std::abs(widget.opacity-0.5f)<0.001f,"mutate style sets opacity");
                        r.check(!widget.enabled,"mutate style sets enabled false");
                        r.check(widget.text=="HP","mutate style sets text");
                        r.check(widget.text_align==engine::HudTextAlign::Center,"mutate style sets textAlign");
                        r.check(widget.text_v_align==engine::HudTextVAlign::Middle,"mutate style sets textVAlign");
                    }
                }
            }
            engine::HudRuntime pause_preview;
            r.check(pause_preview.load(project/"assets"/"ui"/"pause.uicanvas.json").has_value(),"pause canvas loads");
            r.check(pause_preview.get_text("pause.title").value_or("")=="PAUSED","pause text seeds from authored text");
            pause_preview.set_enabled("pause_title", false);
            r.check(!pause_preview.is_enabled("pause_title"),"hud set_enabled toggles inactive");
            engine::HudRuntime menu_preview;
            r.check(menu_preview.load(project/"assets"/"ui"/"main_menu.uicanvas.json").has_value(),"main menu canvas loads");
            r.check(menu_preview.get_text("main_menu.title").value_or("")=="Main Menu","main menu title seeds from text");
            r.check(menu_preview.get_text("main_menu.new_game").value_or("")=="New Game","main menu new_game text");
            r.check(!menu_preview.is_enabled("menu_continue"),"main menu continue is disabled");
            bool found_image=false;
            for(const auto& widget:menu_preview.asset().widgets){
                if(widget.id=="menu_new_game"&&!widget.image.empty()) found_image=true;
            }
            r.check(found_image,"main menu new_game has optional image path");
            engine::UiCanvasStack menu_stack;
            r.check(menu_stack.register_canvas("main_menu",project/"assets"/"ui"/"main_menu.uicanvas.json").has_value(),
                "main menu registers on stack");
            r.check(menu_stack.push("main_menu").has_value(),"main menu pushes on stack");
            r.check(menu_stack.top_modal()==std::optional<std::string>{"main_menu"},"main menu is stack top");
            const auto menu_focus=menu_preview.focusable_widget_ids();
            r.check(menu_focus.size()==3,"main menu has three enabled buttons");
            engine::UiCanvasStack pause_focus_stack;
            r.check(pause_focus_stack.register_canvas("pause",project/"assets"/"ui"/"pause.uicanvas.json").has_value(),
                "pause focus stack registers");
            r.check(pause_focus_stack.push("pause").has_value(),"pause focus stack pushes");
            const auto pause_focus=pause_focus_stack.find_canvas("pause")->focusable_widget_ids();
            r.check(pause_focus.size()==2,"pause has two focusable buttons");
            r.check(pause_focus[0]=="pause_resume","pause initial focus is resume");
            engine::UiCanvasInputEvent nav{};
            nav.viewport_min={0.0f,0.0f};
            nav.viewport_max={1920.0f,1080.0f};
            nav.nav_next=true;
            const auto nav_result=pause_focus_stack.handle_modal_input(nav,nullptr);
            r.check(nav_result.handled,"modal keyboard nav handles");
            r.check(pause_focus_stack.modal_focus_widget_id()&&
                *pause_focus_stack.modal_focus_widget_id()=="pause_quit","modal nav advances to quit");
            engine::UiCanvasInputEvent cancel{};
            cancel.viewport_min={0.0f,0.0f};
            cancel.viewport_max={1920.0f,1080.0f};
            cancel.cancel_pressed=true;
            const auto cancel_result=pause_focus_stack.handle_modal_input(cancel,nullptr);
            r.check(cancel_result.modal_popped,"modal cancel pops stack");
            r.check(cancel_result.canvas_id=="pause","modal cancel reports canvas id");
            r.check(!pause_focus_stack.has_modal(),"modal stack empty after cancel");
            const auto added=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"add",
                R"({"id":"temp_panel","type":"panel","offset":[8,8],"size":[64,64]})");
            r.check(added.has_value(),"mutate add succeeds");
            const auto added_button=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"add",
                R"({"id":"temp_btn","type":"button","text":"OK","bind":"test.ok","textAlign":"center"})");
            r.check(added_button.has_value(),"mutate add button succeeds");
            const auto added_toggle=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"add",
                R"({"id":"temp_toggle","type":"toggle","text":"On"})");
            r.check(added_toggle.has_value(),"mutate add toggle succeeds");
            const auto added_slider=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"add",
                R"({"id":"temp_slider","type":"slider","label":"Vol"})");
            r.check(added_slider.has_value(),"mutate add slider succeeds");
            const auto added_image=engine::mutate_ui_canvas_asset(mutate_canvas.value(),"add",
                R"({"id":"temp_img","type":"image","image":"assets/ui/textures/x.png","imageMode":"contain"})");
            r.check(added_image.has_value(),"mutate add image succeeds");
            if(added_image){
                for(const auto& widget:added_image.value().widgets){
                    if(widget.id=="temp_img"){
                        r.check(widget.image=="assets/ui/textures/x.png","mutate image sets path");
                        r.check(widget.image_mode==engine::HudImageMode::Contain,"mutate image sets mode");
                    }
                }
            }
            if(added){
                const auto removed=engine::mutate_ui_canvas_asset(added.value(),"remove",R"({"id":"temp_panel"})");
                r.check(removed.has_value()&&removed.value().widgets.size()==mutate_canvas.value().widgets.size(),
                    "mutate remove restores input count");
            }
        }
    }else if(suite=="automation"){
        const auto project=std::filesystem::path(ENGINE_REPOSITORY_ROOT)/"samples"/"open-world-rpg";
        const auto plan=engine::classify_scene_plan("place a tree in the scene","worlds/vertical-slice.world.json");
        r.check(plan.target_kind=="scene_data","scene plan classifies world placement");
        const auto lua_plan=engine::classify_scene_plan("update quest script","assets/scripts/quest.lua");
        r.check(lua_plan.target_kind=="lua_script","scene plan classifies lua edits");
        const auto hud_plan=engine::classify_scene_plan("resize health bar","assets/ui/player.hud.json");
        r.check(hud_plan.target_kind=="hud_asset","scene plan classifies hud assets");
        const auto canvas_plan=engine::classify_scene_plan("move health bar","assets/ui/player.uicanvas.json");
        r.check(canvas_plan.target_kind=="ui_canvas","scene plan classifies ui canvas assets");
        const auto wf_plan=engine::classify_scene_plan("update world forge factions",
            "assets/world-forge/factions.worldforge.json");
        r.check(wf_plan.target_kind=="world_forge","scene plan classifies world forge assets");
        const auto wf_get=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","get"},{"kind","factions"}});
        r.check(wf_get.exit_code==engine::ExitCode::Success&&wf_get.metadata.count("content"),
            "world_forge get factions succeeds");
        const auto wf_validate=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","validate"},{"kind","map"}});
        r.check(wf_validate.exit_code==engine::ExitCode::Success,"world_forge validate map succeeds");
        const auto wf_bad=engine::apply_world_forge_operation(project,
            nlohmann::json{{"action","apply"},{"kind","factions"},
                {"source",R"({"schemaVersion":1,"id":"t","entities":[{"id":"","kind":"faction","canonStatus":"draft"}]})"}});
        r.check(wf_bad.exit_code==engine::ExitCode::ValidationFailed,"world_forge apply rejects invalid factions");
        engine::LuaRuntime lua_call_runtime;
        engine::UiCanvasStack lua_call_stack;
        r.check(lua_call_runtime.load_bindings(project,engine::default_script_bindings_path(project)).has_value(),
            "lua_call suite loads bindings");
        r.check(lua_call_stack.set_hud(engine::default_player_hud_path(project)).has_value(),"lua_call suite loads hud");
        r.check(lua_call_stack.register_canvas("pause",project/"assets"/"ui"/"pause.uicanvas.json").has_value(),
            "lua_call suite registers pause");
        lua_call_stack.hud().reset_player_health(100.0,100.0);
        lua_call_runtime.set_hud_runtime(&lua_call_stack.hud());
        lua_call_runtime.set_ui_canvas_stack(&lua_call_stack);
        engine::EditorSessionContext lua_call_context;
        lua_call_context.project_root=project;
        lua_call_context.lua_runtime=&lua_call_runtime;
        lua_call_context.ui_canvas_stack=&lua_call_stack;
        const auto hurt_call=engine::execute_editor_operation(lua_call_context,"lua_call",
            R"({"kind":"combatHurt","id":"body"})");
        r.check(hurt_call.exit_code==engine::ExitCode::Success,"lua_call combatHurt succeeds");
        r.check(lua_call_stack.hud().get_number("player.health")&&*lua_call_stack.hud().get_number("player.health")==90.0,
            "lua_call combatHurt applies sample damage");
        const auto interact_call=engine::execute_editor_operation(lua_call_context,"lua_call",
            R"({"kind":"interaction","id":"use_campfire","type":"enter"})");
        r.check(interact_call.exit_code==engine::ExitCode::Success,"lua_call interaction succeeds");
        r.check(lua_call_stack.hud().get_number("player.health")&&*lua_call_stack.hud().get_number("player.health")==100.0,
            "lua_call campfire heal restores health");
        const auto push_call=engine::execute_editor_operation(lua_call_context,"ui_stack",
            R"({"action":"push","id":"pause"})");
        r.check(push_call.exit_code==engine::ExitCode::Success,"ui_stack push pause succeeds");
        r.check(lua_call_stack.top_modal()&&*lua_call_stack.top_modal()=="pause","ui_stack top is pause");
        const auto pop_call=engine::execute_editor_operation(lua_call_context,"ui_stack",
            R"({"action":"pop"})");
        r.check(pop_call.exit_code==engine::ExitCode::Success,"ui_stack pop succeeds");
        r.check(!lua_call_stack.has_modal(),"ui_stack empty after pop");
        {
            auto quests=engine::WorldForgeQuestsAsset::load(engine::default_world_forge_quests_path(project));
            r.check(quests.has_value(),"quest_call suite loads quests asset");
            engine::QuestRuntime quest_rt;
            if(quests) r.check(quest_rt.bind(&quests.value()).has_value(),"quest_call suite binds QuestRuntime");
            lua_call_context.quest_runtime=&quest_rt;
            lua_call_context.hud_runtime=&lua_call_stack.hud();
            lua_call_runtime.set_quest_runtime(&quest_rt);
            const auto q_start=engine::execute_editor_operation(lua_call_context,"quest_call",
                R"({"kind":"start","questId":"sq_01_cart_again"})");
            r.check(q_start.exit_code==engine::ExitCode::Success&&
                q_start.metadata.count("currentObjectiveId")&&
                q_start.metadata.at("currentObjectiveId")=="find_pellin","quest_call start succeeds");
            const auto hud_text=lua_call_stack.hud().get_text("quest.objectiveText");
            r.check(hud_text&&!hud_text->empty(),"quest_call updates quest.objectiveText");
            const auto q_bad=engine::execute_editor_operation(lua_call_context,"quest_call",
                R"({"kind":"complete_objective","questId":"sq_01_cart_again","objectiveId":"lift_cart"})");
            r.check(q_bad.exit_code==engine::ExitCode::ValidationFailed,"quest_call rejects out-of-order");
            const auto q_done=engine::execute_editor_operation(lua_call_context,"quest_call",
                R"({"kind":"complete_objective","questId":"sq_01_cart_again","objectiveId":"find_pellin"})");
            r.check(q_done.exit_code==engine::ExitCode::Success&&
                q_done.metadata.at("currentObjectiveId")=="clear_scavengers","quest_call completes objective");
            const auto q_list=engine::execute_editor_operation(lua_call_context,"quest_call",R"({"kind":"list"})");
            r.check(q_list.exit_code==engine::ExitCode::Success&&q_list.metadata.at("count")=="1",
                "quest_call list active");
            const auto q_abandon=engine::execute_editor_operation(lua_call_context,"quest_call",
                R"({"kind":"abandon","questId":"sq_01_cart_again"})");
            r.check(q_abandon.exit_code==engine::ExitCode::Success&&
                q_abandon.metadata.at("status")=="abandoned","quest_call abandon");
        }
        {
            auto factions=engine::WorldForgeFactionsAsset::parse(R"({
                "schemaVersion":1,"id":"mcp_standing","entities":[
                  {"id":"cristallo","kind":"faction","canonStatus":"draft","standing":{
                    "tracksPlayer":true,"min":-50,"max":50,
                    "ranks":[{"id":"neutral","minScore":0,"displayName":"N"}]}} ,
                  {"id":"arrotrebae","kind":"faction","canonStatus":"draft","standing":{
                    "tracksPlayer":true,"min":-50,"max":50}}
                ]})");
            auto rels=engine::WorldForgeRelationshipsAsset::parse(R"({
                "schemaVersion":1,"id":"mcp_rel","nodes":[],"edges":[
                  {"id":"c_vs_a","from":{"target":"faction","id":"cristallo"},
                   "to":{"target":"faction","id":"arrotrebae"},"kind":"opposes","canonStatus":"draft",
                   "standingTransfer":1.0}]})");
            r.check(factions.has_value()&&rels.has_value(),"standing_call fixtures parse");
            engine::StandingRuntime standing_rt;
            if(factions&&rels) r.check(standing_rt.bind(&factions.value(),&rels.value()).has_value(),
                "standing_call suite binds StandingRuntime");
            lua_call_context.standing_runtime=&standing_rt;
            lua_call_runtime.set_standing_runtime(&standing_rt);
            const auto s_adj=engine::execute_editor_operation(lua_call_context,"standing_call",
                R"({"kind":"adjust","factionId":"cristallo","delta":10})");
            r.check(s_adj.exit_code==engine::ExitCode::Success,"standing_call adjust succeeds");
            const auto s_get=engine::execute_editor_operation(lua_call_context,"standing_call",
                R"({"kind":"get","factionId":"arrotrebae"})");
            r.check(s_get.exit_code==engine::ExitCode::Success&&s_get.metadata.count("score")&&
                std::stod(s_get.metadata.at("score"))==-10.0,"standing_call hostility fallout via MCP");
            const auto s_list=engine::execute_editor_operation(lua_call_context,"standing_call",
                R"({"kind":"list"})");
            r.check(s_list.exit_code==engine::ExitCode::Success&&s_list.metadata.at("count")=="2",
                "standing_call list tracked");
            lua_call_runtime.set_standing_runtime(nullptr);
            lua_call_context.standing_runtime=nullptr;
        }
        const auto temp_canvas=std::filesystem::temp_directory_path()/"engine-mutate.uicanvas.json";
        {
            auto base=engine::UiCanvasAsset::load(engine::default_player_ui_canvas_path(project));
            r.check(base.has_value(),"automation mutate fixtures from sample");
            if(base) r.check(engine::write_ui_canvas_json_atomic(temp_canvas,base.value().to_json()).has_value(),
                "automation writes temp canvas");
        }
        engine::EditorSessionContext mutate_context;
        mutate_context.project_root=temp_canvas.parent_path();
        // Keep project_root as sample project; pass absolute via path that joins root - use relative under temp by
        // copying into project temp relative path instead:
        const auto project_temp=project/"assets"/"ui"/"_agent_mutate_tmp.uicanvas.json";
        {
            auto base=engine::UiCanvasAsset::load(engine::default_player_ui_canvas_path(project));
            if(base) (void)engine::write_ui_canvas_json_atomic(project_temp,base.value().to_json());
        }
        mutate_context.project_root=project;
        const auto mutate_call=engine::execute_editor_operation(mutate_context,"ui_canvas_mutate",
            R"({"path":"assets/ui/_agent_mutate_tmp.uicanvas.json","action":"move","id":"player_health","delta":[4,0]})");
        r.check(mutate_call.exit_code==engine::ExitCode::Success,"ui_canvas_mutate move succeeds");
        const auto style_call=engine::execute_editor_operation(mutate_context,"ui_canvas_mutate",
            R"({"path":"assets/ui/_agent_mutate_tmp.uicanvas.json","action":"style","id":"player_health","color":[200,40,40,255]})");
        r.check(style_call.exit_code==engine::ExitCode::Success,"ui_canvas_mutate style succeeds");
        std::error_code ec;
        std::filesystem::remove(project_temp,ec);
        std::filesystem::remove(temp_canvas,ec);
        lua_call_runtime.set_hud_runtime(nullptr);
        lua_call_runtime.set_ui_canvas_stack(nullptr);
        engine::EditorBridgeRequest request;request.operation="scene_plan";request.params_json="{\"description\":\"move entity\"}";
        r.check(engine::EditorBridgeResponse::parse_request("{\"schemaVersion\":1,\"operation\":\"editor_status\",\"requestId\":\"abc\"}").has_value(),"bridge request parses");
        engine::EditorBridgeResponse response;response.summary="ok";response.changed_object_ids={"entity"};
        r.check(response.to_json().find("\"summary\":\"ok\"")!=std::string::npos,"bridge response serializes");
        r.check(engine::validate_project_at(project).exit_code==engine::ExitCode::Success,"sample project validates through automation helper");
        r.check(engine::editor_bridge_pipe_name(std::filesystem::path("samples/open-world-rpg")) ==
                engine::editor_bridge_pipe_name(project),
            "relative and absolute project paths share bridge pipe name");
        engine::AssetRegistry assets;
        std::map<std::string, engine::PrefabAsset> catalog;
        engine::EditorSessionContext context;
        context.project_root = project;
        context.assets = &assets;
        context.prefab_catalog = &catalog;
        const auto refresh = engine::execute_editor_operation(context, "asset_apply",
            "{\"action\":\"refresh_catalog\"}");
        r.check(refresh.exit_code == engine::ExitCode::Success, "asset catalog refresh succeeds offline");
        r.check(refresh.metadata.at("prefabCount") == "7", "asset catalog refresh finds sample prefabs");
        const auto terrain = engine::execute_editor_operation(context, "scene_apply",
            "{\"action\":\"sample_terrain\",\"x\":-2.5,\"z\":5.0}");
        r.check(terrain.exit_code == engine::ExitCode::Success, "terrain sample works without live scene");
        r.check(terrain.metadata.contains("height"), "terrain sample returns height metadata");
        engine::Scene scene;
        engine::CommandHistory history;
        bool dirty = false;
        engine::EditorSessionContext batch_context;
        batch_context.scene = &scene;
        batch_context.history = &history;
        batch_context.scene_dirty = &dirty;
        batch_context.project_root = project;
        batch_context.world_path = project / "worlds" / "vertical-slice.world.json";
        const auto tree_a = engine::EntityId::parse("00000000-0000-4000-8000-00000000aa01");
        const auto tree_b = engine::EntityId::parse("00000000-0000-4000-8000-00000000aa02");
        const auto batch_place = engine::execute_editor_operation(batch_context, "scene_apply",
            R"({"action":"batch","label":"mcp-batch-place","ops":[{"action":"place","name":"Batch Tree A","prefab":"assets/prefabs/tree.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[30,0,30],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"place","name":"Batch Tree B","prefab":"assets/prefabs/tree.prefab.json","entityId":"00000000-0000-4000-8000-00000000aa02","transform":{"position":[32,0,32],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
        r.check(batch_place.exit_code == engine::ExitCode::Success, "scene batch place succeeds offline");
        r.check(batch_place.metadata.at("appliedCount") == "2", "scene batch reports applied count");
        r.check(tree_a && tree_b && scene.contains(tree_a.value()) && scene.contains(tree_b.value()), "scene batch places entities");
        const auto batch_move = engine::execute_editor_operation(batch_context, "scene_apply",
            R"({"action":"batch","ops":[{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[35,0,35],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa02","transform":{"position":[37,0,37],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
        r.check(batch_move.exit_code == engine::ExitCode::Success, "scene batch move succeeds");
        r.check(history.undo(scene) && scene.transform(tree_a.value())->position[0] == 30.0f, "single undo reverts scene batch move");
        const auto batch_fail = engine::execute_editor_operation(batch_context, "scene_apply",
            R"({"action":"batch","ops":[{"action":"move","entityId":"00000000-0000-4000-8000-00000000aa01","transform":{"position":[40,0,40],"scale":[1,1,1],"rotation":[0,0,0,1]}},{"action":"move","entityId":"00000000-0000-4000-8000-00000000ffff","transform":{"position":[41,0,41],"scale":[1,1,1],"rotation":[0,0,0,1]}}]})");
        r.check(batch_fail.exit_code == engine::ExitCode::ValidationFailed, "scene batch rolls back on mid-batch failure");
        r.check(scene.transform(tree_a.value())->position[0] == 30.0f, "failed batch leaves prior state unchanged");
        const auto component_plan = engine::classify_scene_plan("add component collider to entity", "");
        r.check(component_plan.target_kind == "scene_data", "scene plan classifies entity component edits");
        const auto prefab_component_plan = engine::classify_scene_plan("add component on prefab", "assets/prefabs/tree.prefab.json");
        r.check(prefab_component_plan.target_kind == "prefab_asset", "scene plan classifies prefab component edits");
        batch_context.prefab_catalog = &catalog;
        const auto add_component = engine::execute_editor_operation(batch_context, "scene_apply",
            R"({"action":"add_component","entityId":"00000000-0000-4000-8000-00000000aa01","component":{"id":"collider-test","type":"collider","data":{"shape":"box","halfExtent":[0.4,0.4,0.4],"layer":"staticWorld","trigger":false}}})");
        r.check(add_component.exit_code == engine::ExitCode::Success, "scene add_component succeeds");
        r.check(scene.authored_components(tree_a.value()) && !scene.authored_components(tree_a.value())->entries.empty(),
            "add_component stores entity component");
        const auto dedicated = engine::execute_editor_operation(batch_context, "entity_component_apply",
            R"({"action":"remove_component","entityId":"00000000-0000-4000-8000-00000000aa01","componentId":"collider-test"})");
        r.check(dedicated.exit_code == engine::ExitCode::Success, "entity_component_apply remove succeeds");
        const auto terrain_plan = engine::classify_scene_plan("flatten terrain near spawn and paint grass", "");
        r.check(terrain_plan.target_kind == "terrain_data", "scene plan classifies terrain sculpt/paint");
        engine::TerrainEditStore terrain_edits;
        engine::TerrainEditHistory terrain_history;
        bool terrain_dirty = false;
        engine::TerrainPaintStore terrain_paint;
        engine::TerrainPaintHistory terrain_paint_history;
        bool paint_dirty = false;
        engine::EditorSessionContext terrain_context;
        terrain_context.project_root = project;
        terrain_context.terrain_edits = &terrain_edits;
        terrain_context.terrain_history = &terrain_history;
        terrain_context.terrain_edits_dirty = &terrain_dirty;
        terrain_context.terrain_paint = &terrain_paint;
        terrain_context.terrain_paint_history = &terrain_paint_history;
        terrain_context.terrain_paint_dirty = &paint_dirty;
        engine::set_active_terrain_edits(&terrain_edits);
        const float before_h = engine::sample_terrain_height(0.0f, 0.0f);
        const auto raised = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"raise","x":0,"z":0,"radius":3,"strength":0.5})");
        r.check(raised.exit_code == engine::ExitCode::Success, "terrain_apply raise succeeds");
        r.check(engine::sample_terrain_height(0.0f, 0.0f) > before_h, "terrain_apply raise changes height");
        const auto flattened = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"flatten","x":0,"z":0,"radius":3,"strength":1.0,"targetHeight":0.0})");
        r.check(flattened.exit_code == engine::ExitCode::Success, "terrain_apply flatten succeeds");
        const auto painted = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"paint","x":0,"z":0,"radius":3,"material":"assets/materials/grass.material.json"})");
        r.check(painted.exit_code == engine::ExitCode::Success, "terrain_apply paint succeeds");
        r.check(paint_dirty, "terrain_apply paint marks dirty");
        const auto batched = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"batch","ops":[{"action":"raise","x":5,"z":5,"radius":2,"strength":0.25},{"action":"paint","x":5,"z":5,"radius":2,"material":"assets/materials/stone.material.json"}]})");
        r.check(batched.exit_code == engine::ExitCode::Success, "terrain_apply batch succeeds");
        r.check(batched.metadata.at("appliedCount") == "2", "terrain_apply batch reports applied count");
        r.check(terrain_history.undo_size() >= 2, "terrain_apply batch commits one height undo entry");
        const auto undone = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"undo","kind":"height"})");
        r.check(undone.exit_code == engine::ExitCode::Success, "terrain_apply undo height succeeds");
        engine::FoliageDensityStore foliage_density;
        engine::FoliageDensityHistory foliage_history;
        bool foliage_dirty = false;
        engine::FoliageLayerPalette foliage_palette;
        foliage_palette.layers.push_back({"grass", "Grass", "grass_blade"});
        foliage_palette.layers.push_back({"flower", "Flower", "flower_clump"});
        terrain_context.foliage_density = &foliage_density;
        terrain_context.foliage_density_history = &foliage_history;
        terrain_context.foliage_density_dirty = &foliage_dirty;
        terrain_context.foliage_layers = &foliage_palette;
        const auto foliage_painted = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"paint_foliage","x":0,"z":0,"radius":3,"strength":0.4,"layer":"grass"})");
        r.check(foliage_painted.exit_code == engine::ExitCode::Success, "terrain_apply paint_foliage succeeds");
        r.check(foliage_dirty, "terrain_apply paint_foliage marks dirty");
        r.check(foliage_density.density_at({0, 0}, 16, 16) > 0, "terrain_apply paint_foliage writes density");
        const auto foliage_batch = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"batch","ops":[{"action":"paint_foliage","x":2,"z":2,"radius":2,"strength":0.3,"layer":"flower"},{"action":"paint_foliage_mixed","x":-2,"z":-2,"radius":2,"strength":0.25}]})");
        r.check(foliage_batch.exit_code == engine::ExitCode::Success, "terrain_apply foliage batch succeeds");
        r.check(foliage_batch.metadata.at("foliageChanged") == "true", "terrain_apply foliage batch reports foliageChanged");
        const auto foliage_undone = engine::execute_editor_operation(terrain_context, "terrain_apply",
            R"({"action":"undo","kind":"foliage"})");
        r.check(foliage_undone.exit_code == engine::ExitCode::Success, "terrain_apply undo foliage succeeds");
        const auto foliage_plan = engine::classify_scene_plan("paint foliage ground cover near spawn", "");
        r.check(foliage_plan.target_kind == "terrain_data", "scene plan classifies foliage paint");
        engine::set_active_terrain_edits(nullptr);
        (void)request;
        {
            const auto missing = engine::apply_project_git_operation(
                std::filesystem::temp_directory_path() / ("engine-not-a-git-" + engine::make_correlation_id()),
                nlohmann::json{{"action", "status"}});
            r.check(missing.exit_code == engine::ExitCode::Unavailable, "project_git rejects missing/non-repo path");
            const auto no_action = engine::apply_project_git_operation(project, nlohmann::json::object());
            r.check(no_action.exit_code == engine::ExitCode::InvalidArguments, "project_git requires action");
            const auto sample_status = engine::apply_project_git_operation(project, nlohmann::json{{"action", "status"}});
            r.check(sample_status.exit_code == engine::ExitCode::Success, "project_git status on sample project");
            r.check(sample_status.metadata.count("branch") != 0, "project_git status reports branch");
            r.check(sample_status.metadata.count("repoRoot") != 0, "project_git status reports repoRoot");

            const auto fixture = std::filesystem::temp_directory_path() /
                ("engine-project-git-" + engine::make_correlation_id());
            std::error_code ec;
            std::filesystem::create_directories(fixture / "assets" / "world-forge", ec);
            std::filesystem::create_directories(fixture / "build", ec);
            {
                std::ofstream manifest(fixture / "project.engine.json");
                manifest << R"({"schemaVersion":1,"projectId":"git_fixture","name":"Git Fixture"})";
            }
            {
                std::ofstream story(fixture / "assets" / "world-forge" / "quests.worldforge.json");
                story << R"({"schemaVersion":1,"id":"quests","quests":[]})";
            }
            {
                std::ofstream junk(fixture / "build" / "ignore-me.exe");
                junk << "x";
            }
            const auto init = std::system(("git -C \"" + fixture.string() + "\" init -b main >/dev/null 2>&1").c_str());
            r.check(init == 0, "project_git fixture git init");
            (void)std::system(("git -C \"" + fixture.string() +
                "\" config user.email \"engine-suite@example.com\" >/dev/null 2>&1").c_str());
            (void)std::system(("git -C \"" + fixture.string() +
                "\" config user.name \"Engine Suite\" >/dev/null 2>&1").c_str());
            const auto commit_empty_msg = engine::apply_project_git_operation(fixture,
                nlohmann::json{{"action", "commit"}, {"message", ""}});
            r.check(commit_empty_msg.exit_code == engine::ExitCode::InvalidArguments,
                "project_git commit requires message");
            const auto commit_ok = engine::apply_project_git_operation(fixture,
                nlohmann::json{{"action", "commit"}, {"message", "seed world forge"}});
            r.check(commit_ok.exit_code == engine::ExitCode::Success, "project_git commit stages project content");
            r.check(commit_ok.metadata.count("stagedPaths") != 0 &&
                    commit_ok.metadata.at("stagedPaths").find("world-forge") != std::string::npos,
                "project_git commit includes world-forge path");
            r.check(commit_ok.metadata.at("stagedPaths").find("build/") == std::string::npos &&
                    commit_ok.metadata.at("stagedPaths").find("ignore-me.exe") == std::string::npos,
                "project_git commit blocks build artifacts");
            const auto clean = engine::apply_project_git_operation(fixture, nlohmann::json{{"action", "status"}});
            r.check(clean.exit_code == engine::ExitCode::Success && clean.metadata.at("dirtyCount") == "0",
                "project_git status clean after commit");
            const auto nothing = engine::apply_project_git_operation(fixture,
                nlohmann::json{{"action", "commit"}, {"message", "noop"}});
            r.check(nothing.exit_code == engine::ExitCode::ValidationFailed, "project_git commit fails when clean");
            engine::EditorSessionContext git_ctx;
            git_ctx.project_root = fixture;
            const auto via_op = engine::execute_editor_operation(git_ctx, "project_git", R"({"action":"status"})");
            r.check(via_op.exit_code == engine::ExitCode::Success, "editor project_git operation succeeds");
            std::filesystem::remove_all(fixture, ec);
        }
    }else if(suite=="diagnostics"){
        auto path=std::filesystem::temp_directory_path()/("engine-diagnostics-suite-"+engine::make_correlation_id()+".jsonl");engine::Logger::instance().initialize(path);
        engine::EngineError error{"TEST-RUNTIME",engine::Severity::Error,engine::ErrorCategory::Validation,"diagnostics-test","visible error",std::nullopt,{},"fix","test-correlation",engine::ErrorPriority::P1High};engine::Logger::instance().write(error);
        r.check(engine::Logger::instance().error_count()==1,"error counter increments");r.check(engine::Logger::instance().recent_errors().size()==1,"recent error retained");
        std::ifstream input(path);std::string line;std::getline(input,line);r.check(line.find("\"code\":\"TEST-RUNTIME\"")!=std::string::npos,"JSONL error persisted");r.check(line.find("\"priority\":\"P1\"")!=std::string::npos,"priority label persisted");
        r.check(line.find("\"gpuDiagnostics\":{\"available\":")!=std::string::npos,"JSONL includes GPU diagnostics");
        r.check(line.find("\"adapterName\":")!=std::string::npos&&line.find("\"driverVersion\":")!=std::string::npos&&line.find("\"dedicatedVideoMemory\":")!=std::string::npos&&line.find("\"featureLevel\":")!=std::string::npos&&line.find("\"deviceRemovalHresult\":")!=std::string::npos,"JSONL includes GPU context fields");
        const auto bundle_root=std::filesystem::temp_directory_path()/("engine-diagnostics-bundle-"+engine::make_correlation_id());
        auto bundle=engine::CrashBundle::write_diagnostic_bundle(bundle_root,error);
        r.check(bundle.has_value(),"crash bundle writes");
        if(bundle){std::ifstream diagnostic(bundle.value()/"diagnostic.json");std::string contents((std::istreambuf_iterator<char>(diagnostic)),{});r.check(contents.find("\"gpuDiagnostics\":{\"available\":")!=std::string::npos,"crash bundle includes GPU diagnostics");r.check(contents.find("\"adapterName\":")!=std::string::npos&&contents.find("\"driverVersion\":")!=std::string::npos&&contents.find("\"dedicatedVideoMemory\":")!=std::string::npos&&contents.find("\"featureLevel\":")!=std::string::npos&&contents.find("\"deviceRemovalHresult\":")!=std::string::npos,"crash bundle includes GPU context fields");}
        std::error_code cleanup;std::filesystem::remove(path,cleanup);std::filesystem::remove_all(bundle_root,cleanup);
    }else if(suite=="animator"){
        const auto root=std::filesystem::temp_directory_path()/("engine-animator-suite-"+engine::make_correlation_id());
        std::filesystem::create_directories(root/"assets/models");
        std::filesystem::create_directories(root/"assets/animators");
        std::filesystem::create_directories(root/"assets/prefabs");
        const auto clips_gltf=root/"assets/models/hero_clips.gltf";
        std::ofstream(clips_gltf)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[
{"name":"Idle","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Walk","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]},
{"name":"Attack","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}
],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"data:application/octet-stream;base64,AAAAAAAAgD8AAAAAAAAAAAAAAAAAAAAAAACAPwAAAAA="}]
})";
        const auto controller_path=root/"assets/animators/hero.animator.json";
        std::ofstream(controller_path)<<R"({
"schemaVersion":1,
"kind":"animatorController",
"id":"hero_locomotion",
"parameters":[
  {"name":"speed","type":"float","default":0.0},
  {"name":"attack","type":"trigger"}
],
"layers":[{
  "name":"base",
  "defaultState":"idle",
  "blendMode":"override",
  "states":[
    {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true}},
    {"name":"locomotion","motion":{"type":"blendTree1D","parameter":"speed","children":[
      {"threshold":0.0,"clipSource":"assets/models/hero_clips.gltf","clip":"Idle","loop":true},
      {"threshold":1.0,"clipSource":"assets/models/hero_clips.gltf","clip":"Walk","loop":true}
    ]}},
    {"name":"attack","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"Attack","loop":false}}
  ],
  "transitions":[
    {"from":"idle","to":"locomotion","duration":0.1,"conditions":[{"parameter":"speed","op":"greater","value":0.1}]},
    {"from":"locomotion","to":"idle","duration":0.1,"conditions":[{"parameter":"speed","op":"lessOrEqual","value":0.1}]},
    {"from":"*","to":"attack","duration":0.05,"conditions":[{"parameter":"attack","op":"trigger"}]}
  ]
}]
})";
        auto parsed=engine::AnimatorControllerAsset::load(controller_path);
        r.check(parsed&&parsed.value().id=="hero_locomotion"&&parsed.value().layers.size()==1,"animator controller loads");
        r.check(parsed&&parsed.value().layers[0].states.size()==3,"animator controller states parsed");
        auto bad_ctrl=engine::AnimatorControllerAsset::parse(R"({"schemaVersion":1,"kind":"animatorController","id":"x","layers":[]})", "bad");
        r.check(!bad_ctrl&&bad_ctrl.error().code=="ANIM-CTRL-LAYERS","empty layers rejected");

        engine::AnimationClipLibrary clips;
        engine::AnimatorRuntime animator;
        animator.set_project_root(root);
        animator.set_clip_library(&clips);
        const std::string entity="00000000-0000-4000-8000-0000000000a1";
        r.check(animator.attach(entity,"assets/animators/hero.animator.json").has_value(),"animator attach succeeds");
        auto idle_state=animator.current_state(entity);
        r.check(idle_state&&idle_state.value()=="idle","animator starts in idle");
        r.check(animator.set_float(entity,"speed",0.5f).has_value(),"animator set_float succeeds");
        animator.tick(0.0f);
        auto mid_state=animator.current_state(entity);
        auto mid_status=animator.status(entity);
        r.check((mid_state&&mid_state.value()=="locomotion")||(mid_status&&mid_status.value().layers[0].in_transition),
            "speed transition toward locomotion");
        for(int i=0;i<5;++i) animator.tick(0.05f);
        auto loco_state=animator.current_state(entity);
        r.check(loco_state&&loco_state.value()=="locomotion","locomotion state after blend duration");
        auto status=animator.status(entity);
        r.check(status&&!status.value().active_clips.empty(),"active clip weights reported");
        bool has_walk=false; float weight_sum=0.0f;
        for(const auto& clip:status.value().active_clips){weight_sum+=clip.weight; if(clip.clip=="Walk") has_walk=true;}
        r.check(has_walk&&weight_sum>0.9f,"blendTree1D includes Walk at speed 0.5");
        r.check(animator.set_trigger(entity,"attack").has_value(),"animator set_trigger succeeds");
        animator.tick(0.0f);
        for(int i=0;i<4;++i) animator.tick(0.05f);
        auto attack_state=animator.current_state(entity);
        r.check(attack_state&&attack_state.value()=="attack","trigger any-state transition to attack");
        auto cross=animator.crossfade(entity,"idle",0.0f);
        auto idle_again=animator.current_state(entity);
        r.check(cross&&idle_again&&idle_again.value()=="idle","crossfade instant to idle");
        auto bad_float=animator.set_float(entity,"missing",1.0f);
        r.check(!bad_float&&bad_float.error().code=="ANIM-PARAM-FLOAT","bad float param fails closed");

        const auto bad_controller=root/"assets/animators/bad_clip.animator.json";
        std::ofstream(bad_controller)<<R"({
"schemaVersion":1,"kind":"animatorController","id":"bad","parameters":[],
"layers":[{"name":"base","defaultState":"idle","states":[
  {"name":"idle","motion":{"type":"clip","clipSource":"assets/models/hero_clips.gltf","clip":"MissingClip","loop":true}}
],"transitions":[]}]}
)";
        engine::AnimatorRuntime bad_rt; bad_rt.set_project_root(root); bad_rt.set_clip_library(&clips);
        r.check(bad_rt.attach("e-bad","assets/animators/bad_clip.animator.json").has_value(),"attach with bad clip still binds controller");
        auto bad_status=bad_rt.status("e-bad");
        r.check(bad_status&&bad_status.value().active_clips.empty()&&!bad_rt.recent_errors().empty(),"missing clip fails closed with diagnostics");

        const auto prefab_path=root/"assets/prefabs/hero.prefab.json";
        std::ofstream(prefab_path)<<R"({"schemaVersion":2,"entities":[{"name":"Body","mesh":{"primitive":"capsule","color":[0.3,0.4,0.8]}}],"components":[{"id":"animator-0","type":"animator","data":{"controller":"assets/animators/hero.animator.json","defaultState":"idle"}}]})";
        auto prefab=engine::PrefabAsset::load(prefab_path);
        r.check(prefab&&prefab.value().animators.size()==1&&prefab.value().animators[0].controller.find("hero.animator.json")!=std::string::npos,"prefab animator component parsed");
        auto seeded=engine::seed_authored_components_from_prefab(prefab.value());
        r.check(seeded.entries.size()==1&&seeded.entries[0].type==engine::AuthoredComponentType::Animator,"authored animator seeded from prefab");
        auto round=engine::authored_component_entry_from_json(engine::authored_component_entry_to_json(seeded.entries[0]));
        r.check(round&&round.value().animator.controller==seeded.entries[0].animator.controller,"animator component JSON round trip");

        const auto rb_prefab_path=root/"assets/prefabs/crate.prefab.json";
        std::ofstream(rb_prefab_path)<<R"({"schemaVersion":2,"entities":[{"name":"Box","mesh":{"primitive":"cube","color":[0.6,0.4,0.2]}}],"components":[{"id":"rigidbody-0","type":"rigidbody","data":{"motionType":"dynamic","mass":2.5,"linearDamping":0.1,"angularDamping":0.2,"useGravity":true,"freezeRotation":false}}]})";
        auto rb_prefab=engine::PrefabAsset::load(rb_prefab_path);
        r.check(rb_prefab&&rb_prefab.value().rigidbodies.size()==1&&rb_prefab.value().rigidbodies[0].mass==2.5f,"prefab rigidbody component parsed");
        auto rb_seeded=engine::seed_authored_components_from_prefab(rb_prefab.value());
        r.check(rb_seeded.entries.size()==1&&rb_seeded.entries[0].type==engine::AuthoredComponentType::Rigidbody,"authored rigidbody seeded from prefab");
        auto rb_round=engine::authored_component_entry_from_json(engine::authored_component_entry_to_json(rb_seeded.entries[0]));
        r.check(rb_round&&rb_round.value().rigidbody.mass==2.5f&&rb_round.value().rigidbody.motion_type=="dynamic","rigidbody component JSON round trip");
        engine::AuthoredComponentEntry bad_mass=rb_seeded.entries[0];
        bad_mass.rigidbody.mass=0.0f;
        r.check(!engine::validate_authored_component_entry(bad_mass),"rigidbody rejects non-positive mass");
        engine::AuthoredComponentEntry bad_motion=rb_seeded.entries[0];
        bad_motion.rigidbody.motion_type="static";
        r.check(!engine::validate_authored_component_entry(bad_motion),"rigidbody rejects invalid motionType");

        engine::LuaRuntime lua; lua.set_animator_runtime(&animator);
        const auto script=root/"drive.lua";
        std::ofstream(script)<<R"(
function drive_anim(payload)
  local data = engine.json_decode(payload)
  engine.animator_set_float(data.entityId, "speed", data.speed)
  engine.animator_set_trigger(data.entityId, "attack")
end
)";
        r.check(lua.load_script(script).has_value(),"animator drive script loads");
        r.check(animator.crossfade(entity,"idle",0.0f).has_value(),"reset idle before lua drive");
        r.check(lua.call_handler("drive_anim", std::string("{\"entityId\":\"")+entity+"\",\"speed\":0.8}").has_value(),"lua animator drive succeeds");
        animator.tick(0.0f);
        for(int i=0;i<8;++i) animator.tick(0.05f);
        auto lua_attack=animator.current_state(entity);
        r.check(lua_attack&&lua_attack.value()=="attack","lua trigger reaches attack state");

        // TICKET-0104 / DEC-0030: root motion extraction + character sync
        const auto walk_bin=root/"assets/models/walk_root.bin";
        {
            // times 0,1 + Hip translation (0,0,0)->(0,0,2)
            const float payload[]={0.0f,1.0f, 0.0f,0.0f,0.0f, 0.0f,0.0f,2.0f};
            std::ofstream(walk_bin,std::ios::binary).write(reinterpret_cast<const char*>(payload),sizeof(payload));
        }
        const auto walk_gltf=root/"assets/models/walk_root.gltf";
        std::ofstream(walk_gltf)<<R"({
"asset":{"version":"2.0"},
"nodes":[{"name":"Hip"}],
"animations":[{"name":"WalkRoot","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}],
"accessors":[
{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","max":[1.0],"min":[0.0]},
{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}
],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],
"buffers":[{"byteLength":32,"uri":"walk_root.bin"}]
})";
        auto walk_set=engine::import_gltf_animation_clips(walk_gltf);
        r.check(walk_set&&!walk_set.value().clips.empty(),"walk root clip imports");
        auto root_delta=engine::extract_clip_root_motion_delta(walk_set.value().clips[0],"Hip",0.0f,0.5f,true);
        r.check(root_delta&&root_delta.value().found_root_channel&&std::abs(root_delta.value().translation[2]-1.0f)<1e-4f,
            "root motion extracts +Z half-second delta");
        auto wrap_delta=engine::extract_clip_root_motion_delta(walk_set.value().clips[0],"Hip",0.75f,1.25f,true);
        r.check(wrap_delta&&std::abs(wrap_delta.value().translation[2]-1.0f)<1e-3f,"root motion wraps looped clip delta");

        const auto root_ctrl=root/"assets/animators/hero_root.animator.json";
        std::ofstream(root_ctrl)<<R"({
"schemaVersion":1,"kind":"animatorController","id":"hero_root","applyRootMotion":true,"rootJoint":"Hip","rootMotionY":false,
"parameters":[{"name":"speed","type":"float","default":1.0}],
"layers":[{"name":"base","defaultState":"walk","states":[
  {"name":"walk","motion":{"type":"clip","clipSource":"assets/models/walk_root.gltf","clip":"WalkRoot","loop":true}}
],"transitions":[]}]
})";
        engine::AnimatorRuntime root_rt; root_rt.set_project_root(root); root_rt.set_clip_library(&clips);
        const std::string root_entity="root-motion-entity";
        r.check(root_rt.attach(root_entity,"assets/animators/hero_root.animator.json").has_value(),"root-motion controller attaches");
        auto arm=root_rt.apply_root_motion(root_entity);
        r.check(arm&&arm.value(),"applyRootMotion enabled on instance");
        root_rt.tick(1.0f/60.0f);
        auto tick_delta=root_rt.root_motion_delta(root_entity);
        r.check(tick_delta&&std::abs(tick_delta.value().translation[2]-(2.0f/60.0f))<1e-4f,"animator tick emits root delta");

        engine::CollisionWorld cworld;
        auto terrain=engine::generate_stylized_terrain({0,0},33,40);
        auto terrain_body=terrain?cworld.add_heightfield(terrain.value(),{},engine::CellCoord{0,0})
            :engine::Result<engine::CollisionBody>::failure(engine::EngineError{});
        r.check(terrain&&terrain_body,"root-motion terrain ready");
        const float ground=engine::sample_terrain_height(8.0f,8.0f);
        auto created=engine::CharacterController::create(cworld,{8.0,ground+5.0,8.0});
        r.check(created.has_value(),"root-motion character created");
        auto character=std::move(created.value());
        for(int i=0;i<180&&!character.on_ground();++i) (void)character.move({0,0,0},0.0f,1.0f/60.0f);
        r.check(character.on_ground(),"root-motion character grounded");
        const auto start_pos=character.position();
        for(int i=0;i<60;++i){
            auto synced=engine::sync_character_root_motion(character,root_rt,root_entity,0.0f,1.0f/60.0f);
            r.check(synced&&synced.value(),"sync_character_root_motion applies");
        }
        const auto end_pos=character.position();
        r.check(std::abs(end_pos.z-start_pos.z)>1.5,"animation-driven root motion moves capsule forward");

        // TICKET-0199: root motion on Rigidbody / CollisionBody
        {
            engine::CollisionWorld rb_world;
            auto floor=rb_world.add_box({0,-1,0},{20,1,20},engine::CollisionLayer::StaticWorld,false);
            r.check(floor.has_value(),"0199 root-motion floor");
            auto settings=engine::CollisionBodySettings::make_dynamic();
            settings.mass=70.0f;
            settings.freeze_rotation=true;
            auto body=rb_world.add_capsule({0.0,1.5,0.0},0.35f,0.85f,engine::CollisionLayer::Dynamic,settings);
            r.check(body.has_value(),"0199 root-motion capsule body");
            for(int i=0;i<120;++i) r.check(rb_world.step(1.0f/60.0f).has_value(),"0199 settle step");
            const auto start=rb_world.position(body.value()).value();
            engine::AnimatorRuntime rb_rt; rb_rt.set_project_root(root); rb_rt.set_clip_library(&clips);
            const std::string rb_entity="root-motion-rb";
            r.check(rb_rt.attach(rb_entity,"assets/animators/hero_root.animator.json").has_value(),"0199 rb controller attaches");
            for(int i=0;i<60;++i){
                auto synced=engine::sync_rigidbody_root_motion(rb_world,body.value(),rb_rt,rb_entity,0.0f,1.0f/60.0f);
                r.check(synced&&synced.value(),"sync_rigidbody_root_motion applies");
                r.check(rb_world.step(1.0f/60.0f).has_value(),"0199 root-motion physics step");
            }
            const auto end=rb_world.position(body.value()).value();
            r.check(end.z>start.z+1.0,"animation-driven root motion moves rigidbody forward");
            auto bad=engine::apply_rigidbody_root_motion(rb_world,{}, {0,0,1},1.0f/60.0f);
            r.check(!bad,"0199 invalid body rejected");
        }

        // TICKET-0105 / DEC-0031: controller timeline events → Lua
        const auto event_ctrl=root/"assets/animators/hero_events.animator.json";
        std::ofstream(event_ctrl)<<R"({
"schemaVersion":1,"kind":"animatorController","id":"hero_events",
"parameters":[],
"layers":[{"name":"base","defaultState":"walk","states":[
  {"name":"walk","motion":{"type":"clip","clipSource":"assets/models/walk_root.gltf","clip":"WalkRoot","loop":true}}
],"transitions":[]}],
"timelineEvents":[
  {"state":"walk","time":0.25,"name":"footstep","layer":"base","payload":{"foot":"left"}},
  {"state":"walk","time":0.75,"name":"footstep","payload":{"foot":"right"}}
]
})";
        auto event_parsed=engine::AnimatorControllerAsset::load(event_ctrl);
        r.check(event_parsed&&event_parsed.value().timeline_events.size()==2,"timelineEvents parse");
        auto bad_event=engine::AnimatorControllerAsset::parse(R"({
"schemaVersion":1,"kind":"animatorController","id":"bad_ev","parameters":[],
"layers":[{"name":"base","defaultState":"idle","states":[{"name":"idle","motion":{"type":"clip","clipSource":"x.gltf","clip":"A","loop":true}}],"transitions":[]}],
"timelineEvents":[{"state":"missing","time":0.1,"name":"x"}]
})", "bad_ev");
        r.check(!bad_event&&bad_event.error().code=="ANIM-CTRL-EVENT-STATE-MISSING","bad timelineEvent state rejected");

        engine::AnimatorRuntime event_rt; event_rt.set_project_root(root); event_rt.set_clip_library(&clips);
        const std::string event_entity="event-entity";
        r.check(event_rt.attach(event_entity,"assets/animators/hero_events.animator.json").has_value(),"event controller attaches");
        (void)event_rt.take_fired_events();
        std::size_t first_hits=0;
        for(int i=0;i<20;++i){ // 20 * 0.05 = 1.0s through one loop of WalkRoot
            event_rt.tick(0.05f);
            first_hits+=event_rt.take_fired_events().size();
        }
        r.check(first_hits==2,"timeline events fire once per cycle");
        std::size_t second_hits=0;
        for(int i=0;i<20;++i){
            event_rt.tick(0.05f);
            second_hits+=event_rt.take_fired_events().size();
        }
        r.check(second_hits==2,"timeline events re-fire after loop wrap");

        engine::LuaRuntime event_lua; event_lua.set_animator_runtime(&event_rt);
        const auto event_script=root/"anim_events.lua";
        std::ofstream(event_script)<<R"(
function on_animation_event(payload)
  local data, err = engine.json_decode(payload)
  if not data then
    engine.blackboard_set("anim_event_err", tostring(err))
    return
  end
  local count = engine.blackboard_get("anim_event_count") or 0
  engine.blackboard_set("anim_event_count", count + 1)
  engine.blackboard_set("anim_event_name", tostring(data.name or ""))
end
)";
        r.check(event_lua.load_script(event_script).has_value(),"animation event script loads");
        event_rt.detach(event_entity);
        r.check(event_rt.attach(event_entity,"assets/animators/hero_events.animator.json").has_value(),"reattach for lua event smoke");
        (void)event_rt.take_fired_events();
        event_lua.blackboard_clear();
        event_lua.clear_recent_errors();
        std::size_t dispatched=0;
        for(int i=0;i<10;++i){
            event_rt.tick(0.05f);
            for(const auto& fired:event_rt.take_fired_events()){
                event_lua.dispatch_animation_event(fired);
                ++dispatched;
            }
        }
        r.check(dispatched>=1,"lua smoke received at least one fired event");
        r.check(event_lua.recent_errors().empty(),"on_animation_event dispatch has no script errors");
        auto count_entry=event_lua.blackboard_get("anim_event_count");
        auto name_entry=event_lua.blackboard_get("anim_event_name");
        r.check(count_entry&&count_entry->type==engine::ScriptBlackboardType::Number&&count_entry->number_value>=1.0,
            "on_animation_event updates blackboard");
        r.check(name_entry&&name_entry->string_value=="footstep","on_animation_event receives event name");

        std::filesystem::remove_all(root);
    }else{std::cerr<<"unknown suite\n";return 2;}
    std::cout<<"{\"suite\":\""<<r.suite<<"\",\"assertions\":"<<r.assertions<<",\"passed\":"<<(r.assertions-r.failures)<<",\"failed\":"<<r.failures<<"}\n";
    return r.failures?1:0;
}
