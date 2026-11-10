import subprocess
import time
import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

VK_CONTROL = 0x11
VK_SHIFT = 0x10
VK_OEM_6 = 0xDD # ']' key
KEYEVENTF_KEYUP = 0x0002

def press_hotkey():
    user32.keybd_event(VK_CONTROL, 0, 0, 0)
    user32.keybd_event(VK_SHIFT, 0, 0, 0)
    user32.keybd_event(VK_OEM_6, 0, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(VK_OEM_6, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)

def test_lock_toggle():
    print("Launching sc2_expo.exe for lock toggle verification...")
    proc = subprocess.Popen(["sc2_expo.exe"])
    try:
        hwnd = 0
        for _ in range(50):
            hwnd = user32.FindWindowW("SC2ExpoNativeOverlay", "SC2 Expo Native")
            if hwnd:
                break
            time.sleep(0.1)
        if not hwnd:
            print("[FAIL] Window not found")
            return False

        GWL_EXSTYLE = -20
        WS_EX_TRANSPARENT = 0x00000020

        # Initial: locked (click-through by default)
        time.sleep(0.5)
        ex = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        is_trans_0 = (ex & WS_EX_TRANSPARENT) != 0
        print(f"Initial: WS_EX_TRANSPARENT = {is_trans_0}")
        if not is_trans_0:
            print("[FAIL] Initial state should be locked (WS_EX_TRANSPARENT=True)")
            return False

        # Press Ctrl+Shift+] to Unlock
        print("Triggering Ctrl+Shift+] to Unlock...")
        press_hotkey()
        time.sleep(0.5)
        ex = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        is_trans_1 = (ex & WS_EX_TRANSPARENT) != 0
        print(f"After 1st toggle: WS_EX_TRANSPARENT = {is_trans_1}")
        if is_trans_1:
            print("[FAIL] Expected unlocked state (WS_EX_TRANSPARENT=False)")
            return False

        # Press Ctrl+Shift+] to Lock again
        print("Triggering Ctrl+Shift+] to Lock again...")
        press_hotkey()
        time.sleep(0.5)
        ex = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        is_trans_2 = (ex & WS_EX_TRANSPARENT) != 0
        print(f"After 2nd toggle: WS_EX_TRANSPARENT = {is_trans_2}")
        if not is_trans_2:
            print("[FAIL] Expected locked state (WS_EX_TRANSPARENT=True)")
            return False

        print("[PASS] Global hotkey Ctrl+Shift+] successfully toggles click-through lock state!")
        user32.PostMessageW(hwnd, 0x0010, 0, 0)
        proc.wait(timeout=5)
        return True
    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    ok = test_lock_toggle()
    sys.exit(0 if ok else 1)
