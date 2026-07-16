#include "engine/assets/world_forge_dialogues_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError dlg_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_dialogues", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeDialogueCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established") return Result<WorldForgeDialogueCanonStatus>::success(WorldForgeDialogueCanonStatus::Established);
    if (key == "draft") return Result<WorldForgeDialogueCanonStatus>::success(WorldForgeDialogueCanonStatus::Draft);
    if (key == "proposal") return Result<WorldForgeDialogueCanonStatus>::success(WorldForgeDialogueCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgeDialogueCanonStatus>::success(WorldForgeDialogueCanonStatus::Open);
    return Result<WorldForgeDialogueCanonStatus>::failure(dlg_error("WORLD-FORGE-DLG-CANON", ErrorCategory::Validation,
        "Unsupported canonStatus: " + raw, "Use established, draft, proposal, or open."));
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

WorldForgeDialogueChoice read_choice(const nlohmann::json& node) {
    WorldForgeDialogueChoice choice;
    if (!node.is_object()) return choice;
    choice.id = node.value("id", std::string{});
    choice.text = node.value("text", std::string{});
    choice.next_node_id = node.value("nextNodeId", std::string{});
    choice.set_flags = read_string_array(node.value("setFlags", nlohmann::json::array()));
    return choice;
}

nlohmann::ordered_json write_choice(const WorldForgeDialogueChoice& choice) {
    return nlohmann::ordered_json{{"id", choice.id}, {"text", choice.text}, {"nextNodeId", choice.next_node_id},
        {"setFlags", write_string_array(choice.set_flags)}};
}

WorldForgeDialogueNode read_node(const nlohmann::json& node) {
    WorldForgeDialogueNode out;
    if (!node.is_object()) return out;
    out.id = node.value("id", std::string{});
    out.speaker_id = node.value("speakerId", std::string{});
    out.line = node.value("line", std::string{});
    const auto choices = node.value("choices", nlohmann::json::array());
    if (choices.is_array()) {
        for (const auto& choice_node : choices) out.choices.push_back(read_choice(choice_node));
    }
    return out;
}

nlohmann::ordered_json write_node(const WorldForgeDialogueNode& node) {
    auto choices_json = nlohmann::ordered_json::array();
    for (const auto& choice : node.choices) choices_json.push_back(write_choice(choice));
    return nlohmann::ordered_json{{"id", node.id}, {"speakerId", node.speaker_id}, {"line", node.line},
        {"choices", std::move(choices_json)}};
}

Result<void> validate_tree(const WorldForgeDialogueTree& tree) {
    if (tree.id.empty()) {
        return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-TREE-ID", ErrorCategory::Validation,
            "Dialogue tree id is required", "Set a unique non-empty id for each tree."));
    }

    std::unordered_set<std::string> node_ids;
    node_ids.reserve(tree.nodes.size());
    for (const auto& node : tree.nodes) {
        if (node.id.empty()) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-NODE-ID", ErrorCategory::Validation,
                "Node id is required on tree '" + tree.id + "'", "Set a unique non-empty id for each node."));
        }
        if (!node_ids.insert(node.id).second) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-NODE-ID-DUP", ErrorCategory::Validation,
                "Duplicate node id '" + node.id + "' on tree '" + tree.id + "'",
                "Ensure node ids are unique within a tree."));
        }
    }

    if (node_ids.find(tree.entry_node_id) == node_ids.end()) {
        return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-ENTRY", ErrorCategory::Validation,
            "entryNodeId '" + tree.entry_node_id + "' not found on tree '" + tree.id + "'",
            "Point entryNodeId at an existing node id."));
    }

    std::unordered_set<std::string> choice_ids;
    for (const auto& node : tree.nodes) {
        for (const auto& choice : node.choices) {
            if (choice.id.empty()) {
                return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-CHOICE-ID", ErrorCategory::Validation,
                    "Choice id is required on tree '" + tree.id + "'", "Set a unique non-empty id for each choice."));
            }
            if (!choice_ids.insert(choice.id).second) {
                return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-CHOICE-ID-DUP", ErrorCategory::Validation,
                    "Duplicate choice id '" + choice.id + "' on tree '" + tree.id + "'",
                    "Ensure choice ids are unique within a tree."));
            }
            if (!choice.next_node_id.empty() && node_ids.find(choice.next_node_id) == node_ids.end()) {
                return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-NEXT", ErrorCategory::Validation,
                    "nextNodeId '" + choice.next_node_id + "' not found on tree '" + tree.id + "'",
                    "Point nextNodeId at an existing node id or leave it empty to end."));
            }
            for (const auto& flag : choice.set_flags) {
                if (flag.empty()) {
                    return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-FLAG", ErrorCategory::Validation,
                        "setFlags entries must be non-empty on tree '" + tree.id + "'",
                        "Remove empty strings from setFlags or omit the array."));
                }
            }
        }
    }
    return Result<void>::success();
}

} // namespace

