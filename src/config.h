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

    // Overlay Window Settings
    int main_window_x = 0;
    int main_window_y = 300;
    int main_window_width = 173;
    int main_window_height = 340;
    int table_height = 228;
    int table_font_size = 13;
    float font_scale = 1.0f;

    // Toast configuration
    bool toast_allowed = true;
    bool toast_when_hidden = false; // Trigger wave toast even when main window is hidden
    int toast_duration_ms = 8000;
    float toast_position_y = 0.18f; // ratio from top of screen
    float toast_scale = 1.43f;
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

    static AppConfig& Instance();
    bool Load(const std::string& path = "config.json");
    bool Save(const std::string& path = "config.json") const;
};
