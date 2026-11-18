#include "settings_window.h"
#include "config.h"
#include "overlay_ui.h"
#include "toast_window.h"
#include "hotkeys.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <dwmapi.h>
#include <algorithm>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct ImGuiContextScope {
    ImGuiContext* prev;
    ImGuiContextScope(ImGuiContext* ctx) {
        prev = ImGui::GetCurrentContext();
        if (ctx) ImGui::SetCurrentContext(ctx);
    }
    ~ImGuiContextScope() {
        if (prev) ImGui::SetCurrentContext(prev);
    }
};

static UINT GetWindowDPI(HWND hwnd) {
    typedef UINT(WINAPI* GetDpiForWindowProc)(HWND);
    static HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32 && hwnd) {
        static GetDpiForWindowProc fn = (GetDpiForWindowProc)GetProcAddress(hUser32, "GetDpiForWindow");
        if (fn) {
            UINT dpi = fn(hwnd);
            if (dpi > 0) return dpi;
        }
    }
    return 96;
}

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (SettingsWindow::Instance().IsRecording()) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) {
            if (wParam == VK_ESCAPE) {
                SettingsWindow::Instance().CancelRecording();
                return 0;
            }
            if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
                wParam == VK_SHIFT   || wParam == VK_LSHIFT   || wParam == VK_RSHIFT ||
                wParam == VK_MENU    || wParam == VK_LMENU    || wParam == VK_RMENU ||
                wParam == VK_LWIN    || wParam == VK_RWIN) {
                return 0;
            }

            unsigned int mod = 0;
            if (GetKeyState(VK_CONTROL) & 0x8000) mod |= 0x0002;
            if (GetKeyState(VK_MENU) & 0x8000)    mod |= 0x0001;
            if (GetKeyState(VK_SHIFT) & 0x8000)   mod |= 0x0004;
            if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mod |= 0x0008;

            SettingsWindow::Instance().ApplyRecordedHotkey(mod, static_cast<unsigned int>(wParam));
            return 0;
        } else if (msg == WM_KEYUP || msg == WM_SYSKEYUP) {
            return 0;
        } else if (msg == WM_KILLFOCUS) {
            SettingsWindow::Instance().CancelRecording();
        }
    }

    ImGuiContextScope scope(SettingsWindow::Instance().GetImGuiContext());

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_DPICHANGED: {
        UINT new_dpi_x = LOWORD(wParam);
        float dpi_scale = (float)new_dpi_x / 96.0f;
        if (SettingsWindow::Instance().GetImGuiContext()) {
            ImGuiContextScope local_scope(SettingsWindow::Instance().GetImGuiContext());
            ImGui::GetStyle().FontScaleDpi = dpi_scale;
        }
        RECT* prc = (RECT*)lParam;
        if (prc) {
            SetWindowPos(hwnd, nullptr,
                         prc->left, prc->top,
                         prc->right - prc->left,
                         prc->bottom - prc->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    }

    case WM_NCCALCSIZE:
        if (wParam == TRUE) {
            return 0; // Eliminates any non-client borders/white seams
        }
        break;

    case WM_CLOSE:
        SettingsWindow::Instance().Hide();
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            SettingsWindow::Instance().Hide();
            return 0;
        }
        break;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED && LOWORD(lParam) > 0 && HIWORD(lParam) > 0) {
            SettingsWindow::Instance().OnResize((UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

SettingsWindow& SettingsWindow::Instance() {
    static SettingsWindow instance;
    return instance;
}

bool SettingsWindow::Init(HINSTANCE hInstance, ID3D11Device* device, ID3D11DeviceContext* context) {
    m_hInstance = hInstance;
    m_device = device;
    m_context = context;

    // 1. Register Window Class
    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        SettingsWndProc,
        0L, 0L,
        hInstance,
        nullptr,
        LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr,
        L"SC2SettingsWindowNative",
        nullptr
    };
    RegisterClassExW(&wc);

    // 2. Position Centered on Screen, dynamically sized based on screen resolution
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int dummy_mw, dummy_mh, dummy_th, win_w, win_h;
    float dummy_fs, dummy_ts;
    AppConfig::GetResolutionDefaults(screen_w, screen_h, dummy_mw, dummy_mh, dummy_th,
                                     dummy_fs, dummy_ts, win_w, win_h);

    int win_x = (screen_w - win_w) / 2;
    int win_y = (screen_h - win_h) / 2;

    // 3. Create Standalone Popup Tool Window (Clean borderless, no native white caption line)
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"设置",
        WS_POPUP,
        win_x, win_y, win_w, win_h,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!m_hwnd) return false;

    // Extend 0 frame to prevent DWM from rendering any top white line/seam
    MARGINS margins = { 0, 0, 0, 0 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    // 4. Create SwapChain
    IDXGIDevice* pDXGIDevice = nullptr;
    HRESULT hr = m_device->QueryInterface(__uuidof(IDXGIDevice), (void**)&pDXGIDevice);
    if (FAILED(hr)) return false;

    IDXGIAdapter* pDXGIAdapter = nullptr;
    hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
    pDXGIDevice->Release();
    if (FAILED(hr)) return false;

    IDXGIFactory* pIDXGIFactory = nullptr;
    hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&pIDXGIFactory);
    pDXGIAdapter->Release();
    if (FAILED(hr)) return false;

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = win_w;
    sd.BufferDesc.Height = win_h;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    hr = pIDXGIFactory->CreateSwapChain(m_device, &sd, &m_swapChain);
    pIDXGIFactory->Release();
    if (FAILED(hr)) return false;

    CreateRenderTarget();

    // 5. Setup Dedicated ImGui Context with Shared Font Atlas
    ImFontAtlas* shared_fonts = ImGui::GetIO().Fonts;
    m_imguiContext = ImGui::CreateContext(shared_fonts);
    ImGui::SetCurrentContext(m_imguiContext);

    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    UINT initial_dpi = GetWindowDPI(m_hwnd);
    style.FontScaleDpi = (float)initial_dpi / 96.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.0f, 0.5f, 0.8f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.6f, 0.9f, 0.65f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.28f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.6f, 0.9f, 0.85f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device, m_context);

    return true;
}

