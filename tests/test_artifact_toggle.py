# -*- coding: utf-8 -*-
"""Check the artifact shortcut and overlay on a controlled debug map."""
import ctypes
from ctypes import wintypes
import subprocess
import sys
import time

from test_bug_fixes import isolated_debug_app, user32

VK_CONTROL = 0x11
VK_SHIFT = 0x10
VK_OEM_5 = 0xDC
KEYEVENTF_KEYUP = 0x0002


def press_artifact_hotkey():
    for key in (VK_CONTROL, VK_SHIFT, VK_OEM_5):
        user32.keybd_event(key, 0, 0, 0)
    time.sleep(0.05)
    for key in (VK_OEM_5, VK_SHIFT, VK_CONTROL):
        user32.keybd_event(key, 0, KEYEVENTF_KEYUP, 0)


def test_artifact_toggle():
    temp_dir, run_dir, exe_path = isolated_debug_app(default_artifact_hotkey=True)
    proc = None
    try:
        proc = subprocess.Popen([exe_path], cwd=run_dir)
        hwnd = 0
        for _ in range(50):
            candidate = user32.FindWindowW("SC2ExpoNativeOverlay", "SC2 Expo Native")
            owner = wintypes.DWORD()
            if candidate:
                user32.GetWindowThreadProcessId(candidate, ctypes.byref(owner))
                if owner.value == proc.pid:
                    hwnd = candidate
                    break
            time.sleep(0.1)
        if not hwnd:
            print("[FAIL] Main window for test process not found")
            return False
        time.sleep(0.8)  # Allow debug mode to select the non-artifact map.
        artifact = user32.FindWindowW("SC2ArtifactOverlayNative", "SC2 Artifact Viewer")
        owner = wintypes.DWORD()
        if artifact:
            user32.GetWindowThreadProcessId(artifact, ctypes.byref(owner))
        if not artifact or owner.value != proc.pid:
            print("[FAIL] Artifact window for test process not found")
            return False
        if not (user32.GetWindowLongW(artifact, -20) & 0x20):
            print("[FAIL] Artifact window must be click-through")
            return False
        # A competing registration would make this test unable to exercise the app.
        if user32.RegisterHotKey(None, 42, 0x0002 | 0x0004, VK_OEM_5):
            user32.UnregisterHotKey(None, 42)
            print("[FAIL] Default artifact shortcut was not registered")
            return False
        press_artifact_hotkey()
        time.sleep(0.5)
        if proc.poll() is not None or user32.IsWindowVisible(artifact):
            print("[FAIL] Artifact overlay appeared on the non-artifact debug map")
            return False
        print("[PASS] Artifact window is click-through and stays hidden on the debug map")
        user32.PostMessageW(hwnd, 0x0010, 0, 0)
        proc.wait(timeout=5)
        return proc.returncode == 0
    finally:
        if proc is not None and proc.poll() is None:
            proc.kill()
            proc.wait()
        temp_dir.cleanup()


if __name__ == "__main__":
    sys.exit(0 if test_artifact_toggle() else 1)
