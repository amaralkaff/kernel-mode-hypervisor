#include "render_utils.h"
#include <vector>
#include <algorithm>
#include <iostream>
#pragma warning(disable : 4244)

std::vector<std::pair<bones, bones>> skeleton = {
    // Spine
    { bones::Neck, bones::UpperChest },
    { bones::UpperChest, bones::LowerChest },
    { bones::LowerChest, bones::Stomach },
    { bones::Stomach, bones::Hip },

    // Left arm
    { bones::Neck, bones::Leftshoulder },
    { bones::Leftshoulder, bones::Leftelbow },
    { bones::Leftelbow, bones::Lefthand },

    // Right arm
    { bones::Neck, bones::Rightshoulder },
    { bones::Rightshoulder, bones::Rightelbow },
    { bones::Rightelbow, bones::Righthand },

    // Left leg
    { bones::Hip, bones::LeftThighs },
    { bones::LeftThighs, bones::Leftknees },
    { bones::Leftknees, bones::Leftleg },

    // Right leg
    { bones::Hip, bones::RightThighs },
    { bones::RightThighs, bones::Rightknees },
    { bones::Rightknees, bones::Rightleg },
};



const bones bounding_bones[] = {
    bones::Head,
    bones::Neck,
    bones::Leftshoulder,
    bones::Rightshoulder,
    bones::Rightelbow,
    bones::Leftelbow,
    bones::Leftleg,
    bones::Rightleg,
    bones::Hip
};

auto is_on_screen(const vec2& pos, Camera& camera, int margin) -> bool
{
    if (pos.x < -9000 || pos.y < -9000)
        return false;

    if (pos.x < -margin || pos.x > camera.get_width() + margin)
        return false;
    if (pos.y < -margin || pos.y > camera.get_height() + margin)
        return false;

    return true;
}

auto get_player_data(Player* player, Camera& camera) -> RenderData
{
    RenderData data{};
    data.valid = false;

    data.head3d = player->get_bone((int)bones::Head);
    data.head = camera.world_to_screen(data.head3d);
    vec2 root = camera.world_to_screen(player->get_origin());
   
    if (!is_on_screen(root, camera, 100))
        return data;

    if (!is_on_screen(data.head, camera, 100))
        return data;

    vec3 min_3d = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
    vec3 max_3d = vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (bones bone_id : bounding_bones) {
        vec3 bone_3d = player->get_bone((int)bone_id);
        min_3d.x = min(min_3d.x, bone_3d.x);
        min_3d.y = min(min_3d.y, bone_3d.y);
        min_3d.z = min(min_3d.z, bone_3d.z);
        max_3d.x = max(max_3d.x, bone_3d.x);
        max_3d.y = max(max_3d.y, bone_3d.y);
        max_3d.z = max(max_3d.z, bone_3d.z);
    }

    const float padding_3d = 5.8f;
    min_3d.x -= padding_3d;
    min_3d.y -= padding_3d;
    min_3d.z -= padding_3d;
    max_3d.x += padding_3d;
    max_3d.y += padding_3d;
    max_3d.z += padding_3d;

    vec3 corners[8] = {
        {min_3d.x, min_3d.y, min_3d.z},
        {max_3d.x, min_3d.y, min_3d.z},
        {min_3d.x, max_3d.y, min_3d.z},
        {max_3d.x, max_3d.y, min_3d.z},
        {min_3d.x, min_3d.y, max_3d.z},
        {max_3d.x, min_3d.y, max_3d.z},
        {min_3d.x, max_3d.y, max_3d.z},
        {max_3d.x, max_3d.y, max_3d.z}
    };

    float min_x = FLT_MAX, max_x = -FLT_MAX;
    float min_y = FLT_MAX, max_y = -FLT_MAX;

    for (int i = 0; i < 8; i++) {
        vec2 screen = camera.world_to_screen(corners[i]);
        if (screen.x < -9000 || screen.y < -9000)
            continue;
        min_x = min(min_x, screen.x);
        max_x = max(max_x, screen.x);
        min_y = min(min_y, screen.y);
        max_y = max(max_y, screen.y);
    }

    if (min_x == FLT_MAX || max_x == -FLT_MAX)
        return data;

    data.box_x = min_x;
    data.box_y = min_y;
    data.box_width = max_x - min_x;
    data.box_height = max_y - min_y;
    data.health = std::clamp((int)player->get_health(), 0, player->get_max_health());

    for (size_t i = 0; i < skeleton.size(); i++)
    {
        auto [from, to] = skeleton[i];
        vec3 from3d = player->get_bone((int)from);
        vec3 to3d = player->get_bone((int)to);

        vec2 from2d = camera.world_to_screen(from3d);
        vec2 to2d = camera.world_to_screen(to3d);
        data.skeleton_lines[i] = { from2d, to2d };
    }

    data.valid = true;
    return data;
}