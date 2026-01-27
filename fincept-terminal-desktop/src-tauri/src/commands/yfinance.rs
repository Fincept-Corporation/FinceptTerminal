// Yahoo Finance data commands
use crate::python;

/// Execute Yahoo Finance Python script command via subprocess
/// Uses direct subprocess instead of worker pool to avoid deadlocks on Windows
#[tauri::command]
pub async fn execute_yfinance_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script via subprocess (avoids worker pool deadlocks)
    python::execute_sync(&app, "yfinance_data.py", cmd_args)
}
