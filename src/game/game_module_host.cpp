#include "engine/game/game_module_host.h"

#include "engine/core/error.h"
#include "engine/diagnostics/logger.h"
#include "engine/scripting/lua_runtime.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace engine {
namespace {

EngineError module_error(const char* code, std::string message, std::string remediation) {
    EngineError error;
    error.code = code;
    error.severity = Severity::Error;
    error.category = ErrorCategory::Io;
    error.subsystem = "game_module";
    error.message = std::move(message);
    error.remediation = std::move(remediation);
    error.correlation_id = make_correlation_id();
    return error;
}

#if defined(_WIN32)
std::string win32_error_message(DWORD code) {
    char* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageA(flags, nullptr, code, 0, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    std::string text = length && buffer ? std::string(buffer, length) : ("error " + std::to_string(code));
    if (buffer) LocalFree(buffer);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) text.pop_back();
    return text;
}
#endif

Result<void> copy_file_replace(const std::filesystem::path& from, const std::filesystem::path& to) {
    std::error_code ec;
    std::filesystem::create_directories(to.parent_path(), ec);
    if (ec) {
        return Result<void>::failure(module_error("GAME-MODULE-COPY-DIR",
            "Could not create generation directory: " + to.parent_path().string() + " (" + ec.message() + ")",
            "Check write permissions next to the game module DLL."));
    }
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return Result<void>::failure(module_error("GAME-MODULE-COPY",
            "Could not copy game module for load: " + ec.message(),
            "Close other handles on the DLL and ensure the path is writable."));
    }
    return Result<void>::success();
}

} // namespace

GameModuleHost::~GameModuleHost() { unload(); }

GameModuleHost::GameModuleHost(GameModuleHost&& other) noexcept
    : module_handle_(std::exchange(other.module_handle_, nullptr))
    , exports_(other.exports_)
    , host_api_(other.host_api_)
    , source_path_(std::move(other.source_path_))
    , loaded_path_(std::move(other.loaded_path_))
    , generation_(other.generation_)
    , abi_version_(other.abi_version_)
    , module_name_(std::move(other.module_name_))
    , last_error_(std::move(other.last_error_))
    , auto_reload_(other.auto_reload_)
    , ticking_(false)
    , reload_requested_(other.reload_requested_)
    , host_tick_count_(other.host_tick_count_)
    , poll_frame_counter_(other.poll_frame_counter_)
    , source_mtime_(other.source_mtime_)
    , lua_(other.lua_)
    , number_mirror_(std::move(other.number_mirror_))
    , bool_mirror_(std::move(other.bool_mirror_)) {
    other.exports_ = {};
    other.host_api_ = {};
    other.generation_ = 0;
    other.abi_version_ = 0;
    other.auto_reload_ = false;
    other.reload_requested_ = false;
    other.host_tick_count_ = 0;
    other.poll_frame_counter_ = 0;
    other.source_mtime_.reset();
    other.lua_ = nullptr;
    // Host table must point at *this after move; rewrite user + re-init callbacks.
    if (module_handle_) fill_host_api(host_api_);
}

GameModuleHost& GameModuleHost::operator=(GameModuleHost&& other) noexcept {
    if (this == &other) return *this;
    unload();
    module_handle_ = std::exchange(other.module_handle_, nullptr);
    exports_ = other.exports_;
    other.exports_ = {};
    host_api_ = other.host_api_;
    other.host_api_ = {};
    source_path_ = std::move(other.source_path_);
    loaded_path_ = std::move(other.loaded_path_);
    generation_ = other.generation_;
    other.generation_ = 0;
    abi_version_ = other.abi_version_;
    other.abi_version_ = 0;
    module_name_ = std::move(other.module_name_);
    last_error_ = std::move(other.last_error_);
    auto_reload_ = other.auto_reload_;
    other.auto_reload_ = false;
    ticking_ = false;
    reload_requested_ = other.reload_requested_;
    other.reload_requested_ = false;
    host_tick_count_ = other.host_tick_count_;
    other.host_tick_count_ = 0;
    poll_frame_counter_ = other.poll_frame_counter_;
    other.poll_frame_counter_ = 0;
    source_mtime_ = other.source_mtime_;
    other.source_mtime_.reset();
    lua_ = other.lua_;
    other.lua_ = nullptr;
    number_mirror_ = std::move(other.number_mirror_);
    bool_mirror_ = std::move(other.bool_mirror_);
    if (module_handle_) fill_host_api(host_api_);
    return *this;
}

