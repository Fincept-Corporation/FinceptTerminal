// Utility module for Python script execution and path resolution
// Handles dual-venv setup (NumPy 1.x and NumPy 2.x compatibility)
// Execution methods:
//   1. Worker pool via python_runtime (fast, persistent workers)
//   2. Direct subprocess execution (fallback, more reliable on Windows)
use std::path::PathBuf;
use tauri::Manager;

/// NumPy 1.x compatible libraries (use venv-numpy1)
/// Based on requirements-numpy1.txt - includes backtesting, time series,
/// portfolio optimization, and quant finance libraries
const NUMPY1_LIBRARIES: &[&str] = &[
    // Backtesting and trading
    "vectorbt",
    "backtesting",
    // Time series and forecasting
    "gluonts",
    "functime",
    // Portfolio optimization
    "PyPortfolioOpt",
    "pyportfolioopt",
    "finquant",
    // Qlib and RD-Agent (match both script paths and library names)
    "pyqlib",
    "qlib",           // matches qlib_service.py paths
    "rdagent",
    "rd_agent",       // matches rd_agent_service.py paths
    "gs-quant",
    "gs_quant",
    // Financial modeling libraries (NumPy 1.x dependent due to numba/llvmlite)
    "ffn",            // matches ffn_wrapper, ffn_service.py
    "fortitudo",      // matches fortitudo_tech_wrapper, fortitudo_service.py
    "financepy",
    // AI Quant Lab uses Qlib/RD-Agent which need NumPy 1.x
    "ai_quant_lab",
];

/// Determine which venv to use based on library name
fn get_venv_for_library(library_name: Option<&str>) -> &'static str {
    if let Some(lib) = library_name {
        // Check if library requires NumPy 1.x
        if NUMPY1_LIBRARIES.iter().any(|&numpy1_lib| lib.contains(numpy1_lib)) {
            return "venv-numpy1";
        }
    }
    // Default to NumPy 2.x venv
    "venv-numpy2"
}

/// Get the Python executable path from app installation directory
/// Supports dual-venv setup for NumPy 1.x and 2.x compatibility
/// In production: Uses installed Python from setup.rs
/// In development: Uses %LOCALAPPDATA%/fincept-dev (matches setup.rs)
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    get_python_path_for_library(app, None)
}

