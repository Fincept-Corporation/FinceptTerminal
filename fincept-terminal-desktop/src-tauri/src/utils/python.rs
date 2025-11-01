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
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    let python_exe = if cfg!(target_os = "windows") {
        "python.exe"
    } else {
        "python"
    };

    let python_path = resource_dir
        .join("resources")
        .join("python")
        .join(python_exe);

    // Debug logging
    eprintln!("[Python] Resource dir: {}", resource_dir.display());
    eprintln!("[Python] Looking for Python at: {}", python_path.display());
    eprintln!("[Python] Python exists: {}", python_path.exists());

    // Verify Python exists
    if !python_path.exists() {
        return Err(format!(
            "Python executable not found at: {}. Resource dir: {}",
            python_path.display(),
            resource_dir.display()
        ));
    }

    Ok(python_path)
}

/// Get the bundled Bun executable path at runtime
pub fn get_bundled_bun_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource directory: {}", e))?;

    let bun_exe = if cfg!(target_os = "windows") {
        "bun.exe"
    } else {
        "bun"
    };

    let bun_path = resource_dir
        .join("resources")
        .join("bun")
        .join(bun_exe);

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
pub fn python_command() -> Command {
    let mut cmd = Command::new("python");

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    cmd
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
