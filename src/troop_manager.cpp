#include "troop_manager.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>

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

TroopManager& TroopManager::Instance() {
    static TroopManager instance;
    return instance;
}

void TroopManager::LoadAllTroops() {
    m_all_troop_names.clear();
    m_troops.clear();

    std::vector<std::string> races = { "protoss", "terran", "zerg" };
    for (const auto& race : races) {
        std::string dir_path = "resources/troops/" + race;
        std::vector<std::string> files = ListDirectoryFiles(dir_path);

        for (const auto& filename : files) {
            TroopComposition comp;
            comp.race = race;
            comp.full_name = filename;

            size_t dash = filename.find('-');
            if (dash != std::string::npos) {
                comp.en_name = filename.substr(0, dash);
                comp.zh_name = filename.substr(dash + 1);
            } else {
                comp.en_name = filename;
                comp.zh_name = filename;
            }

            std::string full_path = dir_path + "/" + filename;
            std::ifstream file(Utf8ToWide(full_path).c_str());
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    if (line.back() == '\r') line.pop_back();

                    // Format: <tier_num>\t<unit1|unit2...> or space separated
                    size_t tab_pos = line.find('\t');
                    if (tab_pos == std::string::npos) tab_pos = line.find(' ');
                    if (tab_pos == std::string::npos) continue;

                    int tier = 0;
                    try {
                        tier = std::stoi(line.substr(0, tab_pos));
                    } catch (...) {
                        continue;
                    }

                    std::string units_str = line.substr(tab_pos + 1);
                    std::stringstream ss(units_str);
                    std::string unit;
                    std::vector<std::string> unit_list;
                    while (std::getline(ss, unit, '|')) {
                        if (!unit.empty()) {
                            // trim
                            unit.erase(0, unit.find_first_not_of(" \t\r\n"));
                            unit.erase(unit.find_last_not_of(" \t\r\n") + 1);
                            if (!unit.empty()) unit_list.push_back(unit);
                        }
                    }
                    comp.tier_units[tier] = unit_list;
                }
            }

            m_troops[comp.full_name] = comp;
            m_troops[comp.en_name] = comp;
            m_troops[comp.zh_name] = comp;
            m_all_troop_names.push_back(comp.full_name);
        }
    }
}

std::vector<std::string> TroopManager::GetUnitsForTier(const std::string& troop_name, int tier) const {
    auto it = m_troops.find(troop_name);
    if (it != m_troops.end()) {
        auto t_it = it->second.tier_units.find(tier);
        if (t_it != it->second.tier_units.end()) {
            return t_it->second;
        }
    }
    return {};
}

std::string TroopManager::GetRaceForTroop(const std::string& troop_name) const {
    auto it = m_troops.find(troop_name);
    if (it != m_troops.end()) {
        return it->second.race;
    }
    return "";
}

const std::vector<std::string>& TroopManager::GetTroopList(const std::string& race) const {
    return m_all_troop_names;
}
