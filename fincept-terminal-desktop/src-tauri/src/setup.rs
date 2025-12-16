// setup.rs - First-time setup for Python and Bun installation
use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::{Command, Stdio};
use tauri::{AppHandle, Emitter, Manager};

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// Windows creation flags to hide console window
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;


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

// Bun download URLs - Using v1.1.0 with CPU-optimized builds
const BUN_VERSION: &str = "1.1.0";

// Optimized builds (require AVX2 - modern CPUs from ~2013+)
const BUN_WINDOWS_X64_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-windows-x64.zip";
const BUN_MACOS_X64_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-darwin-x64.zip";
const BUN_LINUX_X64_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-linux-x64.zip";

// Baseline builds (work on all CPUs, slightly slower)
const BUN_WINDOWS_X64_BASELINE_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-windows-x64-baseline.zip";
const BUN_MACOS_X64_BASELINE_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-darwin-x64-baseline.zip";
const BUN_LINUX_X64_BASELINE_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-linux-x64-baseline.zip";

// ARM builds (always optimized for their architecture)
const BUN_MACOS_ARM_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-darwin-aarch64.zip";
const BUN_LINUX_ARM_URL: &str = "https://github.com/oven-sh/bun/releases/download/bun-v1.1.0/bun-linux-aarch64.zip";

/// Detect if CPU supports AVX2 (required for optimized Bun builds)
#[cfg(target_os = "windows")]
fn cpu_supports_avx2() -> bool {
    // Use CPUID to check for AVX2 support on Windows
    // Run 'wmic cpu get caption' to check CPU model as fallback
    let mut cmd = Command::new("wmic");
    cmd.args(&["cpu", "get", "caption"]);
    cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window
    match cmd.output()
    {
        Ok(output) if output.status.success() => {
            let cpu_info = String::from_utf8_lossy(&output.stdout);
            eprintln!("[SETUP] CPU Info: {}", cpu_info);

            // Check for modern CPU families that support AVX2
            // Intel: Haswell (2013+), AMD: Excavator (2015+)
            let has_modern_cpu = cpu_info.contains("i3-4") || cpu_info.contains("i5-4") ||
                                 cpu_info.contains("i7-4") || cpu_info.contains("i9-") ||
                                 cpu_info.contains("i3-5") || cpu_info.contains("i5-5") ||
                                 cpu_info.contains("i7-5") || cpu_info.contains("i3-6") ||
                                 cpu_info.contains("i5-6") || cpu_info.contains("i7-6") ||
                                 cpu_info.contains("i3-7") || cpu_info.contains("i5-7") ||
                                 cpu_info.contains("i7-7") || cpu_info.contains("i3-8") ||
                                 cpu_info.contains("i5-8") || cpu_info.contains("i7-8") ||
                                 cpu_info.contains("i3-9") || cpu_info.contains("i5-9") ||
                                 cpu_info.contains("i7-9") || cpu_info.contains("i3-10") ||
                                 cpu_info.contains("i5-10") || cpu_info.contains("i7-10") ||
                                 cpu_info.contains("i3-11") || cpu_info.contains("i5-11") ||
                                 cpu_info.contains("i7-11") || cpu_info.contains("i3-12") ||
                                 cpu_info.contains("i5-12") || cpu_info.contains("i7-12") ||
                                 cpu_info.contains("i3-13") || cpu_info.contains("i5-13") ||
                                 cpu_info.contains("i7-13") || cpu_info.contains("i3-14") ||
                                 cpu_info.contains("i5-14") || cpu_info.contains("i7-14") ||
                                 cpu_info.contains("Ryzen");

            if has_modern_cpu {
                eprintln!("[SETUP] Modern CPU detected - using optimized Bun build");
                return true;
            }

            eprintln!("[SETUP] Older CPU detected - using baseline Bun build for compatibility");
            false
        }
        _ => {
            eprintln!("[SETUP] Could not detect CPU, using baseline build for safety");
            false
        }
    }
}

#[cfg(target_os = "macos")]
fn cpu_supports_avx2() -> bool {
    // macOS: Check sysctl for CPU features
    match Command::new("sysctl")
        .arg("machdep.cpu.features")
        .output()
    {
        Ok(output) if output.status.success() => {
            let features = String::from_utf8_lossy(&output.stdout);
            let has_avx2 = features.contains("AVX2");
            eprintln!("[SETUP] macOS CPU AVX2 support: {}", has_avx2);
            has_avx2
        }
        _ => {
            eprintln!("[SETUP] Could not detect CPU features, using baseline build");
            false
        }
    }
}

