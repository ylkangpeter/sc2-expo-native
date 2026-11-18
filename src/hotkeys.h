#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

enum class HotkeyAction {
    MapSwitch,
    LockToggle,
    ArtifactToggle
};

class HotkeyManager {
public:
    static HotkeyManager& Instance();

    bool RegisterAll(HWND hwnd);
    void UnregisterAll(HWND hwnd = nullptr);
    bool ReRegister();
    HWND GetHwnd() const { return m_hwnd; }
    void OnHotkey(int id);

    void SetCallback(HotkeyAction action, std::function<void()> cb);

    const std::vector<std::string>& GetFailedHotkeys() const { return m_failed_hotkeys; }
    bool HasFailedHotkeys() const { return !m_failed_hotkeys.empty(); }

private:
    HotkeyManager() = default;
    HWND m_hwnd = nullptr;
    std::unordered_map<int, HotkeyAction> m_action_map;
    std::unordered_map<HotkeyAction, std::function<void()>> m_callbacks;
    std::vector<std::string> m_failed_hotkeys;
};
