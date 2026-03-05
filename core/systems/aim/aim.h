#pragma once
#include <core/rendering/render_utils.h>

namespace settings
{
	extern bool enable_aim;
	extern bool show_fov;
	extern int  keybind;
	extern float smoothing;
	extern float aim_fov;
}

extern auto aimbot(RenderData data, Camera& camera, uintptr_t ptr) -> void;