void SettingsWindow::Shutdown() {
    CleanupRenderTarget();
    if (m_swapChain) {
        m_swapChain->Release();
        m_swapChain = nullptr;
    }
    if (m_imguiContext) {
        ImGui::SetCurrentContext(m_imguiContext);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(m_imguiContext);
        m_imguiContext = nullptr;
    }
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hInstance) {
        UnregisterClassW(L"SC2SettingsWindowNative", m_hInstance);
        m_hInstance = nullptr;
    }
}

void SettingsWindow::CreateRenderTarget() {
    if (!m_swapChain || !m_device) return;
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (SUCCEEDED(hr) && pBackBuffer) {
        m_device->CreateRenderTargetView(pBackBuffer, nullptr, &m_renderTargetView);
        pBackBuffer->Release();
    }
}

void SettingsWindow::CleanupRenderTarget() {
    if (m_renderTargetView) {
        if (m_context) {
            ID3D11RenderTargetView* nullViews[] = { nullptr };
            m_context->OMSetRenderTargets(1, nullViews, nullptr);
        }
        m_renderTargetView->Release();
        m_renderTargetView = nullptr;
    }
}

void SettingsWindow::OnResize(UINT width, UINT height) {
    if (m_swapChain && width > 0 && height > 0) {
        CleanupRenderTarget();
        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        CreateRenderTarget();
    }
}

