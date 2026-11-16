# -*- coding: utf-8 -*-
import subprocess
import time
import ctypes
from ctypes import wintypes
import sys
import os

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
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, ".."))
    exe_path = os.path.join(repo_root, "dist", "sc2_expo", "sc2_expo.exe")
    run_dir = os.path.join(repo_root, "dist", "sc2_expo")
    if not os.path.exists(exe_path):
        exe_path = os.path.join(repo_root, "sc2_expo.exe")
        run_dir = repo_root
    proc = subprocess.Popen([exe_path], cwd=run_dir)
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

        h_art = user32.FindWindowW("SC2ArtifactOverlayNative", "SC2 Artifact Viewer")
        if not h_art:
            print("[FAIL] Artifact window not found!")
            return False

        # Press Ctrl+Shift+\ to Show Artifact
        print("Triggering Ctrl+Shift+\\ to Show Artifact...")
        press_artifact_hotkey()
        time.sleep(0.8)

        live_non_artifact = False
        try:
            import urllib.request, json
            req = urllib.request.urlopen("http://127.0.0.1:6119/game", timeout=1)
            gdata = json.loads(req.read().decode('utf-8', errors='replace'))
            if gdata.get("players"):
                live_non_artifact = True
        except Exception:
            pass

        is_vis = user32.IsWindowVisible(h_art)
        print(f"Artifact window found (HWND={h_art}), IsVisible={bool(is_vis)}")

        if live_non_artifact:
            if is_vis:
                print("[FAIL] Current map has no artifact, but artifact overlay is visible!")
                return False
            print("[PASS] Verified: Non-artifact map strictly hides artifact overlay!")
            user32.PostMessageW(hwnd, 0x0010, 0, 0)
            proc.wait(timeout=5)
            return True

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
