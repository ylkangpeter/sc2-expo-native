#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>

struct ImGuiContext;

class SettingsWindow {
public:
    static SettingsWindow& Instance();

    bool Init(HINSTANCE hInstance, ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();

    void Show(bool show = true);
    void Hide() { Show(false); }
    void Toggle();
    bool IsVisible() const { return m_visible; }

    void Render(float delta_time);
    void OnResize(UINT width, UINT height);

    HWND GetHWND() const { return m_hwnd; }
    ImGuiContext* GetImGuiContext() const { return m_imguiContext; }

private:
    SettingsWindow() = default;
    ~SettingsWindow() { Shutdown(); }

    void RenderUI(float delta_time);
    void CreateRenderTarget();
    void CleanupRenderTarget();

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;

    ImGuiContext* m_imguiContext = nullptr;
    bool m_visible = false;
    float m_save_notice_timer = 0.0f;
};
