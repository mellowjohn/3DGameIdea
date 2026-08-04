#include "engine/assets/item_catalog_asset.h"

#include "engine/core/error.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace engine {
namespace {

EngineError catalog_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "item_catalog",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

void rebuild_index(ItemCatalogAsset& asset) {
    asset.by_id.clear();
    for (std::size_t i = 0; i < asset.entities.size(); ++i) {
        asset.by_id[asset.entities[i].id] = i;
    }
}

} // namespace

const ItemDef* ItemCatalogAsset::find(const std::string& item_id) const {
    const auto it = by_id.find(item_id);
    if (it == by_id.end()) return nullptr;
    return &entities[it->second];
}

ItemDef* ItemCatalogAsset::find_mutable(const std::string& item_id) {
    const auto it = by_id.find(item_id);
    if (it == by_id.end()) return nullptr;
    return &entities[it->second];
}

const ItemLootTable* ItemCatalogAsset::find_loot_table(const std::string& table_id) const {
    for (const auto& table : loot_tables) {
        if (table.id == table_id) return &table;
    }
    return nullptr;
}

Result<ItemCatalogAsset> ItemCatalogAsset::parse(const std::string& text, const std::string& source_name) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(text);
    } catch (const std::exception& ex) {
        return Result<ItemCatalogAsset>::failure(
            catalog_error("ITEM-JSON", std::string("Failed to parse ") + source_name + ": " + ex.what(),
                "Fix JSON syntax."));
    }
    if (!json.is_object()) {
        return Result<ItemCatalogAsset>::failure(
            catalog_error("ITEM-ROOT", "Item catalog root must be an object", "Wrap entities in an object."));
    }

    ItemCatalogAsset asset;
    asset.schema_version = json.value("schemaVersion", 1);
    asset.id = json.value("id", std::string{});
    asset.notes = json.value("notes", std::string{});

    if (json.contains("entities") && json["entities"].is_array()) {
        for (const auto& node : json["entities"]) {
            if (!node.is_object()) {
                return Result<ItemCatalogAsset>::failure(
                    catalog_error("ITEM-ENTITY", "Each entity must be an object", "Fix entities array."));
            }
            ItemDef def;
            def.id = node.value("id", std::string{});
            if (def.id.empty()) {
                return Result<ItemCatalogAsset>::failure(
                    catalog_error("ITEM-ID", "Item id is required", "Set id from display name slug."));
            }
            def.display_name = node.value("displayName", def.id);
            def.kind = node.value("kind", std::string{"material"});
            def.icon = node.value("icon", std::string{});
            def.world_mesh = node.value("worldMesh", std::string{});
            def.icon_only = node.value("iconOnly", false);
            def.source = node.value("source", std::string{});
            def.notes = node.value("notes", std::string{});
            if (node.contains("tags") && node["tags"].is_array()) {
                for (const auto& tag : node["tags"]) {
                    if (tag.is_string()) def.tags.push_back(tag.get<std::string>());
                }
            }
            if (node.contains("handAttach") && node["handAttach"].is_object()) {
                const auto& ha = node["handAttach"];
                def.hand_attach.joint = ha.value("joint", std::string{});
                if (ha.contains("gripOffset") && ha["gripOffset"].is_array() && ha["gripOffset"].size() >= 3) {
                    def.hand_attach.grip_offset = {ha["gripOffset"][0].get<float>(), ha["gripOffset"][1].get<float>(),
                        ha["gripOffset"][2].get<float>()};
                }
                if (ha.contains("gripEulerDeg") && ha["gripEulerDeg"].is_array() && ha["gripEulerDeg"].size() >= 3) {
                    def.hand_attach.grip_euler_deg = {ha["gripEulerDeg"][0].get<float>(),
                        ha["gripEulerDeg"][1].get<float>(), ha["gripEulerDeg"][2].get<float>()};
                }
                if (ha.contains("gripScale") && ha["gripScale"].is_array() && ha["gripScale"].size() >= 3) {
                    def.hand_attach.grip_scale = {ha["gripScale"][0].get<float>(), ha["gripScale"][1].get<float>(),
                        ha["gripScale"][2].get<float>()};
                }
                for (float& axis : def.hand_attach.grip_scale) {
                    if (!(std::fabs(axis) > 1e-4f)) axis = 1.0f;
                }
                def.hand_attach.draw_clip = ha.value("drawClip", std::string{});
            }
            asset.entities.push_back(std::move(def));
        }
    }

    if (json.contains("lootTables") && json["lootTables"].is_array()) {
        for (const auto& node : json["lootTables"]) {
            if (!node.is_object()) continue;
            ItemLootTable table;
            table.id = node.value("id", std::string{});
            if (node.contains("containerKinds") && node["containerKinds"].is_array()) {
                for (const auto& kind : node["containerKinds"]) {
                    if (kind.is_string()) table.container_kinds.push_back(kind.get<std::string>());
                }
            }
            if (node.contains("entries") && node["entries"].is_array()) {
                for (const auto& entry_node : node["entries"]) {
                    if (!entry_node.is_object()) continue;
                    ItemLootEntry entry;
                    entry.item_id = entry_node.value("itemId", std::string{});
                    entry.count = entry_node.value("count", 1);
                    entry.weight = entry_node.value("weight", 1);
                    if (!entry.item_id.empty() && entry.count > 0 && entry.weight > 0)
                        table.entries.push_back(std::move(entry));
                }
            }
            if (!table.id.empty()) asset.loot_tables.push_back(std::move(table));
        }
    }

    rebuild_index(asset);
    return Result<ItemCatalogAsset>::success(std::move(asset));
}

