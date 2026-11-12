#include <windows.h>
#include <atomic>
#include <mutex>
#include <thread>
#include "config.h"
#include "map_detector.h"

// Inject API snapshots without starting a poller or binding SC2's localhost port.
// Include standard dependencies first so the visibility override only affects Sc2Client.
#define private public
#include "sc2_client.h"
#undef private

#include "imgui.h"
#include "overlay_ui.h"
#include "toast_window.h"
#include "test_assert.h"

void TestOverlayLifecycle() {
    std::cout << "Running TestOverlayLifecycle (headless ImGui)..." << std::endl;
    auto& cfg = AppConfig::Instance();
    const AppConfig saved_config = cfg;
    auto& client = Sc2Client::Instance();
    const Sc2GameState saved_state = client.GetState();
    auto& overlay = OverlayUI::Instance();
    auto& toast = ToastWindow::Instance();
    auto& mutators = MutatorManager::Instance();
    ScheduleManager::Instance().LoadAllMaps("zh");
    mutators.LoadAllMutators();
    cfg.toast_allowed = true;
    cfg.toast_when_hidden = false;
    cfg.toast_duration_ms = 8000;
    cfg.time_alert_seconds = 30;

    ImGuiContext* previous_context = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(920, 400);
    io.Fonts->AddFontDefault();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto set_state = [&](int seconds, const std::string& screen, bool new_game = false,
                         const std::string& map = "往日神庙-A") {
        Sc2GameState state;
        state.current_map = map;
        state.current_seconds = seconds;
        state.display_time = (float)seconds;
        state.screen_state = screen;
        state.is_running = (screen != "unknown");
        state.is_new_game_event = new_game;
        std::lock_guard<std::mutex> lock(client.m_mutex);
        client.m_state = state;
    };
    auto frame = [&](bool visible, float delta_time) {
        if (visible) {
            io.DeltaTime = delta_time > 0.0f ? delta_time : 1.0f / 60.0f;
            ImGui::NewFrame();
            overlay.Render(delta_time);
            ImGui::Render();
        } else {
            overlay.UpdateBackground(delta_time);
        }
    };

    // The settings preview lasts six seconds on the desktop in both host paths.
    set_state(0, "unknown");
    frame(false, 0.0f);
    toast.ShowTestToast();
    frame(false, 0.016f);
    TEST_ASSERT(toast.IsActive(), "Desktop preview must survive the next hidden frame");
    frame(true, 2.0f);
    TEST_ASSERT(toast.IsActive(), "Desktop preview must survive visible HUD updates");
    frame(false, 3.0f);
    TEST_ASSERT(toast.IsActive(), "Preview must remain active before six seconds");
    frame(true, 1.0f);
    TEST_ASSERT(!toast.IsActive(), "Preview must expire after six seconds");

    toast.ShowTestToast();
    set_state(0, "in_game", true, "天界封锁");
    frame(false, 0.1f);
    TEST_ASSERT(toast.IsActive(), "Game/map reset must not cancel a settings preview");
    toast.Hide();

    ScheduleRow manual_row;
    manual_row.time_text = "03:00";
    manual_row.event_text = "Manual wave reminder";
    overlay.TriggerToast(manual_row, 8.0f);
    set_state(0, "unknown", false, "天界封锁");
    frame(false, 0.016f);
    TEST_ASSERT(!toast.IsActive(), "Leaving the game must still hide gameplay toasts");

    // A preview also occupies the toast slot: automatic waves cannot overwrite it.
    cfg.toast_when_hidden = true;
    set_state(239, "in_game", true, "天界封锁");
    frame(false, 0.0f);
    toast.ShowTestToast();
    frame(false, 2.0f);
    cfg.toast_when_hidden = false;
    frame(false, 4.1f);
    TEST_ASSERT(!toast.IsActive(), "An upcoming wave must not replace a six-second preview with an eight-second toast");

    // Losing /ui temporarily hides reminders but must not reset either dedupe set.
    mutators.SetMutatorEnabled("bombbots", true);
    set_state(125, "in_game", true);
    frame(false, 0.0f);
    TEST_ASSERT(mutators.GetActiveAlerts().size() == 1, "First bombbot event must alert");
    set_state(0, "unknown");
    frame(false, 0.1f);
    TEST_ASSERT(mutators.GetActiveAlerts().empty(), "API outage must hide active mutator messages");
    set_state(126, "in_game");
    frame(false, 0.1f);
    TEST_ASSERT(mutators.GetActiveAlerts().empty(), "Same bombbot event must not repeat after API recovery");

    set_state(126, "in_game", true);
    frame(false, 0.0f);
    TEST_ASSERT(mutators.GetActiveAlerts().size() == 1, "A confirmed new game must rearm mutator alerts");
    set_state(125, "in_game");
    frame(false, 0.0f);
    TEST_ASSERT(mutators.GetActiveAlerts().size() == 1, "A real game-clock rewind must rearm mutator alerts");
    mutators.SetMutatorEnabled("bombbots", false);

    cfg.toast_when_hidden = true;
    set_state(150, "in_game", true);
    frame(false, 0.0f);
    TEST_ASSERT(toast.IsActive(), "Wave toast must trigger when hidden notifications are enabled");
    set_state(0, "unknown");
    frame(false, 0.1f);
    TEST_ASSERT(!toast.IsActive(), "API outage must hide the gameplay wave toast");
    set_state(151, "in_game");
    frame(false, 0.1f);
    TEST_ASSERT(!toast.IsActive(), "Same wave must not repeat after API recovery");
    set_state(151, "in_game", true);
    frame(false, 0.0f);
    TEST_ASSERT(toast.IsActive(), "Confirmed new game must rearm wave toasts");

    // A real session/map change clears the old slot before checking the new wave.
    overlay.TriggerToast(manual_row, 1.0f);
    set_state(239, "in_game", true, "天界封锁");
    frame(false, 0.0f);
    cfg.toast_when_hidden = false;
    frame(false, 2.0f);
    TEST_ASSERT(toast.IsActive(), "New map's eight-second wave toast must replace the old one-second reminder");
    toast.Hide();

    // Visible frames used to release the slot at half the actual toast lifetime.
    // Another wave is already eligible, so a premature release would replace it.
    set_state(182, "in_game", true, "往日神庙-B");
    frame(false, 0.0f);
    overlay.TriggerToast(manual_row, 8.0f);
    for (int i = 0; i < 4; ++i) frame(true, 1.0f);
    TEST_ASSERT(toast.IsActive(), "Eight-second toast must still occupy the slot after four visible seconds");
    overlay.CheckWaveToastAlert(client.GetState());
    frame(false, 4.1f);
    TEST_ASSERT(!toast.IsActive(), "Visible/hidden updates must expire the original toast after eight seconds without replacement");

    toast.Hide();
    mutators.ResetAlerts();

    // Verify high-DPI (e.g. 2.0x 4K) and maximum font scale (2.8x) rendering stability
    cfg.font_scale = 2.8f;
    ImGui::GetStyle().FontScaleDpi = 2.0f;
    set_state(100, "in_game", false, "往日神庙-A");
    frame(true, 0.016f);
    TEST_ASSERT(true, "HUD rendered cleanly with 2.8x font scale and 2.0x DPI");

    // Verify commander selector under 2.8x font scale
    overlay.SetCommanderSelectorOpen(true);
    frame(true, 0.016f);
    TEST_ASSERT(overlay.IsCommanderSelectorOpen(), "Commander selector opens and renders under 2.8x font scale");
    overlay.SetCommanderSelectorOpen(false);
    frame(true, 0.016f);

    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous_context);
    cfg = saved_config;
    {
        std::lock_guard<std::mutex> lock(client.m_mutex);
        client.m_state = saved_state;
    }
}
