// NASA GIBS (Global Imagery Browse Services) API commands
#![allow(dead_code)]
use crate::utils::python::execute_python_command;

/// Execute NASA GIBS Python script command
#[tauri::command]
pub async fn execute_nasa_gibs_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    execute_python_command(&app, "nasa_gibs_api.py", &cmd_args)
}

/// List all available imagery layers from NASA GIBS
#[tauri::command]
pub async fn get_nasa_gibs_layers(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "list-layers".to_string(), vec![]).await
}

/// Get detailed information about a specific layer
#[tauri::command]
pub async fn get_nasa_gibs_layer_details(
    app: tauri::AppHandle,
    layer_id: String,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "layer-details".to_string(), vec![layer_id]).await
}

/// Search for layers by keyword
#[tauri::command]
pub async fn search_nasa_gibs_layers(
    app: tauri::AppHandle,
    query: String,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "search-layers".to_string(), vec![query]).await
}

/// Get available time periods for a specific layer
#[tauri::command]
pub async fn get_nasa_gibs_time_periods(
    app: tauri::AppHandle,
    layer_id: String,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "time-periods".to_string(), vec![layer_id]).await
}

/// Get a satellite image tile for specific coordinates and time
#[tauri::command]
pub async fn get_nasa_gibs_tile(
    app: tauri::AppHandle,
    layer_id: String,
    date: String,
    zoom: i32,
    lat: f64,
    lon: f64,
) -> Result<String, String> {
    let args = vec![
        layer_id,
        date,
        zoom.to_string(),
        lat.to_string(),
        lon.to_string(),
    ];
    execute_nasa_gibs_command(app, "get-tile".to_string(), args).await
}

/// Get multiple tiles for a bounding box area
#[tauri::command]
pub async fn get_nasa_gibs_tile_batch(
    app: tauri::AppHandle,
    layer_id: String,
    date: String,
    zoom: i32,
    lat_min: f64,
    lon_min: f64,
    lat_max: f64,
    lon_max: f64,
) -> Result<String, String> {
    let args = vec![
        layer_id,
        date,
        zoom.to_string(),
        lat_min.to_string(),
        lon_min.to_string(),
        lat_max.to_string(),
        lon_max.to_string(),
    ];
    execute_nasa_gibs_command(app, "get-tile-batch".to_string(), args).await
}

/// Get popular imagery layers based on common usage
#[tauri::command]
pub async fn get_nasa_gibs_popular_layers(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "popular-layers".to_string(), vec![]).await
}

/// Get list of supported coordinate projections
#[tauri::command]
pub async fn get_nasa_gibs_supported_projections(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "supported-projections".to_string(), vec![]).await
}

/// Test all NASA GIBS API endpoints
#[tauri::command]
pub async fn test_nasa_gibs_all_endpoints(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_nasa_gibs_command(app, "test-all".to_string(), vec![]).await
}
