// setup.rs - Clean setup for Python, Bun, and UV package manager
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Command;
use std::fs;
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
    pub uv_installed: bool,
    pub packages_installed: bool,
    pub needs_setup: bool,
}

const PYTHON_VERSION: &str = "3.12.7";
const BUN_VERSION: &str = "1.1.0";
const UV_VERSION: &str = "0.9.18";

fn emit_progress(app: &AppHandle, step: &str, progress: u8, message: &str, is_error: bool) {
    let _ = app.emit("setup-progress", SetupProgress {
        step: step.to_string(),
        progress,
        message: message.to_string(),
        is_error,
    });
    eprintln!("[SETUP] [{}] {}% - {}", step, progress, message);
}

/// Detect if running in dev mode
fn is_dev_mode() -> bool {
    cfg!(debug_assertions)
}

/// Get installation directory based on mode
fn get_install_dir(app: &AppHandle) -> Result<PathBuf, String> {
    if is_dev_mode() {
        // Dev mode: use OS-specific user directory (no admin needed)
        let base_dir = if cfg!(target_os = "windows") {
            std::env::var("LOCALAPPDATA")
                .map(PathBuf::from)
                .unwrap_or_else(|_| PathBuf::from("C:\\Users\\Default\\AppData\\Local"))
        } else if cfg!(target_os = "macos") {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join("Library/Application Support"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        } else {
            // Linux
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join(".local/share"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        };
        Ok(base_dir.join("fincept-dev"))
    } else {
        // Production: use app resource directory
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
        install_dir.join("bun/bun")
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

/// Check UV installation
fn check_uv(install_dir: &PathBuf) -> bool {
    let uv_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/Scripts/uv.exe")
    } else {
        install_dir.join("python/bin/uv")
    };
    uv_exe.exists()
}

/// Check if packages are installed
fn check_packages(install_dir: &PathBuf) -> bool {
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python/python.exe")
    } else {
        install_dir.join("python/bin/python3")
    };

    if !python_exe.exists() {
        return false;
    }

    let mut cmd = Command::new(&python_exe);
    cmd.args(&["-c", "import yfinance"]);
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    cmd.output().map(|o| o.status.success()).unwrap_or(false)
}

/// Download file with progress
async fn download_file(url: &str, dest: &PathBuf, app: &AppHandle, step: &str) -> Result<(), String> {
    use reqwest;
    use std::io::Write;
    use std::time::Duration;

    emit_progress(app, step, 10, &format!("Downloading {}", url.split('/').last().unwrap_or("file")), false);

    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(300))
        .build()
        .map_err(|e| format!("Failed to create client: {}", e))?;

    let response = client.get(url).send().await
        .map_err(|e| format!("Download failed: {}", e))?;

    let bytes = response.bytes().await
        .map_err(|e| format!("Failed to read response: {}", e))?;

    let mut file = fs::File::create(dest)
        .map_err(|e| format!("Failed to create file: {}", e))?;

    file.write_all(&bytes)
        .map_err(|e| format!("Failed to write file: {}", e))?;

    emit_progress(app, step, 30, "Download complete", false);
    Ok(())
}

/// Install Python (all platforms)
async fn install_python(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "python", 0, "Installing Python...", false);

    let python_dir = install_dir.join("python");
    fs::create_dir_all(&python_dir)
        .map_err(|e| format!("Failed to create python dir: {}", e))?;

    let temp_dir = std::env::temp_dir();

    #[cfg(target_os = "windows")]
    {
        // Windows: Download embedded Python ZIP
        let url = "https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip";
        let zip_path = temp_dir.join("python.zip");
        download_file(url, &zip_path, app, "python").await?;

        emit_progress(app, "python", 40, "Extracting Python...", false);
        extract_zip(&zip_path, &python_dir)?;

        // Enable site-packages
        let pth_file = python_dir.join("python312._pth");
        if pth_file.exists() {
            let mut content = fs::read_to_string(&pth_file)
                .map_err(|e| format!("Failed to read _pth: {}", e))?;
            content = content.replace("#import site", "import site");
            content.push_str("\nLib\\site-packages\n");
            fs::write(&pth_file, content)
                .map_err(|e| format!("Failed to write _pth: {}", e))?;
        }

        fs::remove_file(zip_path).ok();
    }

    #[cfg(not(target_os = "windows"))]
    {
        // macOS/Linux: Download standalone Python
        let url = if cfg!(target_os = "macos") {
            if cfg!(target_arch = "aarch64") {
                "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-aarch64-apple-darwin-install_only_stripped.tar.gz"
            } else {
                "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-x86_64-apple-darwin-install_only_stripped.tar.gz"
            }
        } else {
            if cfg!(target_arch = "aarch64") {
                "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-aarch64-unknown-linux-gnu-install_only_stripped.tar.gz"
            } else {
                "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz"
            }
        };

        let tar_path = temp_dir.join("python.tar.gz");
        download_file(url, &tar_path, app, "python").await?;

        emit_progress(app, "python", 40, "Extracting Python...", false);
        Command::new("tar")
            .args(&["-xzf", tar_path.to_str().unwrap(), "-C", python_dir.to_str().unwrap(), "--strip-components=1"])
            .output()
            .map_err(|e| format!("Failed to extract: {}", e))?;

        let python_exe = python_dir.join("bin/python3");
        if python_exe.exists() {
            Command::new("chmod").args(&["+x", python_exe.to_str().unwrap()]).output().ok();
        }

        fs::remove_file(tar_path).ok();
    }

    emit_progress(app, "python", 60, "Python extracted", false);

    // Verify
    let (installed, version) = check_python(install_dir);
    if !installed {
        return Err("Python verification failed".to_string());
    }

    emit_progress(app, "python", 70, &format!("Python {} verified", version.unwrap_or_default()), false);
    Ok(())
}

