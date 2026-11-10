#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct TroopComposition {
    std::string race;       // "protoss", "terran", "zerg"
    std::string full_name;  // "classic_infantry-旧世步兵团"
    std::string en_name;    // "classic_infantry"
    std::string zh_name;    // "旧世步兵团"
    std::unordered_map<int, std::vector<std::string>> tier_units; // tier 1..7 -> list of unit names
};

class TroopManager {
public:
    static TroopManager& Instance();

    void LoadAllTroops();
    const std::vector<std::string>& GetTroopList(const std::string& race = "") const;
    std::vector<std::string> GetUnitsForTier(const std::string& troop_name, int tier) const;
    std::string GetRaceForTroop(const std::string& troop_name) const;

private:
    TroopManager() = default;
    std::vector<std::string> m_all_troop_names;
    std::unordered_map<std::string, TroopComposition> m_troops; // key: full_name, en_name, or zh_name
};
