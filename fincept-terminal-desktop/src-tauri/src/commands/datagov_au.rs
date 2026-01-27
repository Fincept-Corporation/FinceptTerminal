// Australian Government data commands
use crate::python;

/// Execute Australian Government Python script command
#[tauri::command]
pub async fn execute_datagov_au_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    
    python::execute(&app, "datagov_au_api.py", cmd_args).await
}

/// Search Australian government datasets
#[tauri::command]
pub async fn search_datagov_au_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_datagov_au_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_datagov_au_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_datagov_au_command(app, "dataset".to_string(), vec![dataset_id]).await
}
