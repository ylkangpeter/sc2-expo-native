#include <windows.h>
#include <windowsx.h>
#include <d3d11.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <tchar.h>
#include <iostream>
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "config.h"
#include "sc2_client.h"
#include "hotkeys.h"
#include "overlay_ui.h"
#include "artifact_viewer.h"
#include "lock_window.h"
#include "toast_window.h"
#include "mutator_alert_window.h"
#include "settings_window.h"

#define WM_TRAYICON (WM_USER + 101)
#define ID_TRAY_EXIT 2001
#define ID_TRAY_LOCK 2002
#define ID_TRAY_MAP 2003
#define ID_TRAY_ARTIFACT 2004
#define ID_TRAY_COMMANDER 2005
#define ID_TRAY_SETTINGS 2006

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static ID3D11BlendState*        g_pBlendState = nullptr;
static NOTIFYICONDATAW          g_nid = {};
static ImGuiContext*            g_mainImGuiContext = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
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

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ImGuiContextScope scope(g_mainImGuiContext);

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCHITTEST:
        return DefWindowProcW(hWnd, msg, wParam, lParam);

    case WM_MOVE:
    case WM_EXITSIZEMOVE: {
        RECT rect;
        if (GetWindowRect(hWnd, &rect)) {
            auto& cfg = AppConfig::Instance();
            cfg.main_window_x = rect.left;
            cfg.main_window_y = rect.top;
            cfg.Save();
            LockWindow::Instance().UpdatePosition(rect.left, rect.top);
        }
        return 0;
    }

    case WM_HOTKEY:
        HotkeyManager::Instance().OnHotkey((int)wParam);
        return 0;

    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED && LOWORD(lParam) > 0 && HIWORD(lParam) > 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            bool is_locked = OverlayUI::Instance().IsLocked();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_LOCK, is_locked ? L"解锁 (Ctrl+Shift+] / Ctrl+])" : L"锁定 (Ctrl+Shift+] / Ctrl+])");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_MAP, L"切换地图变种 (Ctrl+Shift+[ / Ctrl+[)");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_ARTIFACT, L"切换神器点位 (Ctrl+Shift+\\ / Ctrl+\\)");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_COMMANDER, L"指挥官替换器");
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_SETTINGS, L"设置");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"退出 (Exit)");

            SetForegroundWindow(hWnd);
            UINT cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, nullptr);
            PostMessageW(hWnd, WM_NULL, 0, 0);
            DestroyMenu(hMenu);

            if (cmd != 0) {
                SendMessageW(hWnd, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
            }
        } else if (lParam == WM_LBUTTONUP) {
            // User requested: 点击tray隐藏/显示
            bool is_vis = IsWindowVisible(hWnd);
            if (is_vis) {
                ShowWindow(hWnd, SW_HIDE);
                LockWindow::Instance().Show(false);
            } else {
                ShowWindow(hWnd, SW_SHOW);
                LockWindow::Instance().Show(true);
                SetForegroundWindow(hWnd);
            }
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_LOCK:
            OverlayUI::Instance().ToggleLock();
            break;
        case ID_TRAY_MAP:
            OverlayUI::Instance().ToggleMapVariant();
            break;
        case ID_TRAY_ARTIFACT:
            OverlayUI::Instance().ToggleArtifact();
            break;
        case ID_TRAY_COMMANDER:
            OverlayUI::Instance().ToggleCommanderSelector();
            break;
        case ID_TRAY_SETTINGS:
            SettingsWindow::Instance().Toggle();
            break;
        case ID_TRAY_EXIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    // 0. Ensure working directory is the folder containing sc2_expo.exe
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
        wchar_t* lastSlash = wcsrchr(exePath, L'\\');
        if (lastSlash) {
            *lastSlash = L'\0';
            SetCurrentDirectoryW(exePath);
        }
    }

    // 1. Initialize COM for WIC & Shell
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    // 2. DPI Awareness
    SetProcessDPIAware();

    // 3. Load Config
    auto& cfg = AppConfig::Instance();

    // 3. Screen Dimensions & Window Bounds
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    int win_w = cfg.main_window_width > 120 ? cfg.main_window_width : 173;
    int win_h = cfg.main_window_height > 120 ? cfg.main_window_height : 340;
    int win_x = cfg.main_window_x;
    int win_y = cfg.main_window_y;

    if (win_x < 0 || win_x > screen_w - 50) win_x = 20;
    if (win_y < 0 || win_y > screen_h - 50) win_y = 200;

    // 4. Register Window Class
    HICON hAppIcon = LoadIconW(hInstance, L"IDI_ICON1");
    if (!hAppIcon) hAppIcon = LoadIconA(nullptr, IDI_APPLICATION);

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        hInstance,
        hAppIcon,
        LoadCursor(nullptr, IDC_ARROW),
        nullptr,
        nullptr,
        L"SC2ExpoNativeOverlay",
        hAppIcon
    };
    RegisterClassExW(&wc);

    // 5. Create Compact Tool Window (Default locked click-through with WS_EX_TRANSPARENT)
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        wc.lpszClassName,
        L"SC2 Expo Native",
        WS_POPUP | WS_VISIBLE,
        win_x, win_y, win_w, win_h,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hwnd) return 1;

    // Set full layered alpha so the window is visible
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // Enable DWM transparent blur/composition
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 6. Initialize Direct3D 11
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // 7. Initialize Subsystems (LockWindow is always clickable and floats with main window)
    LockWindow::Instance().Init(hInstance, hwnd);
    LockWindow::Instance().UpdatePosition(win_x, win_y);
    LockWindow::Instance().SetLocked(true);

    ArtifactViewer::Instance().Init(hInstance);
    ToastWindow::Instance().Init(hInstance);
    MutatorAlertWindow::Instance().Init(hInstance);
    HotkeyManager::Instance().RegisterAll(hwnd);
    OverlayUI::Instance().Init(g_pd3dDevice, hwnd);
    Sc2Client::Instance().Start();

    // 8. Setup System Tray Icon
    ZeroMemory(&g_nid, sizeof(g_nid));
    g_nid.cbSize = sizeof(NOTIFYICONDATAW);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = hAppIcon;
    wcscpy_s(g_nid.szTip, L"SC2 Expo Timer (Native)");
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    // 9. Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    g_mainImGuiContext = ImGui::GetCurrentContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // Theme styling
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.15f, 0.90f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.0f, 0.5f, 0.8f, 0.45f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.6f, 0.9f, 0.65f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.28f, 0.85f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.0f, 0.6f, 0.9f, 0.85f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.0f, 0.7f, 1.0f, 1.0f);

    // Load Chinese Font
    const char* font_paths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\msyhbd.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc"
    };
    bool font_loaded = false;
    for (const char* fp : font_paths) {
        if (GetFileAttributesA(fp) != INVALID_FILE_ATTRIBUTES) {
            ImFontConfig font_cfg;
            font_cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromFileTTF(fp, 14.5f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
            font_loaded = true;
            break;
        }
    }
    if (!font_loaded) {
        io.Fonts->AddFontDefault();
    }

    // Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Initialize standalone Settings Window (centered, draggable, independent DX11 context)
    SettingsWindow::Instance().Init(hInstance, g_pd3dDevice, g_pd3dDeviceContext);
    ImGui::SetCurrentContext(g_mainImGuiContext);

    // Main Loop
    bool done = false;
    LARGE_INTEGER freq, last_time;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last_time);

    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done) break;

        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        float delta_time = (float)(current_time.QuadPart - last_time.QuadPart) / (float)freq.QuadPart;
        last_time = current_time;
        if (delta_time <= 0.0f) delta_time = 0.016f;

        bool main_vis = IsWindowVisible(hwnd);
        bool settings_vis = SettingsWindow::Instance().IsVisible();

        // 1. Render Main HUD if main window is visible
        if (main_vis && g_mainRenderTargetView) {
            ImGui::SetCurrentContext(g_mainImGuiContext);
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Render Overlay
            OverlayUI::Instance().Render(delta_time);

            // Rendering
            ImGui::Render();
            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);

            // Alpha blend state
            if (g_pBlendState) {
                float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
                g_pd3dDeviceContext->OMSetBlendState(g_pBlendState, blend_factor, 0xffffffff);
            }

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // Present with VSync
            g_pSwapChain->Present(1, 0);
        } else if (!main_vis) {
            // Keep game state / timer / mutators updated even when overlay is hidden
            Sc2GameState state = Sc2Client::Instance().GetState();
            MutatorManager::Instance().Update(state.current_seconds, delta_time);
            ToastWindow::Instance().Update(delta_time);
        }

        MutatorAlertWindow::Instance().Update(delta_time);

        // 2. Render standalone Settings Window if visible
        if (settings_vis) {
            SettingsWindow::Instance().Render(delta_time);
            ImGui::SetCurrentContext(g_mainImGuiContext);
        }

        // 3. Prevent high CPU usage if both windows are hidden
        if (!main_vis && !settings_vis) {
            Sleep(16);
        }
    }

    // Cleanup
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    Sc2Client::Instance().Stop();
    HotkeyManager::Instance().UnregisterAll(hwnd);
    ArtifactViewer::Instance().Shutdown();
    ToastWindow::Instance().Shutdown();
    MutatorAlertWindow::Instance().Shutdown();
    LockWindow::Instance().Shutdown();
    SettingsWindow::Instance().Shutdown();

    if (g_mainImGuiContext) {
        ImGui::SetCurrentContext(g_mainImGuiContext);
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(g_mainImGuiContext);
        g_mainImGuiContext = nullptr;
    }

    if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }
    CleanupRenderTarget();
    CleanupDeviceD3D();

    if (hwnd && IsWindow(hwnd)) {
        DestroyWindow(hwnd);
    }
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();

    return 0;
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // DWM transparent standard
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                               createDeviceFlags, featureLevelArray, 2,
                                               D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                               &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                           createDeviceFlags, featureLevelArray, 2,
                                           D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                           &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    if (res != S_OK) return false;

    CreateRenderTarget();

    // Alpha blending state
    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    g_pd3dDevice->CreateBlendState(&blendDesc, &g_pBlendState);
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    if (!g_pSwapChain || !g_pd3dDevice) return;
    ID3D11Texture2D* pBackBuffer = nullptr;
    HRESULT hr = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (SUCCEEDED(hr) && pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        if (g_pd3dDeviceContext) {
            ID3D11RenderTargetView* nullViews[] = { nullptr };
            g_pd3dDeviceContext->OMSetRenderTargets(1, nullViews, nullptr);
        }
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}
