#include "menu.h"
#include <include/includes.h>
#include <core/systems/visuals/visuals.h>
#include <core/systems/aim/aim.h>
#include <map>
#include <cmath>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <engine/engine_loop.h>
#include <deps/overlay/overlay.h>

// entire file is str8 claude 
static ImGuiKey VKToImGuiKey(int vk)
{
    switch (vk)
    {
    case VK_TAB: return ImGuiKey_Tab;
    case VK_LEFT: return ImGuiKey_LeftArrow;
    case VK_RIGHT: return ImGuiKey_RightArrow;
    case VK_UP: return ImGuiKey_UpArrow;
    case VK_DOWN: return ImGuiKey_DownArrow;
    case VK_PRIOR: return ImGuiKey_PageUp;
    case VK_NEXT: return ImGuiKey_PageDown;
    case VK_HOME: return ImGuiKey_Home;
    case VK_END: return ImGuiKey_End;
    case VK_INSERT: return ImGuiKey_Insert;
    case VK_DELETE: return ImGuiKey_Delete;
    case VK_BACK: return ImGuiKey_Backspace;
    case VK_SPACE: return ImGuiKey_Space;
    case VK_RETURN: return ImGuiKey_Enter;
    case VK_ESCAPE: return ImGuiKey_Escape;
    case VK_OEM_7: return ImGuiKey_Apostrophe;
    case VK_OEM_COMMA: return ImGuiKey_Comma;
    case VK_OEM_MINUS: return ImGuiKey_Minus;
    case VK_OEM_PERIOD: return ImGuiKey_Period;
    case VK_OEM_2: return ImGuiKey_Slash;
    case VK_OEM_1: return ImGuiKey_Semicolon;
    case VK_OEM_PLUS: return ImGuiKey_Equal;
    case VK_OEM_4: return ImGuiKey_LeftBracket;
    case VK_OEM_5: return ImGuiKey_Backslash;
    case VK_OEM_6: return ImGuiKey_RightBracket;
    case VK_OEM_3: return ImGuiKey_GraveAccent;
    case VK_CAPITAL: return ImGuiKey_CapsLock;
    case VK_SCROLL: return ImGuiKey_ScrollLock;
    case VK_NUMLOCK: return ImGuiKey_NumLock;
    case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
    case VK_PAUSE: return ImGuiKey_Pause;
    case VK_NUMPAD0: return ImGuiKey_Keypad0;
    case VK_NUMPAD1: return ImGuiKey_Keypad1;
    case VK_NUMPAD2: return ImGuiKey_Keypad2;
    case VK_NUMPAD3: return ImGuiKey_Keypad3;
    case VK_NUMPAD4: return ImGuiKey_Keypad4;
    case VK_NUMPAD5: return ImGuiKey_Keypad5;
    case VK_NUMPAD6: return ImGuiKey_Keypad6;
    case VK_NUMPAD7: return ImGuiKey_Keypad7;
    case VK_NUMPAD8: return ImGuiKey_Keypad8;
    case VK_NUMPAD9: return ImGuiKey_Keypad9;
    case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
    case VK_DIVIDE: return ImGuiKey_KeypadDivide;
    case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
    case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
    case VK_ADD: return ImGuiKey_KeypadAdd;
    case VK_LSHIFT: case VK_RSHIFT: return ImGuiKey_RightShift;
    case VK_LCONTROL: case VK_RCONTROL: return ImGuiKey_RightCtrl;
    case VK_F1: return ImGuiKey_F1;
    case VK_F2: return ImGuiKey_F2;
    case VK_F3: return ImGuiKey_F3;
    case VK_F4: return ImGuiKey_F4;
    case VK_F5: return ImGuiKey_F5;
    case VK_F6: return ImGuiKey_F6;
    case VK_F7: return ImGuiKey_F7;
    case VK_F8: return ImGuiKey_F8;
    case VK_F9: return ImGuiKey_F9;
    case VK_F10: return ImGuiKey_F10;
    case VK_F11: return ImGuiKey_F11;
    case VK_F12: return ImGuiKey_F12;
    case '0': return ImGuiKey_0;
    case '1': return ImGuiKey_1;
    case '2': return ImGuiKey_2;
    case '3': return ImGuiKey_3;
    case '4': return ImGuiKey_4;
    case '5': return ImGuiKey_5;
    case '6': return ImGuiKey_6;
    case '7': return ImGuiKey_7;
    case '8': return ImGuiKey_8;
    case '9': return ImGuiKey_9;
    case 'A': return ImGuiKey_A;
    case 'B': return ImGuiKey_B;
    case 'C': return ImGuiKey_C;
    case 'D': return ImGuiKey_D;
    case 'E': return ImGuiKey_E;
    case 'F': return ImGuiKey_F;
    case 'G': return ImGuiKey_G;
    case 'H': return ImGuiKey_H;
    case 'I': return ImGuiKey_I;
    case 'J': return ImGuiKey_J;
    case 'K': return ImGuiKey_K;
    case 'L': return ImGuiKey_L;
    case 'M': return ImGuiKey_M;
    case 'N': return ImGuiKey_N;
    case 'O': return ImGuiKey_O;
    case 'P': return ImGuiKey_P;
    case 'Q': return ImGuiKey_Q;
    case 'R': return ImGuiKey_R;
    case 'S': return ImGuiKey_S;
    case 'T': return ImGuiKey_T;
    case 'U': return ImGuiKey_U;
    case 'V': return ImGuiKey_V;
    case 'W': return ImGuiKey_W;
    case 'X': return ImGuiKey_X;
    case 'Y': return ImGuiKey_Y;
    case 'Z': return ImGuiKey_Z;
    default: return ImGuiKey_None;
    }
}

