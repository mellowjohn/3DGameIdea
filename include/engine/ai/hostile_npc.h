#pragma once



#include "engine/physics/collision_world.h"



#include <array>

#include <cstdint>

#include <string>

#include <vector>



namespace engine {



struct PrefabNpcAi;



struct HostileNpcConfig {

    float aggro_radius = 16.0f;

    float lose_aggro_radius = 22.0f;

    float attack_range = 1.9f;

    float attack_cooldown = 1.35f;

    float move_speed = 3.5f;

    /// Delay after seeing a player swing before the guard comes up.

    float block_reaction = 0.16f;

    /// Minimum time to hold Block so the pose is readable after the swing.

    float block_hold = 0.75f;

    /// Marble bag for attack reads (DEC-0057): without-replacement until refill.

    int read_bag_reads = 3;

    int read_bag_misses = 2;

    /// Local XZ offsets from spawn (world Y ignored; host snaps via nav). Empty = no patrol.

    std::vector<std::array<float, 2>> patrol_waypoints;

    float patrol_arrive = 1.25f;

};



enum class HostileNpcRead : std::uint8_t { None, Pending, Blocking, Missed };



struct HostileNpcBrainState {

    bool aggroed = false;

    float attack_cooldown_remaining = 0.0f;

    std::string last_player_melee_state;

    HostileNpcRead read = HostileNpcRead::None;

    float read_timer = 0.0f;

    int read_marbles_remaining = -1;

    int miss_marbles_remaining = -1;

    std::uint32_t read_rng = 1;

    std::size_t patrol_index = 0;

};



struct HostileNpcInput {

    WorldPosition self{};

    WorldPosition player{};

    bool attack_anim_active = false;

    /// Current player upper-body melee state (`attack` / `attack2` / `attack3`), or empty.

    std::string player_melee_state;

    bool dead = false;

    bool stunned = false;

    /// World-space patrol corners (spawn + authored offsets). Empty disables Patrol.

    std::vector<WorldPosition> patrol_world;

};



enum class HostileNpcAction : std::uint8_t { Idle, Chase, Attack, Block, Patrol };



struct HostileNpcCommand {

    HostileNpcAction action = HostileNpcAction::Idle;

    float face_yaw = 0.0f;

    bool wish_forward = false;

    bool fire_attack = false;

    bool wish_block = false;

    /// When set, host should pathfind / steer toward this goal (Patrol / Chase).

    bool has_move_goal = false;

    WorldPosition move_goal{};

};



[[nodiscard]] HostileNpcConfig hostile_npc_config_from_prefab(const PrefabNpcAi& authored);



/// Chase / stop-and-swing / attack-read / patrol brain. Pure of physics and animator (TICKET-0284).

HostileNpcCommand tick_hostile_npc(const HostileNpcConfig& config, HostileNpcBrainState& state,

    const HostileNpcInput& input, float dt_seconds);



} // namespace engine
