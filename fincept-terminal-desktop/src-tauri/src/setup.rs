// setup.rs - Simplified Python and Bun setup
use serde::{Deserialize, Serialize};
use sha2::{Sha256, Digest};
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
    pub needs_sync: bool,
    pub sync_message: Option<String>,
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

        // Use python-build-standalone release 20240726 (same as macOS)
        let download_url = format!("https://github.com/astral-sh/python-build-standalone/releases/download/20240726/cpython-3.12.4+20240726-{}.tar.gz", platform);
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

        // Use python-build-standalone release 20240726
        let download_url = format!("https://github.com/astral-sh/python-build-standalone/releases/download/20240726/cpython-3.12.4+20240726-{}.tar.gz", platform);
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
    // The embeddable Python has import site commented out by default, which prevents pip from working
    #[cfg(target_os = "windows")]
    {
        let pth_file = python_dir.join("python312._pth");
        eprintln!("[SETUP] Checking ._pth file at: {:?}", pth_file);

        if pth_file.exists() {
            if let Ok(content) = std::fs::read_to_string(&pth_file) {
                eprintln!("[SETUP] Original ._pth content:\n{}", content);

                // Enable import site (required for pip to work)
                let new_content = content.replace("#import site", "import site");

                // Also add Lib/site-packages to the path if not present
                let new_content = if !new_content.contains("Lib/site-packages") && !new_content.contains("Lib\\site-packages") {
                    format!("{}\nLib/site-packages\n", new_content.trim_end())
                } else {
                    new_content
                };

                eprintln!("[SETUP] New ._pth content:\n{}", new_content);

                std::fs::write(&pth_file, &new_content)
                    .map_err(|e| format!("Failed to write ._pth file: {}", e))?;
            }
        } else {
            // Create _pth file if it doesn't exist
            eprintln!("[SETUP] Creating new ._pth file");
            let pth_content = "python312.zip\n.\nLib/site-packages\n\nimport site\n";
            std::fs::write(&pth_file, pth_content)
                .map_err(|e| format!("Failed to create ._pth file: {}", e))?;
        }

        // Also create Lib/site-packages directory if it doesn't exist
        let site_packages = python_dir.join("Lib").join("site-packages");
        if !site_packages.exists() {
            std::fs::create_dir_all(&site_packages)
                .map_err(|e| format!("Failed to create site-packages directory: {}", e))?;
            eprintln!("[SETUP] Created site-packages directory: {:?}", site_packages);
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
            &format!(
                "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '{}' -OutFile '{}'",
                get_pip_url, get_pip_path.display()
            )
        ]);
        cmd.creation_flags(CREATE_NO_WINDOW);
        let output = cmd.output().map_err(|e| format!("Failed to download get-pip.py: {}", e))?;

        if !output.status.success() {
            eprintln!("[SETUP] get-pip.py download failed: {}", String::from_utf8_lossy(&output.stderr));
            return Err(format!("Failed to download get-pip.py: {}", String::from_utf8_lossy(&output.stderr)));
        }
    }

    #[cfg(not(target_os = "windows"))]
    {
        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", get_pip_path.to_str().unwrap(), get_pip_url]);
        let output = cmd.output().map_err(|e| format!("Failed to download get-pip.py: {}", e))?;

        if !output.status.success() {
            return Err(format!("Failed to download get-pip.py: {}", String::from_utf8_lossy(&output.stderr)));
        }
    }

    // Verify get-pip.py was downloaded
    if !get_pip_path.exists() {
        return Err("get-pip.py download failed - file does not exist".to_string());
    }

    let file_size = std::fs::metadata(&get_pip_path)
        .map(|m| m.len())
        .unwrap_or(0);
    eprintln!("[SETUP] get-pip.py downloaded, size: {} bytes", file_size);

    if file_size < 1000 {
        return Err(format!("get-pip.py appears corrupted (size: {} bytes)", file_size));
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

    eprintln!("[SETUP] Running: {:?} {:?}", python_exe, get_pip_path);

    let mut cmd = Command::new(&python_exe);
    cmd.arg(get_pip_path.to_str().unwrap());
    // Add --no-warn-script-location to suppress warnings
    cmd.arg("--no-warn-script-location");

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd.output().map_err(|e| format!("Failed to install pip: {}", e))?;

    eprintln!("[SETUP] [Python] pip install stdout: {}", String::from_utf8_lossy(&output.stdout));
    eprintln!("[SETUP] [Python] pip install stderr: {}", String::from_utf8_lossy(&output.stderr));

    if !output.status.success() {
        return Err(format!("Pip install failed: {}", String::from_utf8_lossy(&output.stderr)));
    }

    let _ = std::fs::remove_file(&get_pip_path);

    // Verify pip was installed successfully
    emit_progress(app, "python", 90, "Verifying pip installation...", false);

    let mut verify_cmd = Command::new(&python_exe);
    verify_cmd.args(&["-m", "pip", "--version"]);

    #[cfg(target_os = "windows")]
    verify_cmd.creation_flags(CREATE_NO_WINDOW);

    let verify_output = verify_cmd.output().map_err(|e| format!("Failed to verify pip: {}", e))?;

    if !verify_output.status.success() {
        eprintln!("[SETUP] pip verification failed: {}", String::from_utf8_lossy(&verify_output.stderr));
        return Err("Pip installation verification failed - pip module not found".to_string());
    }

    eprintln!("[SETUP] pip verified: {}", String::from_utf8_lossy(&verify_output.stdout).trim());

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

    // First verify pip is available
    let mut verify_cmd = Command::new(&python_exe);
    verify_cmd.args(&["-m", "pip", "--version"]);

    #[cfg(target_os = "windows")]
    verify_cmd.creation_flags(CREATE_NO_WINDOW);

    eprintln!("[SETUP] Verifying pip before UV install...");
    let verify_output = verify_cmd.output().map_err(|e| format!("Failed to check pip: {}", e))?;

    if !verify_output.status.success() {
        let stderr = String::from_utf8_lossy(&verify_output.stderr);
        eprintln!("[SETUP] pip not available: {}", stderr);
        return Err(format!("UV installation failed: {}: No module named pip", python_exe.display()));
    }

    eprintln!("[SETUP] pip available: {}", String::from_utf8_lossy(&verify_output.stdout).trim());

    // Now install UV
    let mut cmd = Command::new(&python_exe);
    cmd.args(&["-m", "pip", "install", "--no-warn-script-location", "uv"]);

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

    let venv_path_str = venv_path.to_str().ok_or_else(|| format!("Invalid venv path encoding: {:?}", venv_path))?;
    let python_exe_str = python_exe.to_str().ok_or_else(|| format!("Invalid python path encoding: {:?}", python_exe))?;
    cmd.args(&[
        "venv",
        venv_path_str,
        "--python", python_exe_str
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

    // tauri.conf.json maps resources/requirements-*.txt directly to requirements-*.txt
    let requirements_path = app.path()
        .resolve(requirements_file, tauri::path::BaseDirectory::Resource)
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

    let venv_python_str = venv_python.to_str().ok_or_else(|| format!("Invalid venv python path encoding: {:?}", venv_python))?;
    let requirements_path_str = requirements_path.to_str().ok_or_else(|| format!("Invalid requirements path encoding: {:?}", requirements_path))?;
    cmd.args(&[
        "pip", "install",
        "--python", venv_python_str,
        "-r", requirements_path_str
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

// ============================================================================
// Requirements Hash Tracking (for detecting changes on app update)
// ============================================================================

/// Compute SHA-256 hash of a bundled requirements file
fn compute_requirements_hash(app: &AppHandle, requirements_file: &str) -> Result<String, String> {
    // tauri.conf.json maps resources/requirements-*.txt directly to requirements-*.txt
    let requirements_path = app.path()
        .resolve(
            requirements_file,
            tauri::path::BaseDirectory::Resource,
        )
        .map_err(|e| format!("Failed to resolve {}: {}", requirements_file, e))?;

    let content = std::fs::read_to_string(&requirements_path)
        .map_err(|e| format!("Failed to read {}: {}", requirements_file, e))?;

    let mut hasher = Sha256::new();
    hasher.update(content.as_bytes());
    let result = hasher.finalize();
    Ok(format!("{:x}", result))
}

/// Check if bundled requirements files have changed since last install.
/// Returns (needs_sync, description of what changed)
fn check_requirements_sync(app: &AppHandle) -> (bool, Option<String>) {
    let files = [
        ("requirements-numpy1.txt", "requirements_numpy1_hash"),
        ("requirements-numpy2.txt", "requirements_numpy2_hash"),
    ];

    let mut changed: Vec<&str> = Vec::new();

    for (file, setting_key) in &files {
        match compute_requirements_hash(app, file) {
            Ok(current_hash) => {
                let stored_hash = crate::database::operations::get_setting(setting_key)
                    .ok()
                    .flatten();

                match stored_hash {
                    Some(stored) if stored == current_hash => {
                        eprintln!("[SETUP] {} hash matches stored value", file);
                    }
                    _ => {
                        eprintln!(
                            "[SETUP] {} hash mismatch or missing (stored: {:?}, current: {}...)",
                            file,
                            stored_hash.as_deref().map(|s| &s[..s.len().min(16)]),
                            &current_hash[..current_hash.len().min(16)]
                        );
                        changed.push(file);
                    }
                }
            }
            Err(e) => {
                eprintln!("[SETUP] Could not compute hash for {}: {}", file, e);
            }
        }
    }

    if changed.is_empty() {
        (false, None)
    } else {
        let msg = format!("Package updates available: {}", changed.join(", "));
        (true, Some(msg))
    }
}

/// Save the current hash of a requirements file to the database
fn save_requirements_hash(app: &AppHandle, requirements_file: &str, setting_key: &str) -> Result<(), String> {
    let hash = compute_requirements_hash(app, requirements_file)?;
    crate::database::operations::save_setting(setting_key, &hash, Some("setup"))
        .map_err(|e| format!("Failed to save hash for {}: {}", requirements_file, e))
}

/// Check setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    let install_dir = get_install_dir(&app)?;

    let (python_installed, _) = check_python(&install_dir);
    let (bun_installed, _) = check_bun(&install_dir);
    let packages_installed = check_packages(&install_dir);

    let needs_setup = !python_installed || !bun_installed || !packages_installed;

    // Check if requirements files have changed (only relevant if base setup is complete)
    let (needs_sync, sync_message) = if !needs_setup {
        check_requirements_sync(&app)
    } else {
        (false, None)
    };

    // Initialize Python runtime in background if setup is complete and no sync needed
    if !needs_setup && !needs_sync && python_installed {
        let app_clone = app.clone();
        tauri::async_runtime::spawn(async move {
            if let Err(e) = crate::python::initialize(&app_clone).await {
                eprintln!("[Setup] Python initialization failed: {}", e);
            }
        });
    }

    Ok(SetupStatus {
        python_installed,
        bun_installed,
        packages_installed,
        needs_setup,
        needs_sync,
        sync_message,
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

        // Save requirements hashes so future updates can detect changes
        if let Err(e) = save_requirements_hash(&app, "requirements-numpy1.txt", "requirements_numpy1_hash") {
            eprintln!("[SETUP] Warning: failed to save numpy1 requirements hash: {}", e);
        }
        if let Err(e) = save_requirements_hash(&app, "requirements-numpy2.txt", "requirements_numpy2_hash") {
            eprintln!("[SETUP] Warning: failed to save numpy2 requirements hash: {}", e);
        }

        emit_progress(&app, "complete", 100, "Setup complete!", false);

        // Initialize Python runtime
        if let Err(e) = crate::python::initialize(&app).await {
            eprintln!("[Setup] Python initialization warning: {}", e);
        }

        Ok("Setup complete".to_string())
    }.await;

    let mut lock = SETUP_RUNNING.lock().unwrap();
    *lock = false;

    result
}

/// Sync requirements: run UV install for any venv whose requirements changed.
/// Called automatically on app update when requirements files differ from stored hashes.
/// This is lightweight — UV only installs what's missing/changed.
#[tauri::command]
pub async fn sync_requirements(app: AppHandle) -> Result<String, String> {
    let install_dir = get_install_dir(&app)?;

    let files_and_venvs = [
        ("requirements-numpy1.txt", "requirements_numpy1_hash", "venv-numpy1", "NumPy 1.x"),
        ("requirements-numpy2.txt", "requirements_numpy2_hash", "venv-numpy2", "NumPy 2.x"),
    ];

    let mut synced_count = 0;

    for (req_file, setting_key, venv_name, label) in &files_and_venvs {
        let current_hash = match compute_requirements_hash(&app, req_file) {
            Ok(h) => h,
            Err(e) => {
                eprintln!("[SYNC] Could not compute hash for {}: {}", req_file, e);
                continue;
            }
        };

        let stored_hash = crate::database::operations::get_setting(setting_key)
            .ok()
            .flatten();

        let needs_update = match &stored_hash {
            Some(stored) => *stored != current_hash,
            None => true,
        };

        if needs_update {
            eprintln!("[SYNC] Syncing {} packages...", label);
            emit_progress(&app, "sync", synced_count * 50, &format!("Syncing {} packages...", label), false);

            // Ensure UV is available
            if let Err(e) = install_uv(&app, &install_dir).await {
                eprintln!("[SYNC] UV install warning: {}", e);
            }

            // Ensure venv exists; if not, create it
            let venv_python = if cfg!(target_os = "windows") {
                install_dir.join(format!("{}/Scripts/python.exe", venv_name))
            } else {
                install_dir.join(format!("{}/bin/python3", venv_name))
            };

            if !venv_python.exists() {
                eprintln!("[SYNC] Venv {} missing, creating...", venv_name);
                create_venv(&app, &install_dir, venv_name).await?;
            }

            // Run UV pip install (incremental — only installs what's missing/changed)
            install_packages_in_venv(&app, &install_dir, venv_name, req_file, label).await?;

            // Save the new hash on success
            save_requirements_hash(&app, req_file, setting_key)?;
            synced_count += 1;

            eprintln!("[SYNC] Successfully synced {} packages", label);
        }
    }

    emit_progress(&app, "sync", 100, "Package sync complete", false);

    // Initialize Python runtime after sync
    if let Err(e) = crate::python::initialize(&app).await {
        eprintln!("[SYNC] Python initialization warning after sync: {}", e);
    }

    Ok(format!("Synced {} requirement files", synced_count))
}
