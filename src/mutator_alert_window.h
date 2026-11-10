#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>

struct MutatorAlertSlot {
    HWND hwnd = nullptr;
    std::string type;
    std::string message;
    float time_remaining = 0.0f;
    float x_ratio = 0.5f;
    bool active = false;
};

class MutatorAlertWindow {
public:
    static MutatorAlertWindow& Instance();

    void Init(HINSTANCE hInstance);
    void Shutdown();

    void ShowAlert(const std::string& type, const std::string& message, float duration = 6.0f, float x_ratio = 0.5f);
    void HideAlert(const std::string& type);
    void HideAll();

    void Update(float delta_time);
    void ShowTestAlerts();

private:
    MutatorAlertWindow() = default;
    ~MutatorAlertWindow() { Shutdown(); }

    void RenderSlot(int slot_idx);

    HINSTANCE m_hInstance = nullptr;
    std::vector<MutatorAlertSlot> m_slots;
    std::unordered_map<std::string, int> m_type_to_slot;
};
