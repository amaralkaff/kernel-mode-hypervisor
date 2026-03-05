#include <include/includes.h>
#include "aim.h"
#include <engine/impl/offsets.h>
#include <algorithm>
#include <cmath>
#define M_PI		3.14159265358979323846

namespace settings
{
	bool enable_aim = true;
	bool show_fov = true;
	bool visible_check = true;
	int  keybind = VK_RBUTTON;
	float smoothing = 5.f;
	float aim_fov = 100.0f;
}

vec3 CalcAngle(const vec3& src, const vec3& dst)
{
	vec3 angle;
	vec3 delta = vec3(dst.x - src.x, dst.y - src.y, dst.z - src.z);
	float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
	angle.x = atan2f(-delta.z, hyp) * (180.0f / (float)M_PI);

	angle.y = atan2f(delta.y, delta.x) * (180.0f / (float)M_PI);
	angle.z = 0.0f;

	while (angle.y > 180.f) angle.y -= 360.f;
	while (angle.y < -180.f) angle.y += 360.f;

	return angle;
}

auto aimbot(RenderData data, Camera& camera, uintptr_t ptr) -> void
{
	if (!data.valid)
		return;

	if (!settings::enable_aim)
		return;

	if (!(GetAsyncKeyState(settings::keybind)))
		return;

	vec3 old_angle = vec3();
	vec3 headeyes = recode::driver.read<vec3>(ptr + offsets::camera_pos);

	vec3 view_angle = recode::driver.read<vec3>(ptr + offsets::view_angles);
	vec3 target = CalcAngle(headeyes, data.head3d);

	vec3 delta = target - view_angle;

	while (delta.y > 180.f) delta.y -= 360.f;
	while (delta.y < -180.f) delta.y += 360.f;

	delta.x /= settings::smoothing * 30.f;
	delta.y /= settings::smoothing * 30.f;

	recode::driver.write<vec2>(ptr + offsets::view_angles, vec2(view_angle.x + delta.x, view_angle.y + delta.y));
}