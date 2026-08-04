/**
 * Test-only module that reports a mismatched ABI so host load fails closed.
 */

#include "engine/game/game_module_abi.h"

extern "C" {

ENGINE_GAME_MODULE_API uint32_t game_module_abi_version(void) {
    return ENGINE_GAME_MODULE_ABI_VERSION + 99u;
}

ENGINE_GAME_MODULE_API const char* game_module_name(void) {
    return "abi_mismatch_stub";
}

ENGINE_GAME_MODULE_API bool game_module_init(const EngineGameHostV1* /*host*/) {
    return true;
}

ENGINE_GAME_MODULE_API void game_module_tick(float /*dt*/) {}

ENGINE_GAME_MODULE_API void game_module_shutdown(void) {}

} // extern "C"
