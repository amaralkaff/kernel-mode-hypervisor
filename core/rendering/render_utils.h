#pragma once
#pragma once
#include <deps/math/structs.h>
#include <engine/player/player.h>
#include <engine/camera/camera.h>
#include <windows.h>
#include <array>
#include <string>

struct RenderData
{
    vec2 head;
    vec3 head3d;
    float box_x, box_y;
    float box_width, box_height;
    std::array<std::pair<vec2, vec2>, 16> skeleton_lines;
    std::string name;
    std::string weapon;
    int health;
    int team_id;
    bool valid;
};

extern auto is_on_screen(const vec2& pos, Camera& camera, int margin = 100) -> bool;
extern auto get_player_data(Player* player, Camera& camera) -> RenderData;