"""Windows integration regression for restoring the overlay at display edges.

Run after building: python -m unittest discover -s tests -p test_window_position.py
The packaged application and its configuration are copied to a temporary directory.
"""
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time
import unittest


@unittest.skipUnless(os.name == "nt", "requires a Windows desktop")
class WindowPositionTests(unittest.TestCase):
    def test_position_survives_exit_and_restart(self):
        user32 = ctypes.WinDLL("user32", use_last_error=True)
        try:
            user32.SetProcessDpiAwarenessContext(ctypes.c_void_p(-4))
        except AttributeError:
            user32.SetProcessDPIAware()
        user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
        user32.GetClassNameW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
        user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
        user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int, ctypes.c_int,
                                        ctypes.c_int, ctypes.c_int, wintypes.UINT]
        user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
        callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
        user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
        package = Path(__file__).resolve().parents[1] / "dist" / "sc2_expo"
        self.assertTrue((package / "sc2_expo.exe").is_file(), "Build the packaged executable first")

        def find_window(pid):
            found = []
            @callback_type
            def visit(hwnd, _):
                owner = wintypes.DWORD()
                user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
                name = ctypes.create_unicode_buffer(128)
                user32.GetClassNameW(hwnd, name, len(name))
                if owner.value == pid and name.value == "SC2ExpoNativeOverlay":
                    found.append(hwnd)
                return True
            user32.EnumWindows(visit, 0)
            return found[0] if found else None

        def wait_window(proc):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                self.assertIsNone(proc.poll(), "Application exited during startup")
                hwnd = find_window(proc.pid)
                if hwnd:
                    # Allow initialization to finish before moving or reading the window.
                    time.sleep(1)
                    return hwnd
                time.sleep(0.05)
            self.fail("Overlay window did not appear")

        def position(hwnd):
            rect = wintypes.RECT()
            self.assertTrue(user32.GetWindowRect(hwnd, ctypes.byref(rect)))
            return rect.left, rect.top

        with tempfile.TemporaryDirectory(prefix="sc2-position-") as temp:
            app = Path(temp) / "app"
            shutil.copytree(package, app)
            executable = app / "sc2_expo.exe"
            config = app / "config.json"
            # Negative saved x is restored at the primary display's left edge.
            width = user32.GetSystemMetrics(0)  # SM_CXSCREEN
            cases = [(-15, 100), (0, 100), (width // 2, 100)]
            for x, y in cases:
                with self.subTest(x=x, y=y):
                    proc = subprocess.Popen([str(executable)], cwd=app)
                    try:
                        hwnd = wait_window(proc)
                        # Unlock, move, and close without waiting for the save timer.
                        self.assertTrue(user32.PostMessageW(hwnd, 0x0111, 2002, 0))
                        time.sleep(0.2)
                        self.assertTrue(user32.SetWindowPos(hwnd, None, x, y, 0, 0, 0x0001 | 0x0004 | 0x0010))
                        before = position(hwnd)
                        self.assertEqual(before, (x, y))
                        self.assertTrue(user32.PostMessageW(hwnd, 0x0010, 0, 0))
                        self.assertEqual(proc.wait(timeout=15), 0)
                        saved = json.loads(config.read_text(encoding="utf-8"))
                        self.assertEqual((saved["main_window_x"], saved["main_window_y"]), before)
                    finally:
                        if proc.poll() is None:
                            proc.kill()
                            proc.wait()
                    proc = subprocess.Popen([str(executable)], cwd=app)
                    try:
                        hwnd = wait_window(proc)
                        self.assertEqual(position(hwnd), (max(0, x), y))
                        self.assertTrue(user32.PostMessageW(hwnd, 0x0010, 0, 0))
                        self.assertEqual(proc.wait(timeout=15), 0)
                    finally:
                        if proc.poll() is None:
                            proc.kill()
                            proc.wait()


if __name__ == "__main__":
    unittest.main()
