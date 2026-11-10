#include "sc2_client.h"
#include "config.h"
#include "map_detector.h"
#include <windows.h>
#include <winhttp.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <iostream>

using json = nlohmann::json;

Sc2Client& Sc2Client::Instance() {
    static Sc2Client instance;
    return instance;
}

Sc2Client::Sc2Client() {
}

Sc2Client::~Sc2Client() {
    Stop();
}

void Sc2Client::Start() {
    if (m_thread.joinable()) return;
    m_stop_requested = false;
    m_thread = std::thread(&Sc2Client::PollThreadFunc, this);
}

void Sc2Client::Stop() {
    m_stop_requested = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

Sc2GameState Sc2Client::GetState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_state;
}

void Sc2Client::SetManualMap(const std::string& map_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_manual_map = map_name;
    m_state.current_map = map_name;
}

std::string Sc2Client::HttpGet(const std::string& host, int port, const std::string& path) {
    std::string response;
    HINTERNET hSession = WinHttpOpen(L"SC2-Native-Poller/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";

    // Set timeout to 1000ms
    WinHttpSetTimeouts(hSession, 1000, 1000, 1000, 1000);

    std::wstring whost(host.begin(), host.end());
    HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::wstring wpath(path.begin(), path.end());
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                       WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;

            std::vector<char> buffer(dwSize + 1);
            if (WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                buffer[dwDownloaded] = '\0';
                response.append(buffer.data(), dwDownloaded);
            }
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

size_t Sc2Client::ComputeSessionFingerprint(const std::vector<PlayerInfo>& players) {
    std::vector<std::string> keys;
    for (const auto& p : players) {
        keys.push_back(std::to_string(p.id) + "|" + p.name + "|" + p.type);
    }
    std::sort(keys.begin(), keys.end());

    std::string combined;
    for (const auto& k : keys) {
        combined += k + ";";
    }
    return std::hash<std::string>{}(combined);
}

void Sc2Client::PollThreadFunc() {
    float mock_time = 0.0f;

    while (!m_stop_requested) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        if (m_stop_requested) break;

        const auto& cfg = AppConfig::Instance();

        if (cfg.debug_mode) {
            // Debug simulation
            mock_time += 0.5f * cfg.debug_time_factor;
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state.is_running = true;
            m_state.screen_state = "in_game";
            m_state.display_time = mock_time;
            m_state.current_seconds = (int)mock_time;
            if (m_state.current_map.empty()) {
                m_state.current_map = "往日神庙-A";
            }
            continue;
        }

        // Live SC2 API polling
        // 1. Poll /game
        std::string game_json = HttpGet("127.0.0.1", 6119, "/game");
        if (game_json.empty()) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_state.is_running = false;
            m_state.screen_state = "unknown";
            continue;
        }

        // 2. Poll /ui
        std::string ui_json = HttpGet("127.0.0.1", 6119, "/ui");
        std::string screen_state = "unknown";
        if (!ui_json.empty()) {
            try {
                json uij = json::parse(ui_json);
                if (uij.contains("activeScreens") && uij["activeScreens"].is_array()) {
                    if (uij["activeScreens"].empty()) {
                        screen_state = "in_game";
                    } else {
                        screen_state = "matchmaking";
                    }
                }
            } catch (...) {}
        }

        try {
            json gj = json::parse(game_json);
            float display_time = gj.value("displayTime", 0.0f);
            std::string map_hint = gj.value("map", "");

            std::vector<PlayerInfo> current_players;
            if (gj.contains("players") && gj["players"].is_array()) {
                for (const auto& item : gj["players"]) {
                    PlayerInfo p;
                    p.id = item.value("id", 0);
                    p.name = item.value("name", "");
                    p.type = item.value("type", "");
                    p.race = item.value("race", "");
                    current_players.push_back(p);
                }
            }

            size_t new_id = ComputeSessionFingerprint(current_players);
            bool time_went_back = (m_last_display_time >= 0.0f && display_time < m_last_display_time);
            bool is_new_game = (new_id != m_current_session_id || time_went_back);

            m_current_session_id = new_id;
            m_last_display_time = display_time;

            std::lock_guard<std::mutex> lock(m_mutex);
            m_state.is_running = true;
            m_state.screen_state = screen_state;
            m_state.display_time = display_time;
            m_state.current_seconds = (int)display_time;
            m_state.players = current_players;
            m_state.is_new_game_event = is_new_game;

            if (is_new_game && current_players.size() > 2) {
                // Check if not all users (i.e. is coop)
                bool all_users = true;
                for (const auto& p : current_players) {
                    if (p.type != "user") {
                        all_users = false;
                        break;
                    }
                }
                if (!all_users) {
                    std::string identified = MapDetector::Instance().Identify(current_players, map_hint);
                    if (!identified.empty()) {
                        m_state.current_map = identified;
                    }
                }
            }
        } catch (...) {
            // Json parse error or malformed response
        }
    }
}
