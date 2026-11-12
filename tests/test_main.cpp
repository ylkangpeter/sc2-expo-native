#include <windows.h>
#include <wincodec.h>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

#include "config.h"
#include "map_detector.h"
#include "schedule_manager.h"
#include "troop_manager.h"
#include "mutator_manager.h"
#include "commander_manager.h"
#include "texture_loader.h"
#include "artifact_viewer.h"

#include "test_assert.h"

void TestOverlayLifecycle();

void TestConfig() {
    std::cout << "Running TestConfig..." << std::endl;
    AppConfig& cfg = AppConfig::Instance();
    TEST_ASSERT(!cfg.current_region.empty(), "Region should not be empty");
    TEST_ASSERT(!cfg.current_language.empty(), "Language should not be empty");
    TEST_ASSERT(cfg.time_alert_seconds == 30, "Time alert seconds should default to 30");
    TEST_ASSERT(cfg.artifact_positions.size() == 6, "There should be 6 artifact map positions configured");
    TEST_ASSERT(cfg.main_window_width == 173, "main_window_width should default to 173");
    TEST_ASSERT(cfg.main_window_height == 340, "main_window_height should default to 340");
    TEST_ASSERT(cfg.table_height == 228, "table_height should default to 228");
    TEST_ASSERT(std::abs(cfg.toast_position_y - 0.18f) < 0.01f, "toast_position_y should default to 0.18");
    TEST_ASSERT(std::abs(cfg.toast_scale - 1.43f) < 0.01f, "toast_scale should default to 1.43");
    TEST_ASSERT(cfg.toast_text_color.size() == 3, "toast_text_color should have 3 components");
    TEST_ASSERT(cfg.toast_border_color.size() == 3, "toast_border_color should have 3 components");
    TEST_ASSERT(cfg.toast_when_hidden == false, "toast_when_hidden should default to false");

    // Test save & reload
    cfg.toast_when_hidden = true;
    cfg.Save("test_config.json");
    cfg.toast_when_hidden = false; // Reset
    AppConfig test_cfg;
    bool loaded = test_cfg.Load("test_config.json");
    TEST_ASSERT(loaded, "Config should load successfully from test_config.json");
    TEST_ASSERT(test_cfg.current_region == cfg.current_region, "Reloaded region should match");
    TEST_ASSERT(test_cfg.toast_when_hidden == true, "Reloaded toast_when_hidden should match true");
    TEST_ASSERT(test_cfg.toast_text_color.size() == 3, "Reloaded toast_text_color should have 3 components");
    TEST_ASSERT(test_cfg.toast_border_color.size() == 3, "Reloaded toast_border_color should have 3 components");
    remove("test_config.json");

    // Test Resolution Defaults
    {
        int mw, mh, th, sw, sh;
        float fs, ts;
        // 1080p
        AppConfig::GetResolutionDefaults(1920, 1080, mw, mh, th, fs, ts, sw, sh);
        TEST_ASSERT(mw == 180, "1080p main window width should be 180");
        TEST_ASSERT(mh == 350, "1080p main window height should be 350");
        TEST_ASSERT(th == 235, "1080p table height should be 235");
        TEST_ASSERT(std::abs(fs - 1.0f) < 0.05f, "1080p font scale should be 1.0");
        TEST_ASSERT(sw == 380, "1080p setting width should be 380");
        TEST_ASSERT(sh == 600, "1080p setting height should be 600");

        // 2K (2560x1440)
        AppConfig::GetResolutionDefaults(2560, 1440, mw, mh, th, fs, ts, sw, sh);
        TEST_ASSERT(mw > 230 && mw < 250, "2K main window width should scale with width ratio");
        TEST_ASSERT(mh > 450 && mh < 480, "2K main window height should scale");
        TEST_ASSERT(fs >= 1.3f, "2K font scale should be >= 1.3");
        TEST_ASSERT(sw > 490, "2K setting width should be > 490");
        TEST_ASSERT(sh > 750, "2K setting height should be > 750");

        // 4K (3840x2160)
        AppConfig::GetResolutionDefaults(3840, 2160, mw, mh, th, fs, ts, sw, sh);
        TEST_ASSERT(mw == 360, "4K main window width should be 360");
        TEST_ASSERT(mh == 700, "4K main window height should be 700");
        TEST_ASSERT(std::abs(fs - 2.0f) < 0.05f, "4K font scale should be 2.0");
        TEST_ASSERT(sw == 760, "4K setting width should be 760");
        TEST_ASSERT(sh == 1200, "4K setting height should be 1200");

        // Ultrawide (3440x1440) should use 16:9 equivalent to avoid inflating
        AppConfig::GetResolutionDefaults(3440, 1440, mw, mh, th, fs, ts, sw, sh);
        TEST_ASSERT(mw > 230 && mw < 250, "Ultrawide 1440p should use 16:9 equivalent width ratio");
    }
}

