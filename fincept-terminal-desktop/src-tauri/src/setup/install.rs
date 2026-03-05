// setup/install.rs - Python, Bun, UV, venv installation

use std::path::PathBuf;
use std::process::Command;
use tauri::{AppHandle, Manager};
use super::{emit_progress, PYTHON_VERSION, BUN_VERSION};

#[cfg(target_os = "windows")]
use super::CommandExt;
#[cfg(target_os = "windows")]
use super::CREATE_NO_WINDOW;

/// Install Python
pub(super) async fn install_python(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
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

        let tar_path_str = tar_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", tar_path))?;
        let python_dir_str = python_dir.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", python_dir))?;

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", tar_path_str, &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        eprintln!("[SETUP] Download complete, file size: {} bytes",
            std::fs::metadata(&tar_path).map(|m| m.len()).unwrap_or(0));

        emit_progress(app, "python", 60, "Extracting Python...", false);

        let mut cmd = Command::new("tar");
        cmd.args(&["-xzf", tar_path_str, "-C", python_dir_str, "--strip-components=1"]);
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

        let tar_path_str = tar_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", tar_path))?;
        let python_dir_str = python_dir.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", python_dir))?;

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", tar_path_str, &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        eprintln!("[SETUP] Download complete, file size: {} bytes",
            std::fs::metadata(&tar_path).map(|m| m.len()).unwrap_or(0));

        emit_progress(app, "python", 50, "Decompressing archive...", false);

        // Use gunzip separately for better BSD tar compatibility
        let mut gunzip_cmd = Command::new("gunzip");
        gunzip_cmd.arg(tar_path_str);
        let output = gunzip_cmd.output().map_err(|e| format!("Gunzip failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Gunzip failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "python", 60, "Extracting Python...", false);

        // Extract the uncompressed tar file
        let tar_uncompressed = python_dir.join("python.tar");
        let tar_uncompressed_str = tar_uncompressed.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", tar_uncompressed))?;
        let mut cmd = Command::new("tar");
        cmd.args(&["-xf", tar_uncompressed_str, "-C", python_dir_str, "--strip-components=1"]);
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
        let get_pip_path_str = get_pip_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", get_pip_path))?;
        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", get_pip_path_str, get_pip_url]);
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
    let get_pip_str = get_pip_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", get_pip_path))?;
    cmd.arg(get_pip_str);
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
pub(super) async fn install_bun(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
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

        let zip_path_str = zip_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", zip_path))?;
        let bun_dir_str = bun_dir.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", bun_dir))?;

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", zip_path_str, &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "bun", 70, "Extracting Bun...", false);

        let mut cmd = Command::new("unzip");
        cmd.args(&["-o", zip_path_str, "-d", bun_dir_str]);
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

        let zip_path_str = zip_path.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", zip_path))?;
        let bun_dir_str = bun_dir.to_str().ok_or_else(|| format!("Invalid path encoding: {:?}", bun_dir))?;

        let mut cmd = Command::new("curl");
        cmd.args(&["-L", "-o", zip_path_str, &download_url]);
        let output = cmd.output().map_err(|e| format!("Download failed: {}", e))?;
        if !output.status.success() {
            return Err(format!("Download failed: {}", String::from_utf8_lossy(&output.stderr)));
        }

        emit_progress(app, "bun", 70, "Extracting Bun...", false);

        let mut cmd = Command::new("unzip");
        cmd.args(&["-o", zip_path_str, "-d", bun_dir_str]);
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
pub(super) async fn install_uv(app: &AppHandle, install_dir: &PathBuf) -> Result<(), String> {
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
pub(super) async fn create_venv(app: &AppHandle, install_dir: &PathBuf, venv_name: &str) -> Result<(), String> {
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
pub(super) async fn install_packages_in_venv(
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

    // Set environment variables to prevent C extension build failures
    // on embedded Python which lacks development headers (sqlite3.h, etc.)
    cmd.env("PEEWEE_NO_SQLITE_EXTENSIONS", "1");
    cmd.env("PEEWEE_NO_C_EXTENSION", "1");

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
