// setup.rs - First-time setup for Python and Bun installation
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Command;
use tauri::{AppHandle, Emitter, Manager};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SetupProgress {
    pub step: String,
    pub progress: u8, // 0-100
    pub message: String,
    pub is_error: bool,
}

#[derive(Debug, Clone, Serialize)]
pub struct SetupStatus {
    pub python_installed: bool,
    pub bun_installed: bool,
    pub python_version: Option<String>,
    pub bun_version: Option<String>,
    pub needs_setup: bool,
}

// Python 3.12.7 download URLs
const PYTHON_VERSION: &str = "3.12.7";
const PYTHON_WINDOWS_URL: &str = "https://www.python.org/ftp/python/3.12.7/python-3.12.7-amd64.exe";
const PYTHON_MACOS_URL: &str = "https://www.python.org/ftp/python/3.12.7/python-3.12.7-macos11.pkg";
const PYTHON_LINUX_URL: &str = ""; // Linux uses package manager

// Bun download URLs
const BUN_WINDOWS_URL: &str = "https://github.com/oven-sh/bun/releases/latest/download/bun-windows-x64.zip";
const BUN_MACOS_URL: &str = ""; // Bun install script
const BUN_LINUX_URL: &str = ""; // Bun install script

/// Check if Python is installed (check app directory first, then system)
pub fn check_python_installed_in_app(app: &AppHandle) -> (bool, Option<String>) {
    // First check in app's resource directory
    if let Ok(resource_dir) = app.path().resource_dir() {
        let python_exe = if cfg!(target_os = "windows") {
            resource_dir.join("python").join("python.exe")
        } else {
            resource_dir.join("python").join("bin").join("python3")
        };

        if python_exe.exists() {
            match Command::new(&python_exe)
                .arg("--version")
                .output()
            {
                Ok(output) if output.status.success() => {
                    let version = String::from_utf8_lossy(&output.stdout)
                        .trim()
                        .to_string();
                    return (true, Some(version));
                }
                _ => {}
            }
        }
    }

    // Fallback: check system Python
    check_python_installed()
}

/// Check system Python installation
pub fn check_python_installed() -> (bool, Option<String>) {
    #[cfg(target_os = "windows")]
    let python_cmd = "python";

    #[cfg(not(target_os = "windows"))]
    let python_cmd = "python3";

    match Command::new(python_cmd)
        .arg("--version")
        .output()
    {
        Ok(output) if output.status.success() => {
            let version = String::from_utf8_lossy(&output.stdout)
                .trim()
                .to_string();
            (true, Some(version))
        }
        _ => (false, None),
    }
}

/// Check if Bun is installed and get version
pub fn check_bun_installed() -> (bool, Option<String>) {
    match Command::new("bun")
        .arg("--version")
        .output()
    {
        Ok(output) if output.status.success() => {
            let version = String::from_utf8_lossy(&output.stdout)
                .trim()
                .to_string();
            (true, Some(version))
        }
        _ => (false, None),
    }
}

/// Check overall setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    let (python_installed, python_version) = check_python_installed_in_app(&app);
    let (bun_installed, bun_version) = check_bun_installed_in_app(&app);

    let needs_setup = !python_installed || !bun_installed;

    Ok(SetupStatus {
        python_installed,
        bun_installed,
        python_version,
        bun_version,
        needs_setup,
    })
}

/// Check if Bun is installed in app directory
pub fn check_bun_installed_in_app(app: &AppHandle) -> (bool, Option<String>) {
    // First check in app's resource directory
    if let Ok(resource_dir) = app.path().resource_dir() {
        let bun_exe = if cfg!(target_os = "windows") {
            resource_dir.join("bun").join("bun.exe")
        } else {
            resource_dir.join("bun").join("bin").join("bun")
        };

        if bun_exe.exists() {
            match Command::new(&bun_exe)
                .arg("--version")
                .output()
            {
                Ok(output) if output.status.success() => {
                    let version = String::from_utf8_lossy(&output.stdout)
                        .trim()
                        .to_string();
                    return (true, Some(version));
                }
                _ => {}
            }
        }
    }

    // Fallback: check system Bun
    check_bun_installed()
}