static bool g_KeyStates[256] = {};
static bool g_MouseStates[5] = {};
static float g_LastScrollTime = 0.0f;
static float g_AccumulatedScroll = 0.0f;

void input()
{
    ImGuiIO& io = ImGui::GetIO();

    // Mouse position
    POINT p;
    if (GetCursorPos(&p))
    {
        io.AddMousePosEvent((float)p.x, (float)p.y);
    }

    // Mouse buttons - track state changes
    bool mouse_states[5] = {
        (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0,
        (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0,
        (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0,
        (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0,
        (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0
    };

    for (int i = 0; i < 5; i++)
    {
        if (g_MouseStates[i] != mouse_states[i])
        {
            g_MouseStates[i] = mouse_states[i];
            io.AddMouseButtonEvent(i, mouse_states[i]);
        }
    }

    // Mouse wheel detection using high-order bits of GetAsyncKeyState
    // This is a workaround since GetAsyncKeyState doesn't directly report wheel delta
    static SHORT last_wheel_state = 0;
    SHORT wheel_state = GetAsyncKeyState(VK_MBUTTON);
    SHORT wheel_delta = wheel_state - last_wheel_state;

    if (wheel_delta != 0)
    {
        // Normalize the wheel delta
        float wheel_amount = (float)wheel_delta / 120.0f; // WHEEL_DELTA is typically 120

        if (abs(wheel_delta) > 1) // Filter out noise
        {
            io.AddMouseWheelEvent(0.0f, wheel_amount);
        }
    }
    last_wheel_state = wheel_state;

    // Modifiers
    bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    io.AddKeyEvent(ImGuiMod_Alt, alt);

    // Keyboard keys - only send on state change
    for (int vk = 0; vk < 256; vk++)
    {
        bool is_down = (GetAsyncKeyState(vk) & 0x8000) != 0;

        if (g_KeyStates[vk] != is_down)
        {
            g_KeyStates[vk] = is_down;

            ImGuiKey key = VKToImGuiKey(vk);
            if (key != ImGuiKey_None)
            {
                io.AddKeyEvent(key, is_down);
            }

            // Handle text input on key press
            if (is_down && vk >= 0x20 && vk <= 0xFE)
            {
                BYTE kb[256];
                if (GetKeyboardState(kb))
                {
                    WCHAR wch[4];
                    int result = ToUnicode(vk, MapVirtualKeyW(vk, MAPVK_VK_TO_VSC), kb, wch, 4, 0);

                    if (result > 0)
                    {
                        for (int i = 0; i < result; i++)
                        {
                            if (wch[i] > 0 && wch[i] < 0x10000)
                                io.AddInputCharacter((unsigned int)wch[i]);
                        }
                    }
                }
            }
        }
    }
}

namespace L7Menu
{
    // ?? Palette ??????????????????????????????
    static constexpr ImVec4 COL_BG = { 0.04f, 0.05f, 0.07f, 1.00f }; // #0A0D12
    static constexpr ImVec4 COL_BG2 = { 0.06f, 0.08f, 0.11f, 1.00f }; // panel bg
    static constexpr ImVec4 COL_BORDER = { 0.09f, 0.13f, 0.18f, 1.00f };
    static constexpr ImVec4 COL_ACCENT = { 0.22f, 0.64f, 0.76f, 1.00f }; // teal  #38A4C2
    static constexpr ImVec4 COL_ACCENT_DIM = { 0.14f, 0.40f, 0.50f, 1.00f };
    static constexpr ImVec4 COL_ACCENT_HOV = { 0.30f, 0.75f, 0.88f, 1.00f };
    static constexpr ImVec4 COL_TEXT = { 0.88f, 0.92f, 0.96f, 1.00f };
    static constexpr ImVec4 COL_TEXT_DIM = { 0.45f, 0.52f, 0.60f, 1.00f };
    static constexpr ImVec4 COL_WIDGET_BG = { 0.08f, 0.10f, 0.14f, 1.00f };
    static constexpr ImVec4 COL_WIDGET_ACT = { 0.11f, 0.15f, 0.20f, 1.00f };
    static constexpr ImVec4 COL_SEPARATOR = { 0.10f, 0.14f, 0.19f, 1.00f };
    static constexpr ImVec4 COL_HEADER_SEL = { 0.14f, 0.40f, 0.50f, 0.50f };
    static constexpr ImVec4 COL_TRANSPARENT = { 0.00f, 0.00f, 0.00f, 0.00f };

    // ?? State ?????????????????????????????????
    static int  s_tab = 0;   // 0 = Visuals, 1 = World
    static bool s_init_style = false;

    // ?? Helpers ???????????????????????????????
    static inline ImVec4 WithAlpha(ImVec4 c, float a) { return { c.x, c.y, c.z, a }; }

    // Draw the "L7" logo using ImDrawList primitives (flat teal, parallelogram style)
    static void DrawLogo(ImDrawList* dl, ImVec2 origin, float scale = 1.f)
    {
        const ImU32 col = IM_COL32(58, 168, 178, 255);   // flat teal  (#3AA8B2)
        const ImU32 col_dark = IM_COL32(32, 110, 118, 255);   // darker face for depth

        // Helper: map a local (x,y) point ? screen space
        auto P = [&](float x, float y) -> ImVec2 {
            return ImVec2(origin.x + x * scale, origin.y + y * scale);
            };

        // -----------------------------------------------------------------------
        // Shared skew: each "column" leans right by `sk` units per row.
        // A stroke of height H and width W becomes a parallelogram:
        //   top-left, top-right, bottom-right, bottom-left
        // with the top edge shifted right by sk relative to the bottom edge.
        // -----------------------------------------------------------------------
        //
        // Logo bounding box ? 100 � 80 units
        //
        // Stroke thickness (T) and horizontal skew per full height (SK):
        //   T  = 14   (stroke width)
        //   SK = 10   (top of vertical stroke is shifted right by 10 vs bottom)
        //
        //            x=0       x=100
        //   y=0  ___________________
        //        |  L-vert | 7-top |
        //   y=40 |         |   \   |
        //        | L-bot   |  7-diag
        //   y=80 |_________|_______|
        //

        const float T = 14.f;   // stroke thickness
        const float SK = 10.f;   // skew offset (top shifts right relative to bottom)

        // ===== "L" � LEFT GLYPH =================================================

        // L vertical bar
        // Runs from y=0 (top) to y=80 (bottom), left side of logo
        // x range at bottom: 0 ? T ; x range at top: SK ? T+SK
        {
            // quad: TL, TR, BR, BL
            ImVec2 tl = P(0.f + SK, 0.f);
            ImVec2 tr = P(T + SK, 0.f);
            ImVec2 br = P(T, 80.f);
            ImVec2 bl = P(0.f, 80.f);
            dl->AddQuadFilled(tl, tr, br, bl, col);
            // subtle darker left face for a mild bevel feel
            dl->AddQuadFilled(tl, P(tl.x + 2.f * scale, tl.y), P(bl.x + 2.f * scale, bl.y), bl, col_dark);
        }

        // L horizontal bar (bottom)
        // Runs from x=0 to x=52 at y=80, height T upward (with skew)
        {
            float x0 = 0.f, x1 = 52.f;
            float y0 = 80.f - T, y1 = 80.f;
            float sk_frac = SK * (T / 80.f);  // skew contribution for height T
            ImVec2 tl = P(x0 + sk_frac, y0);
            ImVec2 tr = P(x1 + sk_frac, y0);
            ImVec2 br = P(x1, y1);
            ImVec2 bl = P(x0, y1);
            dl->AddQuadFilled(tl, tr, br, bl, col);
        }

        // ===== "7" � RIGHT GLYPH ================================================

        // 7 horizontal bar (top)
        // Runs from x=28 to x=100 at y=0, height T downward
        {
            float x0 = 28.f, x1 = 100.f;
            float y0 = 0.f, y1 = T;
            float sk_frac = SK * (T / 80.f);
            ImVec2 tl = P(x0 + SK, y0);
            ImVec2 tr = P(x1 + SK, y0);
            ImVec2 br = P(x1 + SK - sk_frac, y1);
            ImVec2 bl = P(x0 + SK - sk_frac, y1);
            dl->AddQuadFilled(tl, tr, br, bl, col);
            // top highlight
            dl->AddQuadFilled(tl, tr, P(tr.x, tr.y + 2.f * scale), P(tl.x, tl.y + 2.f * scale), col_dark);
        }

        // 7 diagonal bar
        // Goes from top-right area down to bottom-left area.
        // Top-right corner  ? (88, 14) ? (100, 14)
        // Bottom-left corner ? (38, 80) ? (52, 80)  (meets L's bottom bar)
        // We define it as a thick quad along the diagonal.
        {
            // The diagonal stroke: four corners of a thick parallelogram
            // Top edge (at y?14, right side of logo)
            ImVec2 tl = P(74.f + SK, 14.f);
            ImVec2 tr = P(88.f + SK, 14.f);
            // Bottom edge (at y=80, meeting the L's bottom bar)
            ImVec2 br = P(52.f, 80.f);
            ImVec2 bl = P(38.f, 80.f);
            dl->AddQuadFilled(tl, tr, br, bl, col);
        }
    }

    // ?? Style setup ???????????????????????????
    static void ApplyStyle()
    {
        ImGuiStyle& st = ImGui::GetStyle();

        st.WindowPadding = { 0, 0 };
        st.FramePadding = { 10, 6 };
        st.ItemSpacing = { 8, 6 };
        st.ItemInnerSpacing = { 6, 4 };
        st.ScrollbarSize = 8;
        st.GrabMinSize = 8;
        st.WindowBorderSize = 1;
        st.ChildBorderSize = 0;
        st.FrameBorderSize = 0;
        st.PopupBorderSize = 1;
        st.WindowRounding = 6;
        st.ChildRounding = 4;
        st.FrameRounding = 4;
        st.PopupRounding = 4;
        st.ScrollbarRounding = 4;
        st.GrabRounding = 4;
        st.TabRounding = 4;

        ImVec4* c = st.Colors;
        c[ImGuiCol_WindowBg] = COL_BG;
        c[ImGuiCol_ChildBg] = COL_BG2;
        c[ImGuiCol_PopupBg] = COL_BG;
        c[ImGuiCol_Border] = COL_BORDER;
        c[ImGuiCol_BorderShadow] = COL_TRANSPARENT;
        c[ImGuiCol_FrameBg] = COL_WIDGET_BG;
        c[ImGuiCol_FrameBgHovered] = COL_WIDGET_ACT;
        c[ImGuiCol_FrameBgActive] = COL_WIDGET_ACT;
        c[ImGuiCol_TitleBg] = COL_BG;
        c[ImGuiCol_TitleBgActive] = COL_BG;
        c[ImGuiCol_TitleBgCollapsed] = COL_BG;
        c[ImGuiCol_MenuBarBg] = COL_BG;
        c[ImGuiCol_ScrollbarBg] = COL_BG;
        c[ImGuiCol_ScrollbarGrab] = COL_ACCENT_DIM;
        c[ImGuiCol_ScrollbarGrabHovered] = COL_ACCENT;
        c[ImGuiCol_ScrollbarGrabActive] = COL_ACCENT_HOV;
        c[ImGuiCol_CheckMark] = COL_ACCENT;
        c[ImGuiCol_SliderGrab] = COL_ACCENT;
        c[ImGuiCol_SliderGrabActive] = COL_ACCENT_HOV;
        c[ImGuiCol_Button] = COL_WIDGET_BG;
        c[ImGuiCol_ButtonHovered] = WithAlpha(COL_ACCENT, 0.25f);
        c[ImGuiCol_ButtonActive] = WithAlpha(COL_ACCENT, 0.40f);
        c[ImGuiCol_Header] = COL_HEADER_SEL;
        c[ImGuiCol_HeaderHovered] = WithAlpha(COL_ACCENT, 0.20f);
        c[ImGuiCol_HeaderActive] = WithAlpha(COL_ACCENT, 0.35f);
        c[ImGuiCol_Separator] = COL_SEPARATOR;
        c[ImGuiCol_SeparatorHovered] = COL_ACCENT_DIM;
        c[ImGuiCol_SeparatorActive] = COL_ACCENT;
        c[ImGuiCol_ResizeGrip] = COL_TRANSPARENT;
        c[ImGuiCol_ResizeGripHovered] = WithAlpha(COL_ACCENT, 0.3f);
        c[ImGuiCol_ResizeGripActive] = COL_ACCENT;
        c[ImGuiCol_Tab] = COL_WIDGET_BG;
        c[ImGuiCol_TabHovered] = WithAlpha(COL_ACCENT, 0.25f);
        c[ImGuiCol_TabActive] = COL_ACCENT_DIM;
        c[ImGuiCol_TabUnfocused] = COL_WIDGET_BG;
        c[ImGuiCol_TabUnfocusedActive] = COL_ACCENT_DIM;
        c[ImGuiCol_PlotLines] = COL_ACCENT;
        c[ImGuiCol_PlotHistogram] = COL_ACCENT;
        c[ImGuiCol_TextSelectedBg] = WithAlpha(COL_ACCENT, 0.35f);
        c[ImGuiCol_Text] = COL_TEXT;
        c[ImGuiCol_TextDisabled] = COL_TEXT_DIM;
        c[ImGuiCol_NavHighlight] = COL_ACCENT;
    }

    // ?? Custom toggle (checkbox replacement) ??
    static bool Toggle(const char* label, bool* v)
    {
        ImGui::PushID(label);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        float  w = 36.f;
        float  h = 18.f;
        float  radius = h * 0.5f;

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Invisible button for interaction
        ImGui::SetCursorScreenPos(pos);
        bool clicked = ImGui::InvisibleButton("##tgl", { w + 8 + ImGui::CalcTextSize(label).x, h });
        if (clicked) *v = !*v;

        bool hov = ImGui::IsItemHovered();
        float t = *v ? 1.f : 0.f;

        ImU32 track = *v
            ? IM_COL32(56, 164, 194, 255)
            : IM_COL32(22, 30, 42, 255);

        if (hov)
            track = *v
            ? IM_COL32(80, 195, 220, 255)
            : IM_COL32(35, 48, 62, 255);

        // Track
        dl->AddRectFilled(pos, { pos.x + w, pos.y + h }, track, radius);
        // Track border
        dl->AddRect(pos, { pos.x + w, pos.y + h },
            *v ? IM_COL32(56, 164, 194, 180) : IM_COL32(50, 65, 85, 200),
            radius, 0, 1.f);

        // Knob
        float knob_x = pos.x + radius + t * (w - h);
        dl->AddCircleFilled({ knob_x, pos.y + radius }, radius - 2.f,
            IM_COL32(255, 255, 255, 240));

        // Label
        ImGui::SetCursorScreenPos({ pos.x + w + 8.f, pos.y + 1.f });
        ImGui::TextColored(COL_TEXT, "%s", label);

        ImGui::PopID();
        return clicked;
    }

    // ?? Section header with accent line ???????
    static void SectionHeader(const char* title)
    {
        ImGui::Spacing();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float  w = ImGui::GetContentRegionAvail().x;
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Accent glow line
        dl->AddRectFilled(
            { p.x, p.y + 9.f },
            { p.x + 2.f, p.y + 17.f },
            IM_COL32(56, 164, 194, 255), 1.f
        );
        dl->AddRectFilled(
            { p.x + 3.f, p.y + 11.f },
            { p.x + 3.f + 60.f, p.y + 15.f },
            IM_COL32(56, 164, 194, 30), 0.f
        );

        ImGui::SetCursorScreenPos({ p.x + 9.f, p.y });
        ImGui::TextColored(COL_ACCENT, "%s", title);
        ImGui::SetCursorScreenPos({ p.x, p.y + 26.f });
        // Thin separator
        dl->AddLine(
            { p.x + 9.f, p.y + 25.f },
            { p.x + w,   p.y + 25.f },
            IM_COL32(30, 50, 65, 200), 1.f
        );
        ImGui::Dummy({ 0, 6.f });
    }

    // ?? Custom tab bar ????????????????????????
    static void DrawTabBar(const char** tabs, int count, int* selected, float width)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float  tabW = width / (float)count;
        float  h = 36.f;

        for (int i = 0; i < count; i++)
        {
            float x0 = cursor.x + i * tabW;
            ImVec2 tmin = { x0, cursor.y };
            ImVec2 tmax = { x0 + tabW, cursor.y + h };

            bool isActive = (*selected == i);
            bool hov = ImGui::IsMouseHoveringRect(tmin, tmax);

            ImU32 bg = isActive
                ? IM_COL32(22, 64, 78, 255)
                : (hov ? IM_COL32(15, 22, 30, 255) : IM_COL32(10, 14, 19, 255));

            dl->AddRectFilled(tmin, tmax, bg);

            // Active bottom accent bar
            if (isActive)
                dl->AddRectFilled(
                    { x0 + 6.f, cursor.y + h - 2.f },
                    { x0 + tabW - 6.f, cursor.y + h },
                    IM_COL32(56, 164, 194, 255), 2.f
                );

            // Tab label centered
            ImVec2 tsize = ImGui::CalcTextSize(tabs[i]);
            ImVec2 tpos = { x0 + (tabW - tsize.x) * 0.5f, cursor.y + (h - tsize.y) * 0.5f };
            dl->AddText(tpos,
                isActive ? IM_COL32(56, 164, 194, 255)
                : (hov ? IM_COL32(130, 170, 190, 255) : IM_COL32(80, 100, 120, 255)),
                tabs[i]);

            // Separator
            if (i < count - 1)
                dl->AddLine({ x0 + tabW, cursor.y + 6.f }, { x0 + tabW, cursor.y + h - 6.f },
                    IM_COL32(30, 44, 56, 255), 1.f);

            // Handle click
            if (hov && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                *selected = i;
        }

        ImGui::SetCursorScreenPos({ cursor.x, cursor.y + h });
    }

    // ?? Main render ???????????????????????????
    void menu()
    {

        if (!s_init_style) { ApplyStyle(); s_init_style = true; }

        constexpr float WIN_W = 480.f;
        constexpr float WIN_H = 520.f;

        ImGui::SetNextWindowSize({ WIN_W, WIN_H }, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.f);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::Begin("##L7Menu", 0, flags);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2      wpos = ImGui::GetWindowPos();

        // ?? Header bar ??????????????????????????
        {
            float hh = 54.f;
            dl->AddRectFilled(wpos, { wpos.x + WIN_W, wpos.y + hh },
                IM_COL32(6, 9, 13, 255));

            // subtle accent glow along bottom of header
            dl->AddRectFilled(
                { wpos.x, wpos.y + hh - 1.f },
                { wpos.x + WIN_W, wpos.y + hh },
                IM_COL32(56, 164, 194, 80)
            );
            dl->AddRectFilled(
                { wpos.x, wpos.y + hh - 4.f },
                { wpos.x + WIN_W, wpos.y + hh - 1.f },
                IM_COL32(56, 164, 194, 20)
            );

            // center it
			ImGui::SetCursorPos({ (WIN_W - 47.f) * 0.02f, 6.f });
            ImGui::Image(logor, ImVec2(47.f, 47.f));

            // Title text
            dl->AddText(ImGui::GetFont(), 15.f,
                { wpos.x + 64.f, wpos.y + 12.f },
                IM_COL32(56, 164, 194, 255), "Apex");
            dl->AddText(ImGui::GetFont(), 13.f,
                { wpos.x + 64.f, wpos.y + 30.f },
                IM_COL32(80, 110, 128, 255), "palepale");

            // Version pill
            const char* ver = "v1.0";
            ImVec2 vsz = ImGui::CalcTextSize(ver);
            float vx = wpos.x + WIN_W - vsz.x - 18.f;
            dl->AddRectFilled(
                { vx - 6.f, wpos.y + 17.f },
                { vx + vsz.x + 6.f, wpos.y + 36.f },
                IM_COL32(22, 64, 78, 200), 10.f
            );
            dl->AddText({ vx, wpos.y + 18.f },
                IM_COL32(56, 164, 194, 255), ver);

            ImGui::SetCursorScreenPos({ wpos.x, wpos.y + hh });
        }

        // ?? Tab bar ?????????????????????????????
        {
            const char* tabs[] = { "VISUALS", "CONVARS", "AIM"};
            float tw = WIN_W;
            DrawTabBar(tabs, 3, &s_tab, tw);
        }

        // ?? Thin accent line below tabs ?????????
        {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(
                { wpos.x, cp.y },
                { wpos.x + WIN_W, cp.y + 1.f },
                IM_COL32(20, 40, 52, 255)
            );
        }

        // ?? Content area ?????????????????????????
        ImGui::SetNextWindowContentSize({ WIN_W - 24.f, 0 });
        ImGui::BeginChild("##content", { WIN_W, WIN_H - 54.f - 36.f - 30.f }, false,
            ImGuiWindowFlags_NoScrollbar);

        ImGui::SetCursorPos({ 14.f, 10.f });
        ImGui::BeginGroup();

        if (s_tab == 0) // ?? VISUALS ??????????????
        {
            SectionHeader("ESP");

            ImGui::SetCursorPosX(14.f);
            Toggle("Skeleton", &settings::skeleton);
            ImGui::SetCursorPosX(14.f);
            Toggle("Bounding Box", &settings::box);
            ImGui::SetCursorPosX(14.f);
            Toggle("Head Dot", &settings::headdot);
            ImGui::SetCursorPosX(14.f);
            Toggle("Health Bar", &settings::health_bar);
            ImGui::SetCursorPosX(14.f);
            Toggle("Team Check", &settings::team_check);

            SectionHeader("GLOW");

            ImGui::SetCursorPosX(14.f);
            Toggle("Enable Glow", &settings::Glow);

            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT_HOV);
            ImGui::SliderInt("##glowid", &settings::glow_id, -1, 120, "Glow ID: %d");
            ImGui::PopStyleColor(3);

            SectionHeader("MISC");

            ImGui::SetCursorPosX(14.f);
			Toggle("Local Glow", &settings::local_glow);
            // Weapon Glow
            ImGui::SetCursorPosX(14.f);
            Toggle("Weapon Glow", &settings::weapon_glow);

            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT);
            ImGui::SliderInt("##wgid", &settings::weapon_glow_id, -1, 120, "Weapon Glow ID: %d");
            ImGui::PopStyleColor(3);
        }
        else if (s_tab == 1) // ?? WORLD ????????????????????????????
        {
            SectionHeader("Convars");

            ImGui::SetCursorPosX(14.f);
            Toggle("Full Bright", &settings::fullbright);
            ImGui::SetCursorPosX(14.f);
			Toggle("No Sky", &settings::no_sky);
            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT);
            ImGui::SliderInt("##3pdist", &settings::third_person_distance, 0, 10, "Third person dist: %d");
            ImGui::PopStyleColor(3);
            ImGui::SetCursorPosX(14.f);
			Toggle("Fov Changer", &settings::anti_aim);
            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT);
			ImGui::SliderFloat("##fovamt", &settings::desired_fov, 0.f, 180.f, "Fov Amount: %.1f");
            ImGui::PopStyleColor(3);
            ImGui::SetCursorPosX(14.f);
            Toggle("Gamma modifier", &settings::bloom);
            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT_DIM);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT);
            ImGui::SliderFloat("##blmamt", &settings::bloom_intensity, 1.f, 100.f, "Bloom Amount: %.1f");
            ImGui::PopStyleColor(3);
            ImGui::SetCursorPosX(14.f);
            if (ImGui::Button("true stretch (change ingame res to 4:3)"))
            {
                settings::aspr = true;
            }
        }
        else // ?? AIM ??????????????????????????????????????????
        {
            SectionHeader("Aimbot");

            ImGui::SetCursorPosX(14.f);
            Toggle("Enable Aimbot", &settings::enable_aim);

            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT_HOV);
            ImGui::SliderFloat("##fov", &settings::aim_fov, 0.f, 800.f, "FOV: %.1f");
            ImGui::PopStyleColor(3);

            ImGui::SetCursorPosX(14.f);
            ImGui::SetNextItemWidth(WIN_W - 52.f);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, COL_WIDGET_BG);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, COL_ACCENT);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, COL_ACCENT_HOV);
            ImGui::SliderFloat("##smooth", &settings::smoothing, 1.f, 50.f, "Smooth: %.1f");
            ImGui::PopStyleColor(3);
        }

        ImGui::EndGroup();
        ImGui::EndChild();

        // ?? Footer ???????????????????????????????
        {
            ImVec2 fp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(
                { wpos.x, fp.y },
                { wpos.x + WIN_W, fp.y + 30.f },
                IM_COL32(5, 7, 10, 255)
            );
            dl->AddRectFilled(
                { wpos.x, fp.y },
                { wpos.x + WIN_W, fp.y + 1.f },
                IM_COL32(56, 164, 194, 40)
            );

            // Menu key toggle hint
        }

        ImGui::End();
    }

} //