#[cfg(target_os = "linux")]
fn cpu_supports_avx2() -> bool {
    // Linux: Check /proc/cpuinfo for flags
    use std::fs;

    match fs::read_to_string("/proc/cpuinfo") {
        Ok(cpuinfo) => {
            let has_avx2 = cpuinfo.contains("avx2");
            eprintln!("[SETUP] Linux CPU AVX2 support: {}", has_avx2);
            has_avx2
        }
        _ => {
            eprintln!("[SETUP] Could not read /proc/cpuinfo, using baseline build");
            false
        }
    }
}

/// Check if Python is installed (ONLY check app directory, not system)
pub fn check_python_installed_in_app(app: &AppHandle) -> (bool, Option<String>) {
    // Check in app's resource directory (where Python should be installed)
    if let Ok(resource_dir) = app.path().resource_dir() {
        let python_exe = if cfg!(target_os = "windows") {
            resource_dir.join("python").join("python.exe")
        } else {
            resource_dir.join("python").join("bin").join("python3")
        };

        eprintln!("[SETUP] Checking Python at: {}", python_exe.display());

        if python_exe.exists() {
            let mut cmd = Command::new(&python_exe);
            cmd.arg("--version");
            #[cfg(target_os = "windows")]
            cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window
            match cmd.output()
            {
                Ok(output) if output.status.success() => {
                    let version = String::from_utf8_lossy(&output.stdout)
                        .trim()
                        .to_string();
                    eprintln!("[SETUP] Found app Python: {}", version);
                    return (true, Some(version));
                }
                _ => {
                    eprintln!("[SETUP] Python exe exists but failed to run");
                }
            }
        } else {
            eprintln!("[SETUP] Python exe not found at expected location");
        }
    }

    // Do NOT fallback to system Python - we need app's own Python
    eprintln!("[SETUP] App Python not installed");
    (false, None)
}

// System Python/Bun check functions removed - we only check app's own installations now
// This ensures the app always uses its bundled runtimes, not system ones

