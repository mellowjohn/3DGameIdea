/**
 * Sample hot-reloadable game module (TICKET-0257).
 * Links only the C ABI header — no engine_core / ImGui / Jolt / Lua.
 *
 * Rebuild:game_module  then Diagnostics → Game Module → Reload (or enable auto-reload).
 * `game_module_name()` includes a build stamp so reloads are easy to spot.
 */

#include "engine/game/game_module_abi.h"

#include <cstdint>

namespace {

const EngineGameHostV1* g_host = nullptr;
std::uint64_t g_ticks = 0;
// Bump this string (or rebuild id) when verifying live hot-reload without restarting the editor.
constexpr const char* k_module_name = "sample_game_module/build_1";
constexpr double k_build_id = 1.0;

void host_log(int level, const char* message) {
    if (g_host && g_host->log) g_host->log(g_host->user, level, message);
}

void host_set_number(const char* key, double value) {
    if (g_host && g_host->blackboard_set_number) g_host->blackboard_set_number(g_host->user, key, value);
}

void host_set_bool(const char* key, bool value) {
    if (g_host && g_host->blackboard_set_bool) g_host->blackboard_set_bool(g_host->user, key, value);
}

} // namespace

extern "C" {

ENGINE_GAME_MODULE_API uint32_t game_module_abi_version(void) {
    return ENGINE_GAME_MODULE_ABI_VERSION;
}

ENGINE_GAME_MODULE_API const char* game_module_name(void) {
    return k_module_name;
}

ENGINE_GAME_MODULE_API bool game_module_init(const EngineGameHostV1* host) {
    if (!host || host->abi_version != ENGINE_GAME_MODULE_ABI_VERSION) return false;
    if (host->size < sizeof(EngineGameHostV1)) return false;
    g_host = host;
    g_ticks = 0;
    host_log(ENGINE_GAME_LOG_INFO, "sample game_module_init");
    host_set_number("game.module_build_id", k_build_id);
    host_set_bool("game.module_loaded", true);
    host_set_number("game.module_ticks", 0.0);
    return true;
}

ENGINE_GAME_MODULE_API void game_module_tick(float /*dt_seconds*/) {
    ++g_ticks;
    host_set_number("game.module_ticks", static_cast<double>(g_ticks));
    host_set_number("game.module_build_id", k_build_id);
}

ENGINE_GAME_MODULE_API void game_module_shutdown(void) {
    host_log(ENGINE_GAME_LOG_INFO, "sample game_module_shutdown");
    host_set_bool("game.module_loaded", false);
    g_host = nullptr;
    g_ticks = 0;
}

} // extern "C"
