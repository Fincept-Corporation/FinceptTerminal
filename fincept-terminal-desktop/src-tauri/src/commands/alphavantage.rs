// Alpha Vantage data commands
use crate::utils::python::execute_python_command;

/// Execute Alpha Vantage Python script command
#[tauri::command]
pub async fn execute_alphavantage_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "alphavantage_data.py", &cmd_args)
}

/// Get quote for a single symbol
#[tauri::command]
pub async fn get_alphavantage_quote(app: tauri::AppHandle, symbol: String) -> Result<String, String> {
    execute_alphavantage_command(app, "quote".to_string(), vec![symbol]).await
}

/// Get daily historical data
#[tauri::command]
pub async fn get_alphavantage_daily(
    app: tauri::AppHandle,
    symbol: String,
    outputsize: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(size) = outputsize {
        args.push(size);
    }
    execute_alphavantage_command(app, "daily".to_string(), args).await
}

/// Get intraday data
#[tauri::command]
pub async fn get_alphavantage_intraday(
    app: tauri::AppHandle,
    symbol: String,
    interval: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(interval) = interval {
        args.push(interval);
    }
    execute_alphavantage_command(app, "intraday".to_string(), args).await
}

/// Get company overview
#[tauri::command]
pub async fn get_alphavantage_overview(app: tauri::AppHandle, symbol: String) -> Result<String, String> {
    execute_alphavantage_command(app, "overview".to_string(), vec![symbol]).await
}

/// Search for symbols/companies
#[tauri::command]
pub async fn search_alphavantage_symbols(app: tauri::AppHandle, keywords: String) -> Result<String, String> {
    execute_alphavantage_command(app, "search".to_string(), vec![keywords]).await
}

/// Get comprehensive data from multiple endpoints
#[tauri::command]
pub async fn get_alphavantage_comprehensive(app: tauri::AppHandle, symbol: String) -> Result<String, String> {
    execute_alphavantage_command(app, "comprehensive".to_string(), vec![symbol]).await
}

/// Get market movers
#[tauri::command]
pub async fn get_alphavantage_market_movers(app: tauri::AppHandle) -> Result<String, String> {
    execute_alphavantage_command(app, "market_movers".to_string(), vec![]).await
}