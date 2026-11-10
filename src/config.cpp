#include "config.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

AppConfig& AppConfig::Instance() {
    static AppConfig cfg;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        // Default artifact positions
        cfg.artifact_positions["死亡摇篮"] = { 600.0f / 1174.0f, 483.0f / 876.0f, 40, 40 };
        cfg.artifact_positions["湮灭快车"] = { 830.0f / 1987.0f, 1050.0f / 1353.0f, 40, 40 };
        cfg.artifact_positions["净网行动"] = { 398.0f / 1496.0f, 688.0f / 996.0f, 40, 40 };
        cfg.artifact_positions["营救矿工"] = { 1080.0f / 1755.0f, 391.0f / 844.0f, 40, 40 };
        cfg.artifact_positions["亡者之夜"] = { 468.0f / 1242.0f, 495.0f / 938.0f, 40, 40 };
        cfg.artifact_positions["熔火危机"] = { 730.0f / 1783.0f, 1200.0f / 1334.0f, 40, 40 };
        cfg.Load();
    }
    return cfg;
}

bool AppConfig::Load(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) return false;
        json j;
        file >> j;

        if (j.contains("current_region")) current_region = j["current_region"].get<std::string>();
        if (j.contains("current_language")) current_language = j["current_language"].get<std::string>();
        if (j.contains("debug_mode")) debug_mode = j["debug_mode"].get<bool>();
        if (j.contains("debug_time_factor")) debug_time_factor = j["debug_time_factor"].get<float>();
        if (j.contains("main_window_x")) main_window_x = j["main_window_x"].get<int>();
        if (j.contains("main_window_y")) main_window_y = j["main_window_y"].get<int>();
        if (j.contains("main_window_width")) main_window_width = j["main_window_width"].get<int>();
        if (j.contains("main_window_height")) main_window_height = j["main_window_height"].get<int>();
        if (j.contains("table_height")) table_height = j["table_height"].get<int>();
        if (j.contains("font_scale")) font_scale = j["font_scale"].get<float>();
        if (j.contains("time_alert_seconds")) time_alert_seconds = j["time_alert_seconds"].get<int>();
        if (j.contains("toast_allowed")) toast_allowed = j["toast_allowed"].get<bool>();
        if (j.contains("toast_when_hidden")) toast_when_hidden = j["toast_when_hidden"].get<bool>();
        if (j.contains("toast_position_y")) toast_position_y = j["toast_position_y"].get<float>();
        if (j.contains("toast_scale")) toast_scale = j["toast_scale"].get<float>();
        if (j.contains("toast_duration_ms")) toast_duration_ms = j["toast_duration_ms"].get<int>();
        if (j.contains("toast_text_color") && j["toast_text_color"].is_array() && j["toast_text_color"].size() >= 3) {
            toast_text_color = j["toast_text_color"].get<std::vector<float>>();
        }
        if (j.contains("toast_border_color") && j["toast_border_color"].is_array() && j["toast_border_color"].size() >= 3) {
            toast_border_color = j["toast_border_color"].get<std::vector<float>>();
        }
        if (j.contains("blacklist") && j["blacklist"].is_array()) {
            blacklist = j["blacklist"].get<std::vector<std::string>>();
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config load warning: " << e.what() << std::endl;
        return false;
    }
}

bool AppConfig::Save(const std::string& path) const {
    try {
        json j;
        j["current_region"] = current_region;
        j["current_language"] = current_language;
        j["debug_mode"] = debug_mode;
        j["debug_time_factor"] = debug_time_factor;
        j["main_window_x"] = main_window_x;
        j["main_window_y"] = main_window_y;
        j["main_window_width"] = main_window_width;
        j["main_window_height"] = main_window_height;
        j["table_height"] = table_height;
        j["font_scale"] = font_scale;
        j["time_alert_seconds"] = time_alert_seconds;
        j["toast_allowed"] = toast_allowed;
        j["toast_when_hidden"] = toast_when_hidden;
        j["toast_position_y"] = toast_position_y;
        j["toast_scale"] = toast_scale;
        j["toast_duration_ms"] = toast_duration_ms;
        j["toast_text_color"] = toast_text_color;
        j["toast_border_color"] = toast_border_color;
        j["blacklist"] = blacklist;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config save warning: " << e.what() << std::endl;
        return false;
    }
}
