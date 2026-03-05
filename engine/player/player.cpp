#include "player.h"
#include <engine/impl/offsets.h>
#include <include/includes.h>
#include <unordered_map>

auto Player::get_bone(int hitbox) -> vec3
{
	if (hitbox < 0 || hitbox >= static_cast<int>(bone_cache.bone_array.size()))
		return {};
	return bone_cache.bone_array[hitbox];
}

auto Player::set_glow(int id) -> void
{
	recode::driver.write<uint8_t>(base_address + offsets::glow_t1, id);
	recode::driver.write<int>(base_address + offsets::glow_enable, 7);
	recode::driver.write<int>(base_address + offsets::glow_through_walls, 2);
}

auto Player::update(uintptr_t entity) -> bool
{
	base_address = entity;
	bone_cache.valid = false;

    int new_health = recode::driver.read<int>(entity + offsets::health);
    if (new_health <= 0)
        return false;
    health = new_health;

    int new_max_health = recode::driver.read<int>(entity + offsets::max_health);
    if (new_max_health <= 0)
        return false;
    max_health = new_max_health;

    team_id = recode::driver.read<int>(entity + offsets::team_id);

    origin = recode::driver.read<vec3>(entity + offsets::origin);
    if (origin == vec3())
        return false;

	uintptr_t model = recode::driver.read<uintptr_t>(entity + offsets::studiohdr);
	if (!model) return false;

	uintptr_t hdr = recode::driver.read<uintptr_t>(model + 0x8);
	if (!hdr) return false;

	uint16_t hitboxCache = recode::driver.read<uint16_t>(hdr + 0x34);
	uintptr_t hitboxArray = hdr + (static_cast<uint16_t>(hitboxCache & 0xFFFE) << (4 * (hitboxCache & 1)));
	if (!hitboxArray || hitboxArray < hdr) return false;

	uint16_t indexCache = recode::driver.read<uint16_t>(hitboxArray + 0x4);
	int hitboxIndex = (static_cast<uint16_t>(indexCache & 0xFFFE) << (4 * (indexCache & 1)));
	if (hitboxIndex < 0 || hitboxIndex > 0x10000) return false;

	uintptr_t bone_array = recode::driver.read<uintptr_t>(entity + offsets::bone_array);
	if (!bone_array) return false;

	for (int i = 0; i < 19; i++)
	{
		uint16_t bone = recode::driver.read<uint16_t>(hitboxArray + hitboxIndex + (i * 0x20));
		if (bone > 255) continue;
		if (bone_array + bone * sizeof(matrix3x4_t) < bone_array) continue;

		matrix3x4_t matrix = recode::driver.read<matrix3x4_t>(bone_array + bone * sizeof(matrix3x4_t));

		bone_cache.bone_array[i] = vec3(
			matrix.m_flMatVal[0][3] + origin.x,
			matrix.m_flMatVal[1][3] + origin.y,
			matrix.m_flMatVal[2][3] + origin.z
		);

		if (bone_cache.bone_array[i].x == 0 && bone_cache.bone_array[i].y == 0 && bone_cache.bone_array[i].z == 0)
				continue;
	}



    bone_cache.valid = true;
    return true;
}