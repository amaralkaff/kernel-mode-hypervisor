#pragma once
#include <cstdint>
#include <string>

class Weapon
{
public:
	[[nodiscard]] auto update(uintptr_t entity) -> bool; 
	
	auto set_skin(int id) -> void;
	auto set_model(const std::string& model_name, int model_index) -> void;
	auto set_glow(int id) -> void;

private:
	int skin_id = 0;

	float projectile_speed = 0.f;
	float projectile_scale = 0.f;

	std::string current_model = ""; 
	uintptr_t base_address = 0; // just current weapon
	uintptr_t vmbase_address = 0; // hands
	uintptr_t weapon = 0; // for skinchanger
};