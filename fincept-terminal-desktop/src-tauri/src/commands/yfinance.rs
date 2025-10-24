// Yahoo Finance data commands
use crate::utils::python::execute_python_command;

/// Execute Yahoo Finance Python script command
#[tauri::command]
pub async fn execute_yfinance_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "yfinance_data.py", &cmd_args)
}
