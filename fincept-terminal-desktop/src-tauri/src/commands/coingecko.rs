// CoinGecko cryptocurrency data commands
use std::process::Command;
use crate::utils::python::{get_python_path, get_script_path};

/// Execute CoinGecko Python script command
#[tauri::command]
pub async fn execute_coingecko_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "coingecko.py")?;

    if !script_path.exists() {
        return Err(format!(
            "CoinGecko script not found at: {}",
            script_path.display()
        ));
    }

    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute CoinGecko command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("CoinGecko command failed: {}", stderr))
    }
}

/// Get cryptocurrency price
#[tauri::command]
pub async fn get_coingecko_price(
    app: tauri::AppHandle,
    coin_id: String,
    vs_currency: Option<String>,
) -> Result<String, String> {
    let mut args = vec![coin_id];
    if let Some(currency) = vs_currency {
        args.push(currency);
    }
    execute_coingecko_command(app, "price".to_string(), args).await
}

/// Get cryptocurrency market data
#[tauri::command]
pub async fn get_coingecko_market_data(
    app: tauri::AppHandle,
    coin_id: String,
) -> Result<String, String> {
    execute_coingecko_command(app, "market_data".to_string(), vec![coin_id]).await
}

/// Get cryptocurrency historical data
#[tauri::command]
pub async fn get_coingecko_historical(
    app: tauri::AppHandle,
    coin_id: String,
    vs_currency: String,
    days: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![coin_id, vs_currency];
    if let Some(d) = days {
        args.push(d.to_string());
    }
    execute_coingecko_command(app, "historical".to_string(), args).await
}

/// Search cryptocurrencies
#[tauri::command]
pub async fn search_coingecko_coins(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_coingecko_command(app, "search".to_string(), vec![query]).await
}

/// Get trending cryptocurrencies
#[tauri::command]
pub async fn get_coingecko_trending(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_coingecko_command(app, "trending".to_string(), vec![]).await
}

/// Get global cryptocurrency market data
#[tauri::command]
pub async fn get_coingecko_global_data(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_coingecko_command(app, "global".to_string(), vec![]).await
}

/// Get top cryptocurrencies by market cap
#[tauri::command]
pub async fn get_coingecko_top_coins(
    app: tauri::AppHandle,
    vs_currency: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![];
    if let Some(currency) = vs_currency {
        args.push(currency);
    }
    if let Some(lim) = limit {
        args.push(lim.to_string());
    }
    execute_coingecko_command(app, "top_coins".to_string(), args).await
}
