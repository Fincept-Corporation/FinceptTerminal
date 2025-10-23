// Utility module for Python execution path resolution
use std::path::PathBuf;
use tauri::Manager;

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

    Ok(resource_dir
        .join("resources")
        .join("python")
        .join(python_exe))
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
