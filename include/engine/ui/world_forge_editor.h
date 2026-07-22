#pragma once

#include "engine/assets/world_forge_archetypes_asset.h"
#include "engine/assets/world_forge_dialogues_asset.h"
#include "engine/assets/world_forge_factions_asset.h"
#include "engine/assets/world_forge_map_asset.h"
#include "engine/assets/world_forge_pantheon_asset.h"
#include "engine/assets/world_forge_quests_asset.h"
#include "engine/assets/world_forge_relationships_asset.h"
#include "engine/assets/world_forge_resources_asset.h"
#include "engine/core/result.h"
#include "engine/dialogue/dialogue_graph_edit.h"
#include "engine/ui/editor_ui_hotspots.h"
#include "engine/ui/world_forge_graph_camera.h"

#include <cstdint>
#include <filesystem>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace engine {

class TerrainEditStore;

/// Optional terrain sampling for Map Canvas underlay (TICKET-0188).
struct WorldForgeViewportDrawContext {
    const TerrainEditStore* terrain_edits = nullptr;
    /// Bumped when sculpt heights change so the Map underlay can rebake.
    std::uint64_t terrain_revision = 0;
    /// Optional MCP hotspot sink (filled while drawing World Forge widgets).
    EditorUiHotspotRegistry* hotspots = nullptr;
};

/// Which World Forge sub-tab is active in the Viewports "World Forge" tab.
enum class WorldForgeEditorPane : std::uint8_t {
    Overview,
    Hierarchy,
    Archetypes,
    Resources,
    Relationships,
    Map,
    Quests,
    Dialogues
};

/// Map Canvas authoring tool (TICKET-0208 Pencil tool rail).
enum class WorldForgeMapTool : std::uint8_t {
    Select,
    Anchor,
    Route,
    Border,
    Water
};

/// Hierarchy authorship sub-page (TICKET-0184).
enum class WorldForgeHierarchyPage : std::uint8_t { Religion, Factions, Persons };

/// In-memory editor state for the World Forge Viewports tab (TICKET-0015/0016/0017/0051/0052/0053/0184).
///
/// Holds World Forge assets loaded/saved through `apply_world_forge_operation`
/// (DEC-0003 parity with MCP) so the editor and MCP agents share one mutation path.
struct WorldForgeEditorSession {
    /// Which list/canvas is shown; depends on `pane`.
    enum class ListKind : std::uint8_t {
        Entities,
        Nodes,
        Edges,
        Graph,
        Regions,
        Pois,
        Links,
        Hydrology,
        FerryRoutes,
        TravelRoutes,
        Quests,
        Dialogues,
        DialogueGraph,
        Pantheon,
        Archetypes,
        Resources,
        MapCanvas
    };

    WorldForgeEditorPane pane = WorldForgeEditorPane::Hierarchy;
    /// When true, next pane tab bar draw forces ImGui selection to `pane` (MCP map_view).
    bool force_select_pane = false;
    /// When true, tab bar open-handlers must not overwrite `pane` (keeps MCP map_view sticky).
    bool lock_pane_tab = false;
    /// Global Act lens (DEC-0036). Empty = All acts. Values: act0..act4.
    std::string act_filter;
    WorldForgeHierarchyPage hierarchy_page = WorldForgeHierarchyPage::Religion;
    /// Hierarchy pages: tree list vs parentId graph canvas.
    bool hierarchy_graph_mode = false;
    /// Map pane: list+detail vs spatial XZ canvas (TICKET-0187).
    bool map_canvas_mode = true;
    /// Map Canvas tool rail (TICKET-0208).
    WorldForgeMapTool map_tool = WorldForgeMapTool::Select;
    /// Cartography: titles appear on hover (selected always labeled).
    bool map_labels_on_hover = true;
    bool map_show_legend = true;
    bool map_show_heraldry_legend = true;
    bool map_show_draft_badge = true;
    /// Cursor world XZ under the map aperture (status / toolbar readout).
    bool map_cursor_valid = false;
    float map_cursor_world_x = 0.0f;
    float map_cursor_world_z = 0.0f;
    /// Hierarchy → Persons: when true, list/graph show only person nodes tagged `companion`.
    bool hierarchy_persons_companions_only = false;
    WorldForgeFactionsAsset factions;
    WorldForgePantheonAsset pantheon;
    WorldForgeArchetypesAsset archetypes;
    WorldForgeResourcesAsset resources;
    WorldForgeRelationshipsAsset relationships;
    WorldForgeMapAsset map;
    WorldForgeQuestsAsset quests;
    WorldForgeDialoguesAsset dialogues;
    /// Selected entity/node/edge/region/poi/link/quest/tree id, interpreted per `list_kind`.
    std::string selected_id;
    ListKind list_kind = ListKind::Entities;
    bool dirty = false;
    std::string status;
    bool loaded = false;

