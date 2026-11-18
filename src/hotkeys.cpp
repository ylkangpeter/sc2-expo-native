#include "hotkeys.h"
#include "config.h"
#include <algorithm>
#include <iostream>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace {
bool TryRegister(HWND hwnd, int id, UINT mod, UINT vk) {
    if (RegisterHotKey(hwnd, id, mod | MOD_NOREPEAT, vk)) return true;
    return RegisterHotKey(hwnd, id, mod, vk) != FALSE;
}
}

HotkeyManager& HotkeyManager::Instance() {
    static HotkeyManager instance;
    return instance;
}

int HotkeyManager::NextId() {
    do {
        m_next_id = m_next_id == 0xBFFF ? 1001 : m_next_id + 1;
    } while (std::any_of(m_bindings.begin(), m_bindings.end(), [&](const Binding& b) { return b.id == m_next_id; }));
    return m_next_id;
}

void HotkeyManager::DrainMessages() {
    if (!m_hwnd) return;
    MSG msg;
    while (PeekMessage(&msg, m_hwnd, WM_HOTKEY, WM_HOTKEY, PM_REMOVE)) {}
}

std::vector<HotkeyManager::Binding> HotkeyManager::MakeBindings(const AppConfig& cfg) const {
    std::vector<Binding> result;
    auto add = [&](UINT mod, UINT vk, HotkeyAction action, const std::string& name, UINT stock_vk) {
        if (!vk) return;
        result.push_back({0, mod, vk, action, true, name});
        if (mod == (MOD_CONTROL | MOD_SHIFT) && vk == stock_vk)
            result.push_back({0, MOD_CONTROL, vk, action, false, name});
    };
    add(cfg.map_hotkey_mod, cfg.map_hotkey_vk, HotkeyAction::MapSwitch, cfg.map_shortcut, VK_OEM_4);
    add(cfg.lock_hotkey_mod, cfg.lock_hotkey_vk, HotkeyAction::LockToggle, cfg.lock_shortcut, VK_OEM_6);
    add(cfg.artifact_hotkey_mod, cfg.artifact_hotkey_vk, HotkeyAction::ArtifactToggle, cfg.show_artifact_shortcut, VK_OEM_5);
    // Primary shortcuts take precedence over optional fallback shortcuts.
    std::stable_sort(result.begin(), result.end(), [](const Binding& a, const Binding& b) { return a.primary && !b.primary; });
    return result;
}

bool HotkeyManager::RegisterAll(HWND hwnd) {
    HWND target = hwnd ? hwnd : m_hwnd;
    UnregisterAll();
    m_hwnd = target;
    if (!m_hwnd) return false;
    m_requested = MakeBindings(AppConfig::Instance());
    for (auto binding : m_requested) {
        const auto duplicate = std::find_if(m_bindings.begin(), m_bindings.end(), [&](const Binding& b) { return b.mod == binding.mod && b.vk == binding.vk; });
        binding.id = NextId();
        if (duplicate == m_bindings.end() && TryRegister(m_hwnd, binding.id, binding.mod, binding.vk)) {
            m_bindings.push_back(binding);
        }
    }
    UpdateFailedHotkeys();
    for (const auto& name : m_failed_hotkeys) {
        std::cerr << "Warning: Failed to register global hotkey '" << name << "'." << std::endl;
    }
    return !HasFailedHotkeys();
}

void HotkeyManager::UnregisterAll(HWND hwnd) {
    HWND target = hwnd ? hwnd : m_hwnd;
    if (target) for (const auto& b : m_bindings) UnregisterHotKey(target, b.id);
    DrainMessages();
    m_bindings.clear();
    m_suspended_bindings.clear();
    m_requested.clear();
    m_failed_hotkeys.clear();
    m_suspended = false;
    m_hwnd = nullptr;
}

bool HotkeyManager::Restore(const std::vector<Binding>& bindings) {
    bool success = true;
    for (auto b : bindings) {
        b.id = NextId();
        if (TryRegister(m_hwnd, b.id, b.mod, b.vk)) m_bindings.push_back(b);
        else success = false;
    }
    return success;
}

