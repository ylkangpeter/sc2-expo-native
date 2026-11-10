#include "mutator_alert_window.h"
#include "config.h"
#include <wincodec.h>
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

static LRESULT CALLBACK MutatorAlertWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT; // Click-through
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

struct CachedIcon {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> pixels; // Premultiplied BGRA
};

static std::unordered_map<std::string, CachedIcon> g_iconCache;

static bool LoadIconWIC(const std::string& path, int target_w, int target_h, CachedIcon& out_icon) {
    std::wstring wpath = Utf8ToWide(path);
    IWICImagingFactory* pFactory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory, (LPVOID*)&pFactory))) {
        return false;
    }
    IWICBitmapDecoder* pDecoder = nullptr;
    if (FAILED(pFactory->CreateDecoderFromFilename(wpath.c_str(), NULL, GENERIC_READ,
                                                  WICDecodeMetadataCacheOnDemand, &pDecoder))) {
        pFactory->Release();
        return false;
    }
    IWICBitmapFrameDecode* pFrame = nullptr;
    if (FAILED(pDecoder->GetFrame(0, &pFrame))) {
        pDecoder->Release();
        pFactory->Release();
        return false;
    }
    IWICBitmapScaler* pScaler = nullptr;
    if (FAILED(pFactory->CreateBitmapScaler(&pScaler))) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }
    pScaler->Initialize(pFrame, target_w, target_h, WICBitmapInterpolationModeCubic);

    IWICFormatConverter* pConverter = nullptr;
    if (FAILED(pFactory->CreateFormatConverter(&pConverter))) {
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }
    pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                           nullptr, 0.0, WICBitmapPaletteTypeCustom);

    out_icon.width = target_w;
    out_icon.height = target_h;
    out_icon.pixels.resize(target_w * target_h);
    pConverter->CopyPixels(nullptr, target_w * 4, target_w * target_h * 4, (BYTE*)out_icon.pixels.data());

    pConverter->Release();
    pScaler->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();
    return true;
}

MutatorAlertWindow& MutatorAlertWindow::Instance() {
    static MutatorAlertWindow instance;
    return instance;
}

void MutatorAlertWindow::Init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        MutatorAlertWndProc,
        0L, 0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"SC2MutatorAlertNative",
        nullptr
    };
    RegisterClassExW(&wc);

    const std::vector<std::string> types = { "deployment", "propagator", "voidrifts", "killbots", "bombbots" };
    const std::vector<float> default_ratios = { 0.20f, 0.35f, 0.50f, 0.65f, 0.80f };

    m_slots.resize(types.size());
    for (size_t i = 0; i < types.size(); ++i) {
        m_slots[i].type = types[i];
        m_slots[i].x_ratio = default_ratios[i];
        m_slots[i].active = false;
        m_slots[i].hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            wc.lpszClassName,
            L"SC2 Mutator Toast",
            WS_POPUP,
            0, 0, 1, 1,
            nullptr, nullptr, hInstance, nullptr
        );
        m_type_to_slot[types[i]] = (int)i;

        // Preload icon
        std::string icon_path = "ico/mutator/" + types[i] + ".png";
        CachedIcon cicon;
        if (LoadIconWIC(icon_path, 28, 28, cicon)) {
            g_iconCache[types[i]] = cicon;
        }
    }
}

void MutatorAlertWindow::Shutdown() {
    for (auto& slot : m_slots) {
        if (slot.hwnd) {
            DestroyWindow(slot.hwnd);
            slot.hwnd = nullptr;
        }
    }
    m_slots.clear();
    m_type_to_slot.clear();
    g_iconCache.clear();

    if (m_hInstance) {
        UnregisterClassW(L"SC2MutatorAlertNative", m_hInstance);
        m_hInstance = nullptr;
    }
}

void MutatorAlertWindow::ShowAlert(const std::string& type, const std::string& message, float duration, float x_ratio) {
    auto it = m_type_to_slot.find(type);
    if (it == m_type_to_slot.end()) return;

    int idx = it->second;
    m_slots[idx].message = message;
    m_slots[idx].time_remaining = duration > 0.0f ? duration : 6.0f;
    m_slots[idx].x_ratio = x_ratio;
    m_slots[idx].active = true;

    RenderSlot(idx);
}

void MutatorAlertWindow::HideAlert(const std::string& type) {
    auto it = m_type_to_slot.find(type);
    if (it == m_type_to_slot.end()) return;

    int idx = it->second;
    m_slots[idx].active = false;
    m_slots[idx].time_remaining = 0.0f;
    if (m_slots[idx].hwnd) {
        ShowWindow(m_slots[idx].hwnd, SW_HIDE);
    }
}

void MutatorAlertWindow::HideAll() {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        m_slots[i].active = false;
        m_slots[i].time_remaining = 0.0f;
        if (m_slots[i].hwnd) {
            ShowWindow(m_slots[i].hwnd, SW_HIDE);
        }
    }
}

