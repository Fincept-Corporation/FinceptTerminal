// Alpha Vantage data commands
use std::process::Command;

/// Execute Alpha Vantage Python script command
#[tauri::command]
pub async fn execute_alphavantage_command(
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Get the Python script path
    let manifest_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR"));
    let script_path = manifest_dir
        .join("resources")
        .join("scripts")
        .join("alphavantage_data.py");

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Alpha Vantage script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new("python")
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute Alpha Vantage command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Alpha Vantage command failed: {}", stderr))
    }
}

/// Get quote for a single symbol
#[tauri::command]
pub async fn get_alphavantage_quote(symbol: String) -> Result<String, String> {
    execute_alphavantage_command("quote".to_string(), vec![symbol]).await
}

/// Get daily historical data
#[tauri::command]
pub async fn get_alphavantage_daily(
    symbol: String,
    outputsize: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(size) = outputsize {
        args.push(size);
    }
    execute_alphavantage_command("daily".to_string(), args).await
}

/// Get intraday data
#[tauri::command]
pub async fn get_alphavantage_intraday(
    symbol: String,
    interval: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(interval) = interval {
        args.push(interval);
    }
    execute_alphavantage_command("intraday".to_string(), args).await
}

/// Get company overview
#[tauri::command]
pub async fn get_alphavantage_overview(symbol: String) -> Result<String, String> {
    execute_alphavantage_command("overview".to_string(), vec![symbol]).await
}

/// Search for symbols/companies
#[tauri::command]
pub async fn search_alphavantage_symbols(keywords: String) -> Result<String, String> {
    execute_alphavantage_command("search".to_string(), vec![keywords]).await
}

/// Get comprehensive data from multiple endpoints
#[tauri::command]
pub async fn get_alphavantage_comprehensive(symbol: String) -> Result<String, String> {
    execute_alphavantage_command("comprehensive".to_string(), vec![symbol]).await
}

/// Get market movers
#[tauri::command]
pub async fn get_alphavantage_market_movers() -> Result<String, String> {
    execute_alphavantage_command("market_movers".to_string(), vec![]).await
}