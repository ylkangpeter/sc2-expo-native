import subprocess
import sys
import os
import shutil
import zipfile
import time

CLANG_PATH = r"C:\Users\kingpeter\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe"
WINDRES_PATH = r"C:\Users\kingpeter\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\x86_64-w64-mingw32-windres.exe"

if sys.platform != "win32":
    CLANG_PATH = "/mnt/c/Users/kingpeter/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/clang++.exe"
    WINDRES_PATH = "/mnt/c/Users/kingpeter/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/x86_64-w64-mingw32-windres.exe"

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
        print("Compiling Windows resource...")
        res_cmd = [WINDRES_PATH, "res/resource.rc", "-O", "coff", "-o", "res/resource.o"]
        subprocess.run(res_cmd, check=True)
    
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
    
    target_exe = os.path.join(app_dir, "sc2_expo.exe")
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
    res = subprocess.run(cmd, capture_output=True, text=True)
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

    # 4. Create ZIP archive in dist/
    zip_path = os.path.join(dist_dir, "sc2_expo_native.zip")
    print(f"Creating package archive: {zip_path} ...")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for foldername, subfolders, filenames in os.walk(app_dir):
            for filename in filenames:
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
        "src/config.cpp",
        "src/map_detector.cpp",
        "src/schedule_manager.cpp",
        "src/troop_manager.cpp",
        "src/mutator_manager.cpp",
        "src/commander_manager.cpp",
        "src/texture_loader.cpp"
    ]
    
    includes = [
        "-Ivendor",
        "-Ivendor/nlohmann",
        "-Isrc"
    ]
    
    libs = [
        "-lole32",
        "-lwindowscodecs",
        "-ld3d11",
        "-lgdi32",
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
    res = subprocess.run(cmd, capture_output=True, text=True)
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
