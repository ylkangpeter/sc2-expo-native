#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include "schedule_manager.h"

class ToastWindow {
public:
    static ToastWindow& Instance();

    void Init(HINSTANCE hInstance);
    void Shutdown();

    void ShowToast(const ScheduleRow& row, const std::string& current_troop, float duration = 8.0f);
    void ShowTestToast();
    void Hide();
    void HideGameToast();
    void Update(float delta_time);

    bool IsActive() const { return m_active; }

private:
    ToastWindow() = default;
    ~ToastWindow() { Shutdown(); }

    void RenderToast();
    void ShowToast(const ScheduleRow& row, const std::string& current_troop, float duration, bool is_preview);

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;
    bool m_active = false;
    bool m_is_preview = false;
    float m_remaining_time = 0.0f;

    std::string m_time_text;
    std::string m_event_text;
    std::string m_tier_text;
    std::vector<std::string> m_unit_icons;
    std::vector<std::pair<std::string, int>> m_hybrid_icons;
};
