#include "overlay_ui.h"
#include "imgui.h"
#include "sc2_client.h"
#include "config.h"
#include "troop_manager.h"
#include "texture_loader.h"
#include "commander_manager.h"
#include "hotkeys.h"
#include "artifact_viewer.h"
#include "lock_window.h"
#include "toast_window.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <iostream>

#define WIKI_URL "https://starcraft.huijiwiki.com/wiki/%E5%90%88%E4%BD%9C%E4%BB%BB%E5%8A%A1/%E7%AA%81%E5%8F%98%E5%88%97%E8%A1%A8"

OverlayUI& OverlayUI::Instance() {
    static OverlayUI instance;
    return instance;
}

void OverlayUI::Init(ID3D11Device* device, HWND hwnd) {
    m_device = device;
    m_hwnd = hwnd;

    const auto& cfg = AppConfig::Instance();
    m_current_region = cfg.current_region;
    std::transform(m_current_region.begin(), m_current_region.end(), m_current_region.begin(), ::toupper);

    ScheduleManager::Instance().LoadAllMaps(cfg.current_language);
    TroopManager::Instance().LoadAllTroops();
    MutatorManager::Instance().LoadAllMutators();
    CommanderManager::Instance().LoadConfig();
    TextureLoader::Instance().Init(device);

    const auto& map_list = ScheduleManager::Instance().GetMapList();
    if (!map_list.empty()) {
        m_current_map = map_list[0];
    }

    // Hotkey callbacks
    HotkeyManager::Instance().SetCallback(HotkeyAction::LockToggle, [this]() {
        ToggleLock();
    });
    HotkeyManager::Instance().SetCallback(HotkeyAction::MapSwitch, [this]() {
        ToggleMapVariant();
    });
    HotkeyManager::Instance().SetCallback(HotkeyAction::ArtifactToggle, [this]() {
        ToggleArtifact();
    });

    // Default to locked (click-through) on startup as requested
    SetLocked(true);
}

