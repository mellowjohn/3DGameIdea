#pragma once

#include "engine/core/result.h"

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace engine {

/// Diffable authoring data for a weapon-following VFX ribbon.
struct WeaponSweepAsset {
  int schema_version = 1;
  std::string id;
  std::string texture;
  std::array<float, 4> color{1.0f, 0.82f, 0.42f, 1.0f};
  std::array<float, 4> core_color{1.0f, 0.98f, 0.90f, 1.0f};
  /// Radius multipliers measured from the weapon hilt along the hilt-to-tip
  /// axis. The outer band can deliberately exceed the physical blade.
  float inner_radius = 0.10f;
  float outer_radius = 1.85f;
  /// A narrower bright band layered over the coloured aura band.
  float core_inner_radius = 0.42f;
  float core_outer_radius = 1.38f;
  float light_emission = 0.9f;
  float trail_seconds = 0.12f;
  std::size_t max_samples = 10;

  [[nodiscard]] static Result<WeaponSweepAsset> parse(const std::string& json_text);
  [[nodiscard]] static Result<WeaponSweepAsset> load(const std::filesystem::path& path);
  [[nodiscard]] Result<void> validate_texture(const std::filesystem::path& project_root) const;
};

/// One vertex of an additive world-space sweep ribbon. Renderer ownership stays separate.
struct WeaponSweepDrawVertex {
  std::array<float, 3> position{};
  std::array<float, 4> color{};
  std::array<float, 2> uv{};
  float light_emission = 0.0f;
};

/// Samples a weapon hilt/tip through an active attack and emits a triangle list.
class WeaponSweepSystem final {
public:
  void begin(const WeaponSweepAsset& asset);
  void end() noexcept { accepting_samples_ = false; }
  void clear() noexcept;
  void sample(const std::array<float, 3>& hilt, const std::array<float, 3>& tip, float now_seconds);
  void tick(float now_seconds);
  [[nodiscard]] const std::vector<WeaponSweepDrawVertex>& draw_vertices() const noexcept { return draw_; }
  [[nodiscard]] bool active() const noexcept { return accepting_samples_ || !samples_.empty(); }
  [[nodiscard]] bool accepting_samples() const noexcept { return accepting_samples_; }
  [[nodiscard]] const WeaponSweepAsset* asset() const noexcept { return asset_ ? &*asset_ : nullptr; }

private:
  struct Sample { std::array<float, 3> hilt{}, tip{}; float time = 0.0f; };
  void rebuild(float now_seconds);
  std::optional<WeaponSweepAsset> asset_;
  std::vector<Sample> samples_;
  std::vector<WeaponSweepDrawVertex> draw_;
  bool accepting_samples_ = false;
};

} // namespace engine
