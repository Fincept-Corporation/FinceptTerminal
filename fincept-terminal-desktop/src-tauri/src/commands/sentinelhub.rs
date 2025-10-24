// Sentinel Hub satellite data commands
use crate::utils::python::execute_python_command;

/// Execute Sentinel Hub Python script command
#[tauri::command]
pub async fn execute_sentinelhub_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "sentinelhub_data.py", &cmd_args)
}

/// Get satellite imagery
#[tauri::command]
pub async fn get_sentinelhub_imagery(
    app: tauri::AppHandle,
    location: String,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![location];
    if let Some(d) = date {
        args.push(d);
    }
    execute_sentinelhub_command(app, "imagery".to_string(), args).await
}

/// Get available collections
#[tauri::command]
pub async fn get_sentinelhub_collections(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_sentinelhub_command(app, "collections".to_string(), vec![]).await
}

/// Search satellite data
#[tauri::command]
pub async fn search_sentinelhub_data(
    app: tauri::AppHandle,
    bbox: String,
    start_date: String,
    end_date: String,
) -> Result<String, String> {
    execute_sentinelhub_command(app, "search".to_string(), vec![bbox, start_date, end_date]).await
}
