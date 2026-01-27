// Geocoding commands using geopy Python library
use crate::python;


#[tauri::command]
pub async fn execute_geocoding_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);


    python::execute(&app, "geocoding_service.py", cmd_args).await
}

/// Search for locations with autocomplete
#[tauri::command]
pub async fn search_geocode(
    app: tauri::AppHandle,
    query: String,
    limit: Option<u32>,
) -> Result<String, String> {
    let limit_str = limit.unwrap_or(5).to_string();
    execute_geocoding_command(
        app,
        "search".to_string(),
        vec![query, limit_str]
    ).await
}

/// Reverse geocode coordinates to location
#[tauri::command]
pub async fn reverse_geocode(
    app: tauri::AppHandle,
    lat: f64,
    lng: f64,
) -> Result<String, String> {
    execute_geocoding_command(
        app,
        "reverse".to_string(),
        vec![lat.to_string(), lng.to_string()]
    ).await
}
