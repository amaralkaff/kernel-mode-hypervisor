#pragma once
#include <cstdint>
#include <vector>

class Game
{
public:
	[[nodiscard]] auto update() -> bool;

	[[nodiscard]] auto get_entity_list() const -> std::vector<uintptr_t> { return entity_list; }
private:
	std::vector<uintptr_t> entity_list; 
};