void TestMapDetector() {
    std::cout << "Running TestMapDetector..." << std::endl;
    MapDetector& detector = MapDetector::Instance();

    // 1. 升格之恋 (9 players, P6: Ji'nara, P8: Slayn Elemental)
    {
        std::vector<PlayerInfo> players(9);
        players[6].name = "吉娜拉";
        players[8].name = "斯雷恩元素生物";
        std::string res = detector.Identify(players);
        TEST_ASSERT(res == "升格之恋", "Should identify 升格之恋");

        // Test English
        players[6].name = "Ji'nara";
        players[8].name = "Slayn Elemental";
        TEST_ASSERT(detector.Identify(players) == "升格之恋", "Should identify 升格之恋 with English names");

        // Test Russian
        players[6].name = "Джи-нара";
        players[8].name = "Элементаль Слейна";
        TEST_ASSERT(detector.Identify(players) == "升格之恋", "Should identify 升格之恋 with Russian names");
    }

    // 2. 死亡摇篮 (6 players, P4: Amons Spezialkräfte, P5: Amons Spezialkräfte)
    {
        std::vector<PlayerInfo> players(6);
        players[4].name = "埃蒙的特战队";
        players[5].name = "埃蒙的特战队";
        TEST_ASSERT(detector.Identify(players) == "死亡摇篮", "Should identify 死亡摇篮");
    }

    // 3. 亡者之夜 (7 players, P4: Infested / 感染体, P5: Sensor Tower / 感应塔)
    {
        std::vector<PlayerInfo> players(7);
        players[4].name = "感染体";
        players[5].name = "感应塔";
        TEST_ASSERT(detector.Identify(players) == "亡者之夜", "Should identify 亡者之夜");
    }

    // 4. 天界封锁 (4 players, P2: Amon's Forces, P3: Amon's Forces)
    {
        std::vector<PlayerInfo> players(4);
        players[2].name = "埃蒙的部队";
        players[3].name = "埃蒙的部队";
        TEST_ASSERT(detector.Identify(players) == "天界封锁", "Should identify 天界封锁");
    }

    // 5. 净网行动 (6 players, P2: Purifier Hologram, P4: Megalith)
    {
        std::vector<PlayerInfo> players(6);
        players[2].name = "净化者全息投影";
        players[4].name = "麦加利斯";
        TEST_ASSERT(detector.Identify(players) == "净网行动", "Should identify 净网行动");
    }

    // 6. 营救矿工 (11 players, P7 & P9: Kel-Morian Miners)
    {
        std::vector<PlayerInfo> players(11);
        players[7].name = "凯莫瑞安矿工";
        players[9].name = "凯莫瑞安矿工";
        TEST_ASSERT(detector.Identify(players) == "营救矿工", "Should identify 营救矿工");
    }

    // 7. 机会渺茫 (6 players, P4: Terrazine, P5: Egon Stetmann)
    {
        std::vector<PlayerInfo> players(6);
        players[4].name = "地嗪";
        players[5].name = "艾贡·斯台特曼";
        TEST_ASSERT(detector.Identify(players) == "机会渺茫", "Should identify 机会渺茫");
    }

    // 8. 湮灭快车 (7 players, P5 & P6: Amon's Forces)
    {
        std::vector<PlayerInfo> players(7);
        players[5].name = "埃蒙的部队";
        players[6].name = "埃蒙的部队";
        TEST_ASSERT(detector.Identify(players) == "湮灭快车", "Should identify 湮灭快车");
    }

    // 9. 聚铁成兵 (6 players, P4: Balius, P5: Moebius Train)
    {
        std::vector<PlayerInfo> players(6);
        players[4].name = "巴利俄斯";
        players[5].name = "莫比斯列车";
        TEST_ASSERT(detector.Identify(players) == "聚铁成兵", "Should identify 聚铁成兵");
    }

    // 10. 克哈裂痕 (6 players, P4: Void Shard, P5: Pirates)
    {
        std::vector<PlayerInfo> players(6);
        players[4].name = "虚空碎片";
        players[5].name = "海盗";
        TEST_ASSERT(detector.Identify(players) == "克哈裂痕", "Should identify 克哈裂痕");
    }

    // 11. 黑暗杀星 (8 players, P6: Amon's Forces, P7: Evacuees)
    {
        std::vector<PlayerInfo> players(8);
        players[6].name = "埃蒙的部队";
        players[7].name = "待救者";
        TEST_ASSERT(detector.Identify(players) == "黑暗杀星", "Should identify 黑暗杀星");
    }

    // 12. 往日神庙-A & 往日神庙-B (9 players, P6: Temple, P8: Rocks)
    {
        std::vector<PlayerInfo> players(9);
        players[6].name = "神庙";
        players[8].name = "岩石";
        std::string resA = detector.Identify(players, "往日神庙-A");
        TEST_ASSERT(resA == "往日神庙-A", "Should identify 往日神庙-A with hint");
        std::string resB = detector.Identify(players, "往日神庙-B");
        TEST_ASSERT(resB == "往日神庙-B", "Should identify 往日神庙-B with hint");
    }

    // 13. 熔火危机 (8 players, P6: Molten Salamander, P7: Civilians)
    {
        std::vector<PlayerInfo> players(8);
        players[6].name = "熔岩巨蜥";
        players[7].name = "平民";
        TEST_ASSERT(detector.Identify(players) == "熔火危机", "Should identify 熔火危机");
    }

    // 14. 虚空降临 (7 players, P5: Warp Conduit, P6: Scientific Research Team)
    {
        std::vector<PlayerInfo> players(7);
        players[5].name = "时空航道";
        players[6].name = "科研队伍";
        TEST_ASSERT(detector.Identify(players) == "虚空降临", "Should identify 虚空降临");
    }

    // 15. 虚空撕裂-左 & 虚空撕裂-右 (6 players, P4: Amon's Forces, P5: Sgt. Hammer's Forces)
    {
        std::vector<PlayerInfo> players(6);
        players[4].name = "埃蒙的部队";
        players[5].name = "重锤军士的部队";
        std::string resL = detector.Identify(players, "虚空撕裂-左");
        TEST_ASSERT(resL == "虚空撕裂-左", "Should identify 虚空撕裂-左 with hint");
        std::string resR = detector.Identify(players, "虚空撕裂-右");
        TEST_ASSERT(resR == "虚空撕裂-右", "Should identify 虚空撕裂-右 with hint");
    }

    // Mismatched player count
    {
        std::vector<PlayerInfo> players(2); // 1v1 ladder
        TEST_ASSERT(detector.Identify(players).empty(), "2-player ladder game should return empty");
    }
}

