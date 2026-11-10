#include "commander_manager.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <random>
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

CommanderManager& CommanderManager::Instance() {
    static CommanderManager instance;
    return instance;
}

CommanderManager::CommanderManager() {
    m_avatar_grid = {
        { "raynor", "kerrigan", "artanis", "swann", "zagara", "vorazun", "karax", "abathur", "alarak", "nova" },
        { "stukov", "fenix", "dehaka", "horner", "tychus", "zeratul", "stetmann", "mengsk" }
    };
}

void CommanderManager::LoadConfig() {
    // 1. Load replacement.conf
    std::ifstream file("resources/commander/replacement.conf");
    if (file.is_open()) {
        std::string line;
        std::string current_cmd;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            // Trim
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            if (line.empty()) continue;

            size_t eq_pos = line.find('=');
            if (eq_pos != std::string::npos && line.find('{') != std::string::npos) {
                current_cmd = line.substr(0, eq_pos);
                current_cmd.erase(0, current_cmd.find_first_not_of(" \t"));
                current_cmd.erase(current_cmd.find_last_not_of(" \t") + 1);
                m_commanders[current_cmd].name = current_cmd;
                continue;
            }

            if (!current_cmd.empty() && line[0] == '}') {
                current_cmd.clear();
                continue;
            }

            if (!current_cmd.empty() && line.find(':') != std::string::npos) {
                // e.g. 'KR': 'battlenet:://starcraft/map/3/148741',
                size_t q1 = line.find('\'');
                size_t q2 = (q1 != std::string::npos) ? line.find('\'', q1 + 1) : std::string::npos;
                size_t q3 = (q2 != std::string::npos) ? line.find('\'', q2 + 1) : std::string::npos;
                size_t q4 = (q3 != std::string::npos) ? line.find('\'', q3 + 1) : std::string::npos;

                if (q1 != std::string::npos && q2 != std::string::npos && q3 != std::string::npos && q4 != std::string::npos) {
                    std::string reg = line.substr(q1 + 1, q2 - q1 - 1);
                    std::string url = line.substr(q3 + 1, q4 - q3 - 1);
                    m_commanders[current_cmd].region_urls[reg] = url;
                }
            }
        }
    }

    // 2. Load words.conf
    std::ifstream wfile("resources/words.conf");
    if (wfile.is_open()) {
        try {
            json j;
            wfile >> j;
            if (j.contains("commander_selector")) {
                for (auto& lang_item : j["commander_selector"].items()) {
                    std::string lang = lang_item.key();
                    for (auto& kv : lang_item.value().items()) {
                        m_words[lang][kv.key()] = kv.value().get<std::string>();
                    }
                }
            }
        } catch (...) {}
    }
}

std::string CommanderManager::GetUrl(const std::string& commander, const std::string& region) const {
    auto it = m_commanders.find(commander);
    if (it != m_commanders.end()) {
        std::string r = region;
        std::transform(r.begin(), r.end(), r.begin(), ::toupper);
        auto r_it = it->second.region_urls.find(r);
        if (r_it != it->second.region_urls.end()) {
            return r_it->second;
        }
    }
    return "";
}

std::string CommanderManager::GetReplacementCommand(const std::string& commander, const std::string& region) const {
    std::string url = GetUrl(commander, region);
    if (url.empty()) return "";
    return "替换为" + commander + "指挥官: " + url;
}

bool CommanderManager::CopyReplacementLink(const std::string& commander, const std::string& region, HWND hwnd) {
    std::string text = GetReplacementCommand(commander, region);
    if (text.empty()) return false;

    // Convert UTF-8 std::string to UTF-16 std::wstring for Windows clipboard
    int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return false;

    std::wstring wtext(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);

    bool opened = false;
    for (int i = 0; i < 5; ++i) {
        if (OpenClipboard(hwnd)) {
            opened = true;
            break;
        }
        Sleep(10);
    }
    if (!opened) return false;

    EmptyClipboard();

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, wlen * sizeof(wchar_t));
    if (hMem) {
        wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
        if (pMem) {
            wcscpy_s(pMem, wlen, wtext.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }
    CloseClipboard();
    return true;
}

std::string CommanderManager::PickRandomCommander(const std::string& region, const std::vector<std::string>& blacklist) const {
    std::vector<std::string> pool;
    for (const auto& row : m_avatar_grid) {
        for (const auto& cmd : row) {
            if (std::find(blacklist.begin(), blacklist.end(), cmd) != blacklist.end()) {
                continue;
            }
            if (!GetUrl(cmd, region).empty()) {
                pool.push_back(cmd);
            }
        }
    }
    if (pool.empty()) return "";

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, (int)pool.size() - 1);
    return pool[dis(gen)];
}

std::string CommanderManager::GetText(const std::string& key, const std::string& lang) const {
    auto l_it = m_words.find(lang);
    if (l_it != m_words.end()) {
        auto k_it = l_it->second.find(key);
        if (k_it != l_it->second.end()) {
            return k_it->second;
        }
    }
    return key;
}
