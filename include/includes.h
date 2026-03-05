#pragma once
#include <Windows.h>
#include <iostream>
#include <mutex>
#include <atomic>
#include <thread>
#include <string>
#include <memory>
#include <chrono>
#include <vector>
#include <d3d11.h>
#include <wincodec.h>

// crypt
#include <deps/crypter/sk.h>

// memory
#include <deps/memory/mem.h>

// imgui
#include <deps/imgui/imgui.h>
#include <deps/imgui/imgui_internal.h>
#include <deps/imgui/imgui_impl_dx11.h>
#include <deps/imgui/imgui_impl_win32.h>

namespace recode
{
	inline float render_dist = 360.f;
	inline std::atomic_bool is_running = false;
	inline Kmem driver;
	inline uintptr_t process_image; // game assembly but im too lazy to change it
}