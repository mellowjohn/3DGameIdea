#include "engine/save/rpg_save.h"

#include "engine/core/error.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

namespace engine {
namespace {

EngineError save_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "rpg_save", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

EngineError save_io_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Io, "rpg_save", std::move(message),
        ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

QuestInstanceStatus parse_quest_status(const std::string& value) {
    if (value == "active") return QuestInstanceStatus::Active;
    if (value == "completed") return QuestInstanceStatus::Completed;
    if (value == "abandoned") return QuestInstanceStatus::Abandoned;
    return QuestInstanceStatus::Inactive;
}

nlohmann::json profile_to_json(const RpgSavePlayerProfile& profile) {
    return nlohmann::json{{"playerSlot", profile.player_slot}, {"displayName", profile.display_name},
        {"archetypeId", profile.archetype_id}, {"appearance", nlohmann::json::object()},
        {"inventory", nlohmann::json::object()}};
}

Result<RpgSavePlayerProfile> profile_from_json(const nlohmann::json& json, int expected_slot) {
    if (!json.is_object()) {
        return Result<RpgSavePlayerProfile>::failure(
            save_error("RPG-SAVE-INVALID-PROFILE", "Player profile must be an object",
                "Fix hostProfile / guestProfile in the save file."));
    }
    RpgSavePlayerProfile profile;
    profile.player_slot = json.value("playerSlot", expected_slot);
    profile.display_name = json.value("displayName", "");
    profile.archetype_id = json.value("archetypeId", "");
    if (profile.player_slot != expected_slot) {
        return Result<RpgSavePlayerProfile>::failure(save_error(
            expected_slot == 0 ? "RPG-SAVE-INVALID-HOST-SLOT" : "RPG-SAVE-INVALID-GUEST-SLOT",
            "playerSlot must be " + std::to_string(expected_slot), "Fix playerSlot on the profile."));
    }
    return Result<RpgSavePlayerProfile>::success(std::move(profile));
}

} // namespace

Result<void> RpgSaveDocument::validate() const {
    if (schema_version != k_schema_version) {
        return Result<void>::failure(save_error("RPG-SAVE-UNSUPPORTED-VERSION",
            "Unsupported schemaVersion " + std::to_string(schema_version),
            "Migrate the save or use a compatible engine build."));
    }
    if (session_mode != SessionMode::Solo && session_mode != SessionMode::Coop) {
        return Result<void>::failure(
            save_error("RPG-SAVE-INVALID-MODE", "sessionMode must be solo or coop", "Fix sessionMode."));
    }
    if (host_profile.player_slot != 0) {
        return Result<void>::failure(save_error("RPG-SAVE-INVALID-HOST-SLOT", "hostProfile.playerSlot must be 0",
            "Set hostProfile.playerSlot to 0."));
    }
    if (session_mode == SessionMode::Solo) {
        if (guest_profile.has_value()) {
            return Result<void>::failure(save_error("RPG-SAVE-SOLO-GUEST-PRESENT",
                "Solo saves must not include guestProfile", "Remove guestProfile or set sessionMode to coop."));
        }
    } else if (!guest_profile.has_value()) {
        return Result<void>::failure(save_error("RPG-SAVE-COOP-MISSING-GUEST",
            "Co-op saves require guestProfile", "Add guestProfile or set sessionMode to solo."));
    } else if (guest_profile->player_slot != 1) {
        return Result<void>::failure(save_error("RPG-SAVE-INVALID-GUEST-SLOT", "guestProfile.playerSlot must be 1",
            "Set guestProfile.playerSlot to 1."));
    }

    const std::size_t companion_cap = session_mode == SessionMode::Coop ? 2 : 3;
    if (shared_campaign.active_companion_ids.size() > companion_cap) {
        return Result<void>::failure(save_error("RPG-SAVE-PARTY-OVER-CAP",
            "activeCompanionIds exceeds mode cap (" + std::to_string(companion_cap) + ")",
            "Trim companions to match solo (3) or coop (2)."));
    }
    return Result<void>::success();
}

std::string RpgSaveDocument::to_json() const {
    nlohmann::json quests = nlohmann::json::object();
    nlohmann::json instances = nlohmann::json::array();
    for (const auto& instance : shared_campaign.quest_instances) {
        instances.push_back({{"questId", instance.quest_id}, {"status", to_string(instance.status)},
            {"completedObjectiveIds", instance.completed_objective_ids}});
    }
    quests["instances"] = std::move(instances);
    quests["outcomeFlags"] = shared_campaign.outcome_flags;

    nlohmann::json standing_scores = nlohmann::json::array();
    for (const auto& score : shared_campaign.standing_scores) {
        standing_scores.push_back({{"factionId", score.faction_id}, {"score", score.score}});
    }

    nlohmann::json shared = {{"quests", std::move(quests)},
        {"standing", {{"scores", std::move(standing_scores)}, {"lockInFactionId", shared_campaign.lock_in_faction_id}}},
        {"morality", {{"score", shared_campaign.morality_score}, {"bandId", shared_campaign.morality_band_id},
            {"unlockedArchetypeIds", shared_campaign.unlocked_archetype_ids}}},
        {"flags", {{"bools", nlohmann::json::object()}, {"strings", nlohmann::json::object()}}},
        {"discovery", {{"revealedRegionIds", nlohmann::json::array()}, {"discoveredPostIds", nlohmann::json::array()},
            {"mapFog", nlohmann::json::object()}}},
        {"party", {{"activeCompanionIds", shared_campaign.active_companion_ids},
            {"maxCompanions", session_mode == SessionMode::Coop ? 2 : 3}}},
        {"camp", {{"unlocked", false}, {"layout", nlohmann::json::object()}}},
        {"economy", {{"gold", shared_campaign.gold}}},
        {"instances", {{"activeInstanceId", ""}, {"returnAnchor", nullptr}}}};

    nlohmann::json root = {{"schemaVersion", schema_version}, {"saveId", save_id}, {"displayName", display_name},
        {"sessionMode", to_string(session_mode)}, {"difficulty", difficulty},
        {"playTimeSeconds", play_time_seconds},
        {"worldAnchor", {{"sceneId", world_anchor.scene_id}, {"regionId", world_anchor.region_id},
            {"position", world_anchor.position}, {"yawDegrees", world_anchor.yaw_degrees}}},
        {"hostProfile", profile_to_json(host_profile)},
        {"guestProfile", guest_profile ? profile_to_json(*guest_profile) : nlohmann::json(nullptr)},
        {"sharedCampaign", std::move(shared)}};
    return root.dump(2);
}

Result<RpgSaveDocument> RpgSaveDocument::from_json(const std::string& text) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        return Result<RpgSaveDocument>::failure(
            save_error("RPG-SAVE-PARSE", std::string("Failed to parse save JSON: ") + ex.what(),
                "Repair or replace the corrupt save file."));
    }
    if (!root.is_object()) {
        return Result<RpgSaveDocument>::failure(
            save_error("RPG-SAVE-PARSE", "Save root must be a JSON object", "Repair the save file."));
    }

    auto migrated = migrate_rpg_save_json(text);
    if (!migrated) return Result<RpgSaveDocument>::failure(migrated.error());
    try {
        root = nlohmann::json::parse(migrated.value());
    } catch (const std::exception& ex) {
        return Result<RpgSaveDocument>::failure(
            save_error("RPG-SAVE-PARSE", std::string("Migrated save failed to parse: ") + ex.what(),
                "Repair the save file."));
    }

    RpgSaveDocument doc;
    doc.schema_version = root.value("schemaVersion", 0);
    doc.save_id = root.value("saveId", "");
    doc.display_name = root.value("displayName", "");
    doc.difficulty = root.value("difficulty", "normal");
    doc.play_time_seconds = root.value("playTimeSeconds", 0ull);
    const auto mode = root.value("sessionMode", "solo");
    if (mode == "coop") doc.session_mode = SessionMode::Coop;
    else if (mode == "solo") doc.session_mode = SessionMode::Solo;
    else {
        return Result<RpgSaveDocument>::failure(
            save_error("RPG-SAVE-INVALID-MODE", "sessionMode must be solo or coop", "Fix sessionMode."));
    }

    if (root.contains("worldAnchor") && root["worldAnchor"].is_object()) {
        const auto& anchor = root["worldAnchor"];
        doc.world_anchor.scene_id = anchor.value("sceneId", "");
        doc.world_anchor.region_id = anchor.value("regionId", "");
        doc.world_anchor.yaw_degrees = anchor.value("yawDegrees", 0.0);
        if (anchor.contains("position") && anchor["position"].is_array() && anchor["position"].size() == 3) {
            doc.world_anchor.position = {anchor["position"][0].get<double>(), anchor["position"][1].get<double>(),
                anchor["position"][2].get<double>()};
        }
    }

    if (!root.contains("hostProfile")) {
        return Result<RpgSaveDocument>::failure(
            save_error("RPG-SAVE-INVALID-PROFILE", "hostProfile is required", "Add hostProfile."));
    }
    auto host = profile_from_json(root["hostProfile"], 0);
    if (!host) return Result<RpgSaveDocument>::failure(host.error());
    doc.host_profile = std::move(host.value());

    if (root.contains("guestProfile") && !root["guestProfile"].is_null()) {
        auto guest = profile_from_json(root["guestProfile"], 1);
        if (!guest) return Result<RpgSaveDocument>::failure(guest.error());
        doc.guest_profile = std::move(guest.value());
    }

    if (root.contains("sharedCampaign") && root["sharedCampaign"].is_object()) {
        const auto& shared = root["sharedCampaign"];
        if (shared.contains("quests") && shared["quests"].is_object()) {
            const auto& quests = shared["quests"];
            if (quests.contains("outcomeFlags") && quests["outcomeFlags"].is_array()) {
                for (const auto& flag : quests["outcomeFlags"]) {
                    if (flag.is_string()) doc.shared_campaign.outcome_flags.push_back(flag.get<std::string>());
                }
            }
            if (quests.contains("instances") && quests["instances"].is_array()) {
                for (const auto& entry : quests["instances"]) {
                    if (!entry.is_object()) continue;
                    RpgSaveQuestInstance instance;
                    instance.quest_id = entry.value("questId", "");
                    instance.status = parse_quest_status(entry.value("status", "inactive"));
                    if (entry.contains("completedObjectiveIds") && entry["completedObjectiveIds"].is_array()) {
                        for (const auto& id : entry["completedObjectiveIds"]) {
                            if (id.is_string()) instance.completed_objective_ids.push_back(id.get<std::string>());
                        }
                    }
                    if (!instance.quest_id.empty() && instance.status != QuestInstanceStatus::Inactive)
                        doc.shared_campaign.quest_instances.push_back(std::move(instance));
                }
            }
        }
        if (shared.contains("standing") && shared["standing"].is_object()) {
            const auto& standing = shared["standing"];
            doc.shared_campaign.lock_in_faction_id = standing.value("lockInFactionId", "");
            if (standing.contains("scores") && standing["scores"].is_array()) {
                for (const auto& entry : standing["scores"]) {
                    if (!entry.is_object()) continue;
                    RpgSaveStandingScore score;
                    score.faction_id = entry.value("factionId", "");
                    score.score = entry.value("score", 0.0);
                    if (!score.faction_id.empty()) doc.shared_campaign.standing_scores.push_back(std::move(score));
                }
            }
        }
        if (shared.contains("morality") && shared["morality"].is_object()) {
            const auto& morality = shared["morality"];
            doc.shared_campaign.morality_score = morality.value("score", 0.0);
            doc.shared_campaign.morality_band_id = morality.value("bandId", "neutral");
            if (morality.contains("unlockedArchetypeIds") && morality["unlockedArchetypeIds"].is_array()) {
                for (const auto& id : morality["unlockedArchetypeIds"]) {
                    if (id.is_string())
                        doc.shared_campaign.unlocked_archetype_ids.push_back(id.get<std::string>());
                }
            }
        }
        if (shared.contains("party") && shared["party"].is_object() &&
            shared["party"].contains("activeCompanionIds") && shared["party"]["activeCompanionIds"].is_array()) {
            for (const auto& id : shared["party"]["activeCompanionIds"]) {
                if (id.is_string()) doc.shared_campaign.active_companion_ids.push_back(id.get<std::string>());
            }
        }
        if (shared.contains("economy") && shared["economy"].is_object())
            doc.shared_campaign.gold = shared["economy"].value("gold", 0.0);
    }

    if (const auto valid = doc.validate(); !valid) return Result<RpgSaveDocument>::failure(valid.error());
    return Result<RpgSaveDocument>::success(std::move(doc));
}

