// German Government data commands
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Execute German Government Python script command
#[tauri::command]
pub async fn execute_govdata_de_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with PyO3
    let script_path = get_script_path(&app, "govdata_de_api_complete.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

/// Search German government datasets
#[tauri::command]
pub async fn search_govdata_de_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_govdata_de_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_govdata_de_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_govdata_de_command(app, "dataset".to_string(), vec![dataset_id]).await
}
