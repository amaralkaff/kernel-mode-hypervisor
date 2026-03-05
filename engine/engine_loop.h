#pragma once
#include <engine/threading/player_thread.h>

namespace settings
{
	extern bool fullbright;

	extern int weapon_glow_id;
	extern bool weapon_glow;

	extern bool third_person;
	extern int third_person_key;
	extern int third_person_distance;

	extern bool no_viewmodel;
	extern bool no_sky;
	extern bool no_fog;
	extern bool anti_aim;
	extern bool local_glow;

	extern bool flat_world;
	extern float desired_fov;

	extern bool bloom;
	extern float bloom_intensity;

	extern bool aspr;
}


auto engine_loop(std::shared_ptr<PlayerCache> p_cache) -> void;