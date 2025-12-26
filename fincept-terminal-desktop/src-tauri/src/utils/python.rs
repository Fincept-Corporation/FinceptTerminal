// Utility module for Python execution with PyO3
use std::path::PathBuf;
use tauri::Manager;

/// Get the Python executable path from app installation directory
/// In production: Uses installed Python from setup.rs
/// In development: Uses %LOCALAPPDATA%/fincept-dev (matches setup.rs)
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    // Get install directory (same logic as setup.rs)
    let install_dir = if cfg!(debug_assertions) {
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
        base_dir.join("fincept-dev")
    } else {
        // Production: use app resource directory
        app.path()
            .resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?
    };

    // Platform-specific Python executable location
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join("python").join("python.exe")
    } else {
        install_dir.join("python").join("bin").join("python3")
    };

    // DEVELOPMENT MODE: Use bundled Python (where packages are installed)
    #[cfg(debug_assertions)]
    {
        // Check bundled Python first (has all packages installed)
        if python_exe.exists() {
            let path_str = python_exe.to_string_lossy().to_string();
            let clean_path = if path_str.starts_with(r"\\?\") {
                PathBuf::from(&path_str[4..])
            } else {
                python_exe.clone()
            };
            return Ok(clean_path);
        }

        // Fallback to system Python if bundled not found
        use std::process::Command;
        let system_python = if cfg!(target_os = "windows") {
            "python"
        } else {
            "python3"
        };

        if let Ok(output) = Command::new(system_python).arg("--version").output() {
            if output.status.success() {
                return Ok(PathBuf::from(system_python));
            }
        }
    }

    // PRODUCTION MODE: Check bundled Python
    #[cfg(not(debug_assertions))]
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

    // If we get here, Python is not available
    Err(format!(
        "Python interpreter not found at: {}\n\n\
        The Python runtime is required to run analytics and data processing features.\n\n\
        Troubleshooting steps:\n\
        1. Run the setup wizard from the application to install Python\n\
        2. If setup fails, ensure Microsoft Visual C++ Redistributable is installed:\n\
           Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe\n\
        3. Alternatively, install Python 3.12 manually from: https://www.python.org/downloads/\n\
        4. Restart the application after installation",
        python_exe.display()
    ))
}

/// Get the Bun executable path from app installation directory
/// In production: Uses installed Bun from setup.rs
/// In development: Uses %LOCALAPPDATA%/fincept-dev (matches setup.rs)
pub fn get_bundled_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    // Get install directory (same logic as setup.rs)
    let install_dir = if cfg!(debug_assertions) {
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
        base_dir.join("fincept-dev")
    } else {
        app.path()
            .resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?
    };

    // Platform-specific Bun executable location
    let bun_exe = if cfg!(target_os = "windows") {
        install_dir.join("bun").join("bun.exe")
    } else {
        install_dir.join("bun").join("bun")
    };

    // DEVELOPMENT MODE: Prefer system Bun
    #[cfg(debug_assertions)]
    {
        use std::process::Command;

        let system_bun = if cfg!(target_os = "windows") {
            "bun.exe"
        } else {
            "bun"
        };

        // Try system Bun first in dev mode
        if let Ok(output) = Command::new(system_bun).arg("--version").output() {
            if output.status.success() {
                return Ok(PathBuf::from(system_bun));
            }
        }
    }

    // PRODUCTION MODE or dev fallback: Check bundled Bun
    if bun_exe.exists() {
        return Ok(bun_exe);
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

/// Execute Python script with PyO3 embedded runtime
/// This is the primary execution method - fast, embedded, no subprocess
pub fn execute_python_script_simple(
    app: &tauri::AppHandle,
    script_relative_path: &str,
    args: &[&str],
) -> Result<String, String> {
    let script_path = get_script_path(app, script_relative_path)?;
    let args_vec: Vec<String> = args.iter().map(|s| s.to_string()).collect();

    // Execute with PyO3
    crate::python_runtime::execute_python_script(&script_path, args_vec)
}