/// Download file with progress reporting
async fn download_file(
    url: &str,
    dest: &PathBuf,
    app: &AppHandle,
    step_name: &str,
) -> Result<(), String> {
    use reqwest;
    use std::fs::File;
    use std::io::Write;

    emit_progress(app, step_name, 0, &format!("Downloading from {}...", url), false);

    let client = reqwest::Client::new();
    let response = client
        .get(url)
        .send()
        .await
        .map_err(|e| format!("Download failed: {}", e))?;

    let total_size = response.content_length().unwrap_or(0);

    let mut file = File::create(dest)
        .map_err(|e| format!("Failed to create file: {}", e))?;

    let mut downloaded: u64 = 0;
    let mut stream = response.bytes_stream();

    use futures_util::StreamExt;

    while let Some(chunk) = stream.next().await {
        let chunk = chunk.map_err(|e| format!("Download error: {}", e))?;
        file.write_all(&chunk)
            .map_err(|e| format!("Write error: {}", e))?;

        downloaded += chunk.len() as u64;

        if total_size > 0 {
            let progress = ((downloaded as f64 / total_size as f64) * 100.0) as u8;
            emit_progress(
                app,
                step_name,
                progress,
                &format!("Downloaded {} / {} MB", downloaded / 1_000_000, total_size / 1_000_000),
                false,
            );
        }
    }

    emit_progress(app, step_name, 100, "Download complete", false);
    Ok(())
}

/// Emit progress event to frontend
fn emit_progress(app: &AppHandle, step: &str, progress: u8, message: &str, is_error: bool) {
    let progress_data = SetupProgress {
        step: step.to_string(),
        progress,
        message: message.to_string(),
        is_error,
    };

    let _ = app.emit("setup-progress", progress_data);
}

/// Install Python on Windows in app directory (portable, no admin)
#[cfg(target_os = "windows")]
async fn install_python_windows(app: &AppHandle) -> Result<(), String> {
    use std::env;
    use std::fs;

    emit_progress(app, "python", 0, "Preparing Python installation...", false);

    // Get app resource directory
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let python_dir = resource_dir.join("python");
    fs::create_dir_all(&python_dir)
        .map_err(|e| format!("Failed to create python directory: {}", e))?;

    // Download Python embeddable package (portable, no install needed)
    let temp_dir = env::temp_dir();
    let zip_path = temp_dir.join("python-embeddable.zip");

    // Use embeddable package instead of installer
    let embeddable_url = "https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip";

    download_file(embeddable_url, &zip_path, app, "python").await?;

    emit_progress(app, "python", 50, "Extracting Python...", false);

    // Extract ZIP to python directory
    extract_zip(&zip_path, &python_dir)?;

    emit_progress(app, "python", 75, "Configuring Python...", false);

    // Download and install pip in embeddable Python
    let get_pip_url = "https://bootstrap.pypa.io/get-pip.py";
    let get_pip_path = python_dir.join("get-pip.py");

    download_file(get_pip_url, &get_pip_path, app, "python").await?;

    // Run get-pip.py
    let python_exe = python_dir.join("python.exe");
    let output = Command::new(&python_exe)
        .arg(&get_pip_path)
        .output()
        .map_err(|e| format!("Failed to install pip: {}", e))?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Pip installation failed: {}", error));
    }

    // Enable site-packages in python312._pth
    let pth_file = python_dir.join("python312._pth");
    if pth_file.exists() {
        let mut content = fs::read_to_string(&pth_file)
            .map_err(|e| format!("Failed to read _pth file: {}", e))?;

        // Uncomment import site
        content = content.replace("#import site", "import site");
        content.push_str("\nLib\\site-packages\n");

        fs::write(&pth_file, content)
            .map_err(|e| format!("Failed to write _pth file: {}", e))?;
    }

    emit_progress(app, "python", 100, "Python installed successfully", false);

    // Cleanup
    let _ = fs::remove_file(zip_path);
    let _ = fs::remove_file(get_pip_path);

    Ok(())
}

