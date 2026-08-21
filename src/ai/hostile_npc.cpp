#include "engine/ai/hostile_npc.h"



#include "engine/assets/prefab_asset.h"



#include <algorithm>

#include <cmath>



namespace engine {

namespace {



void clear_read(HostileNpcBrainState& state) {

    state.read = HostileNpcRead::None;

    state.read_timer = 0.0f;

}



bool draw_hostile_attack_read(const HostileNpcConfig& config, HostileNpcBrainState& state) {

    if (state.read_marbles_remaining < 0 || state.miss_marbles_remaining < 0 ||

        state.read_marbles_remaining + state.miss_marbles_remaining <= 0) {

        state.read_marbles_remaining = std::max(0, config.read_bag_reads);

        state.miss_marbles_remaining = std::max(0, config.read_bag_misses);

    }

    const int total = state.read_marbles_remaining + state.miss_marbles_remaining;

    if (total <= 0)

        return false;

    state.read_rng = state.read_rng * 1664525u + 1013904223u;

    const int pick = static_cast<int>(state.read_rng % static_cast<std::uint32_t>(total));

    if (pick < state.read_marbles_remaining) {

        --state.read_marbles_remaining;

        return true;

    }

    --state.miss_marbles_remaining;

    return false;

}



void begin_read_attempt(const HostileNpcConfig& config, HostileNpcBrainState& state) {

    const float reaction = std::max(0.0f, config.block_reaction);

    if (reaction <= 1.0e-4f) {

        state.read = HostileNpcRead::Blocking;

        state.read_timer = std::max(0.0f, config.block_hold);

        return;

    }

    state.read = HostileNpcRead::Pending;

    state.read_timer = reaction;

}



HostileNpcCommand make_move_toward(HostileNpcAction action, const WorldPosition& self,

    const WorldPosition& goal) {

    HostileNpcCommand command;

    command.action = action;

    const float dx = static_cast<float>(goal.x - self.x);

    const float dz = static_cast<float>(goal.z - self.z);

    const float dist = std::sqrt(dx * dx + dz * dz);

    command.face_yaw = (dist > 1.0e-4f) ? std::atan2(dx, dz) : 0.0f;

    command.wish_forward = dist > 1.0e-3f;

    command.has_move_goal = true;

    command.move_goal = goal;

    return command;

}



float xz_distance(const WorldPosition& a, const WorldPosition& b) {

    const float dx = static_cast<float>(a.x - b.x);

    const float dz = static_cast<float>(a.z - b.z);

    return std::sqrt(dx * dx + dz * dz);

}



} // namespace



HostileNpcConfig hostile_npc_config_from_prefab(const PrefabNpcAi& authored) {

    HostileNpcConfig config;

    if (authored.aggro_radius > 0.0f) config.aggro_radius = authored.aggro_radius;

    if (authored.lose_aggro_radius > 0.0f) config.lose_aggro_radius = authored.lose_aggro_radius;

    if (authored.attack_range > 0.0f) config.attack_range = authored.attack_range;

    if (authored.attack_cooldown > 0.0f) config.attack_cooldown = authored.attack_cooldown;

    if (authored.move_speed > 0.0f) config.move_speed = authored.move_speed;

    if (config.lose_aggro_radius < config.aggro_radius) config.lose_aggro_radius = config.aggro_radius;

    if (authored.patrol_arrive > 0.0f) config.patrol_arrive = authored.patrol_arrive;

    // Nav samples are 4 m apart; arrive must clear snap error or the brain never advances.

    config.patrol_arrive = std::max(config.patrol_arrive, 2.25f);

    config.patrol_waypoints = authored.patrol_waypoints;

    return config;

}



HostileNpcCommand tick_hostile_npc(const HostileNpcConfig& config, HostileNpcBrainState& state,

    const HostileNpcInput& input, float dt_seconds) {

    const float dt = std::max(0.0f, dt_seconds);

    // Recovery starts after the Attack clip, not when the trigger fires — otherwise

    // the next swing eats the whole cooldown and the clone never leaves Attack.

    if (!input.attack_anim_active)

        state.attack_cooldown_remaining = std::max(0.0f, state.attack_cooldown_remaining - dt);



    const float dx = static_cast<float>(input.player.x - input.self.x);

    const float dz = static_cast<float>(input.player.z - input.self.z);

    const float dist = std::sqrt(dx * dx + dz * dz);

    const float face_yaw = (dist > 1.0e-4f) ? std::atan2(dx, dz) : 0.0f;

    const bool in_range = dist <= config.attack_range;

    const bool new_player_swing =

        !input.player_melee_state.empty() && input.player_melee_state != state.last_player_melee_state;

    if (in_range)

        state.last_player_melee_state = input.player_melee_state;

    else

        state.last_player_melee_state.clear();



    HostileNpcCommand command;

    command.face_yaw = face_yaw;



    if (input.dead) {

        state.aggroed = false;

        clear_read(state);

        command.action = HostileNpcAction::Idle;

        return command;

    }



    if (input.stunned) {

        clear_read(state);

        command.action = HostileNpcAction::Idle;

        return command;

    }



    if (dist > config.lose_aggro_radius)

        state.aggroed = false;

    else if (dist <= config.aggro_radius)

        state.aggroed = true;



    if (!state.aggroed) {

        clear_read(state);

        if (!input.patrol_world.empty()) {

            if (state.patrol_index >= input.patrol_world.size())

                state.patrol_index = 0;

            const float arrive = std::max(0.35f, config.patrol_arrive);

            // Skip through any waypoints already inside the arrive radius so we

            // never ping-pong between two nearby snaps.

            for (std::size_t n = 0; n < input.patrol_world.size(); ++n) {

                const WorldPosition& candidate = input.patrol_world[state.patrol_index];

                if (xz_distance(input.self, candidate) > arrive)

                    break;

                state.patrol_index = (state.patrol_index + 1) % input.patrol_world.size();

            }

            const WorldPosition& next = input.patrol_world[state.patrol_index];

            return make_move_toward(HostileNpcAction::Patrol, input.self, next);

        }

        command.action = HostileNpcAction::Idle;

        return command;

    }



    if (input.attack_anim_active) {

        clear_read(state);

        command.action = HostileNpcAction::Attack;

        return command;

    }



    if (in_range && new_player_swing) {

        if (draw_hostile_attack_read(config, state))

            begin_read_attempt(config, state);

        else {

            state.read = HostileNpcRead::Missed;

            state.read_timer = 0.0f;

        }

    }



    if (state.read == HostileNpcRead::Pending) {

        state.read_timer = std::max(0.0f, state.read_timer - dt);

        if (state.read_timer <= 1.0e-4f) {

            state.read = HostileNpcRead::Blocking;

            state.read_timer = std::max(0.0f, config.block_hold);

        } else {

            command.action = HostileNpcAction::Idle;

            return command;

        }

    }



    if (state.read == HostileNpcRead::Blocking) {

        if (input.player_melee_state.empty())

            state.read_timer = std::max(0.0f, state.read_timer - dt);

        if (input.player_melee_state.empty() && state.read_timer <= 1.0e-4f)

            clear_read(state);

        else {

            command.action = HostileNpcAction::Block;

            command.wish_block = true;

            return command;

        }

    }



    if (state.read == HostileNpcRead::Missed && input.player_melee_state.empty())

        clear_read(state);



    if (in_range && state.attack_cooldown_remaining <= 0.0f &&

        state.read != HostileNpcRead::Pending && state.read != HostileNpcRead::Blocking) {

        state.attack_cooldown_remaining = config.attack_cooldown;

        command.action = HostileNpcAction::Attack;

        command.fire_attack = true;

        return command;

    }



    if (dist > config.attack_range) {

        return make_move_toward(HostileNpcAction::Chase, input.self, input.player);

    }



    command.action = HostileNpcAction::Idle;

    return command;

}



} // namespace engine
