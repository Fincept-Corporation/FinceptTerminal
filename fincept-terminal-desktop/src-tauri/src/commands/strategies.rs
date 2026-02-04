use serde_json::json;
use std::fs;
use std::path::PathBuf;
use tauri::Manager;

/// Get the strategies base directory
fn get_strategies_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let strategies_dir = resource_dir.join("resources").join("scripts").join("strategies");

    if strategies_dir.exists() {
        return Ok(strategies_dir);
    }

    // Fallback for development mode
    let dev_dir = std::env::current_dir()
        .map_err(|e| format!("Failed to get current dir: {}", e))?
        .join("src-tauri")
        .join("resources")
        .join("scripts")
        .join("strategies");

    if dev_dir.exists() {
        return Ok(dev_dir);
    }

    Err("Strategies directory not found".to_string())
}

/// List all strategies from the _registry.py file
#[tauri::command]
pub async fn list_strategies(app: tauri::AppHandle) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "Strategy registry not found"
        }).to_string());
    }

    let content = fs::read_to_string(&registry_path)
        .map_err(|e| format!("Failed to read registry: {}", e))?;

    let mut strategies = Vec::new();

    for line in content.lines() {
        let trimmed = line.trim();
        if !trimmed.starts_with("\"FCT-") {
            continue;
        }

        // Parse: "FCT-XXXX": {"name": "...", "category": "...", "path": "..."},
        if let Some(id_end) = trimmed.find("\":") {
            let id = trimmed[1..id_end].to_string();

            let name = extract_field(trimmed, "name");
            let category = extract_field(trimmed, "category");
            let path = extract_field(trimmed, "path");

            if !name.is_empty() {
                strategies.push(json!({
                    "id": id,
                    "name": name,
                    "category": category,
                    "path": path,
                }));
            }
        }
    }

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    }).to_string())
}

/// Read source code of a specific strategy file
#[tauri::command]
pub async fn read_strategy_source(app: tauri::AppHandle, path: String) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let file_path = strategies_dir.join(&path);

    // Security: ensure path doesn't escape strategies dir
    let canonical = file_path.canonicalize()
        .map_err(|e| format!("Invalid path: {}", e))?;
    let canonical_base = strategies_dir.canonicalize()
        .map_err(|e| format!("Invalid base path: {}", e))?;

    if !canonical.starts_with(&canonical_base) {
        return Ok(json!({
            "success": false,
            "error": "Access denied: path outside strategies directory"
        }).to_string());
    }

    if !file_path.exists() {
        return Ok(json!({
            "success": false,
            "error": format!("Strategy file not found: {}", path)
        }).to_string());
    }

    let source = fs::read_to_string(&file_path)
        .map_err(|e| format!("Failed to read file: {}", e))?;

    Ok(json!({
        "success": true,
        "data": source
    }).to_string())
}

/// Extract a JSON field value from a registry line
fn extract_field(line: &str, field: &str) -> String {
    let pattern = format!("\"{}\":", field);
    if let Some(pos) = line.find(&pattern) {
        let after = &line[pos + pattern.len()..];
        let after = after.trim();
        if after.starts_with('\"') {
            let inner = &after[1..];
            if let Some(end) = inner.find('\"') {
                return inner[..end].to_string();
            }
        }
    }
    String::new()
}

/// Execute a Fincept strategy with backtesting parameters
#[tauri::command]
pub async fn execute_fincept_strategy(
    app: tauri::AppHandle,
    strategy_id: String,
    params: String,
) -> Result<String, String> {
    use std::process::Command;

    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let runner_path = resource_dir
        .join("resources")
        .join("scripts")
        .join("Analytics")
        .join("backtesting")
        .join("base")
        .join("fincept_strategy_runner.py");

    // Fallback for development
    let runner_path = if runner_path.exists() {
        runner_path
    } else {
        std::env::current_dir()
            .map_err(|e| format!("Failed to get current dir: {}", e))?
            .join("src-tauri")
            .join("resources")
            .join("scripts")
            .join("Analytics")
            .join("backtesting")
            .join("base")
            .join("fincept_strategy_runner.py")
    };

    if !runner_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "Strategy runner not found"
        }).to_string());
    }

    // Execute Python runner
    let output = Command::new("python")
        .arg(runner_path)
        .arg("run")
        .arg(&strategy_id)
        .arg(&params)
        .output()
        .map_err(|e| format!("Failed to execute strategy: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Ok(json!({
            "success": false,
            "error": format!("Strategy execution failed: {}", stderr)
        }).to_string());
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(stdout.to_string())
}

