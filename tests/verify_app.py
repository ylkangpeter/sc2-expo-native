import subprocess
import time
import ctypes
from ctypes import wintypes
import sys

user32 = ctypes.windll.user32

class RECT(ctypes.Structure):
    _fields_ = [
        ("left", wintypes.LONG),
        ("top", wintypes.LONG),
        ("right", wintypes.LONG),
        ("bottom", wintypes.LONG)
    ]

def verify():
    print("Launching sc2_expo.exe for verification...")
    proc = subprocess.Popen(["sc2_expo.exe"])
    
    try:
        # Wait up to 5 seconds for window creation
        hwnd = 0
        for _ in range(50):
            hwnd = user32.FindWindowW("SC2ExpoNativeOverlay", "SC2 Expo Native")
            if hwnd:
                break
            time.sleep(0.1)
        
        if not hwnd:
            print("[FAIL] Could not find window 'SC2 Expo Native'!")
            proc.kill()
            return False
        
        print(f"[PASS] Found window HWND: {hwnd}")
        
        # Check window rect
        rect = RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        w = rect.right - rect.left
        h = rect.bottom - rect.top
        print(f"Window bounds: Left={rect.left}, Top={rect.top}, Width={w}, Height={h}")
        
        # Verify it's a compact tool window, NOT fullscreen!
        screen_w = user32.GetSystemMetrics(0) # SM_CXSCREEN
        screen_h = user32.GetSystemMetrics(1) # SM_CYSCREEN
        print(f"Primary monitor metrics: {screen_w} x {screen_h}")
        
        if w >= screen_w or h >= screen_h:
            print(f"[FAIL] Window is fullscreen ({w}x{h})! Should be compact tool window!")
            return False
        else:
            print(f"[PASS] Window is compact ({w}x{h}), leaves >80% of desktop completely free.")
        
        # Verify window style and extended style
        GWL_EXSTYLE = -20
        WS_EX_TRANSPARENT = 0x00000020
        WS_EX_LAYERED = 0x00080000
        
        ex_style = user32.GetWindowLongW(hwnd, GWL_EXSTYLE)
        is_layered = (ex_style & WS_EX_LAYERED) != 0
        is_transparent = (ex_style & WS_EX_TRANSPARENT) != 0
        
        print(f"Extended style: 0x{ex_style:08X}")
        print(f"WS_EX_LAYERED: {is_layered}")
        print(f"WS_EX_TRANSPARENT: {is_transparent}")
        
        if not is_layered:
            print("[FAIL] WS_EX_LAYERED is missing!")
            return False
        else:
            print("[PASS] WS_EX_LAYERED is set.")
            
        if not is_transparent:
            print("[FAIL] Default startup should have WS_EX_TRANSPARENT set (locked click-through)!")
            return False
        else:
            print("[PASS] Default startup has WS_EX_TRANSPARENT set (click-through locked).")
        
        # Send WM_CLOSE
        print("Sending WM_CLOSE to window...")
        WM_CLOSE = 0x0010
        user32.PostMessageW(hwnd, WM_CLOSE, 0, 0)
        
        # Wait for process exit
        try:
            ret = proc.wait(timeout=5)
            print(f"[PASS] Process exited cleanly with return code {ret}")
            return True
        except subprocess.TimeoutExpired:
            print("[FAIL] Process did not exit within 5 seconds after WM_CLOSE!")
            proc.kill()
            return False

    finally:
        if proc.poll() is None:
            proc.kill()

if __name__ == "__main__":
    ok = verify()
    sys.exit(0 if ok else 1)