Result<ItemCatalogAsset> ItemCatalogAsset::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        return Result<ItemCatalogAsset>::failure(
            catalog_error("ITEM-IO", "Failed to open item catalog: " + path.generic_string(),
                "Check the path exists."));
    }
    const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return parse(text, path.generic_string());
}

Result<void> save_item_hand_attach(const std::filesystem::path& project_root, const ItemDef& def) {
    if (def.id.empty()) {
        return Result<void>::failure(
            catalog_error("ITEM-SAVE-ID", "Cannot save handAttach without item id", "Select a hotbar item first."));
    }
    if (def.source_path.empty()) {
        return Result<void>::failure(catalog_error("ITEM-SAVE-SOURCE",
            "Item has no source_path for handAttach save",
            "Reload the item catalog from assets/items/*.json."));
    }
    const auto absolute = (project_root / def.source_path).lexically_normal();
    nlohmann::json json;
    {
        // Close the read handle before opening for trunc write — Windows often fails ofstream::trunc while
        // an ifstream on the same path remains open (ITEM-SAVE-WRITE with "permissions" noise).
        std::ifstream input(absolute, std::ios::binary);
        if (!input) {
            return Result<void>::failure(catalog_error("ITEM-SAVE-IO",
                "Failed to open item catalog for write: " + absolute.generic_string(),
                "Check the path exists and project root is absolute."));
        }
        try {
            input >> json;
        } catch (const std::exception& ex) {
            return Result<void>::failure(catalog_error("ITEM-SAVE-PARSE",
                std::string("Failed to parse catalog before save: ") + ex.what(), "Fix JSON syntax."));
        }
    }
    if (!json.contains("entities") || !json["entities"].is_array()) {
        return Result<void>::failure(
            catalog_error("ITEM-SAVE-ENTITIES", "Catalog missing entities array", "Repair the items JSON."));
    }
    bool found = false;
    for (auto& node : json["entities"]) {
        if (!node.is_object() || node.value("id", std::string{}) != def.id) continue;
        nlohmann::json ha = nlohmann::json::object();
        if (!def.hand_attach.joint.empty()) ha["joint"] = def.hand_attach.joint;
        ha["gripOffset"] = {def.hand_attach.grip_offset[0], def.hand_attach.grip_offset[1],
            def.hand_attach.grip_offset[2]};
        ha["gripEulerDeg"] = {def.hand_attach.grip_euler_deg[0], def.hand_attach.grip_euler_deg[1],
            def.hand_attach.grip_euler_deg[2]};
        ha["gripScale"] = {def.hand_attach.grip_scale[0], def.hand_attach.grip_scale[1],
            def.hand_attach.grip_scale[2]};
        if (!def.hand_attach.draw_clip.empty()) ha["drawClip"] = def.hand_attach.draw_clip;
        else if (node.contains("handAttach") && node["handAttach"].is_object()
            && node["handAttach"].contains("drawClip") && node["handAttach"]["drawClip"].is_string()) {
            // Preserve authored drawClip when grips are saved without reloading the optional field into live state.
            ha["drawClip"] = node["handAttach"]["drawClip"].get<std::string>();
        }
        node["handAttach"] = std::move(ha);
        found = true;
        break;
    }
    if (!found) {
        return Result<void>::failure(catalog_error("ITEM-SAVE-MISSING",
            "Item id not found in " + def.source_path, "Confirm the item lives in that catalog file."));
    }

    const auto temp = absolute.string() + ".tmp";
    {
        std::ofstream output(temp, std::ios::trunc | std::ios::binary);
        if (!output) {
            return Result<void>::failure(catalog_error("ITEM-SAVE-WRITE",
                "Failed to write item catalog temp: " + temp, "Check directory write permissions."));
        }
        output << json.dump(2) << '\n';
        if (!output) {
            return Result<void>::failure(catalog_error("ITEM-SAVE-WRITE",
                "Failed while writing item catalog temp: " + temp, "Check free disk space."));
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, absolute, ec);
    if (ec) {
        // Windows rename-over-existing may need remove first on some filesystems.
        std::error_code remove_ec;
        std::filesystem::remove(absolute, remove_ec);
        std::filesystem::rename(temp, absolute, ec);
    }
    if (ec) {
        std::error_code cleanup;
        std::filesystem::remove(temp, cleanup);
        return Result<void>::failure(catalog_error("ITEM-SAVE-WRITE",
            "Failed to replace item catalog: " + absolute.generic_string() + " (" + ec.message() + ")",
            "Close other editors using the file and retry."));
    }
    return Result<void>::success();
}

Result<ItemCatalogAsset> load_project_item_catalog(const std::filesystem::path& project_root) {
    ItemCatalogAsset merged;
    merged.id = "project_items";
    const auto dir = project_root / "assets" / "items";
    if (!std::filesystem::is_directory(dir)) {
        return Result<ItemCatalogAsset>::success(std::move(merged));
    }
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        auto loaded = ItemCatalogAsset::load(path);
        if (!loaded) return loaded;
        auto& part = loaded.value();
        const auto rel = std::filesystem::relative(path, project_root).generic_string();
        if (merged.id == "project_items" && !part.id.empty()) merged.id = part.id;
        if (!part.notes.empty()) {
            if (!merged.notes.empty()) merged.notes += "\n";
            merged.notes += part.notes;
        }
        for (auto& def : part.entities) {
            def.source_path = rel;
            if (const auto it = merged.by_id.find(def.id); it != merged.by_id.end()) {
                merged.entities[it->second] = std::move(def);
            } else {
                merged.by_id[def.id] = merged.entities.size();
                merged.entities.push_back(std::move(def));
            }
        }
        for (auto& table : part.loot_tables) {
            bool replaced = false;
            for (auto& existing : merged.loot_tables) {
                if (existing.id == table.id) {
                    existing = std::move(table);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) merged.loot_tables.push_back(std::move(table));
        }
        rebuild_index(merged);
    }
    return Result<ItemCatalogAsset>::success(std::move(merged));
}

} // namespace engine
