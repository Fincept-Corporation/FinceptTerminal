use crate::utils::python::{execute_python_command, get_script_path};
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

    // Get script path
    let script_name = "technicals/technical_analysis.py";
    let script_path = get_script_path(&app, script_name)
        .map_err(|e| format!("Failed to locate script: {}", e))?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Technical analysis script not found at: {}",
            script_path.display()
        ));
    }

    // Execute Python command
    match execute_python_command(&app, script_name, &cmd_args) {
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
    // Get script path for compute_technicals.py
    let script_name = "compute_technicals.py";
    let script_path = get_script_path(&app, script_name)
        .map_err(|e| format!("Failed to locate script: {}", e))?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Compute technicals script not found at: {}",
            script_path.display()
        ));
    }

    // Execute Python command with historical data as argument
    let args = vec![historical_data];
    match execute_python_command(&app, script_name, &args) {
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
