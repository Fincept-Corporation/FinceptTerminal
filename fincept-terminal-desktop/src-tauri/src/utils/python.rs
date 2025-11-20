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

#[cfg(not(target_os = "windows"))]
use once_cell::sync::OnceCell;
#[cfg(not(target_os = "windows"))]
use std::env;
#[cfg(not(target_os = "windows"))]
static PYTHON_PATH: OnceCell<PathBuf> = OnceCell::new();

/// Get the Python executable path at runtime (Windows uses bundled runtime)
#[cfg(target_os = "windows")]
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    let base_dir = resource_dir.join("resources").join("python-windows");
    let candidates = [
        base_dir.join("python").join("bin").join("python.exe"),
        base_dir.join("python.exe"),
        base_dir.join("pythonw.exe"),
    ];

    eprintln!("[Python] Resource dir: {}", resource_dir.display());
    for candidate in &candidates {
        eprintln!(
            "[Python] Looking for bundled Windows Python at: {} (exists: {})",
            candidate.display(),
            candidate.exists()
        );
        if candidate.exists() {
            return Ok(candidate.clone());
        }
    }

    Err(format!(
        "Python executable not found in bundled resources under {}",
        base_dir.display()
    ))
}

/// macOS/Linux rely on an existing Python 3 installation
#[cfg(not(target_os = "windows"))]
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    cached_python_path(Some(app))
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

    let bun_path = resource_dir.join("resources").join(bun_dir).join(bun_exe);

    // Debug logging
    eprintln!("[Bun] Resource dir: {}", resource_dir.display());
    eprintln!("[Bun] Platform dir: {}", bun_dir);
    eprintln!("[Bun] Looking for Bun at: {}", bun_path.display());
    eprintln!("[Bun] Bun exists: {}", bun_path.exists());

    // Verify Bun exists
    if !bun_path.exists() {
        return Err(format!("Bundled Bun not found at: {}", bun_path.display()));
    }

    Ok(bun_path)
}

/// Get a Python script path at runtime
pub fn get_script_path(app: &tauri::AppHandle, script_name: &str) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    Ok(resource_dir
        .join("resources")
        .join("scripts")
        .join(script_name))
}

/// Create a Command that hides console windows on Windows
/// Use this instead of Command::new("python") to prevent terminal windows
#[cfg(target_os = "windows")]
pub fn python_command() -> Command {
    let mut cmd = Command::new("python");
    cmd.creation_flags(CREATE_NO_WINDOW);
    cmd
}