void TestScheduleManager() {
    std::cout << "Running TestScheduleManager..." << std::endl;
    ScheduleManager& sm = ScheduleManager::Instance();

    // Time parsing
    TEST_ASSERT(sm.ParseTimeToSeconds("03:15") == 195, "03:15 should be 195 seconds");
    TEST_ASSERT(sm.ParseTimeToSeconds("0:45") == 45, "0:45 should be 45 seconds");
    TEST_ASSERT(sm.ParseTimeToSeconds("1:00:00") == 3600, "1:00:00 should be 3600 seconds");
    TEST_ASSERT(sm.FormatSecondsToTime(195) == "03:15", "195 seconds should format to 03:15");
    TEST_ASSERT(sm.FormatSecondsToTime(3665) == "01:01:05", "3665 seconds should format to 01:01:05");

    // Load Maps
    sm.LoadAllMaps("zh");
    const auto& maps = sm.GetMapList();
    TEST_ASSERT(maps.size() == 17, "Should load all 17 coop map schedules");

    // Check specific schedule (往日神庙-A)
    const auto& rows = sm.GetSchedule("往日神庙-A");
    TEST_ASSERT(!rows.empty(), "往日神庙-A schedule should have rows");
    TEST_ASSERT(rows[0].time_text == "3:00", "First event time should be 3:00");
    TEST_ASSERT(rows[0].tier == "T1", "First event tier should be T1");

    // Check hybrid parsing in 往日神庙-A row at 6:00
    bool found_hybrid = false;
    for (const auto& r : rows) {
        if (!r.hybrids.empty()) {
            found_hybrid = true;
            TEST_ASSERT(r.hybrids[0].first.find("hybrid") != std::string::npos, "Hybrid name should contain 'hybrid'");
            break;
        }
    }
    TEST_ASSERT(found_hybrid, "Should parse hybrid units in schedule rows");

    // Variant detection
    MapVariant varA;
    TEST_ASSERT(sm.GetVariant("往日神庙-A", varA), "往日神庙-A should have variant");
    TEST_ASSERT(varA.type == "AB", "Variant type should be AB");
    TEST_ASSERT(varA.current_suffix == "A", "Current suffix should be A");

    std::string switchedA = sm.SwitchVariant("往日神庙-A");
    TEST_ASSERT(switchedA == "往日神庙-B", "Switching 往日神庙-A should give 往日神庙-B");

    // Base name without suffix fallback
    std::string switchedBase = sm.SwitchVariant("往日神庙");
    TEST_ASSERT(switchedBase == "往日神庙-B", "Switching 往日神庙 (base) should give 往日神庙-B");

    MapVariant varL;
    TEST_ASSERT(sm.GetVariant("虚空撕裂-左", varL), "虚空撕裂-左 should have variant");
    TEST_ASSERT(varL.type == "LR", "Variant type should be LR");
    std::string switchedL = sm.SwitchVariant("虚空撕裂-左");
    TEST_ASSERT(switchedL == "虚空撕裂-右", "Switching 虚空撕裂-左 should give 虚空撕裂-右");

    std::string switchedBaseL = sm.SwitchVariant("虚空撕裂");
    TEST_ASSERT(switchedBaseL == "虚空撕裂-左" || switchedBaseL == "虚空撕裂-右", "Switching 虚空撕裂 (base) should cycle variant");

    // English mode and cross-language variant switching tests
    sm.LoadAllMaps("en");
    TEST_ASSERT(sm.GetMapList().size() == 17, "Should load 17 maps in English mode");
    TEST_ASSERT(sm.GetCanonicalMapId("升格之恋") == "chain_of_ascension", "升格之恋 should map to chain_of_ascension");
    TEST_ASSERT(sm.GetMapDisplayName("升格之恋", "en") == "Chain of Ascension", "升格之恋 in en should be Chain of Ascension");
    TEST_ASSERT(sm.GetMapDisplayName("Chain of Ascension", "zh") == "升格之恋", "Chain of Ascension in zh should be 升格之恋");

    // Auto-detected Chinese map name requesting English schedule
    const auto& en_rows = sm.GetSchedule("升格之恋");
    TEST_ASSERT(!en_rows.empty(), "Should get schedule for 升格之恋 even in en mode");

    // Switching variant in English mode
    std::string switchedEnA = sm.SwitchVariant("Temple of the Past-A");
    TEST_ASSERT(switchedEnA == "Temple of the Past-B", "Switching Temple of the Past-A should give Temple of the Past-B");
    std::string switchedEnFromZh = sm.SwitchVariant("往日神庙-A");
    TEST_ASSERT(switchedEnFromZh == "Temple of the Past-B", "Switching 往日神庙-A in en mode should switch to Temple of the Past-B");

    std::string switchedEnL = sm.SwitchVariant("Void Thrashing-Left");
    TEST_ASSERT(switchedEnL == "Void Thrashing-Right", "Switching Void Thrashing-Left should give Void Thrashing-Right");

    // Restore to zh
    sm.LoadAllMaps("zh");
}

