#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct MutatorEvent {
    int seconds = 0;
    std::string message;
};

struct MutatorActiveAlert {
    std::string mutator_type;
    std::string message;
    float time_remaining = 0.0f; // countdown in seconds
    float expire_time = 0.0f;    // timestamp when toast disappears
    float screen_x_ratio = 0.5f; // horizontal placement on screen
};

class MutatorManager {
public:
    static MutatorManager& Instance();

    void LoadAllMutators();
    bool IsMutatorEnabled(const std::string& type) const;
    void SetMutatorEnabled(const std::string& type, bool enabled);
    void ToggleMutator(const std::string& type);

    // Check alerts given current game time
    void Update(int current_seconds, float delta_time);
    const std::vector<MutatorActiveAlert>& GetActiveAlerts() const { return m_active_alerts; }
    void ResetAlerts();

    const std::vector<std::string>& GetTypes() const { return m_types; }

private:
    MutatorManager();
    std::vector<std::string> m_types = { "deployment", "propagator", "voidrifts", "killbots", "bombbots" };
    std::unordered_map<std::string, bool> m_enabled;
    std::unordered_map<std::string, std::vector<MutatorEvent>> m_events;
    std::unordered_map<std::string, std::unordered_set<int>> m_alerted_points;
    std::vector<MutatorActiveAlert> m_active_alerts;
    int m_last_game_seconds = -1;
};
