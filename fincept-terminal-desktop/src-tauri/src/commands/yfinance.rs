// Yahoo Finance data commands
use crate::python;

/// Execute Yahoo Finance Python script command via subprocess
#[tauri::command]
pub async fn execute_yfinance_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command.clone()];
    cmd_args.extend(args.clone());

    eprintln!("[RUST] execute_yfinance_command: command={}, args={:?}", command, args);

    let result = python::execute_sync(&app, "yfinance_data.py", cmd_args);

    match &result {
        Ok(output) => eprintln!("[RUST] yfinance output (first 500 chars): {}", &output.chars().take(500).collect::<String>()),
        Err(e) => eprintln!("[RUST] yfinance error: {}", e),
    }

    result
}
