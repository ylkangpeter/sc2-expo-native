#include "schedule_manager.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring wstr(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wstr[0], wlen);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

static std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, &str[0], len, nullptr, nullptr);
    if (!str.empty() && str.back() == '\0') str.pop_back();
    return str;
}

static std::vector<std::string> ListDirectoryFiles(const std::string& dir_path) {
    std::vector<std::string> files;
    std::wstring search_path = Utf8ToWide(dir_path) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search_path.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(WideToUtf8(fd.cFileName));
            }
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    return files;
}

ScheduleManager& ScheduleManager::Instance() {
    static ScheduleManager instance;
    return instance;
}

int ScheduleManager::ParseTimeToSeconds(const std::string& time_str) {
    if (time_str.empty()) return -1;
    std::string s = time_str;
    s.erase(0, s.find_first_not_of(" \t\r\n"));
    s.erase(s.find_last_not_of(" \t\r\n") + 1);

    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ':')) {
        parts.push_back(item);
    }

    try {
        if (parts.size() == 2) {
            return std::stoi(parts[0]) * 60 + std::stoi(parts[1]);
        } else if (parts.size() == 3) {
            return std::stoi(parts[0]) * 3600 + std::stoi(parts[1]) * 60 + std::stoi(parts[2]);
        }
    } catch (...) {
        return -1;
    }
    return -1;
}

std::string ScheduleManager::FormatSecondsToTime(int total_seconds) {
    if (total_seconds < 0) total_seconds = 0;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    char buf[32];
    if (hours > 0) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, seconds);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds);
    }
    return std::string(buf);
}

void ScheduleManager::LoadAllMaps(const std::string& lang) {
    m_current_lang = lang;
    m_map_list.clear();
    m_schedules.clear();

    std::string dir_path = "resources/maps/" + lang;
    std::vector<std::string> files = ListDirectoryFiles(dir_path);
    if (files.empty()) {
        dir_path = "resources/maps/zh";
        files = ListDirectoryFiles(dir_path);
    }
    if (files.empty()) {
        std::cerr << "Maps directory not found or empty: " << dir_path << std::endl;
        return;
    }

    for (const auto& map_name : files) {
        m_map_list.push_back(map_name);
        GetSchedule(map_name); // Preload
    }

    std::sort(m_map_list.begin(), m_map_list.end());
}

const std::vector<ScheduleRow>& ScheduleManager::GetSchedule(const std::string& map_name) {
    auto it = m_schedules.find(map_name);
    if (it != m_schedules.end()) {
        return it->second;
    }

    std::vector<ScheduleRow> rows;
    std::string file_path = "resources/maps/" + m_current_lang + "/" + map_name;
    std::wstring wpath = Utf8ToWide(file_path);
    std::ifstream file(wpath.c_str());
    if (!file.is_open()) {
        file_path = "resources/maps/zh/" + map_name;
        wpath = Utf8ToWide(file_path);
        file.open(wpath.c_str());
    }

    if (!file.is_open()) {
        m_schedules[map_name] = rows;
        return m_schedules[map_name];
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        // strip CR
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> parts;
        size_t start = 0;
        size_t pos = 0;
        while ((pos = line.find('\t', start)) != std::string::npos) {
            parts.push_back(line.substr(start, pos - start));
            start = pos + 1;
        }
        parts.push_back(line.substr(start));

        ScheduleRow row;
        if (parts.size() >= 2) {
            row.time_text = parts[0];
            row.event_text = parts[1];
            row.seconds = ParseTimeToSeconds(parts[0]);
            if (parts.size() >= 3) {
                row.army_text = parts[2];
                // parse tier and hybrids: e.g. "T4|hybriddestroyer*2|hybridnemesis"
                std::stringstream ss(parts[2]);
                std::string segment;
                while (std::getline(ss, segment, '|')) {
                    if (segment.empty()) continue;
                    if (segment[0] == 'T') {
                        row.tier = segment;
                    } else {
                        // e.g. "hybriddestroyer*2" or "hybriddestroyer"
                        size_t star_pos = segment.find('*');
                        if (star_pos != std::string::npos) {
                            std::string hname = segment.substr(0, star_pos);
                            int count = 1;
                            try { count = std::stoi(segment.substr(star_pos + 1)); } catch (...) {}
                            row.hybrids.push_back({ hname, count });
                        } else {
                            row.hybrids.push_back({ segment, 1 });
                        }
                    }
                }
            }
            rows.push_back(row);
        }
    }

    m_schedules[map_name] = rows;
    return m_schedules[map_name];
}

bool ScheduleManager::GetVariant(const std::string& map_name, MapVariant& out_variant) const {
    if (map_name.empty()) return false;

    size_t dash = map_name.rfind('-');
    if (dash == std::string::npos) {
        // Fallback: If map_name has no suffix (e.g. "往日神庙"), search for existing variant in m_map_list
        std::string prefix_match = map_name + "-";
        for (const auto& m : m_map_list) {
            if (m.rfind(prefix_match, 0) == 0) {
                return GetVariant(m, out_variant);
            }
        }
        return false;
    }

    std::string prefix = map_name.substr(0, dash);
    std::string suffix = map_name.substr(dash + 1);

    if (suffix == "A" || suffix == "B") {
        out_variant.type = "AB";
        out_variant.current_suffix = suffix;
        out_variant.prefix = prefix;
        out_variant.opt1 = "A";
        out_variant.opt2 = "B";
        return true;
    } else if (suffix == "左" || suffix == "右") {
        out_variant.type = "LR";
        out_variant.current_suffix = suffix;
        out_variant.prefix = prefix;
        out_variant.opt1 = "左";
        out_variant.opt2 = "右";
        return true;
    } else if (suffix == "Left" || suffix == "Right") {
        out_variant.type = "LR";
        out_variant.current_suffix = suffix;
        out_variant.prefix = prefix;
        out_variant.opt1 = "Left";
        out_variant.opt2 = "Right";
        return true;
    }
    return false;
}

std::string ScheduleManager::SwitchVariant(const std::string& current_map) const {
    MapVariant var;
    if (!GetVariant(current_map, var)) return current_map;

    std::string next_suffix = (var.current_suffix == var.opt1) ? var.opt2 : var.opt1;
    std::string next_map = var.prefix + "-" + next_suffix;

    // Check if next_map exists in loaded map list
    if (std::find(m_map_list.begin(), m_map_list.end(), next_map) != m_map_list.end()) {
        return next_map;
    }
    return current_map;
}
