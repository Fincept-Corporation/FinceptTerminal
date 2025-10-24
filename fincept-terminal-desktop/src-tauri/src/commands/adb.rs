// Asian Development Bank data commands
use crate::utils::python::execute_python_command;

/// Execute ADB Python script command
#[tauri::command]
pub async fn execute_adb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "adb_data.py", &cmd_args)
}

/// Get economic indicators for Asian countries
#[tauri::command]
pub async fn get_adb_indicators(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_adb_command(app, "indicators".to_string(), vec![country]).await
}

/// Search ADB datasets
#[tauri::command]
pub async fn search_adb_datasets(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_adb_command(app, "search".to_string(), vec![query]).await
}

/// Get development data
#[tauri::command]
pub async fn get_adb_development_data(
    app: tauri::AppHandle,
    country: String,
) -> Result<String, String> {
    execute_adb_command(app, "development".to_string(), vec![country]).await
}
