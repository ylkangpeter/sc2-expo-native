#include "hotkeys.h"
#include <iostream>

#define HOTKEY_MAP_ID       1001
#define HOTKEY_LOCK_ID      1002
#define HOTKEY_ARTIFACT_ID  1003
#define HOTKEY_SCREENSHOT_ID 1004

HotkeyManager& HotkeyManager::Instance() {
    static HotkeyManager instance;
    return instance;
}

bool HotkeyManager::RegisterAll(HWND hwnd) {
    UnregisterAll(hwnd);

    // Ctrl + Shift + [
    if (RegisterHotKey(hwnd, HOTKEY_MAP_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_4)) {
        m_action_map[HOTKEY_MAP_ID] = HotkeyAction::MapSwitch;
    }

    // Ctrl + Shift + ]
    if (RegisterHotKey(hwnd, HOTKEY_LOCK_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_6)) {
        m_action_map[HOTKEY_LOCK_ID] = HotkeyAction::LockToggle;
    }

    // Ctrl + Shift + \ (VK_OEM_5)
    if (RegisterHotKey(hwnd, HOTKEY_ARTIFACT_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_5)) {
        m_action_map[HOTKEY_ARTIFACT_ID] = HotkeyAction::ArtifactToggle;
    }

    // Ctrl + Shift + - (VK_OEM_MINUS)
    if (RegisterHotKey(hwnd, HOTKEY_SCREENSHOT_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_MINUS)) {
        m_action_map[HOTKEY_SCREENSHOT_ID] = HotkeyAction::Screenshot;
    }

    return true;
}

void HotkeyManager::UnregisterAll(HWND hwnd) {
    for (const auto& kv : m_action_map) {
        UnregisterHotKey(hwnd, kv.first);
    }
    m_action_map.clear();
}

void HotkeyManager::OnHotkey(int id) {
    auto it = m_action_map.find(id);
    if (it != m_action_map.end()) {
        auto cb_it = m_callbacks.find(it->second);
        if (cb_it != m_callbacks.end() && cb_it->second) {
            cb_it->second();
        }
    }
}

void HotkeyManager::SetCallback(HotkeyAction action, std::function<void()> cb) {
    m_callbacks[action] = cb;
}
