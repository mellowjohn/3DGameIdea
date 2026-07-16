#include "engine/assets/world_forge_relationships_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace engine {
namespace {

EngineError relationship_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_relationships", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeRelationshipNodeKind> parse_node_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "person") return Result<WorldForgeRelationshipNodeKind>::success(WorldForgeRelationshipNodeKind::Person);
    if (key == "deity") return Result<WorldForgeRelationshipNodeKind>::success(WorldForgeRelationshipNodeKind::Deity);
    if (key == "artifact")
        return Result<WorldForgeRelationshipNodeKind>::success(WorldForgeRelationshipNodeKind::Artifact);
    if (key == "organization")
        return Result<WorldForgeRelationshipNodeKind>::success(WorldForgeRelationshipNodeKind::Organization);
    return Result<WorldForgeRelationshipNodeKind>::failure(
        relationship_error("WORLD-FORGE-REL-NODE-KIND", ErrorCategory::Validation,
            "Unsupported World Forge relationship node kind: " + raw,
            "Use person, deity, artifact, or organization."));
}

Result<WorldForgeRelationshipCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established")
        return Result<WorldForgeRelationshipCanonStatus>::success(WorldForgeRelationshipCanonStatus::Established);
    if (key == "draft")
        return Result<WorldForgeRelationshipCanonStatus>::success(WorldForgeRelationshipCanonStatus::Draft);
    if (key == "proposal")
        return Result<WorldForgeRelationshipCanonStatus>::success(WorldForgeRelationshipCanonStatus::Proposal);
    if (key == "open")
        return Result<WorldForgeRelationshipCanonStatus>::success(WorldForgeRelationshipCanonStatus::Open);
    return Result<WorldForgeRelationshipCanonStatus>::failure(
        relationship_error("WORLD-FORGE-REL-CANON", ErrorCategory::Validation, "Unsupported canonStatus: " + raw,
            "Use established, draft, proposal, or open."));
}

Result<WorldForgeRelationshipEndpointTarget> parse_endpoint_target(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "node")
        return Result<WorldForgeRelationshipEndpointTarget>::success(WorldForgeRelationshipEndpointTarget::Node);
    if (key == "faction")
        return Result<WorldForgeRelationshipEndpointTarget>::success(WorldForgeRelationshipEndpointTarget::Faction);
    return Result<WorldForgeRelationshipEndpointTarget>::failure(
        relationship_error("WORLD-FORGE-REL-ENDPOINT", ErrorCategory::Validation,
            "Unsupported relationship endpoint target: " + raw, "Use node or faction."));
}

Result<WorldForgeRelationshipEdgeKind> parse_edge_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "ally") return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Ally);
    if (key == "rival") return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Rival);
    if (key == "member_of" || key == "memberof")
        return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::MemberOf);
    if (key == "leads") return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Leads);
    if (key == "kin") return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Kin);
    if (key == "serves") return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Serves);
    if (key == "opposes")
        return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Opposes);
    if (key == "influences")
        return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Influences);
    if (key == "related")
        return Result<WorldForgeRelationshipEdgeKind>::success(WorldForgeRelationshipEdgeKind::Related);
    return Result<WorldForgeRelationshipEdgeKind>::failure(
        relationship_error("WORLD-FORGE-REL-EDGE-KIND", ErrorCategory::Validation,
            "Unsupported World Forge relationship edge kind: " + raw,
            "Use ally, rival, member_of, leads, kin, serves, opposes, influences, or related."));
}

std::vector<std::string> read_string_array(const nlohmann::json& node) {
    std::vector<std::string> out;
    if (!node.is_array()) return out;
    for (const auto& entry : node) {
        if (entry.is_string()) out.push_back(entry.get<std::string>());
    }
    return out;
}

nlohmann::json write_string_array(const std::vector<std::string>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (const auto& value : values) out.push_back(value);
    return out;
}

