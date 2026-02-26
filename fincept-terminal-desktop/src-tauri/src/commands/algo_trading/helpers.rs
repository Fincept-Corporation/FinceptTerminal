//! Helper functions for algo trading module

use std::path::PathBuf;
use tauri::Manager;

/// Get the algo trading scripts directory
pub(super) fn get_algo_scripts_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    // Production: tauri bundles resources/scripts -> scripts/
    let bundled_dir = resource_dir
        .join("scripts")
        .join("algo_trading");

    if bundled_dir.exists() {
        return Ok(bundled_dir);
    }

    // Alternate: some builds keep the resources/ prefix
    let alt_dir = resource_dir
        .join("resources")
        .join("scripts")
        .join("algo_trading");

    if alt_dir.exists() {
        return Ok(alt_dir);
    }

    // Fallback for development mode
    let dev_dir = std::env::current_dir()
        .map_err(|e| format!("Failed to get current dir: {}", e))?
        .join("src-tauri")
        .join("resources")
        .join("scripts")
        .join("algo_trading");

    if dev_dir.exists() {
        return Ok(dev_dir);
    }

    Err("Algo trading scripts directory not found".to_string())
}

/// Get the main database path for passing to Python subprocesses
pub(super) fn get_main_db_path_str() -> Result<String, String> {
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .map_err(|_| "APPDATA not set".to_string())?;
        PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .map_err(|_| "HOME not set".to_string())?;
        PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .map_err(|_| "HOME not set".to_string())?;
        let xdg = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        PathBuf::from(xdg).join("fincept-terminal")
    };
    Ok(db_dir.join("fincept_terminal.db").to_string_lossy().to_string())
}

/// Get the strategies directory (for Python strategies)
pub(super) fn get_strategies_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    // Production: tauri bundles resources/scripts -> scripts/
    let bundled_dir = resource_dir
        .join("scripts")
        .join("strategies");

    if bundled_dir.exists() {
        return Ok(bundled_dir);
    }

    // Alternate: some builds keep the resources/ prefix
    let alt_dir = resource_dir
        .join("resources")
        .join("scripts")
        .join("strategies");

    if alt_dir.exists() {
        return Ok(alt_dir);
    }

    // Fallback for development mode
    let dev_dir = std::env::current_dir()
        .map_err(|e| format!("Failed to get current dir: {}", e))?
        .join("src-tauri")
        .join("resources")
        .join("scripts")
        .join("strategies");

    if dev_dir.exists() {
        return Ok(dev_dir);
    }

    Err("Strategies directory not found".to_string())
}
