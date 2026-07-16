#include "engine/assets/world_forge_quests_asset.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace engine {
namespace {

EngineError quest_error(std::string code, ErrorCategory category, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, category, "world_forge_quests", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

Result<WorldForgeQuestCanonStatus> parse_canon_status(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "established") return Result<WorldForgeQuestCanonStatus>::success(WorldForgeQuestCanonStatus::Established);
    if (key == "draft") return Result<WorldForgeQuestCanonStatus>::success(WorldForgeQuestCanonStatus::Draft);
    if (key == "proposal") return Result<WorldForgeQuestCanonStatus>::success(WorldForgeQuestCanonStatus::Proposal);
    if (key == "open") return Result<WorldForgeQuestCanonStatus>::success(WorldForgeQuestCanonStatus::Open);
    return Result<WorldForgeQuestCanonStatus>::failure(quest_error("WORLD-FORGE-QUEST-CANON", ErrorCategory::Validation,
        "Unsupported canonStatus: " + raw, "Use established, draft, proposal, or open."));
}

Result<WorldForgeQuestKind> parse_kind(const std::string& raw) {
    const auto key = lower_copy(raw);
    if (key == "main") return Result<WorldForgeQuestKind>::success(WorldForgeQuestKind::Main);
    if (key == "side") return Result<WorldForgeQuestKind>::success(WorldForgeQuestKind::Side);
    if (key == "faction") return Result<WorldForgeQuestKind>::success(WorldForgeQuestKind::Faction);
    return Result<WorldForgeQuestKind>::failure(quest_error("WORLD-FORGE-QUEST-KIND", ErrorCategory::Validation,
        "Unsupported quest kind: " + raw, "Use main, side, or faction."));
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

WorldForgeQuestDialogueHooks read_dialogue_hooks(const nlohmann::json& node) {
    WorldForgeQuestDialogueHooks hooks;
    if (!node.is_object()) return hooks;
    hooks.start_id = node.value("startId", std::string{});
    hooks.complete_id = node.value("completeId", std::string{});
    hooks.abandon_id = node.value("abandonId", std::string{});
    return hooks;
}

nlohmann::json write_dialogue_hooks(const WorldForgeQuestDialogueHooks& hooks) {
    return nlohmann::ordered_json{{"startId", hooks.start_id}, {"completeId", hooks.complete_id},
        {"abandonId", hooks.abandon_id}};
}

} // namespace

const char* to_string(WorldForgeQuestKind value) noexcept {
    switch (value) {
    case WorldForgeQuestKind::Main: return "main";
    case WorldForgeQuestKind::Side: return "side";
    case WorldForgeQuestKind::Faction: return "faction";
    }
    return "side";
}

const char* to_string(WorldForgeQuestCanonStatus value) noexcept {
    switch (value) {
    case WorldForgeQuestCanonStatus::Established: return "established";
    case WorldForgeQuestCanonStatus::Draft: return "draft";
    case WorldForgeQuestCanonStatus::Proposal: return "proposal";
    case WorldForgeQuestCanonStatus::Open: return "open";
    }
    return "draft";
}

std::filesystem::path default_world_forge_quests_path(const std::filesystem::path& project_root) {
    return project_root / "assets" / "world-forge" / "quests.worldforge.json";
}

Result<void> WorldForgeQuestsAsset::validate() const {
    if (schema_version != 1) {
        return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-SCHEMA", ErrorCategory::Validation,
            "Only World Forge quests schemaVersion 1 is supported", "Use schemaVersion 1."));
    }
    std::unordered_set<std::string> quest_ids;
    quest_ids.reserve(quests.size());
    for (const auto& quest : quests) {
        if (quest.id.empty()) {
            return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-ID", ErrorCategory::Validation,
                "Quest id is required", "Set a unique non-empty id for each quest."));
        }
        if (!quest_ids.insert(quest.id).second) {
            return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-ID-DUP", ErrorCategory::Validation,
                "Duplicate quest id: " + quest.id, "Ensure every quest id is unique."));
        }

        std::unordered_set<std::string> objective_ids;
        for (const auto& objective : quest.objectives) {
            if (objective.id.empty()) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-OBJECTIVE-ID", ErrorCategory::Validation,
                    "Objective id is required on quest '" + quest.id + "'",
                    "Set a unique non-empty id for each objective."));
            }
            if (!objective_ids.insert(objective.id).second) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-OBJECTIVE-ID-DUP",
                    ErrorCategory::Validation, "Duplicate objective id '" + objective.id + "' on quest '" + quest.id + "'",
                    "Ensure objective ids are unique within a quest."));
            }
        }

        std::unordered_set<std::string> fork_ids;
        for (const auto& fork : quest.forks) {
            if (fork.id.empty()) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-FORK-ID", ErrorCategory::Validation,
                    "Fork id is required on quest '" + quest.id + "'",
                    "Set a unique non-empty id for each fork."));
            }
            if (!fork_ids.insert(fork.id).second) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-FORK-ID-DUP", ErrorCategory::Validation,
                    "Duplicate fork id '" + fork.id + "' on quest '" + quest.id + "'",
                    "Ensure fork ids are unique within a quest."));
            }
        }
        for (const auto& req : quest.standing_requirements) {
            if (req.faction_id.empty()) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REQ", ErrorCategory::Validation,
                    "standingRequirements[].factionId is required on quest '" + quest.id + "'",
                    "Set a non-empty factionId."));
            }
            if (!req.min_score && req.min_rank_id.empty()) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REQ", ErrorCategory::Validation,
                    "standingRequirements entry needs minScore and/or minRankId on quest '" + quest.id + "'",
                    "Provide at least one gate field."));
            }
        }
        for (const auto& reward : quest.standing_rewards) {
            if (reward.faction_id.empty()) {
                return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REWARD", ErrorCategory::Validation,
                    "standingRewards[].factionId is required on quest '" + quest.id + "'",
                    "Set a non-empty factionId."));
            }
        }
    }
    return Result<void>::success();
}

