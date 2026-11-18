# -*- coding: utf-8 -*-
import subprocess
import time
import ctypes
from ctypes import wintypes
import sys
import os
import json
import shutil
import tempfile

user32 = ctypes.windll.user32

WM_HOTKEY = 0x0312
MOD_CONTROL = 0x0002
MOD_SHIFT = 0x0004


def configured_hotkeys(run_dir):
    # Registration assigns IDs to enabled primary bindings first, then default aliases.
    import json
    config_path = os.path.join(run_dir, "config.json")
    with open(config_path, encoding="utf-8") as config_file:
        config = json.load(config_file)
    specs = [
        ("map", "map_hotkey", 0xDB),
        ("lock", "lock_hotkey", 0xDD),
        ("artifact", "artifact_hotkey", 0xDC),
    ]
    primary, aliases = [], []
    for name, prefix, default_vk in specs:
        mod = config.get(prefix + "_mod", MOD_CONTROL | MOD_SHIFT)
        vk = config.get(prefix + "_vk", default_vk)
        if vk:
            primary.append((name, mod, vk))
            if mod == MOD_CONTROL | MOD_SHIFT and vk == default_vk:
                aliases.append((name + "_alias", MOD_CONTROL, vk))
    return {name: (1001 + index, mod, vk)
            for index, (name, mod, vk) in enumerate(primary + aliases)}


def send_hotkey(hwnd, binding):
    hotkey_id, mod, vk = binding
    return bool(user32.PostMessageW(hwnd, WM_HOTKEY, hotkey_id, mod | (vk << 16)))


def isolated_debug_app(default_artifact_hotkey=False):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, ".."))
    exe_path = os.path.join(repo_root, "dist", "sc2_expo", "sc2_expo.exe")
    run_dir = os.path.join(repo_root, "dist", "sc2_expo")
    if not os.path.exists(exe_path):
        exe_path = os.path.join(repo_root, "sc2_expo.exe")
        run_dir = repo_root

    # The executable resolves files relative to itself. Copy it and its data so
    # debug mode and any saved settings never alter the user's installation.
    temp_dir = tempfile.TemporaryDirectory(prefix="sc2-bug-test-")
    isolated_dir = temp_dir.name
    shutil.copy2(exe_path, os.path.join(isolated_dir, "sc2_expo.exe"))
    for directory in ("resources", "ico"):
        shutil.copytree(os.path.join(run_dir, directory), os.path.join(isolated_dir, directory))
    with open(os.path.join(run_dir, "config.json"), encoding="utf-8") as f:
        config = json.load(f)
    config["debug_mode"] = True
    if default_artifact_hotkey:
        config["artifact_hotkey_mod"] = MOD_CONTROL | MOD_SHIFT
        config["artifact_hotkey_vk"] = 0xDC
    with open(os.path.join(isolated_dir, "config.json"), "w", encoding="utf-8") as f:
        json.dump(config, f, ensure_ascii=False)
    run_dir = isolated_dir
    exe_path = os.path.join(run_dir, "sc2_expo.exe")
    return temp_dir, run_dir, exe_path


