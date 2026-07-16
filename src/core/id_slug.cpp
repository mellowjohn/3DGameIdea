#include "engine/core/id_slug.h"

#include <cctype>

namespace engine {

std::string slugify_id(std::string_view title) {
    std::string out;
    out.reserve(title.size());
    bool prev_us = false;
    for (unsigned char c : title) {
        if (std::isspace(c) || c == '-' || c == '/' || c == '\\' || c == ':' || c == ',' || c == '.' || c == '\'') {
            if (!prev_us && !out.empty()) {
                out.push_back('_');
                prev_us = true;
            }
            continue;
        }
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
            prev_us = false;
        } else if (!prev_us && !out.empty()) {
            out.push_back('_');
            prev_us = true;
        }
    }
    while (!out.empty() && out.front() == '_') out.erase(out.begin());
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out;
}

} // namespace engine
