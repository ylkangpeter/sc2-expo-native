#include "artifact_viewer.h"
#include "config.h"
#include <wincodec.h>
#include <vector>
#include <iostream>
#include <algorithm>

static const std::vector<std::string> kArtifactMaps = {
    "死亡摇篮", "湮灭快车", "净网行动", "营救矿工", "亡者之夜", "熔火危机"
};

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring wstr(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], len);
    if (!wstr.empty() && wstr.back() == L'\0') wstr.pop_back();
    return wstr;
}

static LRESULT CALLBACK ArtifactWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTTRANSPARENT; // Always click-through!
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

ArtifactViewer& ArtifactViewer::Instance() {
    static ArtifactViewer instance;
    return instance;
}

bool ArtifactViewer::HasArtifact(const std::string& map_name) const {
    if (map_name.empty()) return false;
    for (const auto& m : kArtifactMaps) {
        if (map_name.find(m) != std::string::npos) return true;
    }
    if (map_name.find("Cradle of Death") != std::string::npos) return true;
    if (map_name.find("Oblivion Express") != std::string::npos) return true;
    if (map_name.find("Malwarfare") != std::string::npos) return true;
    if (map_name.find("Miner Evacuation") != std::string::npos) return true;
    if (map_name.find("Dead of Night") != std::string::npos) return true;
    if (map_name.find("Vermillion") != std::string::npos) return true;
    return false;
}

std::string ArtifactViewer::ResolveArtifactMap(const std::string& input_map) const {
    if (!input_map.empty()) {
        for (const auto& m : kArtifactMaps) {
            if (input_map.find(m) != std::string::npos) return m;
        }
        if (input_map.find("Cradle of Death") != std::string::npos) return "死亡摇篮";
        if (input_map.find("Oblivion Express") != std::string::npos) return "湮灭快车";
        if (input_map.find("Malwarfare") != std::string::npos) return "净网行动";
        if (input_map.find("Miner Evacuation") != std::string::npos) return "营救矿工";
        if (input_map.find("Dead of Night") != std::string::npos) return "亡者之夜";
        if (input_map.find("Vermillion") != std::string::npos) return "熔火危机";
    }
    // Fallback: if input map doesn't have an artifact, return last viewed or default to 死亡摇篮
    return m_last_map.empty() ? kArtifactMaps[0] : m_last_map;
}

void ArtifactViewer::ShowNextMap() {
    int idx = 0;
    for (int i = 0; i < (int)kArtifactMaps.size(); ++i) {
        if (kArtifactMaps[i] == m_last_map) {
            idx = (i + 1) % (int)kArtifactMaps.size();
            break;
        }
    }
    ShowMap(kArtifactMaps[idx]);
}

void ArtifactViewer::Init(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        ArtifactWndProc,
        0L, 0L,
        hInstance,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"SC2ArtifactOverlayNative",
        nullptr
    };
    RegisterClassExW(&wc);

    // Create layered, click-through, tool window
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"SC2 Artifact Viewer",
        WS_POPUP,
        0, 0, 1, 1,
        nullptr, nullptr, hInstance, nullptr
    );
}

void ArtifactViewer::Shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hInstance) {
        UnregisterClassW(L"SC2ArtifactOverlayNative", m_hInstance);
        m_hInstance = nullptr;
    }
}

void ArtifactViewer::ShowMap(const std::string& map_name) {
    if (!m_hwnd) return;
    std::string target = ResolveArtifactMap(map_name);
    if (RenderToLayeredWindow(target)) {
        m_visible = true;
        m_last_map = target;
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    }
}

void ArtifactViewer::Hide() {
    if (!m_hwnd) return;
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);
}

void ArtifactViewer::Toggle(const std::string& map_name) {
    if (m_visible) {
        // If current map has an artifact and it's different from the one currently shown, switch to it!
        if (HasArtifact(map_name)) {
            std::string target = ResolveArtifactMap(map_name);
            if (target != m_last_map) {
                ShowMap(target);
                return;
            }
        }
        Hide();
    } else {
        ShowMap(map_name);
    }
}

