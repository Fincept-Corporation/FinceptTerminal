// Trading Economics data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute Trading Economics Python script command
#[tauri::command]
pub async fn execute_trading_economics_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "trading_economics_data.py")?;

    if !script_path.exists() {
        return Err(format!(
            "Trading Economics script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Trading Economics command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Trading Economics command failed: {}", stderr))
    }
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
