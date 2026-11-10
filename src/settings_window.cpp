#include "settings_window.h"
#include "config.h"
#include "overlay_ui.h"
#include "toast_window.h"
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <dwmapi.h>
#include <algorithm>
#include <iostream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ImGuiContext* prev = ImGui::GetCurrentContext();
    if (SettingsWindow::Instance().GetImGuiContext()) {
        ImGui::SetCurrentContext(SettingsWindow::Instance().GetImGuiContext());
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
            if (prev) ImGui::SetCurrentContext(prev);
            return true;
        }
    }
    if (prev) ImGui::SetCurrentContext(prev);

    switch (msg) {
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

    // 2. Position Centered on Screen (Width: 380, Height: 600)
    int win_w = 380;
    int win_h = 600;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int win_x = (screen_w - win_w) / 2;
    int win_y = (screen_h - win_h) / 2;

    // 3. Create Standalone Popup Tool Window (Independent from Main HUD)
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"设置",
        WS_POPUP,
        win_x, win_y, win_w, win_h,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!m_hwnd) return false;

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
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.14f, 0.18f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.0f, 0.6f, 0.9f, 0.8f);
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
    if (m_hwnd) {
        if (m_visible) {
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
        } else {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }
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
}

void SettingsWindow::RenderUI(float delta_time) {
    auto& cfg = AppConfig::Instance();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

    if (m_save_notice_timer > 0.0f) {
        m_save_notice_timer -= delta_time;
    }

    if (ImGui::Begin("SettingsRootWindow", nullptr, flags)) {
        // Drag window by clicking and dragging on the background or title
        if (ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0) && !ImGui::IsAnyItemActive()) {
            ReleaseCapture();
            SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Header
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "设置");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("按住可拖动设置窗口位置");
            if (ImGui::IsMouseDragging(0)) {
                ReleaseCapture();
                SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 42);
        if (ImGui::Button("X##CloseSettings", ImVec2(28, 22))) {
            Hide();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 1. Window Dimensions
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 窗体尺寸设置 ]");

        bool size_changed = false;
        ImGui::SetNextItemWidth(210);
        if (ImGui::SliderInt("主窗宽度", &cfg.main_window_width, 140, 320, "%d px")) {
            size_changed = true;
        }

        ImGui::SetNextItemWidth(210);
        if (ImGui::SliderInt("主窗高度", &cfg.main_window_height, 180, 500, "%d px")) {
            size_changed = true;
        }

        ImGui::SetNextItemWidth(210);
        if (ImGui::SliderInt("表格高度", &cfg.table_height, 90, 320, "%d px")) {
            size_changed = true;
        }

        if (size_changed) {
            OverlayUI::Instance().UpdateWindowDimensions();
        }

        ImGui::Spacing();

        // 2. Font Size
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 文字字体缩放 ]");
        ImGui::SetNextItemWidth(210);
        ImGui::SliderFloat("字体大小", &cfg.font_scale, 0.75f, 1.50f, "%.2f x");

        ImGui::Spacing();

        // 3. Toast Notifications
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Toast 弹窗通知设置 ]");
        ImGui::Checkbox("启用 Toast 预警通知", &cfg.toast_allowed);

        ImGui::SetNextItemWidth(210);
        ImGui::SliderFloat("Toast垂直位置", &cfg.toast_position_y, 0.05f, 0.95f, "%.2f (屏幕比例)");

        ImGui::SetNextItemWidth(210);
        ImGui::SliderFloat("Toast尺寸缩放", &cfg.toast_scale, 0.60f, 2.00f, "%.2f x");

        ImGui::SetNextItemWidth(210);
        int duration_sec = cfg.toast_duration_ms / 1000;
        if (ImGui::SliderInt("显示持续时间", &duration_sec, 2, 15, "%d 秒")) {
            cfg.toast_duration_ms = duration_sec * 1000;
        }

        if (cfg.toast_text_color.size() < 3) {
            cfg.toast_text_color = { 0.0f, 0.86f, 1.0f };
        }
        if (cfg.toast_border_color.size() < 3) {
            cfg.toast_border_color = { 0.0f, 0.75f, 1.0f };
        }

        ImGui::SetNextItemWidth(210);
        ImGui::ColorEdit3("Toast文字颜色", cfg.toast_text_color.data());

        ImGui::SetNextItemWidth(210);
        ImGui::ColorEdit3("Toast边框颜色", cfg.toast_border_color.data());

        ImGui::Spacing();
        if (ImGui::Button("测试 Toast 效果", ImVec2(350, 28))) {
            ToastWindow::Instance().ShowTestToast();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 4. Action Buttons
        if (ImGui::Button("保存设置", ImVec2(105, 30))) {
            cfg.Save();
            m_save_notice_timer = 3.0f;
        }

        ImGui::SameLine();
        if (ImGui::Button("恢复默认", ImVec2(105, 30))) {
            cfg.main_window_width = 173;
            cfg.main_window_height = 340;
            cfg.table_height = 228;
            cfg.font_scale = 1.0f;
            cfg.toast_allowed = true;
            cfg.toast_position_y = 0.18f;
            cfg.toast_scale = 1.43f;
            cfg.toast_duration_ms = 8000;
            cfg.toast_text_color = { 0.0f, 0.86f, 1.0f };
            cfg.toast_border_color = { 0.0f, 0.75f, 1.0f };
            cfg.Save();
            OverlayUI::Instance().UpdateWindowDimensions();
            m_save_notice_timer = 3.0f;
        }

        ImGui::SameLine();
        if (ImGui::Button("关闭", ImVec2(80, 30))) {
            Hide();
        }

        if (m_save_notice_timer > 0.0f) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "设置已成功保存并生效！");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
}
