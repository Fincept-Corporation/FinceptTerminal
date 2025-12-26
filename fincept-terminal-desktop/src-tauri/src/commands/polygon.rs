// Polygon.io data commands
use serde::{Deserialize, Serialize};
use crate::utils::python::get_script_path;
use crate::python_runtime;

#[derive(Debug, Serialize, Deserialize)]
pub struct PolygonResponse {
    pub success: bool,
    pub data: Option<String>,
    pub error: Option<String>,
}

/// Execute Polygon.io Python script command with PyO3
#[tauri::command]
pub async fn execute_polygon_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
    api_key: Option<String>,
) -> Result<String, String> {
    if let Some(key) = api_key {
        std::env::set_var("POLYGON_API_KEY", key);
    }

    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    let script_path = get_script_path(&app, "polygon_data.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}
