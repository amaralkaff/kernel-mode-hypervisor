#pragma once
#include <cstdint>

// apex offsets — global ptrs shift per patch, struct offsets are stable

namespace offsets
{
	// --- global ptrs (run dump_apex.py after patches) ---
	constexpr uintptr_t entity_list = 0x1f62278;       // cl_entitylist
	constexpr uintptr_t local_player = 0x24354F8;      // LocalPlayer
	constexpr uintptr_t view_render = 0x76e9ab8;       // ViewRender
	constexpr uintptr_t name_list = 0xd427360;         // NameList
	constexpr uintptr_t model_name = 0xd427360;        // same as name_list
	constexpr uintptr_t highlight_settings = 0xb1db5a0; // HighlightSettings

	// --- view matrix ---
	constexpr uintptr_t view_matrix = 0x11A350;

	// --- entity ---
	constexpr uintptr_t health = 0x0328;               // m_iHealth
	constexpr uintptr_t max_health = 0x0470;            // m_iMaxHealth
	constexpr uintptr_t sheild = 0x01a0;               // m_shieldHealth
	constexpr uintptr_t max_sheild = 0x01a4;           // m_shieldHealthMax
	constexpr uintptr_t team_id = 0x0338;              // m_iTeamNum
	constexpr uintptr_t life_state = 0x0690;           // m_lifeState
	constexpr uintptr_t bleedout_state = 0x2760;       // m_bleedoutState
	constexpr uintptr_t origin = 0x017c;               // m_vecAbsOrigin
	constexpr uintptr_t abs_velocity = 0x0170;         // m_vecAbsVelocity

	// --- bones ---
	constexpr uintptr_t bone_array = 0x0db0 + 0x48;   // m_nForceBone + 0x48
	constexpr uintptr_t studiohdr = 0x1000;            // CBaseAnimating!m_pStudioHdr

	// --- weapon ---
	constexpr uintptr_t last_active = 0x1944;          // m_latestPrimaryWeapons
	constexpr uintptr_t skin_id = 0x0d68;              // weapon skin ID
	constexpr uintptr_t view_model = 0x2d98;           // m_hViewModels
	constexpr uintptr_t bullet_speed = 0x19d8 + 0x04ec; // CWeaponX!m_flProjectileSpeed
	constexpr uintptr_t bullet_scale = 0x19d8 + 0x04f4; // CWeaponX!m_flProjectileScale
	constexpr uintptr_t zoom_fov = 0x15e0 + 0x00b8;   // m_playerData + m_curZoomFOV
	constexpr uintptr_t ammo = 0x1590;                 // m_ammoInClip

	// --- aim / view ---
	constexpr uintptr_t aimpunch = 0x2438;             // m_currentFrameLocalPlayer.m_vecPunchWeapon_Angle
	constexpr uintptr_t camera_pos = 0x1ee0;           // CPlayer!camera_origin
	constexpr uintptr_t view_angles = 0x2534 - 0x14;  // m_ammoPoolCapacity - 0x14
	constexpr uintptr_t breath_angles = view_angles - 0x10;
	constexpr uintptr_t visible_time = 0x19a0;         // CPlayer!lastVisibleTime
	constexpr uintptr_t zooming = 0x1be1;              // m_bZooming
	constexpr uintptr_t yaw = 0x223c - 0x8;            // m_currentFramePlayer.m_ammoPoolCount - 0x8

	// --- observer ---
	constexpr uintptr_t observer_mode = 0x3584;        // m_iObserverMode
	constexpr uintptr_t observing_target = 0x3590;     // m_hObserverTarget

	// --- glow ---
	constexpr uintptr_t item_glow = 0x02f0;            // m_highlightFunctionBits
	constexpr uintptr_t glow_enable = 0x28C;           // 7=enabled, 2=disabled
	constexpr uintptr_t glow_through_walls = 0x26c;    // 2=enabled, 5=disabled
	constexpr uintptr_t glow_fix = 0x278;              // glow fix
	constexpr uintptr_t glow_context_id = 0x29c;       // glow context ID
	constexpr uintptr_t glow_t1 = 0x292;               // 16256=enabled, 0=disabled
	constexpr uintptr_t glow_t2 = 0x30c;               // 1193322764=enabled, 0=disabled
}