Result<void> WorldForgeQuestsAsset::validate_region_refs(const std::unordered_set<std::string>& known_region_ids) const {
    if (known_region_ids.empty()) return Result<void>::success();
    for (const auto& quest : quests) {
        if (quest.region_id.empty()) continue;
        if (known_region_ids.find(quest.region_id) == known_region_ids.end()) {
            return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-REGION-REF", ErrorCategory::Validation,
                "Unknown regionId '" + quest.region_id + "' on quest '" + quest.id + "'",
                "Point regionId at a map region id or leave it empty."));
        }
    }
    return Result<void>::success();
}

Result<WorldForgeQuestsAsset> WorldForgeQuestsAsset::parse(const std::string& text, const std::string& source_name) {
    try {
        const auto json = nlohmann::json::parse(text);
        if (!json.is_object()) {
            return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-ROOT",
                ErrorCategory::Serialization, source_name + " must be a JSON object",
                "Wrap quests in an object with schemaVersion and id."));
        }
        WorldForgeQuestsAsset asset;
        asset.schema_version = json.value("schemaVersion", 0);
        if (asset.schema_version != 1) {
            return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-SCHEMA",
                ErrorCategory::Validation, "Unsupported World Forge quests schemaVersion", "Use schemaVersion 1."));
        }
        asset.id = json.value("id", std::string{});
        const auto quests_json = json.value("quests", nlohmann::json::array());
        if (!quests_json.is_array()) {
            return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-QUESTS",
                ErrorCategory::Validation, "quests must be an array", "Provide a quests array."));
        }
        for (const auto& node : quests_json) {
            if (!node.is_object()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-ENTRY",
                    ErrorCategory::Validation, "Each quest must be an object", "Fix quest entries."));
            }
            WorldForgeQuest quest;
            quest.id = node.value("id", std::string{});
            if (quest.id.empty()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-ID",
                    ErrorCategory::Validation, "Quest id is required",
                    "Set a unique non-empty id for each quest."));
            }
            const auto kind = parse_kind(node.value("kind", std::string{}));
            if (!kind) return Result<WorldForgeQuestsAsset>::failure(kind.error());
            quest.kind = kind.value();
            quest.display_name = node.value("displayName", std::string{});
            const auto canon = parse_canon_status(node.value("canonStatus", std::string{}));
            if (!canon) return Result<WorldForgeQuestsAsset>::failure(canon.error());
            quest.canon_status = canon.value();
            quest.summary = node.value("summary", std::string{});
            quest.story_ref = node.value("storyRef", std::string{});
            quest.consequential = node.value("consequential", false);
            quest.region_id = node.value("regionId", std::string{});
            quest.starts = node.value("starts", std::string{});
            quest.dialogue = read_dialogue_hooks(node.value("dialogue", nlohmann::json::object()));
            quest.tags = read_string_array(node.value("tags", nlohmann::json::array()));
            quest.open_questions = read_string_array(node.value("openQuestions", nlohmann::json::array()));

            const auto objectives = node.value("objectives", nlohmann::json::array());
            if (!objectives.is_array()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-OBJECTIVES",
                    ErrorCategory::Validation, "objectives must be an array on quest '" + quest.id + "'",
                    "Provide an objectives array (may be empty)."));
            }
            for (const auto& obj_node : objectives) {
                if (!obj_node.is_object()) {
                    return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-OBJECTIVE",
                        ErrorCategory::Validation, "Each objective must be an object", "Fix objectives."));
                }
                WorldForgeQuestObjective objective;
                objective.id = obj_node.value("id", std::string{});
                objective.summary = obj_node.value("summary", std::string{});
                objective.dialogue_id = obj_node.value("dialogueId", std::string{});
                quest.objectives.push_back(std::move(objective));
            }

            const auto forks = node.value("forks", nlohmann::json::array());
            if (!forks.is_array()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-FORKS",
                    ErrorCategory::Validation, "forks must be an array on quest '" + quest.id + "'",
                    "Provide a forks array (may be empty)."));
            }
            for (const auto& fork_node : forks) {
                if (!fork_node.is_object()) {
                    return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-FORK",
                        ErrorCategory::Validation, "Each fork must be an object", "Fix forks."));
                }
                WorldForgeQuestFork fork;
                fork.id = fork_node.value("id", std::string{});
                fork.summary = fork_node.value("summary", std::string{});
                fork.outcome_flags = read_string_array(fork_node.value("outcomeFlags", nlohmann::json::array()));
                fork.dialogue_id = fork_node.value("dialogueId", std::string{});
                quest.forks.push_back(std::move(fork));
            }

            const auto standing_reqs = node.value("standingRequirements", nlohmann::json::array());
            if (!standing_reqs.is_array()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REQ",
                    ErrorCategory::Validation, "standingRequirements must be an array on quest '" + quest.id + "'",
                    "Provide an array (may be empty)."));
            }
            for (const auto& req_node : standing_reqs) {
                if (!req_node.is_object()) {
                    return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REQ",
                        ErrorCategory::Validation, "Each standingRequirements entry must be an object",
                        "Fix standingRequirements."));
                }
                WorldForgeQuestStandingRequirement req;
                req.faction_id = req_node.value("factionId", std::string{});
                if (req_node.contains("minScore") && !req_node["minScore"].is_null()) {
                    req.min_score = req_node["minScore"].get<double>();
                }
                req.min_rank_id = req_node.value("minRankId", std::string{});
                quest.standing_requirements.push_back(std::move(req));
            }
            const auto standing_rewards = node.value("standingRewards", nlohmann::json::array());
            if (!standing_rewards.is_array()) {
                return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REWARD",
                    ErrorCategory::Validation, "standingRewards must be an array on quest '" + quest.id + "'",
                    "Provide an array (may be empty)."));
            }
            for (const auto& reward_node : standing_rewards) {
                if (!reward_node.is_object()) {
                    return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-STANDING-REWARD",
                        ErrorCategory::Validation, "Each standingRewards entry must be an object",
                        "Fix standingRewards."));
                }
                WorldForgeQuestStandingReward reward;
                reward.faction_id = reward_node.value("factionId", std::string{});
                reward.delta = reward_node.value("delta", 0.0);
                quest.standing_rewards.push_back(std::move(reward));
            }

            asset.quests.push_back(std::move(quest));
        }
        if (const auto valid = asset.validate(); !valid) {
            return Result<WorldForgeQuestsAsset>::failure(valid.error());
        }
        return Result<WorldForgeQuestsAsset>::success(std::move(asset));
    } catch (const std::exception& exception) {
        auto error = quest_error("WORLD-FORGE-QUEST-PARSE", ErrorCategory::Serialization,
            "Failed to parse " + source_name, "Fix JSON syntax.");
        error.causes.push_back(exception.what());
        return Result<WorldForgeQuestsAsset>::failure(std::move(error));
    }
}

