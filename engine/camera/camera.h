#pragma once
#include <deps/math/structs.h>

class Camera
{
public:
	[[nodiscard]] auto update() -> bool;

	auto world_to_screen(const vec3& world) const -> vec2;

	[[nodiscard]] auto get_width() const -> int { return width; }
	[[nodiscard]] auto get_height() const -> int { return height; }
	[[nodiscard]] auto get_view_matrix() const -> const view_matrix_t& { return view_matrix; }

private:
	view_matrix_t view_matrix{};
	int width = 0;
	int height = 0;

};