// OpenAFRICA data portal commands
use crate::utils::python::execute_python_command;

/// Execute OpenAFRICA Python script command
#[tauri::command]
pub async fn execute_openafrica_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "openafrica_api.py", &cmd_args)
}

/// Search African datasets
#[tauri::command]
pub async fn search_openafrica_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "search".to_string(), vec![query]).await
}

/// Get dataset by ID
#[tauri::command]
pub async fn get_openafrica_dataset(
    app: tauri::AppHandle,
    dataset_id: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "dataset".to_string(), vec![dataset_id]).await
}

/// Get datasets by country
#[tauri::command]
pub async fn get_openafrica_country_datasets(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_openafrica_command(app, "country".to_string(), vec![country]).await
}
