// EconDB economic database commands
use crate::utils::python::execute_python_command;

/// Execute EconDB Python script command
#[tauri::command]
pub async fn execute_econdb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "econdb_data.py", &cmd_args)
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
