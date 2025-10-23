// Databento market data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Databento Python script command
#[tauri::command]
pub async fn execute_databento_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "explore_databento_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Databento script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Databento command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Databento command failed: {}", stderr))
    }
}

/// Get market data
#[tauri::command]
pub async fn get_databento_market_data(
    app: tauri::AppHandle,
    symbol: String,
    dataset: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(ds) = dataset {
        args.push(ds);
    }
    execute_databento_command(app, "market_data".to_string(), args).await
}

/// Get available datasets
#[tauri::command]
pub async fn get_databento_datasets(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_databento_command(app, "datasets".to_string(), vec![]).await
}

/// Get historical data
#[tauri::command]
pub async fn get_databento_historical(
    app: tauri::AppHandle,
    symbol: String,
    start_date: String,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol, start_date];
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_databento_command(app, "historical".to_string(), args).await
}
