import subprocess
import time
import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

VK_CONTROL = 0x11
VK_SHIFT = 0x10
VK_OEM_5 = 0xDC # '\' key
KEYEVENTF_KEYUP = 0x0002

def press_artifact_hotkey():
    user32.keybd_event(VK_CONTROL, 0, 0, 0)
    user32.keybd_event(VK_SHIFT, 0, 0, 0)
    user32.keybd_event(VK_OEM_5, 0, 0, 0)
    time.sleep(0.05)
    user32.keybd_event(VK_OEM_5, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_SHIFT, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)

def test_artifact_toggle():
    print("Launching sc2_expo.exe for artifact overlay verification...")
    proc = subprocess.Popen(["sc2_expo.exe"])
    try:
        hwnd = 0
        for _ in range(50):
            hwnd = user32.FindWindowW("SC2ExpoNativeOverlay", "SC2 Expo Native")
            if hwnd:
                break
            time.sleep(0.1)
        if not hwnd:
            print("[FAIL] Main window not found")
            return False

        time.sleep(0.5)

        # Press Ctrl+Shift+\ to Show Artifact
        print("Triggering Ctrl+Shift+\\ to Show Artifact...")
        press_artifact_hotkey()
        time.sleep(0.8)

        h_art = user32.FindWindowW("SC2ArtifactOverlayNative", "SC2 Artifact Viewer")
        if not h_art:
            print("[FAIL] Artifact window not found after hotkey!")
            return False

        is_vis = user32.IsWindowVisible(h_art)
        print(f"Artifact window found (HWND={h_art}), IsVisible={bool(is_vis)}")
        if not is_vis:
            print("[FAIL] Artifact window should be visible")
            return False

        # Verify it has WS_EX_TRANSPARENT
        ex = user32.GetWindowLongW(h_art, -20)
        if not (ex & 0x00000020):
            print("[FAIL] Artifact window should have WS_EX_TRANSPARENT set!")
            return False
        print("[PASS] Artifact window is visible and 100% click-through (WS_EX_TRANSPARENT)!")

        # Press Ctrl+Shift+\ to Hide Artifact
        print("Triggering Ctrl+Shift+\\ to Hide Artifact...")
        press_artifact_hotkey()
        time.sleep(0.5)

        is_vis2 = user32.IsWindowVisible(h_art)
        print(f"Artifact window after 2nd hotkey, IsVisible={bool(is_vis2)}")
        if is_vis2:
            print("[FAIL] Artifact window should be hidden")
            return False

        print("[PASS] Artifact overlay successfully toggles show/hide via Ctrl+Shift+\\!")
        user32.PostMessageW(hwnd, 0x0010, 0, 0)
        proc.wait(timeout=5)
        return True
    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    ok = test_artifact_toggle()
    sys.exit(0 if ok else 1)
