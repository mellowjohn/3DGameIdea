#include "engine/vfx/weapon_sweep.h"

#include "engine/core/error.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace engine {
namespace {
Result<void> fail(std::string code, std::string message) {
  return Result<void>::failure(EngineError{std::move(code), Severity::Error, ErrorCategory::Validation,
                                           "weapon-sweep", std::move(message), std::nullopt, {}, "Fix the sweep asset."});
}
bool safe_relative(const std::string& p) { return !p.empty() && p.find("..") == std::string::npos && p.find(':') == std::string::npos && p.front() != '/'; }
}

Result<WeaponSweepAsset> WeaponSweepAsset::parse(const std::string& json_text) {
  try {
    const auto j = nlohmann::json::parse(json_text);
    WeaponSweepAsset out;
    out.schema_version = j.value("schemaVersion", 0);
    if (out.schema_version != 1) return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-SCHEMA", Severity::Error, ErrorCategory::Validation, "weapon-sweep", "Unsupported schemaVersion", std::nullopt, {}, "Use schemaVersion 1."});
    out.id = j.value("id", ""); out.texture = j.value("texture", "");
    if (out.id.empty()) return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-ID", Severity::Error, ErrorCategory::Validation, "weapon-sweep", "id is required", std::nullopt, {}, "Set a stable id."});
    if (!safe_relative(out.texture)) return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-TEXTURE-PATH-INVALID", Severity::Error, ErrorCategory::Validation, "weapon-sweep", "texture must be a project-relative path", std::nullopt, {}, "Use assets/... without traversal."});
    if (j.contains("color") && j["color"].is_array() && j["color"].size() == 4)
      for (std::size_t i=0;i<4;++i) out.color[i]=j["color"][i].get<float>();
    if (j.contains("coreColor") && j["coreColor"].is_array() && j["coreColor"].size() == 4)
      for (std::size_t i=0;i<4;++i) out.core_color[i]=j["coreColor"][i].get<float>();
    out.inner_radius = j.value("innerRadius", out.inner_radius);
    out.outer_radius = j.value("outerRadius", out.outer_radius);
    out.core_inner_radius = j.value("coreInnerRadius", out.core_inner_radius);
    out.core_outer_radius = j.value("coreOuterRadius", out.core_outer_radius);
    out.light_emission = j.value("lightEmission", out.light_emission);
    out.trail_seconds = j.value("trailSeconds", out.trail_seconds);
    out.max_samples = j.value("maxSamples", out.max_samples);
    if (out.trail_seconds <= 0.0f || out.max_samples < 2 || out.max_samples > 64 ||
        out.inner_radius < 0.0f || out.outer_radius <= out.inner_radius ||
        out.core_inner_radius < out.inner_radius || out.core_outer_radius > out.outer_radius ||
        out.core_outer_radius <= out.core_inner_radius)
      return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-RANGE", Severity::Error, ErrorCategory::Validation, "weapon-sweep", "trailSeconds must be positive and maxSamples must be 2..64", std::nullopt, {}, "Correct sweep timing/sample count."});
    return Result<WeaponSweepAsset>::success(std::move(out));
  } catch (const std::exception& e) { return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-JSON", Severity::Error, ErrorCategory::Validation, "weapon-sweep", e.what(), std::nullopt, {}, "Fix JSON syntax."}); }
}
Result<WeaponSweepAsset> WeaponSweepAsset::load(const std::filesystem::path& path) {
  std::ifstream in(path); if (!in) return Result<WeaponSweepAsset>::failure(EngineError{"SWEEP-LOAD", Severity::Error, ErrorCategory::Io, "weapon-sweep", "Could not open asset", std::nullopt, {}, "Check the asset path."});
  return parse(std::string((std::istreambuf_iterator<char>(in)), {}));
}
Result<void> WeaponSweepAsset::validate_texture(const std::filesystem::path& project_root) const {
  if (!safe_relative(texture)) return fail("SWEEP-TEXTURE-PATH-INVALID", "texture must be project-relative");
  if (!std::filesystem::exists(project_root / texture)) return fail("SWEEP-TEXTURE-MISSING", "texture file is missing");
  return Result<void>::success();
}
void WeaponSweepSystem::begin(const WeaponSweepAsset& asset) { asset_ = asset; samples_.clear(); draw_.clear(); accepting_samples_ = true; }
void WeaponSweepSystem::clear() noexcept { asset_.reset(); samples_.clear(); draw_.clear(); accepting_samples_ = false; }
void WeaponSweepSystem::sample(const std::array<float,3>& hilt, const std::array<float,3>& tip, float now) {
  if (!asset_ || !accepting_samples_) return;
  samples_.push_back({hilt, tip, now});
  while (samples_.size() > asset_->max_samples) samples_.erase(samples_.begin());
  rebuild(now);
}
void WeaponSweepSystem::tick(float now) { if (!asset_) return; rebuild(now); if (!accepting_samples_ && samples_.empty()) asset_.reset(); }
void WeaponSweepSystem::rebuild(float now) {
  draw_.clear(); if (!asset_) return;
  const float life=asset_->trail_seconds;
  samples_.erase(std::remove_if(samples_.begin(), samples_.end(), [&](const Sample& s){return now-s.time>life;}), samples_.end());
  if (samples_.size()<2) return;
  const float oldest=samples_.front().time;
  auto point_at = [](const Sample& sample, float radius) {
    return std::array<float, 3>{
        sample.hilt[0] + (sample.tip[0] - sample.hilt[0]) * radius,
        sample.hilt[1] + (sample.tip[1] - sample.hilt[1]) * radius,
        sample.hilt[2] + (sample.tip[2] - sample.hilt[2]) * radius};
  };
  for (std::size_t i=1;i<samples_.size();++i) {
    const auto& a=samples_[i-1]; const auto& b=samples_[i];
    const float ua=(a.time-oldest)/std::max(1e-4f, life), ub=(b.time-oldest)/std::max(1e-4f, life);
    const float aa=now-a.time, ab=now-b.time;
    auto emit_band = [&](float inner, float outer, const std::array<float, 4>& color,
                         float alpha_scale) {
      auto v = [&](const std::array<float,3>& p, float u, float vv, float age) {
        auto c = color;
        c[3] *= alpha_scale * std::clamp(1.0f - age / life, 0.0f, 1.0f);
        return WeaponSweepDrawVertex{p, c, {u, vv}, asset_->light_emission};
      };
      const auto a_inner = point_at(a, inner), a_outer = point_at(a, outer);
      const auto b_inner = point_at(b, inner), b_outer = point_at(b, outer);
      draw_.push_back(v(a_inner, ua, 0, aa)); draw_.push_back(v(a_outer, ua, 1, aa)); draw_.push_back(v(b_outer, ub, 1, ab));
      draw_.push_back(v(a_inner, ua, 0, aa)); draw_.push_back(v(b_outer, ub, 1, ab)); draw_.push_back(v(b_inner, ub, 0, ab));
    };
    emit_band(asset_->inner_radius, asset_->outer_radius, asset_->color, 0.68f);
    emit_band(asset_->core_inner_radius, asset_->core_outer_radius, asset_->core_color, 1.0f);
  }
}
} // namespace engine