Result<WorldForgeQuestsAsset> WorldForgeQuestsAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeQuestsAsset>::failure(quest_error("WORLD-FORGE-QUEST-READ", ErrorCategory::Io,
            "Could not read World Forge quests: " + path.generic_string(),
            "Check the path and file permissions."));
    }
    std::ostringstream text;
    text << input.rdbuf();
    return parse(text.str(), path.filename().string());
}

std::string WorldForgeQuestsAsset::to_json() const {
    nlohmann::ordered_json json;
    json["schemaVersion"] = schema_version;
    json["id"] = id;
    auto quests_json = nlohmann::ordered_json::array();
    for (const auto& quest : this->quests) {
        nlohmann::ordered_json node;
        node["id"] = quest.id;
        node["kind"] = to_string(quest.kind);
        node["displayName"] = quest.display_name;
        node["canonStatus"] = to_string(quest.canon_status);
        node["summary"] = quest.summary;
        node["storyRef"] = quest.story_ref;
        node["consequential"] = quest.consequential;
        node["regionId"] = quest.region_id;
        node["starts"] = quest.starts;
        node["dialogue"] = write_dialogue_hooks(quest.dialogue);
        auto objectives_json = nlohmann::ordered_json::array();
        for (const auto& objective : quest.objectives) {
            objectives_json.push_back(nlohmann::ordered_json{{"id", objective.id}, {"summary", objective.summary},
                {"dialogueId", objective.dialogue_id}});
        }
        node["objectives"] = std::move(objectives_json);
        auto forks_json = nlohmann::ordered_json::array();
        for (const auto& fork : quest.forks) {
            forks_json.push_back(nlohmann::ordered_json{{"id", fork.id}, {"summary", fork.summary},
                {"outcomeFlags", write_string_array(fork.outcome_flags)}, {"dialogueId", fork.dialogue_id}});
        }
        node["forks"] = std::move(forks_json);
        auto standing_reqs_json = nlohmann::ordered_json::array();
        for (const auto& req : quest.standing_requirements) {
            nlohmann::ordered_json entry{{"factionId", req.faction_id}, {"minRankId", req.min_rank_id}};
            if (req.min_score) entry["minScore"] = *req.min_score;
            standing_reqs_json.push_back(std::move(entry));
        }
        node["standingRequirements"] = std::move(standing_reqs_json);
        auto standing_rewards_json = nlohmann::ordered_json::array();
        for (const auto& reward : quest.standing_rewards) {
            standing_rewards_json.push_back(
                nlohmann::ordered_json{{"factionId", reward.faction_id}, {"delta", reward.delta}});
        }
        node["standingRewards"] = std::move(standing_rewards_json);
        node["tags"] = write_string_array(quest.tags);
        node["openQuestions"] = write_string_array(quest.open_questions);
        quests_json.push_back(std::move(node));
    }
    json["quests"] = std::move(quests_json);
    return json.dump(2) + "\n";
}

