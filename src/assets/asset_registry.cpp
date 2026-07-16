#include "engine/assets/asset_registry.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>

namespace engine {
namespace {
std::string normalized(std::filesystem::path value) {
    auto result = value.lexically_normal().generic_string();
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
std::string asset_id(const std::string& path) {
    std::uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : path) { hash ^= c; hash *= 1099511628211ull; }
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(16) << hash; return out.str();
}
std::string file_hash(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::uint64_t hash = 14695981039346656037ull;
    char buffer[8192];
    while (input) {
        input.read(buffer, sizeof(buffer));
        for (std::streamsize i = 0; i < input.gcount(); ++i) { hash ^= static_cast<unsigned char>(buffer[i]); hash *= 1099511628211ull; }
    }
    std::ostringstream out; out << std::hex << std::setfill('0') << std::setw(16) << hash; return out.str();
}
EngineError asset_error(std::string code, std::string message) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::AssetImport, "assets", std::move(message),
                       ENGINE_SOURCE_CONTEXT, {}, "Correct the asset path or dependency sidecar and rescan.", make_correlation_id()};
}
}

Result<void> AssetRegistry::scan(const std::filesystem::path& project_root) {
    root_ = project_root;
    records_.clear(); path_to_id_.clear();
    const auto asset_root = root_ / "assets";
    if (!std::filesystem::exists(asset_root)) return Result<void>::success();
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(asset_root)) {
            if (!entry.is_regular_file() || entry.path().extension() == ".meta") continue;
            const auto relative = normalized(std::filesystem::relative(entry.path(), root_));
            const auto id = asset_id(relative);
            if (const auto found = records_.find(id); found != records_.end() && found->second.path != relative)
                return Result<void>::failure(asset_error("ASSET-ID-COLLISION", "Two asset paths produced ID " + id));
            AssetRecord record{id, relative, entry.file_size(), file_hash(entry.path()), {}};
            const auto sidecar = entry.path().string() + ".meta";
            if (std::filesystem::exists(sidecar)) {
                std::ifstream input(sidecar);
                nlohmann::json metadata; input >> metadata;
                for (const auto& dependency : metadata.value("dependencies", std::vector<std::string>{}))
                    record.dependencies.push_back(normalized(dependency));
                std::sort(record.dependencies.begin(), record.dependencies.end());
                record.dependencies.erase(std::unique(record.dependencies.begin(), record.dependencies.end()), record.dependencies.end());
            }
            records_[id] = record;
            path_to_id_[relative] = id;
        }
        return Result<void>::success();
    } catch (const std::exception& exception) {
        auto error = asset_error("ASSET-SCAN-FAILED", "Asset scan failed"); error.causes.push_back(exception.what());
        return Result<void>::failure(std::move(error));
    }
}

std::vector<EngineError> AssetRegistry::validate() const {
    std::vector<EngineError> errors;
    for (const auto& entry : records_)
        for (const auto& dependency : entry.second.dependencies)
            if (path_to_id_.find(dependency) == path_to_id_.end())
                errors.push_back(asset_error("ASSET-DEPENDENCY-MISSING", entry.second.path + " depends on missing " + dependency));
    enum class Visit { Active, Done };
    std::map<std::string, Visit> visits;
    std::function<void(const std::string&, const std::string&)> visit = [&](const std::string& path, const std::string& origin) {
        const auto state = visits.find(path);
        if (state != visits.end()) {
            if (state->second == Visit::Active) errors.push_back(asset_error("ASSET-DEPENDENCY-CYCLE", origin + " participates in a dependency cycle through " + path));
            return;
        }
        visits[path] = Visit::Active;
        const auto id = path_to_id_.find(path);
        if (id != path_to_id_.end())
            for (const auto& dependency : records_.at(id->second).dependencies) visit(dependency, origin);
        visits[path] = Visit::Done;
    };
    for (const auto& entry : path_to_id_) visit(entry.first, entry.first);
    return errors;
}

std::vector<AssetChange> AssetRegistry::diff(const AssetRegistry& previous) const {
    std::vector<AssetChange> changes;
    for (const auto& entry : records_) {
        const auto old = previous.records_.find(entry.first);
        if (old == previous.records_.end()) changes.push_back({AssetChangeKind::Added, entry.first, entry.second.path});
        else if (old->second.content_hash != entry.second.content_hash || old->second.dependencies != entry.second.dependencies)
            changes.push_back({AssetChangeKind::Modified, entry.first, entry.second.path});
    }
    for (const auto& entry : previous.records_)
        if (records_.find(entry.first) == records_.end()) changes.push_back({AssetChangeKind::Removed, entry.first, entry.second.path});
    return changes;
}

Result<bool> AssetRegistry::write_database_if_changed(const std::filesystem::path& path) const {
    try {
        const auto content = to_json();
        if (std::filesystem::exists(path)) {
            std::ifstream input(path, std::ios::binary);
            if (std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) == content)
                return Result<bool>::success(false);
        }
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        const auto temporary = path.string() + ".tmp";
        { std::ofstream output(temporary, std::ios::binary | std::ios::trunc); output << content; if (!output) throw std::runtime_error("write failed"); }
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
        std::filesystem::rename(temporary, path);
        return Result<bool>::success(true);
    } catch (const std::exception& exception) {
        auto error = asset_error("ASSET-DATABASE-WRITE", "Could not write compiled asset database"); error.causes.push_back(exception.what());
        return Result<bool>::failure(std::move(error));
    }
}

std::string AssetRegistry::to_json() const {
    nlohmann::ordered_json root{{"schemaVersion", 1}, {"assets", nlohmann::ordered_json::array()}};
    for (const auto& entry : records_) root["assets"].push_back({{"id", entry.second.id}, {"path", entry.second.path},
        {"size", entry.second.size}, {"contentHash", entry.second.content_hash}, {"dependencies", entry.second.dependencies}});
    return root.dump(2) + "\n";
}

Result<std::vector<AssetChange>> AssetMonitor::poll(const std::filesystem::path& project_root) {
    AssetRegistry candidate;
    auto scanned = candidate.scan(project_root);
    if (!scanned) return Result<std::vector<AssetChange>>::failure(scanned.error());
    const auto errors = candidate.validate();
    if (!errors.empty()) return Result<std::vector<AssetChange>>::failure(errors.front());
    AssetRegistry empty;
    auto changes = candidate.diff(accepted_snapshot_ ? *accepted_snapshot_ : empty);
    accepted_snapshot_ = std::move(candidate);
    return Result<std::vector<AssetChange>>::success(std::move(changes));
}

} // namespace engine