/// Extract ZIP (Windows only)
#[cfg(target_os = "windows")]
fn extract_zip(zip_path: &PathBuf, dest_dir: &PathBuf) -> Result<(), String> {
    use zip::ZipArchive;
    use std::io::copy;

    let file = fs::File::open(zip_path)
        .map_err(|e| format!("Failed to open ZIP: {}", e))?;
    let mut archive = ZipArchive::new(file)
        .map_err(|e| format!("Failed to read ZIP: {}", e))?;

    for i in 0..archive.len() {
        let mut file = archive.by_index(i)
            .map_err(|e| format!("Failed to read entry: {}", e))?;
        let outpath = match file.enclosed_name() {
            Some(path) => dest_dir.join(path),
            None => continue,
        };

        if file.name().ends_with('/') {
            fs::create_dir_all(&outpath)
                .map_err(|e| format!("Failed to create dir: {}", e))?;
        } else {
            if let Some(p) = outpath.parent() {
                fs::create_dir_all(p)
                    .map_err(|e| format!("Failed to create parent: {}", e))?;
            }
            let mut outfile = fs::File::create(&outpath)
                .map_err(|e| format!("Failed to create file: {}", e))?;
            copy(&mut file, &mut outfile)
                .map_err(|e| format!("Failed to extract: {}", e))?;
        }
    }
    Ok(())
}

