#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct PlayerInfo {
    int id = 0;
    std::string name;
    std::string type;
    std::string race;
};

class MapDetector {
public:
    static MapDetector& Instance();

    // Identify map by player roster and optional map_hint from SC2
    std::string Identify(const std::vector<PlayerInfo>& players, const std::string& map_hint = "");

private:
    MapDetector();
    struct MapCheckRule {
        int total_players = 0;
        std::unordered_map<int, std::unordered_set<std::string>> checks; // player_index -> set of names
    };
    std::unordered_map<std::string, MapCheckRule> m_rules;
};