/// Get the path to the live strategy runner
fn get_live_runner_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let runner_path = resource_dir
        .join("resources")
        .join("scripts")
        .join("strategies")
        .join("live_runner.py");

    if runner_path.exists() {
        return Ok(runner_path);
    }

    // Fallback for development
    let dev_path = std::env::current_dir()
        .map_err(|e| format!("Failed to get current dir: {}", e))?
        .join("src-tauri")
        .join("resources")
        .join("scripts")
        .join("strategies")
        .join("live_runner.py");

    if dev_path.exists() {
        return Ok(dev_path);
    }

    Err("Live strategy runner not found".to_string())
}

/// Get the SQLite database path for deployed strategies
fn get_deploy_db_path(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let app_data = app
        .path()
        .app_data_dir()
        .map_err(|e| format!("Failed to get app data dir: {}", e))?;

    fs::create_dir_all(&app_data)
        .map_err(|e| format!("Failed to create app data dir: {}", e))?;

    Ok(app_data.join("deployed_strategies.db"))
}

/// Get the main application database path (contains strategy_price_cache from WebSocket ticks)
fn get_main_db_path() -> Result<PathBuf, String> {
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .map_err(|_| "APPDATA not set".to_string())?;
        PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .map_err(|_| "HOME not set".to_string())?;
        PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .map_err(|_| "HOME not set".to_string())?;
        let xdg = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        PathBuf::from(xdg).join("fincept-terminal")
    };
    Ok(db_dir.join("fincept_terminal.db"))
}

/// Deploy a strategy for live/paper trading (SQLite-only, no in-memory state)
#[tauri::command]
pub async fn deploy_strategy(
    app: tauri::AppHandle,
    params: String,
) -> Result<String, String> {
    use std::process::Command;

    let parsed: serde_json::Value = serde_json::from_str(&params)
        .map_err(|e| format!("Invalid params JSON: {}", e))?;

    let strategy_id = parsed["strategy_id"].as_str().unwrap_or("").to_string();
    let symbol = parsed["symbol"].as_str().unwrap_or("").to_string();
    let asset_type = parsed["asset_type"].as_str().unwrap_or("equity").to_string();

    if strategy_id.is_empty() || symbol.is_empty() {
        return Ok(json!({
            "success": false,
            "error": "strategy_id and symbol are required"
        }).to_string());
    }

    let runner_path = get_live_runner_path(&app)?;
    let db_path = get_deploy_db_path(&app)?;
    let db_path_str = db_path.to_string_lossy().to_string();
    let strategies_dir = get_strategies_dir(&app)?;
    let main_db_path = get_main_db_path().unwrap_or_default();
    let main_db_str = main_db_path.to_string_lossy().to_string();

    // Duplicate check: query SQLite for same strategy+symbol already running/waiting
    let dup_check = Command::new("python")
        .arg(&runner_path)
        .arg("check_duplicate")
        .arg(&strategy_id)
        .arg(&symbol)
        .arg("--db")
        .arg(&db_path_str)
        .output();

    if let Ok(output) = dup_check {
        if output.status.success() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            if let Ok(parsed_dup) = serde_json::from_str::<serde_json::Value>(&stdout) {
                if parsed_dup["is_duplicate"].as_bool().unwrap_or(false) {
                    return Ok(json!({
                        "success": false,
                        "error": format!("Strategy {} is already deployed on {}", strategy_id, symbol)
                    }).to_string());
                }
            }
        }
    }

    // Generate unique deploy ID
    let deploy_id = format!("deploy-{}-{}", strategy_id, chrono::Utc::now().timestamp_millis());

    // Spawn the Python live runner as a background process
    // Pass --app-db so runner can read WebSocket price cache from main app DB
    let child = Command::new("python")
        .arg(&runner_path)
        .arg("deploy")
        .arg(&deploy_id)
        .arg(&strategy_id)
        .arg(&params)
        .arg("--db")
        .arg(&db_path_str)
        .arg("--strategies-dir")
        .arg(strategies_dir.to_string_lossy().to_string())
        .arg("--app-db")
        .arg(&main_db_str)
        .spawn()
        .map_err(|e| format!("Failed to spawn strategy process: {}", e))?;

    let pid = child.id();

    Ok(json!({
        "success": true,
        "deploy_id": deploy_id,
        "pid": pid
    }).to_string())
}

