// Utility module for Python execution path resolution
use std::path::PathBuf;
use std::process::Command;
use tauri::Manager;

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// Windows creation flags to hide console window
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

/// Get the Python executable path at runtime
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    use std::process::Command;

    // Strategy for all platforms:
    // 1. Try bundled Python first (ensures consistency)
    // 2. Fall back to system Python (for compatibility)
    // This ensures users don't need to install Python separately

    #[cfg(target_os = "windows")]
    {
        // Try bundled Python first on Windows
        let resource_dir = app
            .path()
            .resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;

        let bundled_python = resource_dir
            .join("python-windows")
            .join("python.exe");

        if bundled_python.exists() {
            return Ok(bundled_python);
        }

        // Fallback to system Python
        if let Ok(output) = Command::new("python").arg("--version").output() {
            if output.status.success() {
                if let Ok(python_path) = which::which("python") {
                    return Ok(python_path);
                }
            }
        }

        return Err("Python not found. Please install Python 3.x from python.org".to_string());
    }

    #[cfg(target_os = "macos")]
    {
        // Try bundled Python first on macOS
        let resource_dir = app
            .path()
            .resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;

        let bundled_python = resource_dir
            .join("python-macos")
            .join("bin")
            .join("python3");

        if bundled_python.exists() {
            return Ok(bundled_python);
        }

        // Fallback to system python3
        if let Ok(output) = Command::new("python3").arg("--version").output() {
            if output.status.success() {
                if let Ok(python_path) = which::which("python3") {
                    return Ok(python_path);
                }
            }
        }

        // Try python as last resort
        if let Ok(output) = Command::new("python").arg("--version").output() {
            if output.status.success() {
                if let Ok(python_path) = which::which("python") {
                    return Ok(python_path);
                }
            }
        }

        return Err("Python not found. Please install Python 3.x:\n  brew install python3".to_string());
    }

    #[cfg(target_os = "linux")]
    {
        // Try bundled Python first on Linux
        let resource_dir = app
            .path()
            .resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;

        let bundled_python = resource_dir
            .join("python-linux")
            .join("bin")
            .join("python3");

        if bundled_python.exists() {
            return Ok(bundled_python);
        }

        // Fallback to system python3
        if let Ok(output) = Command::new("python3").arg("--version").output() {
            if output.status.success() {
                if let Ok(python_path) = which::which("python3") {
                    return Ok(python_path);
                }
            }
        }

        // Try python as last resort
        if let Ok(output) = Command::new("python").arg("--version").output() {
            if output.status.success() {
                if let Ok(python_path) = which::which("python") {
                    return Ok(python_path);
                }
            }
        }

        return Err("Python not found. Please install Python 3.x:\n  sudo apt install python3 python3-pip\n  OR\n  sudo yum install python3 python3-pip".to_string());
    }

    #[cfg(not(any(target_os = "windows", target_os = "macos", target_os = "linux")))]
    {
        Err("Unsupported operating system".to_string())
    }
}

/// Get the bundled Bun executable path at runtime
pub fn get_bundled_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    // Determine platform-specific directory and executable name
    let (bun_dir, bun_exe) = if cfg!(target_os = "windows") {
        ("bun-windows", "bun.exe")
    } else if cfg!(target_os = "macos") {
        ("bun-macos", "bun")
    } else {
        ("bun-linux", "bun")
    };

    let bun_path = resource_dir
        .join(bun_dir)
        .join(bun_exe);

    // Verify Bun exists
    if !bun_path.exists() {
        return Err(format!("Bundled Bun not found at: {}", bun_path.display()));
    }

    Ok(bun_path)
}

/// Get a Python script path at runtime
/// Works in dev mode, production builds, and CI/CD pipelines
pub fn get_script_path(app: &tauri::AppHandle, script_name: &str) -> Result<PathBuf, String> {
    // Strategy: Try multiple paths in order until we find the script

    let mut candidate_paths = Vec::new();

    // 1. Try Tauri's resource_dir (works in production and should work in dev)
    if let Ok(resource_dir) = app.path().resource_dir() {
        candidate_paths.push(resource_dir.join("scripts").join(script_name));
    }

    // 2. Try relative to current executable (production fallback)
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(exe_dir) = exe_path.parent() {
            candidate_paths.push(exe_dir.join("resources").join("scripts").join(script_name));
            candidate_paths.push(exe_dir.join("scripts").join(script_name));
        }
    }

    // 3. Try relative to current working directory (dev mode)
    if let Ok(cwd) = std::env::current_dir() {
        // If CWD is src-tauri
        candidate_paths.push(cwd.join("resources").join("scripts").join(script_name));
        // If CWD is project root
        candidate_paths.push(cwd.join("src-tauri").join("resources").join("scripts").join(script_name));
    }

    // Try each candidate path
    for path in candidate_paths.iter() {
        if path.exists() {
            // Strip the \\?\ prefix that Python can't handle on Windows
            let path_str = path.to_string_lossy().to_string();
            let clean_path = if path_str.starts_with(r"\\?\") {
                PathBuf::from(&path_str[4..])
            } else {
                path.clone()
            };

            return Ok(clean_path);
        }
    }

    Err(format!(
        "Script '{}' not found in any of {} candidate locations",
        script_name,
        candidate_paths.len()
    ))
}

/// Create a Command that hides console windows on Windows
/// Use this instead of Command::new("python") to prevent terminal windows
pub fn python_command() -> Command {
    #[cfg(target_os = "windows")]
    {
        let mut cmd = Command::new("python");
        cmd.creation_flags(CREATE_NO_WINDOW);
        cmd
    }

    #[cfg(not(target_os = "windows"))]
    {
        Command::new("python")
    }
}

/// Execute a Python script with arguments and return the output
/// This function automatically hides console windows on Windows
pub fn execute_python_command(
    app: &tauri::AppHandle,
    script_name: &str,
    args: &[String],
) -> Result<String, String> {
    let python_path = get_python_path(app)?;
    let script_path = get_script_path(app, script_name)?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Script not found at: {}",
            script_path.display()
        ));
    }

    // Build command
    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path).args(args);

    // Get Polygon API key from window storage via webview
    // For now, we'll check common environment variable
    if let Ok(api_key) = std::env::var("POLYGON_API_KEY") {
        cmd.env("POLYGON_API_KEY", api_key);
    }

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    // Execute and capture output
    let output = cmd
        .output()
        .map_err(|e| format!("Failed to execute command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Command failed: {}", stderr))
    }
}
