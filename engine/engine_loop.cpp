#include "engine/engine_loop.h"
#include <engine/player/player.h>
#include <include/includes.h>
#include <engine/impl/offsets.h>
#include <core/systems/visuals/visuals.h>
#include <engine/weapon/weapon.h>

// Convar offsets (game-version specific, update with each patch)
// These are addresses relative to process base for various convar values
namespace convar
{
	constexpr uintptr_t aspect_ratio = 0x1e94be0 + 0x58;
	constexpr uintptr_t letterbox_aspect = 0x20ab0a0 + 0x58;
	constexpr uintptr_t wide_pillarbox = 0x20ab700 + 0x58;
	constexpr uintptr_t bloom_amount = 0x27045f0 + 0x58;
	constexpr uintptr_t fullbright_addr = 0x1ead970 + 0x5C;
	constexpr uintptr_t no_sky_addr = 0x2820640 + 0x5C;
	constexpr uintptr_t third_person_addr = 0x26691a0 + 0x5C;
	constexpr uintptr_t fov_offset1 = 0x4830;
	constexpr uintptr_t fov_offset2 = 0x4834;
	constexpr uintptr_t fov_offset3 = 0x482c;
}

namespace settings
{
	 bool fullbright = false;

	 int weapon_glow_id = 37;
	 bool weapon_glow;

	 bool third_person = false;
	 int third_person_key = 0x04;
	 int third_person_distance = 90;

	 bool no_viewmodel = false;
	 bool no_sky = false ;
	 bool no_fog = false;
	 bool flat_world = false;

	 bool anti_aim = false;
	 bool local_glow = false;
	 float desired_fov = 120.f;

	 bool bloom = false;
	 float bloom_intensity = -100.f;

	 bool aspr = false;	
}

auto engine_loop(std::shared_ptr<PlayerCache> p_cache) -> void
{
	while (recode::is_running.load(std::memory_order_acquire))
	{
		if (settings::aspr)
		{
			recode::driver.write<float>(recode::process_image + convar::aspect_ratio, 1.333f);
			recode::driver.write<float>(recode::process_image + convar::letterbox_aspect, 1.333f);
			recode::driver.write<float>(recode::process_image + convar::wide_pillarbox, 0.f);
			settings::aspr = false;
		}

		if (settings::bloom)
			recode::driver.write<float>(recode::process_image + convar::bloom_amount, settings::bloom_intensity);
		else
			recode::driver.write<float>(recode::process_image + convar::bloom_amount, 1.f);

		auto local = p_cache->get_local_player();
		if (!local)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (settings::local_glow)
			local->set_glow(64);


		Weapon weapon{};
		if (!weapon.update(local->get_entity()))
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (settings::anti_aim)
		{
			float fov_new = settings::desired_fov / 100.0f;
			recode::driver.write<float>(local->get_entity() + convar::fov_offset1, fov_new);
			recode::driver.write<float>(local->get_entity() + convar::fov_offset2, fov_new);
			recode::driver.write<float>(local->get_entity() + convar::fov_offset3, fov_new);
		}
		
		if (settings::weapon_glow)
			weapon.set_glow(settings::weapon_glow_id);

		if (settings::third_person)
			weapon.set_skin(settings::third_person_key); 
		 
		if (settings::fullbright)
			recode::driver.write<int>(recode::process_image + convar::fullbright_addr, 1);
		else
			recode::driver.write<int>(recode::process_image + convar::fullbright_addr, 0);

		if (settings::no_sky)
			recode::driver.write<int>(recode::process_image + convar::no_sky_addr, 32);
		else
			recode::driver.write<int>(recode::process_image + convar::no_sky_addr, 0);

		if (GetAsyncKeyState(settings::third_person_key) & 1)
			settings::third_person = !settings::third_person;

		bool ads = GetAsyncKeyState(VK_RBUTTON) & 0x8000;

		if (settings::third_person && !ads)
		{
			recode::driver.write<int>(recode::process_image + convar::third_person_addr, settings::third_person_distance);
		}
		else
		{
			recode::driver.write<int>(recode::process_image + convar::third_person_addr, 0);
		}

		auto players = p_cache->get_players(); 
		if (players->empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		for (auto& p : *players)
		{
			if (!p)
				continue;

			if (settings::Glow)
				p->set_glow(settings::glow_id);
		}
	}
}