// Japan Statistics Bureau data commands
use crate::python;

/// Execute Japan Statistics Python script command
#[tauri::command]
pub async fn execute_estat_japan_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    python::execute(&app, "estat_japan_api.py", cmd_args).await
}

/// Get statistical data
#[tauri::command]
pub async fn get_estat_japan_data(
    app: tauri::AppHandle,
    stats_code: String,
) -> Result<String, String> {
    execute_estat_japan_command(app, "data".to_string(), vec![stats_code]).await
}

/// Search statistics
#[tauri::command]
pub async fn search_estat_japan_stats(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_estat_japan_command(app, "search".to_string(), vec![query]).await
}