Result<WorldForgeRelationshipEndpoint> parse_endpoint(const nlohmann::json& node, const char* field_name) {
    if (!node.is_object()) {
        return Result<WorldForgeRelationshipEndpoint>::failure(
            relationship_error("WORLD-FORGE-REL-ENDPOINT", ErrorCategory::Validation,
                std::string(field_name) + " must be an object with target and id",
                "Example: {\"target\":\"node\",\"id\":\"luceran_the_hollow\"}."));
    }
    WorldForgeRelationshipEndpoint endpoint;
    const auto target = parse_endpoint_target(node.value("target", std::string{}));
    if (!target) return Result<WorldForgeRelationshipEndpoint>::failure(target.error());
    endpoint.target = target.value();
    endpoint.id = node.value("id", std::string{});
    if (endpoint.id.empty()) {
        return Result<WorldForgeRelationshipEndpoint>::failure(
            relationship_error("WORLD-FORGE-REL-ENDPOINT", ErrorCategory::Validation,
                std::string(field_name) + ".id is required", "Set a non-empty endpoint id."));
    }
    return Result<WorldForgeRelationshipEndpoint>::success(std::move(endpoint));
}

} // namespace

const char* to_string(WorldForgeRelationshipNodeKind value) noexcept {
    switch (value) {
    case WorldForgeRelationshipNodeKind::Person: return "person";
    case WorldForgeRelationshipNodeKind::Deity: return "deity";
    case WorldForgeRelationshipNodeKind::Artifact: return "artifact";
    case WorldForgeRelationshipNodeKind::Organization: return "organization";
    }
    return "person";
}

const char* to_string(WorldForgeRelationshipCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeRelationshipCanonStatus::Established: return "established";
    case WorldForgeRelationshipCanonStatus::Draft: return "draft";
    case WorldForgeRelationshipCanonStatus::Proposal: return "proposal";
    case WorldForgeRelationshipCanonStatus::Open: return "open";
    }
    return "draft";
}

const char* to_string(WorldForgeRelationshipEndpointTarget value) noexcept {
    switch (value) {
    case WorldForgeRelationshipEndpointTarget::Node: return "node";
    case WorldForgeRelationshipEndpointTarget::Faction: return "faction";
    }
    return "node";
}

const char* to_string(WorldForgeRelationshipEdgeKind value) noexcept {
    switch (value) {
    case WorldForgeRelationshipEdgeKind::Ally: return "ally";
    case WorldForgeRelationshipEdgeKind::Rival: return "rival";
    case WorldForgeRelationshipEdgeKind::MemberOf: return "member_of";
    case WorldForgeRelationshipEdgeKind::Leads: return "leads";
    case WorldForgeRelationshipEdgeKind::Kin: return "kin";
    case WorldForgeRelationshipEdgeKind::Serves: return "serves";
    case WorldForgeRelationshipEdgeKind::Opposes: return "opposes";
    case WorldForgeRelationshipEdgeKind::Influences: return "influences";
    case WorldForgeRelationshipEdgeKind::Related: return "related";
    }
    return "related";
}

std::filesystem::path default_world_forge_relationships_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "relationships.worldforge.json";
}

Result<void> WorldForgeRelationshipsAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(relationship_error("WORLD-FORGE-REL-SCHEMA", ErrorCategory::Validation,
            "Only World Forge relationships schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> node_ids;
    node_ids.reserve(nodes.size());
    for (const auto& node : nodes) {
        if (node.id.empty()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-NODE-ID", ErrorCategory::Validation,
                "Relationship node id is required", "Set a unique non-empty id for each node."));
        }
        if (!node_ids.insert(node.id).second) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-NODE-ID-DUP", ErrorCategory::Validation,
                "Duplicate relationship node id: " + node.id, "Ensure every node id is unique."));
        }
    }
    std::unordered_set<std::string> edge_ids;
    edge_ids.reserve(edges.size());
    for (const auto& edge : edges) {
        if (edge.id.empty()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-EDGE-ID", ErrorCategory::Validation,
                "Relationship edge id is required", "Set a unique non-empty id for each edge."));
        }
        if (!edge_ids.insert(edge.id).second) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-EDGE-ID-DUP", ErrorCategory::Validation,
                "Duplicate relationship edge id: " + edge.id, "Ensure every edge id is unique."));
        }
        if (edge.from.id.empty() || edge.to.id.empty()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-ENDPOINT", ErrorCategory::Validation,
                "Edge endpoints require non-empty ids: " + edge.id, "Set from.id and to.id."));
        }
        if (edge.from.target == WorldForgeRelationshipEndpointTarget::Node &&
            node_ids.find(edge.from.id) == node_ids.end()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-REF", ErrorCategory::Validation,
                "Unknown from node '" + edge.from.id + "' on edge '" + edge.id + "'",
                "from.id with target node must match a node id in this file."));
        }
        if (edge.to.target == WorldForgeRelationshipEndpointTarget::Node &&
            node_ids.find(edge.to.id) == node_ids.end()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-REF", ErrorCategory::Validation,
                "Unknown to node '" + edge.to.id + "' on edge '" + edge.id + "'",
                "to.id with target node must match a node id in this file."));
        }
        if (edge.from.target == edge.to.target && edge.from.id == edge.to.id) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-SELF", ErrorCategory::Validation,
                "Relationship edge cannot connect an endpoint to itself: " + edge.id,
                "Use two distinct endpoints."));
        }
        if (edge.standing_transfer < 0.0) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-STANDING-TRANSFER",
                ErrorCategory::Validation, "standingTransfer must be >= 0 on edge '" + edge.id + "'",
                "Use a non-negative transfer weight."));
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeRelationshipsAsset::validate_faction_refs(
    const std::unordered_set<std::string>& known_faction_ids) const {
    if (known_faction_ids.empty()) return Result<void>::success();
    for (const auto& edge : edges) {
        if (edge.from.target == WorldForgeRelationshipEndpointTarget::Faction &&
            known_faction_ids.find(edge.from.id) == known_faction_ids.end()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-FACTION-REF", ErrorCategory::Validation,
                "Unknown faction endpoint '" + edge.from.id + "' on edge '" + edge.id + "'",
                "Faction endpoints must match an id in factions.worldforge.json."));
        }
        if (edge.to.target == WorldForgeRelationshipEndpointTarget::Faction &&
            known_faction_ids.find(edge.to.id) == known_faction_ids.end()) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-FACTION-REF", ErrorCategory::Validation,
                "Unknown faction endpoint '" + edge.to.id + "' on edge '" + edge.id + "'",
                "Faction endpoints must match an id in factions.worldforge.json."));
        }
    }
    return Result<void>::success();
}