std::filesystem::path GameModuleHost::default_module_path() {
#if defined(_WIN32)
    wchar_t module_path[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, module_path, MAX_PATH) == 0) {
        return std::filesystem::path{"game_module.dll"};
    }
    return std::filesystem::path(module_path).parent_path() / "game_module.dll";
#else
    return std::filesystem::path{"game_module.dll"};
#endif
}

Result<void> GameModuleHost::load(const std::filesystem::path& source_dll) {
    if (ticking_) {
        reload_requested_ = true;
        source_path_ = source_dll;
        return Result<void>::success();
    }
    return load_locked(source_dll);
}

Result<void> GameModuleHost::reload() {
    if (source_path_.empty()) {
        return load(default_module_path());
    }
    return load(source_path_);
}

void GameModuleHost::unload() {
    if (ticking_) {
        reload_requested_ = false;
        // Defer: cannot FreeLibrary while tick is active; next tick end will free via reload path.
        last_error_ = "Unload deferred until tick completes";
        return;
    }
    free_loaded_module();
    abi_version_ = 0;
    module_name_.clear();
    source_mtime_.reset();
}

void GameModuleHost::tick(float dt_seconds) {
    if (!module_handle_ || !exports_.tick) return;
    ticking_ = true;
    exports_.tick(dt_seconds);
    ++host_tick_count_;
    ticking_ = false;
    if (reload_requested_) {
        reload_requested_ = false;
        const auto path = source_path_.empty() ? default_module_path() : source_path_;
        if (const auto result = load_locked(path); !result) {
            last_error_ = result.error().message;
            Logger::instance().write(result.error());
        }
    }
}

void GameModuleHost::poll_source_changes() {
    if (!auto_reload_ || source_path_.empty()) return;
    // Same cadence as ScriptFileMonitor consumers (~every 30 frames in editor).
    if ((++poll_frame_counter_ % 30u) != 0u) return;
    std::error_code ec;
    if (!std::filesystem::exists(source_path_, ec) || ec) return;
    const auto mtime = std::filesystem::last_write_time(source_path_, ec);
    if (ec) return;
    if (!source_mtime_.has_value()) {
        source_mtime_ = mtime;
        return;
    }
    if (mtime != *source_mtime_) {
        source_mtime_ = mtime;
        Logger::instance().write(Severity::Info, "game_module",
            "Source DLL changed; auto-reloading " + source_path_.string());
        if (const auto result = reload(); !result) {
            last_error_ = result.error().message;
            Logger::instance().write(result.error());
        }
    }
}

std::optional<double> GameModuleHost::mirror_number(const std::string& key) const {
    const auto it = number_mirror_.find(key);
    if (it == number_mirror_.end()) return std::nullopt;
    return it->second;
}

std::optional<bool> GameModuleHost::mirror_bool(const std::string& key) const {
    const auto it = bool_mirror_.find(key);
    if (it == bool_mirror_.end()) return std::nullopt;
    return it->second;
}

void GameModuleHost::free_loaded_module() noexcept {
    if (exports_.shutdown && module_handle_) {
        exports_.shutdown();
    }
    exports_ = {};
#if defined(_WIN32)
    if (module_handle_) {
        FreeLibrary(static_cast<HMODULE>(module_handle_));
        module_handle_ = nullptr;
    }
#else
    module_handle_ = nullptr;
#endif
    if (!loaded_path_.empty()) {
        std::error_code ec;
        std::filesystem::remove(loaded_path_, ec);
        loaded_path_.clear();
    }
}

