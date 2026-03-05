#pragma once
#include <core/rendering/render_utils.h>
#include <vector>

namespace settings
{
    extern bool filter_ai;
    extern bool team_check;
    extern bool menu_key;
    extern bool box;
    extern bool snaplines;
    extern bool skeleton;
    extern bool name;
    extern bool headdot;
    extern bool health_bar;
    extern bool Glow;
    extern int glow_id;
}

extern auto draw_visuals(std::vector<RenderData> data, Camera& camera) -> void;