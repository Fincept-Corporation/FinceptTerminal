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

/// Get the Python executable path from app installation directory
/// In production: Uses installed Python from setup.rs
/// In development: Falls back to system Python if installed Python not found
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    // Get the resource directory where Python was installed by setup.rs
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    // Platform-specific Python executable location
    let python_exe = if cfg!(target_os = "windows") {
        // Windows: resources/python/python.exe
        resource_dir.join("python").join("python.exe")
    } else if cfg!(target_os = "linux") {
        // Linux: resources/python/bin/python3
        resource_dir.join("python").join("bin").join("python3")
    } else if cfg!(target_os = "macos") {
        // macOS: resources/python/bin/python3
        resource_dir.join("python").join("bin").join("python3")
    } else {
        return Err("Unsupported platform".to_string());
    };

    // Check if installed Python exists
    if python_exe.exists() {
        // Strip the \\?\ prefix that can cause issues on Windows
        let path_str = python_exe.to_string_lossy().to_string();
        let clean_path = if path_str.starts_with(r"\\?\") {
            PathBuf::from(&path_str[4..])
        } else {
            python_exe
        };
        return Ok(clean_path);
    }

    // DEVELOPMENT MODE FALLBACK: Try system Python
    // This allows development without running setup first
    #[cfg(debug_assertions)]
    {
        use std::process::Command;

        let system_python = if cfg!(target_os = "windows") {
            "python"
        } else {
            "python3"
        };

        // Check if system Python is available
        if let Ok(output) = Command::new(system_python).arg("--version").output() {
            if output.status.success() {
                return Ok(PathBuf::from(system_python));
            }
        }
    }

    // If we get here, Python is not available
    Err(format!(
        "Python interpreter not found at: {}\n\nPlease run the setup process to install Python.",
        python_exe.display()
    ))
}

/// Get the Bun executable path from app installation directory
/// In production: Uses installed Bun from setup.rs
/// In development: Falls back to system Bun if installed Bun not found
pub fn get_bundled_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    // Platform-specific Bun executable location
    let bun_exe = if cfg!(target_os = "windows") {
        // Windows: resources/bun/bun.exe
        resource_dir.join("bun").join("bun.exe")
    } else if cfg!(target_os = "linux") {
        // Linux: resources/bun/bin/bun
        resource_dir.join("bun").join("bin").join("bun")
    } else if cfg!(target_os = "macos") {
        // macOS: resources/bun/bin/bun
        resource_dir.join("bun").join("bin").join("bun")
    } else {
        return Err("Unsupported platform".to_string());
    };

    // Check if installed Bun exists
    if bun_exe.exists() {
        return Ok(bun_exe);
    }

    // DEVELOPMENT MODE FALLBACK: Try system Bun
    // This allows development without running setup first
    #[cfg(debug_assertions)]
    {
        use std::process::Command;

        let system_bun = if cfg!(target_os = "windows") {
            "bun.exe"
        } else {
            "bun"
        };

        // Check if system Bun is available
        if let Ok(output) = Command::new(system_bun).arg("--version").output() {
            if output.status.success() {
                return Ok(PathBuf::from(system_bun));
            }
        }
    }

    // If we get here, Bun is not available
    Err(format!(
        "Bun not found at: {}\n\nPlease run the setup process to install Bun.",
        bun_exe.display()
    ))
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
