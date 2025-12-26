// WITS (World Integrated Trade Solution) data commands
use crate::utils::python::get_script_path;
use crate::python_runtime;

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

    // Execute Python script with PyO3
    let script_path = get_script_path(&app, "wits_trade_data.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}
