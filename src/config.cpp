#include "config.h"
#include <windows.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cctype>
#include <cmath>
#include <algorithm>

using json = nlohmann::json;

std::string AppConfig::FormatHotkey(unsigned int mod, unsigned int vk) {
    if (vk == 0) return "无";
    std::string s;
    if (mod & 0x0002) s += "Ctrl + ";
    if (mod & 0x0001) s += "Alt + ";
    if (mod & 0x0004) s += "Shift + ";
    if (mod & 0x0008) s += "Win + ";

    if (vk >= 'A' && vk <= 'Z') {
        s += static_cast<char>(vk);
    } else if (vk >= '0' && vk <= '9') {
        s += static_cast<char>(vk);
    } else if (vk >= VK_F1 && vk <= VK_F24) {
        s += "F" + std::to_string(vk - VK_F1 + 1);
    } else {
        switch (vk) {
            case VK_OEM_4:      s += "["; break;
            case VK_OEM_6:      s += "]"; break;
            case VK_OEM_5:      s += "\\"; break;
            case VK_OEM_MINUS:  s += "-"; break;
            case VK_OEM_PLUS:   s += "="; break;
            case VK_OEM_1:      s += ";"; break;
            case VK_OEM_7:      s += "'"; break;
            case VK_OEM_COMMA:  s += ","; break;
            case VK_OEM_PERIOD: s += "."; break;
            case VK_OEM_2:      s += "/"; break;
            case VK_OEM_3:      s += "~"; break;
            case VK_SPACE:      s += "Space"; break;
            case VK_TAB:        s += "Tab"; break;
            case VK_LEFT:       s += "Left"; break;
            case VK_RIGHT:      s += "Right"; break;
            case VK_UP:         s += "Up"; break;
            case VK_DOWN:       s += "Down"; break;
            case VK_PRIOR:      s += "PageUp"; break;
            case VK_NEXT:       s += "PageDown"; break;
            case VK_HOME:       s += "Home"; break;
            case VK_END:        s += "End"; break;
            case VK_INSERT:     s += "Insert"; break;
            case VK_DELETE:     s += "Delete"; break;
            default: {
                char hex[16];
                snprintf(hex, sizeof(hex), "0x%02X", vk);
                s += hex;
                break;
            }
        }
    }
    return s;
}

