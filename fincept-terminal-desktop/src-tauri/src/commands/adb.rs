// Asian Development Bank data commands
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Execute ADB Python script command with PyO3
#[tauri::command]
pub async fn execute_adb_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with PyO3
    let script_path = get_script_path(&app, "adb_data.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
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