Result<WorldForgeRelationshipsAsset> WorldForgeRelationshipsAsset::parse(const std::string& text,
    const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeRelationshipsAsset>::failure(
                relationship_error("WORLD-FORGE-REL-ROOT", ErrorCategory::Serialization,
                    source_name + " must be a JSON object",
                    "Wrap nodes/edges in an object with schemaVersion and id."));
        }
        WorldForgeRelationshipsAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeRelationshipsAsset>::failure(
                relationship_error("WORLD-FORGE-REL-SCHEMA", ErrorCategory::Validation,
                    "Unsupported World Forge relationships schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto nodes = json.value("nodes", nlohmann::json::array());
        if (!nodes.is_array()) {
            return Result<WorldForgeRelationshipsAsset>::failure(
                relationship_error("WORLD-FORGE-REL-NODES", ErrorCategory::Validation, "nodes must be an array",
                    "Provide a nodes array."));
        }
        for (const auto& node : nodes) {
            if (!node.is_object()) {
                return Result<WorldForgeRelationshipsAsset>::failure(
                    relationship_error("WORLD-FORGE-REL-NODE", ErrorCategory::Validation,
                        "Each node must be an object", "Fix node entries."));
            }
            WorldForgeRelationshipNode entry;
            entry.id = node.value("id", std::string{});
            if (entry.id.empty()) {
                return Result<WorldForgeRelationshipsAsset>::failure(
                    relationship_error("WORLD-FORGE-REL-NODE-ID", ErrorCategory::Validation,
                        "Relationship node id is required", "Set a unique non-empty id for each node."));
            }
            const auto kind = parse_node_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeRelationshipsAsset>::failure(kind.error());
            entry.kind = kind.value();
            entry.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeRelationshipsAsset>::failure(canon.error());
            entry.canon_status = canon.value();
            entry.summary = node.value("summary", std::string{});
            entry.story_ref = node.value("storyRef", std::string{});
            entry.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            entry.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));
            asset.nodes.push_back(std::move(entry));
        }
        const auto edges = json.value("edges", nlohmann::json::array());
        if (!edges.is_array()) {
            return Result<WorldForgeRelationshipsAsset>::failure(
                relationship_error("WORLD-FORGE-REL-EDGES", ErrorCategory::Validation, "edges must be an array",
                    "Provide an edges array."));
        }
        for (const auto& edge_node : edges) {
            if (!edge_node.is_object()) {
                return Result<WorldForgeRelationshipsAsset>::failure(
                    relationship_error("WORLD-FORGE-REL-EDGE", ErrorCategory::Validation,
                        "Each edge must be an object", "Fix edge entries."));
            }
            WorldForgeRelationshipEdge edge;
            edge.id = edge_node.value("id", std::string{});
            if (edge.id.empty()) {
                return Result<WorldForgeRelationshipsAsset>::failure(
                    relationship_error("WORLD-FORGE-REL-EDGE-ID", ErrorCategory::Validation,
                        "Relationship edge id is required", "Set a unique non-empty id for each edge."));
            }
            auto from = parse_endpoint(edge_node.value("from", nlohmann::json{}), "from");
            if (!from) return Result<WorldForgeRelationshipsAsset>::failure(from.error());
            edge.from = std::move(from.value());
            auto to = parse_endpoint(edge_node.value("to", nlohmann::json{}), "to");
            if (!to) return Result<WorldForgeRelationshipsAsset>::failure(to.error());
            edge.to = std::move(to.value());
            const auto kind = parse_edge_kind(edge_node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeRelationshipsAsset>::failure(kind.error());
            edge.kind = kind.value();
            const auto canon = parse_canon_status(edge_node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeRelationshipsAsset>::failure(canon.error());
            edge.canon_status = canon.value();
            edge.bidirectional = edge_node.value("bidirectional", false);
            edge.summary = edge_node.value("summary", std::string{});
            edge.story_ref = edge_node.value("storyRef", std::string{});
            edge.open_questions = read_string_array(edge_node.value("openQuestions", nlohmann::json::array()));
            edge.standing_transfer = edge_node.value("standingTransfer", 0.0);
            asset.edges.push_back(std::move(edge));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeRelationshipsAsset>::failure(valid.error());
        }
        return Result<WorldForgeRelationshipsAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = relationship_error("WORLD-FORGE-REL-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeRelationshipsAsset>::failure(std::move(error));
    }
}

Result<WorldForgeRelationshipsAsset> WorldForgeRelationshipsAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeRelationshipsAsset>::failure(
            relationship_error("WORLD-FORGE-REL-READ", ErrorCategory::Io,
                "Could not read World Forge relationships: " + path.generic_string(),
                "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeRelationshipsAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto nodes_json = nlohmann::ordered_json::array();
    for (const auto& node : nodes) {
        nlohmann::ordered_json entry;
        entry["id"] = node.id;
        entry["kind"] = to_string(node.kind);
        entry["displayName"] = node.display_name;
        entry["canonStatus"] = to_string(node.canon_status);
        entry["summary"] = node.summary;
        entry["storyRef"] = node.story_ref;
        entry["tags"] = write_string_array(node.tags);
        entry["openQuestions"] = write_string_array(node.open_questions);
        nodes_json.push_back(std::move(entry));
    }
    json["nodes"] = std::move(nodes_json);
    auto edges_json = nlohmann::ordered_json::array();
    for (const auto& edge : edges) {
        nlohmann::ordered_json entry;
        entry["id"] = edge.id;
        entry["from"] = nlohmann::ordered_json{{"target", to_string(edge.from.target)}, {"id", edge.from.id}};
        entry["to"] = nlohmann::ordered_json{{"target", to_string(edge.to.target)}, {"id", edge.to.id}};
        entry["kind"] = to_string(edge.kind);
        entry["canonStatus"] = to_string(edge.canon_status);
        entry["bidirectional"] = edge.bidirectional;
        entry["summary"] = edge.summary;
        entry["storyRef"] = edge.story_ref;
        entry["openQuestions"] = write_string_array(edge.open_questions);
        entry["standingTransfer"] = edge.standing_transfer;
        edges_json.push_back(std::move(entry));
    }
    json["edges"] = std::move(edges_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeRelationshipsAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(relationship_error("WORLD-FORGE-REL-IO", ErrorCategory::Io,
                "Could not write World Forge relationships: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
    }
    std::error_code ignored;
    if (std::filesystem::exists(path)) {
        std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing, ignored);
    }
    std::filesystem::rename(temporary, path, ignored);
    if (ignored) {
        return Result<void>::failure(relationship_error("WORLD-FORGE-REL-IO", ErrorCategory::Io,
            "Could not replace World Forge relationships: " + path.generic_string(),
            "Check file permissions and disk space."));
    }
    if (std::filesystem::exists(backup)) std::filesystem::remove(backup, ignored);
    return Result<void>::success();
}

Result<void> WorldForgeRelationshipsAsset::validate_file(const std::filesystem::path& path) {
    return validate_file(path, {});
}

Result<void> WorldForgeRelationshipsAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_faction_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return loaded.value().validate_faction_refs(known_faction_ids);
}

} // namespace engine
