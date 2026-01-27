use crate::python;
use tauri::AppHandle;

/// Execute technical analysis Python script
///
/// Supports three commands:
/// - yfinance: Load data from Yahoo Finance and calculate indicators
///   Example: command="yfinance", args=["AAPL", "1y", "momentum,trend"]
///
/// - csv: Load data from CSV file and calculate indicators
///   Example: command="csv", args=["/path/to/data.csv", "momentum,volume"]
///
/// - json: Load data from JSON string and calculate indicators
///   Example: command="json", args=['[{"timestamp":...,"open":...}]', "all"]
///
/// Categories: momentum, volume, volatility, trend, others
/// Default: all categories if not specified
#[tauri::command]
pub async fn execute_technical_analysis_command(
    app: AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Validate command
    let valid_commands = ["yfinance", "csv", "json", "help"];
    if !valid_commands.contains(&command.as_str()) {
        return Err(format!(
            "Invalid command: '{}'. Valid commands: {:?}",
            command, valid_commands
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![command.clone()];
    cmd_args.extend(args);

    // Execute Python script
    match python::execute(&app, "technicals/technical_analysis.py", cmd_args).await {
        Ok(output) => {
            // Check if output contains error JSON
            if output.contains("\"error\"") {
                return Err(output);
            }
            Ok(output)
        }
        Err(e) => Err(format!("Technical analysis execution failed: {}", e)),
    }
}

/// Get help information for technical analysis
#[tauri::command]
pub async fn get_technical_analysis_help(app: AppHandle) -> Result<String, String> {
    execute_technical_analysis_command(app, "help".to_string(), vec![]).await
}

/// Convenience command for yfinance data with indicators
#[tauri::command]
pub async fn calculate_indicators_yfinance(
    app: AppHandle,
    symbol: String,
    period: String,
    categories: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol, period];
    if let Some(cats) = categories {
        args.push(cats);
    }
    execute_technical_analysis_command(app, "yfinance".to_string(), args).await
}

/// Convenience command for CSV file with indicators
#[tauri::command]
pub async fn calculate_indicators_csv(
    app: AppHandle,
    filepath: String,
    categories: Option<String>,
) -> Result<String, String> {
    let mut args = vec![filepath];
    if let Some(cats) = categories {
        args.push(cats);
    }
    execute_technical_analysis_command(app, "csv".to_string(), args).await
}

/// Convenience command for JSON data with indicators
#[tauri::command]
pub async fn calculate_indicators_json(
    app: AppHandle,
    json_data: String,
    categories: Option<String>,
) -> Result<String, String> {
    let mut args = vec![json_data];
    if let Some(cats) = categories {
        args.push(cats);
    }
    execute_technical_analysis_command(app, "json".to_string(), args).await
}

/// Compute all technical indicators from historical OHLCV data
/// This command receives historical data and returns all computed technicals
#[tauri::command]
pub async fn compute_all_technicals(
    app: AppHandle,
    historical_data: String,
) -> Result<String, String> {
    // Execute Python script for compute_technicals.py
    let cmd_args = vec![historical_data];
    match python::execute(&app, "compute_technicals.py", cmd_args).await {
        Ok(output) => {
            // Check if output contains error JSON
            if output.contains("\"success\":false") || output.contains("\"error\"") {
                return Err(output);
            }
            Ok(output)
        }
        Err(e) => Err(format!("Technical computation failed: {}", e)),
    }
}
