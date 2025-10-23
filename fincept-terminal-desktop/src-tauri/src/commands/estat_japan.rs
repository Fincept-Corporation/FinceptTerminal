// Japan Statistics Bureau data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Japan Statistics Python script command
#[tauri::command]
pub async fn execute_estat_japan_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "estat_japan_api.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Japan Statistics script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Japan Statistics command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Japan Statistics command failed: {}", stderr))
    }
}

/// Get statistical data
#[tauri::command]
pub async fn get_estat_japan_data(
    app: tauri::AppHandle,
    stats_code: String,
) -> Result<String, String> {
    execute_estat_japan_command(app, "data".to_string(), vec![stats_code]).await
}

/// Search statistics
#[tauri::command]
pub async fn search_estat_japan_stats(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_estat_japan_command(app, "search".to_string(), vec![query]).await
}
