// Trading Economics data commands
use crate::utils::python::execute_python_command;

/// Execute Trading Economics Python script command
#[tauri::command]
pub async fn execute_trading_economics_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "trading_economics_data.py", &cmd_args)
}

/// Get economic indicators for a country
#[tauri::command]
pub async fn get_trading_economics_indicators(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_trading_economics_command(app, "indicators".to_string(), vec![country]).await
}

/// Get historical data for an indicator
#[tauri::command]
pub async fn get_trading_economics_historical(
    app: tauri::AppHandle,
    country: String,
    indicator: String,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![country, indicator];
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_trading_economics_command(app, "historical".to_string(), args).await
}

/// Get economic calendar events
#[tauri::command]
pub async fn get_trading_economics_calendar(
    app: tauri::AppHandle,
    country: Option<String>,
    start_date: Option<String>,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![];
    if let Some(c) = country {
        args.push(c);
    }
    if let Some(start) = start_date {
        args.push(start);
    }
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_trading_economics_command(app, "calendar".to_string(), args).await
}

/// Get market snapshots
#[tauri::command]
pub async fn get_trading_economics_markets(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_trading_economics_command(app, "markets".to_string(), vec![]).await
}

/// Get country credit ratings
#[tauri::command]
pub async fn get_trading_economics_ratings(
    app: tauri::AppHandle,
    country: Option<String>,
) -> Result<String, String> {
    let args = if let Some(c) = country {
        vec![c]
    } else {
        vec![]
    };
    execute_trading_economics_command(app, "ratings".to_string(), args).await
}

/// Get forecasts for indicators
#[tauri::command]
pub async fn get_trading_economics_forecasts(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_trading_economics_command(app, "forecasts".to_string(), vec![country]).await
}