void TestTroopManager() {
    std::cout << "Running TestTroopManager..." << std::endl;
    TroopManager& tm = TroopManager::Instance();
    tm.LoadAllTroops();

    const auto& list = tm.GetTroopList();
    TEST_ASSERT(!list.empty(), "Troop list should not be empty");

    // Test terran classic_infantry
    std::vector<std::string> t1 = tm.GetUnitsForTier("classic_infantry-旧世步兵团", 1);
    TEST_ASSERT(!t1.empty(), "Classic infantry T1 should have units");
    TEST_ASSERT(t1[0] == "marine", "T1 unit should be marine");

    std::vector<std::string> t3 = tm.GetUnitsForTier("classic_infantry-旧世步兵团", 3);
    TEST_ASSERT(t3.size() == 3, "Classic infantry T3 should have 3 units: marine, medic, firebat");

    std::string race = tm.GetRaceForTroop("classic_infantry-旧世步兵团");
    TEST_ASSERT(race == "terran", "Race should be terran");
}

void TestMutatorManager() {
    std::cout << "Running TestMutatorManager..." << std::endl;
    MutatorManager& mm = MutatorManager::Instance();
    mm.LoadAllMutators();

    TEST_ASSERT(mm.GetTypes().size() == 5, "There should be 5 mutator types");
    mm.SetMutatorEnabled("bombbots", true);
    TEST_ASSERT(mm.IsMutatorEnabled("bombbots"), "bombbots should be enabled");

    // In bombbots.txt, first spawn is 2:30 (150 seconds)
    // If current time is 125 seconds (25s before 150), it should alert!
    mm.ResetAlerts();
    mm.Update(125, 0.5f);
    const auto& alerts = mm.GetActiveAlerts();
    TEST_ASSERT(!alerts.empty(), "Should trigger active alert when within 30s");
    TEST_ASSERT(alerts[0].mutator_type == "bombbots", "Alert type should be bombbots");

    // Second update at 126s: should not duplicate alert for the same 150s point
    mm.Update(126, 0.5f);
    TEST_ASSERT(alerts.size() == 1, "Alert should not duplicate for the same time point");

    mm.SetMutatorEnabled("bombbots", false);
    TEST_ASSERT(!mm.IsMutatorEnabled("bombbots"), "bombbots should be disabled");

    // Test non-in-game alert suppression
    mm.ResetAlerts();
    mm.SetMutatorEnabled("bombbots", true);
    mm.Update(125, 0.5f, false); // is_in_game = false
    TEST_ASSERT(mm.GetActiveAlerts().empty(), "Non-in-game state must not trigger mutator alert");

    // Now become in-game: should trigger properly without being previously consumed
    mm.Update(125, 0.5f, true); // is_in_game = true
    TEST_ASSERT(!mm.GetActiveAlerts().empty(), "In-game state should trigger mutator alert");
    mm.SetMutatorEnabled("bombbots", false);
    mm.ResetAlerts();
}

