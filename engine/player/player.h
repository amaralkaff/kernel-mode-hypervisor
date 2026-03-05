#pragma once
#include <cstdint>
#include <deps/math/structs.h>
#include <engine/player/player_utils.h>

class Player
{
public:
	virtual auto update(uintptr_t entity) -> bool;

	auto get_bone(int hitbox) -> vec3; 
	auto set_glow(int id) -> void;

	auto get_health() const -> int { return health; }
	auto get_max_health() const -> int { return max_health; }
	auto get_team_id() const -> int { return team_id; }

	auto get_is_downed() const -> bool { return is_downed; }
	auto get_is_alive() const -> bool { return is_alive; }
	auto get_origin() const -> vec3 { return origin; }

	auto get_entity() const -> uintptr_t { return base_address; }

	virtual ~Player() = default;

private:
	int     health = 0;
	int max_health = 0;
	int    team_id = 0;

	bool is_downed = false;
	bool  is_alive = false;

	vec3 origin;
	BoneCache bone_cache;

	uintptr_t base_address = 0;
};