/// Extract ZIP file
#[cfg(target_os = "windows")]
fn extract_zip(zip_path: &PathBuf, dest_dir: &PathBuf) -> Result<(), String> {
    use zip::ZipArchive;
    use std::fs::File;
    use std::io::copy;

    let file = File::open(zip_path)
        .map_err(|e| format!("Failed to open ZIP: {}", e))?;

    let mut archive = ZipArchive::new(file)
        .map_err(|e| format!("Failed to read ZIP: {}", e))?;

    for i in 0..archive.len() {
        let mut file = archive.by_index(i)
            .map_err(|e| format!("Failed to read ZIP entry: {}", e))?;

        let outpath = match file.enclosed_name() {
            Some(path) => dest_dir.join(path),
            None => continue,
        };

        if file.name().ends_with('/') {
            std::fs::create_dir_all(&outpath)
                .map_err(|e| format!("Failed to create directory: {}", e))?;
        } else {
            if let Some(p) = outpath.parent() {
                if !p.exists() {
                    std::fs::create_dir_all(p)
                        .map_err(|e| format!("Failed to create parent directory: {}", e))?;
                }
            }
            let mut outfile = File::create(&outpath)
                .map_err(|e| format!("Failed to create file: {}", e))?;
            copy(&mut file, &mut outfile)
                .map_err(|e| format!("Failed to extract file: {}", e))?;
        }
    }

    Ok(())
}

/// Install Python on macOS in app directory (portable)
#[cfg(target_os = "macos")]
async fn install_python_macos(app: &AppHandle) -> Result<(), String> {
    use std::env;
    use std::fs;

    emit_progress(app, "python", 0, "Downloading Python...", false);

    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let python_dir = resource_dir.join("python");
    fs::create_dir_all(&python_dir)
        .map_err(|e| format!("Failed to create python directory: {}", e))?;

    let temp_dir = env::temp_dir();
    let tar_path = temp_dir.join("python.tar.gz");

    // Download standalone Python build for macOS
    let url = if cfg!(target_arch = "aarch64") {
        "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-aarch64-apple-darwin-install_only_stripped.tar.gz"
    } else {
        "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-x86_64-apple-darwin-install_only_stripped.tar.gz"
    };

    download_file(url, &tar_path, app, "python").await?;

    emit_progress(app, "python", 50, "Extracting Python...", false);

    // Extract tar.gz
    let output = Command::new("tar")
        .args(&["-xzf", tar_path.to_str().unwrap(), "-C", python_dir.to_str().unwrap()])
        .output()
        .map_err(|e| format!("Failed to extract: {}", e))?;

    if !output.status.success() {
        return Err("Failed to extract Python".to_string());
    }

    emit_progress(app, "python", 100, "Python installed successfully", false);

    let _ = fs::remove_file(tar_path);
    Ok(())
}

/// Install Python on Linux in app directory (portable)
#[cfg(target_os = "linux")]
async fn install_python_linux(app: &AppHandle) -> Result<(), String> {
    use std::env;
    use std::fs;

    emit_progress(app, "python", 0, "Downloading Python...", false);

    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let python_dir = resource_dir.join("python");
    fs::create_dir_all(&python_dir)
        .map_err(|e| format!("Failed to create python directory: {}", e))?;

    let temp_dir = env::temp_dir();
    let tar_path = temp_dir.join("python.tar.gz");

    // Download standalone Python build for Linux
    let url = if cfg!(target_arch = "aarch64") {
        "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-aarch64-unknown-linux-gnu-install_only_stripped.tar.gz"
    } else {
        "https://github.com/indygreg/python-build-standalone/releases/download/20241016/cpython-3.12.7+20241016-x86_64-unknown-linux-gnu-install_only_stripped.tar.gz"
    };

    download_file(url, &tar_path, app, "python").await?;

    emit_progress(app, "python", 50, "Extracting Python...", false);

    // Extract tar.gz
    let output = Command::new("tar")
        .args(&["-xzf", tar_path.to_str().unwrap(), "-C", python_dir.to_str().unwrap()])
        .output()
        .map_err(|e| format!("Failed to extract: {}", e))?;

    if !output.status.success() {
        return Err("Failed to extract Python".to_string());
    }

    emit_progress(app, "python", 100, "Python installed successfully", false);

    let _ = fs::remove_file(tar_path);
    Ok(())
}

