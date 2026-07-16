#include "engine/dialogue/twee_import.h"

#include "engine/core/error.h"
#include "engine/core/id_slug.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace engine {
namespace {

EngineError twee_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "twee_import",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

std::string trim_copy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

std::string slugify(std::string title) {
    auto out = slugify_id(title);
    return out.empty() ? "node" : out;
}

std::string strip_quotes(std::string value) {
    value = trim_copy(std::move(value));
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

struct Passage {
    std::string title;
    std::string body;
};

std::unordered_map<std::string, Passage> parse_twee(const std::string& text) {
    std::unordered_map<std::string, Passage> passages;
    std::string current_title;
    std::vector<std::string> body_lines;
    const std::regex header_re(R"(^::\s*(.+?)(?:\s+(\{.*\}))?\s*$)");

    auto flush = [&]() {
        if (current_title.empty()) return;
        if (current_title == "StoryTitle" || current_title == "StoryData") {
            current_title.clear();
            body_lines.clear();
            return;
        }
        std::ostringstream body;
        for (std::size_t i = 0; i < body_lines.size(); ++i) {
            if (i) body << '\n';
            body << body_lines[i];
        }
        Passage passage;
        passage.title = current_title;
        passage.body = trim_copy(body.str());
        passages[current_title] = std::move(passage);
        current_title.clear();
        body_lines.clear();
    };

    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::smatch match;
        if (std::regex_match(line, match, header_re)) {
            flush();
            std::string title = trim_copy(match[1].str());
            if (!title.empty() && title.front() == '\\') title = trim_copy(title.substr(1));
            title = strip_quotes(std::move(title));
            current_title = std::move(title);
            continue;
        }
        if (!current_title.empty()) body_lines.push_back(line);
    }
    flush();
    return passages;
}

struct ExtractedLinks {
    std::string line;
    std::vector<std::pair<std::string, std::string>> choices; // label, dest title
};

ExtractedLinks extract_links(std::string body) {
    ExtractedLinks out;
    // [[ "label" | target ]] or [[label|target]] or [[label]]
    const std::regex link_re(R"(\[\[(?:\"([^\"]+)\"|([^\]|]+))(?:\|([^\]]+))?\]\])");
    std::sregex_iterator it(body.begin(), body.end(), link_re);
    const std::sregex_iterator end;
    for (; it != end; ++it) {
        const auto& m = *it;
        std::string label = trim_copy(m[1].matched ? m[1].str() : m[2].str());
        std::string dest = m[3].matched ? trim_copy(m[3].str()) : label;
        dest = strip_quotes(std::move(dest));
        out.choices.emplace_back(label, dest);
    }
    out.line = std::regex_replace(body, link_re, "");
    out.line = std::regex_replace(out.line, std::regex(R"(\(\(OOC[^)]*\)\))"), "");
    out.line = std::regex_replace(out.line, std::regex(R"(\(OOC[^)]*\)\)?)"), "");
    out.line = std::regex_replace(out.line, std::regex(R"(\n{3,})"), "\n\n");
    out.line = trim_copy(std::move(out.line));
    return out;
}

std::string infer_speaker(const std::string& line, const std::string& title) {
    std::string lower = line + " " + title;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lower.find("creotar") != std::string::npos &&
        (lower.find("whisper") != std::string::npos || lower.find("silhouette") != std::string::npos ||
            lower.find("my name") != std::string::npos))
        return "creotar";
    if (lower.find("commander") != std::string::npos || lower.find("grenge") != std::string::npos)
        return "commander_grenge";
    if (lower.find("larrell") != std::string::npos) return "sergeant_larrell";
    if (title == "Prologue") return "narrator";
    if (title.find("Realm of Darkness") != std::string::npos || title.find("Frangitur") != std::string::npos)
        return "frangitur";
    if (title.rfind("Tutorial", 0) == 0)
        return lower.find("arkand") != std::string::npos ? "arkand" : "narrator";
    if (std::regex_search(line, std::regex(R"(Arkand.*states|Arkand.*replies|Arkand Shouts)", std::regex::icase)))
        return "arkand";
    return "narrator";
}

} // namespace