/// Stop a deployed strategy (reads PID from SQLite, kills process, updates DB)
#[tauri::command]
pub async fn stop_strategy(
    app: tauri::AppHandle,
    deploy_id: String,
) -> Result<String, String> {
    use std::process::Command;

    let runner_path = get_live_runner_path(&app)?;
    let db_path = get_deploy_db_path(&app)?;

    // Stop via Python runner (it reads PID from DB, kills process, updates status)
    let output = Command::new("python")
        .arg(&runner_path)
        .arg("stop")
        .arg(&deploy_id)
        .arg("--db")
        .arg(db_path.to_string_lossy().to_string())
        .output()
        .map_err(|e| format!("Failed to stop strategy: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        Ok(json!({
            "success": false,
            "error": "Failed to stop strategy"
        }).to_string())
    }
}

/// Stop all deployed strategies for a given asset type
#[tauri::command]
pub async fn stop_all_strategies(
    app: tauri::AppHandle,
    asset_type: String,
) -> Result<String, String> {
    use std::process::Command;

    let runner_path = get_live_runner_path(&app)?;
    let db_path = get_deploy_db_path(&app)?;

    let output = Command::new("python")
        .arg(&runner_path)
        .arg("stop_all")
        .arg(&asset_type)
        .arg("--db")
        .arg(db_path.to_string_lossy().to_string())
        .output()
        .map_err(|e| format!("Failed to stop all strategies: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        Ok(json!({
            "success": false,
            "error": "Failed to stop all strategies"
        }).to_string())
    }
}

/// List all deployed strategies (reads from SQLite only)
#[tauri::command]
pub async fn list_deployed_strategies(
    app: tauri::AppHandle,
    asset_type: String,
) -> Result<String, String> {
    use std::process::Command;

    let runner_path = get_live_runner_path(&app)?;
    let db_path = get_deploy_db_path(&app)?;

    let output = Command::new("python")
        .arg(&runner_path)
        .arg("list")
        .arg(&asset_type)
        .arg("--db")
        .arg(db_path.to_string_lossy().to_string())
        .output()
        .map_err(|e| format!("Failed to list strategies: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        Ok(json!({
            "success": true,
            "data": [],
            "source": "sqlite"
        }).to_string())
    }
}

/// Update parameters of a running strategy (writes to SQLite, runner picks up changes)
#[tauri::command]
pub async fn update_strategy(
    app: tauri::AppHandle,
    deploy_id: String,
    params: String,
) -> Result<String, String> {
    use std::process::Command;

    let runner_path = get_live_runner_path(&app)?;
    let db_path = get_deploy_db_path(&app)?;

    let output = Command::new("python")
        .arg(&runner_path)
        .arg("update")
        .arg(&deploy_id)
        .arg(&params)
        .arg("--db")
        .arg(db_path.to_string_lossy().to_string())
        .output()
        .map_err(|e| format!("Failed to update strategy: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        Ok(stdout.to_string())
    } else {
        Ok(json!({
            "success": false,
            "error": "Failed to update strategy parameters"
        }).to_string())
    }
}