void TestCommanderManager() {
    std::cout << "Running TestCommanderManager..." << std::endl;
    CommanderManager& cm = CommanderManager::Instance();
    cm.LoadConfig();

    const auto& grid = cm.GetAvatarGrid();
    TEST_ASSERT(grid.size() == 2, "Avatar grid should have 2 rows");
    TEST_ASSERT(grid[0].size() == 10, "Row 1 should have 10 commanders");
    TEST_ASSERT(grid[1].size() == 8, "Row 2 should have 8 commanders");

    // Check Raynor URLs
    std::string kr_url = cm.GetUrl("raynor", "KR");
    TEST_ASSERT(kr_url == "battlenet:://starcraft/map/3/148741", "Raynor KR url mismatch");
    std::string us_url = cm.GetUrl("raynor", "US");
    TEST_ASSERT(us_url == "battlenet:://starcraft/map/1/334863", "Raynor US url mismatch");

    // Check random picker with blacklist
    std::vector<std::string> blacklist = {
        "raynor", "kerrigan", "artanis", "swann", "zagara", "vorazun", "karax", "abathur", "alarak", "nova",
        "stukov", "fenix", "dehaka", "horner", "tychus", "zeratul", "stetmann"
    };
    // All blacklisted except mengsk
    std::string picked = cm.PickRandomCommander("KR", blacklist);
    TEST_ASSERT(picked == "mengsk", "Picked commander should be mengsk when others are blacklisted");

    // Replacement command format test
    std::string cmd = cm.GetReplacementCommand("raynor", "KR");
    TEST_ASSERT(cmd == "替换为raynor指挥官: battlenet:://starcraft/map/3/148741", "Replacement command format mismatch");

    // Clipboard test (attempts copy, gracefully handles headless non-desktop environment)
    bool copied = cm.CopyReplacementLink("raynor", "KR");
    if (!copied && GetLastError() == ERROR_ACCESS_DENIED) {
        std::cout << "  (Note: OpenClipboard returned ERROR_ACCESS_DENIED in headless WSL environment, command generation verified)" << std::endl;
        TEST_ASSERT(!cmd.empty(), "Command should be successfully generated");
    } else {
        TEST_ASSERT(copied, "CopyReplacementLink should succeed when clipboard is accessible");
    }
}