Result<WorldForgeDialogueTree> import_twee_dialogue_tree(const std::string& twee_text,
    const TweeImportOptions& options) {
    if (options.tree_id.empty()) {
        return Result<WorldForgeDialogueTree>::failure(
            twee_error("TWEE-IMPORT-TREE-ID", "treeId is required for Twine import", "Pass treeId for the dialogue tree."));
    }
    auto passages = parse_twee(twee_text);
    if (passages.empty()) {
        return Result<WorldForgeDialogueTree>::failure(twee_error("TWEE-IMPORT-EMPTY",
            "No Twine passages found in source", "Provide a .twee file with :: Passage headers."));
    }

    std::unordered_map<std::string, std::string> title_to_id;
    std::unordered_map<std::string, int> seen;
    for (const auto& entry : passages) {
        auto id = slugify(entry.first);
        if (seen.count(id)) {
            ++seen[id];
            id = id + "_" + std::to_string(seen[id]);
        } else {
            seen[id] = 1;
        }
        title_to_id[entry.first] = id;
    }

    WorldForgeDialogueTree tree;
    tree.id = options.tree_id;
    tree.display_name = options.display_name.empty() ? options.tree_id : options.display_name;
    tree.parent_quest_id = options.parent_quest_id;
    tree.canon_status = WorldForgeDialogueCanonStatus::Draft;
    tree.summary = "Imported from Twine (.twee). Canon stays draft until review.";
    tree.story_ref = options.story_ref.empty() ? options.source_label : options.story_ref;
    tree.tags = {"twine-import"};

    for (const auto& entry : passages) {
        const auto extracted = extract_links(entry.second.body);
        WorldForgeDialogueNode node;
        node.id = title_to_id[entry.first];
        node.speaker_id = infer_speaker(extracted.line, entry.first);
        node.line = extracted.line.empty() ? "((" + entry.first + "))" : extracted.line;
        for (std::size_t i = 0; i < extracted.choices.size(); ++i) {
            const auto& choice_pair = extracted.choices[i];
            WorldForgeDialogueChoice choice;
            choice.id = node.id + "_c" + std::to_string(i + 1);
            choice.text = choice_pair.first;
            auto target = title_to_id.find(choice_pair.second);
            std::string next_id;
            if (target != title_to_id.end()) {
                next_id = target->second;
            } else {
                for (const auto& map_entry : title_to_id) {
                    if (strip_quotes(map_entry.first) == strip_quotes(choice_pair.second)) {
                        next_id = map_entry.second;
                        break;
                    }
                }
            }
            choice.next_node_id = std::move(next_id);
            node.choices.push_back(std::move(choice));
        }
        tree.nodes.push_back(std::move(node));
    }

    std::sort(tree.nodes.begin(), tree.nodes.end(),
        [](const WorldForgeDialogueNode& a, const WorldForgeDialogueNode& b) { return a.id < b.id; });

    if (!options.entry_node_id.empty()) {
        tree.entry_node_id = options.entry_node_id;
    } else if (title_to_id.count("Prologue")) {
        tree.entry_node_id = title_to_id["Prologue"];
    } else if (title_to_id.count("prologue")) {
        tree.entry_node_id = title_to_id["prologue"];
    } else if (!tree.nodes.empty()) {
        tree.entry_node_id = tree.nodes.front().id;
    }

    WorldForgeDialoguesAsset probe;
    probe.schema_version = 1;
    probe.id = "import_probe";
    probe.trees.push_back(tree);
    if (const auto valid = probe.validate(); !valid) return Result<WorldForgeDialogueTree>::failure(valid.error());
    return Result<WorldForgeDialogueTree>::success(std::move(tree));
}

Result<WorldForgeDialogueTree> import_twee_dialogue_tree_file(const std::filesystem::path& twee_path,
    const TweeImportOptions& options) {
    std::ifstream input(twee_path, std::ios::binary);
    if (!input) {
        return Result<WorldForgeDialogueTree>::failure(twee_error("TWEE-IMPORT-READ",
            "Failed to read Twine file: " + twee_path.generic_string(), "Check the path and try again."));
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    TweeImportOptions resolved = options;
    if (resolved.source_label.empty()) resolved.source_label = twee_path.generic_string();
    if (resolved.story_ref.empty()) resolved.story_ref = resolved.source_label;
    return import_twee_dialogue_tree(buffer.str(), resolved);
}

Result<void> upsert_dialogue_tree(WorldForgeDialoguesAsset& asset, WorldForgeDialogueTree tree) {
    if (tree.id.empty()) {
        return Result<void>::failure(
            twee_error("TWEE-IMPORT-TREE-ID", "Dialogue tree id is required", "Provide treeId."));
    }
    bool replaced = false;
    for (auto& existing : asset.trees) {
        if (existing.id == tree.id) {
            existing = std::move(tree);
            replaced = true;
            break;
        }
    }
    if (!replaced) asset.trees.push_back(std::move(tree));
    if (asset.id.empty()) asset.id = "tessera_dialogues";
    if (asset.schema_version <= 0) asset.schema_version = 1;
    return asset.validate();
}

} // namespace engine
