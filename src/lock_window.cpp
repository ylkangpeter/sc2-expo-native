#include "lock_window.h"
#include "overlay_ui.h"
#include <windows.h>
#include <iostream>

LockWindow& LockWindow::Instance() {
    static LockWindow instance;
    return instance;
}

LRESULT CALLBACK LockWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LockWindow& self = LockWindow::Instance();

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        self.OnPaint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SETCURSOR: {
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    }
    case WM_MOUSEMOVE: {
        if (!self.m_hovered) {
            self.m_hovered = true;
            InvalidateRect(hwnd, nullptr, FALSE);
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        self.m_hovered = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        OverlayUI::Instance().ToggleLock();
        self.SetLocked(OverlayUI::Instance().IsLocked());
        return 0;
    }
    case WM_DESTROY: {
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void LockWindow::Init(HINSTANCE hInstance, HWND hParent) {
    m_hInstance = hInstance;
    m_hParent = hParent;

    WNDCLASSEXW wc = {
        sizeof(wc),
        CS_CLASSDC,
        WndProc,
        0L, 0L,
        hInstance,
        nullptr,
        LoadCursor(nullptr, IDC_HAND),
        nullptr,
        nullptr,
        L"SC2LockWindowNative",
        nullptr
    };
    RegisterClassExW(&wc);

    // Initial position above the main window
    RECT parent_rect = { 0, 300, 175, 630 };
    if (hParent) {
        GetWindowRect(hParent, &parent_rect);
    }
    int lock_x = parent_rect.left;
    int lock_y = (parent_rect.top >= 30) ? (parent_rect.top - 28) : (parent_rect.top);

    // CRITICAL: NEVER include WS_EX_TRANSPARENT here!
    // This window MUST remain clickable even when the main HUD is in click-through mode!
    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        wc.lpszClassName,
        L"SC2 Lock Button",
        WS_POPUP | WS_VISIBLE,
        lock_x, lock_y, 28, 28,
        nullptr, nullptr, hInstance, nullptr
    );

    if (m_hwnd) {
        SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);
    }
}

void LockWindow::Shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_hInstance) {
        UnregisterClassW(L"SC2LockWindowNative", m_hInstance);
        m_hInstance = nullptr;
    }
}

void LockWindow::UpdatePosition(int main_x, int main_y) {
    if (!m_hwnd) return;
    int lock_x = main_x;
    int lock_y = (main_y >= 30) ? (main_y - 28) : main_y;
    SetWindowPos(m_hwnd, HWND_TOPMOST, lock_x, lock_y, 28, 28,
                 SWP_NOACTIVATE | SWP_NOZORDER);
}

void LockWindow::SetLocked(bool locked) {
    m_locked = locked;
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void LockWindow::Show(bool show) {
    if (!m_hwnd) return;
    ShowWindow(m_hwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
}

void LockWindow::OnPaint(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Create double-buffer memory DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
    HGDIOBJ oldBitmap = SelectObject(memDC, memBitmap);

    // 1. Draw rounded background
    COLORREF bg_color = m_hovered ? RGB(45, 52, 62) : RGB(25, 28, 34);
    HBRUSH bgBrush = CreateSolidBrush(bg_color);
    COLORREF border_color = m_hovered ? RGB(80, 100, 130) : RGB(45, 50, 60);
    HPEN borderPen = CreatePen(PS_SOLID, 1, border_color);

    HGDIOBJ oldBrush = SelectObject(memDC, bgBrush);
    HGDIOBJ oldPen = SelectObject(memDC, borderPen);

    RoundRect(memDC, 0, 0, rect.right, rect.bottom, 6, 6);

    // 2. Draw Vector Lock Icon
    if (m_locked) {
        // Locked: Amber/Gold lock with closed shackle
        HPEN shacklePen = CreatePen(PS_SOLID, 2, RGB(240, 180, 50));
        HBRUSH bodyBrush = CreateSolidBrush(RGB(220, 160, 40));

        SelectObject(memDC, shacklePen);
        // Closed shackle (curved arch from x=9 to x=19, top=5)
        Arc(memDC, 8, 4, 20, 16, 20, 10, 8, 10);
        MoveToEx(memDC, 8, 10, nullptr);
        LineTo(memDC, 8, 13);
        MoveToEx(memDC, 20, 10, nullptr);
        LineTo(memDC, 20, 13);

        // Lock body (x=6 to x=22, y=12 to y=23)
        SelectObject(memDC, bodyBrush);
        HPEN bodyPen = CreatePen(PS_SOLID, 1, RGB(250, 200, 70));
        SelectObject(memDC, bodyPen);
        RoundRect(memDC, 6, 12, 22, 23, 3, 3);

        // Keyhole
        HBRUSH keyholeBrush = CreateSolidBrush(RGB(30, 20, 10));
        SelectObject(memDC, keyholeBrush);
        Ellipse(memDC, 13, 14, 16, 17);
        MoveToEx(memDC, 14, 17, nullptr);
        LineTo(memDC, 14, 20);

        DeleteObject(shacklePen);
        DeleteObject(bodyBrush);
        DeleteObject(bodyPen);
        DeleteObject(keyholeBrush);
    } else {
        // Unlocked: Green lock with open shackle
        HPEN shacklePen = CreatePen(PS_SOLID, 2, RGB(50, 205, 120));
        HBRUSH bodyBrush = CreateSolidBrush(RGB(40, 180, 100));

        SelectObject(memDC, shacklePen);
        // Open shackle (raised and shifted)
        Arc(memDC, 8, 2, 20, 14, 20, 8, 8, 8);
        MoveToEx(memDC, 8, 8, nullptr);
        LineTo(memDC, 8, 13);

        // Lock body
        SelectObject(memDC, bodyBrush);
        HPEN bodyPen = CreatePen(PS_SOLID, 1, RGB(70, 230, 140));
        SelectObject(memDC, bodyPen);
        RoundRect(memDC, 6, 12, 22, 23, 3, 3);

        // Keyhole
        HBRUSH keyholeBrush = CreateSolidBrush(RGB(15, 40, 25));
        SelectObject(memDC, keyholeBrush);
        Ellipse(memDC, 13, 14, 16, 17);
        MoveToEx(memDC, 14, 17, nullptr);
        LineTo(memDC, 14, 20);

        DeleteObject(shacklePen);
        DeleteObject(bodyBrush);
        DeleteObject(bodyPen);
        DeleteObject(keyholeBrush);
    }

    // Copy to screen DC
    BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);

    // Cleanup
    SelectObject(memDC, oldBrush);
    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBitmap);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);
    DeleteObject(memBitmap);
    DeleteDC(memDC);
}
