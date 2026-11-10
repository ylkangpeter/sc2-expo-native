#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct ScheduleRow {
    int seconds = 0;
    std::string time_text;
    std::string event_text;
    std::string army_text; // e.g. "T4|hybriddestroyer*2"
    std::string tier;      // e.g. "T4"
    std::vector<std::pair<std::string, int>> hybrids; // e.g. ("hybriddestroyer", 2)
};

struct MapVariant {
    std::string type; // "AB" or "LR"
    std::string current_suffix; // "A", "B", "左", "右"
    std::string prefix;
    std::string opt1;
    std::string opt2;
};

class ScheduleManager {
public:
    static ScheduleManager& Instance();

    void LoadAllMaps(const std::string& lang = "zh");
    const std::vector<std::string>& GetMapList() const { return m_map_list; }
    const std::vector<ScheduleRow>& GetSchedule(const std::string& map_name);
    
    // Check variant
    bool GetVariant(const std::string& map_name, MapVariant& out_variant) const;
    std::string SwitchVariant(const std::string& current_map) const;

    // Canonical Map ID and Localization mapping
    std::string GetCanonicalMapId(const std::string& any_name) const;
    std::string GetMapDisplayName(const std::string& any_name, const std::string& target_lang) const;
    std::string NormalizeToCurrentLanguage(const std::string& any_name) const;
    const std::string& GetCurrentLanguage() const { return m_current_lang; }

    // Time parsing and formatting utilities
    static int ParseTimeToSeconds(const std::string& time_str);
    static std::string FormatSecondsToTime(int total_seconds);

private:
    ScheduleManager() = default;
    std::string m_current_lang;
    std::vector<std::string> m_map_list;
    std::unordered_map<std::string, std::vector<ScheduleRow>> m_schedules;
};
