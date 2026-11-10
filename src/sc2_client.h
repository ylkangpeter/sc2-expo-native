#pragma once

#include "map_detector.h"
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>

struct Sc2GameState {
    bool is_running = false;
    std::string screen_state = "unknown"; // "in_game", "matchmaking", "unknown"
    float display_time = 0.0f;
    int current_seconds = 0;
    std::string current_map;
    std::vector<PlayerInfo> players;
    bool is_new_game_event = false;
};

class Sc2Client {
public:
    static Sc2Client& Instance();

    void Start();
    void Stop();

    Sc2GameState GetState();
    bool ConsumeNewGameEvent();
    void SetManualMap(const std::string& map_name);

private:
    Sc2Client();
    ~Sc2Client();

    void PollThreadFunc();
    std::string HttpGet(const std::string& host, int port, const std::string& path);
    size_t ComputeSessionFingerprint(const std::vector<PlayerInfo>& players);

    std::atomic<bool> m_stop_requested{ false };
    std::thread m_thread;

    std::mutex m_mutex;
    Sc2GameState m_state;
    size_t m_current_session_id = 0;
    float m_last_display_time = -1.0f;
    std::string m_manual_map;
};
