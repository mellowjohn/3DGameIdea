#pragma once

/**
 * C ABI for hot-reloadable game modules (DEC-0053 / TICKET-0257).
 * POD and function pointers only — no STL, C++ classes, or engine types across the boundary.
 *
 * Host resolves exports via LoadLibrary/GetProcAddress (does not link the DLL).
 * Module sources define ENGINE_GAME_MODULE_EXPORTS when building the SHARED target.
 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ENGINE_GAME_MODULE_ABI_VERSION
#define ENGINE_GAME_MODULE_ABI_VERSION 1u
#endif

/** Log severity: 0=info, 1=warning, 2=error. */
enum EngineGameLogLevel {
    ENGINE_GAME_LOG_INFO = 0,
    ENGINE_GAME_LOG_WARNING = 1,
    ENGINE_GAME_LOG_ERROR = 2
};

typedef struct EngineGameHostV1 {
    uint32_t size;
    uint32_t abi_version;
    void* user;
    void (*log)(void* user, int level, const char* message);
    void (*blackboard_set_number)(void* user, const char* key, double value);
    void (*blackboard_set_bool)(void* user, const char* key, bool value);
    bool (*blackboard_get_number)(void* user, const char* key, double* out_value);
    bool (*blackboard_get_bool)(void* user, const char* key, bool* out_value);
} EngineGameHostV1;

#if defined(_WIN32)
#if defined(ENGINE_GAME_MODULE_EXPORTS)
#define ENGINE_GAME_MODULE_API __declspec(dllexport)
#else
#define ENGINE_GAME_MODULE_API
#endif
#else
#define ENGINE_GAME_MODULE_API
#endif

/** Exported by game_module.dll (and test stubs). */
ENGINE_GAME_MODULE_API uint32_t game_module_abi_version(void);
ENGINE_GAME_MODULE_API const char* game_module_name(void);
ENGINE_GAME_MODULE_API bool game_module_init(const EngineGameHostV1* host);
ENGINE_GAME_MODULE_API void game_module_tick(float dt_seconds);
ENGINE_GAME_MODULE_API void game_module_shutdown(void);

#ifdef __cplusplus
}
#endif
