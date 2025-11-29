// WITS (World Integrated Trade Solution) data commands
use crate::utils::python::execute_python_command;

/// Execute WITS Python script command
#[tauri::command]
pub async fn execute_wits_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "wits_trade_data.py", &cmd_args)
}
