#include "player_loop.h"
#include <core/rendering/render_utils.h>
#include <core/rendering/draw_primitives.h>
#include <core/systems/visuals/visuals.h>
#include <core/systems/aim/aim.h>
#include <vector>
#include <iostream>
#include <include/includes.h>

auto player_loop(std::shared_ptr<PlayerCache> cache) -> void
{
    if (!cache)
        return;

    Camera cam{};
    if (!cam.update())
        return;

    const auto& players = cache->get_players();
    if (players->empty())
        return;

    std::vector<RenderData> draw_data;
    draw_data.reserve(players->size());

    size_t best_index = SIZE_MAX;
    float best_distance = FLT_MAX;

    float fov_squared = settings::aim_fov * settings::aim_fov;
    float center_x = cam.get_width() * 0.5f;
    float center_y = cam.get_height() * 0.5f;


    draw_data.clear();
    auto local = cache->get_local_player().get();
    for (auto& p : *players)
    {
        auto data = get_player_data(p.get(), cam);
        if (!data.valid)
            continue;

        draw_data.push_back(data);
        size_t current_index = draw_data.size() - 1;

        float dx = data.head.x - center_x;
        float dy = data.head.y - center_y;
        float dist_squared = dx * dx + dy * dy;

        if (dist_squared <= fov_squared && dist_squared < best_distance)
        {
            best_distance = dist_squared;
            best_index = current_index;
        }

    }

    if (best_index != SIZE_MAX && best_index < draw_data.size())
    {
        aimbot(draw_data[best_index], cam, local->get_entity());
    }

    draw_visuals(draw_data, cam);
}