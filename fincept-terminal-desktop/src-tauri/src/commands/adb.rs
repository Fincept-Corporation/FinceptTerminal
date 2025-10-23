// Asian Development Bank data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute ADB Python script command
#[tauri::command]
pub async fn execute_adb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "adb_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "ADB script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute ADB command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("ADB command failed: {}", stderr))
    }
}

/// Get economic indicators for Asian countries
#[tauri::command]
pub async fn get_adb_indicators(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_adb_command(app, "indicators".to_string(), vec![country]).await
}

/// Search ADB datasets
#[tauri::command]
pub async fn search_adb_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_adb_command(app, "search".to_string(), vec![query]).await
}

/// Get development data
#[tauri::command]
pub async fn get_adb_development_data(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_adb_command(app, "development".to_string(), vec![country]).await
}
