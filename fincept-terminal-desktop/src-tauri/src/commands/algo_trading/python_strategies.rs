//! Python strategy management

use crate::database::pool::get_db;
use serde_json::json;
use std::path::PathBuf;

use super::helpers::get_strategies_dir;

/// Parse the _registry.py file and extract strategy metadata
fn parse_strategy_registry(registry_path: &PathBuf) -> Result<Vec<serde_json::Value>, String> {
    let content = std::fs::read_to_string(registry_path)
        .map_err(|e| format!("Failed to read registry: {}", e))?;

    let mut strategies = Vec::new();

    // Find the STRATEGY_REGISTRY dict content
    let start_marker = "STRATEGY_REGISTRY = {";
    let end_marker = "}";

    if let Some(start_idx) = content.find(start_marker) {
        let dict_content = &content[start_idx + start_marker.len()..];

        // Parse each entry: "FCT-XXXXX": {"name": "...", "category": "...", "path": "..."}
        // Use regex-like manual parsing
        let mut current_pos = 0;
        let chars: Vec<char> = dict_content.chars().collect();

        while current_pos < chars.len() {
            // Find the start of an ID (quoted string starting with FCT-)
            if let Some(id_start) = dict_content[current_pos..].find("\"FCT-") {
                let id_start = current_pos + id_start + 1; // Skip opening quote

                // Find the end of the ID
                if let Some(id_end_offset) = dict_content[id_start..].find("\"") {
                    let id_end = id_start + id_end_offset;
                    let strategy_id = &dict_content[id_start..id_end];

                    // Find the opening brace for this entry's data
                    if let Some(brace_start) = dict_content[id_end..].find("{") {
                        let data_start = id_end + brace_start;

                        // Find the matching closing brace
                        if let Some(brace_end) = dict_content[data_start..].find("}") {
                            let data_end = data_start + brace_end + 1;
                            let entry_dict = &dict_content[data_start..data_end];

                            // Extract name, category, path using simple string search
                            let name = extract_field(entry_dict, "name").unwrap_or_default();
                            let category = extract_field(entry_dict, "category").unwrap_or_default();
                            let path = extract_field(entry_dict, "path").unwrap_or_default();

                            strategies.push(json!({
                                "id": strategy_id,
                                "name": name,
                                "category": category,
                                "path": path,
                            }));

                            current_pos = data_end;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
    }

    Ok(strategies)
}

/// Extract field value from dict-like string
fn extract_field(dict_str: &str, field: &str) -> Option<String> {
    let re = regex::Regex::new(&format!(r#"['"]{}['"]\s*:\s*['"]([^'"]+)['"]"#, field)).ok()?;
    re.captures(dict_str)?.get(1).map(|m| m.as_str().to_string())
}

/// Extract docstring from Python code
fn extract_docstring(code: &str) -> Option<String> {
    let re = regex::Regex::new(r#"^\s*('''|""")(.+?)('''|""")"#).ok()?;
    re.captures(code)?.get(2).map(|m| m.as_str().trim().to_string())
}

/// List all Python strategies from the library
#[tauri::command]
pub async fn list_python_strategies(
    app: tauri::AppHandle,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": true,
            "data": [],
            "count": 0
        }).to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    }).to_string())
}

/// Get strategy categories
#[tauri::command]
pub async fn get_strategy_categories(app: tauri::AppHandle) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": true,
            "categories": []
        }).to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;
    let mut categories: Vec<String> = strategies
        .iter()
        .filter_map(|s| s.get("category").and_then(|c| c.as_str()).map(String::from))
        .collect();
    categories.sort();
    categories.dedup();

    Ok(json!({
        "success": true,
        "categories": categories
    }).to_string())
}

/// Get a single Python strategy by ID
#[tauri::command]
pub async fn get_python_strategy(
    app: tauri::AppHandle,
    id: String,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "Strategy registry not found"
        }).to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;
    let strategy = strategies.iter().find(|s| {
        s.get("id").and_then(|i| i.as_str()) == Some(&id)
    });

    match strategy {
        Some(s) => Ok(json!({ "success": true, "data": s }).to_string()),
        None => Ok(json!({ "success": false, "error": "Strategy not found" }).to_string()),
    }
}

