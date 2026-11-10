#include "hotkeys.h"
#include <iostream>

#define HOTKEY_MAP_ID          1001
#define HOTKEY_LOCK_ID         1002
#define HOTKEY_ARTIFACT_ID     1003
#define HOTKEY_SCREENSHOT_ID   1004

#define HOTKEY_MAP_ALT_ID      1011
#define HOTKEY_LOCK_ALT_ID     1012
#define HOTKEY_ARTIFACT_ALT_ID 1013

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

HotkeyManager& HotkeyManager::Instance() {
    static HotkeyManager instance;
    return instance;
}

static bool TryRegister(HWND hwnd, int id, UINT mod, UINT vk) {
    if (RegisterHotKey(hwnd, id, mod | MOD_NOREPEAT, vk)) {
        return true;
    }
    return RegisterHotKey(hwnd, id, mod, vk);
}

bool HotkeyManager::RegisterAll(HWND hwnd) {
    UnregisterAll(hwnd);

    // Map Variant Switch:
    // Primary: Ctrl + Shift + [
    if (TryRegister(hwnd, HOTKEY_MAP_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_4)) {
        m_action_map[HOTKEY_MAP_ID] = HotkeyAction::MapSwitch;
    }
    // Alternative / Direct: Ctrl + [ (bypasses Windows IME Ctrl+Shift input switcher)
    if (TryRegister(hwnd, HOTKEY_MAP_ALT_ID, MOD_CONTROL, VK_OEM_4)) {
        m_action_map[HOTKEY_MAP_ALT_ID] = HotkeyAction::MapSwitch;
    }

    // Lock Toggle:
    // Primary: Ctrl + Shift + ]
    if (TryRegister(hwnd, HOTKEY_LOCK_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_6)) {
        m_action_map[HOTKEY_LOCK_ID] = HotkeyAction::LockToggle;
    }
    // Alternative / Direct: Ctrl + ]
    if (TryRegister(hwnd, HOTKEY_LOCK_ALT_ID, MOD_CONTROL, VK_OEM_6)) {
        m_action_map[HOTKEY_LOCK_ALT_ID] = HotkeyAction::LockToggle;
    }

    // Artifact Toggle:
    // Primary: Ctrl + Shift + \ (VK_OEM_5)
    if (TryRegister(hwnd, HOTKEY_ARTIFACT_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_5)) {
        m_action_map[HOTKEY_ARTIFACT_ID] = HotkeyAction::ArtifactToggle;
    }
    // Alternative / Direct: Ctrl + \ (VK_OEM_5)
    if (TryRegister(hwnd, HOTKEY_ARTIFACT_ALT_ID, MOD_CONTROL, VK_OEM_5)) {
        m_action_map[HOTKEY_ARTIFACT_ALT_ID] = HotkeyAction::ArtifactToggle;
    }

    // Screenshot:
    // Primary: Ctrl + Shift + - (VK_OEM_MINUS)
    if (TryRegister(hwnd, HOTKEY_SCREENSHOT_ID, MOD_CONTROL | MOD_SHIFT, VK_OEM_MINUS)) {
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