/// Get Python path for a specific library (switches between numpy1 and numpy2 venvs)
pub fn get_python_path_for_library(app: &tauri::AppHandle, library_name: Option<&str>) -> Result<PathBuf, String> {
    // Get install directory - MUST match setup.rs get_install_dir()
    let install_dir = if cfg!(debug_assertions) {
        // Dev mode: use LOCALAPPDATA/fincept-dev
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
        // Production: use app data directory
        app.path().app_data_dir()
            .map_err(|e| format!("Failed to get app data dir: {}", e))?
    };

    // Determine which venv to use based on library
    let venv_name = get_venv_for_library(library_name);

    // Platform-specific Python executable location in venv
    let python_exe = if cfg!(target_os = "windows") {
        install_dir.join(format!("{}/Scripts/python.exe", venv_name))
    } else {
        install_dir.join(format!("{}/bin/python3", venv_name))
    };


    // Check if venv Python exists
    if python_exe.exists() {
        // Strip the \\?\ prefix that can cause issues on Windows
        let path_str = python_exe.to_string_lossy().to_string();
        let clean_path = if path_str.starts_with(r"\\?\") {
            PathBuf::from(&path_str[4..])
        } else {
            python_exe.clone()
        };
        return Ok(clean_path);
    }

    // Fallback to system Python in dev mode only
    #[cfg(debug_assertions)]
    {
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

    // If we get here, Python is not available
    Err(format!(
        "Python interpreter not found at: {}\n\n\
        The Python runtime is required to run analytics and data processing features.\n\n\
        Troubleshooting steps:\n\
        1. Run the setup wizard from the application to install Python\n\
        2. Ensure both venv-numpy1 and venv-numpy2 are created\n\
        3. If setup fails, ensure Microsoft Visual C++ Redistributable is installed:\n\
           Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe\n\
        4. Restart the application after installation",
        python_exe.display()
    ))
}

/// Get the Bun executable path from app installation directory
pub fn get_bundled_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    // Get install directory - MUST match setup.rs get_install_dir()
    let install_dir = if cfg!(debug_assertions) {
        // Dev mode: use LOCALAPPDATA/fincept-dev
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
        // Production: use app data directory
        app.path().app_data_dir()
            .map_err(|e| format!("Failed to get app data dir: {}", e))?
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
    // Debug logging only in debug builds
    #[cfg(debug_assertions)]
    {
        eprintln!("[SCRIPT_PATH_DEBUG] Searching for: {}", script_name);
    }

    for (_i, path) in candidate_paths.iter().enumerate() {
        #[cfg(debug_assertions)]
        {
            eprintln!("[SCRIPT_PATH_DEBUG] Candidate {}: {:?}", _i+1, path);
        }

        if path.exists() {
            #[cfg(debug_assertions)]
            {
                eprintln!("[SCRIPT_PATH_DEBUG] Found at candidate {}", _i+1);
            }

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

/// Execute Python script with worker pool
/// This is the primary execution method - fast, persistent workers, no subprocess spawning
pub fn execute_python_script_simple(
    app: &tauri::AppHandle,
    script_relative_path: &str,
    args: &[&str],
) -> Result<String, String> {
    let script_path = get_script_path(app, script_relative_path)?;
    let args_vec: Vec<String> = args.iter().map(|s| s.to_string()).collect();

    // Execute with worker pool (sync wrapper)
    crate::python_runtime::execute_python_script(&script_path, args_vec)
}

/// Execute Python script directly using subprocess (bypasses worker pool)
/// Use this for commands that need specific venv or when worker pool has issues
/// This is slower but more reliable on Windows
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

pub fn execute_python_subprocess(
    app: &tauri::AppHandle,
    script_relative_path: &str,
    args: &[String],
    library_hint: Option<&str>,
) -> Result<String, String> {
    use std::process::Command;
    #[cfg(target_os = "windows")]
    use std::os::windows::process::CommandExt;

    let script_path = get_script_path(app, script_relative_path)?;
    let python_path = get_python_path_for_library(app, library_hint)?;

    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path)
        .args(args)
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped());

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python: {}", e))?;

    if output.status.success() {
        let raw_output = String::from_utf8(output.stdout)
            .map_err(|e| format!("Failed to parse output: {}", e))?;

        // Extract JSON from output - handles case where logging/warnings leak to stdout
        // Look for the last line that starts with '{' or '[' (our JSON result)
        let json_line = raw_output
            .lines()
            .rev()
            .find(|line| {
                let trimmed = line.trim();
                trimmed.starts_with('{') || trimmed.starts_with('[')
            });

        match json_line {
            Some(json) => Ok(json.to_string()),
            None => {
                // If no JSON found, return raw output (might be plain text result)
                Ok(raw_output.trim().to_string())
            }
        }
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        Err(format!("Python script failed:\nstderr: {}\nstdout: {}", stderr, stdout))
    }
}

/// Execute Python script with data passed via stdin (for large payloads)
/// This avoids Windows command line length limits
pub fn execute_python_subprocess_with_stdin(
    app: &tauri::AppHandle,
    script_relative_path: &str,
    command: &str,
    stdin_data: &str,
    library_hint: Option<&str>,
) -> Result<String, String> {
    use std::process::{Command, Stdio};
    use std::io::Write;
    #[cfg(target_os = "windows")]
    use std::os::windows::process::CommandExt;

    let script_path = get_script_path(app, script_relative_path)?;
    let python_path = get_python_path_for_library(app, library_hint)?;

    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path)
        .arg(command)
        .arg("--stdin")  // Signal to read from stdin
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    let mut child = cmd.spawn()
        .map_err(|e| format!("Failed to spawn Python: {}", e))?;

    // Write data to stdin
    if let Some(mut stdin) = child.stdin.take() {
        stdin.write_all(stdin_data.as_bytes())
            .map_err(|e| format!("Failed to write to stdin: {}", e))?;
    }

    let output = child.wait_with_output()
        .map_err(|e| format!("Failed to wait for Python: {}", e))?;

    if output.status.success() {
        String::from_utf8(output.stdout)
            .map_err(|e| format!("Failed to parse output: {}", e))
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        let stdout = String::from_utf8_lossy(&output.stdout);
        Err(format!("Python script failed:\nstderr: {}\nstdout: {}", stderr, stdout))
    }
}
