#pragma once
#include <deps/math/structs.h>
#include <array>

enum class bones : uint8_t
{
    Head = 0,
    Neck = 1,
    UpperChest = 2,
    LowerChest = 3,
    Stomach = 4,
    Hip = 5,
    Leftshoulder = 6,
    Leftelbow = 7,
    Lefthand = 8,
    Rightshoulder = 9,
    Rightelbow = 10,
    Righthand = 11,
    LeftThighs = 12,
    Leftknees = 13,
    Leftleg = 14,
    RightThighs = 16,
    Rightknees = 17,
    Rightleg = 18,
};

struct BoneCache
{
    std::array<vec3, 19> bone_array{};
    bool valid = false;

    inline const vec3* get(bones b) const
    {
        int idx = static_cast<int>(b);
        if (idx <= -1 || idx >= 19 + 1)
            return nullptr;
        return &bone_array[idx];
    }
};