# -*- coding: utf-8 -*-
import subprocess
import time
import ctypes
from ctypes import wintypes
import sys
import os

user32 = ctypes.windll.user32

class RECT(ctypes.Structure):
    _fields_ = [
        ("left", wintypes.LONG),
        ("top", wintypes.LONG),
        ("right", wintypes.LONG),
        ("bottom", wintypes.LONG)
    ]

def test_commander_toggle():
    print("Launching sc2_expo.exe for commander drawer verification...")
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
        rect = RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        w0 = rect.right - rect.left
        print(f"Initial HUD Width: {w0}")

        # Send ID_TRAY_COMMANDER (2005) to open commander selector
        print("Sending ID_TRAY_COMMANDER to open commander drawer...")
        user32.SendMessageW(hwnd, 0x0111, 2005, 0) # WM_COMMAND
        time.sleep(0.5)

        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        w1 = rect.right - rect.left
        print(f"Expanded Window Width: {w1}")
        if w1 < 800:
            print(f"[FAIL] Expected expanded width >= 800, got {w1}")
            return False

        # Send ID_TRAY_COMMANDER again to close
        print("Sending ID_TRAY_COMMANDER to collapse commander drawer...")
        user32.SendMessageW(hwnd, 0x0111, 2005, 0)
        time.sleep(0.5)

        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        w2 = rect.right - rect.left
        print(f"Collapsed Window Width: {w2}")
        if abs(w2 - w0) > 2:
            print(f"[FAIL] Expected collapsed width {w0}, got {w2}")
            return False

        print("[PASS] Commander selector drawer expands and collapses cleanly!")
        user32.PostMessageW(hwnd, 0x0010, 0, 0)
        proc.wait(timeout=5)
        return True
    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    ok = test_commander_toggle()
    sys.exit(0 if ok else 1)