const char* to_string(WorldForgeDialogueCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeDialogueCanonStatus::Established: return "established";
    case WorldForgeDialogueCanonStatus::Draft: return "draft";
    case WorldForgeDialogueCanonStatus::Proposal: return "proposal";
    case WorldForgeDialogueCanonStatus::Open: return "open";
    }
    return "draft";
}

const char* to_string(WorldForgeDialogueSpeakerTarget value) noexcept {
    switch (value) {
    case WorldForgeDialogueSpeakerTarget::Node: return "node";
    case WorldForgeDialogueSpeakerTarget::Faction: return "faction";
    case WorldForgeDialogueSpeakerTarget::Narrator: return "narrator";
    }
    return "node";
}

std::filesystem::path default_world_forge_dialogues_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "dialogues.worldforge.json";
}

Result<void> WorldForgeDialoguesAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-SCHEMA", ErrorCategory::Validation,
            "Only World Forge dialogues schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> tree_ids;
    tree_ids.reserve(trees.size());
    for (const auto& tree : trees) {
        if (tree.id.empty()) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-TREE-ID", ErrorCategory::Validation,
                "Dialogue tree id is required", "Set a unique non-empty id for each tree."));
        }
        if (!tree_ids.insert(tree.id).second) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-ID-DUP", ErrorCategory::Validation,
                "Duplicate dialogue tree id: " + tree.id, "Ensure every tree id is unique."));
        }
        if (const auto valid = validate_tree(tree); !valid) return Result<void>::failure(valid.error());
    }
    return Result<void>::success();
}

Result<void> WorldForgeDialoguesAsset::validate_quest_refs(const std::unordered_set<std::string>& known_quest_ids) const {
    if (known_quest_ids.empty()) return Result<void>::success();
    for (const auto& tree : trees) {
        if (tree.parent_quest_id.empty()) continue;
        if (known_quest_ids.find(tree.parent_quest_id) == known_quest_ids.end()) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-QUEST-REF", ErrorCategory::Validation,
                "Unknown parentQuestId '" + tree.parent_quest_id + "' on tree '" + tree.id + "'",
                "Point parentQuestId at a quest id or leave it empty."));
        }
    }
    return Result<void>::success();
}

const WorldForgeDialogueTree* WorldForgeDialoguesAsset::find_tree(const std::string& tree_id) const {
    for (const auto& tree : trees) {
        if (tree.id == tree_id) return &tree;
    }
    return nullptr;
}

WorldForgeDialogueTree* WorldForgeDialoguesAsset::find_tree(const std::string& tree_id) {
    for (auto& tree : trees) {
        if (tree.id == tree_id) return &tree;
    }
    return nullptr;
}

