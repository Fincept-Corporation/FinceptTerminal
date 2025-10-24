// CoinGecko cryptocurrency data commands
use crate::utils::python::execute_python_command;

/// Execute CoinGecko Python script command
#[tauri::command]
pub async fn execute_coingecko_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "coingecko.py", &cmd_args)
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