Result<RpgSaveDocument> RpgSaveDocument::load(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<RpgSaveDocument>::failure(save_io_error("RPG-SAVE-READ",
            "Failed to open save: " + path.generic_string(), "Check the path exists and is readable."));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input && !input.eof()) {
        return Result<RpgSaveDocument>::failure(save_io_error("RPG-SAVE-READ",
            "Failed to read save: " + path.generic_string(), "Check disk permissions."));
    }
    return from_json(buffer.str());
}

Result<void> RpgSaveDocument::save(const std::filesystem::path& path) const {
    if (const auto valid = validate(); !valid) return valid;
    try {
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        const auto temporary = std::filesystem::path(path.string() + ".tmp");
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                return Result<void>::failure(save_io_error("RPG-SAVE-WRITE",
                    "Failed to open temp save for write", "Check disk space and permissions."));
            }
            output << to_json();
            if (!output) {
                return Result<void>::failure(save_io_error("RPG-SAVE-WRITE", "Failed while writing temp save",
                    "Check disk space and permissions."));
            }
        }
        std::error_code ec;
        std::filesystem::rename(temporary, path, ec);
        if (ec) {
            std::filesystem::remove(path, ec);
            std::filesystem::rename(temporary, path, ec);
            if (ec) {
                return Result<void>::failure(save_io_error("RPG-SAVE-WRITE",
                    "Atomic rename failed: " + ec.message(), "Retry save or free the destination path."));
            }
        }
        return Result<void>::success();
    } catch (const std::exception& ex) {
        return Result<void>::failure(
            save_io_error("RPG-SAVE-WRITE", std::string("Save failed: ") + ex.what(), "Retry the save."));
    }
}

