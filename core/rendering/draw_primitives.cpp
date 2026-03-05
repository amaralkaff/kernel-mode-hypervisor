#pragma once
#include "draw_primitives.h"

auto draw_cornered_box(float x, float y, float w, float h, const ImColor color, float thickness, ImDrawList* draw) -> void
{
    draw->AddLine(ImVec2(x, y), ImVec2(x, y + (h / 3)), color, thickness);
    draw->AddLine(ImVec2(x, y), ImVec2(x + (w / 3), y), color, thickness);
    draw->AddLine(ImVec2(x + w - (w / 3), y), ImVec2(x + w, y), color, thickness);
    draw->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + (h / 3)), color, thickness);
    draw->AddLine(ImVec2(x, y + h - (h / 3)), ImVec2(x, y + h), color, thickness);
    draw->AddLine(ImVec2(x, y + h), ImVec2(x + (w / 3), y + h), color, thickness);
    draw->AddLine(ImVec2(x + w - (w / 3), y + h), ImVec2(x + w, y + h), color, thickness);
    draw->AddLine(ImVec2(x + w, y + h - (h / 3)), ImVec2(x + w, y + h), color, thickness);
}

auto draw_head_dot(const vec2& head_pos, float radius, const ImColor color, ImDrawList* draw) -> void
{
    draw->AddCircleFilled(ImVec2(head_pos.x, head_pos.y), radius, color);
}

auto draw_health_bar(const RenderData& data, ImDrawList* draw) -> void
{
    float bar_width = 2.f;
    float hp_h = data.box_height * (data.health / 100.f);

    float bar_x1 = data.box_x - bar_width;
    float bar_x2 = data.box_x;
    float bar_y1 = data.box_y;
    float bar_y2 = data.box_y + data.box_height;

    draw->AddRectFilled(ImVec2(bar_x1, bar_y1), ImVec2(bar_x2, bar_y2), IM_COL32(0, 0, 0, 200));
    draw->AddRectFilled(ImVec2(bar_x1, bar_y2 - hp_h), ImVec2(bar_x2, bar_y2), ImColor(129, 255, 0, 255));
}

auto draw_skeleton(const RenderData& data, const ImColor color, ImDrawList* draw) -> void
{
    for (const auto& [from, to] : data.skeleton_lines)
        draw->AddLine(ImVec2(from.x, from.y), ImVec2(to.x, to.y), color, 1.f);
}

auto draw_name(const RenderData& data, const ImColor color, ImDrawList* draw) -> void {
    ImVec2 box_pos(data.box_x, data.box_y);
    std::string tag = data.name + " [" + data.weapon + "]"; // Added space for better readability

    ImVec2 text_size = ImGui::CalcTextSize(tag.c_str());
    ImVec2 text_pos(
        box_pos.x + (data.box_width / 2.0f) - (text_size.x / 2.0f),
        box_pos.y - text_size.y - 6.0f 
    );

    const float padding_x = 8.0f;
    const float padding_y = 4.0f;
    const float rounding = 4.0f;

    ImVec2 rect_min(text_pos.x - padding_x, text_pos.y - padding_y);
    ImVec2 rect_max(text_pos.x + text_size.x + padding_x, text_pos.y + text_size.y + padding_y);

    for (int i = 3; i > 0; --i) {
        float offset = i * 1.5f;
        ImVec2 glow_min(rect_min.x - offset, rect_min.y - offset);
        ImVec2 glow_max(rect_max.x + offset, rect_max.y + offset);
        draw->AddRectFilled(
            glow_min, glow_max,
            IM_COL32(color.Value.x * 255, color.Value.y * 255, color.Value.z * 255, 15 / i),
            rounding + offset
        );
    }

    ImU32 bg_top = IM_COL32(15, 15, 20, 200);
    ImU32 bg_bottom = IM_COL32(25, 25, 35, 180);
    draw->AddRectFilledMultiColor(
        rect_min, rect_max,
        bg_top, bg_top, bg_bottom, bg_bottom
    );

    ImVec2 border_start(rect_min.x, rect_min.y);
    ImVec2 border_end(rect_max.x, rect_min.y + 2.0f);
    ImU32 accent_left = color;
    ImU32 accent_right = IM_COL32(color.Value.x * 255 * 0.6f, color.Value.y * 255 * 0.6f, color.Value.z * 255 * 0.6f, 255);
    draw->AddRectFilledMultiColor(
        border_start, border_end,
        accent_left, accent_right, accent_right, accent_left
    );

    draw->AddRect(rect_min, rect_max, IM_COL32(255, 255, 255, 40), rounding, 0, 1.0f);

    ImVec2 shadow_pos(text_pos.x + 1.0f, text_pos.y + 1.0f);
    draw->AddText(shadow_pos, IM_COL32(0, 0, 0, 180), tag.c_str());

    draw->AddText(text_pos, color, tag.c_str());

    ImVec2 shine_min(rect_min.x, rect_min.y);
    ImVec2 shine_max(rect_max.x, rect_min.y + (rect_max.y - rect_min.y) * 0.4f);
    draw->AddRectFilledMultiColor(
        shine_min, shine_max,
        IM_COL32(255, 255, 255, 25), IM_COL32(255, 255, 255, 25),
        IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0)
    );
}
