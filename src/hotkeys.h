#pragma once

#include <windows.h>
#include <functional>
#include <unordered_map>

enum class HotkeyAction {
    MapSwitch,
    LockToggle,
    ArtifactToggle,
    Screenshot
};

class HotkeyManager {
public:
    static HotkeyManager& Instance();

    bool RegisterAll(HWND hwnd);
    void UnregisterAll(HWND hwnd);
    void OnHotkey(int id);

    void SetCallback(HotkeyAction action, std::function<void()> cb);

private:
    HotkeyManager() = default;
    std::unordered_map<int, HotkeyAction> m_action_map;
    std::unordered_map<HotkeyAction, std::function<void()>> m_callbacks;
};