void GameModuleHost::fill_host_api(EngineGameHostV1& host) noexcept {
    host.size = static_cast<uint32_t>(sizeof(EngineGameHostV1));
    host.abi_version = ENGINE_GAME_MODULE_ABI_VERSION;
    host.user = this;
    host.log = &GameModuleHost::host_log;
    host.blackboard_set_number = &GameModuleHost::host_bb_set_number;
    host.blackboard_set_bool = &GameModuleHost::host_bb_set_bool;
    host.blackboard_get_number = &GameModuleHost::host_bb_get_number;
    host.blackboard_get_bool = &GameModuleHost::host_bb_get_bool;
}

void GameModuleHost::host_log(void* user, int level, const char* message) {
    auto* self = static_cast<GameModuleHost*>(user);
    if (!self || !message) return;
    Severity severity = Severity::Info;
    if (level == ENGINE_GAME_LOG_WARNING) severity = Severity::Warning;
    else if (level >= ENGINE_GAME_LOG_ERROR) severity = Severity::Error;
    Logger::instance().write(severity, "game_module", message);
}

void GameModuleHost::host_bb_set_number(void* user, const char* key, double value) {
    auto* self = static_cast<GameModuleHost*>(user);
    if (!self || !key || key[0] == '\0') return;
    self->number_mirror_[key] = value;
    if (self->lua_) self->lua_->blackboard_set_number(key, value);
}

void GameModuleHost::host_bb_set_bool(void* user, const char* key, bool value) {
    auto* self = static_cast<GameModuleHost*>(user);
    if (!self || !key || key[0] == '\0') return;
    self->bool_mirror_[key] = value;
    if (self->lua_) self->lua_->blackboard_set_bool(key, value);
}

bool GameModuleHost::host_bb_get_number(void* user, const char* key, double* out_value) {
    auto* self = static_cast<GameModuleHost*>(user);
    if (!self || !key || !out_value) return false;
    if (self->lua_) {
        if (const auto entry = self->lua_->blackboard_get(key); entry &&
            entry->type == ScriptBlackboardType::Number) {
            *out_value = entry->number_value;
            return true;
        }
    }
    const auto it = self->number_mirror_.find(key);
    if (it == self->number_mirror_.end()) return false;
    *out_value = it->second;
    return true;
}

bool GameModuleHost::host_bb_get_bool(void* user, const char* key, bool* out_value) {
    auto* self = static_cast<GameModuleHost*>(user);
    if (!self || !key || !out_value) return false;
    if (self->lua_) {
        if (const auto entry = self->lua_->blackboard_get(key); entry &&
            entry->type == ScriptBlackboardType::Bool) {
            *out_value = entry->bool_value;
            return true;
        }
    }
    const auto it = self->bool_mirror_.find(key);
    if (it == self->bool_mirror_.end()) return false;
    *out_value = it->second;
    return true;
}

void GameModuleHost::notify_lua_reloaded() {
    if (!lua_) return;
    nlohmann::json payload;
    payload["name"] = module_name_;
    payload["abiVersion"] = abi_version_;
    payload["generation"] = generation_;
    payload["source"] = source_path_.generic_string();
    // Optional global — missing handler is not an error.
    (void)lua_->call_handler("on_game_module_reloaded", payload.dump());
}

