//! Alpha Arena V2 Commands
//!
//! Tauri commands for the refactored Alpha Arena system.
//! Refactored to use unified python module API.

use tauri::AppHandle;
use crate::python;

/// Extract the last valid JSON object from output that may contain log lines
fn extract_json(output: &str) -> Option<String> {
    let mut brace_count: i32 = 0;
    let mut json_start: Option<usize> = None;
    let mut last_valid_json: Option<String> = None;

    let chars: Vec<char> = output.chars().collect();

    for (i, ch) in chars.iter().enumerate() {
        match ch {
            '{' => {
                if brace_count == 0 {
                    json_start = Some(i);
                }
                brace_count += 1;
            }
            '}' => {
                if brace_count > 0 {
                    brace_count -= 1;
                    if brace_count == 0 {
                        if let Some(start) = json_start {
                            let candidate: String = chars[start..=i].iter().collect();
                            if serde_json::from_str::<serde_json::Value>(&candidate).is_ok() {
                                last_valid_json = Some(candidate);
                            }
                            json_start = None;
                        }
                    }
                }
            }
            _ => {}
        }
    }

    last_valid_json
}

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
    // Use the unified python module API
    let output = python::execute_sync(&app, "alpha_arena/main.py", vec![payload])?;

    // Find JSON in output
    extract_json(&output)
        .ok_or_else(|| "No JSON output from Python script".to_string())
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