    /// Ephemeral ImGui canvas layout (not serialized). Keys are node ids or `faction:<id>`.
    std::unordered_map<std::string, std::array<float, 2>> graph_positions;
    bool graph_needs_layout = true;
    std::string graph_drag_key;
    float graph_zoom = 1.0f;
    std::array<float, 2> graph_pan{{0.0f, 0.0f}};
    bool graph_panning = false;
    std::array<float, 2> graph_pan_start_mouse{{0.0f, 0.0f}};
    std::array<float, 2> graph_pan_start_pan{{0.0f, 0.0f}};
    std::array<char, 128> graph_filter_text{};
    bool graph_filter_person = true;
    bool graph_filter_deity = true;
    bool graph_filter_artifact = true;
    bool graph_filter_organization = true;
    bool graph_filter_hide_factions = false;
    bool graph_filter_focus_neighborhood = false;
    int graph_expand_hops = 1;
    bool graph_filter_edge_kinds[9] = {true, true, true, true, true, true, true, true, true};

    std::array<char, 96> create_node_id{};
    std::array<char, 96> create_node_name{};
    WorldForgeRelationshipNodeKind create_node_kind = WorldForgeRelationshipNodeKind::Person;
    std::array<char, 96> create_quest_id{};
    std::array<char, 96> create_quest_name{};
    std::array<char, 96> create_objective_id{};
    std::array<char, 96> create_fork_id{};

    std::array<char, 96> create_faction_name{};
    WorldForgeFactionKind create_faction_kind = WorldForgeFactionKind::Faction;

    std::array<char, 96> create_pantheon_name{};
    WorldForgePantheonKind create_pantheon_kind = WorldForgePantheonKind::Deity;

    std::array<char, 96> create_archetype_name{};
    WorldForgeArchetypeKind create_archetype_kind = WorldForgeArchetypeKind::Starting;

    std::array<char, 96> create_resource_name{};
    WorldForgeResourceKind create_resource_kind = WorldForgeResourceKind::Mineral;

    std::array<char, 96> create_region_name{};
    WorldForgeRegionKind create_region_kind = WorldForgeRegionKind::Region;
    std::array<char, 96> create_poi_name{};
    WorldForgePoiKind create_poi_kind = WorldForgePoiKind::Landmark;
    std::array<char, 96> create_poi_region_id{};
    std::array<char, 96> create_link_id{};
    WorldForgeMapLinkKind create_link_kind = WorldForgeMapLinkKind::Travel;
    WorldForgeMapEndpointKind create_link_from_kind = WorldForgeMapEndpointKind::Region;
    WorldForgeMapEndpointKind create_link_to_kind = WorldForgeMapEndpointKind::Region;
    std::array<char, 96> create_link_from_id{};
    std::array<char, 96> create_link_to_id{};

    std::array<char, 96> create_hydrology_name{};
    WorldForgeHydrologyKind create_hydrology_kind = WorldForgeHydrologyKind::Lake;
    std::array<char, 96> create_ferry_route_name{};
    std::array<char, 96> create_ferry_from_poi_id{};
    std::array<char, 96> create_ferry_to_poi_id{};
    std::array<char, 96> create_travel_route_name{};
    std::array<char, 96> create_travel_from_poi_id{};
    std::array<char, 96> create_travel_to_poi_id{};
    WorldForgeTravelRouteKind create_travel_kind = WorldForgeTravelRouteKind::Road;

    std::array<char, 96> create_dialogue_tree_name{};
    std::array<char, 96> create_dialogue_tree_parent_quest{};