/// Install Bun in app directory (portable)
async fn install_bun(app: &AppHandle) -> Result<(), String> {
    use std::env;
    use std::fs;

    emit_progress(app, "bun", 0, "Downloading Bun...", false);

    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let bun_dir = resource_dir.join("bun");
    fs::create_dir_all(&bun_dir)
        .map_err(|e| format!("Failed to create bun directory: {}", e))?;

    let temp_dir = env::temp_dir();

    #[cfg(target_os = "windows")]
    {
        let zip_path = temp_dir.join("bun-windows.zip");
        download_file(BUN_WINDOWS_URL, &zip_path, app, "bun").await?;

        emit_progress(app, "bun", 50, "Extracting Bun...", false);

        // Extract to temporary directory first
        let temp_extract_dir = temp_dir.join("bun_extract");
        fs::create_dir_all(&temp_extract_dir)
            .map_err(|e| format!("Failed to create temp extract directory: {}", e))?;

        extract_zip(&zip_path, &temp_extract_dir)?;

        // Find bun.exe in the extracted folder and move to bun/ directory
        // Bun Windows zip contains: bun-windows-x64/bun.exe
        let extracted_entries = fs::read_dir(&temp_extract_dir)
            .map_err(|e| format!("Failed to read extracted directory: {}", e))?;

        let mut bun_found = false;
        for entry in extracted_entries {
            if let Ok(entry) = entry {
                let path = entry.path();
                if path.is_dir() {
                    let bun_exe_path = path.join("bun.exe");
                    if bun_exe_path.exists() {
                        let target_bun = bun_dir.join("bun.exe");
                        fs::copy(&bun_exe_path, &target_bun)
                            .map_err(|e| format!("Failed to copy bun.exe: {}", e))?;
                        bun_found = true;
                        break;
                    }
                }
            }
        }

        if !bun_found {
            return Err("bun.exe not found in extracted archive".to_string());
        }

        emit_progress(app, "bun", 100, "Bun installed successfully", false);

        // Cleanup
        let _ = fs::remove_file(zip_path);
        let _ = fs::remove_dir_all(temp_extract_dir);
    }

    #[cfg(not(target_os = "windows"))]
    {
        let zip_path = temp_dir.join("bun.zip");

        let url = if cfg!(target_os = "macos") {
            if cfg!(target_arch = "aarch64") {
                "https://github.com/oven-sh/bun/releases/latest/download/bun-darwin-aarch64.zip"
            } else {
                "https://github.com/oven-sh/bun/releases/latest/download/bun-darwin-x64.zip"
            }
        } else {
            if cfg!(target_arch = "aarch64") {
                "https://github.com/oven-sh/bun/releases/latest/download/bun-linux-aarch64.zip"
            } else {
                "https://github.com/oven-sh/bun/releases/latest/download/bun-linux-x64.zip"
            }
        };

        download_file(url, &zip_path, app, "bun").await?;

        emit_progress(app, "bun", 50, "Extracting Bun...", false);

        // Extract to temporary directory first
        let temp_extract_dir = temp_dir.join("bun_extract");
        fs::create_dir_all(&temp_extract_dir)
            .map_err(|e| format!("Failed to create temp extract directory: {}", e))?;

        // Extract using unzip command
        let output = Command::new("unzip")
            .args(&["-q", zip_path.to_str().unwrap(), "-d", temp_extract_dir.to_str().unwrap()])
            .output()
            .map_err(|e| format!("Failed to extract Bun: {}", e))?;

        if !output.status.success() {
            return Err("Failed to extract Bun".to_string());
        }

        // Create bin directory in final location
        let bin_dir = bun_dir.join("bin");
        fs::create_dir_all(&bin_dir)
            .map_err(|e| format!("Failed to create bin directory: {}", e))?;

        // Find the bun executable in extracted folder and move it
        // Bun zips contain a folder like "bun-darwin-aarch64/bun" or "bun-linux-x64/bun"
        let extracted_entries = fs::read_dir(&temp_extract_dir)
            .map_err(|e| format!("Failed to read extracted directory: {}", e))?;

        let mut bun_found = false;
        for entry in extracted_entries {
            if let Ok(entry) = entry {
                let path = entry.path();
                if path.is_dir() {
                    let bun_exe_path = path.join("bun");
                    if bun_exe_path.exists() {
                        let target_bun = bin_dir.join("bun");
                        fs::copy(&bun_exe_path, &target_bun)
                            .map_err(|e| format!("Failed to copy bun executable: {}", e))?;

                        // Make executable
                        Command::new("chmod")
                            .args(&["+x", target_bun.to_str().unwrap()])
                            .output()
                            .ok();

                        bun_found = true;
                        break;
                    }
                }
            }
        }

        if !bun_found {
            return Err("Bun executable not found in extracted archive".to_string());
        }

        emit_progress(app, "bun", 100, "Bun installed successfully", false);

        // Cleanup
        let _ = fs::remove_file(zip_path);
        let _ = fs::remove_dir_all(temp_extract_dir);
    }

    Ok(())
}

