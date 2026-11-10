#include "mutator_manager.h"
#include "schedule_manager.h"
#include <fstream>
#include <iostream>

MutatorManager& MutatorManager::Instance() {
    static MutatorManager instance;
    return instance;
}

MutatorManager::MutatorManager() {
    for (const auto& t : m_types) {
        m_enabled[t] = false;
    }
}

void MutatorManager::LoadAllMutators() {
    m_events.clear();
    for (const auto& t : m_types) {
        std::string path = "resources/mutator/" + t + ".txt";
        std::ifstream file(path);
        if (!file.is_open()) continue;

        std::vector<MutatorEvent> list;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            if (line.back() == '\r') line.pop_back();

            size_t tab_pos = line.find('\t');
            if (tab_pos == std::string::npos) continue;

            std::string time_str = line.substr(0, tab_pos);
            std::string msg = line.substr(tab_pos + 1);
            int sec = ScheduleManager::ParseTimeToSeconds(time_str);
            if (sec >= 0) {
                list.push_back({ sec, msg });
            }
        }
        m_events[t] = list;
    }
}

bool MutatorManager::IsMutatorEnabled(const std::string& type) const {
    auto it = m_enabled.find(type);
    return (it != m_enabled.end() && it->second);
}

void MutatorManager::SetMutatorEnabled(const std::string& type, bool enabled) {
    m_enabled[type] = enabled;
    if (!enabled) {
        m_alerted_points[type].clear();
        // remove existing active alerts of this type
        for (auto it = m_active_alerts.begin(); it != m_active_alerts.end(); ) {
            if (it->mutator_type == type) {
                it = m_active_alerts.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void MutatorManager::ToggleMutator(const std::string& type) {
    SetMutatorEnabled(type, !IsMutatorEnabled(type));
}

void MutatorManager::ResetAlerts() {
    m_alerted_points.clear();
    m_active_alerts.clear();
    m_last_game_seconds = -1;
}

void MutatorManager::Update(int current_seconds, float delta_time) {
    // If game restarted (time went back)
    if (m_last_game_seconds > 0 && current_seconds < m_last_game_seconds) {
        ResetAlerts();
    }
    m_last_game_seconds = current_seconds;

    // Decay active alerts
    for (auto it = m_active_alerts.begin(); it != m_active_alerts.end(); ) {
        it->expire_time -= delta_time;
        if (it->expire_time <= 0.0f) {
            it = m_active_alerts.erase(it);
        } else {
            ++it;
        }
    }

    if (current_seconds <= 0) return;

    // Position mapping
    std::unordered_map<std::string, float> pos_map = {
        { "deployment", 0.20f },
        { "propagator", 0.35f },
        { "voidrifts",  0.50f },
        { "killbots",   0.65f },
        { "bombbots",   0.80f }
    };

    const int alert_threshold = 30; // seconds

    for (const auto& t : m_types) {
        if (!m_enabled[t]) continue;
        auto ev_it = m_events.find(t);
        if (ev_it == m_events.end()) continue;

        auto& alerted = m_alerted_points[t];
        for (const auto& ev : ev_it->second) {
            if (alerted.find(ev.seconds) != alerted.end()) continue;

            int diff = ev.seconds - current_seconds;
            if (diff > 0 && diff <= alert_threshold) {
                alerted.insert(ev.seconds);

                MutatorActiveAlert alert;
                alert.mutator_type = t;
                alert.message = std::to_string(diff) + "s : " + ev.message;
                alert.time_remaining = (float)diff;
                alert.expire_time = 5.0f; // 5 seconds display
                alert.screen_x_ratio = pos_map[t];
                m_active_alerts.push_back(alert);
            }
        }
    }
}