void OverlayUI::UpdateClickThroughState() {
    if (!m_hwnd) return;
    LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (m_locked) {
        ex |= WS_EX_TRANSPARENT;
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void OverlayUI::UpdateWindowDimensions() {
    if (!m_hwnd) return;
    const auto& cfg = AppConfig::Instance();

    int base_w = cfg.main_window_width > 120 ? cfg.main_window_width : 175;
    int base_h = cfg.main_window_height > 120 ? cfg.main_window_height : 218;

    int target_w = base_w;
    int target_h = m_locked ? base_h : (base_h + 32);

    if (m_show_commander_selector) {
        target_w = 920;
        target_h = std::max(target_h, 330);
    } else if (m_show_settings) {
        target_w = base_w + 320;
        target_h = std::max(target_h, 360);
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, target_w, target_h,
                 SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
}

void OverlayUI::SetSettingsOpen(bool open) {
    m_show_settings = open;
    if (m_show_settings) {
        if (m_locked) {
            SetLocked(false);
        }
    }
    UpdateWindowDimensions();
}

void OverlayUI::ToggleSettings() {
    SetSettingsOpen(!m_show_settings);
}

void OverlayUI::SetLocked(bool locked) {
    m_locked = locked;
    UpdateClickThroughState();
    LockWindow::Instance().SetLocked(m_locked);

    if (m_locked) {
        if (m_show_commander_selector) {
            m_show_commander_selector = false;
        }
        if (m_show_settings) {
            m_show_settings = false;
        }
    }
    UpdateWindowDimensions();
}

void OverlayUI::ToggleLock() {
    SetLocked(!m_locked);
}

void OverlayUI::ToggleArtifact() {
    if (!ArtifactViewer::Instance().HasArtifact(m_current_map)) {
        ArtifactViewer::Instance().Hide();
        return;
    }
    ArtifactViewer::Instance().Toggle(m_current_map);
}

void OverlayUI::ToggleMapVariant() {
    std::string switched = ScheduleManager::Instance().SwitchVariant(m_current_map);
    if (switched != m_current_map) {
        m_current_map = switched;
        Sc2Client::Instance().SetManualMap(switched);
        m_last_active_row = -1;

        if (ArtifactViewer::Instance().IsVisible()) {
            if (ArtifactViewer::Instance().HasArtifact(m_current_map)) {
                ArtifactViewer::Instance().ShowMap(m_current_map);
            } else {
                ArtifactViewer::Instance().Hide();
            }
        }
    }
}

void OverlayUI::SetCommanderSelectorOpen(bool open) {
    m_show_commander_selector = open;
    if (m_show_commander_selector && m_show_settings) {
        m_show_settings = false;
    }
    UpdateWindowDimensions();
}

void OverlayUI::ToggleCommanderSelector() {
    SetCommanderSelectorOpen(!m_show_commander_selector);
}

void OverlayUI::TriggerToast(const ScheduleRow& row, float duration) {
    m_toast.active = true;
    m_toast.time_text = row.time_text;
    m_toast.event_text = row.event_text;
    m_toast.tier_text = row.tier;
    m_toast.hybrid_icons = row.hybrids;
    m_toast.expire_time = duration;

    m_toast.regular_unit_icons.clear();
    if (!row.tier.empty() && row.tier[0] == 'T') {
        try {
            int t_num = std::stoi(row.tier.substr(1));
            m_toast.regular_unit_icons = TroopManager::Instance().GetUnitsForTier(m_current_troop, t_num);
        } catch (...) {}
    }

    ToastWindow::Instance().ShowToast(row, m_current_troop, duration);
}

void OverlayUI::Render(float delta_time) {
    // 1. Sync SC2 State
    Sc2GameState state = Sc2Client::Instance().GetState();
    if (!state.current_map.empty() && state.current_map != m_current_map) {
        m_current_map = state.current_map;
        m_last_active_row = -1;
        if (ArtifactViewer::Instance().IsVisible()) {
            if (ArtifactViewer::Instance().HasArtifact(m_current_map)) {
                ArtifactViewer::Instance().ShowMap(m_current_map);
            } else {
                ArtifactViewer::Instance().Hide();
            }
        }
    }

    // Update mutators & toast window
    MutatorManager::Instance().Update(state.current_seconds, delta_time);
    ToastWindow::Instance().Update(delta_time);

    // 2. Render Windows
    RenderMainHUD(delta_time);
    if (m_show_commander_selector) {
        RenderCommanderSelector(delta_time);
    }
    if (m_show_settings) {
        RenderSettings(delta_time);
    }
}

void OverlayUI::RenderMainHUD(float delta_time) {
    auto& cfg = AppConfig::Instance();
    Sc2GameState state = Sc2Client::Instance().GetState();
    ImGuiIO& io = ImGui::GetIO();

    int hud_w = cfg.main_window_width > 120 ? cfg.main_window_width : 175;
    int hud_h = m_locked ? cfg.main_window_height : (cfg.main_window_height + 32);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)hud_w, (float)hud_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.12f, 0.15f, 0.90f));
    // Subtle clean dark border - no distracting red/green border!
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.28f, 0.35f, 0.6f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 5.0f));

    if (ImGui::Begin("MainHUD", nullptr, flags)) {
        float base_scale = cfg.font_scale > 0.5f ? cfg.font_scale : 1.0f;
        ImGui::SetWindowFontScale(base_scale);

        // Drag window by clicking and dragging on the background when unlocked
        if (!m_locked && ImGui::IsWindowHovered() && ImGui::IsMouseDragging(0) && !ImGui::IsAnyItemActive()) {
            ReleaseCapture();
            SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // --- Row 1: Game Time Display ---
        std::string time_str = ScheduleManager::FormatSecondsToTime(state.current_seconds);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.5f, 1.0f));
        ImGui::SetWindowFontScale(base_scale * 1.15f);
        ImGui::Text(" Time: %s", time_str.c_str());
        if (!m_locked && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("按住可拖动窗口位置");
            if (ImGui::IsMouseDragging(0)) {
                ReleaseCapture();
                SendMessageW(m_hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
        }
        ImGui::SetWindowFontScale(base_scale);
        ImGui::PopStyleColor();

        // --- Row 2: Map selection & Variant buttons ---
        const auto& maps = ScheduleManager::Instance().GetMapList();
        MapVariant var;
        bool has_variant = ScheduleManager::Instance().GetVariant(m_current_map, var);

        ImGui::SetNextItemWidth(has_variant ? 104.0f : 163.0f);
        if (ImGui::BeginCombo("##MapCombo", m_current_map.c_str())) {
            for (const auto& m : maps) {
                bool is_selected = (m == m_current_map);
                if (ImGui::Selectable(m.c_str(), is_selected)) {
                    m_current_map = m;
                    Sc2Client::Instance().SetManualMap(m);
                    m_last_active_row = -1;
                    if (ArtifactViewer::Instance().IsVisible()) {
                        if (ArtifactViewer::Instance().HasArtifact(m_current_map)) {
                            ArtifactViewer::Instance().ShowMap(m_current_map);
                        } else {
                            ArtifactViewer::Instance().Hide();
                        }
                    }
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Map variants buttons (A/B or 左/右)
        if (has_variant) {
            ImGui::SameLine();
            bool is_opt1 = (var.current_suffix == var.opt1);
            if (is_opt1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.9f, 0.9f));
            if (ImGui::Button(var.opt1.c_str(), ImVec2(24, 20))) {
                m_current_map = var.prefix + "-" + var.opt1;
                Sc2Client::Instance().SetManualMap(m_current_map);
                m_last_active_row = -1;
                if (ArtifactViewer::Instance().IsVisible()) {
                    if (ArtifactViewer::Instance().HasArtifact(m_current_map)) {
                        ArtifactViewer::Instance().ShowMap(m_current_map);
                    } else {
                        ArtifactViewer::Instance().Hide();
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("切换到变种 %s (快捷键: Ctrl+Shift+[ 或 Ctrl+[)", var.opt1.c_str());
            }
            if (is_opt1) ImGui::PopStyleColor();

            ImGui::SameLine();
            bool is_opt2 = (var.current_suffix == var.opt2);
            if (is_opt2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.9f, 0.9f));
            if (ImGui::Button(var.opt2.c_str(), ImVec2(24, 20))) {
                m_current_map = var.prefix + "-" + var.opt2;
                Sc2Client::Instance().SetManualMap(m_current_map);
                m_last_active_row = -1;
                if (ArtifactViewer::Instance().IsVisible()) {
                    if (ArtifactViewer::Instance().HasArtifact(m_current_map)) {
                        ArtifactViewer::Instance().ShowMap(m_current_map);
                    } else {
                        ArtifactViewer::Instance().Hide();
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("切换到变种 %s (快捷键: Ctrl+Shift+[ 或 Ctrl+[)", var.opt2.c_str());
            }
            if (is_opt2) ImGui::PopStyleColor();
        }

        // --- Row 4: Wave Schedule Table ---
        const auto& schedule = ScheduleManager::Instance().GetSchedule(m_current_map);

        // Find upcoming event
        int active_row = -1;
        int active_seconds = -1;
        for (int i = 0; i < (int)schedule.size(); ++i) {
            if (schedule[i].seconds >= state.current_seconds) {
                active_row = i;
                active_seconds = schedule[i].seconds;
                break;
            }
        }
        if (active_row == -1 && !schedule.empty()) {
            active_row = (int)schedule.size() - 1;
            active_seconds = schedule.back().seconds;
        }

        // Auto trigger toast within threshold (ONLY during active in-game match)
        if (state.screen_state == "in_game" && state.current_seconds > 0 && active_row >= 0 && active_seconds > 0) {
            int diff = active_seconds - state.current_seconds;
            if (diff > 0 && diff <= cfg.time_alert_seconds && m_last_toast_seconds != active_seconds && !m_toast.active) {
                TriggerToast(schedule[active_row], (float)cfg.toast_duration_ms / 1000.0f);
                m_last_toast_seconds = active_seconds;
            }
        }

        if (m_toast.active) {
            m_toast.expire_time -= delta_time;
            if (m_toast.expire_time <= 0.0f) {
                m_toast.active = false;
            }
        }

        // --- Row 5: Schedule Table (Fixed, stable height) ---
        float table_h = (float)cfg.table_height;
        if (ImGui::BeginTable("ScheduleTable", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg, ImVec2(163, table_h))) {
            ImGui::TableSetupColumn("时间", ImGuiTableColumnFlags_WidthFixed, 36.0f);
            ImGui::TableSetupColumn("波次/事件", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("阶数", ImGuiTableColumnFlags_WidthFixed, 28.0f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < (int)schedule.size(); ++i) {
                const auto& row = schedule[i];
                ImGui::TableNextRow();

                ImVec4 text_color(1.0f, 1.0f, 1.0f, 0.85f);
                if (i < active_row) {
                    text_color = ImVec4(0.5f, 0.5f, 0.5f, 0.6f);
                } else if (i == active_row) {
                    text_color = ImVec4(0.0f, 1.0f, 0.5f, 1.0f);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(0, 255, 128, 45));
                } else if (row.seconds > state.current_seconds && (row.seconds - state.current_seconds) <= cfg.time_alert_seconds) {
                    text_color = ImVec4(0.0f, 0.85f, 1.0f, 1.0f);
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImColor(0, 191, 255, 35));
                }

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, text_color);
                ImGui::Text("%s", row.time_text.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    TriggerToast(row);
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(row.event_text.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    TriggerToast(row);
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(row.tier.c_str());
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    TriggerToast(row);
                }

                ImGui::PopStyleColor();

                if (i == active_row && m_last_active_row != active_row) {
                    ImGui::SetScrollHereY(0.4f);
                    m_last_active_row = active_row;
                }
            }
            ImGui::EndTable();
        }

        // --- Row 6: Mutator Icons (Compact bottom layout) ---
        ImGui::Spacing();
        const auto& mutator_types = MutatorManager::Instance().GetTypes();
        for (const auto& mt : mutator_types) {
            bool enabled = MutatorManager::Instance().IsMutatorEnabled(mt);
            std::string icon_path = "ico/mutator/" + mt + ".png";
            TextureData td = TextureLoader::Instance().LoadTexture(icon_path);

            ImVec4 tint = enabled ? ImVec4(1, 1, 1, 1) : ImVec4(0.35f, 0.35f, 0.35f, 0.4f);
            if (td.srv) {
                if (ImGui::ImageButton(mt.c_str(), (ImTextureID)td.srv, ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint)) {
                    if (!m_locked) {
                        MutatorManager::Instance().ToggleMutator(mt);
                    }
                }
            } else {
                if (ImGui::Button(mt.substr(0, 3).c_str(), ImVec2(20, 20))) {
                    if (!m_locked) MutatorManager::Instance().ToggleMutator(mt);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s (点击%s)", mt.c_str(), enabled ? "关闭" : "开启");
            }
            ImGui::SameLine();
        }

        // Active Mutator Alerts (only break line if there are alerts)
        const auto& alerts = MutatorManager::Instance().GetActiveAlerts();
        if (!alerts.empty()) {
            ImGui::NewLine();
            for (const auto& a : alerts) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "⚠️ %s", a.message.c_str());
            }
        }

        // --- Row 7: Action Buttons (Expand Commander / Artifact) ---
        if (!m_locked) {
            ImGui::NewLine();
            if (ImGui::Button(m_show_commander_selector ? "收起" : "替换指挥官", ImVec2(80, 22))) {
                ToggleCommanderSelector();
            }
            ImGui::SameLine();
            bool has_art = ArtifactViewer::Instance().HasArtifact(m_current_map);
            if (!has_art) {
                ImGui::BeginDisabled(true);
            }
            if (ImGui::Button("神器点位", ImVec2(75, 22))) {
                if (has_art) {
                    ToggleArtifact();
                }
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                if (has_art) {
                    std::string target = ArtifactViewer::Instance().ResolveArtifactMap(m_current_map);
                    ImGui::SetTooltip("查看当前地图 [%s] 泽拉图神器点位 (Ctrl+Shift+\\ / Ctrl+\\)", target.c_str());
                } else {
                    ImGui::SetTooltip("当前地图无专属神器");
                }
            }
            if (!has_art) {
                ImGui::EndDisabled();
            }
        }

        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void OverlayUI::RenderCommanderSelector(float delta_time) {
    ImGuiIO& io = ImGui::GetIO();

    // Side panel to the right of HUD: width ~685px
    ImGui::SetNextWindowPos(ImVec2(252.0f, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 252.0f, io.DisplaySize.y));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.14f, 0.18f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.6f, 0.9f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));

    if (ImGui::Begin("CommanderSelectorPanel", nullptr, flags)) {
        // Header
        ImGui::TextColored(ImVec4(0.0f, 0.8f, 1.0f, 1.0f), "指挥官替换器 (Commander Selector)");
        ImGui::SameLine(ImGui::GetWindowWidth() - 75);
        if (ImGui::Button("收起 X", ImVec2(65, 22))) {
            ToggleCommanderSelector();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // Region selection
        ImGui::Text("选择服务器区域: ");
        ImGui::SameLine();
        std::vector<std::string> regions = { "CN", "KR", "EU", "US" };
        for (const auto& reg : regions) {
            bool is_curr = (m_current_region == reg);
            if (is_curr) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.9f, 0.9f));
            if (ImGui::Button(reg.c_str(), ImVec2(45, 24))) {
                m_current_region = reg;
                AppConfig::Instance().current_region = reg;
                AppConfig::Instance().Save();
            }
            if (is_curr) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::Spacing();

        // Commander avatars grid (2 rows of commanders)
        const auto& grid = CommanderManager::Instance().GetAvatarGrid();
        for (const auto& row : grid) {
            for (const auto& cmd : row) {
                std::string icon_path = "ico/commander/" + cmd + ".png";
                TextureData td = TextureLoader::Instance().LoadTexture(icon_path);

                if (td.srv) {
                    if (ImGui::ImageButton(cmd.c_str(), (ImTextureID)td.srv, ImVec2(56, 56))) {
                        if (CommanderManager::Instance().CopyReplacementLink(cmd, m_current_region)) {
                            m_notice_text = "已复制指令: 替换为 " + cmd + " 指挥官";
                            m_notice_expire = 3.5f;
                        }
                    }
                } else {
                    if (ImGui::Button(cmd.c_str(), ImVec2(56, 56))) {
                        if (CommanderManager::Instance().CopyReplacementLink(cmd, m_current_region)) {
                            m_notice_text = "已复制指令: 替换为 " + cmd + " 指挥官";
                            m_notice_expire = 3.5f;
                        }
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("点击复制 %s 替换指令", cmd.c_str());
                }
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Random & Wiki buttons
        if (ImGui::Button("🎲 随机指挥官 (Random)", ImVec2(165, 30))) {
            std::string picked = CommanderManager::Instance().PickRandomCommander(m_current_region, AppConfig::Instance().blacklist);
            if (!picked.empty()) {
                CommanderManager::Instance().CopyReplacementLink(picked, m_current_region);
                m_notice_text = "已随机挑选并复制: 替换为 " + picked + " 指挥官";
                m_notice_expire = 3.5f;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("🌐 突变列表 Wiki", ImVec2(140, 30))) {
            ShellExecuteA(NULL, "open", WIKI_URL, NULL, NULL, SW_SHOWNORMAL);
        }

        // Ephemeral notification
        if (m_notice_expire > 0.0f) {
            m_notice_expire -= delta_time;
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "  ✓ %s", m_notice_text.c_str());
        }

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 0.85f),
            "使用说明：点击头像复制指令后，在游戏聊天框按 Ctrl+V 发送，并双击发送出的链接即可进入替换图。");
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void OverlayUI::RenderSettings(float delta_time) {
    auto& cfg = AppConfig::Instance();

    int base_w = cfg.main_window_width > 120 ? cfg.main_window_width : 175;
    int base_h = cfg.main_window_height > 120 ? cfg.main_window_height : 218;
    int target_h = std::max(base_h, 380);

    ImGui::SetNextWindowPos(ImVec2((float)base_w + 4.0f, 0));
    ImGui::SetNextWindowSize(ImVec2(310.0f, (float)target_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                            ImGuiWindowFlags_NoBringToFrontOnFocus;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.14f, 0.18f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.6f, 0.9f, 0.8f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

    static float save_notice_timer = 0.0f;
    if (save_notice_timer > 0.0f) save_notice_timer -= delta_time;

    if (ImGui::Begin("SettingsPanel", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f), "界面与Toast设置 (Settings)");
        ImGui::SameLine(ImGui::GetWindowWidth() - 40);
        if (ImGui::Button("X##CloseSettings", ImVec2(30, 20))) {
            ToggleSettings();
        }

        ImGui::Separator();
        ImGui::Spacing();

        // 1. Window Dimensions
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 窗体尺寸设置 ]");

        bool size_changed = false;
        ImGui::SetNextItemWidth(170);
        if (ImGui::SliderInt("主窗宽度", &cfg.main_window_width, 140, 300, "%d px")) {
            size_changed = true;
        }

        ImGui::SetNextItemWidth(170);
        if (ImGui::SliderInt("主窗高度", &cfg.main_window_height, 180, 450, "%d px")) {
            size_changed = true;
        }

        ImGui::SetNextItemWidth(170);
        if (ImGui::SliderInt("表格高度", &cfg.table_height, 90, 260, "%d px")) {
            size_changed = true;
        }

        if (size_changed) {
            UpdateWindowDimensions();
        }

        ImGui::Spacing();

        // 2. Font Size
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ 文字字体缩放 ]");
        ImGui::SetNextItemWidth(170);
        ImGui::SliderFloat("字体大小", &cfg.font_scale, 0.75f, 1.50f, "%.2f x");

        ImGui::Spacing();

        // 3. Toast Notifications
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Toast 弹窗通知设置 ]");
        ImGui::Checkbox("启用 Toast 预警通知", &cfg.toast_allowed);

        ImGui::SetNextItemWidth(170);
        ImGui::SliderFloat("Toast垂直位置", &cfg.toast_position_y, 0.05f, 0.95f, "%.2f (屏幕比例)");

        ImGui::SetNextItemWidth(170);
        ImGui::SliderFloat("Toast尺寸缩放", &cfg.toast_scale, 0.6f, 1.8f, "%.2f x");

        ImGui::SetNextItemWidth(170);
        int duration_sec = cfg.toast_duration_ms / 1000;
        if (ImGui::SliderInt("显示持续时间", &duration_sec, 2, 15, "%d 秒")) {
            cfg.toast_duration_ms = duration_sec * 1000;
        }

        if (ImGui::Button("🔔 测试 Toast 效果 (Preview)", ImVec2(285, 24))) {
            ToastWindow::Instance().ShowTestToast();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 4. Save & Reset buttons
        if (ImGui::Button("💾 保存设置", ImVec2(100, 26))) {
            cfg.Save();
            save_notice_timer = 3.0f;
        }

        ImGui::SameLine();
        if (ImGui::Button("↺ 恢复紧凑默认", ImVec2(110, 26))) {
            cfg.main_window_width = 175;
            cfg.main_window_height = 218;
            cfg.table_height = 135;
            cfg.font_scale = 1.0f;
            cfg.toast_allowed = true;
            cfg.toast_position_y = 0.88f;
            cfg.toast_scale = 1.0f;
            cfg.toast_duration_ms = 8000;
            cfg.Save();
            UpdateWindowDimensions();
            save_notice_timer = 3.0f;
        }

        ImGui::SameLine();
        if (ImGui::Button("关闭", ImVec2(60, 26))) {
            ToggleSettings();
        }

        if (save_notice_timer > 0.0f) {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.5f, 1.0f), "✓ 设置已成功保存并生效！");
        }
    }
    ImGui::End();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}
