#pragma once

#include "engine/core/result.h"
#include "engine/game/game_module_abi.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace engine {

class LuaRuntime;

/**
 * Loads a copy of a game-module DLL so MSBuild can overwrite the canonical path
 * while a previous generation remains mapped. Tick is single-threaded from the host.
 */
class GameModuleHost {
public:
    GameModuleHost() = default;
    ~GameModuleHost();
    GameModuleHost(const GameModuleHost&) = delete;
    GameModuleHost& operator=(const GameModuleHost&) = delete;
    GameModuleHost(GameModuleHost&& other) noexcept;
    GameModuleHost& operator=(GameModuleHost&& other) noexcept;

    /// Prefer the DLL next to engine.exe; override with an explicit path when needed.
    [[nodiscard]] static std::filesystem::path default_module_path();

    void set_lua_runtime(LuaRuntime* runtime) noexcept { lua_ = runtime; }
    [[nodiscard]] LuaRuntime* lua_runtime() const noexcept { return lua_; }

    void set_auto_reload(bool enabled) noexcept { auto_reload_ = enabled; }
    [[nodiscard]] bool auto_reload() const noexcept { return auto_reload_; }

    [[nodiscard]] Result<void> load(const std::filesystem::path& source_dll);
    [[nodiscard]] Result<void> reload();
    void unload();

    void tick(float dt_seconds);

    /// When auto-reload is on, reload if the source DLL mtime changed (throttled).
    void poll_source_changes();

    [[nodiscard]] bool loaded() const noexcept { return module_handle_ != nullptr; }
    [[nodiscard]] const std::filesystem::path& source_path() const noexcept { return source_path_; }
    [[nodiscard]] const std::filesystem::path& loaded_path() const noexcept { return loaded_path_; }
    [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }
    [[nodiscard]] std::uint32_t abi_version() const noexcept { return abi_version_; }
    [[nodiscard]] const std::string& module_name() const noexcept { return module_name_; }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }
    [[nodiscard]] std::uint64_t tick_count() const noexcept { return host_tick_count_; }

    /// Mirror of the last blackboard keys written through the host callbacks (for tests / Diagnostics).
    [[nodiscard]] std::optional<double> mirror_number(const std::string& key) const;
    [[nodiscard]] std::optional<bool> mirror_bool(const std::string& key) const;

private:
    struct Exports {
        using AbiVersionFn = uint32_t (*)();
        using NameFn = const char* (*)();
        using InitFn = bool (*)(const EngineGameHostV1*);
        using TickFn = void (*)(float);
        using ShutdownFn = void (*)();

        AbiVersionFn abi_version = nullptr;
        NameFn name = nullptr;
        InitFn init = nullptr;
        TickFn tick = nullptr;
        ShutdownFn shutdown = nullptr;
    };

    [[nodiscard]] Result<void> load_locked(const std::filesystem::path& source_dll);
    void free_loaded_module() noexcept;
    void fill_host_api(EngineGameHostV1& host) noexcept;
    static void host_log(void* user, int level, const char* message);
    static void host_bb_set_number(void* user, const char* key, double value);
    static void host_bb_set_bool(void* user, const char* key, bool value);
    static bool host_bb_get_number(void* user, const char* key, double* out_value);
    static bool host_bb_get_bool(void* user, const char* key, bool* out_value);
    void notify_lua_reloaded();

    void* module_handle_ = nullptr;
    Exports exports_{};
    EngineGameHostV1 host_api_{};
    std::filesystem::path source_path_;
    std::filesystem::path loaded_path_;
    std::uint32_t generation_ = 0;
    std::uint32_t abi_version_ = 0;
    std::string module_name_;
    std::string last_error_;
    bool auto_reload_ = false;
    bool ticking_ = false;
    bool reload_requested_ = false;
    std::uint64_t host_tick_count_ = 0;
    std::uint32_t poll_frame_counter_ = 0;
    std::optional<std::filesystem::file_time_type> source_mtime_;
    LuaRuntime* lua_ = nullptr;

    std::map<std::string, double> number_mirror_;
    std::map<std::string, bool> bool_mirror_;
};

} // namespace engine
