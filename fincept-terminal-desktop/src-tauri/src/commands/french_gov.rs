// French Government data commands
use crate::utils::python::execute_python_command;

/// Execute French Government Python script command
#[tauri::command]
pub async fn execute_french_gov_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "french_gov_api.py", &cmd_args)
}

/// Search French government datasets
#[tauri::command]
pub async fn search_french_gov_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_french_gov_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_french_gov_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_french_gov_command(app, "dataset".to_string(), vec![dataset_id]).await
}
