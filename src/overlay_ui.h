#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include "schedule_manager.h"
#include "mutator_manager.h"

struct ToastNotification {
    bool active = false;
    std::string time_text;
    std::string event_text;
    std::string tier_text;
    std::vector<std::string> regular_unit_icons;
    std::vector<std::pair<std::string, int>> hybrid_icons;
    float expire_time = 0.0f;
};

struct Sc2GameState;

class OverlayUI {
public:
    static OverlayUI& Instance();

    void Init(ID3D11Device* device, HWND hwnd);
    void Render(float delta_time);
    void UpdateBackground(float delta_time);
    void CheckWaveToastAlert(const Sc2GameState& state);

    bool IsLocked() const { return m_locked; }
    void SetLocked(bool locked);
    void ToggleLock();

    void ToggleArtifact();
    void ToggleMapVariant();

    void TriggerToast(const ScheduleRow& row, float duration = 10.0f);

    bool IsCommanderSelectorOpen() const { return m_show_commander_selector; }
    void ToggleCommanderSelector();
    void SetCommanderSelectorOpen(bool open);

    bool IsSettingsOpen() const;
    void ToggleSettings();
    void SetSettingsOpen(bool open);

    void UpdateWindowDimensions();

    std::string GetCurrentMap() const { return m_current_map; }

private:
    OverlayUI() = default;

    void RenderMainHUD(float delta_time);
    void RenderAlertBanner(float delta_time);
    void RenderCommanderSelector(float delta_time);

    void UpdateClickThroughState();

    bool m_locked = true; // default LOCKED (click-through) on startup
    bool m_show_commander_selector = false;
    std::string m_current_map;
    std::string m_current_region = "KR";
    std::string m_current_troop = "classic_infantry-旧世步兵团";

    int m_last_active_row = -1;
    int m_last_toast_seconds = -1;
    int m_last_game_seconds = -1;
    std::string m_last_toast_map;

    ToastNotification m_toast;

    // Ephemeral notification (e.g. "已复制指挥官替换链接!")
    std::string m_notice_text;
    float m_notice_expire = 0.0f;

    HWND m_hwnd = nullptr;
    ID3D11Device* m_device = nullptr;
};
