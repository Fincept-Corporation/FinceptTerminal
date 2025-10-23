// Spanish economic data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Spain data Python script command
#[tauri::command]
pub async fn execute_spain_data_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "spain_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Spain data script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Spain data command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Spain data command failed: {}", stderr))
    }
}

/// Get Spanish economic indicators
#[tauri::command]
pub async fn get_spain_economic_data(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_spain_data_command(app, "economic".to_string(), vec![]).await
}

/// Search Spanish datasets
#[tauri::command]
pub async fn search_spain_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_spain_data_command(app, "search".to_string(), vec![query]).await
}