/// Get Python strategy source code
#[tauri::command]
pub async fn get_python_strategy_code(
    app: tauri::AppHandle,
    id: String,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "Strategy registry not found"
        }).to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;
    let strategy = strategies.iter().find(|s| {
        s.get("id").and_then(|i| i.as_str()) == Some(&id)
    });

    match strategy {
        Some(s) => {
            let path = s.get("path").and_then(|p| p.as_str()).unwrap_or("");
            let file_path = strategies_dir.join(path);

            if !file_path.exists() {
                return Ok(json!({
                    "success": false,
                    "error": "Strategy file not found"
                }).to_string());
            }

            let code = std::fs::read_to_string(&file_path)
                .map_err(|e| format!("Failed to read strategy file: {}", e))?;

            let docstring = extract_docstring(&code);

            Ok(json!({
                "success": true,
                "data": {
                    "code": code,
                    "docstring": docstring
                }
            }).to_string())
        }
        None => Ok(json!({ "success": false, "error": "Strategy not found" }).to_string()),
    }
}

/// Save a custom Python strategy (user-created)
#[tauri::command]
pub async fn save_custom_python_strategy(
    id: String,
    name: String,
    description: Option<String>,
    code: String,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO custom_python_strategies (id, name, description, code, updated_at)
         VALUES (?1, ?2, ?3, ?4, CURRENT_TIMESTAMP)
         ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            code = excluded.code,
            updated_at = CURRENT_TIMESTAMP",
        rusqlite::params![id, name, description.unwrap_or_default(), code],
    )
    .map_err(|e| format!("Failed to save custom strategy: {}", e))?;

    Ok(json!({
        "success": true,
        "id": id
    }).to_string())
}

/// List all custom Python strategies
#[tauri::command]
pub async fn list_custom_python_strategies() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, name, description, created_at, updated_at
             FROM custom_python_strategies
             ORDER BY updated_at DESC",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map([], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "created_at": row.get::<_, String>(3)?,
                "updated_at": row.get::<_, String>(4)?,
            }))
        })
        .map_err(|e| format!("Failed to query custom strategies: {}", e))?;

    let strategies: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    }).to_string())
}

/// Get a single custom Python strategy
#[tauri::command]
pub async fn get_custom_python_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let result = conn.query_row(
        "SELECT id, name, description, code, created_at, updated_at
         FROM custom_python_strategies WHERE id = ?1",
        rusqlite::params![id],
        |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "code": row.get::<_, String>(3)?,
                "created_at": row.get::<_, String>(4)?,
                "updated_at": row.get::<_, String>(5)?,
            }))
        },
    );

    match result {
        Ok(strategy) => Ok(json!({ "success": true, "data": strategy }).to_string()),
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Ok(json!({ "success": false, "error": "Custom strategy not found" }).to_string())
        }
        Err(e) => Ok(json!({ "success": false, "error": e.to_string() }).to_string()),
    }
}

/// Update a custom Python strategy
#[tauri::command]
pub async fn update_custom_python_strategy(
    id: String,
    name: Option<String>,
    description: Option<String>,
    code: Option<String>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // Build dynamic update query based on provided fields
    let mut updates = Vec::new();
    let mut params: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();

    if let Some(n) = name {
        updates.push("name = ?");
        params.push(Box::new(n));
    }
    if let Some(d) = description {
        updates.push("description = ?");
        params.push(Box::new(d));
    }
    if let Some(c) = code {
        updates.push("code = ?");
        params.push(Box::new(c));
    }

    if updates.is_empty() {
        return Ok(json!({
            "success": false,
            "error": "No fields to update"
        }).to_string());
    }

    updates.push("updated_at = CURRENT_TIMESTAMP");

    let query = format!(
        "UPDATE custom_python_strategies SET {} WHERE id = ?",
        updates.join(", ")
    );

    params.push(Box::new(id.clone()));

    let param_refs: Vec<&dyn rusqlite::ToSql> = params.iter().map(|p| p.as_ref()).collect();

    let changes = conn
        .execute(&query, param_refs.as_slice())
        .map_err(|e| format!("Failed to update custom strategy: {}", e))?;

    Ok(json!({
        "success": changes > 0,
        "updated": changes > 0
    }).to_string())
}

/// Delete a custom Python strategy
#[tauri::command]
pub async fn delete_custom_python_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let changes = conn
        .execute("DELETE FROM custom_python_strategies WHERE id = ?1", rusqlite::params![id])
        .map_err(|e| format!("Failed to delete custom strategy: {}", e))?;

    Ok(json!({
        "success": changes > 0,
        "deleted": changes > 0
    }).to_string())
}

