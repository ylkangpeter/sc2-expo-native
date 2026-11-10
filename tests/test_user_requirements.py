import os
import sys
import time
import subprocess
import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32

# Win32 Constants
GWL_EXSTYLE = -20
WS_EX_TRANSPARENT = 0x00000020
WS_EX_LAYERED = 0x00080000

WM_USER = 0x0400
WM_TRAYICON = WM_USER + 101  # Defined in main.cpp: (WM_USER + 101)
WM_CLOSE = 0x0010
WM_LBUTTONDOWN = 0x0201
WM_LBUTTONUP = 0x0202
MK_LBUTTON = 0x0001
MOUSEEVENTF_LEFTDOWN = 0x0002
MOUSEEVENTF_LEFTUP = 0x0004

class RECT(ctypes.Structure):
    _fields_ = [
        ("left", wintypes.LONG),
        ("top", wintypes.LONG),
        ("right", wintypes.LONG),
        ("bottom", wintypes.LONG),
    ]

def wait_for_window(class_name, window_name=None, timeout=6.0, poll_interval=0.1):
    start = time.time()
    while time.time() - start < timeout:
        hwnd = user32.FindWindowW(class_name, window_name)
        if hwnd:
            return hwnd
        time.sleep(poll_interval)
    return 0

def wait_for_condition(cond_fn, timeout=4.0, poll_interval=0.1):
    start = time.time()
    while time.time() - start < timeout:
        if cond_fn():
            return True
        time.sleep(poll_interval)
    return False

