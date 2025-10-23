// Polygon.io data commands
use serde::{Deserialize, Serialize};
use crate::utils::python::{get_python_path, get_script_path};
use std::process::Command;

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
    // Get the Python script path - use CARGO_MANIFEST_DIR for correct path resolution
    let python_path = get_python_path(&app)?;
    let script_path = get_script_path(&app, "polygon_data.py")?;

    // Verify script exists
    if !script_path.exists() {
        return Err(format!(
            "Polygon script not found at: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut cmd_args = vec![script_path.to_string_lossy().to_string(), command];
    cmd_args.extend(args);

    // Execute Python script
    let output = Command::new(&python_path)
        .args(&cmd_args)
        .output()
        .map_err(|e| format!("Failed to execute command: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Err(format!("Command failed: {}", stderr))
    }
}