void HotkeyManager::UpdateFailedHotkeys() {
    m_failed_hotkeys.clear();
    for (const auto& binding : m_requested) {
        if (!binding.primary) continue;
        const bool active = std::any_of(m_bindings.begin(), m_bindings.end(), [&](const Binding& b) {
            return b.action == binding.action && b.mod == binding.mod && b.vk == binding.vk;
        });
        if (!active) m_failed_hotkeys.push_back(binding.name);
    }
}

bool HotkeyManager::Suspend() {
    if (!m_hwnd) return false;
    if (m_suspended) return true;
    m_suspended_bindings = m_bindings;
    for (const auto& b : m_bindings) UnregisterHotKey(m_hwnd, b.id);
    m_bindings.clear();
    DrainMessages();
    m_suspended = true;
    return true;
}

bool HotkeyManager::Resume() {
    if (!m_suspended) return true;
    const bool success = Restore(m_suspended_bindings);
    m_suspended_bindings.clear();
    m_suspended = false;
    UpdateFailedHotkeys();
    return success;
}

bool HotkeyManager::Apply(const AppConfig& candidate, const std::function<bool()>& persist) {
    if (!m_hwnd) return false;
    const bool was_suspended = m_suspended;
    std::vector<Binding> desired, added;
    bool success = true;
    const auto requested = MakeBindings(candidate);
    const auto& previous = was_suspended ? m_suspended_bindings : m_bindings;
    auto has_primary = [](const std::vector<Binding>& bindings, const Binding& binding) {
        return std::any_of(bindings.begin(), bindings.end(), [&](const Binding& b) {
            return b.primary && b.action == binding.action && b.mod == binding.mod && b.vk == binding.vk;
        });
    };
    for (auto b : requested) {
        const bool required = b.primary && (!has_primary(m_requested, b) || has_primary(previous, b));
        auto duplicate = std::find_if(desired.begin(), desired.end(), [&](const Binding& x) { return x.mod == b.mod && x.vk == b.vk; });
        if (duplicate != desired.end()) {
            if (required) success = false;
            continue;
        }
        auto existing = std::find_if(m_bindings.begin(), m_bindings.end(), [&](const Binding& x) { return x.mod == b.mod && x.vk == b.vk; });
        if (existing != m_bindings.end()) b.id = existing->id;
        else {
            b.id = NextId();
            if (!TryRegister(m_hwnd, b.id, b.mod, b.vk)) {
                if (required) success = false;
                continue;
            }
            added.push_back(b);
        }
        desired.push_back(b);
    }
    if (success && persist) success = persist();
    if (!success) {
        for (const auto& b : added) UnregisterHotKey(m_hwnd, b.id);
        DrainMessages();
        if (was_suspended) Resume();
        return false;
    }
    for (const auto& b : m_bindings) {
        if (std::none_of(desired.begin(), desired.end(), [&](const Binding& x) { return x.id == b.id; }))
            UnregisterHotKey(m_hwnd, b.id);
    }
    DrainMessages();
    m_bindings = std::move(desired);
    m_requested = requested;
    m_suspended_bindings.clear();
    m_suspended = false;
    UpdateFailedHotkeys();
    return true;
}

void HotkeyManager::OnHotkey(int id, LPARAM data) {
    if (m_suspended) return;
    auto it = std::find_if(m_bindings.begin(), m_bindings.end(), [&](const Binding& b) { return b.id == id; });
    if (it == m_bindings.end() || (LOWORD(data) & ~MOD_NOREPEAT) != it->mod || HIWORD(data) != it->vk) return;
    auto cb = m_callbacks.find(it->action);
    if (cb != m_callbacks.end() && cb->second) cb->second();
}

void HotkeyManager::SetCallback(HotkeyAction action, std::function<void()> cb) { m_callbacks[action] = cb; }
