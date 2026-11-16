#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

struct ArtifactPos {
    float x_ratio = 0.0f;
    float y_ratio = 0.0f;
    int width = 40;
    int height = 40;
};

struct AppConfig {
    std::string current_region = "kr";
    std::string current_language = "zh";

    bool debug_mode = false;
    float debug_time_factor = 5.0f;

    // Hotkeys
    std::string map_shortcut = "ctrl + shift + [";
    std::string lock_shortcut = "ctrl + shift + ]";
    std::string screenshot_shortcut = "ctrl + shift + -";
    std::string show_artifact_shortcut = "ctrl + shift + \\";

    // Overlay Window Settings (Base 1080p reference, dynamically scaled)
    int main_window_x = 0;
    int main_window_y = 300;
    int main_window_width = 180;
    int main_window_height = 350;
    int table_height = 235;
    int table_font_size = 13;
    float font_scale = 1.0f;

    // Config schema version for safe migrations without guessing values
    int config_version = 2;

    // Toast configuration
    bool toast_allowed = true;
    bool toast_when_hidden = false; // Trigger wave toast even when main window is hidden
    int toast_duration_ms = 8000;
    float toast_position_y = 0.18f; // ratio from top of screen
    float toast_scale = 1.43f;
    bool toast_show_border = true;
    float toast_opacity = 1.0f;
    std::vector<float> toast_text_color = { 0.0f, 0.86f, 1.0f }; // Bright cyan RGB [0..1]
    std::vector<float> toast_border_color = { 0.0f, 0.75f, 1.0f }; // Bright cyan RGB [0..1]

    // Time alert threshold in seconds
    int time_alert_seconds = 30;

    // Mutator alert configuration
    int mutator_alert_seconds = 30;
    int mutator_duration_seconds = 5;
    float mutator_toast_position_y = 0.70f;

    // Artifact display
    float artifact_scale_ratio = 0.70f;
    float artifact_opacity = 0.65f;

    // Blacklist for random commander
    std::vector<std::string> blacklist;

    // Artifact positions
    std::unordered_map<std::string, ArtifactPos> artifact_positions;

    AppConfig() {
        InitDefaultArtifactPositions();
    }

    void InitDefaultArtifactPositions() {
        artifact_positions.clear();
        artifact_positions["死亡摇篮"] = { 600.0f / 1174.0f, 483.0f / 876.0f, 40, 40 };
        artifact_positions["湮灭快车"] = { 830.0f / 1987.0f, 1050.0f / 1353.0f, 40, 40 };
        artifact_positions["净网行动"] = { 398.0f / 1496.0f, 688.0f / 996.0f, 40, 40 };
        artifact_positions["营救矿工"] = { 1080.0f / 1755.0f, 391.0f / 844.0f, 40, 40 };
        artifact_positions["亡者之夜"] = { 468.0f / 1242.0f, 495.0f / 938.0f, 40, 40 };
        artifact_positions["熔火危机"] = { 730.0f / 1783.0f, 1200.0f / 1334.0f, 40, 40 };
    }

    static AppConfig& Instance();
    static void GetResolutionDefaults(int screen_w, int screen_h,
                                      int& out_main_w, int& out_main_h, int& out_table_h,
                                      float& out_font_scale, float& out_toast_scale,
                                      int& out_setting_w, int& out_setting_h);
    void ResetToResolutionDefaults(int screen_w, int screen_h);

    bool NeedsMigration() const { return config_version < 2; }
    void MarkMigrated() { config_version = 2; }

    bool Load(const std::string& path = "config.json");
    bool Save(const std::string& path = "config.json") const;
};