Result<void> RpgSaveDocument::capture_from(const QuestRuntime& quests, const StandingRuntime& standing,
    const FlagRuntime& flags, const PartyRuntime* party) {
    shared_campaign.quest_instances.clear();
    for (const auto& instance : quests.list_instances()) {
        RpgSaveQuestInstance saved;
        saved.quest_id = instance.quest_id;
        saved.status = instance.status;
        saved.completed_objective_ids = instance.completed_objective_ids;
        shared_campaign.quest_instances.push_back(std::move(saved));
    }
    shared_campaign.outcome_flags = flags.list();
    shared_campaign.standing_scores.clear();
    for (const auto& entry : standing.list_tracked()) {
        shared_campaign.standing_scores.push_back({entry.faction_id, entry.score});
    }
    if (auto locked = standing.lock_in_faction(); locked)
        shared_campaign.lock_in_faction_id = locked.value();
    if (party) shared_campaign.active_companion_ids = party->list_active();
    return Result<void>::success();
}

Result<void> RpgSaveDocument::hydrate_into(QuestRuntime& quests, StandingRuntime& standing, FlagRuntime& flags,
    PartyRuntime* party) const {
    if (const auto valid = validate(); !valid) return valid;
    quests.reset();
    for (const auto& instance : shared_campaign.quest_instances) {
        if (const auto restored =
                quests.restore_instance(instance.quest_id, instance.status, instance.completed_objective_ids);
            !restored)
            return restored;
    }
    flags.restore(shared_campaign.outcome_flags);
    standing.reset();
    for (const auto& score : shared_campaign.standing_scores) {
        if (const auto set = standing.set(score.faction_id, score.score); !set) return set;
    }
    if (party) {
        if (const auto set = party->set_active_companions(shared_campaign.active_companion_ids); !set) return set;
    } else if (shared_campaign.active_companion_ids.size() > (session_mode == SessionMode::Coop ? 2u : 3u)) {
        return Result<void>::failure(save_error("RPG-SAVE-PARTY-OVER-CAP",
            "activeCompanionIds exceeds mode cap", "Trim companions before hydrate."));
    }
    return Result<void>::success();
}