Result<void> WorldForgeQuestsAsset::save_atomic(const std::filesystem::path& path) const {
    const auto valid = validate();
    if (!valid) return Result<void>::failure(valid.error());
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    const auto backup = path.string() + ".bak";
    {
        std::ofstream output(temporary, std::ios::trunc);
        if (!output) {
            return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-IO", ErrorCategory::Io,
                "Could not write World Forge quests: " + path.generic_string(),
                "Check file permissions and disk space."));
        }
        output << to_json();
        if (!output) {
            return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-IO", ErrorCategory::Io,
                "Failed while writing World Forge quests: " + path.generic_string(),
                "Check disk space and retry."));
        }
    }
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::rename(path, backup, ec);
    }
    std::filesystem::rename(temporary, path, ec);
    if (ec) {
        return Result<void>::failure(quest_error("WORLD-FORGE-QUEST-IO", ErrorCategory::Io,
            "Could not finalize World Forge quests write: " + path.generic_string(),
            "Check file permissions; restore from .bak if needed."));
    }
    return Result<void>::success();
}

Result<void> WorldForgeQuestsAsset::validate_file(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    return loaded.value().validate();
}

Result<void> WorldForgeQuestsAsset::validate_file(const std::filesystem::path& path,
    const std::unordered_set<std::string>& known_region_ids) {
    if (!std::filesystem::exists(path)) return Result<void>::success();
    const auto loaded = load(path);
    if (!loaded) return Result<void>::failure(loaded.error());
    if (const auto valid = loaded.value().validate(); !valid) return Result<void>::failure(valid.error());
    return loaded.value().validate_region_refs(known_region_ids);
}

} // namespace engine
