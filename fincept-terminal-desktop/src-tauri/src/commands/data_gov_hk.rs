// Hong Kong Government data commands
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Execute Hong Kong Government Python script command
#[tauri::command]
pub async fn execute_data_gov_hk_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with PyO3
    let script_path = get_script_path(&app, "data_gov_hk_api.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

/// Search Hong Kong government datasets
#[tauri::command]
pub async fn search_data_gov_hk_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_data_gov_hk_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_data_gov_hk_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_data_gov_hk_command(app, "dataset".to_string(), vec![dataset_id]).await
}