void SettingsWindow::Show(bool show) {
    m_visible = show;
    if (!m_visible) {
        CancelRecording();
    }
    if (m_hwnd) {
        if (m_visible) {
            RECT rc;
            if (GetWindowRect(m_hwnd, &rc)) {
                int screen_w = GetSystemMetrics(SM_CXSCREEN);
                int screen_h = GetSystemMetrics(SM_CYSCREEN);
                if (rc.right <= 0 || rc.left >= screen_w || rc.bottom <= 0 || rc.top >= screen_h) {
                    int win_w = rc.right - rc.left;
                    int win_h = rc.bottom - rc.top;
                    int win_x = (screen_w - win_w) / 2;
                    int win_y = (screen_h - win_h) / 2;
                    SetWindowPos(m_hwnd, HWND_TOPMOST, win_x, win_y, win_w, win_h, SWP_NOACTIVATE);
                }
            }
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
        } else {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }
}

void SettingsWindow::ApplyRecordedHotkey(unsigned int mod, unsigned int vk) {
    auto& cfg = AppConfig::Instance();
    std::string formatted = AppConfig::FormatHotkey(mod, vk);

    if (m_recording_action == 1) {
        cfg.map_hotkey_mod = mod;
        cfg.map_hotkey_vk = vk;
        cfg.map_shortcut = formatted;
    } else if (m_recording_action == 2) {
        cfg.lock_hotkey_mod = mod;
        cfg.lock_hotkey_vk = vk;
        cfg.lock_shortcut = formatted;
    } else if (m_recording_action == 3) {
        cfg.artifact_hotkey_mod = mod;
        cfg.artifact_hotkey_vk = vk;
        cfg.show_artifact_shortcut = formatted;
    }

    m_recording_action = 0;
    cfg.Save();
    HotkeyManager::Instance().ReRegister();
}

void SettingsWindow::Toggle() {
    Show(!m_visible);
}

void SettingsWindow::Render(float delta_time) {
    if (!m_visible || !m_hwnd || !m_swapChain || !m_renderTargetView) return;

    ImGui::SetCurrentContext(m_imguiContext);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderUI(delta_time);

    ImGui::Render();
    const float clear_color[4] = { 0.12f, 0.14f, 0.18f, 1.0f };
    m_context->OMSetRenderTargets(1, &m_renderTargetView, nullptr);
    m_context->ClearRenderTargetView(m_renderTargetView, clear_color);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    m_swapChain->Present(1, 0);

    // Perform deferred actions safely outside of the active ImGui frame
    if (m_need_update_dimensions) {
        m_need_update_dimensions = false;
        OverlayUI::Instance().UpdateWindowDimensions();
    }
    if (m_need_hide) {
        m_need_hide = false;
        Hide();
    }

    // Dynamically adapt window height to content if height differs
    if (m_measured_content_h > 300) {
        RECT rc;
        if (GetWindowRect(m_hwnd, &rc)) {
            int cur_w = rc.right - rc.left;
            int cur_h = rc.bottom - rc.top;
            if (std::abs(m_measured_content_h - cur_h) >= 4) {
                int cur_x = rc.left;
                int cur_y = rc.top;
                int screen_h = GetSystemMetrics(SM_CYSCREEN);
                if (screen_h > 0 && cur_y + m_measured_content_h > screen_h - 10) {
                    cur_y = std::max(10, screen_h - m_measured_content_h - 30);
                }
                SetWindowPos(m_hwnd, nullptr, cur_x, cur_y, cur_w, m_measured_content_h, SWP_NOZORDER | SWP_NOACTIVATE);
            }
        }
    }
}

void SettingsWindow::RenderUI(float delta_time) {
    auto& cfg = AppConfig::Instance();

    // Scale font size according to user configuration
    ImGui::GetStyle().FontScaleMain = (cfg.font_scale > 0.5f ? cfg.font_scale : 1.0f);

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

    if (m_save_notice_timer > 0.0f) {
        m_save_notice_timer -= delta_time;
    }

    if (ImGui::Begin("SettingsRootWindow", nullptr, flags)) {
        float avail_w = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        // Adaptive helper: keeps label from being clipped on large font sizes by measuring width
        // If single line has enough room (>= 100px for control), aligns control left and label right;
        // otherwise gracefully folds to two rows: full-width label on top, full-width control below.
        auto DrawItemWithLabel = [&](const char* label, auto&& draw_func) {
            float label_w = ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
            float min_ctrl_w = 100.0f;
            if (avail_w - label_w >= min_ctrl_w) {
                float ctrl_w = avail_w - label_w - spacing;
                ImGui::SetNextItemWidth(ctrl_w);
                draw_func(label);
            } else {
                ImGui::TextUnformatted(label);
                std::string hidden_id = std::string("##") + label;
                ImGui::SetNextItemWidth(avail_w);
                draw_func(hidden_id.c_str());
            }
        };

        // Drag window by clicking and dragging on the background or title bar
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0) && !ImGui::IsAnyItemActive()) {
            ReleaseCapture();
            SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Header / Title Bar (Whole row up to close button is draggable)
        float title_h = ImGui::GetFrameHeight() * 1.15f;
        float close_size = ImGui::GetFrameHeight();
        float avail_title_w = ImGui::GetContentRegionAvail().x;
        float drag_w = avail_title_w - close_size - ImGui::GetStyle().ItemSpacing.x;

        ImVec2 title_p0 = ImGui::GetCursorScreenPos();

        ImGui::InvisibleButton("##TitleBarDragArea", ImVec2(drag_w, title_h));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            ReleaseCapture();
            SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("按住可拖动设置窗口位置");
        }

        float text_y = title_p0.y + (title_h - ImGui::GetTextLineHeight()) * 0.5f;
        ImGui::GetWindowDrawList()->AddText(ImVec2(title_p0.x, text_y),
                                           IM_COL32(0, 217, 255, 255), "设置");

        ImGui::SameLine();
        if (ImGui::Button("X##CloseSettings", ImVec2(close_size, title_h))) {
            m_need_hide = true;
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 1. Window Dimensions
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 窗体尺寸设置 ]");

        bool size_changed = false;
        int min_mw = (int)std::max(140.0f, std::round(140.0f * (cfg.font_scale > 0.5f ? cfg.font_scale : 1.0f)));
        if (cfg.main_window_width < min_mw) {
            cfg.main_window_width = min_mw;
            size_changed = true;
        }

        DrawItemWithLabel("主窗宽度", [&](const char* id) {
            if (ImGui::SliderInt(id, &cfg.main_window_width, min_mw, 800, "%d px")) {
                size_changed = true;
            }
        });

        DrawItemWithLabel("主窗高度", [&](const char* id) {
            if (ImGui::SliderInt(id, &cfg.main_window_height, 180, 1400, "%d px")) {
                size_changed = true;
            }
        });

        DrawItemWithLabel("表格高度", [&](const char* id) {
            if (ImGui::SliderInt(id, &cfg.table_height, 90, 1000, "%d px")) {
                size_changed = true;
            }
        });

        if (size_changed) {
            m_need_update_dimensions = true;
        }

        ImGui::Spacing();

        // 2. Font Size (Expanded range: 0.8x to 2.8x, clean 0.1 step)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 文字字体缩放 ]");
        DrawItemWithLabel("字体大小", [&](const char* id) {
            if (ImGui::SliderFloat(id, &cfg.font_scale, 0.8f, 2.8f, "%.1f x")) {
                cfg.font_scale = std::round(cfg.font_scale * 10.0f) / 10.0f;
                m_need_update_dimensions = true;
            }
        });

        ImGui::Spacing();

        // 3. Toast Notifications
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Toast 弹窗通知设置 ]");
        ImGui::Checkbox("启用 Toast 预警通知", &cfg.toast_allowed);

        ImGui::Checkbox("主窗隐藏时仍提示波次", &cfg.toast_when_hidden);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("开启后，即使通过托盘隐藏了主悬浮窗，在游戏中到达波次时间仍会正常弹出屏幕 Toast 预警（默认关闭）");
        }

        DrawItemWithLabel("Toast垂直位置", [&](const char* id) {
            ImGui::SliderFloat(id, &cfg.toast_position_y, 0.05f, 0.95f, "%.2f (屏幕比例)");
        });

        DrawItemWithLabel("Toast尺寸缩放", [&](const char* id) {
            if (ImGui::SliderFloat(id, &cfg.toast_scale, 0.60f, 3.00f, "%.1f x")) {
                cfg.toast_scale = std::round(cfg.toast_scale * 10.0f) / 10.0f;
            }
        });

        DrawItemWithLabel("显示持续时间", [&](const char* id) {
            int duration_sec = cfg.toast_duration_ms / 1000;
            if (ImGui::SliderInt(id, &duration_sec, 2, 15, "%d 秒")) {
                cfg.toast_duration_ms = duration_sec * 1000;
            }
        });

        ImGui::Checkbox("显示 Toast 边框", &cfg.toast_show_border);
        DrawItemWithLabel("Toast透明度", [&](const char* id) {
            if (ImGui::SliderFloat(id, &cfg.toast_opacity, 0.1f, 1.0f, "%.2f")) {
                cfg.toast_opacity = std::round(cfg.toast_opacity * 100.0f) / 100.0f;
            }
        });

        if (cfg.toast_text_color.size() < 3) {
            cfg.toast_text_color = { 0.0f, 0.86f, 1.0f };
        }
        if (cfg.toast_border_color.size() < 3) {
            cfg.toast_border_color = { 0.0f, 0.75f, 1.0f };
        }

        DrawItemWithLabel("Toast文字颜色", [&](const char* id) {
            ImGui::ColorEdit3(id, cfg.toast_text_color.data());
        });

        if (!cfg.toast_show_border) ImGui::BeginDisabled();
        DrawItemWithLabel("Toast边框颜色", [&](const char* id) {
            ImGui::ColorEdit3(id, cfg.toast_border_color.data());
        });
        if (!cfg.toast_show_border) ImGui::EndDisabled();

        ImGui::Spacing();
        float toast_btn_h = std::max(28.0f * cfg.font_scale, ImGui::GetFrameHeight() * 1.15f);
        if (ImGui::Button("测试 Toast 效果", ImVec2(avail_w, toast_btn_h))) {
            ToastWindow::Instance().ShowTestToast();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 4. Hotkeys Configuration
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 全局快捷键设置 ]");

        auto DrawHotkeyRow = [&](const char* label, int action_id, const std::string& shortcut_str) {
            bool recording = (m_recording_action == action_id);
            std::string btn_label;
            if (recording) {
                btn_label = "按下组合键... (Esc取消)##" + std::string(label);
            } else {
                btn_label = (shortcut_str.empty() ? "未设置" : shortcut_str) + "##" + std::string(label);
            }

            // Left: Feature label
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "%s", label);

            // Right: Interactive button
            ImGui::SameLine();
            float btn_w = std::max(170.0f, avail_w * 0.48f);
            float target_x = ImGui::GetWindowWidth() - btn_w - ImGui::GetStyle().WindowPadding.x;
            if (target_x > ImGui::GetCursorPosX()) {
                ImGui::SetCursorPosX(target_x);
            }

            if (recording) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.25f, 0.25f, 0.95f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.35f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.22f, 0.28f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.6f, 0.9f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 1.0f, 1.0f));
            }

            float btn_h = std::max(24.0f * cfg.font_scale, ImGui::GetFrameHeight());
            if (ImGui::Button(btn_label.c_str(), ImVec2(btn_w, btn_h))) {
                if (recording) {
                    CancelRecording();
                } else {
                    StartRecording(action_id);
                }
            }
            ImGui::PopStyleColor(3);
        };

        DrawHotkeyRow("切换地图路线", 1, cfg.map_shortcut);
        DrawHotkeyRow("锁定/解锁窗口", 2, cfg.lock_shortcut);
        DrawHotkeyRow("切换神器标注", 3, cfg.show_artifact_shortcut);

        float rst_hotkey_h = std::max(24.0f * cfg.font_scale, ImGui::GetFrameHeight());
        if (ImGui::Button("恢复默认快捷键", ImVec2(avail_w, rst_hotkey_h))) {
            CancelRecording();
            cfg.map_hotkey_mod = 0x0002 | 0x0004;
            cfg.map_hotkey_vk = 0xDB;
            cfg.map_shortcut = AppConfig::FormatHotkey(cfg.map_hotkey_mod, cfg.map_hotkey_vk);

            cfg.lock_hotkey_mod = 0x0002 | 0x0004;
            cfg.lock_hotkey_vk = 0xDD;
            cfg.lock_shortcut = AppConfig::FormatHotkey(cfg.lock_hotkey_mod, cfg.lock_hotkey_vk);

            cfg.artifact_hotkey_mod = 0x0002 | 0x0004;
            cfg.artifact_hotkey_vk = 0xDC;
            cfg.show_artifact_shortcut = AppConfig::FormatHotkey(cfg.artifact_hotkey_mod, cfg.artifact_hotkey_vk);

            cfg.Save();
            HotkeyManager::Instance().ReRegister();
        }

        if (HotkeyManager::Instance().HasFailedHotkeys()) {
            ImGui::Spacing();
            std::string warn = "注意：以下快捷键冲突（被其他程序占用）：";
            for (const auto& name : HotkeyManager::Instance().GetFailedHotkeys()) {
                warn += "\n  - " + name;
            }
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", warn.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 5. Action Buttons (Adaptive layout for any font scale: 3 columns, 2 columns, or 1 column)
        float act_btn_h = std::max(30.0f, ImGui::GetFrameHeight() * 1.15f);
        float min_btn_w = ImGui::CalcTextSize("恢复默认").x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;

        auto SaveAction = [&]() {
            cfg.Save();
            HotkeyManager::Instance().ReRegister();
            m_save_notice_timer = 3.0f;
        };
        auto ResetAction = [&]() {
            int sw = GetSystemMetrics(SM_CXSCREEN);
            int sh = GetSystemMetrics(SM_CYSCREEN);
            cfg.ResetToResolutionDefaults(sw, sh);
            cfg.Save();
            HotkeyManager::Instance().ReRegister();
            m_need_update_dimensions = true;
            m_save_notice_timer = 3.0f;
        };
        auto CloseAction = [&]() {
            m_need_hide = true;
        };

        if (min_btn_w * 3.0f + spacing * 2.0f <= avail_w) {
            float w3 = std::floor((avail_w - 2.0f * spacing) / 3.0f);
            if (ImGui::Button("保存设置", ImVec2(w3, act_btn_h))) SaveAction();
            ImGui::SameLine();
            if (ImGui::Button("恢复默认", ImVec2(w3, act_btn_h))) ResetAction();
            ImGui::SameLine();
            if (ImGui::Button("关闭", ImVec2(avail_w - w3 * 2.0f - spacing * 2.0f, act_btn_h))) CloseAction();
        } else if (min_btn_w * 2.0f + spacing <= avail_w) {
            float w2 = std::floor((avail_w - spacing) / 2.0f);
            if (ImGui::Button("保存设置", ImVec2(w2, act_btn_h))) SaveAction();
            ImGui::SameLine();
            if (ImGui::Button("恢复默认", ImVec2(avail_w - w2 - spacing, act_btn_h))) ResetAction();
            ImGui::Spacing();
            if (ImGui::Button("关闭", ImVec2(avail_w, act_btn_h))) CloseAction();
        } else {
            if (ImGui::Button("保存设置", ImVec2(avail_w, act_btn_h))) SaveAction();
            ImGui::Spacing();
            if (ImGui::Button("恢复默认", ImVec2(avail_w, act_btn_h))) ResetAction();
            ImGui::Spacing();
            if (ImGui::Button("关闭", ImVec2(avail_w, act_btn_h))) CloseAction();
        }

        if (m_save_notice_timer > 0.0f) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "设置已成功保存并生效！");
        }

        // Measure content bottom to adapt window height
        float measured_h = ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y + 4.0f;
        if (measured_h > 300.0f) {
            m_measured_content_h = (int)std::ceil(measured_h);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar(4);
}
