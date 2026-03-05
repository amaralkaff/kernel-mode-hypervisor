#include "camera.h"
#include <engine/impl/offsets.h>
#include <include/includes.h>

auto Camera::update() -> bool
{
	uintptr_t view_render = recode::driver.read<uintptr_t>(recode::process_image + offsets::view_render);
	if (!view_render)
		return false;
	
	view_matrix = recode::driver.read<view_matrix_t>(recode::driver.read < uintptr_t>(view_render + offsets::view_matrix));

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);

	return true;
}

auto Camera::world_to_screen(const vec3& world) const -> vec2
{
	float w = view_matrix.matrix[12] * world.x + view_matrix.matrix[13] * world.y + view_matrix.matrix[14] * world.z + view_matrix.matrix[15];
	if (w < 0.001f)
		return vec2(-1, -1);

	vec2 screen;

	screen.x = (view_matrix.matrix[0] * world.x + view_matrix.matrix[1] * world.y + view_matrix.matrix[2] * world.z + view_matrix.matrix[3]);
	screen.y = (view_matrix.matrix[4] * world.x + view_matrix.matrix[5] * world.y + view_matrix.matrix[6] * world.z + view_matrix.matrix[7]);

	float invw = 1.0f / w;
	screen.x *= invw;
	screen.y *= invw;

	float x = (float)width / 2;
	float y = (float)height / 2;

	x += 0.5f * screen.x * width + 0.5f;
	y -= 0.5f * screen.y * height + 0.5f;

	screen.x = x;
	screen.y = y;

	return screen;
}
