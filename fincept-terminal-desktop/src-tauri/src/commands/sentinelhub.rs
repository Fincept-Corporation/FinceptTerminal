// Sentinel Hub satellite data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Sentinel Hub Python script command
#[tauri::command]
pub async fn execute_sentinelhub_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "sentinelhub_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Sentinel Hub script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Sentinel Hub command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Sentinel Hub command failed: {}", stderr))
    }
}

/// Get satellite imagery
#[tauri::command]
pub async fn get_sentinelhub_imagery(
    app: tauri::AppHandle,
    location: String,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![location];
    if let Some(d) = date {
        args.push(d);
    }
    execute_sentinelhub_command(app, "imagery".to_string(), args).await
}

/// Get available collections
#[tauri::command]
pub async fn get_sentinelhub_collections(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_sentinelhub_command(app, "collections".to_string(), vec![]).await
}

/// Search satellite data
#[tauri::command]
pub async fn search_sentinelhub_data(
    app: tauri::AppHandle,
    bbox: String,
    start_date: String,
    end_date: String,
) -> Result<String, String> {
    execute_sentinelhub_command(app, "search".to_string(), vec![bbox, start_date, end_date]).await
}
