#pragma once

#include "engine/core/result.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace engine {

struct AssetRecord {
    std::string id;
    std::string path;
    std::uintmax_t size = 0;
    std::string content_hash;
    std::vector<std::string> dependencies;
};

enum class AssetChangeKind { Added, Modified, Removed };
struct AssetChange { AssetChangeKind kind; std::string id; std::string path; };

class AssetRegistry final {
public:
    [[nodiscard]] Result<void> scan(const std::filesystem::path& project_root);
    [[nodiscard]] std::vector<EngineError> validate() const;
    [[nodiscard]] std::vector<AssetChange> diff(const AssetRegistry& previous) const;
    [[nodiscard]] Result<bool> write_database_if_changed(const std::filesystem::path& path) const;
    [[nodiscard]] const std::map<std::string, AssetRecord>& records() const noexcept { return records_; }
    [[nodiscard]] std::string to_json() const;
private:
    std::filesystem::path root_;
    std::map<std::string, AssetRecord> records_;
    std::map<std::string, std::string> path_to_id_;
};

class AssetMonitor final {
public:
    [[nodiscard]] Result<std::vector<AssetChange>> poll(const std::filesystem::path& project_root);
private:
    std::optional<AssetRegistry> accepted_snapshot_;
};

} // namespace engine
