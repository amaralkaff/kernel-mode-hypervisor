#pragma once
#include <include/includes.h>

class Overlay
{
public:
	auto  hijack_window() -> bool;
	auto  create_device() -> bool;
	auto destroy_device() -> bool;

	HWND discord_hwnd = nullptr;
	static ID3D11Device* device;
	static ID3D11DeviceContext* device_ctx;
	static IDXGISwapChain* swap_chain;
	static ID3D11RenderTargetView* render_target;
};

inline ID3D11ShaderResourceView* logor = nullptr;