    std::array<char, 96> create_edge_id{};
    std::array<char, 96> create_edge_from{};
    std::array<char, 96> create_edge_to{};
    WorldForgeRelationshipEndpointTarget create_edge_from_target = WorldForgeRelationshipEndpointTarget::Node;
    WorldForgeRelationshipEndpointTarget create_edge_to_target = WorldForgeRelationshipEndpointTarget::Node;
    WorldForgeRelationshipEdgeKind create_edge_kind = WorldForgeRelationshipEdgeKind::Related;
    std::string graph_link_from;
    bool graph_fit_requested = false;

    /// Hierarchy graph canvas (Religion/Factions/Persons) — separate from Relationships Graph.
    std::unordered_map<std::string, std::array<float, 2>> hierarchy_graph_positions;
    bool hierarchy_graph_needs_layout = true;
    std::string hierarchy_graph_drag_key;
    WorldForgeGraphCamera hierarchy_graph_camera;
    bool hierarchy_graph_panning = false;
    std::array<float, 2> hierarchy_graph_pan_start_mouse{{0.0f, 0.0f}};
    std::array<float, 2> hierarchy_graph_pan_start_pan{{0.0f, 0.0f}};
    bool hierarchy_graph_fit_requested = false;

    /// Map spatial canvas (TICKET-0187 / 0188) — world XZ via graph camera.
    WorldForgeGraphCamera map_camera;
    bool map_camera_panning = false;
    std::array<float, 2> map_camera_pan_start_mouse{{0.0f, 0.0f}};
    std::array<float, 2> map_camera_pan_start_pan{{0.0f, 0.0f}};
    bool map_camera_fit_requested = false;
    std::string map_drag_key;
    /// When set, next empty-canvas click places this region/poi id (without kind prefix).
    std::string map_place_id;
    bool map_place_is_poi = false;
    bool map_filter_regions = true;
    bool map_filter_pois = true;
    bool map_filter_links = true;
    bool map_filter_hydrology = true;
    bool map_filter_ferry_routes = true;
    bool map_filter_travel_routes = true;
    /// Cartography (parchment) vs top-down terrain canvas.
    bool map_cartography_mode = true;
    /// Cartography: draw official Tessera world-map art under markers.
    /// When `map.cartography_plate` is set, the backdrop locks to that world-meter window.
    bool map_show_official_backdrop = true;
    /// Draft span (km) for Apply plate + rescale (default 4 km = v1 slice width).
    float map_plate_span_km = 4.0f;
    /// Cartography: ornate parchment frame overlay (screen-space chrome). Off by default — plate fills the canvas.
    bool map_show_frame = false;
    bool map_show_borders = true;
    /// Click on canvas appends polyline points to this travel route (empty = off).
    std::string map_travel_draw_id;
    /// Click on canvas appends points to this region's border (empty = off).
    std::string map_border_region_id;
    bool map_reference_popup = false;
    /// Click-drag on canvas sets bounds for this hydrology id (empty = off).
    std::string map_hydrology_bounds_id;
    bool map_hydrology_bounds_dragging = false;
    std::array<float, 2> map_hydrology_bounds_drag_start{{0.0f, 0.0f}};
    /// Click on canvas appends polyline points to this ferry route (empty = off).
    std::string map_ferry_draw_id;
    bool map_show_terrain = true;
    bool map_show_grid = true;
    bool map_show_contours = true;
    /// Cached normalized height field (0..1) for smooth topo underlay.
    std::vector<float> map_underlay_heights;
    int map_underlay_w = 0;
    int map_underlay_h = 0;
    float map_underlay_min_x = 0.0f;
    float map_underlay_max_x = 0.0f;
    float map_underlay_min_z = 0.0f;
    float map_underlay_max_z = 0.0f;
    std::uint64_t map_underlay_revision = 0;
    bool map_underlay_ready = false;

