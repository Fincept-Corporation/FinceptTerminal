// FRED (Federal Reserve Economic Data) commands
use crate::utils::python::execute_python_command;

/// Execute FRED Python script command
#[tauri::command]
pub async fn execute_fred_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "fred_data.py", &cmd_args)
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