/// Validate Python syntax
#[tauri::command]
pub async fn validate_python_syntax(code: String) -> Result<String, String> {
    use std::process::Command;

    let output = Command::new("python")
        .arg("-c")
        .arg(format!("import ast; ast.parse('''{}''')", code.replace("'", "\\'")))
        .output();

    match output {
        Ok(out) => {
            if out.status.success() {
                Ok(json!({ "success": true, "valid": true }).to_string())
            } else {
                let error = String::from_utf8_lossy(&out.stderr);
                Ok(json!({
                    "success": true,
                    "valid": false,
                    "error": error.to_string()
                }).to_string())
            }
        }
        Err(e) => Ok(json!({
            "success": false,
            "error": format!("Failed to run Python: {}", e)
        }).to_string()),
    }
}

/// Run a Python backtest for a strategy
#[tauri::command]
pub async fn run_python_backtest(
    app: tauri::AppHandle,
    strategy_id: String,
    symbol: String,
    start_date: String,
    end_date: String,
    initial_capital: f64,
    params: Option<String>,
) -> Result<String, String> {
    use std::process::Command;

    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "Strategy registry not found"
        }).to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;
    let strategy = strategies.iter().find(|s| {
        s.get("id").and_then(|i| i.as_str()) == Some(&strategy_id)
    });

    match strategy {
        Some(s) => {
            let path = s.get("path").and_then(|p| p.as_str()).unwrap_or("");
            let file_path = strategies_dir.join(path);

            if !file_path.exists() {
                return Ok(json!({
                    "success": false,
                    "error": "Strategy file not found"
                }).to_string());
            }

            // Run the Python backtest script
            let output = Command::new("python")
                .arg(file_path)
                .arg("--symbol")
                .arg(&symbol)
                .arg("--start")
                .arg(&start_date)
                .arg("--end")
                .arg(&end_date)
                .arg("--capital")
                .arg(initial_capital.to_string())
                .arg("--params")
                .arg(params.unwrap_or_else(|| "{}".to_string()))
                .output();

            match output {
                Ok(out) => {
                    if out.status.success() {
                        let stdout = String::from_utf8_lossy(&out.stdout);
                        Ok(json!({
                            "success": true,
                            "output": stdout.to_string()
                        }).to_string())
                    } else {
                        let stderr = String::from_utf8_lossy(&out.stderr);
                        Ok(json!({
                            "success": false,
                            "error": stderr.to_string()
                        }).to_string())
                    }
                }
                Err(e) => Ok(json!({
                    "success": false,
                    "error": format!("Failed to run backtest: {}", e)
                }).to_string()),
            }
        }
        None => Ok(json!({ "success": false, "error": "Strategy not found" }).to_string()),
    }
}

/// Deploy a Python strategy to paper or live trading
/// This is a wrapper around deploy_algo_strategy with Python-specific handling
#[tauri::command]
pub async fn deploy_python_strategy(
    app: tauri::AppHandle,
    state: tauri::State<'_, crate::WebSocketState>,
    strategy_id: String,
    symbol: String,
    mode: String,
    broker: Option<String>,
    quantity: f64,
    timeframe: String,
    parameters: Option<String>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
    max_daily_loss: Option<f64>,
) -> Result<String, String> {
    // Validate quantity
    if quantity <= 0.0 {
        return Ok(json!({
            "success": false,
            "error": "Quantity must be greater than 0"
        }).to_string());
    }

    // Build params JSON including all Python strategy specific fields
    let params_obj = json!({
        "parameters": parameters.unwrap_or_else(|| "{}".to_string()),
        "stop_loss": stop_loss,
        "take_profit": take_profit,
        "max_daily_loss": max_daily_loss,
    });

    // Delegate to the main deploy function
    super::deployment::deploy_algo_strategy(
        app,
        state,
        strategy_id,
        symbol,
        broker,
        Some(mode),
        Some(timeframe),
        Some(quantity),
        Some(params_obj.to_string()),
    ).await
}

/// Extract strategy parameters from Python code
#[tauri::command]
pub async fn extract_strategy_parameters(code: String) -> Result<String, String> {
    // Parse Python code to extract parameter definitions
    // Look for patterns like: PARAM_NAME = 14  or  self.period = params.get('period', 14)

    let mut parameters = Vec::new();

    // Simple regex-based parameter extraction
    if let Ok(re) = regex::Regex::new(r"(?m)^\s*([A-Z_]+)\s*=\s*([0-9.]+)") {
        for cap in re.captures_iter(&code) {
            if let (Some(name), Some(value)) = (cap.get(1), cap.get(2)) {
                parameters.push(json!({
                    "name": name.as_str(),
                    "default": value.as_str(),
                    "type": if value.as_str().contains('.') { "float" } else { "int" }
                }));
            }
        }
    }

    Ok(json!({
        "success": true,
        "parameters": parameters
    }).to_string())
}
