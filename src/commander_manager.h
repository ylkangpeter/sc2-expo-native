#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>

struct CommanderInfo {
    std::string name; // "raynor"
    std::unordered_map<std::string, std::string> region_urls; // "KR" -> "battlenet:://starcraft/map/3/148741"
};

class CommanderManager {
public:
    static CommanderManager& Instance();

    void LoadConfig();
    const std::vector<std::vector<std::string>>& GetAvatarGrid() const { return m_avatar_grid; }
    std::string GetUrl(const std::string& commander, const std::string& region) const;
    std::string GetReplacementCommand(const std::string& commander, const std::string& region) const;

    // Clipboard operation
    bool CopyReplacementLink(const std::string& commander, const std::string& region, HWND hwnd = nullptr);
    std::string PickRandomCommander(const std::string& region, const std::vector<std::string>& blacklist) const;

    // Localized text from words.conf
    std::string GetText(const std::string& key, const std::string& lang = "zh") const;

private:
    CommanderManager();
    std::vector<std::vector<std::string>> m_avatar_grid;
    std::unordered_map<std::string, CommanderInfo> m_commanders;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_words; // section -> (key -> text)
};
