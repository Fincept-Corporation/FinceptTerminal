// FRED (Federal Reserve Economic Data) commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute FRED Python script command
#[tauri::command]
pub async fn execute_fred_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "fred_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "FRED script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute FRED command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("FRED command failed: {}", stderr))
    }
}

/// Get FRED series data
#[tauri::command]
pub async fn get_fred_series(
    app: tauri::AppHandle,
    series_id: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![series_id];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_fred_command(app, "series".to_string(), args).await
}

/// Search FRED data series
#[tauri::command]
pub async fn search_fred_series(
    app: tauri::AppHandle,
    search_text: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![search_text];
    if let Some(lim) = limit {
        args.push(lim.to_string());
    }
    execute_fred_command(app, "search".to_string(), args).await
}

/// Get FRED series information
#[tauri::command]
pub async fn get_fred_series_info(
    app: tauri::AppHandle,
    series_id: String,
) -> Result<String, String> {
    execute_fred_command(app, "info".to_string(), vec![series_id]).await
}

/// Get FRED category data
#[tauri::command]
pub async fn get_fred_category(
    app: tauri::AppHandle,
    category_id: Option<i32>,
) -> Result<String, String> {
    let args = if let Some(cat_id) = category_id {
        vec![cat_id.to_string()]
    } else {
        vec![]
    };
    execute_fred_command(app, "category".to_string(), args).await
}

/// Get multiple FRED series
#[tauri::command]
pub async fn get_fred_multiple_series(
    app: tauri::AppHandle,
    series_ids: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![series_ids];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_fred_command(app, "multiple_series".to_string(), args).await
}
