#include "visuals.h"
#include <core/rendering/draw_primitives.h>
#include <core/systems/aim/aim.h>

namespace settings
{
    bool menu_key = true;
    bool team_check = true;
    bool box = true;
    bool skeleton = true;
    bool headdot = false;
    bool health_bar = true;
	bool Glow = false;
    int glow_id = 64 ;
}

auto draw_visuals(std::vector<RenderData> data, Camera& camera) -> void
{
    if (data.empty())
        return;

    auto* draw = ImGui::GetBackgroundDrawList();

    if (settings::enable_aim && settings::show_fov)
    {
        draw->AddCircle(ImVec2(camera.get_width() / 2.0f, camera.get_height() / 2.0f), settings::aim_fov, IM_COL32(255, 255, 255, 100), 64);
    }

    for (const auto& data : data)
    {
        const ImColor color = ImColor(220, 220, 220, 255);
        if (settings::box)
            draw_cornered_box(data.box_x, data.box_y, data.box_width, data.box_height, color, 1.f, draw);

        if (settings::headdot)
            draw_head_dot(data.head, 1.f, color, draw);

        if (settings::skeleton)
            draw_skeleton(data, ImColor(217, 252, 255, 255), draw);

        if (settings::health_bar)
            draw_health_bar(data, draw);
    }
}