    /// Dialogue graph canvas state (TICKET-0053 / 0165–0168). `selected_id` is the tree; this is the node.
    DialogueGraphPositions dialogue_graph_positions;
    /// When true, clear positions and run layered layout (tree switch / Auto Layout / import).
    bool dialogue_graph_full_relayout = true;
    std::string dialogue_selected_node_id;
    std::string dialogue_graph_drag_key;
    std::string dialogue_link_from;
    WorldForgeGraphCamera dialogue_graph_camera;
    bool dialogue_graph_panning = false;
    std::array<float, 2> dialogue_graph_pan_start_mouse{{0.0f, 0.0f}};
    std::array<float, 2> dialogue_graph_pan_start_pan{{0.0f, 0.0f}};
    bool dialogue_graph_fit_requested = false;
    bool dialogue_graph_zoom_to_selected = false;
    DialogueGraphNodeDisplayMode dialogue_node_display_mode = DialogueGraphNodeDisplayMode::Standard;
    std::array<char, 128> dialogue_search_text{};
    bool dialogue_search_focus_requested = false;
    std::vector<std::string> dialogue_nav_history;
    int dialogue_nav_history_index = -1;
    bool dialogue_nav_suppress = false;
    std::unordered_set<std::string> dialogue_bookmarks;
    std::vector<WorldForgeDialogueTree> dialogue_undo_stack;
    std::vector<WorldForgeDialogueTree> dialogue_redo_stack;
    std::array<char, 96> create_dialogue_node_id{};
    std::array<char, 128> create_dialogue_choice_text{};
    std::array<char, 260> twee_import_path{};
    std::array<char, 96> twee_import_tree_id{};
    std::array<char, 96> twee_import_parent_quest{};
    std::array<char, 128> twee_import_display_name{};

    std::unordered_map<std::string, std::uint64_t> concept_placeholder_tex;
    bool concept_placeholder_tex_ready = false;
    /// Cartography Map Canvas icons / heraldry (`icon-village`, `heraldry-kingdom_tessera`, …).
    std::unordered_map<std::string, std::uint64_t> cartography_tex;
    bool cartography_tex_ready = false;

    /// Discrete Cartography zoom plates (see `world-map-layers/manifest.json`). Preferred over tiles.
    struct WorldMapLayer {
        std::string id;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 1.0f;
        float v1 = 1.0f;
        float min_zoom = 0.0f;
        int priority = 0;
        int width = 0;
        int height = 0;
    };
    bool map_layers_ready = false;
    float map_layer_aspect = 1.5f;
    int map_layer_native_width = 0;
    float map_layer_transition_seconds = 0.35f;
    std::vector<WorldMapLayer> map_layers;
    std::unordered_map<std::string, std::uint64_t> map_layer_tex;
    std::string map_layer_active_id;
    std::string map_layer_pending_id;
    /// 0 = idle; (0,1] = fog-in / swap / fog-out progress.
    float map_layer_transition_t = 0.0f;
    /// Status: last drawn layer id.
    std::string map_layer_draw_id;

    /// Legacy multi-LOD tiles (fallback if layers missing).
    struct WorldMapTileLevel {
        int lod = 0;
        int cols = 0;
        int rows = 0;
        int content_width = 0;
        int content_height = 0;
        int level_width = 0;
        int level_height = 0;
    };
    bool map_tiles_ready = false;
    int map_tile_size = 512;
    int map_tile_max_lod = 0;
    int map_tile_native_width = 0;
    float map_tile_aspect = 1.5f;
    /// Last drawn backdrop LOD (for status); -1 if unused this frame.
    int map_tile_draw_lod = -1;
    std::vector<WorldMapTileLevel> map_tile_levels;
    /// Packed key: `(lod << 24) | (x << 12) | y` → ImGui texture bits.
    std::unordered_map<std::uint32_t, std::uint64_t> map_tile_tex;

    [[nodiscard]] Result<void> reload(const std::filesystem::path& project_root);
    [[nodiscard]] Result<void> save(const std::filesystem::path& project_root);
};

void draw_world_forge_viewport(WorldForgeEditorSession& session, const std::filesystem::path& project_root,
    const WorldForgeViewportDrawContext& draw_context = {});

/// Resolve region/POI anchor for a map link endpoint (nullptr if missing / unanchored).
[[nodiscard]] const WorldForgeWorldAnchor* resolve_map_endpoint_anchor(const WorldForgeMapAsset& asset,
    WorldForgeMapEndpointKind kind, const std::string& id);

[[nodiscard]] std::string map_region_marker_key(const std::string& region_id);
[[nodiscard]] std::string map_poi_marker_key(const std::string& poi_id);

} // namespace engine
