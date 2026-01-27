// UK Government data commands
use crate::python;

/// Execute UK Government Python script command
#[tauri::command]
pub async fn execute_datagovuk_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    
    python::execute(&app, "datagovuk_api.py", cmd_args).await
}

/// Search UK government datasets
#[tauri::command]
pub async fn search_datagovuk_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_datagovuk_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_datagovuk_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_datagovuk_command(app, "dataset".to_string(), vec![dataset_id]).await
}
