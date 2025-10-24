// Spanish economic data commands
use crate::utils::python::execute_python_command;

/// Execute Spain data Python script command
#[tauri::command]
pub async fn execute_spain_data_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "spain_data.py", &cmd_args)
}

/// Get Spanish economic indicators
#[tauri::command]
pub async fn get_spain_economic_data(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_spain_data_command(app, "economic".to_string(), vec![]).await
}

/// Search Spanish datasets
#[tauri::command]
pub async fn search_spain_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_spain_data_command(app, "search".to_string(), vec![query]).await
}
