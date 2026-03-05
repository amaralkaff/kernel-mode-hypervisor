#pragma once
#include <deps/imgui/imgui.h>
#include <core/rendering/render_utils.h>

extern auto draw_cornered_box(float x, float y, float w, float h, const ImColor color, float thickness, ImDrawList* draw) -> void;
extern auto draw_health_bar(const RenderData& data, ImDrawList* draw) -> void;
extern auto draw_head_dot(const vec2& head_pos, float radius, const ImColor color, ImDrawList* draw) -> void;
extern auto draw_skeleton(const RenderData& data, const ImColor color, ImDrawList* draw) -> void;
extern auto draw_name(const RenderData& data, const ImColor color, ImDrawList* draw) -> void;