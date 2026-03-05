#include "render_loop.h"
#include <core/systems/player_loop.h>
#include <core/systems/visuals/visuals.h>
#include <core/menu/menu.h>
#include <include/includes.h>

auto core::render_loop(std::shared_ptr<PlayerCache> p_cache, Overlay overlay) -> void
{
	MSG msg{};
	ZeroMemory(&msg, sizeof(MSG));

	while (msg.message != WM_QUIT)
	{
		if (!recode::is_running)
			break;

		while (PeekMessage(&msg, overlay.discord_hwnd, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (GetAsyncKeyState(VK_INSERT) & 1)
			settings::menu_key = !settings::menu_key;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (settings::menu_key) {
			L7Menu::menu();
			input();
		}
		else {
			SetWindowLong(overlay.discord_hwnd, GWL_EXSTYLE, WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW);
			SetFocus(nullptr);
		}

		const char* text = skCrypt("guys hes hacking report him").decrypt();
		ImVec2 text_size = ImGui::CalcTextSize(text);

		float pos_x = (1080.f - text_size.x) / 2.f;
		float pos_y = (1920.f - text_size.y) / 2.f;

		ImGui::GetBackgroundDrawList()->AddText(ImVec2(pos_x, pos_y), IM_COL32(255, 255, 255, 255), text);

		auto pcache = p_cache;
		if (!pcache) continue;

		player_loop(pcache);;
		ImGui::EndFrame();
		ImGui::Render();

		float clear[4] = { 0.f, 0.f, 0.f, 0.f };
		overlay.device_ctx->OMSetRenderTargets(1, &overlay.render_target, nullptr);
		overlay.device_ctx->ClearRenderTargetView(overlay.render_target, clear);

		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		overlay.swap_chain->Present(0, 0);
	}
}