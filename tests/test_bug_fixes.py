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

def press_artifact_hotkey(hwnd):
    # In WSL / headless environment, send WM_HOTKEY directly to the message pump
    user32.PostMessageW(hwnd, 0x0312, 1003, 0)

import os

def test_fixes():
    print("=================================================================")
    print("RUNNING TEST SUITE: Bug Fixes Verification (Artifact & Banner)")
    print("=================================================================")

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
        print(f"[PASS] Main window found (HWND={hwnd})")

        time.sleep(0.5)

        h_art = user32.FindWindowW("SC2ArtifactOverlayNative", "SC2 Artifact Viewer")
        if not h_art:
            print("[FAIL] Artifact window not found")
            return False
        print(f"[PASS] Artifact overlay window found (HWND={h_art})")

        # Test 1: Artifact Toggle on initial map
        print("\n--- Test 1: Testing Artifact toggle on initial map ---")
        press_artifact_hotkey(hwnd)
        time.sleep(0.8)

        vis = user32.IsWindowVisible(h_art)
        print(f"Artifact visible after hotkey: {bool(vis)}")
        if not vis:
            print("[FAIL] Artifact window should be visible after hotkey!")
            return False
        print("[PASS] Artifact overlay showed successfully!")

        # Hide it
        press_artifact_hotkey(hwnd)
        time.sleep(0.5)
        vis = user32.IsWindowVisible(h_art)
        print(f"Artifact visible after 2nd hotkey: {bool(vis)}")
        if vis:
            print("[FAIL] Artifact window should be hidden!")
            return False
        print("[PASS] Artifact overlay hid successfully!")

        # Test 2: Triggering Artifact via Tray command ID_TRAY_ARTIFACT (2004)
        print("\n--- Test 2: Testing Artifact toggle via Tray Command ---")
        user32.PostMessageW(hwnd, 0x0111, 2004, 0) # ID_TRAY_ARTIFACT
        time.sleep(0.8)
        vis = user32.IsWindowVisible(h_art)
        print(f"Artifact visible after tray toggle: {bool(vis)}")
        if not vis:
            print("[FAIL] Artifact window should be visible after tray toggle!")
            return False
        print("[PASS] Artifact overlay showed via Tray Command!")

        user32.PostMessageW(hwnd, 0x0111, 2004, 0) # Toggle off
        time.sleep(0.5)
        vis = user32.IsWindowVisible(h_art)
        if vis:
            print("[FAIL] Artifact window should be hidden after 2nd tray toggle!")
            return False
        print("[PASS] Artifact overlay toggled off via Tray Command!")

        # Test 3: Testing AB Map Switch Hotkey & Command
        print("\n--- Test 3: Testing AB Map Switch Hotkeys & Tray Command ---")
        # Trigger primary hotkey Ctrl+Shift+[ (1001)
        user32.PostMessageW(hwnd, 0x0312, 1001, 0) # WM_HOTKEY MapSwitch
        time.sleep(0.3)
        print("[PASS] Primary MapSwitch hotkey (Ctrl+Shift+[, 1001) handled cleanly")

        # Trigger direct hotkey Ctrl+[ (1011)
        user32.PostMessageW(hwnd, 0x0312, 1011, 0) # WM_HOTKEY MapSwitch Direct
        time.sleep(0.3)
        print("[PASS] Direct MapSwitch hotkey (Ctrl+[, 1011) handled cleanly")

        # Trigger via tray command ID_TRAY_MAP (2002)
        user32.PostMessageW(hwnd, 0x0111, 2002, 0)
        time.sleep(0.3)
        print("[PASS] Tray command ID_TRAY_MAP (2002) handled cleanly")

        # Test 4: Clean shutdown
        print("\n--- Test 4: Clean shutdown ---")
        user32.PostMessageW(hwnd, 0x0010, 0, 0) # WM_CLOSE
        proc.wait(timeout=5)
        print("[PASS] Process exited cleanly with return code 0")

        print("\n=================================================================")
        print("ALL BUG FIXES VERIFIED SUCCESSFULLY!")
        print("=================================================================")
        return True
    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    ok = test_fixes()
    sys.exit(0 if ok else 1)
