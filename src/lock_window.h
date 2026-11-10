#pragma once

#include <windows.h>

class LockWindow {
public:
    static LockWindow& Instance();

    void Init(HINSTANCE hInstance, HWND hParent);
    void Shutdown();

    void UpdatePosition(int main_x, int main_y);
    void SetLocked(bool locked);
    bool IsLocked() const { return m_locked; }
    void Show(bool show);
    HWND GetHwnd() const { return m_hwnd; }

private:
    LockWindow() = default;
    ~LockWindow() { Shutdown(); }

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void OnPaint(HDC hdc);

    HWND m_hwnd = nullptr;
    HWND m_hParent = nullptr;
    HINSTANCE m_hInstance = nullptr;
    bool m_locked = true;
    bool m_hovered = false;
    HFONT m_hFont = nullptr;
};