Result<void> GameModuleHost::load_locked(const std::filesystem::path& source_dll) {
#if !defined(_WIN32)
    return Result<void>::failure(module_error("GAME-MODULE-PLATFORM",
        "Game module hot-reload is only implemented on Windows.",
        "Use a Windows build tree."));
#else
    free_loaded_module();
    last_error_.clear();
    source_path_ = source_dll.lexically_normal();

    if (!std::filesystem::exists(source_path_)) {
        last_error_ = "Game module not found: " + source_path_.string();
        return Result<void>::failure(module_error("GAME-MODULE-MISSING", last_error_,
            "Build the game_module target or set an explicit path."));
    }

    ++generation_;
    const auto gen_dir = source_path_.parent_path() / "game_module_generations";
    loaded_path_ = gen_dir / ("game_module." + std::to_string(generation_) + ".dll");
    if (const auto copied = copy_file_replace(source_path_, loaded_path_); !copied) {
        --generation_;
        loaded_path_.clear();
        last_error_ = copied.error().message;
        return copied;
    }

    HMODULE handle = LoadLibraryW(loaded_path_.wstring().c_str());
    if (!handle) {
        const DWORD err = GetLastError();
        std::error_code remove_ec;
        std::filesystem::remove(loaded_path_, remove_ec);
        loaded_path_.clear();
        --generation_;
        last_error_ = "LoadLibrary failed: " + win32_error_message(err);
        return Result<void>::failure(module_error("GAME-MODULE-LOAD", last_error_,
            "Rebuild game_module and confirm the DLL is a 64-bit MSVC binary."));
    }

    Exports exports{};
    exports.abi_version = reinterpret_cast<Exports::AbiVersionFn>(GetProcAddress(handle, "game_module_abi_version"));
    exports.name = reinterpret_cast<Exports::NameFn>(GetProcAddress(handle, "game_module_name"));
    exports.init = reinterpret_cast<Exports::InitFn>(GetProcAddress(handle, "game_module_init"));
    exports.tick = reinterpret_cast<Exports::TickFn>(GetProcAddress(handle, "game_module_tick"));
    exports.shutdown = reinterpret_cast<Exports::ShutdownFn>(GetProcAddress(handle, "game_module_shutdown"));

    if (!exports.abi_version || !exports.name || !exports.init || !exports.tick || !exports.shutdown) {
        FreeLibrary(handle);
        std::error_code remove_ec;
        std::filesystem::remove(loaded_path_, remove_ec);
        loaded_path_.clear();
        --generation_;
        last_error_ = "Game module is missing required exports";
        return Result<void>::failure(module_error("GAME-MODULE-EXPORTS", last_error_,
            "Export game_module_abi_version, name, init, tick, and shutdown."));
    }

    const uint32_t abi = exports.abi_version();
    if (abi != ENGINE_GAME_MODULE_ABI_VERSION) {
        FreeLibrary(handle);
        std::error_code remove_ec;
        std::filesystem::remove(loaded_path_, remove_ec);
        loaded_path_.clear();
        --generation_;
        last_error_ = "Game module ABI " + std::to_string(abi) + " does not match host " +
            std::to_string(ENGINE_GAME_MODULE_ABI_VERSION);
        return Result<void>::failure(module_error("GAME-MODULE-ABI", last_error_,
            "Rebuild the module against the current game_module_abi.h."));
    }

    EngineGameHostV1 host_api{};
    fill_host_api(host_api);
    host_api_ = host_api;
    module_handle_ = handle;
    exports_ = exports;
    if (!exports.init(&host_api_)) {
        free_loaded_module();
        --generation_;
        last_error_ = "game_module_init returned false";
        return Result<void>::failure(module_error("GAME-MODULE-INIT", last_error_,
            "Inspect game module logs and init failure path."));
    }

    abi_version_ = abi;
    const char* name = exports.name();
    module_name_ = name ? name : "";
    std::error_code mtime_ec;
    source_mtime_ = std::filesystem::last_write_time(source_path_, mtime_ec);
    if (mtime_ec) source_mtime_.reset();

    Logger::instance().write(Severity::Info, "game_module",
        "Loaded generation " + std::to_string(generation_) + " (" + module_name_ + ") from " +
            source_path_.string());
    notify_lua_reloaded();
    return Result<void>::success();
#endif
}

} // namespace engine