Result<std::string> migrate_rpg_save_json(std::string json_text, int* out_from_version) {
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json_text);
    } catch (const std::exception& ex) {
        return Result<std::string>::failure(
            save_error("RPG-SAVE-PARSE", std::string("Failed to parse save JSON: ") + ex.what(),
                "Repair or replace the corrupt save file."));
    }
    const int version = root.value("schemaVersion", 0);
    if (out_from_version) *out_from_version = version;
    if (version == RpgSaveDocument::k_schema_version) return Result<std::string>::success(std::move(json_text));
    // Stub: future migrate_vN_to_vN+1 chains land here.
    return Result<std::string>::failure(save_error("RPG-SAVE-UNSUPPORTED-VERSION",
        "Unsupported schemaVersion " + std::to_string(version),
        "No migration path from this version yet."));
}

Result<void> apply_rpg_save_to_session(const RpgSaveDocument& doc, GameSession& session, bool guest_connected) {
    if (const auto valid = doc.validate(); !valid) return valid;
    session.reset_to_menu();
    if (doc.session_mode == SessionMode::Solo) {
        if (const auto begin = session.begin_solo(); !begin) return begin;
        return session.start_playing();
    }
    if (const auto begin = session.begin_coop_lobby(); !begin) return begin;
    (void)session.set_slot_connected(0, true, 0);
    if (!guest_connected) {
        // Lobby-only: fail closed to playing (DEC-0042).
        return Result<void>::success();
    }
    (void)session.set_slot_connected(1, true, 1);
    return session.start_playing();
}

} // namespace engine
