// Edgar (SEC filings) data commands using edgartools library
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Execute Edgar Python script command with PyO3
#[tauri::command]
pub async fn execute_edgar_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with PyO3
    let script_path = get_script_path(&app, "edgar_tools.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}
