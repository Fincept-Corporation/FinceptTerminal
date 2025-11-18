// Utility module for Python execution path resolution
use std::path::PathBuf;
use std::process::Command;
use tauri::Manager;

#[cfg(not(target_os = "windows"))]
use std::env;

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// Windows creation flags to hide console window
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

/// Get the Python executable path at runtime (Windows uses bundled runtime)
#[cfg(target_os = "windows")]
pub fn get_python_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    let python_path = resource_dir
        .join("resources")
        .join("python-windows")
        .join("python")
        .join("bin")
        .join("python.exe");

    eprintln!("[Python] Resource dir: {}", resource_dir.display());
    eprintln!("[Python] Looking for Python at: {}", python_path.display());
    eprintln!("[Python] Python exists: {}", python_path.exists());

    if !python_path.exists() {
        return Err(format!(
            "Python executable not found at: {}. Resource dir: {}",
            python_path.display(),
            resource_dir.display()
        ));
    }

    Ok(python_path)
}

/// macOS/Linux rely on an existing Python 3 installation
#[cfg(not(target_os = "windows"))]
pub fn get_python_path(_app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let python_path = find_system_python()?;
    eprintln!(
        "[Python] Using system interpreter at: {}",
        python_path.display()
    );
    Ok(python_path)
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
    match find_system_python() {
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