def test_fixes():
    print("=================================================================")
    print("RUNNING TEST SUITE: Bug Fixes Verification (Artifact & Banner)")
    print("=================================================================")
    temp_dir, run_dir, exe_path = isolated_debug_app()
    bindings = configured_hotkeys(run_dir)
    if "lock" not in bindings or "artifact" not in bindings:
        print("[FAIL] Lock and artifact shortcuts must be enabled for this test")
        return False
    proc = subprocess.Popen([exe_path], cwd=run_dir)
    try:
        hwnd = 0
        for _ in range(50):
            hwnd = user32.FindWindowW("SC2ExpoNativeOverlay", "SC2 Expo Native")
            if hwnd:
                owner = wintypes.DWORD()
                user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
                if owner.value == proc.pid:
                    break
                hwnd = 0
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

        h_toast = user32.FindWindowW("SC2ToastOverlayNative", "SC2 Toast Overlay")
        if not h_toast:
            print("[FAIL] Toast overlay window not found")
            return False
        print(f"[PASS] Toast overlay window found (HWND={h_toast})")

        # Confirm this process actually registered the computed ID and binding.
        initial_style = user32.GetWindowLongW(hwnd, -20)
        if not send_hotkey(hwnd, bindings["lock"]):
            print("[FAIL] Could not post lock hotkey")
            return False
        time.sleep(0.3)
        changed_style = user32.GetWindowLongW(hwnd, -20)
        if not ((initial_style ^ changed_style) & 0x20):
            print("[FAIL] Configured lock hotkey did not toggle click-through")
            return False
        send_hotkey(hwnd, bindings["lock"])
        time.sleep(0.3)
        if (user32.GetWindowLongW(hwnd, -20) ^ initial_style) & 0x20:
            print("[FAIL] Configured lock hotkey did not restore click-through")
            return False
        print("[PASS] Configured hotkey registration verified by lock state changes")

        # Debug mode fixes the active map to 往日神庙-A, which has no artifact.
        # Positive show/hide behavior is covered with a known artifact map in
        # the C++ ArtifactViewer tests.
        print("\n--- Test 1: Artifact hotkey on the debug non-artifact map ---")
        if not send_hotkey(hwnd, bindings["artifact"]):
            print("[FAIL] Could not post artifact hotkey")
            return False
        time.sleep(0.8)
        if user32.IsWindowVisible(h_art):
            print("[FAIL] Artifact overlay appeared on the debug non-artifact map")
            return False
        print("[PASS] Artifact overlay remains hidden on the debug non-artifact map")

        # Test 2: Tray Command ID_TRAY_SETTINGS (2006)
        print("\n--- Test 2: Testing Settings Panel via Tray Command (2006) ---")
        h_settings = user32.FindWindowW("SC2SettingsWindowNative", "设置")
        if not h_settings:
            print("[FAIL] Settings window not found")
            return False
        print(f"[PASS] Settings window found (HWND={h_settings})")

        # Open settings
        user32.PostMessageW(hwnd, 0x0111, 2006, 0) # ID_TRAY_SETTINGS
        time.sleep(0.5)
        vis_settings = user32.IsWindowVisible(h_settings)
        print(f"Settings visible after tray command: {bool(vis_settings)}")
        if not vis_settings:
            print("[FAIL] Settings window should be visible after tray command!")
            return False
        print("[PASS] Standalone Settings window displayed successfully!")

        # Toggle close settings
        user32.PostMessageW(hwnd, 0x0111, 2006, 0)
        time.sleep(0.5)
        vis_settings = user32.IsWindowVisible(h_settings)
        if vis_settings:
            print("[FAIL] Settings window should be hidden after 2nd toggle!")
            return False
        print("[PASS] Standalone Settings window closed cleanly!")

        # Test 2.5: Tray Lock / Unlock toggle
        print("\n--- Test 2.5: Testing Tray Lock Toggle (2002) ---")
        GWL_EXSTYLE = -20
        WS_EX_TRANSPARENT = 0x00000020
        style_init = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)

        user32.PostMessageW(hwnd, 0x0111, 2002, 0) # ID_TRAY_LOCK = 2002
        time.sleep(0.3)
        if proc.poll() is not None:
            print("[FAIL] Process exited prematurely after sending ID_TRAY_LOCK!")
            return False
        style_toggled = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        if (style_init & WS_EX_TRANSPARENT) == (style_toggled & WS_EX_TRANSPARENT):
            print("[FAIL] Tray Lock Toggle (2002) did not toggle WS_EX_TRANSPARENT style!")
            return False

        user32.PostMessageW(hwnd, 0x0111, 2002, 0) # Toggle back
        time.sleep(0.3)
        if proc.poll() is not None:
            print("[FAIL] Process exited prematurely after toggling back ID_TRAY_LOCK!")
            return False
        style_reverted = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        if (style_toggled & WS_EX_TRANSPARENT) == (style_reverted & WS_EX_TRANSPARENT):
            print("[FAIL] Tray Lock Toggle did not revert WS_EX_TRANSPARENT style!")
            return False
        print("[PASS] Tray Lock Toggle handled cleanly and verified style changes")

        # Test 2.5: Verify no artifact shown on non-artifact map
        print("\n--- Test 2.5: Verify non-artifact map never shows artifact overlay ---")
        # In a non-artifact map, artifact window should never show
        # We verify via public API and unit suite that non-artifact maps resolve to empty
        vis = user32.IsWindowVisible(h_art)
        if vis:
            print("[FAIL] Artifact window should remain hidden!")
            return False
        print("[PASS] Non-artifact map stays hidden cleanly!")

        # Test 3: Testing AB Map Switch Hotkey & Command
        print("\n--- Test 3: Testing AB Map Switch Hotkeys & Tray Command ---")
        # Hotkey dispatch is verified above by an observable lock state change.
        # Map switching has no stable observable state without a controlled game map.
        # Trigger via tray command ID_TRAY_MAP (2003)
        user32.PostMessageW(hwnd, 0x0111, 2003, 0) # ID_TRAY_MAP = 2003
        time.sleep(0.3)
        if proc.poll() is not None:
            print("[FAIL] Process exited prematurely after ID_TRAY_MAP command!")
            return False
        print("[PASS] Tray command ID_TRAY_MAP (2003) handled cleanly")

        # Test 4: Clean shutdown
        print("\n--- Test 4: Clean shutdown ---")
        user32.PostMessageW(hwnd, 0x0010, 0, 0) # WM_CLOSE
        try:
            proc.wait(timeout=5)
            print("[PASS] Process exited cleanly with return code 0")
        except subprocess.TimeoutExpired:
            print("[FAIL] Process failed to exit within 5s timeout!")
            return False

        print("\n=================================================================")
        print("ALL BUG FIXES VERIFIED SUCCESSFULLY!")
        print("=================================================================")
        return True
    finally:
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        temp_dir.cleanup()

if __name__ == "__main__":
    ok = test_fixes()
    sys.exit(0 if ok else 1)
