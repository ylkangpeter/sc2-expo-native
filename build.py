import subprocess
import sys
import os
import time

CLANG_PATH = r"C:\Users\<USERNAME>\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\clang++.exe"
WINDRES_PATH = r"C:\Users\<USERNAME>\AppData\Local\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\llvm-mingw-20261020-ucrt-x86_64\bin\x86_64-w64-mingw32-windres.exe"

if sys.platform != "win32":
    CLANG_PATH = "/mnt/c/Users/<USERNAME>/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/clang++.exe"
    WINDRES_PATH = "/mnt/c/Users/<USERNAME>/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20261020-ucrt-x86_64/bin/x86_64-w64-mingw32-windres.exe"

def build():
    root = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root)

    # Compile Windows Resource (.rc)
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
    
    output_exe = "sc2_expo.exe"
    cmd = [
        CLANG_PATH,
        "-O2",
        "-std=c++20",
        "-static",
        "-Wl,-s",
        "-mwindows",
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
        print("Build FAILED!")
        print("STDOUT:", res.stdout)
        print("STDERR:", res.stderr)
        return False
    else:
        print(f"Build SUCCEEDED in {t1 - t0:.2f}s!")
        size = os.path.getsize(output_exe)
        print(f"Output binary size: {size / (1024*1024):.2f} MB ({size} bytes)")
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