void MutatorAlertWindow::Update(float delta_time) {
    for (size_t i = 0; i < m_slots.size(); ++i) {
        if (!m_slots[i].active) continue;
        m_slots[i].time_remaining -= delta_time;
        if (m_slots[i].time_remaining <= 0.0f) {
            m_slots[i].active = false;
            if (m_slots[i].hwnd) {
                ShowWindow(m_slots[i].hwnd, SW_HIDE);
            }
        }
    }
}

void MutatorAlertWindow::ShowTestAlerts() {
    ShowAlert("deployment", "30s : 第一波部署", 6.0f, 0.20f);
    ShowAlert("propagator", "30s : 传播者出现", 6.0f, 0.35f);
    ShowAlert("voidrifts",  "30s : 虚空裂隙", 6.0f, 0.50f);
    ShowAlert("killbots",   "30s : 杀戮机器人", 6.0f, 0.65f);
    ShowAlert("bombbots",   "30s : 炸弹机器人", 6.0f, 0.80f);
}

void MutatorAlertWindow::RenderSlot(int slot_idx) {
    if (slot_idx < 0 || slot_idx >= (int)m_slots.size()) return;
    auto& slot = m_slots[slot_idx];
    if (!slot.hwnd) return;

    int width = 250;
    int height = 48;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int toast_x = (int)((float)screen_w * slot.x_ratio) - width / 2;
    int toast_y = (int)((float)screen_h * 0.70f);

    if (toast_x < 10) toast_x = 10;
    if (toast_x + width > screen_w - 10) toast_x = screen_w - width - 10;
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

    // 1. Fill dark translucent background with mint-green border
    uint32_t* pixels = (uint32_t*)pBits;
    uint8_t bg_alpha = 215;
    uint8_t bg_r = 16, bg_g = 20, bg_b = 28;
    uint32_t premul_bg = (bg_alpha << 24) |
                         ((uint8_t)((bg_r * bg_alpha) / 255) << 16) |
                         ((uint8_t)((bg_g * bg_alpha) / 255) << 8) |
                         ((uint8_t)((bg_b * bg_alpha) / 255));

    uint8_t border_alpha = 240;
    uint8_t b_r = 0, b_g = 255, b_b = 128; // Mint green mutator alert color
    uint32_t premul_border = (border_alpha << 24) |
                             ((uint8_t)((b_r * border_alpha) / 255) << 16) |
                             ((uint8_t)((b_g * border_alpha) / 255) << 8) |
                             ((uint8_t)((b_b * border_alpha) / 255));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool is_border = (x <= 1 || x >= width - 2 || y <= 1 || y >= height - 2);
            pixels[y * width + x] = is_border ? premul_border : premul_bg;
        }
    }

    // 2. Draw icon if cached
    auto icon_it = g_iconCache.find(slot.type);
    if (icon_it != g_iconCache.end()) {
        const auto& icon = icon_it->second;
        int icon_x = 10;
        int icon_y = (height - icon.height) / 2;
        for (int iy = 0; iy < icon.height; ++iy) {
            for (int ix = 0; ix < icon.width; ++ix) {
                int tx = icon_x + ix;
                int ty = icon_y + iy;
                if (tx >= 0 && tx < width && ty >= 0 && ty < height) {
                    uint32_t src = icon.pixels[iy * icon.width + ix];
                    uint8_t sa = (src >> 24) & 0xFF;
                    if (sa == 0) continue;
                    uint32_t dst = pixels[ty * width + tx];
                    uint32_t inv_sa = 255 - sa;
                    uint8_t sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
                    uint8_t da = (dst >> 24) & 0xFF, dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
                    uint8_t out_a = sa + (da * inv_sa + 127) / 255;
                    uint8_t out_r = sr + (dr * inv_sa + 127) / 255;
                    uint8_t out_g = sg + (dg * inv_sa + 127) / 255;
                    uint8_t out_b = sb + (db * inv_sa + 127) / 255;
                    pixels[ty * width + tx] = (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }

    // 3. Draw text using GDI
    SetBkMode(hdcMem, TRANSPARENT);
    HFONT hFont = CreateFontW(
        20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );
    HGDIOBJ hOldFont = SelectObject(hdcMem, hFont);

    SetTextColor(hdcMem, RGB(0, 255, 128));
    RECT rcText = { 46, 0, width - 10, height };
    std::wstring wmsg = Utf8ToWide(slot.message);
    DrawTextW(hdcMem, wmsg.c_str(), -1, &rcText, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    // Fix alpha on GDI text pixels
    for (int y = 0; y < height; ++y) {
        for (int x = 46; x < width; ++x) {
            uint32_t val = pixels[y * width + x];
            if ((val & 0xFF000000) == 0 && (val & 0x00FFFFFF) != 0) {
                pixels[y * width + x] = 0xFF000000 | val;
            }
        }
    }

    // 4. Update Layered Window
    POINT ptPos = { toast_x, toast_y };
    SIZE sizeWnd = { width, height };
    POINT ptSrc = { 0, 0 };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    SetWindowPos(slot.hwnd, HWND_TOPMOST, toast_x, toast_y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    UpdateLayeredWindow(slot.hwnd, hdcScreen, &ptPos, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    ShowWindow(slot.hwnd, SW_SHOWNOACTIVATE);

    // Cleanup
    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}
