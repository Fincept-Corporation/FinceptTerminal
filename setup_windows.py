import os
import sys
import subprocess
import shutil
import platform

def run_command(cmd, shell=False):
    print(f"Running: {' '.join(cmd) if isinstance(cmd, list) else cmd}")
    try:
        # Use shell=True for commands that might be entry points installed by pip
        subprocess.check_call(cmd, shell=shell)
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error running command: {e}")
        return False

def find_msvc():
    vswhere_path = os.path.expandvars("%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe")
    if os.path.exists(vswhere_path):
        try:
            output = subprocess.check_output([
                vswhere_path, "-latest", "-products", "*",
                "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                "-property", "installationPath"
            ], encoding='utf-8').strip()
            return output
        except:
            return None
    return None

def main():
    print("================================================")
    print("  Fincept Terminal Local Setup (Windows)")
    print("  This script will prepare your system to run")
    print("  Fincept Terminal locally with Personal mode.")
    print("================================================")

    if platform.system() != "Windows":
        print("Error: This script is intended for Windows systems.")
        sys.exit(1)

    # 1. Check Python version
    py_ver = sys.version_info
    if py_ver.major < 3 or (py_ver.major == 3 and py_ver.minor < 11):
        print(f"Error: Python 3.11+ is required. Found {py_ver.major}.{py_ver.minor}")
        sys.exit(1)
    print(f"OK: Python {py_ver.major}.{py_ver.minor} found.")

    # 2. Check for MSVC (Visual Studio 2022)
    vs_path = find_msvc()
    if vs_path:
        print(f"OK: Visual Studio 2022 found at {vs_path}")
    else:
        print("\n!!! WARNING !!!")
        print("Visual Studio 2022 (with 'C++ Desktop Development' workload) not detected.")
        print("This is REQUIRED to build the application.")
        print("Download it here: https://visualstudio.microsoft.com/vs/community/")
        print("Ensure you check 'Desktop development with C++' during installation.\n")
        input("Press Enter to continue if you think this is a mistake, or Ctrl+C to abort...")

    # 3. Install/Upgrade build tools via pip
    print("\n[1/3] Installing build tools (aqtinstall, cmake, ninja)...")
    if not run_command([sys.executable, "-m", "pip", "install", "--upgrade", "aqtinstall", "cmake", "ninja"]):
        print("Error: Failed to install build tools via pip.")
        sys.exit(1)

    # 4. Install Qt 6.8.3
    qt_version = "6.8.3"
    root_dir = os.path.dirname(os.path.abspath(__file__))
    qt_install_dir = os.path.join(root_dir, ".qt")
    qt_path = os.path.join(qt_install_dir, qt_version, "msvc2022_64")

    print(f"\n[2/3] Locating Qt {qt_version}...")
    if os.path.exists(os.path.join(qt_path, "bin", "qmake.exe")):
        print(f"OK: Qt {qt_version} already installed at {qt_path}")
    else:
        print(f"Installing Qt {qt_version} via aqtinstall (this may take a few minutes)...")
        # Use python -m aqt to ensure we use the one we just installed
        if not run_command([sys.executable, "-m", "aqt", "install-qt", "windows", "desktop", qt_version, "win64_msvc2022_64",
                            "-m", "qtcharts", "qtwebsockets", "qtmultimedia", "qtspeech", "--outputdir", qt_install_dir]):
            print("Error: Failed to install Qt.")
            sys.exit(1)

    # 5. Configure with CMake
    print(f"\n[3/3] Configuring Fincept Terminal...")
    app_dir = os.path.join(root_dir, "fincept-qt")
    if not os.path.exists(app_dir):
        print(f"Error: {app_dir} not found. Ensure you are running this from the repo root.")
        sys.exit(1)

    os.chdir(app_dir)

    # Use the absolute path to the freshly installed Qt
    # Call cmake via python -m cmake to ensure it's found even if not in PATH
    # Explicitly point to the source directory -S fincept-qt
    # We attempt to run within the vcvarsall environment if possible, but
    # since we're already in Python, the easiest way for the user is to
    # run run_local.bat which handles the environment setup.
    if not run_command([sys.executable, "-m", "cmake", "-S", ".", "--preset", "win-release", f"-DCMAKE_PREFIX_PATH={qt_path}"]):
        print("\nError: CMake configuration failed.")
        print("Common fix: Ensure you have Visual Studio 2022 installed with C++ support.")
        print("Tip: If detection fails, try running this script from the 'Developer Command Prompt for VS 2022'.")
        sys.exit(1)

    print("\n================================================")
    print("  Setup Complete!")
    print("  To build and launch the app, run:")
    print("  run_local.bat")
    print("================================================")

if __name__ == "__main__":
    main()