/// Install Bun (all platforms)
async fn install_bun(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "bun", 0, "Installing Bun...", false);

    let bun_dir = install_dir.join("bun");
    fs::create_dir_all(&bun_dir)
        .map_err(|e| format!("Failed to create bun dir: {}", e))?;

    let temp_dir = std::env::temp_dir();

    #[cfg(target_os = "windows")]
    {
        let url = if cfg!(target_arch = "aarch64") {
            format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-windows-aarch64.zip", BUN_VERSION)
        } else {
            format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-windows-x64.zip", BUN_VERSION)
        };

        let zip_path = temp_dir.join("bun.zip");
        download_file(&url, &zip_path, app, "bun").await?;

        emit_progress(app, "bun", 40, "Extracting Bun...", false);
        let temp_extract = temp_dir.join("bun_temp");
        fs::create_dir_all(&temp_extract).ok();
        extract_zip(&zip_path, &temp_extract)?;

        // Move bun.exe from nested folder to bun_dir
        if let Ok(entries) = fs::read_dir(&temp_extract) {
            for entry in entries.flatten() {
                let bun_exe = entry.path().join("bun.exe");
                if bun_exe.exists() {
                    fs::copy(&bun_exe, bun_dir.join("bun.exe"))
                        .map_err(|e| format!("Failed to copy bun.exe: {}", e))?;
                    break;
                }
            }
        }

        fs::remove_file(zip_path).ok();
        fs::remove_dir_all(temp_extract).ok();
    }

    #[cfg(not(target_os = "windows"))]
    {
        let url = if cfg!(target_os = "macos") {
            if cfg!(target_arch = "aarch64") {
                format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-darwin-aarch64.zip", BUN_VERSION)
            } else {
                format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-darwin-x64.zip", BUN_VERSION)
            }
        } else {
            if cfg!(target_arch = "aarch64") {
                format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-linux-aarch64.zip", BUN_VERSION)
            } else {
                format!("https://github.com/oven-sh/bun/releases/download/bun-v{}/bun-linux-x64.zip", BUN_VERSION)
            }
        };

        let zip_path = temp_dir.join("bun.zip");
        download_file(&url, &zip_path, app, "bun").await?;

        emit_progress(app, "bun", 40, "Extracting Bun...", false);

        let temp_extract = temp_dir.join("bun_temp");
        fs::create_dir_all(&temp_extract).ok();

        Command::new("unzip")
            .args(&["-o", zip_path.to_str().unwrap(), "-d", temp_extract.to_str().unwrap()])
            .output()
            .map_err(|e| format!("Failed to extract: {}", e))?;

        // Find and move bun executable from nested folder
        fn find_bun_executable(dir: &PathBuf) -> Option<PathBuf> {
            if let Ok(entries) = fs::read_dir(dir) {
                for entry in entries.flatten() {
                    let path = entry.path();
                    if path.is_file() && path.file_name().map(|n| n == "bun").unwrap_or(false) {
                        return Some(path);
                    } else if path.is_dir() {
                        if let Some(found) = find_bun_executable(&path) {
                            return Some(found);
                        }
                    }
                }
            }
            None
        }

        if let Some(bun_file) = find_bun_executable(&temp_extract) {
            let bun_dest = bun_dir.join("bun");
            fs::copy(&bun_file, &bun_dest)
                .map_err(|e| format!("Failed to copy bun: {}", e))?;
            Command::new("chmod").args(&["+x", bun_dest.to_str().unwrap()]).output().ok();
        } else {
            return Err("Bun executable not found in archive".to_string());
        }

        fs::remove_file(zip_path).ok();
        fs::remove_dir_all(temp_extract).ok();
    }

    emit_progress(app, "bun", 60, "Bun extracted", false);

    // Verify
    let (installed, version) = check_bun(install_dir);
    if !installed {
        return Err("Bun verification failed".to_string());
    }

    emit_progress(app, "bun", 70, &format!("Bun {} verified", version.unwrap_or_default()), false);
    Ok(())
}