bool AppConfig::ParseHotkey(const std::string& str, unsigned int& out_mod, unsigned int& out_vk) {
    if (str.empty() || str == "无" || str == "None") {
        out_mod = 0;
        out_vk = 0;
        return true;
    }
    std::string lower;
    for (char c : str) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    unsigned int mod = 0;
    std::stringstream ss(lower);
    std::string token;
    std::vector<std::string> parts;
    while (std::getline(ss, token, '+')) {
        size_t first = token.find_first_not_of(" \t\r\n");
        size_t last = token.find_last_not_of(" \t\r\n");
        if (first != std::string::npos && last != std::string::npos) {
            parts.push_back(token.substr(first, last - first + 1));
        }
    }
    if (parts.empty()) return false;

    for (size_t i = 0; i < parts.size() - 1; ++i) {
        const auto& p = parts[i];
        if (p == "ctrl" || p == "control") mod |= 0x0002;
        else if (p == "alt") mod |= 0x0001;
        else if (p == "shift") mod |= 0x0004;
        else if (p == "win") mod |= 0x0008;
    }

    const auto& key_part = parts.back();
    unsigned int vk = 0;
    if (key_part.size() == 1) {
        char c = key_part[0];
        if (c >= 'a' && c <= 'z') vk = static_cast<unsigned int>(std::toupper(static_cast<unsigned char>(c)));
        else if (c >= '0' && c <= '9') vk = static_cast<unsigned int>(c);
        else if (c == '[') vk = VK_OEM_4;
        else if (c == ']') vk = VK_OEM_6;
        else if (c == '\\') vk = VK_OEM_5;
        else if (c == '-') vk = VK_OEM_MINUS;
        else if (c == '=' || c == '+') vk = VK_OEM_PLUS;
        else if (c == ';') vk = VK_OEM_1;
        else if (c == '\'') vk = VK_OEM_7;
        else if (c == ',') vk = VK_OEM_COMMA;
        else if (c == '.') vk = VK_OEM_PERIOD;
        else if (c == '/') vk = VK_OEM_2;
        else if (c == '`' || c == '~') vk = VK_OEM_3;
    } else if (key_part.rfind("f", 0) == 0 && key_part.size() > 1 && std::isdigit(static_cast<unsigned char>(key_part[1]))) {
        int fnum = std::stoi(key_part.substr(1));
        if (fnum >= 1 && fnum <= 24) vk = VK_F1 + (fnum - 1);
    } else if (key_part == "space") vk = VK_SPACE;
    else if (key_part == "tab") vk = VK_TAB;
    else if (key_part == "left") vk = VK_LEFT;
    else if (key_part == "right") vk = VK_RIGHT;
    else if (key_part == "up") vk = VK_UP;
    else if (key_part == "down") vk = VK_DOWN;
    else if (key_part == "pageup") vk = VK_PRIOR;
    else if (key_part == "pagedown") vk = VK_NEXT;
    else if (key_part == "home") vk = VK_HOME;
    else if (key_part == "end") vk = VK_END;
    else if (key_part == "insert") vk = VK_INSERT;
    else if (key_part == "delete") vk = VK_DELETE;

    if (vk == 0) return false;
    out_mod = mod;
    out_vk = vk;
    return true;
}

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
    out_setting_w = std::max(420, (int)std::round(420.0f * scale));
    out_setting_h = std::max(660, (int)std::round(660.0f * scale));
    if (screen_h > 0 && out_setting_h > (int)(screen_h * 0.95f)) {
        out_setting_h = (int)(screen_h * 0.95f);
    }
}

void AppConfig::ResetToResolutionDefaults(int screen_w, int screen_h) {
    int dummy_sw, dummy_sh;
    GetResolutionDefaults(screen_w, screen_h,
                          main_window_width, main_window_height, table_height,
                          font_scale, toast_scale, dummy_sw, dummy_sh);
    config_version = 2;
    map_hotkey_mod = 0x0002 | 0x0004; // Ctrl + Shift
    map_hotkey_vk = 0xDB;              // VK_OEM_4 '['
    map_shortcut = FormatHotkey(map_hotkey_mod, map_hotkey_vk);

    lock_hotkey_mod = 0x0002 | 0x0004; // Ctrl + Shift
    lock_hotkey_vk = 0xDD;             // VK_OEM_6 ']'
    lock_shortcut = FormatHotkey(lock_hotkey_mod, lock_hotkey_vk);

    artifact_hotkey_mod = 0x0002 | 0x0004; // Ctrl + Shift
    artifact_hotkey_vk = 0xDC;             // VK_OEM_5 '\'
    show_artifact_shortcut = FormatHotkey(artifact_hotkey_mod, artifact_hotkey_vk);

    table_font_size = 13;
    toast_allowed = true;
    toast_when_hidden = false;
    toast_position_y = 0.18f;
    toast_duration_ms = 8000;
    toast_show_border = true;
    toast_opacity = 1.0f;
    toast_text_color = { 0.0f, 0.86f, 1.0f };
    toast_border_color = { 0.0f, 0.75f, 1.0f };
    time_alert_seconds = 30;
    mutator_alert_seconds = 30;
    mutator_duration_seconds = 5;
    mutator_toast_position_y = 0.70f;
    artifact_scale_ratio = 0.70f;
    artifact_opacity = 0.65f;
    InitDefaultArtifactPositions();
}

