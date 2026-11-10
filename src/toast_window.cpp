#include "toast_window.h"
#include "config.h"
#include "troop_manager.h"
#include <iostream>
#include <algorithm>

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

static LRESULT CALLBACK ToastWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT; // Always click-through!
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

ToastWindow& ToastWindow::Instance() {
    static ToastWindow instance;
    return instance;
}

void ToastWindow::Init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        ToastWndProc,
        0L, 0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"SC2ToastOverlayNative",
        nullptr
    };
    RegisterClassExW(&wc);

    // Create layered, click-through, tool window
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"SC2 Toast Overlay",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, hInstance, nullptr
    );
}

void ToastWindow::Shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hInstance) {
        UnregisterClassW(L"SC2ToastOverlayNative", m_hInstance);
        m_hInstance = nullptr;
    }
}

void ToastWindow::ShowToast(const ScheduleRow& row, const std::string& current_troop, float duration) {
    const auto& cfg = AppConfig::Instance();
    if (!cfg.toast_allowed) return;

    m_time_text = row.time_text;
    m_event_text = row.event_text;
    m_tier_text = row.tier;
    m_hybrid_icons = row.hybrids;
    m_unit_icons.clear();

    if (!row.tier.empty() && row.tier[0] == 'T') {
        try {
            int t_num = std::stoi(row.tier.substr(1));
            m_unit_icons = TroopManager::Instance().GetUnitsForTier(current_troop, t_num);
        } catch (...) {}
    }

    m_remaining_time = duration > 0.0f ? duration : (float)cfg.toast_duration_ms / 1000.0f;
    m_active = true;
    RenderToast();
}

void ToastWindow::ShowTestToast() {
    ScheduleRow test_row;
    test_row.time_text = "03:00";
    test_row.event_text = "进攻波次预警 (Toast 预览测试)";
    test_row.tier = "T1";
    ShowToast(test_row, "", 6.0f);
}

void ToastWindow::Hide() {
    m_active = false;
    m_remaining_time = 0.0f;
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void ToastWindow::Update(float delta_time) {
    if (!m_active) return;
    m_remaining_time -= delta_time;
    if (m_remaining_time <= 0.0f) {
        Hide();
    }
}

void ToastWindow::RenderToast() {
    if (!m_hwnd) return;
    const auto& cfg = AppConfig::Instance();

    float scale = (cfg.toast_scale > 0.3f && cfg.toast_scale < 3.0f) ? cfg.toast_scale : 1.0f;
    int base_w = 460;
    int base_h = 56;
    int width = (int)((float)base_w * scale);
    int height = (int)((float)base_h * scale);

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int toast_x = (screen_w - width) / 2;
    int toast_y = (int)((float)screen_h * cfg.toast_position_y);

    // Clamp inside screen bounds
    if (toast_x < 10) toast_x = 10;
    if (toast_y < 10) toast_y = 10;
    if (toast_y + height > screen_h - 10) toast_y = screen_h - height - 10;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    HGDIOBJ hOldBmp = SelectObject(hdcMem, hBitmap);

    // 1. Fill smooth rounded dark background with alpha into pBits
    uint32_t* pixels = (uint32_t*)pBits;
    uint8_t alpha = 210; // ~82% opacity
    uint8_t r = 16, g = 20, b = 28; // Deep navy slate
    uint32_t premul_bg = (alpha << 24) |
                         ((uint8_t)((r * alpha) / 255) << 16) |
                         ((uint8_t)((g * alpha) / 255) << 8) |
                         ((uint8_t)((b * alpha) / 255));

    uint8_t border_alpha = 240;
    uint8_t br = 0, bg_c = 191, bb = 255; // Bright cyan border
    uint32_t premul_border = (border_alpha << 24) |
                            ((uint8_t)((br * border_alpha) / 255) << 16) |
                            ((uint8_t)((bg_c * border_alpha) / 255) << 8) |
                            ((uint8_t)((bb * border_alpha) / 255));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool is_border = (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2);
            pixels[y * width + x] = is_border ? premul_border : premul_bg;
        }
    }

    // 2. Draw text using GDI
    SetBkMode(hdcMem, TRANSPARENT);

    int font_h = (int)(24.0f * scale);
    HFONT hFont = CreateFontW(
        font_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HGDIOBJ hOldFont = SelectObject(hdcMem, hFont);

    // Compose display text
    std::string text_str = "[ " + m_time_text + " ]  " + m_event_text;
    if (!m_tier_text.empty()) {
        text_str += "  [" + m_tier_text + "]";
    }
    std::wstring wtext = Utf8ToWide(text_str);

    RECT rcText = { 15, 0, width - 15, height };
    SetTextColor(hdcMem, RGB(0, 220, 255)); // Bright cyan
    DrawTextW(hdcMem, wtext.c_str(), -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_CENTER);

    // Ensure drawn GDI text has proper non-zero alpha in the 32-bit DIB
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint32_t val = pixels[y * width + x];
            if ((val & 0xFF000000) == 0 && (val & 0x00FFFFFF) != 0) {
                pixels[y * width + x] = 0xFF000000 | val;
            }
        }
    }

    // 3. Update layered window
    POINT ptPos = { toast_x, toast_y };
    SIZE sizeWnd = { width, height };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    SetWindowPos(m_hwnd, HWND_TOPMOST, toast_x, toast_y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptPos, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);

    // Cleanup
    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}
