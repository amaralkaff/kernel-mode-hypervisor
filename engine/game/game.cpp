#include "game.h"
#include <engine/impl/offsets.h>
#include <include/includes.h>

auto Game::update() -> bool
{
	entity_list.clear();
	std::vector<uintptr_t> temp;

	uintptr_t local_player = recode::driver.read<uintptr_t>(recode::process_image + offsets::local_player);
	if (!local_player)
		return false;

	for (int i = 0; i < 15000; i++)
	{
		uintptr_t entity = recode::driver.read<uintptr_t>(recode::process_image + offsets::entity_list + (i * 0x20));
		if (!entity)
			continue;

		if (entity == local_player)
			continue;

		auto class_ptr = recode::driver.read<uintptr_t>(entity + 0x470);
		if (!class_ptr)
			continue;
		char buffer[128] = { 0 };
		if (!recode::driver.read_physical(PVOID(class_ptr), buffer, sizeof(buffer)))
			continue;
		buffer[sizeof(buffer) - 1] = '\0';
		std::string class_name(buffer);

		if (class_name.find("npc_dummie") == std::string::npos && class_name.find("player") == std::string::npos)
			continue;

		temp.push_back(entity);
	}

	entity_list.swap(temp);
	return true;
}