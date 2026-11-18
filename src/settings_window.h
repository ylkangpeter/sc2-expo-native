#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>

struct ImGuiContext;
struct AppConfig;

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

    bool IsRecording() const { return m_recording_action != 0; }
    void CancelRecording();
    void ApplyRecordedHotkey(unsigned int mod, unsigned int vk);

private:
    SettingsWindow() = default;
    ~SettingsWindow() { Shutdown(); }

    void RenderUI(float delta_time);
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void StartRecording(int action);
    bool SaveHotkeyChanges(const AppConfig& candidate);
    void ShowSaveNotice(const char* message, bool success);

    HINSTANCE m_hInstance = nullptr;
    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_renderTargetView = nullptr;

    ImGuiContext* m_imguiContext = nullptr;
    bool m_visible = false;
    bool m_need_hide = false;
    bool m_need_update_dimensions = false;
    float m_save_notice_timer = 0.0f;
    const char* m_save_notice = "";
    bool m_save_succeeded = false;
    int m_recording_action = 0; // 0 = none, 1 = map, 2 = lock, 3 = artifact
    int m_measured_content_h = 0;
};
