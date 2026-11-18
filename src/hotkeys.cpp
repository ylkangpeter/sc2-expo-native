#include "hotkeys.h"
#include "config.h"
#include <iostream>

#define HOTKEY_MAP_ID          1001
#define HOTKEY_LOCK_ID         1002
#define HOTKEY_ARTIFACT_ID     1003

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
    if (hwnd) m_hwnd = hwnd;
    HWND target = m_hwnd;

    UnregisterAll(target);
    m_failed_hotkeys.clear();

    bool all_success = true;

    auto do_register = [&](int id, UINT mod, UINT vk, HotkeyAction action, const std::string& name) {
        if (TryRegister(target, id, mod, vk)) {
            m_action_map[id] = action;
        } else {
            all_success = false;
            m_failed_hotkeys.push_back(name);
            std::cerr << "Warning: Failed to register global hotkey '" << name
                      << "' (Error: " << GetLastError() << "). It may be occupied by another application." << std::endl;
        }
    };

    const auto& cfg = AppConfig::Instance();

    // 1. Map Switch
    if (cfg.map_hotkey_vk != 0) {
        do_register(HOTKEY_MAP_ID, cfg.map_hotkey_mod, cfg.map_hotkey_vk, HotkeyAction::MapSwitch, cfg.map_shortcut);
    }

    // 2. Lock Toggle
    if (cfg.lock_hotkey_vk != 0) {
        do_register(HOTKEY_LOCK_ID, cfg.lock_hotkey_mod, cfg.lock_hotkey_vk, HotkeyAction::LockToggle, cfg.lock_shortcut);
    }

    // 3. Artifact Toggle
    if (cfg.artifact_hotkey_vk != 0) {
        do_register(HOTKEY_ARTIFACT_ID, cfg.artifact_hotkey_mod, cfg.artifact_hotkey_vk, HotkeyAction::ArtifactToggle, cfg.show_artifact_shortcut);
    }

    return all_success;
}

void HotkeyManager::UnregisterAll(HWND hwnd) {
    HWND target = hwnd ? hwnd : m_hwnd;
    if (target) {
        for (const auto& kv : m_action_map) {
            UnregisterHotKey(target, kv.first);
        }
    }
    m_action_map.clear();
}

bool HotkeyManager::ReRegister() {
    if (!m_hwnd) return false;
    return RegisterAll(m_hwnd);
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
