// Databento market data commands
use crate::utils::python::execute_python_command;

/// Execute Databento Python script command
#[tauri::command]
pub async fn execute_databento_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "explore_databento_data.py", &cmd_args)
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