bool ArtifactViewer::RenderToLayeredWindow(const std::string& map_name) {
    const auto& cfg = AppConfig::Instance();
    std::string target = ResolveArtifactMap(map_name);
    std::string webp_path = "resources/artifacts/" + target + ".webp";

    std::wstring wpath = Utf8ToWide(webp_path);
    if (GetFileAttributesW(wpath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        target = kArtifactMaps[0];
        webp_path = "resources/artifacts/" + target + ".webp";
        wpath = Utf8ToWide(webp_path);
    }
    if (GetFileAttributesW(wpath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IWICImagingFactory, (LPVOID*)&pFactory);
    if (FAILED(hr)) {
        return false;
    }

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromFilename(wpath.c_str(), NULL, GENERIC_READ,
                                            WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        pFactory->Release();
        return false;
    }

    IWICBitmapFrameDecode* pFrame = nullptr;
    hr = pDecoder->GetFrame(0, &pFrame);
    if (FAILED(hr)) {
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    UINT orig_w = 0, orig_h = 0;
    pFrame->GetSize(&orig_w, &orig_h);

    UINT target_w = (UINT)((float)orig_w * cfg.artifact_scale_ratio);
    UINT target_h = (UINT)((float)orig_h * cfg.artifact_scale_ratio);
    if (target_w < 10) target_w = 10;
    if (target_h < 10) target_h = 10;

    // Scaler
    IWICBitmapScaler* pScaler = nullptr;
    hr = pFactory->CreateBitmapScaler(&pScaler);
    if (FAILED(hr)) {
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }
    pScaler->Initialize(pFrame, target_w, target_h, WICBitmapInterpolationModeFant);

    // Format Converter to 32bppPBGRA (Premultiplied BGRA)
    IWICFormatConverter* pConverter = nullptr;
    hr = pFactory->CreateFormatConverter(&pConverter);
    if (FAILED(hr)) {
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }
    pConverter->Initialize(pScaler, GUID_WICPixelFormat32bppPBGRA,
                           WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

    // Create DIB section
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = target_w;
    bmi.bmiHeader.biHeight = -((LONG)target_h); // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBitmap || !pBits) {
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        pConverter->Release();
        pScaler->Release();
        pFrame->Release();
        pDecoder->Release();
        pFactory->Release();
        return false;
    }

    SelectObject(hdcMem, hBitmap);
    pConverter->CopyPixels(NULL, target_w * 4, target_w * target_h * 4, (BYTE*)pBits);

    // Draw stylish green border around the map
    uint32_t* pixels = (uint32_t*)pBits;
    uint32_t border_color = 0xFF00FF80; // Alpha 255, Green/Cyan
    int border_thickness = 3;
    for (UINT y = 0; y < target_h; ++y) {
        for (UINT x = 0; x < target_w; ++x) {
            if (x < (UINT)border_thickness || x >= target_w - border_thickness ||
                y < (UINT)border_thickness || y >= target_h - border_thickness) {
                pixels[y * target_w + x] = border_color;
            }
        }
    }

    // Overlay ikun marker if position exists
    auto it = cfg.artifact_positions.find(target);
    if (it != cfg.artifact_positions.end()) {
        IWICBitmapDecoder* pIkunDecoder = nullptr;
        if (SUCCEEDED(pFactory->CreateDecoderFromFilename(L"ico/ikun.png", NULL, GENERIC_READ,
                                                         WICDecodeMetadataCacheOnDemand, &pIkunDecoder))) {
            IWICBitmapFrameDecode* pIkunFrame = nullptr;
            if (SUCCEEDED(pIkunDecoder->GetFrame(0, &pIkunFrame))) {
                UINT ikun_w = (UINT)it->second.width;
                UINT ikun_h = (UINT)it->second.height;
                if (ikun_w <= 0) ikun_w = 40;
                if (ikun_h <= 0) ikun_h = 40;

                IWICBitmapScaler* pIkunScaler = nullptr;
                if (SUCCEEDED(pFactory->CreateBitmapScaler(&pIkunScaler))) {
                    pIkunScaler->Initialize(pIkunFrame, ikun_w, ikun_h, WICBitmapInterpolationModeFant);

                    IWICFormatConverter* pIkunConv = nullptr;
                    if (SUCCEEDED(pFactory->CreateFormatConverter(&pIkunConv))) {
                        pIkunConv->Initialize(pIkunScaler, GUID_WICPixelFormat32bppPBGRA,
                                              WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

                        std::vector<BYTE> ikun_buf(ikun_w * ikun_h * 4);
                        if (SUCCEEDED(pIkunConv->CopyPixels(NULL, ikun_w * 4, (UINT)ikun_buf.size(), ikun_buf.data()))) {
                            int start_x = (int)(target_w * it->second.x_ratio);
                            int start_y = (int)(target_h * it->second.y_ratio);

                            const uint32_t* ikun_pix = (const uint32_t*)ikun_buf.data();
                            for (UINT iy = 0; iy < ikun_h; ++iy) {
                                int dy = start_y + (int)iy;
                                if (dy < 0 || dy >= (int)target_h) continue;

                                for (UINT ix = 0; ix < ikun_w; ++ix) {
                                    int dx = start_x + (int)ix;
                                    if (dx < 0 || dx >= (int)target_w) continue;

                                    uint32_t src = ikun_pix[iy * ikun_w + ix];
                                    BYTE sa = (BYTE)(src >> 24);
                                    if (sa == 0) continue;

                                    if (sa == 255) {
                                        pixels[dy * target_w + dx] = src;
                                    } else {
                                        // Alpha blend with background
                                        uint32_t dst = pixels[dy * target_w + dx];
                                        BYTE da = (BYTE)(dst >> 24);
                                        BYTE sr = (BYTE)(src >> 16);
                                        BYTE sg = (BYTE)(src >> 8);
                                        BYTE sb = (BYTE)(src);
                                        BYTE dr = (BYTE)(dst >> 16);
                                        BYTE dg = (BYTE)(dst >> 8);
                                        BYTE db = (BYTE)(dst);

                                        BYTE out_r = sr + dr * (255 - sa) / 255;
                                        BYTE out_g = sg + dg * (255 - sa) / 255;
                                        BYTE out_b = sb + db * (255 - sa) / 255;
                                        BYTE out_a = sa + da * (255 - sa) / 255;

                                        pixels[dy * target_w + dx] = ((uint32_t)out_a << 24) |
                                                                     ((uint32_t)out_r << 16) |
                                                                     ((uint32_t)out_g << 8) |
                                                                     (uint32_t)out_b;
                                    }
                                }
                            }
                        }
                        pIkunConv->Release();
                    }
                    pIkunScaler->Release();
                }
                pIkunFrame->Release();
            }
            pIkunDecoder->Release();
        }
    }

    // Screen Center Position
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int center_x = (screen_w - (int)target_w) / 2;
    int center_y = (screen_h - (int)target_h) / 2;

    POINT ptDst = { center_x, center_y };
    SIZE sizeWnd = { (LONG)target_w, (LONG)target_h };
    POINT ptSrc = { 0, 0 };

    BYTE opacity_byte = (BYTE)(std::clamp(cfg.artifact_opacity, 0.1f, 1.0f) * 255.0f);
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, opacity_byte, AC_SRC_ALPHA };

    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);
    SetWindowPos(m_hwnd, HWND_TOPMOST, center_x, center_y, target_w, target_h, SWP_NOACTIVATE);

    // Clean up
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);

    pConverter->Release();
    pScaler->Release();
    pFrame->Release();
    pDecoder->Release();
    pFactory->Release();

    return true;
}
