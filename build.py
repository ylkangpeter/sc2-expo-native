import subprocess
import sys
import os
import shutil
import zipfile
import time
import glob

def find_toolchain():
    # 1. Search standard WinGet LLVM-MinGW installation directories
    patterns = [
        os.path.expandvars(r"%LOCALAPPDATA%\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT*\*\bin"),
        r"C:\Users\*\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT*\*\bin",
        "/mnt/c/Users/*/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT*/*/bin"
    ]
    for pat in patterns:
        matches = glob.glob(pat)
        if matches:
            bin_dir = matches[0]
            c = os.path.join(bin_dir, "clang++.exe" if sys.platform == "win32" or pat.startswith("/mnt") else "clang++")
            w = os.path.join(bin_dir, "x86_64-w64-mingw32-windres.exe")
            if os.path.exists(c) and os.path.exists(w):
                return c, w

    # 2. Check system PATH
    clang = shutil.which("clang++") or shutil.which("clang++.exe") or "clang++"
    windres = (shutil.which("x86_64-w64-mingw32-windres") or 
               shutil.which("x86_64-w64-mingw32-windres.exe") or 
               shutil.which("windres.exe") or "windres")
    return clang, windres

CLANG_PATH, WINDRES_PATH = find_toolchain()

def build():
    root = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root)

    # 1. Clean root executable if exists (per user request: 不在根目录生成exe，根目录的删除)
    root_exe = os.path.join(root, "sc2_expo.exe")
    if os.path.exists(root_exe):
        try:
            os.remove(root_exe)
            print("Removed old root executable: sc2_expo.exe")
        except Exception as e:
            print(f"Warning: could not remove root sc2_expo.exe: {e}")

    # Prepare dist directories
    dist_dir = os.path.join(root, "dist")
    app_dir = os.path.join(dist_dir, "sc2_expo")
    os.makedirs(app_dir, exist_ok=True)

    # 2. Compile Windows Resource (.rc)
    if os.path.exists("res/resource.rc"):
        rc_mtime = os.path.getmtime("res/resource.rc")
        obj_mtime = os.path.getmtime("res/resource.o") if os.path.exists("res/resource.o") else 0
        if rc_mtime > obj_mtime:
            print("Compiling Windows resource...")
            res_cmd = [WINDRES_PATH, "res/resource.rc", "-O", "coff", "-o", "res/resource.o"]
            subprocess.run(res_cmd, check=True, stdin=subprocess.DEVNULL)
    
    sources = [
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/backends/imgui_impl_win32.cpp",
        "vendor/imgui/backends/imgui_impl_dx11.cpp",
        "src/main.cpp",
    ]
    
    # check for other src files
    for fname in os.listdir("src"):
        if fname.endswith(".cpp") and fname != "main.cpp":
            sources.append(os.path.join("src", fname))

    if os.path.exists("res/resource.o"):
        sources.append("res/resource.o")

    includes = [
        "-Ivendor",
        "-Ivendor/imgui",
        "-Ivendor/imgui/backends",
        "-Ivendor/nlohmann",
        "-Isrc"
    ]
    
    libs = [
        "-ld3d11",
        "-ld3dcompiler",
        "-ldwmapi",
        "-lole32",
        "-lwindowscodecs",
        "-lwinhttp",
        "-limm32",
        "-lgdi32",
        "-lshell32",
    ]
    
    target_exe = "dist/sc2_expo/sc2_expo.exe"
    cmd = [
        CLANG_PATH,
        "-O2",
        "-std=c++20",
        "-static",
        "-Wl,-s",
        "-mwindows",
        *includes,
        *sources,
        "-o", target_exe,
        *libs
    ]
    
    print(f"Building {target_exe} with {len(sources)} source files...")
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    t1 = time.time()
    
    if res.returncode != 0:
        print("Build FAILED!")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        return False
    
    print(f"Build SUCCEEDED in {t1 - t0:.2f}s!")
    size = os.path.getsize(target_exe)
    print(f"Output binary size: {size / (1024*1024):.2f} MB ({size} bytes)")

    # 3. Synchronize runtime dependencies into dist/sc2_expo/
    print("Syncing runtime dependencies into dist/sc2_expo/ ...")

    dist_resources = os.path.join(app_dir, "resources")
    if os.path.exists(dist_resources):
        shutil.rmtree(dist_resources)
    shutil.copytree(os.path.join(root, "resources"), dist_resources)

    dist_ico = os.path.join(app_dir, "ico")
    if os.path.exists(dist_ico):
        shutil.rmtree(dist_ico)
    shutil.copytree(os.path.join(root, "ico"), dist_ico)

    cfg_src = os.path.join(root, "config.json")
    if os.path.exists(cfg_src):
        shutil.copy2(cfg_src, os.path.join(app_dir, "config.json"))

    readme_src = os.path.join(root, "README.md")
    if os.path.exists(readme_src):
        shutil.copy2(readme_src, os.path.join(app_dir, "README.md"))

    # Clean any temporary or backup files in dist/sc2_expo
    for fname in os.listdir(app_dir):
        if fname.endswith(".old") or fname.endswith(".bak") or fname.endswith(".tmp"):
            try:
                os.remove(os.path.join(app_dir, fname))
                print(f"Cleaned backup file: {fname}")
            except Exception:
                pass

    # 4. Create ZIP archive in dist/
    zip_path = os.path.join(dist_dir, "sc2_expo_native.zip")
    print(f"Creating package archive: {zip_path} ...")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for foldername, subfolders, filenames in os.walk(app_dir):
            for filename in filenames:
                if filename.endswith(".old") or filename.endswith(".bak") or filename.endswith(".tmp"):
                    continue
                filepath = os.path.join(foldername, filename)
                arcname = os.path.relpath(filepath, dist_dir)
                zf.write(filepath, arcname)

    zip_size = os.path.getsize(zip_path)
    print(f"ZIP Archive created: {zip_path} ({zip_size / (1024*1024):.2f} MB)")
    print("\nPackage Summary:")
    print(f"  - App Directory (源文件/独立免解压目录): {app_dir}")
    print(f"  - ZIP Archive (分发压缩包): {zip_path}")
    return True

