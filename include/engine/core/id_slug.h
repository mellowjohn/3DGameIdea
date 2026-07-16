#pragma once

#include <string>
#include <string_view>

namespace engine {

/// Derive a stable snake_case id from a display title/name.
/// Example: "Go Get the flowers" → "go_get_the_flowers"
[[nodiscard]] std::string slugify_id(std::string_view title);

/// Like `slugify_id`, but appends `_2`, `_3`, … while `exists(candidate)` is true.
template <typename ExistsFn>
[[nodiscard]] std::string unique_slugify_id(std::string_view title, ExistsFn&& exists,
    std::string_view fallback = "item") {
    std::string base = slugify_id(title);
    if (base.empty()) base = std::string(fallback);
    if (!exists(base)) return base;
    for (int i = 2; i < 10000; ++i) {
        const auto candidate = base + "_" + std::to_string(i);
        if (!exists(candidate)) return candidate;
    }
    return base + "_x";
}

} // namespace engine