void TestTextureLoader() {
    std::cout << "Running TestTextureLoader (WIC decoding)..." << std::endl;
    // Test WIC decoding directly on the 6 WebP maps and PNG icons without needing full D3D device
    std::vector<std::string> maps = {
        "亡者之夜", "净网行动", "死亡摇篮", "湮灭快车", "熔火危机", "营救矿工"
    };

    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                          IID_IWICImagingFactory, (LPVOID*)&pFactory);
    TEST_ASSERT(SUCCEEDED(hr), "WIC ImagingFactory should create");

    if (pFactory) {
        for (const auto& m : maps) {
            std::string path = "resources/artifacts/" + m + ".webp";
            int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            std::wstring wpath(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

            IWICBitmapDecoder* pDecoder = nullptr;
            hr = pFactory->CreateDecoderFromFilename(wpath.c_str(), NULL, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &pDecoder);
            TEST_ASSERT(SUCCEEDED(hr), ("Failed to decode WebP artifact: " + path).c_str());

            if (pDecoder) {
                IWICBitmapFrameDecode* pFrame = nullptr;
                hr = pDecoder->GetFrame(0, &pFrame);
                TEST_ASSERT(SUCCEEDED(hr), "GetFrame 0 should succeed");
                if (pFrame) {
                    UINT w = 0, h = 0;
                    pFrame->GetSize(&w, &h);
                    TEST_ASSERT(w > 500 && h > 400, "WebP map should have valid dimensions");
                    pFrame->Release();
                }
                pDecoder->Release();
            }
        }

        // Test hybrid troop webp icons
        std::vector<std::string> hybrid_icons = {
            "hybridbehemoth", "hybriddestroyer", "hybridnemesis"
        };
        for (const auto& hi : hybrid_icons) {
            std::string path = "ico/troops/hybrid/" + hi + ".webp";
            int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
            std::wstring wpath(wlen, 0);
            MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

            IWICBitmapDecoder* pDecoder = nullptr;
            hr = pFactory->CreateDecoderFromFilename(wpath.c_str(), NULL, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &pDecoder);
            TEST_ASSERT(SUCCEEDED(hr), ("Failed to decode hybrid WebP: " + path).c_str());
            if (pDecoder) pDecoder->Release();
        }

        // Test ikun.png
        {
            IWICBitmapDecoder* pDecoder = nullptr;
            hr = pFactory->CreateDecoderFromFilename(L"ico/ikun.png", NULL, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &pDecoder);
            TEST_ASSERT(SUCCEEDED(hr), "Failed to decode ico/ikun.png");
            if (pDecoder) pDecoder->Release();
        }

        pFactory->Release();
    }
    CoUninitialize();
}

