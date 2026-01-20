//! Alpha Arena V2 Commands
//!
//! Tauri commands for the refactored Alpha Arena system.

use std::process::Command;
use tauri::AppHandle;

/// Execute an Alpha Arena action
///
/// Takes a JSON payload with:
/// - action: The action to perform
/// - api_keys: Dict of provider API keys
/// - params: Action-specific parameters
#[tauri::command]
pub async fn alpha_arena_action(
    app: AppHandle,
    payload: String,
) -> Result<String, String> {
    // Get the Python executable path
    let python_path = get_python_path(&app)?;

    // Get the script path
    let script_path = get_script_path(&app)?;

    // Execute Python script
    let output = Command::new(&python_path)
        .arg(&script_path)
        .arg(&payload)
        .current_dir(script_path.parent().unwrap_or(&script_path))
        .output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        // Try to extract JSON error from stderr
        if let Some(json_start) = stderr.find('{') {
            if let Some(json_end) = stderr.rfind('}') {
                return Ok(stderr[json_start..=json_end].to_string());
            }
        }
        return Err(format!(
            "Python script failed: {}",
            stderr
        ));
    }

    let stdout = String::from_utf8_lossy(&output.stdout);

    // Find JSON in output
    if let Some(json_start) = stdout.find('{') {
        if let Some(json_end) = stdout.rfind('}') {
            return Ok(stdout[json_start..=json_end].to_string());
        }
    }

    Err("No JSON output from Python script".to_string())
}

/// Get API keys from database
#[tauri::command]
pub async fn get_api_keys(app: AppHandle) -> Result<String, String> {
    use crate::commands::db_commands::get_db_path;

    let db_path = get_db_path(&app).map_err(|e| e.to_string())?;

    let conn = rusqlite::Connection::open(&db_path)
        .map_err(|e| format!("Failed to open database: {}", e))?;

    // Create table if not exists
    conn.execute(
        "CREATE TABLE IF NOT EXISTS api_keys (
            provider TEXT PRIMARY KEY,
            api_key TEXT NOT NULL
        )",
        [],
    )
    .map_err(|e| format!("Failed to create table: {}", e))?;

    // Fetch all keys
    let mut stmt = conn
        .prepare("SELECT provider, api_key FROM api_keys")
        .map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let keys: std::collections::HashMap<String, String> = stmt
        .query_map([], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, String>(1)?))
        })
        .map_err(|e| format!("Failed to query: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    serde_json::to_string(&keys).map_err(|e| format!("Failed to serialize: {}", e))
}

/// Save API keys to database
#[tauri::command]
pub async fn save_api_keys(app: AppHandle, keys: String) -> Result<(), String> {
    use crate::commands::db_commands::get_db_path;

    let db_path = get_db_path(&app).map_err(|e| e.to_string())?;

    let conn = rusqlite::Connection::open(&db_path)
        .map_err(|e| format!("Failed to open database: {}", e))?;

    // Create table if not exists
    conn.execute(
        "CREATE TABLE IF NOT EXISTS api_keys (
            provider TEXT PRIMARY KEY,
            api_key TEXT NOT NULL
        )",
        [],
    )
    .map_err(|e| format!("Failed to create table: {}", e))?;

    // Parse keys
    let keys_map: std::collections::HashMap<String, String> =
        serde_json::from_str(&keys).map_err(|e| format!("Failed to parse keys: {}", e))?;

    // Insert/update each key
    for (provider, api_key) in keys_map {
        conn.execute(
            "INSERT OR REPLACE INTO api_keys (provider, api_key) VALUES (?1, ?2)",
            [&provider, &api_key],
        )
        .map_err(|e| format!("Failed to insert key: {}", e))?;
    }

    Ok(())
}

// Helper functions
fn get_python_path(app: &AppHandle) -> Result<std::path::PathBuf, String> {
    // Check for bundled Python first
    if let Ok(resource_dir) = app.path().resource_dir() {
        let bundled_python = if cfg!(windows) {
            resource_dir.join("python").join("python.exe")
        } else {
            resource_dir.join("python").join("bin").join("python3")
        };

        if bundled_python.exists() {
            return Ok(bundled_python);
        }
    }

    // Fall back to system Python
    let python_name = if cfg!(windows) { "python" } else { "python3" };

    Ok(std::path::PathBuf::from(python_name))
}

fn get_script_path(app: &AppHandle) -> Result<std::path::PathBuf, String> {
    if let Ok(resource_dir) = app.path().resource_dir() {
        let script = resource_dir
            .join("scripts")
            .join("alpha_arena")
            .join("main.py");

        if script.exists() {
            return Ok(script);
        }
    }

    Err("Alpha Arena script not found".to_string())
}