#[cfg(not(target_os = "windows"))]
pub fn python_command() -> Command {
    match cached_python_path(None) {
        Ok(path) => Command::new(path),
        Err(err) => {
            eprintln!("[Python] {err} - falling back to `python3` on PATH");
            Command::new("python3")
        }
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
        return Err(format!("Script not found at: {}", script_path.display()));
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

#[cfg(not(target_os = "windows"))]
fn find_system_python() -> Result<PathBuf, String> {
    let candidates = collect_python_candidates();

    for candidate in candidates {
        if candidate.contains(std::path::MAIN_SEPARATOR) || candidate.starts_with('.') {
            let path = expand_home_path(&candidate);
            eprintln!("[Python] Checking candidate path: {}", path.display());
            if path.exists() {
                return Ok(path);
            }
        } else if let Some(found) = which_python(&candidate) {
            eprintln!(
                "[Python] Found `{}` via PATH at: {}",
                candidate,
                found.display()
            );
            return Ok(found);
        } else {
            eprintln!("[Python] `{}` not found on PATH", candidate);
        }
    }

    Err(
        "Unable to locate a Python 3 executable. Install Python via Homebrew (`brew install python@3`), Conda, or set the FINCEPT_PYTHON_PATH environment variable."
            .to_string(),
    )
}

#[cfg(not(target_os = "windows"))]
fn cached_python_path(app: Option<&tauri::AppHandle>) -> Result<PathBuf, String> {
    PYTHON_PATH
        .get_or_try_init(|| {
            if let Some(app_handle) = app {
                if let Some(path) = bundled_python_from_app(app_handle) {
                    eprintln!("[Python] Using bundled interpreter at: {}", path.display());
                    return Ok(path);
                } else {
                    eprintln!("[Python] Bundled interpreter not found, probing system...");
                }
            }

            let path = find_system_python()?;
            eprintln!("[Python] Using system interpreter at: {}", path.display());
            Ok(path)
        })
        .map(|p| p.clone())
}

#[cfg(not(target_os = "windows"))]
pub fn seed_python_path(app: &tauri::AppHandle) {
    if PYTHON_PATH.get().is_some() {
        return;
    }

    if let Some(path) = bundled_python_from_app(app) {
        let path_str = path.to_string_lossy().to_string();
        let _ = env::set_var("FINCEPT_PYTHON_PATH", &path_str);
        let _ = PYTHON_PATH.set(path.clone());
        eprintln!("[Python] Using bundled interpreter at: {}", path.display());
    } else {
        eprintln!(
            "[Python] Bundled interpreter not found; system Python will be used if available."
        );
    }
}

#[cfg(not(target_os = "windows"))]
fn bundled_python_from_app(app: &tauri::AppHandle) -> Option<PathBuf> {
    let resource_dir = app.path().resource_dir().ok()?;
    bundled_python_from_resource_dir(&resource_dir)
}

#[cfg(not(target_os = "windows"))]
fn bundled_python_from_resource_dir(resource_dir: &PathBuf) -> Option<PathBuf> {
    let base_dir = if cfg!(target_os = "macos") {
        resource_dir.join("resources").join("python-macos")
    } else {
        resource_dir.join("resources").join("python-linux")
    };

    let candidates = [
        base_dir.join("python").join("bin").join("python3"),
        base_dir.join("python").join("bin").join("python"),
        base_dir.join("python3"),
        base_dir.join("python"),
    ];

    for candidate in candidates {
        if candidate.is_file() {
            return Some(candidate);
        }
    }

    None
}

#[cfg(not(target_os = "windows"))]
fn collect_python_candidates() -> Vec<String> {
    let mut candidates: Vec<String> = Vec::new();

    for var in ["FINCEPT_PYTHON_PATH", "PYTHON_EXECUTABLE"] {
        if let Ok(value) = env::var(var) {
            push_candidate(&mut candidates, value);
        }
    }

    if let Ok(conda_prefix) = env::var("CONDA_PREFIX") {
        let bin_dir = PathBuf::from(&conda_prefix).join("bin");
        push_candidate(
            &mut candidates,
            bin_dir.join("python3").to_string_lossy().to_string(),
        );
        push_candidate(
            &mut candidates,
            bin_dir.join("python").to_string_lossy().to_string(),
        );
    }

    if let Ok(virtual_env) = env::var("VIRTUAL_ENV") {
        let bin_dir = PathBuf::from(&virtual_env).join("bin");
        push_candidate(
            &mut candidates,
            bin_dir.join("python3").to_string_lossy().to_string(),
        );
        push_candidate(
            &mut candidates,
            bin_dir.join("python").to_string_lossy().to_string(),
        );
    }

    if let Ok(pyenv_root) = env::var("PYENV_ROOT") {
        push_candidate(
            &mut candidates,
            PathBuf::from(&pyenv_root)
                .join("shims")
                .join("python3")
                .to_string_lossy()
                .to_string(),
        );
    }

    for path in [
        "/opt/homebrew/bin/python3",
        "/usr/local/bin/python3",
        "/usr/bin/python3",
        "/usr/local/bin/python",
        "/usr/bin/python",
    ] {
        push_candidate(&mut candidates, path.to_string());
    }

    // Command names get checked via `which`
    push_candidate(&mut candidates, "python3".to_string());
    push_candidate(&mut candidates, "python".to_string());

    candidates
}

#[cfg(not(target_os = "windows"))]
fn push_candidate(candidates: &mut Vec<String>, candidate: String) {
    let trimmed = candidate.trim();
    if trimmed.is_empty() {
        return;
    }
    if !candidates.iter().any(|existing| existing == trimmed) {
        candidates.push(trimmed.to_string());
    }
}

#[cfg(not(target_os = "windows"))]
fn expand_home_path(candidate: &str) -> PathBuf {
    if let Some(stripped) = candidate.strip_prefix("~/") {
        if let Ok(home) = env::var("HOME") {
            return PathBuf::from(home).join(stripped);
        }
    }
    PathBuf::from(candidate)
}

#[cfg(not(target_os = "windows"))]
fn which_python(binary: &str) -> Option<PathBuf> {
    match Command::new("which").arg(binary).output() {
        Ok(output) if output.status.success() => {
            let stdout = String::from_utf8_lossy(&output.stdout).trim().to_string();
            if stdout.is_empty() {
                None
            } else {
                let path = PathBuf::from(stdout);
                if path.exists() {
                    Some(path)
                } else {
                    None
                }
            }
        }
        Ok(_) => None,
        Err(err) => {
            eprintln!("[Python] Failed to execute `which {}`: {}", binary, err);
            None
        }
    }
}
