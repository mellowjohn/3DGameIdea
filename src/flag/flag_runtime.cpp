#include "engine/flag/flag_runtime.h"

#include "engine/core/error.h"

#include <algorithm>

namespace engine {
namespace {

EngineError flag_error(std::string code, std::string message, std::string remedy) {
    return EngineError{std::move(code), Severity::Error, ErrorCategory::Validation, "flag_runtime",
        std::move(message), ENGINE_SOURCE_CONTEXT, {}, std::move(remedy), make_correlation_id()};
}

} // namespace

Result<void> FlagRuntime::set(const std::string& flag_id) {
    if (flag_id.empty()) {
        return Result<void>::failure(flag_error("FLAG-RUNTIME-EMPTY", "Flag id must be non-empty",
            "Pass a story/outcome flag id such as act0.helped_larrell."));
    }
    flags_.insert(flag_id);
    return Result<void>::success();
}

Result<void> FlagRuntime::clear(const std::string& flag_id) {
    if (flag_id.empty()) {
        return Result<void>::failure(flag_error("FLAG-RUNTIME-EMPTY", "Flag id must be non-empty",
            "Pass the flag id to clear."));
    }
    flags_.erase(flag_id);
    return Result<void>::success();
}

Result<void> FlagRuntime::apply_many(const std::vector<std::string>& flag_ids) {
    for (const auto& flag_id : flag_ids) {
        if (const auto set = this->set(flag_id); !set) return set;
    }
    return Result<void>::success();
}

bool FlagRuntime::has(const std::string& flag_id) const noexcept {
    if (flag_id.empty()) return false;
    return flags_.find(flag_id) != flags_.end();
}

std::vector<std::string> FlagRuntime::list() const {
    std::vector<std::string> out(flags_.begin(), flags_.end());
    std::sort(out.begin(), out.end());
    return out;
}

void FlagRuntime::reset() noexcept { flags_.clear(); }

void FlagRuntime::restore(std::vector<std::string> flag_ids) {
    flags_.clear();
    for (auto& flag_id : flag_ids) {
        if (!flag_id.empty()) flags_.insert(std::move(flag_id));
    }
}

} // namespace engine
