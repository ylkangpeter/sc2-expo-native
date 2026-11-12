#include "config.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;

void AppConfig::GetResolutionDefaults(int screen_w, int screen_h,
                                      int& out_main_w, int& out_main_h, int& out_table_h,
                                      float& out_font_scale, float& out_toast_scale,
                                      int& out_setting_w, int& out_setting_h) {
    if (screen_w <= 0) screen_w = 1920;
    if (screen_h <= 0) screen_h = 1080;

    // Handle ultrawide (21:9, 32:9) screens so horizontal resolution does not inflate UI scale
    int effective_w = screen_w;
    if (screen_h > 0 && screen_w > (screen_h * 16 / 9) * 1.15f) {
        effective_w = (screen_h * 16) / 9;
    }

    float scale = (float)effective_w / 1920.0f;
    if (scale < 0.75f) scale = 0.75f;

    // Main HUD dimensions
    out_main_w = (int)std::round(180.0f * scale);
    out_main_h = (int)std::round(350.0f * scale);
    out_table_h = (int)std::round(235.0f * scale);

    // Font and Toast scales
    // User font preference baseline remains 1.0x; physical High-DPI scaling is handled by ImGui's FontScaleDpi
    out_font_scale = 1.0f;
    out_toast_scale = std::round(1.4f * scale * 10.0f) / 10.0f;

    // Settings window dimensions
    out_setting_w = std::max(380, (int)std::round(380.0f * scale));
    out_setting_h = std::max(600, (int)std::round(600.0f * scale));
    if (screen_h > 0 && out_setting_h > (int)(screen_h * 0.9f)) {
        out_setting_h = (int)(screen_h * 0.9f);
    }
}

void AppConfig::ResetToResolutionDefaults(int screen_w, int screen_h) {
    int dummy_sw, dummy_sh;
    GetResolutionDefaults(screen_w, screen_h,
                          main_window_width, main_window_height, table_height,
                          font_scale, toast_scale, dummy_sw, dummy_sh);
    config_version = 2;
    toast_allowed = true;
    toast_when_hidden = false;
    toast_position_y = 0.18f;
    toast_duration_ms = 8000;
    toast_text_color = { 0.0f, 0.86f, 1.0f };
    toast_border_color = { 0.0f, 0.75f, 1.0f };
}

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
        if (j.contains("config_version")) config_version = j["config_version"].get<int>();
        else config_version = 0; // Legacy v1.0.0 unversioned config
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
        j["config_version"] = config_version;
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
