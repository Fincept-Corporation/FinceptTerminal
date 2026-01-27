// Polygon.io data commands
use serde::{Deserialize, Serialize};
use crate::python;

#[derive(Debug, Serialize, Deserialize)]
pub struct PolygonResponse {
    pub success: bool,
    pub data: Option<String>,
    pub error: Option<String>,
}


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

    python::execute(&app, "polygon_data.py", cmd_args).await
}