/// Install UV binary (all platforms)
async fn install_uv(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "uv", 0, "Installing UV package manager...", false);

    let temp_dir = std::env::temp_dir();

    #[cfg(target_os = "windows")]
    {
        let url = if cfg!(target_arch = "aarch64") {
            format!("https://github.com/astral-sh/uv/releases/download/{}/uv-aarch64-pc-windows-msvc.zip", UV_VERSION)
        } else {
            format!("https://github.com/astral-sh/uv/releases/download/{}/uv-x86_64-pc-windows-msvc.zip", UV_VERSION)
        };

        let zip_path = temp_dir.join("uv.zip");
        download_file(&url, &zip_path, app, "uv").await?;

        emit_progress(app, "uv", 40, "Extracting UV...", false);

        let scripts_dir = install_dir.join("python/Scripts");
        fs::create_dir_all(&scripts_dir)
            .map_err(|e| format!("Failed to create Scripts dir: {}", e))?;

        use zip::ZipArchive;
        use std::io::copy;

        let file = fs::File::open(&zip_path)
            .map_err(|e| format!("Failed to open uv ZIP: {}", e))?;
        let mut archive = ZipArchive::new(file)
            .map_err(|e| format!("Failed to read uv ZIP: {}", e))?;

        let mut found = false;
        for i in 0..archive.len() {
            let mut zip_file = archive.by_index(i)
                .map_err(|e| format!("Failed to read ZIP entry: {}", e))?;

            if zip_file.name().ends_with("uv.exe") {
                let uv_dest = scripts_dir.join("uv.exe");
                let mut dest_file = fs::File::create(&uv_dest)
                    .map_err(|e| format!("Failed to create uv.exe: {}", e))?;
                copy(&mut zip_file, &mut dest_file)
                    .map_err(|e| format!("Failed to extract uv.exe: {}", e))?;
                found = true;
                break;
            }
        }

        if !found {
            return Err("uv.exe not found in archive".to_string());
        }

        fs::remove_file(zip_path).ok();
    }

    #[cfg(not(target_os = "windows"))]
    {
        let url = if cfg!(target_os = "macos") {
            if cfg!(target_arch = "aarch64") {
                format!("https://github.com/astral-sh/uv/releases/download/{}/uv-aarch64-apple-darwin.tar.gz", UV_VERSION)
            } else {
                format!("https://github.com/astral-sh/uv/releases/download/{}/uv-x86_64-apple-darwin.tar.gz", UV_VERSION)
            }
        } else {
            if cfg!(target_arch = "aarch64") {
                format!("https://github.com/astral-sh/uv/releases/download/{}/uv-aarch64-unknown-linux-gnu.tar.gz", UV_VERSION)
            } else {
                format!("https://github.com/astral-sh/uv/releases/download/{}/uv-x86_64-unknown-linux-gnu.tar.gz", UV_VERSION)
            }
        };

        let tar_path = temp_dir.join("uv.tar.gz");
        download_file(&url, &tar_path, app, "uv").await?;

        emit_progress(app, "uv", 40, "Extracting UV...", false);

        let bin_dir = install_dir.join("python/bin");
        fs::create_dir_all(&bin_dir)
            .map_err(|e| format!("Failed to create bin dir: {}", e))?;

        let temp_extract_dir = temp_dir.join("uv_extract");
        fs::create_dir_all(&temp_extract_dir)
            .map_err(|e| format!("Failed to create temp dir: {}", e))?;

        Command::new("tar")
            .args(&["-xzf", tar_path.to_str().unwrap(), "-C", temp_extract_dir.to_str().unwrap()])
            .output()
            .map_err(|e| format!("Failed to extract uv: {}", e))?;

        let mut found = false;
        if let Ok(entries) = fs::read_dir(&temp_extract_dir) {
            for entry in entries.flatten() {
                let uv_file = entry.path().join("uv");
                if uv_file.exists() {
                    let uv_dest = bin_dir.join("uv");
                    fs::copy(&uv_file, &uv_dest)
                        .map_err(|e| format!("Failed to copy uv: {}", e))?;
                    Command::new("chmod").args(&["+x", uv_dest.to_str().unwrap()]).output().ok();
                    found = true;
                    break;
                }
            }
        }

        if !found {
            return Err("uv binary not found in archive".to_string());
        }

        fs::remove_file(tar_path).ok();
        fs::remove_dir_all(temp_extract_dir).ok();
    }

    emit_progress(app, "uv", 60, "UV extracted", false);

    // Verify
    if !check_uv(install_dir) {
        return Err("UV verification failed".to_string());
    }

    emit_progress(app, "uv", 70, "UV verified", false);
    Ok(())
}

