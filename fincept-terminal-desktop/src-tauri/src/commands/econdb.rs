// EconDB economic database commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute EconDB Python script command
#[tauri::command]
pub async fn execute_econdb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "econdb_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "EconDB script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute EconDB command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("EconDB command failed: {}", stderr))
    }
}

/// Get economic series data
#[tauri::command]
pub async fn get_econdb_series(
    app: tauri::AppHandle,
    ticker: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![ticker];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_econdb_command(app, "series".to_string(), args).await
}

/// Search economic indicators
#[tauri::command]
pub async fn search_econdb_indicators(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_econdb_command(app, "search".to_string(), vec![query]).await
}

/// Get available datasets
#[tauri::command]
pub async fn get_econdb_datasets(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_econdb_command(app, "datasets".to_string(), vec![]).await
}

/// Get country-specific economic data
#[tauri::command]
pub async fn get_econdb_country_data(
    app: tauri::AppHandle,
    country_code: String,
) -> Result<String, String> {
    execute_econdb_command(app, "country".to_string(), vec![country_code]).await
}

/// Get multiple series
#[tauri::command]
pub async fn get_econdb_multiple_series(
    app: tauri::AppHandle,
    tickers: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![tickers];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_econdb_command(app, "multiple_series".to_string(), args).await
}