def run_tests():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, ".."))
    exe_path = os.path.join(repo_root, "dist", "sc2_expo", "sc2_expo.exe")
    run_dir = os.path.join(repo_root, "dist", "sc2_expo")
    if not os.path.exists(exe_path):
        exe_path = os.path.join(repo_root, "sc2_expo.exe")
        run_dir = repo_root

    if not os.path.exists(exe_path):
        print(f"[ERROR] Executable not found at {exe_path}")
        return False

    print("=" * 65)
    print("RUNNING TEST SUITE: User Requirements Verification")
    print(f"Target binary: {exe_path}")
    print("=" * 65)

    print("\n[Step 1] Launching sc2_expo.exe...")
    proc = subprocess.Popen([exe_path], cwd=run_dir)

    try:
        # 1. Locate Main Window
        print("Waiting for main window 'SC2ExpoNativeOverlay'...")
        main_hwnd = wait_for_window("SC2ExpoNativeOverlay", "SC2 Expo Native", timeout=6.0)
        if not main_hwnd:
            main_hwnd = wait_for_window("SC2ExpoNativeOverlay", None, timeout=2.0)
        if not main_hwnd:
            print("[FAIL] Could not find main window (class='SC2ExpoNativeOverlay')!")
            return False
        print(f"[PASS] Found main window HWND: {main_hwnd} (0x{main_hwnd:08X})")

        # Allow UI message loop to initialize geometry and state
        time.sleep(0.6)

        # 2. Check Dimensions: Height <= 350, Width <= 190
        print("\n[Step 2] Verifying compact window dimensions (width <= 190, height <= 350)...")
        rect = RECT()
        user32.GetWindowRect(main_hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top
        print(f"Main window bounds: Left={rect.left}, Top={rect.top}, Right={rect.right}, Bottom={rect.bottom} => Width={w}px, Height={h}px")

        if w > 190:
            print(f"[FAIL] Window width ({w}px) exceeds maximum allowed 190px!")
            return False
        if h > 350:
            print(f"[FAIL] Window height ({h}px) exceeds maximum allowed 350px!")
            return False
        print(f"[PASS] Window dimensions compact: Width={w}px (<= 190), Height={h}px (<= 350)")

        # 3. Check Initial Click-Through (WS_EX_TRANSPARENT)
        print("\n[Step 3] Checking initial click-through state (default locked with WS_EX_TRANSPARENT)...")
        ex_style = user32.GetWindowLongW(main_hwnd, GWL_EXSTYLE)
        is_transparent = bool(ex_style & WS_EX_TRANSPARENT)
        print(f"Main window extended style: 0x{ex_style:08X}, WS_EX_TRANSPARENT={is_transparent}")
        if not is_transparent:
            print("[FAIL] Initial state must have WS_EX_TRANSPARENT set (click-through locked by default)!")
            return False
        print("[PASS] Initial state has WS_EX_TRANSPARENT set (default locked).")

        # 4. Check LockWindow exists, is visible, and does NOT have WS_EX_TRANSPARENT
        print("\n[Step 4] Checking LockWindow ('SC2LockWindowNative')...")
        lock_hwnd = wait_for_window("SC2LockWindowNative", None, timeout=5.0)
        if not lock_hwnd:
            print("[FAIL] Lock window 'SC2LockWindowNative' not found!")
            return False
        print(f"[PASS] Found LockWindow HWND: {lock_hwnd} (0x{lock_hwnd:08X})")

        lock_visible = bool(user32.IsWindowVisible(lock_hwnd))
        print(f"LockWindow IsWindowVisible: {lock_visible}")
        if not lock_visible:
            print("[FAIL] LockWindow must be visible!")
            return False
        print("[PASS] LockWindow is visible.")

        lock_ex_style = user32.GetWindowLongW(lock_hwnd, GWL_EXSTYLE)
        lock_is_transparent = bool(lock_ex_style & WS_EX_TRANSPARENT)
        print(f"LockWindow extended style: 0x{lock_ex_style:08X}, WS_EX_TRANSPARENT={lock_is_transparent}")
        if lock_is_transparent:
            print("[FAIL] LockWindow must NOT have WS_EX_TRANSPARENT so it remains clickable during click-through!")
            return False
        print("[PASS] LockWindow does NOT have WS_EX_TRANSPARENT (clickable).")

        lock_rect = RECT()
        user32.GetWindowRect(lock_hwnd, ctypes.byref(lock_rect))
        print(f"LockWindow bounds: Left={lock_rect.left}, Top={lock_rect.top}, Right={lock_rect.right}, Bottom={lock_rect.bottom}")

        # 5. Send click / WM_LBUTTONUP to LockWindow -> Verify Main Window Unlocks
        print("\n[Step 5] Clicking LockWindow to unlock main window...")
        # Send WM_LBUTTONDOWN and WM_LBUTTONUP to lock window
        user32.PostMessageW(lock_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0)
        time.sleep(0.05)
        user32.PostMessageW(lock_hwnd, WM_LBUTTONUP, 0, 0)

        def is_unlocked():
            st = user32.GetWindowLongW(main_hwnd, GWL_EXSTYLE)
            return not bool(st & WS_EX_TRANSPARENT)

        unlocked = wait_for_condition(is_unlocked, timeout=2.5)
        if not unlocked:
            # Fallback 1: Try SendMessageW
            print("PostMessage did not trigger unlock immediately, trying SendMessageW...")
            user32.SendMessageW(lock_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, 0)
            user32.SendMessageW(lock_hwnd, WM_LBUTTONUP, 0, 0)
            unlocked = wait_for_condition(is_unlocked, timeout=2.0)

        if not unlocked:
            # Fallback 2: Physical cursor click
            print("Trying physical mouse_event on LockWindow...")
            cx = (lock_rect.left + lock_rect.right) // 2
            cy = (lock_rect.top + lock_rect.bottom) // 2
            user32.SetCursorPos(cx, cy)
            time.sleep(0.05)
            user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
            time.sleep(0.05)
            user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
            unlocked = wait_for_condition(is_unlocked, timeout=2.0)

        ex_after = user32.GetWindowLongW(main_hwnd, GWL_EXSTYLE)
        print(f"Main window extended style after lock click: 0x{ex_after:08X}, WS_EX_TRANSPARENT={bool(ex_after & WS_EX_TRANSPARENT)}")
        if not unlocked:
            print("[FAIL] Main window failed to unlock after clicking LockWindow!")
            return False
        print("[PASS] Main window unlocked successfully (WS_EX_TRANSPARENT cleared).")

        # 6. Send WM_TRAYICON with WM_LBUTTONUP -> Verify Main Window Hides
        print("\n[Step 6] Sending WM_TRAYICON with WM_LBUTTONUP to toggle hide...")
        user32.PostMessageW(main_hwnd, WM_TRAYICON, 1, WM_LBUTTONUP)

        def is_hidden():
            return not bool(user32.IsWindowVisible(main_hwnd))

        hidden = wait_for_condition(is_hidden, timeout=3.0)
        if not hidden:
            print("PostMessage WM_TRAYICON not received or processed, trying SendMessageW...")
            user32.SendMessageW(main_hwnd, WM_TRAYICON, 1, WM_LBUTTONUP)
            hidden = wait_for_condition(is_hidden, timeout=2.0)

        vis_after_hide = bool(user32.IsWindowVisible(main_hwnd))
        print(f"Main window visibility after tray click: IsWindowVisible={vis_after_hide}")
        if not hidden:
            print("[FAIL] Main window did not hide after tray icon click!")
            return False
        print("[PASS] Main window successfully hidden (IsWindowVisible == False).")

        # 7. Send WM_TRAYICON with WM_LBUTTONUP again -> Verify Main Window Shows
        print("\n[Step 7] Sending WM_TRAYICON with WM_LBUTTONUP to toggle show...")
        user32.PostMessageW(main_hwnd, WM_TRAYICON, 1, WM_LBUTTONUP)

        def is_visible():
            return bool(user32.IsWindowVisible(main_hwnd))

        shown = wait_for_condition(is_visible, timeout=3.0)
        if not shown:
            print("PostMessage WM_TRAYICON not processed, trying SendMessageW...")
            user32.SendMessageW(main_hwnd, WM_TRAYICON, 1, WM_LBUTTONUP)
            shown = wait_for_condition(is_visible, timeout=2.0)

        vis_after_show = bool(user32.IsWindowVisible(main_hwnd))
        print(f"Main window visibility after second tray click: IsWindowVisible={vis_after_show}")
        if not shown:
            print("[FAIL] Main window did not show after second tray icon click!")
            return False
        print("[PASS] Main window successfully shown (IsWindowVisible == True).")

        # 8. Clean Exit via WM_CLOSE
        print("\n[Step 8] Sending WM_CLOSE to main window for clean shutdown...")
        user32.PostMessageW(main_hwnd, WM_CLOSE, 0, 0)

        try:
            exit_code = proc.wait(timeout=5.0)
            print(f"[PASS] Process exited cleanly with return code {exit_code}.")
        except subprocess.TimeoutExpired:
            print("[FAIL] Process did not terminate within 5 seconds after WM_CLOSE!")
            proc.kill()
            return False

        print("\n" + "=" * 65)
        print("ALL USER REQUIREMENTS VERIFIED SUCCESSFULLY!")
        print("=" * 65)
        return True

    finally:
        if proc.poll() is None:
            print("Cleaning up process...")
            proc.kill()

if __name__ == "__main__":
    success = run_tests()
    sys.exit(0 if success else 1)