/// Install Python packages from requirements.txt using app's Python
async fn install_python_packages(app: &AppHandle) -> Result<(), String> {
    emit_progress(app, "packages", 0, "Installing Python packages...", false);

    // Get Python executable from app directory
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let python_exe = if cfg!(target_os = "windows") {
        resource_dir.join("python").join("python.exe")
    } else {
        resource_dir.join("python").join("bin").join("python3")
    };

    if !python_exe.exists() {
        return Err("Python not found in app directory".to_string());
    }

    // Get requirements.txt path
    let requirements_path = resource_dir.join("requirements.txt");

    if !requirements_path.exists() {
        return Err("requirements.txt not found".to_string());
    }

    emit_progress(app, "packages", 25, "Running pip install...", false);

    let output = Command::new(&python_exe)
        .args(&[
            "-m",
            "pip",
            "install",
            "-r",
            requirements_path.to_str().unwrap(),
        ])
        .output()
        .map_err(|e| format!("Failed to run pip: {}", e))?;

    if !output.status.success() {
        let error = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Package installation failed: {}", error));
    }

    emit_progress(app, "packages", 100, "All packages installed successfully", false);

    Ok(())
}

/// Run complete setup process
#[tauri::command]
pub async fn run_setup(app: AppHandle) -> Result<(), String> {
    let status = check_setup_status(app.clone())?;

    // Install Python if needed
    if !status.python_installed {
        #[cfg(target_os = "windows")]
        install_python_windows(&app).await?;

        #[cfg(target_os = "macos")]
        install_python_macos(&app).await?;

        #[cfg(target_os = "linux")]
        install_python_linux(&app).await?;

        // Verify installation
        let (installed, version) = check_python_installed_in_app(&app);
        if !installed {
            return Err("Python installation verification failed".to_string());
        }
        emit_progress(&app, "python", 100, &format!("Python {} verified", version.unwrap_or_default()), false);
    }

    // Install Bun if needed
    if !status.bun_installed {
        install_bun(&app).await?;

        // Verify installation
        let (installed, version) = check_bun_installed_in_app(&app);
        if !installed {
            return Err("Bun installation verification failed".to_string());
        }
        emit_progress(&app, "bun", 100, &format!("Bun {} verified", version.unwrap_or_default()), false);
    }

    // Install Python packages
    install_python_packages(&app).await?;

    // Final verification
    emit_progress(&app, "complete", 100, "Setup complete!", false);

    Ok(())
}
