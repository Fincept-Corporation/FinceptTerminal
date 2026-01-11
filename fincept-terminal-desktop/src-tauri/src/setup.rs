// setup.rs - Simplified Python and Bun setup
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Command;
use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

static SETUP_RUNNING: Mutex<bool> = Mutex::new(false);

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SetupProgress {
    pub step: String,
    pub progress: u8,
    pub message: String,
    pub is_error: bool,
}

#[derive(Debug, Clone, Serialize)]
pub struct SetupStatus {
    pub python_installed: bool,
    pub bun_installed: bool,
    pub packages_installed: bool,
    pub needs_setup: bool,
}

const PYTHON_VERSION: &str = "3.12.7";
const BUN_VERSION: &str = "1.1.0";

fn emit_progress(app: &AppHandle, step: &str, progress: u8, message: &str, is_error: bool) {
    let _ = app.emit("setup-progress", SetupProgress {
        step: step.to_string(),
        progress,
        message: message.to_string(),
        is_error,
    });
    eprintln!("[SETUP] [{}] {}% - {}", step, progress, message);
}

fn is_dev_mode() -> bool {
    cfg!(debug_assertions)
}

fn get_install_dir(app: &AppHandle) -> Result<PathBuf, String> {
    if is_dev_mode() {
        // Dev mode: use OS-specific user directory
        let base_dir = if cfg!(target_os = "windows") {
            std::env::var("LOCALAPPDATA")
                .map(PathBuf::from)
                .unwrap_or_else(|_| PathBuf::from("C:\\Users\\Default\\AppData\\Local"))
        } else if cfg!(target_os = "macos") {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join("Library/Application Support"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        } else {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join(".local/share"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        };
        Ok(base_dir.join("fincept-dev"))
    } else {
        // Production: use app data directory (writable, persists across updates)
        // Windows: C:\Users\<Username>\AppData\Roaming\com.fincept.terminal\
        // macOS: ~/Library/Application Support/com.fincept.terminal/
        // Linux: ~/.local/share/com.fincept.terminal/
        let app_data_dir = app.path().app_data_dir()
            .map_err(|e| format!("Failed to get app data dir: {}", e))?;

        eprintln!("[SETUP] Production install directory: {:?}", app_data_dir);
        Ok(app_data_dir)
    }
}

/// Check Python installation
fn check_python(install_dir: &PathBuf) -> (bool, Option<String>) {
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        install_dir.join("python/bin/python3")
    };

    if !python_exe.exists() {
        return (false, None);
    }

    let mut cmd = Command::new(&python_exe);
    cmd.arg("--version");

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    if let Ok(output) = cmd.output() {
        if output.status.success() {
            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
            return (true, Some(version));
        }
    }
    (false, None)
}

/// Check Bun installation
fn check_bun(install_dir: &PathBuf) -> (bool, Option<String>) {
    let bun_exe = if cfg!(target_os = "windows") {
        install_dir.join("bun/bun.exe")
    } else {
        install_dir.join("bun/bin/bun")
    };

    if !bun_exe.exists() {
        return (false, None);
    }

    let mut cmd = Command::new(&bun_exe);
    cmd.arg("--version");
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    if let Ok(output) = cmd.output() {
        if output.status.success() {
            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
            return (true, Some(version));
        }
    }
    (false, None)
}

/// Check LLVM installation (macOS only, needed for llvmlite/numba)
#[cfg(target_os = "macos")]
fn check_llvm(install_dir: &PathBuf) -> (bool, Option<String>) {
    let llvm_dir = install_dir.join("llvm");
    let llvm_config = llvm_dir.join("bin/llvm-config");

    if llvm_config.exists() {
        return (true, Some(llvm_dir.to_string_lossy().to_string()));
    }

    (false, None)
}

#[cfg(not(target_os = "macos"))]
fn check_llvm(_install_dir: &PathBuf) -> (bool, Option<String>) {
    // LLVM not needed on Windows/Linux for our use case
    (true, Some("Not required".to_string()))
}

/// Check TA-Lib installation (needed for ta-lib Python wrapper)
#[cfg(target_os = "macos")]
fn check_talib(install_dir: &PathBuf) -> (bool, Option<String>) {
    let talib_dir = install_dir.join("ta-lib");
    let talib_lib = talib_dir.join("lib/libta_lib.a");

    if talib_lib.exists() {
        return (true, Some(talib_dir.to_string_lossy().to_string()));
    }

    (false, None)
}

#[cfg(not(target_os = "macos"))]
fn check_talib(_install_dir: &PathBuf) -> (bool, Option<String>) {
    // For Windows/Linux, check if ta-lib headers are available
    // This is a simplified check - may need platform-specific logic
    (true, Some("Check manually".to_string()))
}

/// Check if Python packages are installed in both venvs
fn check_packages(install_dir: &PathBuf) -> bool {
    let venv1_python = if cfg!(target_os = "windows") {
        install_dir.join("venv-numpy1/Scripts/python.exe")
    } else {
        install_dir.join("venv-numpy1/bin/python3")
    };

    let venv2_python = if cfg!(target_os = "windows") {
        install_dir.join("venv-numpy2/Scripts/python.exe")
    } else {
        install_dir.join("venv-numpy2/bin/python3")
    };

    if !venv1_python.exists() || !venv2_python.exists() {
        eprintln!("[SETUP] Venvs don't exist - need to install packages");
        return false;
    }

    // Check numpy1 venv for key packages
    let mut cmd1 = Command::new(&venv1_python);
    cmd1.args(&["-c", "import numpy, pandas, vectorbt, backtesting, gluonts"]);

    #[cfg(target_os = "windows")]
    cmd1.creation_flags(CREATE_NO_WINDOW);
    let check1 = cmd1.output().map(|o| o.status.success()).unwrap_or(false);

    if !check1 {
        eprintln!("[SETUP] NumPy 1.x venv packages check failed - need to reinstall");
    }

    // Check numpy2 venv for key packages
    let mut cmd2 = Command::new(&venv2_python);
    cmd2.args(&["-c", "import numpy, pandas, yfinance, vnpy"]);

    #[cfg(target_os = "windows")]
    cmd2.creation_flags(CREATE_NO_WINDOW);
    let check2 = cmd2.output().map(|o| o.status.success()).unwrap_or(false);

    if !check2 {
        eprintln!("[SETUP] NumPy 2.x venv packages check failed - need to reinstall");
    }

    check1 && check2
}

/// Install Python
async fn install_python(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "python", 0, "Installing Python...", false);

    let python_dir = install_dir.join("python");
    std::fs::create_dir_all(&python_dir)
        .map_err(|e| format!("Failed to create Python dir: {}", e))?;

    // Download Python
    emit_progress(app, "python", 20, "Downloading Python...", false);

    #[cfg(target_os = "windows")]
    {
        let download_url = format!("https://www.python.org/ftp/python/{}/python-{}-embed-amd64.zip", PYTHON_VERSION, PYTHON_VERSION);
        let zip_path = python_dir.join("python.zip");

        let mut cmd = Command::new("powershell");
        cmd.args(&[
            "-Command",
            &format!("Invoke-WebRequest -Uri '{}' -OutFile \"{}\"", download_url, zip_path.display())
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "python", 60, "Extracting Python...", false);

        let mut cmd = Command::new("powershell");
        cmd.args(&[
            "-Command",
            &format!("Expand-Archive -Path '{}' -DestinationPath '{}' -Force", zip_path.display(), python_dir.display())
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }
        let _ = std::fs::remove_file(&zip_path);
    }

    #[cfg(target_os = "linux")]
    {
        // Detect architecture (x86_64 or ARM)
        let arch = std::env::consts::ARCH;
        let platform = if arch == "aarch64" {
            "aarch64-unknown-linux-gnu-install_only"
        } else {
            "x86_64-unknown-linux-gnu-install_only"
        };
        let download_url = format!("https://github.com/indygreg/python-build-standalone/releases/download/20240107/cpython-3.12.7+20240107-{}.tar.gz", platform);
        let tar_path = python_dir.join("python.tar.gz");

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", tar_path.to_str().unwrap(), &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "python", 60, "Extracting Python...", false);

        let mut cmd = Command::new("tar");
        cmd.args(&["-xzf", tar_path.to_str().unwrap(), "-C", python_dir.to_str().unwrap(), "--strip-components=1"]);
        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }
        let _ = std::fs::remove_file(&tar_path);
    }

    #[cfg(target_os = "macos")]
    {
        // Use python-build-standalone for macOS (same approach as Linux)
        // Detect architecture (Apple Silicon M1/M2 or Intel)
        let arch = std::env::consts::ARCH;
        let platform = if arch == "aarch64" {
            "aarch64-apple-darwin-install_only"
        } else {
            "x86_64-apple-darwin-install_only"
        };

        // Use latest python-build-standalone release
        let download_url = format!("https://github.com/astral-sh/python-build-standalone/releases/download/20241217/cpython-3.12.8+20241217-{}.tar.gz", platform);
        let tar_path = python_dir.join("python.tar.gz");

        emit_progress(app, "python", 40, "Downloading Python...", false);

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", tar_path.to_str().unwrap(), &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        eprintln!("[SETUP] Download complete, file size: {} bytes",
            std::fs::metadata(&tar_path).map(|m| m.len()).unwrap_or(0));

        emit_progress(app, "python", 50, "Decompressing archive...", false);

        // Use gunzip separately for better BSD tar compatibility
        let mut gunzip_cmd = Command::new("gunzip");
        gunzip_cmd.arg(tar_path.to_str().unwrap());
        let output = gunzip_cmd.output().map_err(|e| format!("Gunzip failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Gunzip failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "python", 60, "Extracting Python...", false);

        // Extract the uncompressed tar file
        let tar_uncompressed = python_dir.join("python.tar");
        let mut cmd = Command::new("tar");
        cmd.args(&["-xf", tar_uncompressed.to_str().unwrap(), "-C", python_dir.to_str().unwrap(), "--strip-components=1"]);
        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        // Cleanup
        let _ = std::fs::remove_file(&tar_uncompressed);

        // Check what was extracted
        eprintln!("[SETUP] Checking extracted Python files...");
        if let Ok(entries) = std::fs::read_dir(&python_dir) {
            for entry in entries.flatten() {
                eprintln!("[SETUP]   - {:?}", entry.path());
            }
        }
    }

    // Enable pip by modifying python312._pth (Windows only)
    #[cfg(target_os = "windows")]
    {
        let pth_file = python_dir.join("python312._pth");
        if pth_file.exists() {
            if let Ok(content) = std::fs::read_to_string(&pth_file) {
                let new_content = content.replace("#import site", "import site");
                let _ = std::fs::write(&pth_file, new_content);
            }
        } else {
            // Create _pth file if it doesn't exist
            let pth_content = "python312.zip\n.\n\nimport site\n";
            let _ = std::fs::write(&pth_file, pth_content);
        }
    }

    // Download and install pip
    emit_progress(app, "python", 80, "Installing pip...", false);

    let get_pip_url = "https://bootstrap.pypa.io/get-pip.py";
    let get_pip_path = python_dir.join("get-pip.py");

    // Download get-pip.py
    #[cfg(target_os = "windows")]
    {
        let mut cmd = Command::new("powershell");
        cmd.args(&[
            "-Command",
            &format!("Invoke-WebRequest -Uri '{}' -OutFile '{}'", get_pip_url, get_pip_path.display())
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);
        cmd.output().map_err(|e| format!("Failed to download get-pip.py: {}", e))?;
    }

    #[cfg(not(target_os = "windows"))]
    {
        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", get_pip_path.to_str().unwrap(), get_pip_url]);
        cmd.output().map_err(|e| format!("Failed to download get-pip.py: {}", e))?;
    }

    // Install pip
    let python_exe = if cfg!(target_os = "windows") {
        python_dir.join("python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        python_dir.join("bin/python3")
    };

    if !python_exe.exists() {
        return Err(format!("Python executable not found at: {:?}", python_exe));
    }

    let mut cmd = Command::new(&python_exe);
    cmd.arg(get_pip_path.to_str().unwrap());

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd.output().map_err(|e| format!("Failed to install pip: {}", e))?;

    eprintln!("[SETUP] [Python] pip install stdout: {}", String::from_utf8_lossy(&output.stdout));
    eprintln!("[SETUP] [Python] pip install stderr: {}", String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err(format!("Pip install failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    let _ = std::fs::remove_file(&get_pip_path);

    emit_progress(app, "python", 100, "Python installed", false);
    Ok(())
}

/// Install Bun
async fn install_bun(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "bun", 0, "Installing Bun...", false);

    let bun_dir = install_dir.join("bun");
    std::fs::create_dir_all(&bun_dir)
        .map_err(|e| format!("Failed to create Bun dir: {}", e))?;

    emit_progress(app, "bun", 30, "Downloading Bun...", false);

    #[cfg(target_os = "windows")]
    {
        let download_url = format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-windows-x64.zip", BUN_VERSION);
        let zip_path = bun_dir.join("bun.zip");

        let mut cmd = Command::new("powershell");
        cmd.args(&[
            "-Command",
            &format!("Invoke-WebRequest -Uri '{}' -OutFile \"{}\"", download_url, zip_path.display())
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "bun", 70, "Extracting Bun...", false);

        let mut cmd = Command::new("powershell");
        cmd.args(&[
            "-Command",
            &format!("Expand-Archive -Path '{}' -DestinationPath '{}' -Force", zip_path.display(), bun_dir.display())
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        let _ = std::fs::remove_file(&zip_path);

        // Move bun.exe from nested folder to bun/ root
        let nested_bun = bun_dir.join("bun-windows-x64/bun.exe");
        let target_bun = bun_dir.join("bun.exe");

        if nested_bun.exists() && !target_bun.exists() {
            std::fs::copy(&nested_bun, &target_bun)
                .map_err(|e| format!("Failed to copy bun.exe: {}", e))?;
            let _ = std::fs::remove_dir_all(bun_dir.join("bun-windows-x64"));
        }
    }

    #[cfg(target_os = "linux")]
    {
        let download_url = format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-linux-x64.zip", BUN_VERSION);
        let zip_path = bun_dir.join("bun.zip");

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", zip_path.to_str().unwrap(), &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "bun", 70, "Extracting Bun...", false);

        let mut cmd = Command::new("unzip");
        cmd.args(&["-o", zip_path.to_str().unwrap(), "-d", bun_dir.to_str().unwrap()]);
        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        let _ = std::fs::remove_file(&zip_path);

        // Move bun from nested folder to bun/bin/ and make executable
        let nested_bun = bun_dir.join("bun-linux-x64/bun");
        let bin_dir = bun_dir.join("bin");
        let target_bun = bin_dir.join("bun");
        std::fs::create_dir_all(&bin_dir)
            .map_err(|e| format!("Failed to create bin dir: {}", e))?;

        if nested_bun.exists() {
            std::fs::copy(&nested_bun, &target_bun)
                .map_err(|e| format!("Failed to copy bun: {}", e))?;

            // Make executable
            use std::os::unix::fs::PermissionsExt;
            let mut perms = std::fs::metadata(&target_bun)
                .map_err(|e| format!("Failed to get metadata: {}", e))?
                .permissions();
            perms.set_mode(0o755);
            std::fs::set_permissions(&target_bun, perms)
                .map_err(|e| format!("Failed to set permissions: {}", e))?;

            let _ = std::fs::remove_dir_all(bun_dir.join("bun-linux-x64"));
        }
    }

    #[cfg(target_os = "macos")]
    {
        // Detect Apple Silicon (M1/M2) vs Intel
        let arch = std::env::consts::ARCH;
        let (bun_arch, nested_folder) = if arch == "aarch64" {
            ("aarch64", "bun-darwin-aarch64")
        } else {
            ("x64", "bun-darwin-x64")
        };

        let download_url = format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-darwin-{}.zip", BUN_VERSION, bun_arch);
        let zip_path = bun_dir.join("bun.zip");

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", zip_path.to_str().unwrap(), &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "bun", 70, "Extracting Bun...", false);

        let mut cmd = Command::new("unzip");
        cmd.args(&["-o", zip_path.to_str().unwrap(), "-d", bun_dir.to_str().unwrap()]);
        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        let _ = std::fs::remove_file(&zip_path);

        // Move bun from nested folder to bun/ root and make executable
        let nested_bun = bun_dir.join(format!("{}/bun", nested_folder));
        let target_bun = bun_dir.join("bin/bun");
        std::fs::create_dir_all(bun_dir.join("bin"))
            .map_err(|e| format!("Failed to create bin dir: {}", e))?;

        if nested_bun.exists() {
            std::fs::copy(&nested_bun, &target_bun)
                .map_err(|e| format!("Failed to copy bun: {}", e))?;

            // Make executable
            use std::os::unix::fs::PermissionsExt;
            let mut perms = std::fs::metadata(&target_bun)
                .map_err(|e| format!("Failed to get metadata: {}", e))?
                .permissions();
            perms.set_mode(0o755);
            std::fs::set_permissions(&target_bun, perms)
                .map_err(|e| format!("Failed to set permissions: {}", e))?;

            let _ = std::fs::remove_dir_all(bun_dir.join(nested_folder));
        }
    }

    emit_progress(app, "bun", 100, "Bun installed", false);
    Ok(())
}

/// Install LLVM (macOS only, needed for llvmlite/numba)
#[cfg(target_os = "macos")]
async fn install_llvm(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "llvm", 0, "Installing LLVM...", false);

    let llvm_dir = install_dir.join("llvm");
    std::fs::create_dir_all(&llvm_dir)
        .map_err(|e| format!("Failed to create LLVM dir: {}", e))?;

    // Detect architecture
    let arch = std::env::consts::ARCH;
    let (platform, llvm_version) = if arch == "aarch64" {
        ("arm64-apple-darwin22.0", "17.0.6")
    } else {
        ("x86_64-apple-darwin22.0", "17.0.6")
    };

    emit_progress(app, "llvm", 20, "Downloading LLVM...", false);

    let download_url = format!(
        "https://github.com/llvm/llvm-project/releases/download/llvmorg-{}/clang+llvm-{}-{}.tar.xz",
        llvm_version, llvm_version, platform
    );
    let tar_path = llvm_dir.join("llvm.tar.xz");

    let mut cmd = Command::new("curl");
    cmd.args(&["-L", "-o", tar_path.to_str().unwrap(), &download_url]);
    let output = cmd.output().map_err(|e| format!("LLVM download failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("LLVM download failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    eprintln!("[SETUP] LLVM download complete, file size: {} bytes",
        std::fs::metadata(&tar_path).map(|m| m.len()).unwrap_or(0));

    emit_progress(app, "llvm", 60, "Extracting LLVM...", false);

    // Extract tar.xz
    let mut cmd = Command::new("tar");
    cmd.args(&["-xf", tar_path.to_str().unwrap(), "-C", llvm_dir.to_str().unwrap(), "--strip-components=1"]);
    let output = cmd.output().map_err(|e| format!("LLVM extract failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("LLVM extract failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    let _ = std::fs::remove_file(&tar_path);

    emit_progress(app, "llvm", 100, "LLVM installed", false);
    Ok(())
}

#[cfg(not(target_os = "macos"))]
async fn install_llvm(_app: &AppHandle, _install_dir: &PathBuf) -> Result<(), String> {
    // LLVM not needed on Windows/Linux
    Ok(())
}

/// Install TA-Lib (needed for ta-lib Python wrapper)
#[cfg(target_os = "macos")]
async fn install_talib(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "talib", 0, "Installing TA-Lib...", false);

    let talib_dir = install_dir.join("ta-lib");
    std::fs::create_dir_all(&talib_dir)
        .map_err(|e| format!("Failed to create TA-Lib dir: {}", e))?;

    emit_progress(app, "talib", 20, "Downloading TA-Lib...", false);

    // Download TA-Lib source
    let talib_version = "0.4.0";
    let download_url = format!("https://downloads.sourceforge.net/project/ta-lib/ta-lib/{}/ta-lib-{}-src.tar.gz", talib_version, talib_version);
    let tar_path = talib_dir.join("ta-lib.tar.gz");

    let mut cmd = Command::new("curl");
    cmd.args(&["-L", "-o", tar_path.to_str().unwrap(), &download_url]);
    let output = cmd.output().map_err(|e| format!("TA-Lib download failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib download failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    eprintln!("[SETUP] TA-Lib download complete, file size: {} bytes",
        std::fs::metadata(&tar_path).map(|m| m.len()).unwrap_or(0));

    emit_progress(app, "talib", 40, "Extracting TA-Lib...", false);

    // Decompress
    let mut gunzip_cmd = Command::new("gunzip");
    gunzip_cmd.arg(tar_path.to_str().unwrap());
    let output = gunzip_cmd.output().map_err(|e| format!("TA-Lib gunzip failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib gunzip failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    // Extract
    let tar_uncompressed = talib_dir.join("ta-lib.tar");
    let mut cmd = Command::new("tar");
    cmd.args(&["-xf", tar_uncompressed.to_str().unwrap(), "-C", talib_dir.to_str().unwrap(), "--strip-components=1"]);
    let output = cmd.output().map_err(|e| format!("TA-Lib extract failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib extract failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    let _ = std::fs::remove_file(&tar_uncompressed);

    emit_progress(app, "talib", 60, "Compiling TA-Lib...", false);

    // Configure and compile (install to ta-lib dir)
    let configure_script = talib_dir.join("configure");
    let mut cmd = Command::new(&configure_script);
    cmd.args(&["--prefix", talib_dir.to_str().unwrap()]);
    cmd.current_dir(&talib_dir);
    let output = cmd.output().map_err(|e| format!("TA-Lib configure failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib configure failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    emit_progress(app, "talib", 80, "Building TA-Lib (this may take a few minutes)...", false);

    // Make and install
    let mut cmd = Command::new("make");
    cmd.current_dir(&talib_dir);
    let output = cmd.output().map_err(|e| format!("TA-Lib make failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib make failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    let mut cmd = Command::new("make");
    cmd.arg("install");
    cmd.current_dir(&talib_dir);
    let output = cmd.output().map_err(|e| format!("TA-Lib install failed: {}", e))?;
    if !output.status.success() {
        return Err(format!("TA-Lib install failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    emit_progress(app, "talib", 100, "TA-Lib installed", false);
    Ok(())
}

#[cfg(not(target_os = "macos"))]
async fn install_talib(_app: &AppHandle, _install_dir: &PathBuf) -> Result<(), String> {
    // For Windows/Linux, TA-Lib needs to be installed manually
    eprintln!("[SETUP] TA-Lib installation on Windows/Linux requires manual setup");
    Ok(())
}

/// Install UV package manager
async fn install_uv(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "uv", 0, "Installing UV package manager...", false);

    // Use python -m pip instead of pip directly (more reliable)
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        install_dir.join("python/bin/python3")
    };

    let mut cmd = Command::new(&python_exe);
    cmd.args(&["-m", "pip", "install", "uv"]);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    eprintln!("[SETUP] Installing UV: {:?}", cmd);
    let output = cmd.output().map_err(|e| format!("Failed to install UV: {}", e))?;

    eprintln!("[SETUP] UV stdout: {}", String::from_utf8_lossy(&output.stdout));
    eprintln!("[SETUP] UV stderr: {}", String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err(format!("UV installation failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    emit_progress(app, "uv", 100, "UV installed", false);
    Ok(())
}

/// Create virtual environment using UV
async fn create_venv(app: &AppHandle, install_dir: &PathBuf, venv_name: &str) -> Result<(), String> {
    emit_progress(app, "venv", 0, &format!("Creating {}...", venv_name), false);

    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        install_dir.join("python/bin/python3")
    };

    // Find UV executable - check possible locations
    let uv_candidates = if cfg!(target_os = "windows") {
        vec![install_dir.join("python/Scripts/uv.exe")]
    } else {
        vec![install_dir.join("python/bin/uv")]
    };

    let uv_exe = uv_candidates.iter().find(|p| p.exists());

    let venv_path = install_dir.join(venv_name);

    let mut cmd = if let Some(uv_path) = uv_exe {
        eprintln!("[SETUP] Using UV at: {:?}", uv_path);
        Command::new(uv_path)
    } else {
        // Fallback to python -m uv if uv binary not found
        eprintln!("[SETUP] UV binary not found, using python -m uv");
        let mut cmd = Command::new(&python_exe);
        cmd.arg("-m").arg("uv");
        cmd
    };

    cmd.args(&[
        "venv",
        venv_path.to_str().unwrap(),
        "--python", python_exe.to_str().unwrap()
    ]);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    eprintln!("[SETUP] Creating venv: {:?}", cmd);
    let output = cmd.output().map_err(|e| format!("Failed to create {}: {}", venv_name, e))?;

    eprintln!("[SETUP] Venv stdout: {}", String::from_utf8_lossy(&output.stdout));
    eprintln!("[SETUP] Venv stderr: {}", String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err(format!("{} creation failed: {}", venv_name, String::from_utf8_lossy(&output.stderr)));
    }

    emit_progress(app, "venv", 100, &format!("{} created", venv_name), false);
    Ok(())
}

/// Install Python packages in a specific venv using UV
async fn install_packages_in_venv(
    app: &AppHandle,
    install_dir: &PathBuf,
    venv_name: &str,
    requirements_file: &str,
    progress_label: &str
) -> Result<(), String> {
    emit_progress(app, "packages", 0, &format!("Installing {} packages...", progress_label), false);

    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        // Linux and macOS both use python-build-standalone now
        install_dir.join("python/bin/python3")
    };

    let venv_python = if cfg!(target_os = "windows") {
        install_dir.join(format!("{}/Scripts/python.exe", venv_name))
    } else {
        install_dir.join(format!("{}/bin/python3", venv_name))
    };

    // Find UV executable - check possible locations
    let uv_candidates = if cfg!(target_os = "windows") {
        vec![install_dir.join("python/Scripts/uv.exe")]
    } else {
        vec![install_dir.join("python/bin/uv")]
    };

    let uv_exe = uv_candidates.iter().find(|p| p.exists());

    let requirements_path = app.path()
        .resolve(&format!("resources/{}", requirements_file), tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to find {}: {}", requirements_file, e))?;

    emit_progress(app, "packages", 20, &format!("Installing {} packages with UV...", progress_label), false);

    let mut cmd = if let Some(uv_path) = uv_exe {
        eprintln!("[SETUP] Using UV at: {:?}", uv_path);
        Command::new(uv_path)
    } else {
        // Fallback to python -m uv if uv binary not found
        eprintln!("[SETUP] UV binary not found, using python -m uv");
        let mut cmd = Command::new(&python_exe);
        cmd.arg("-m").arg("uv");
        cmd
    };

    // On macOS, set LLVM and TA-Lib environment variables for package builds
    #[cfg(target_os = "macos")]
    {
        if let (true, Some(llvm_path)) = check_llvm(install_dir) {
            eprintln!("[SETUP] Setting LLVM environment variables for package build");
            cmd.env("LLVM_CONFIG", format!("{}/bin/llvm-config", llvm_path));
        }

        if let (true, Some(talib_path)) = check_talib(install_dir) {
            eprintln!("[SETUP] Setting TA-Lib environment variables for package build");
            cmd.env("TA_INCLUDE_PATH", format!("{}/include", talib_path));
            cmd.env("TA_LIBRARY_PATH", format!("{}/lib", talib_path));
            // Also set compiler flags to help find the headers
            let cflags = format!("-I{}/include", talib_path);
            let ldflags = format!("-L{}/lib", talib_path);
            cmd.env("CFLAGS", &cflags);
            cmd.env("LDFLAGS", &ldflags);
        }
    }

    cmd.args(&[
        "pip", "install",
        "--python", venv_python.to_str().unwrap(),
        "-r", requirements_path.to_str().unwrap()
    ]);
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    eprintln!("[SETUP] Installing {} packages: {:?}", progress_label, cmd);
    let output = cmd.output().map_err(|e| format!("Failed to install {} packages: {}", progress_label, e))?;

    eprintln!("[SETUP] {} stdout: {}", progress_label, String::from_utf8_lossy(&output.stdout));
    eprintln!("[SETUP] {} stderr: {}", progress_label, String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err(format!("{} package installation failed: {}", progress_label, String::from_utf8_lossy(&output.stderr)));
    }

    emit_progress(app, "packages", 100, &format!("{} packages installed", progress_label), false);
    Ok(())
}

/// Check setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    let install_dir = get_install_dir(&app)?;

    let (python_installed, _) = check_python(&install_dir);
    let (bun_installed, _) = check_bun(&install_dir);
    let (llvm_installed, _) = check_llvm(&install_dir);
    let (talib_installed, _) = check_talib(&install_dir);
    let packages_installed = check_packages(&install_dir);

    // On macOS, LLVM and TA-Lib are required for full package installation
    #[cfg(target_os = "macos")]
    let needs_setup = !python_installed || !bun_installed || !llvm_installed || !talib_installed || !packages_installed;

    #[cfg(not(target_os = "macos"))]
    let needs_setup = !python_installed || !bun_installed || !packages_installed;

    // Initialize worker pool in background if setup is already complete (non-blocking)
    // CRITICAL: Use tauri::async_runtime to ensure worker pool lives in Tauri's persistent runtime
    if !needs_setup && python_installed {
        eprintln!("[SETUP] Python already installed, initializing worker pool in background...");
        let app_clone = app.clone();
        tauri::async_runtime::spawn(async move {
            let install_dir = match get_install_dir(&app_clone) {
                Ok(dir) => dir,
                Err(e) => {
                    eprintln!("[SETUP] Failed to get install dir: {}", e);
                    return;
                }
            };

            if let Err(e) = crate::python_runtime::initialize_global_async(install_dir).await {
                eprintln!("[SETUP] Warning: Failed to initialize worker pool: {}", e);
            } else {
                eprintln!("[SETUP] Worker pool initialized successfully in background");
            }
        });
        eprintln!("[SETUP] Worker pool initialization started in background (non-blocking)");
    }

    Ok(SetupStatus {
        python_installed,
        bun_installed,
        packages_installed,
        needs_setup,
    })
}

/// Run complete setup
#[tauri::command]
pub async fn run_setup(app: AppHandle) -> Result<String, String> {
    // Check if setup is already running
    {
        let mut lock = SETUP_RUNNING.lock().unwrap();
        if *lock {
            return Err("Setup already running".to_string());
        }
        *lock = true;
    } // Lock dropped here

    let result = async {
        let install_dir = get_install_dir(&app)?;
        std::fs::create_dir_all(&install_dir)
            .map_err(|e| format!("Failed to create install dir: {}", e))?;

        eprintln!("[SETUP] Install directory: {}", install_dir.display());
        emit_progress(&app, "init", 0, "Starting setup...", false);

        // Python
        let (python_installed, python_version) = check_python(&install_dir);
        if !python_installed {
            install_python(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] Python already installed: {}", python_version.unwrap_or_default());
            emit_progress(&app, "python", 100, "Python already installed", false);
        }

        // Bun
        let (bun_installed, bun_version) = check_bun(&install_dir);
        if !bun_installed {
            install_bun(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] Bun already installed: {}", bun_version.unwrap_or_default());
            emit_progress(&app, "bun", 100, "Bun already installed", false);
        }

        // LLVM (macOS only, needed for llvmlite/numba in financial packages)
        let (llvm_installed, llvm_path) = check_llvm(&install_dir);
        if !llvm_installed {
            install_llvm(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] LLVM already installed: {}", llvm_path.unwrap_or_default());
            emit_progress(&app, "llvm", 100, "LLVM already installed", false);
        }

        // TA-Lib (needed for ta-lib Python wrapper used by vnpy)
        let (talib_installed, talib_path) = check_talib(&install_dir);
        if !talib_installed {
            install_talib(&app, &install_dir).await?;
        } else {
            eprintln!("[SETUP] TA-Lib already installed: {}", talib_path.unwrap_or_default());
            emit_progress(&app, "talib", 100, "TA-Lib already installed", false);
        }

        // UV (fast package manager)
        install_uv(&app, &install_dir).await?;

        // Create venvs and install packages
        if !check_packages(&install_dir) {
            // Create venv-numpy1 for NumPy 1.x libraries
            emit_progress(&app, "venv", 0, "Creating NumPy 1.x environment...", false);
            create_venv(&app, &install_dir, "venv-numpy1").await?;

            // Install numpy1 packages
            install_packages_in_venv(
                &app,
                &install_dir,
                "venv-numpy1",
                "requirements-numpy1.txt",
                "NumPy 1.x"
            ).await?;

            // Create venv-numpy2 for NumPy 2.x libraries
            emit_progress(&app, "venv", 50, "Creating NumPy 2.x environment...", false);
            create_venv(&app, &install_dir, "venv-numpy2").await?;

            // Install numpy2 packages
            install_packages_in_venv(
                &app,
                &install_dir,
                "venv-numpy2",
                "requirements-numpy2.txt",
                "NumPy 2.x"
            ).await?;
        } else {
            eprintln!("[SETUP] Packages already installed in both venvs");
            emit_progress(&app, "packages", 100, "Packages already installed", false);
        }

        emit_progress(&app, "complete", 100, "Setup complete!", false);

        // Initialize worker pool in background (non-blocking)
        // CRITICAL: Use tauri::async_runtime to ensure worker pool lives in Tauri's persistent runtime
        eprintln!("[SETUP] Initializing worker pool in background...");
        let install_dir_clone = install_dir.clone();
        tauri::async_runtime::spawn(async move {
            if let Err(e) = crate::python_runtime::initialize_global_async(install_dir_clone).await {
                eprintln!("[SETUP] Warning: Failed to initialize worker pool: {}", e);
            } else {
                eprintln!("[SETUP] Worker pool initialized successfully in background");
            }
        });
        eprintln!("[SETUP] Worker pool initialization started in background (non-blocking)");

        Ok("Setup complete".to_string())
    }.await;

    let mut lock = SETUP_RUNNING.lock().unwrap();
    *lock = false;

    result
}