Result<WorldForgeDialoguesAsset> WorldForgeDialoguesAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-ROOT", ErrorCategory::Serialization,
                source_name + " must be a JSON object", "Wrap dialogues in an object with schemaVersion and id."));
        }
        WorldForgeDialoguesAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-SCHEMA", ErrorCategory::Validation,
                "Unsupported World Forge dialogues schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto trees_json = json.value("trees", nlohmann::json::array());
        if (!trees_json.is_array()) {
            return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-TREES", ErrorCategory::Validation,
                "trees must be an array", "Provide a trees array."));
        }
        for (const auto& tree_node : trees_json) {
            if (!tree_node.is_object()) {
                return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-TREE", ErrorCategory::Validation,
                    "Each dialogue tree must be an object", "Fix tree entries."));
            }
            WorldForgeDialogueTree tree;
            tree.id = tree_node.value("id", std::string{});
            if (tree.id.empty()) {
                return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-TREE-ID", ErrorCategory::Validation,
                    "Dialogue tree id is required", "Set a unique non-empty id for each tree."));
            }
            tree.parent_quest_id = tree_node.value("parentQuestId", std::string{});
            tree.display_name = tree_node.value("displayName", std::string{});
            const auto canon = parse_canon_status(tree_node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeDialoguesAsset>::failure(canon.error());
            tree.canon_status = canon.value();
            tree.summary = tree_node.value("summary", std::string{});
            tree.story_ref = tree_node.value("storyRef", std::string{});
            tree.entry_node_id = tree_node.value("entryNodeId", std::string{});
            tree.tags = read_string_array(tree_node.value("tags", nlohmann::json::array()));
            tree.open_questions = read_string_array(tree_node.value("openQuestions", nlohmann::json::array()));

            const auto nodes_json = tree_node.value("nodes", nlohmann::json::array());
            if (!nodes_json.is_array()) {
                return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-NODES", ErrorCategory::Validation,
                    "nodes must be an array on tree '" + tree.id + "'", "Provide a nodes array (may be empty)."));
            }
            for (const auto& node_json : nodes_json) {
                if (!node_json.is_object()) {
                    return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-NODE", ErrorCategory::Validation,
                        "Each node must be an object", "Fix node entries."));
                }
                tree.nodes.push_back(read_node(node_json));
            }

            asset.trees.push_back(std::move(tree));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeDialoguesAsset>::failure(valid.error());
        }
        return Result<WorldForgeDialoguesAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = dlg_error("WORLD-FORGE-DLG-PARSE", ErrorCategory::Serialization, "Failed to parse " + source_name,
            "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeDialoguesAsset>::failure(std::move(error));
    }
}

Result<WorldForgeDialoguesAsset> WorldForgeDialoguesAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeDialoguesAsset>::failure(dlg_error("WORLD-FORGE-DLG-READ", ErrorCategory::Io,
            "Could not read World Forge dialogues: " + path.generic_string(), "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeDialoguesAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto trees_json = nlohmann::ordered_json::array();
    for (const auto& tree : trees) {
        nlohmann::ordered_json tree_json;
        tree_json["id"] = tree.id;
        tree_json["parentQuestId"] = tree.parent_quest_id;
        tree_json["displayName"] = tree.display_name;
        tree_json["canonStatus"] = to_string(tree.canon_status);
        tree_json["summary"] = tree.summary;
        tree_json["storyRef"] = tree.story_ref;
        tree_json["entryNodeId"] = tree.entry_node_id;
        auto nodes_json = nlohmann::ordered_json::array();
        for (const auto& node : tree.nodes) nodes_json.push_back(write_node(node));
        tree_json["nodes"] = std::move(nodes_json);
        tree_json["tags"] = write_string_array(tree.tags);
        tree_json["openQuestions"] = write_string_array(tree.open_questions);
        trees_json.push_back(std::move(tree_json));
    }
    json["trees"] = std::move(trees_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeDialoguesAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-IO", ErrorCategory::Io,
                "Could not write World Forge dialogues: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
        if (!output) {
            return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-IO", ErrorCategory::Io,
                "Failed while writing World Forge dialogues: " + path.generic_string(),
                "Check disk space and retry."));
        }
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::rename(path, backup, ec);
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        return Result<void>::failure(dlg_error("WORLD-FORGE-DLG-IO", ErrorCategory::Io,
            "Could not finalize World Forge dialogues write: " + path.generic_string(),
            "Check file permissions; restore from .bak if needed."));
    }
    return Result<void>::success();
}

Result<void> WorldForgeDialoguesAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return loaded.value().validate();
}

Result<void> WorldForgeDialoguesAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_quest_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    if (const auto valid = loaded.value().validate(); !valid) return Result<void>::failure(valid.error());
    return loaded.value().validate_quest_refs(known_quest_ids);
}

} // namespace engine
