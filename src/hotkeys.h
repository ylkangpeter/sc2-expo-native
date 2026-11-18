#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

struct AppConfig;
enum class HotkeyAction { MapSwitch, LockToggle, ArtifactToggle };

class HotkeyManager {
public:
    static HotkeyManager& Instance();
    bool RegisterAll(HWND hwnd);
    void UnregisterAll(HWND hwnd = nullptr);
    // Persist runs after new primary shortcuts are registered and active ones are retained.
    bool Apply(const AppConfig& candidate, const std::function<bool()>& persist);
    bool Suspend();
    bool Resume();
    void OnHotkey(int id, LPARAM data);
    void SetCallback(HotkeyAction action, std::function<void()> cb);
    const std::vector<std::string>& GetFailedHotkeys() const { return m_failed_hotkeys; }
    bool HasFailedHotkeys() const { return !m_failed_hotkeys.empty(); }
private:
    struct Binding { int id; UINT mod; UINT vk; HotkeyAction action; bool primary; std::string name; };
    HotkeyManager() = default;
    std::vector<Binding> MakeBindings(const AppConfig& cfg) const;
    bool Restore(const std::vector<Binding>& bindings);
    void UpdateFailedHotkeys();
    int NextId();
    void DrainMessages();
    HWND m_hwnd = nullptr;
    int m_next_id = 1000;
    bool m_suspended = false;
    std::vector<Binding> m_bindings;
    std::vector<Binding> m_suspended_bindings;
    std::vector<Binding> m_requested;
    std::unordered_map<HotkeyAction, std::function<void()>> m_callbacks;
    std::vector<std::string> m_failed_hotkeys;
};
