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
        app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource dir: {}", e))
    }
}

/// Check Python installation
fn check_python(install_dir: &PathBuf) -> (bool, Option<String>) {
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
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
            &format!("Invoke-WebRequest -Uri '{}' -OutFile '{}'", download_url, zip_path.display())
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
        // Download official Python.org universal2 installer
        let download_url = format!("https://www.python.org/ftp/python/{}/python-{}-macos11.pkg", PYTHON_VERSION, PYTHON_VERSION);
        let pkg_path = python_dir.join("python.pkg");

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", pkg_path.to_str().unwrap(), &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "python", 60, "Extracting Python...", false);

        // Extract pkg using xar (works better than pkgutil)
        let temp_extract = std::env::temp_dir().join(format!("fincept_py_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&temp_extract);
        std::fs::create_dir_all(&temp_extract)
            .map_err(|e| format!("Failed to create temp dir: {}", e))?;

        let mut cmd = Command::new("xar");
        cmd.args(&["-xf", pkg_path.to_str().unwrap(), "-C", temp_extract.to_str().unwrap()]);
        let output = cmd.output().map_err(|e| format!("Extract failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Extract failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        // Find and extract Python Framework payload
        let payload_path = temp_extract.join("Python_Framework.pkg/Payload");
        if payload_path.exists() {
            let mut cmd = Command::new("tar");
            cmd.args(&["-xzf", payload_path.to_str().unwrap(), "-C", python_dir.to_str().unwrap(), "--strip-components=0"]);
            let output = cmd.output().map_err(|e| format!("Payload extract failed: {}", e))?;
            if !output.status.success() {
                return Err(format!("Payload extract failed: {}", String::from_utf8_lossy(&output.stderr)));
            }

            // Also check what was actually extracted
            eprintln!("[SETUP] Checking extracted files...");
            if let Ok(entries) = std::fs::read_dir(&python_dir) {
                for entry in entries.flatten() {
                    eprintln!("[SETUP]   - {:?}", entry.path());
                }
            }
        } else {
            return Err("Python Framework payload not found in pkg".to_string());
        }

        // Cleanup
        let _ = std::fs::remove_file(&pkg_path);
        let _ = std::fs::remove_dir_all(&temp_extract);
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
    } else if cfg!(target_os = "macos") {
        // macOS: Python extracted from .pkg has Versions/3.12/bin/python3
        let versions_python = python_dir.join(format!("Versions/{}/bin/python3", &PYTHON_VERSION[..4]));
        let bin_python = python_dir.join("bin/python3");
        if versions_python.exists() {
            versions_python
        } else if bin_python.exists() {
            bin_python
        } else {
            return Err(format!("Python executable not found. Checked:\n  - {:?}\n  - {:?}", versions_python, bin_python));
        }
    } else {
        python_dir.join("bin/python3")
    };

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
            &format!("Invoke-WebRequest -Uri '{}' -OutFile '{}'", download_url, zip_path.display())
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

        // Move bun from nested folder to bun/ root and make executable
        let nested_bun = bun_dir.join("bun-linux-x64/bun");
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

    let pip_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/Scripts/pip.exe")
    } else if cfg!(target_os = "macos") {
        install_dir.join("python/Versions/3.12/bin/pip3")
    } else {
        install_dir.join("python/bin/pip")
    };

    let mut cmd = Command::new(&pip_exe);
    cmd.args(&["install", "uv"]);
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
        install_dir.join("python/bin/python3")
    };

    let uv_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/Scripts/uv.exe")
    } else {
        install_dir.join("python/bin/uv")
    };

    let venv_path = install_dir.join(venv_name);

    let mut cmd = Command::new(&uv_exe);
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

    let venv_python = if cfg!(target_os = "windows") {
        install_dir.join(format!("{}/Scripts/python.exe", venv_name))
    } else {
        install_dir.join(format!("{}/bin/python3", venv_name))
    };

    let uv_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/Scripts/uv.exe")
    } else {
        install_dir.join("python/bin/uv")
    };

    let requirements_path = app.path()
        .resolve(&format!("resources/{}", requirements_file), tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to find {}: {}", requirements_file, e))?;

    emit_progress(app, "packages", 20, &format!("Installing {} packages with UV...", progress_label), false);

    let mut cmd = Command::new(&uv_exe);
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
    let packages_installed = check_packages(&install_dir);

    let needs_setup = !python_installed || !bun_installed || !packages_installed;

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
        Ok("Setup complete".to_string())
    }.await;

    let mut lock = SETUP_RUNNING.lock().unwrap();
    *lock = false;

    result
}