/// Install Python packages using UV
async fn install_packages(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
    emit_progress(app, "packages", 0, "Installing Python packages...", false);

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

    if !uv_exe.exists() {
        return Err(format!("UV not found at: {}", uv_exe.display()));
    }

    let requirements_path = app.path()
        .resolve("resources/requirements.txt", tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to find requirements.txt: {}", e))?;

    eprintln!("[SETUP] Python: {}", python_exe.display());
    eprintln!("[SETUP] UV: {}", uv_exe.display());
    eprintln!("[SETUP] Requirements: {}", requirements_path.display());

    let mut cmd = Command::new(&uv_exe);
    cmd.args(&["pip", "install", "--python", python_exe.to_str().unwrap(), "-r", requirements_path.to_str().unwrap()]);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    emit_progress(app, "packages", 20, "Running UV pip install...", false);

    eprintln!("[SETUP] Running: {:?}", cmd);

    let output = cmd.output()
        .map_err(|e| format!("Failed to run UV: {}", e))?;

    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);

    eprintln!("[SETUP] UV stdout: {}", stdout);
    eprintln!("[SETUP] UV stderr: {}", stderr);

    if !output.status.success() {
        return Err(format!("Package installation failed: {}", stderr));
    }

    emit_progress(app, "packages", 80, "Packages installed", false);

    // Verify
    if !check_packages(install_dir) {
        return Err("Package verification failed".to_string());
    }

    emit_progress(app, "packages", 100, "Packages verified", false);
    Ok(())
}

/// Check setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    let install_dir = get_install_dir(&app)?;

    let (python_installed, _) = check_python(&install_dir);
    let (bun_installed, _) = check_bun(&install_dir);
    let uv_installed = check_uv(&install_dir);
    let packages_installed = check_packages(&install_dir);

    let needs_setup = !python_installed || !bun_installed || !uv_installed || !packages_installed;

    Ok(SetupStatus {
        python_installed,
        bun_installed,
        uv_installed,
        packages_installed,
        needs_setup,
    })
}

/// Run complete setup
#[tauri::command]
pub async fn run_setup(app: AppHandle) -> Result<(), String> {
    // Prevent concurrent setup runs
    {
        let mut running = SETUP_RUNNING.lock().unwrap();
        if *running {
            return Err("Setup already running".to_string());
        }
        *running = true;
    }

    let result = run_setup_internal(app).await;

    // Release lock
    *SETUP_RUNNING.lock().unwrap() = false;
    result
}

async fn run_setup_internal(app: AppHandle) -> Result<(), String> {
    let install_dir = get_install_dir(&app)?;
    eprintln!("[SETUP] Install directory: {}", install_dir.display());
    eprintln!("[SETUP] Dev mode: {}", is_dev_mode());

    fs::create_dir_all(&install_dir)
        .map_err(|e| format!("Failed to create install dir: {}", e))?;

    let status = check_setup_status(app.clone())?;

    // Step 1: Install Python
    if !status.python_installed {
        install_python(&app, &install_dir).await?;
        emit_progress(&app, "python", 100, "Python complete", false);
    } else {
        emit_progress(&app, "python", 100, "Python already installed", false);
    }

    // Step 2: Install Bun
    if !status.bun_installed {
        install_bun(&app, &install_dir).await?;
        emit_progress(&app, "bun", 100, "Bun complete", false);
    } else {
        emit_progress(&app, "bun", 100, "Bun already installed", false);
    }

    // Step 3: Install UV
    if !status.uv_installed {
        install_uv(&app, &install_dir).await?;
        emit_progress(&app, "uv", 100, "UV complete", false);
    } else {
        emit_progress(&app, "uv", 100, "UV already installed", false);
    }

    // Step 4: Install packages
    if !status.packages_installed {
        install_packages(&app, &install_dir).await?;
    } else {
        emit_progress(&app, "packages", 100, "Packages already installed", false);
    }

    emit_progress(&app, "complete", 100, "Setup complete!", false);
    Ok(())
}