def build_tests():
    root = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root)
    
    sources = [
        "tests/test_main.cpp",
        "tests/test_overlay_lifecycle.cpp",
        "src/config.cpp",
        "src/map_detector.cpp",
        "src/schedule_manager.cpp",
        "src/troop_manager.cpp",
        "src/mutator_manager.cpp",
        "src/mutator_alert_window.cpp",
        "src/commander_manager.cpp",
        "src/texture_loader.cpp",
        "src/artifact_viewer.cpp",
        "src/overlay_ui.cpp",
        "src/sc2_client.cpp",
        "src/hotkeys.cpp",
        "src/lock_window.cpp",
        "src/settings_window.cpp",
        "src/toast_window.cpp",
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/backends/imgui_impl_win32.cpp",
        "vendor/imgui/backends/imgui_impl_dx11.cpp",
    ]
    
    includes = [
        "-Ivendor",
        "-Ivendor/imgui",
        "-Ivendor/imgui/backends",
        "-Ivendor/nlohmann",
        "-Isrc"
    ]
    
    libs = [
        "-lole32",
        "-lwindowscodecs",
        "-ld3d11",
        "-lgdi32",
        "-ld3dcompiler",
        "-ldwmapi",
        "-lwinhttp",
        "-limm32",
        "-lshell32",
    ]
    
    output_exe = "test_runner.exe"
    cmd = [
        CLANG_PATH,
        "-O2",
        "-mconsole",
        "-std=c++20",
        "-static",
        "-Wl,-s",
        *includes,
        *sources,
        "-o", output_exe,
        *libs
    ]
    
    print(f"Building {output_exe} with {len(sources)} source files...")
    t0 = time.time()
    res = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL)
    t1 = time.time()
    
    if res.returncode != 0:
        print("Tests Build FAILED!")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        return False
    else:
        print(f"Tests Build SUCCEEDED in {t1 - t0:.2f}s!")
        return True

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "test":
        success = build_tests()
    else:
        success = build()
    sys.exit(0 if success else 1)
