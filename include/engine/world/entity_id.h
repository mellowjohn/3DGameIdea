#pragma once

#include "engine/core/result.h"

#include <compare>
#include <string>

namespace engine {

class EntityId final {
public:
    EntityId() = default;
    [[nodiscard]] static EntityId generate();
    [[nodiscard]] static Result<EntityId> parse(std::string value);
    [[nodiscard]] const std::string& str() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }
    auto operator<=>(const EntityId&) const = default;

private:
    explicit EntityId(std::string value) : value_(std::move(value)) {}
    std::string value_;
};

} // namespace engine