AppConfig& AppConfig::Instance() {
    static AppConfig cfg;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
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
        if (j.contains("map_shortcut")) map_shortcut = j["map_shortcut"].get<std::string>();
        if (j.contains("lock_shortcut")) lock_shortcut = j["lock_shortcut"].get<std::string>();
        if (j.contains("show_artifact_shortcut")) show_artifact_shortcut = j["show_artifact_shortcut"].get<std::string>();

        if (j.contains("map_hotkey_mod")) map_hotkey_mod = j["map_hotkey_mod"].get<unsigned int>();
        if (j.contains("map_hotkey_vk")) map_hotkey_vk = j["map_hotkey_vk"].get<unsigned int>();
        else ParseHotkey(map_shortcut, map_hotkey_mod, map_hotkey_vk);

        if (j.contains("lock_hotkey_mod")) lock_hotkey_mod = j["lock_hotkey_mod"].get<unsigned int>();
        if (j.contains("lock_hotkey_vk")) lock_hotkey_vk = j["lock_hotkey_vk"].get<unsigned int>();
        else ParseHotkey(lock_shortcut, lock_hotkey_mod, lock_hotkey_vk);

        if (j.contains("artifact_hotkey_mod")) artifact_hotkey_mod = j["artifact_hotkey_mod"].get<unsigned int>();
        if (j.contains("artifact_hotkey_vk")) artifact_hotkey_vk = j["artifact_hotkey_vk"].get<unsigned int>();
        else ParseHotkey(show_artifact_shortcut, artifact_hotkey_mod, artifact_hotkey_vk);

        map_shortcut = FormatHotkey(map_hotkey_mod, map_hotkey_vk);
        lock_shortcut = FormatHotkey(lock_hotkey_mod, lock_hotkey_vk);
        show_artifact_shortcut = FormatHotkey(artifact_hotkey_mod, artifact_hotkey_vk);
        if (j.contains("main_window_x")) main_window_x = j["main_window_x"].get<int>();
        if (j.contains("main_window_y")) main_window_y = j["main_window_y"].get<int>();
        if (j.contains("main_window_width")) main_window_width = j["main_window_width"].get<int>();
        if (j.contains("main_window_height")) main_window_height = j["main_window_height"].get<int>();
        if (j.contains("table_height")) table_height = j["table_height"].get<int>();
        if (j.contains("table_font_size")) table_font_size = j["table_font_size"].get<int>();
        if (j.contains("font_scale")) font_scale = j["font_scale"].get<float>();
        if (j.contains("time_alert_seconds")) time_alert_seconds = j["time_alert_seconds"].get<int>();
        if (j.contains("toast_allowed")) toast_allowed = j["toast_allowed"].get<bool>();
        if (j.contains("toast_when_hidden")) toast_when_hidden = j["toast_when_hidden"].get<bool>();
        if (j.contains("toast_position_y")) toast_position_y = j["toast_position_y"].get<float>();
        if (j.contains("toast_scale")) toast_scale = j["toast_scale"].get<float>();
        if (j.contains("toast_duration_ms")) toast_duration_ms = j["toast_duration_ms"].get<int>();
        if (j.contains("toast_show_border")) toast_show_border = j["toast_show_border"].get<bool>();
        if (j.contains("toast_opacity")) {
            float val = j["toast_opacity"].get<float>();
            if (!std::isfinite(val)) val = 1.0f;
            toast_opacity = std::clamp(val, 0.1f, 1.0f);
        }
        if (j.contains("toast_text_color") && j["toast_text_color"].is_array() && j["toast_text_color"].size() >= 3) {
            toast_text_color = j["toast_text_color"].get<std::vector<float>>();
        }
        if (j.contains("toast_border_color") && j["toast_border_color"].is_array() && j["toast_border_color"].size() >= 3) {
            toast_border_color = j["toast_border_color"].get<std::vector<float>>();
        }
        if (j.contains("mutator_alert_seconds")) mutator_alert_seconds = j["mutator_alert_seconds"].get<int>();
        if (j.contains("mutator_duration_seconds")) mutator_duration_seconds = j["mutator_duration_seconds"].get<int>();
        if (j.contains("mutator_toast_position_y")) mutator_toast_position_y = j["mutator_toast_position_y"].get<float>();
        if (j.contains("artifact_scale_ratio")) {
            float val = j["artifact_scale_ratio"].get<float>();
            if (std::isnan(val) || val < 0.1f) {
                artifact_scale_ratio = 0.1f;
            } else if (val > 3.0f) {
                artifact_scale_ratio = 3.0f;
            } else {
                artifact_scale_ratio = val;
            }
        }
        if (j.contains("artifact_opacity")) {
            float val = j["artifact_opacity"].get<float>();
            if (std::isnan(val) || val < 0.1f) {
                artifact_opacity = 0.1f;
            } else if (val > 1.0f) {
                artifact_opacity = 1.0f;
            } else {
                artifact_opacity = val;
            }
        }
        if (j.contains("blacklist") && j["blacklist"].is_array()) {
            blacklist = j["blacklist"].get<std::vector<std::string>>();
        }
        if (j.contains("artifact_positions") && j["artifact_positions"].is_object()) {
            for (auto& [k, v] : j["artifact_positions"].items()) {
                ArtifactPos pos;
                try {
                    if (!v.is_object()) continue;
                    if (v.contains("x_ratio")) pos.x_ratio = v["x_ratio"].get<float>();
                    if (v.contains("y_ratio")) pos.y_ratio = v["y_ratio"].get<float>();
                    if (v.contains("width")) pos.width = v["width"].get<int>();
                    if (v.contains("height")) pos.height = v["height"].get<int>();
                } catch (...) {
                    // Ignore malformed individual markers and retain defaults.
                    continue;
                }
                if (!std::isfinite(pos.x_ratio)) pos.x_ratio = 0.0f;
                if (!std::isfinite(pos.y_ratio)) pos.y_ratio = 0.0f;
                pos.x_ratio = std::clamp(pos.x_ratio, 0.0f, 1.0f);
                pos.y_ratio = std::clamp(pos.y_ratio, 0.0f, 1.0f);
                pos.width = std::clamp(pos.width, 1, 512);
                pos.height = std::clamp(pos.height, 1, 512);
                artifact_positions[k] = pos;
            }
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
        j["map_shortcut"] = map_shortcut;
        j["map_hotkey_mod"] = map_hotkey_mod;
        j["map_hotkey_vk"] = map_hotkey_vk;

        j["lock_shortcut"] = lock_shortcut;
        j["lock_hotkey_mod"] = lock_hotkey_mod;
        j["lock_hotkey_vk"] = lock_hotkey_vk;

        j["show_artifact_shortcut"] = show_artifact_shortcut;
        j["artifact_hotkey_mod"] = artifact_hotkey_mod;
        j["artifact_hotkey_vk"] = artifact_hotkey_vk;
        j["main_window_x"] = main_window_x;
        j["main_window_y"] = main_window_y;
        j["main_window_width"] = main_window_width;
        j["main_window_height"] = main_window_height;
        j["table_height"] = table_height;
        j["table_font_size"] = table_font_size;
        j["font_scale"] = font_scale;
        j["time_alert_seconds"] = time_alert_seconds;
        j["toast_allowed"] = toast_allowed;
        j["toast_when_hidden"] = toast_when_hidden;
        j["toast_position_y"] = toast_position_y;
        j["toast_scale"] = toast_scale;
        j["toast_duration_ms"] = toast_duration_ms;
        j["toast_show_border"] = toast_show_border;
        j["toast_opacity"] = toast_opacity;
        j["toast_text_color"] = toast_text_color;
        j["toast_border_color"] = toast_border_color;
        j["mutator_alert_seconds"] = mutator_alert_seconds;
        j["mutator_duration_seconds"] = mutator_duration_seconds;
        j["mutator_toast_position_y"] = mutator_toast_position_y;
        j["artifact_scale_ratio"] = artifact_scale_ratio;
        j["artifact_opacity"] = artifact_opacity;
        j["blacklist"] = blacklist;

        json j_art = json::object();
        for (const auto& [k, v] : artifact_positions) {
            j_art[k] = {
                { "x_ratio", v.x_ratio },
                { "y_ratio", v.y_ratio },
                { "width", v.width },
                { "height", v.height }
            };
        }
        j["artifact_positions"] = j_art;

        std::ofstream file(path);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Config save warning: " << e.what() << std::endl;
        return false;
    }
}
