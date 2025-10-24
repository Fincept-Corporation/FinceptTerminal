// Polygon.io data commands
use serde::{Deserialize, Serialize};
use crate::utils::python::execute_python_command;

#[derive(Debug, Serialize, Deserialize)]
pub struct PolygonResponse {
    pub success: bool,
    pub data: Option<String>,
    pub error: Option<String>,
}

/// Execute Polygon.io Python script command
#[tauri::command]
pub async fn execute_polygon_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    // Build command arguments
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Execute Python script with console window hidden on Windows
    execute_python_command(&app, "polygon_data.py", &cmd_args)
}