/// Check overall setup status
#[tauri::command]
pub fn check_setup_status(app: AppHandle) -> Result<SetupStatus, String> {
    // Debug: Print resource_dir path
    if let Ok(resource_dir) = app.path().resource_dir() {
        eprintln!("[SETUP] Resource dir: {}", resource_dir.display());
        eprintln!("[SETUP] Python check path: {}", resource_dir.join("python").display());
        eprintln!("[SETUP] Bun check path: {}", resource_dir.join("bun").display());
    } else {
        eprintln!("[SETUP] Failed to get resource_dir!");
    }

    let (python_installed, python_version) = check_python_installed_in_app(&app);
    let (bun_installed, bun_version) = check_bun_installed_in_app(&app);

    eprintln!("[SETUP] Python installed: {}, Bun installed: {}", python_installed, bun_installed);
    eprintln!("[SETUP] Needs setup: {}", !python_installed || !bun_installed);

    // Both Python and Bun needed (Bun is required for MCP servers)
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
    if let Ok(resource_dir) = app.path().resource_dir() {
        let bun_exe = if cfg!(target_os = "windows") {
            resource_dir.join("bun").join("bun.exe")
        } else {
            resource_dir.join("bun").join("bun")
        };

        eprintln!("[SETUP] Checking Bun at: {}", bun_exe.display());

        if bun_exe.exists() {
            if let Ok(metadata) = std::fs::metadata(&bun_exe) {
                eprintln!("[SETUP] Bun file size: {} bytes", metadata.len());

                if metadata.len() == 0 {
                    eprintln!("[SETUP] Bun exe is empty!");
                    return (false, None);
                }

                // On Windows, use PowerShell to execute (avoids bash path issues)
                #[cfg(target_os = "windows")]
                {
                    let mut cmd = Command::new("powershell");
                    cmd.args(&["-Command", &format!("& '{}' --version", bun_exe.display())]);
                    cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window
                    match cmd.output()
                    {
                        Ok(output) if output.status.success() => {
                            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
                            eprintln!("[SETUP] Found Bun v{}", version);
                            return (true, Some(version));
                        }
                        Ok(output) => {
                            eprintln!("[SETUP] Bun failed to run: {:?}", output.status.code());
                            eprintln!("[SETUP] Stderr: {}", String::from_utf8_lossy(&output.stderr));
                        }
                        Err(e) => {
                            eprintln!("[SETUP] Failed to execute Bun: {}", e);
                        }
                    }
                }

                // On Unix, execute directly
                #[cfg(not(target_os = "windows"))]
                {
                    match Command::new(&bun_exe).arg("--version").output() {
                        Ok(output) if output.status.success() => {
                            let version = String::from_utf8_lossy(&output.stdout).trim().to_string();
                            eprintln!("[SETUP] Found Bun v{}", version);
                            return (true, Some(version));
                        }
                        Ok(output) => {
                            eprintln!("[SETUP] Bun failed to run: {:?}", output.status.code());
                        }
                        Err(e) => {
                            eprintln!("[SETUP] Failed to execute Bun: {}", e);
                        }
                    }
                }
            }
        }
    }

    eprintln!("[SETUP] Bun not installed");
    (false, None)
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

/// Check if Microsoft Visual C++ Redistributable is installed (Windows only)
#[cfg(target_os = "windows")]
fn check_vcredist_installed() -> bool {
    // Check for VC++ Redistributable by looking for vcruntime140.dll in System32
    let system32 = std::env::var("SystemRoot")
        .map(|root| PathBuf::from(root).join("System32"))
        .unwrap_or_else(|_| PathBuf::from(r"C:\Windows\System32"));

    let vcruntime_dll = system32.join("vcruntime140.dll");
    let msvcp_dll = system32.join("msvcp140.dll");

    vcruntime_dll.exists() && msvcp_dll.exists()
}

/// Install Python on Windows in app directory (portable, no admin)
#[cfg(target_os = "windows")]
async fn install_python_windows(app: &AppHandle) -> Result<(), String> {
    use std::env;
    use std::fs;

    emit_progress(app, "python", 0, "Checking system requirements...", false);

    // Check for VC++ Redistributable
    if !check_vcredist_installed() {
        emit_progress(
            app,
            "python",
            0,
            "Warning: Microsoft Visual C++ Redistributable may not be installed",
            false
        );

        // Return a warning but allow setup to continue
        // The detailed error will be shown when Python fails to run
        return Err(format!(
            "Microsoft Visual C++ Redistributable 2015-2022 is required but may not be installed.\n\n\
            Python 3.12 embedded requires this runtime to function.\n\n\
            Please install it first from:\n\
            https://aka.ms/vs/17/release/vc_redist.{}.exe\n\n\
            After installing VC++ Redistributable, please restart the application and try again.",
            if cfg!(target_arch = "x86_64") { "x64" } else { "x86" }
        ));
    }

    emit_progress(app, "python", 5, "Preparing Python installation...", false);

    // Install Python in the application's resource directory
    // This keeps everything together in the installation folder
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

    // Detect system architecture and select appropriate Python build
    let embeddable_url = if cfg!(target_arch = "x86_64") {
        // 64-bit Windows
        "https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-amd64.zip"
    } else if cfg!(target_arch = "x86") {
        // 32-bit Windows
        "https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-win32.zip"
    } else if cfg!(target_arch = "aarch64") {
        // ARM64 Windows
        "https://www.python.org/ftp/python/3.12.7/python-3.12.7-embed-arm64.zip"
    } else {
        return Err(format!(
            "Unsupported Windows architecture: {}. Please install Python 3.12 manually from python.org",
            std::env::consts::ARCH
        ));
    };

    emit_progress(app, "python", 10, &format!("Downloading Python for {} architecture...", std::env::consts::ARCH), false);

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

    // Test if Python executable can run (checks for VC++ Runtime and other dependencies)
    emit_progress(app, "python", 70, "Testing Python installation...", false);
    let mut test_cmd = Command::new(&python_exe);
    test_cmd.arg("--version");
    test_cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window
    let test_output = test_cmd.output();

    match test_output {
        Err(e) => {
            return Err(format!(
                "Python installation failed. The Python interpreter cannot run on this system.\n\n\
                Common causes:\n\
                1. Missing Microsoft Visual C++ Redistributable 2015-2022\n\
                2. System incompatibility\n\n\
                Error: {}\n\n\
                Please install:\n\
                - Microsoft Visual C++ Redistributable from: https://aka.ms/vs/17/release/vc_redist.x64.exe\n\
                - Or install Python 3.12 manually from: https://www.python.org/downloads/",
                e
            ));
        }
        Ok(output) if !output.status.success() => {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(format!(
                "Python installation test failed.\n\n\
                Error: {}\n\n\
                Please ensure Microsoft Visual C++ Redistributable 2015-2022 is installed.\n\
                Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe",
                stderr
            ));
        }
        Ok(_) => {
            // Python exe works, continue with pip installation
        }
    }

    emit_progress(app, "python", 75, "Installing pip package manager...", false);
    let mut pip_cmd = Command::new(&python_exe);
    pip_cmd.arg(&get_pip_path);
    pip_cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window
    let output = pip_cmd.output()
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
    // These builds from indygreg include pip, setuptools, and wheel
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

    emit_progress(app, "python", 75, "Verifying Python installation...", false);

    // Verify Python and pip are working
    let python_exe = python_dir.join("bin").join("python3");
    let test_output = Command::new(&python_exe)
        .arg("--version")
        .output();

    match test_output {
        Err(e) => {
            return Err(format!("Python installation test failed: {}", e));
        }
        Ok(output) if !output.status.success() => {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(format!("Python installation test failed: {}", stderr));
        }
        Ok(_) => {
            // Python works, continue
        }
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
    // These builds from indygreg include pip, setuptools, and wheel
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

    emit_progress(app, "python", 75, "Verifying Python installation...", false);

    // Verify Python and pip are working
    let python_exe = python_dir.join("bin").join("python3");
    let test_output = Command::new(&python_exe)
        .arg("--version")
        .output();

    match test_output {
        Err(e) => {
            return Err(format!("Python installation test failed: {}", e));
        }
        Ok(output) if !output.status.success() => {
            let stderr = String::from_utf8_lossy(&output.stderr);
            return Err(format!("Python installation test failed: {}", stderr));
        }
        Ok(_) => {
            // Python works, continue
        }
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

        // Detect CPU and choose appropriate build
        let url = if cpu_supports_avx2() {
            emit_progress(app, "bun", 5, "Downloading optimized Bun for modern CPU...", false);
            BUN_WINDOWS_X64_URL
        } else {
            emit_progress(app, "bun", 5, "Downloading baseline Bun for maximum compatibility...", false);
            BUN_WINDOWS_X64_BASELINE_URL
        };

        download_file(url, &zip_path, app, "bun").await?;

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
            // List what we actually found to help debug
            let mut found_files = Vec::new();
            if let Ok(entries) = fs::read_dir(&temp_extract_dir) {
                for entry in entries.flatten() {
                    found_files.push(entry.path().display().to_string());
                }
            }
            return Err(format!(
                "bun.exe not found in extracted archive.\n\n\
                Expected structure: bun-windows-x64/bun.exe\n\
                Found in archive:\n{}\n\n\
                This may indicate a corrupted download. Please retry the setup.",
                found_files.join("\n")
            ));
        }

        // Verify the file was copied and has content
        let target_bun = bun_dir.join("bun.exe");
        if !target_bun.exists() {
            return Err("Failed to copy bun.exe to target directory".to_string());
        }

        if let Ok(metadata) = fs::metadata(&target_bun) {
            if metadata.len() == 0 {
                return Err("Copied bun.exe is empty (0 bytes). Download may be corrupted.".to_string());
            }
            eprintln!("[SETUP] Bun.exe copied successfully, size: {} bytes", metadata.len());
        }

        emit_progress(app, "bun", 100, "Bun installed successfully", false);

        // Cleanup
        let _ = fs::remove_file(zip_path);
        let _ = fs::remove_dir_all(temp_extract_dir);
    }

    #[cfg(not(target_os = "windows"))]
    {
        let zip_path = temp_dir.join("bun.zip");

        // Choose URL based on OS, architecture, and CPU features
        let url = if cfg!(target_os = "macos") {
            if cfg!(target_arch = "aarch64") {
                // ARM Macs always use optimized build
                emit_progress(app, "bun", 5, "Downloading Bun for Apple Silicon...", false);
                BUN_MACOS_ARM_URL
            } else {
                // Intel Macs - check for AVX2
                if cpu_supports_avx2() {
                    emit_progress(app, "bun", 5, "Downloading optimized Bun for modern CPU...", false);
                    BUN_MACOS_X64_URL
                } else {
                    emit_progress(app, "bun", 5, "Downloading baseline Bun for compatibility...", false);
                    BUN_MACOS_X64_BASELINE_URL
                }
            }
        } else {
            // Linux
            if cfg!(target_arch = "aarch64") {
                // ARM Linux always use optimized build
                emit_progress(app, "bun", 5, "Downloading Bun for ARM64...", false);
                BUN_LINUX_ARM_URL
            } else {
                // x64 Linux - check for AVX2
                if cpu_supports_avx2() {
                    emit_progress(app, "bun", 5, "Downloading optimized Bun for modern CPU...", false);
                    BUN_LINUX_X64_URL
                } else {
                    emit_progress(app, "bun", 5, "Downloading baseline Bun for compatibility...", false);
                    BUN_LINUX_X64_BASELINE_URL
                }
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

        // Find the bun executable in extracted folder and move it directly to bun_dir
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
                        let target_bun = bun_dir.join("bun");
                        fs::copy(&bun_exe_path, &target_bun)
                            .map_err(|e| format!("Failed to copy bun executable: {}", e))?;

                        // Make executable
                        Command::new("chmod")
                            .args(&["+x", target_bun.to_str().unwrap()])
                            .output()
                            .ok();

                        eprintln!("[SETUP] Bun copied to: {}", target_bun.display());
                        bun_found = true;
                        break;
                    }
                }
            }
        }

        if !bun_found {
            let mut found_files = Vec::new();
            if let Ok(entries) = fs::read_dir(&temp_extract_dir) {
                for entry in entries.flatten() {
                    found_files.push(entry.path().display().to_string());
                }
            }
            return Err(format!(
                "Bun executable not found in extracted archive.\n\
                Found:\n{}",
                found_files.join("\n")
            ));
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
    use std::io::{BufRead, BufReader};
    use std::process::Stdio;

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

    // Step 1: Upgrade pip, setuptools, and wheel first
    // This is critical for embedded Python to handle modern packages
    emit_progress(app, "packages", 5, "Upgrading pip...", false);

    let mut upgrade_cmd = Command::new(&python_exe);
    upgrade_cmd.args(&[
        "-m",
        "pip",
        "install",
        "--upgrade",
        "pip",
        "setuptools",
        "wheel",
    ]);

    #[cfg(target_os = "windows")]
    upgrade_cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window

    let upgrade_output = upgrade_cmd.output()
        .map_err(|e| format!("Failed to upgrade pip/setuptools: {}", e))?;

    if !upgrade_output.status.success() {
        let error = String::from_utf8_lossy(&upgrade_output.stderr);
        eprintln!("[SETUP] Warning: Failed to upgrade pip/setuptools: {}", error);
        // Continue anyway - might still work
    } else {
        emit_progress(app, "packages", 15, "✓ pip, setuptools, and wheel upgraded", false);
    }

    // Step 2: Get requirements.txt path using resolve_resource (works in dev and prod)
    let requirements_path = app
        .path()
        .resolve("resources/requirements.txt", tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to resolve requirements.txt path: {}", e))?;

    eprintln!("[SETUP] Looking for requirements.txt at: {}", requirements_path.display());

    if !requirements_path.exists() {
        return Err(format!("requirements.txt not found at: {}", requirements_path.display()));
    }

    // Step 2.5: Count total packages for progress calculation
    let total_packages = count_packages(&requirements_path)?;
    eprintln!("[SETUP] Total packages to install: {}", total_packages);

    // Step 3: Install packages from requirements.txt with real-time progress
    emit_progress(app, "packages", 20, "Reading requirements.txt...", false);

    let mut child_cmd = Command::new(&python_exe);
    child_cmd.args(&[
        "-m",
        "pip",
        "install",
        "--no-cache-dir",  // Avoid cache issues
        "-r",
        requirements_path.to_str().unwrap(),
    ]);
    child_cmd.stdout(Stdio::piped());
    child_cmd.stderr(Stdio::piped());

    #[cfg(target_os = "windows")]
    child_cmd.creation_flags(CREATE_NO_WINDOW); // Hide console window

    let mut child = child_cmd.spawn()
        .map_err(|e| format!("Failed to run pip: {}", e))?;

    // Capture stdout and stderr for progress tracking
    let stdout = child.stdout.take().ok_or("Failed to capture stdout")?;
    let stderr = child.stderr.take().ok_or("Failed to capture stderr")?;

    let app_clone = app.clone();
    let total_packages_clone = total_packages;

    // Spawn thread to read stdout in real-time
    std::thread::spawn(move || {
        let reader = BufReader::new(stdout);
        let mut installed_count = 0;

        for line in reader.lines() {
            if let Ok(line) = line {
                eprintln!("[PIP] {}", line);

                // Parse pip output for package installation
                if line.contains("Collecting") {
                    let package_name = extract_package_name(&line, "Collecting");
                    let progress = 20 + ((installed_count as f32 / total_packages_clone as f32) * 75.0) as u8;
                    emit_progress(&app_clone, "packages", progress, &format!("📦 Collecting {}...", package_name), false);
                } else if line.contains("Downloading") {
                    let package_name = extract_package_name(&line, "Downloading");
                    let progress = 20 + ((installed_count as f32 / total_packages_clone as f32) * 75.0) as u8;
                    emit_progress(&app_clone, "packages", progress, &format!("⬇️  Downloading {}...", package_name), false);
                } else if line.contains("Installing collected packages:") {
                    emit_progress(&app_clone, "packages", 85, "Installing collected packages...", false);
                } else if line.contains("Successfully installed") {
                    installed_count = total_packages_clone; // All done
                    let progress = 95;
                    emit_progress(&app_clone, "packages", progress, "✓ All packages installed successfully", false);
                }
            }
        }
    });

    // Read stderr for errors
    let stderr_reader = BufReader::new(stderr);
    let mut error_messages = Vec::new();

    for line in stderr_reader.lines() {
        if let Ok(line) = line {
            eprintln!("[PIP STDERR] {}", line);
            if line.contains("ERROR") || line.contains("error") {
                error_messages.push(line);
            }
        }
    }

    // Wait for process to complete
    let status = child.wait().map_err(|e| format!("Failed to wait for pip: {}", e))?;

    if !status.success() {
        let error = if !error_messages.is_empty() {
            error_messages.join("\n")
        } else {
            "Package installation failed (unknown error)".to_string()
        };
        return Err(format!("Package installation failed: {}", error));
    }

    emit_progress(app, "packages", 100, "✓ All packages installed successfully", false);

    Ok(())
}

/// Count number of packages in requirements.txt
fn count_packages(requirements_path: &std::path::PathBuf) -> Result<usize, String> {
    use std::fs::File;
    use std::io::{BufRead, BufReader};

    let file = File::open(requirements_path)
        .map_err(|e| format!("Failed to open requirements.txt: {}", e))?;

    let reader = BufReader::new(file);
    let mut count = 0;

    for line in reader.lines() {
        if let Ok(line) = line {
            let trimmed = line.trim();
            // Count non-empty, non-comment lines
            if !trimmed.is_empty() && !trimmed.starts_with('#') {
                count += 1;
            }
        }
    }

    Ok(count)
}

/// Extract package name from pip output line
fn extract_package_name(line: &str, prefix: &str) -> String {
    if let Some(pos) = line.find(prefix) {
        let after_prefix = &line[pos + prefix.len()..];
        let words: Vec<&str> = after_prefix.trim().split_whitespace().collect();
        if !words.is_empty() {
            // Remove version specifiers like (==1.2.3)
            return words[0].split('=').next().unwrap_or(words[0]).to_string();
        }
    }
    "package".to_string()
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
    } else {
        // Python already installed, skip
        emit_progress(&app, "python", 100, &format!("✓ Python {} already installed", status.python_version.unwrap_or_default()), false);
    }

    // Install Bun if needed (required for MCP servers)
    if !status.bun_installed {
        install_bun(&app).await?;

        // Wait for filesystem sync on Windows
        #[cfg(target_os = "windows")]
        tokio::time::sleep(tokio::time::Duration::from_millis(2000)).await;

        // Verify installation
        let (installed, version) = check_bun_installed_in_app(&app);
        if !installed {
            return Err(format!(
                "Bun v{} installation verification failed.\n\n\
                The file was downloaded but cannot execute.\n\
                This is needed for MCP server functionality.\n\n\
                Please try:\n\
                1. Retry the setup\n\
                2. Check antivirus settings\n\
                3. Run as administrator",
                BUN_VERSION
            ));
        }
        emit_progress(&app, "bun", 100, &format!("✓ Bun v{} installed", version.unwrap_or_default()), false);
    } else {
        // Bun already installed, skip
        emit_progress(&app, "bun", 100, &format!("✓ Bun {} already installed", status.bun_version.unwrap_or_default()), false);
    }

    // Install Python packages
    install_python_packages(&app).await?;

    // Final verification
    emit_progress(&app, "complete", 100, "Setup complete!", false);

    Ok(())
}