void TestArtifactViewer() {
    std::cout << "Running TestArtifactViewer..." << std::endl;
    ArtifactViewer& av = ArtifactViewer::Instance();

    // 6 Zeratul maps have artifacts
    TEST_ASSERT(av.HasArtifact("死亡摇篮"), "死亡摇篮 should have artifact");
    TEST_ASSERT(av.HasArtifact("亡者之夜"), "亡者之夜 should have artifact");
    TEST_ASSERT(av.HasArtifact("湮灭快车"), "湮灭快车 should have artifact");
    TEST_ASSERT(av.HasArtifact("净网行动"), "净网行动 should have artifact");
    TEST_ASSERT(av.HasArtifact("营救矿工"), "营救矿工 should have artifact");
    TEST_ASSERT(av.HasArtifact("熔火危机"), "熔火危机 should have artifact");

    // English names
    TEST_ASSERT(av.HasArtifact("Cradle of Death"), "Cradle of Death should have artifact");
    TEST_ASSERT(av.HasArtifact("Dead of Night"), "Dead of Night should have artifact");

    // Other maps MUST NOT have artifacts
    TEST_ASSERT(!av.HasArtifact("克哈裂痕"), "克哈裂痕 should NOT have artifact");
    TEST_ASSERT(!av.HasArtifact("往日神庙-A"), "往日神庙-A should NOT have artifact");
    TEST_ASSERT(!av.HasArtifact("往日神庙-B"), "往日神庙-B should NOT have artifact");
    TEST_ASSERT(!av.HasArtifact("天界封锁"), "天界封锁 should NOT have artifact");
    TEST_ASSERT(!av.HasArtifact("虚空撕裂-左"), "虚空撕裂-左 should NOT have artifact");
    TEST_ASSERT(!av.HasArtifact("黑暗杀星"), "黑暗杀星 should NOT have artifact");

    // Resolution: non-artifact maps MUST resolve to empty string (no fallback to default image!)
    TEST_ASSERT(av.ResolveArtifactMap("克哈裂痕").empty(), "ResolveArtifactMap on non-artifact map must return empty string");
    TEST_ASSERT(av.ResolveArtifactMap("往日神庙-A").empty(), "ResolveArtifactMap on non-artifact map must return empty string");
    TEST_ASSERT(av.ResolveArtifactMap("天界封锁").empty(), "ResolveArtifactMap on non-artifact map must return empty string");

    // Artifact maps resolve to canonical names
    TEST_ASSERT(av.ResolveArtifactMap("亡者之夜") == "亡者之夜", "ResolveArtifactMap 亡者之夜");
    TEST_ASSERT(av.ResolveArtifactMap("Dead of Night") == "亡者之夜", "ResolveArtifactMap Dead of Night");
    TEST_ASSERT(av.ResolveArtifactMap("死亡摇篮") == "死亡摇篮", "ResolveArtifactMap 死亡摇篮");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   SC2 Expo Native Verification Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    TestConfig();
    TestMapDetector();
    TestScheduleManager();
    TestTroopManager();
    TestMutatorManager();
    TestCommanderManager();
    TestTextureLoader();
    TestArtifactViewer();
    TestOverlayLifecycle();

    std::cout << "========================================" << std::endl;
    std::cout << "Results: " << g_tests_passed << "/" << g_tests_run << " tests passed." << std::endl;
    if (g_tests_passed == g_tests_run) {
        std::cout << "[SUCCESS] ALL TESTS PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "[FAILURE] SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}
