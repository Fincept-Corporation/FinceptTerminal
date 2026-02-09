//! Algo Trading Commands
//!
//! Tauri commands for the visual algo trading system:
//! - Strategy CRUD (save, list, get, delete)
//! - Deployment lifecycle (deploy, stop, stop-all, list)
//! - Trade history
//! - Scanner execution
//! - Candle cache reads
//! - One-shot condition evaluation (preview)
//! - Candle aggregation control

use crate::database::pool::get_db;
use serde_json::json;
use std::path::PathBuf;
use tauri::{Emitter, Manager};

// ============================================================================
// HELPERS
// ============================================================================

/// Get the algo trading scripts directory
fn get_algo_scripts_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let scripts_dir = resource_dir
        .join("resources")
        .join("scripts")
        .join("algo_trading");

    if scripts_dir.exists() {
        return Ok(scripts_dir);
    }

    // Fallback for development mode
    let dev_dir = std::env::current_dir()
        .map_err(|e| format!("Failed to get current dir: {}", e))?
        .join("src-tauri")
        .join("resources")
        .join("scripts")
        .join("algo_trading");

    if dev_dir.exists() {
        return Ok(dev_dir);
    }

    Err("Algo trading scripts directory not found".to_string())
}

/// Get the main database path for passing to Python subprocesses
fn get_main_db_path_str() -> Result<String, String> {
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
    Ok(db_dir.join("fincept_terminal.db").to_string_lossy().to_string())
}

// ============================================================================
// STRATEGY CRUD
// ============================================================================

/// Save or update an algo strategy (condition-based)
#[tauri::command]
pub async fn save_algo_strategy(
    id: String,
    name: String,
    description: Option<String>,
    entry_conditions: String,
    exit_conditions: String,
    entry_logic: Option<String>,
    exit_logic: Option<String>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
    trailing_stop: Option<f64>,
    trailing_stop_type: Option<String>,
    timeframe: Option<String>,
    symbols: Option<String>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO algo_strategies (id, name, description, entry_conditions, exit_conditions,
         entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, trailing_stop_type,
         timeframe, symbols, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, CURRENT_TIMESTAMP)
         ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            entry_conditions = excluded.entry_conditions,
            exit_conditions = excluded.exit_conditions,
            entry_logic = excluded.entry_logic,
            exit_logic = excluded.exit_logic,
            stop_loss = excluded.stop_loss,
            take_profit = excluded.take_profit,
            trailing_stop = excluded.trailing_stop,
            trailing_stop_type = excluded.trailing_stop_type,
            timeframe = excluded.timeframe,
            symbols = excluded.symbols,
            updated_at = CURRENT_TIMESTAMP",
        rusqlite::params![
            id,
            name,
            description.unwrap_or_default(),
            entry_conditions,
            exit_conditions,
            entry_logic.unwrap_or_else(|| "AND".to_string()),
            exit_logic.unwrap_or_else(|| "AND".to_string()),
            stop_loss,
            take_profit,
            trailing_stop,
            trailing_stop_type.unwrap_or_else(|| "percent".to_string()),
            timeframe.unwrap_or_else(|| "5m".to_string()),
            symbols.unwrap_or_else(|| "[]".to_string()),
        ],
    )
    .map_err(|e| format!("Failed to save strategy: {}", e))?;

    Ok(json!({
        "success": true,
        "id": id
    }).to_string())
}

/// List all algo strategies
#[tauri::command]
pub async fn list_algo_strategies() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, name, description, entry_conditions, exit_conditions,
                    entry_logic, exit_logic, stop_loss, take_profit, trailing_stop,
                    trailing_stop_type, timeframe, symbols, is_active, created_at, updated_at
             FROM algo_strategies ORDER BY updated_at DESC",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map([], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "entry_conditions": row.get::<_, String>(3)?,
                "exit_conditions": row.get::<_, String>(4)?,
                "entry_logic": row.get::<_, String>(5)?,
                "exit_logic": row.get::<_, String>(6)?,
                "stop_loss": row.get::<_, Option<f64>>(7)?,
                "take_profit": row.get::<_, Option<f64>>(8)?,
                "trailing_stop": row.get::<_, Option<f64>>(9)?,
                "trailing_stop_type": row.get::<_, String>(10)?,
                "timeframe": row.get::<_, String>(11)?,
                "symbols": row.get::<_, String>(12)?,
                "is_active": row.get::<_, i32>(13)?,
                "created_at": row.get::<_, String>(14)?,
                "updated_at": row.get::<_, String>(15)?,
            }))
        })
        .map_err(|e| format!("Failed to query strategies: {}", e))?;

    let strategies: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    }).to_string())
}

/// Get a single algo strategy by ID
#[tauri::command]
pub async fn get_algo_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let result = conn.query_row(
        "SELECT id, name, description, entry_conditions, exit_conditions,
                entry_logic, exit_logic, stop_loss, take_profit, trailing_stop,
                trailing_stop_type, timeframe, symbols, is_active, created_at, updated_at
         FROM algo_strategies WHERE id = ?1",
        rusqlite::params![id],
        |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "entry_conditions": row.get::<_, String>(3)?,
                "exit_conditions": row.get::<_, String>(4)?,
                "entry_logic": row.get::<_, String>(5)?,
                "exit_logic": row.get::<_, String>(6)?,
                "stop_loss": row.get::<_, Option<f64>>(7)?,
                "take_profit": row.get::<_, Option<f64>>(8)?,
                "trailing_stop": row.get::<_, Option<f64>>(9)?,
                "trailing_stop_type": row.get::<_, String>(10)?,
                "timeframe": row.get::<_, String>(11)?,
                "symbols": row.get::<_, String>(12)?,
                "is_active": row.get::<_, i32>(13)?,
                "created_at": row.get::<_, String>(14)?,
                "updated_at": row.get::<_, String>(15)?,
            }))
        },
    );

    match result {
        Ok(strategy) => Ok(json!({ "success": true, "data": strategy }).to_string()),
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Ok(json!({ "success": false, "error": "Strategy not found" }).to_string())
        }
        Err(e) => Ok(json!({ "success": false, "error": e.to_string() }).to_string()),
    }
}

/// Delete an algo strategy
#[tauri::command]
pub async fn delete_algo_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let changes = conn
        .execute("DELETE FROM algo_strategies WHERE id = ?1", rusqlite::params![id])
        .map_err(|e| format!("Failed to delete strategy: {}", e))?;

    Ok(json!({
        "success": changes > 0,
        "deleted": changes > 0
    }).to_string())
}

// ============================================================================
// DEPLOYMENT LIFECYCLE
// ============================================================================

/// Deploy an algo strategy (spawn Python algo_live_runner.py as background process)
#[tauri::command]
pub async fn deploy_algo_strategy(
    app: tauri::AppHandle,
    state: tauri::State<'_, crate::WebSocketState>,
    strategy_id: String,
    symbol: String,
    provider: Option<String>,
    mode: Option<String>,
    timeframe: Option<String>,
    quantity: Option<f64>,
    params: Option<String>,
) -> Result<String, String> {
    use std::process::Command;

    let deploy_mode = mode.unwrap_or_else(|| "paper".to_string());
    let deploy_timeframe = timeframe.unwrap_or_else(|| "5m".to_string());
    let deploy_qty = quantity.unwrap_or(1.0);
    let deploy_provider = provider.unwrap_or_default();
    let deploy_params = params.unwrap_or_else(|| "{}".to_string());

    // Generate deployment ID
    let deploy_id = format!("algo-{}", uuid::Uuid::new_v4());

    // Insert deployment record
    let conn = get_db().map_err(|e| e.to_string())?;
    conn.execute(
        "INSERT INTO algo_deployments (id, strategy_id, symbol, provider, mode, status, timeframe, quantity, params)
         VALUES (?1, ?2, ?3, ?4, ?5, 'starting', ?6, ?7, ?8)",
        rusqlite::params![deploy_id, strategy_id, symbol, deploy_provider, deploy_mode, deploy_timeframe, deploy_qty, deploy_params],
    )
    .map_err(|e| format!("Failed to create deployment: {}", e))?;

    // Insert initial metrics row
    conn.execute(
        "INSERT OR IGNORE INTO algo_metrics (deployment_id) VALUES (?1)",
        rusqlite::params![deploy_id],
    )
    .map_err(|e| format!("Failed to create metrics: {}", e))?;

    // Get paths
    let scripts_dir = get_algo_scripts_dir(&app)?;
    let runner_path = scripts_dir.join("algo_live_runner.py");
    let db_path = get_main_db_path_str()?;

    if !runner_path.exists() {
        // Update deployment status to error
        let _ = conn.execute(
            "UPDATE algo_deployments SET status = 'error', error_message = 'Runner script not found' WHERE id = ?1",
            rusqlite::params![deploy_id],
        );
        return Ok(json!({
            "success": false,
            "error": "algo_live_runner.py not found"
        }).to_string());
    }

    // Spawn the Python runner as a background process
    let child = Command::new("python")
        .arg(&runner_path)
        .arg("--deploy-id")
        .arg(&deploy_id)
        .arg("--strategy-id")
        .arg(&strategy_id)
        .arg("--symbol")
        .arg(&symbol)
        .arg("--provider")
        .arg(&deploy_provider)
        .arg("--mode")
        .arg(&deploy_mode)
        .arg("--timeframe")
        .arg(&deploy_timeframe)
        .arg("--quantity")
        .arg(deploy_qty.to_string())
        .arg("--db")
        .arg(&db_path)
        .spawn();

    match child {
        Ok(child) => {
            let pid = child.id();

            // Update deployment with PID and running status
            let _ = conn.execute(
                "UPDATE algo_deployments SET status = 'running', pid = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
                rusqlite::params![pid as i64, deploy_id],
            );

            // Auto-start candle aggregation for this symbol+timeframe
            let services = state.services.read().await;
            services
                .candle_aggregator
                .add_subscription(symbol.clone(), deploy_timeframe.clone())
                .await;

            Ok(json!({
                "success": true,
                "deploy_id": deploy_id,
                "pid": pid
            }).to_string())
        }
        Err(e) => {
            let _ = conn.execute(
                "UPDATE algo_deployments SET status = 'error', error_message = ?1 WHERE id = ?2",
                rusqlite::params![format!("Failed to spawn: {}", e), deploy_id],
            );
            Ok(json!({
                "success": false,
                "error": format!("Failed to spawn runner: {}", e)
            }).to_string())
        }
    }
}

/// Stop a running algo deployment (kill process, update DB)
#[tauri::command]
pub async fn stop_algo_deployment(deploy_id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // Get PID from deployment record
    let pid_result: Result<Option<i64>, _> = conn.query_row(
        "SELECT pid FROM algo_deployments WHERE id = ?1 AND status = 'running'",
        rusqlite::params![deploy_id],
        |row| row.get(0),
    );

    match pid_result {
        Ok(Some(pid)) => {
            // Kill the process
            #[cfg(target_os = "windows")]
            {
                let _ = std::process::Command::new("taskkill")
                    .args(["/F", "/PID", &pid.to_string()])
                    .output();
            }
            #[cfg(not(target_os = "windows"))]
            {
                let _ = std::process::Command::new("kill")
                    .arg(pid.to_string())
                    .output();
            }

            // Update status
            conn.execute(
                "UPDATE algo_deployments SET status = 'stopped', stopped_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
                rusqlite::params![deploy_id],
            )
            .map_err(|e| format!("Failed to update deployment: {}", e))?;

            Ok(json!({ "success": true, "stopped": true }).to_string())
        }
        Ok(None) => {
            // No PID but try to update status anyway
            let _ = conn.execute(
                "UPDATE algo_deployments SET status = 'stopped', stopped_at = CURRENT_TIMESTAMP WHERE id = ?1",
                rusqlite::params![deploy_id],
            );
            Ok(json!({ "success": true, "stopped": true, "note": "No PID found" }).to_string())
        }
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Ok(json!({ "success": false, "error": "Deployment not found or not running" }).to_string())
        }
        Err(e) => {
            Ok(json!({ "success": false, "error": e.to_string() }).to_string())
        }
    }
}

/// Emergency stop all running algo deployments
#[tauri::command]
pub async fn stop_all_algo_deployments() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // Get all running PIDs
    let mut stmt = conn
        .prepare("SELECT id, pid FROM algo_deployments WHERE status = 'running' AND pid IS NOT NULL")
        .map_err(|e| format!("Failed to query: {}", e))?;

    let deployments: Vec<(String, i64)> = stmt
        .query_map([], |row| {
            Ok((row.get::<_, String>(0)?, row.get::<_, i64>(1)?))
        })
        .map_err(|e| format!("Failed to read deployments: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    let mut stopped = 0;
    for (id, pid) in &deployments {
        #[cfg(target_os = "windows")]
        {
            let _ = std::process::Command::new("taskkill")
                .args(["/F", "/PID", &pid.to_string()])
                .output();
        }
        #[cfg(not(target_os = "windows"))]
        {
            let _ = std::process::Command::new("kill")
                .arg(pid.to_string())
                .output();
        }

        let _ = conn.execute(
            "UPDATE algo_deployments SET status = 'stopped', stopped_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
            rusqlite::params![id],
        );
        stopped += 1;
    }

    Ok(json!({
        "success": true,
        "stopped": stopped
    }).to_string())
}

/// List all algo deployments with their metrics
#[tauri::command]
pub async fn list_algo_deployments() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT d.id, d.strategy_id, d.symbol, d.provider, d.mode, d.status,
                    d.timeframe, d.quantity, d.params, d.pid, d.error_message,
                    d.created_at, d.updated_at, d.stopped_at,
                    m.total_pnl, m.unrealized_pnl, m.total_trades, m.win_rate,
                    m.max_drawdown, m.current_position_qty, m.current_position_side,
                    m.current_position_entry, m.updated_at as metrics_updated,
                    s.name as strategy_name
             FROM algo_deployments d
             LEFT JOIN algo_metrics m ON d.id = m.deployment_id
             LEFT JOIN algo_strategies s ON d.strategy_id = s.id
             ORDER BY d.created_at DESC",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map([], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "strategy_id": row.get::<_, String>(1)?,
                "symbol": row.get::<_, String>(2)?,
                "provider": row.get::<_, String>(3)?,
                "mode": row.get::<_, String>(4)?,
                "status": row.get::<_, String>(5)?,
                "timeframe": row.get::<_, String>(6)?,
                "quantity": row.get::<_, f64>(7)?,
                "params": row.get::<_, String>(8)?,
                "pid": row.get::<_, Option<i64>>(9)?,
                "error_message": row.get::<_, Option<String>>(10)?,
                "created_at": row.get::<_, String>(11)?,
                "updated_at": row.get::<_, String>(12)?,
                "stopped_at": row.get::<_, Option<String>>(13)?,
                "metrics": {
                    "total_pnl": row.get::<_, Option<f64>>(14)?.unwrap_or(0.0),
                    "unrealized_pnl": row.get::<_, Option<f64>>(15)?.unwrap_or(0.0),
                    "total_trades": row.get::<_, Option<i32>>(16)?.unwrap_or(0),
                    "win_rate": row.get::<_, Option<f64>>(17)?.unwrap_or(0.0),
                    "max_drawdown": row.get::<_, Option<f64>>(18)?.unwrap_or(0.0),
                    "current_position_qty": row.get::<_, Option<f64>>(19)?.unwrap_or(0.0),
                    "current_position_side": row.get::<_, Option<String>>(20)?.unwrap_or_default(),
                    "current_position_entry": row.get::<_, Option<f64>>(21)?.unwrap_or(0.0),
                    "metrics_updated": row.get::<_, Option<String>>(22)?,
                },
                "strategy_name": row.get::<_, Option<String>>(23)?,
            }))
        })
        .map_err(|e| format!("Failed to query deployments: {}", e))?;

    let deployments: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": deployments,
        "count": deployments.len()
    }).to_string())
}

// ============================================================================
// TRADE HISTORY
// ============================================================================

/// Get trade history for a deployment
#[tauri::command]
pub async fn get_algo_trades(
    deployment_id: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;
    let row_limit = limit.unwrap_or(100);

    let mut stmt = conn
        .prepare(
            "SELECT id, deployment_id, symbol, side, quantity, price, pnl, timestamp, signal_reason
             FROM algo_trades WHERE deployment_id = ?1
             ORDER BY timestamp DESC LIMIT ?2",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map(rusqlite::params![deployment_id, row_limit], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "deployment_id": row.get::<_, String>(1)?,
                "symbol": row.get::<_, String>(2)?,
                "side": row.get::<_, String>(3)?,
                "quantity": row.get::<_, f64>(4)?,
                "price": row.get::<_, f64>(5)?,
                "pnl": row.get::<_, f64>(6)?,
                "timestamp": row.get::<_, String>(7)?,
                "signal_reason": row.get::<_, String>(8)?,
            }))
        })
        .map_err(|e| format!("Failed to query trades: {}", e))?;

    let trades: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": trades,
        "count": trades.len()
    }).to_string())
}

// ============================================================================
// SCANNER
// ============================================================================

/// Run algo scanner across a list of symbols.
/// For daily+ timeframes, pre-fetches historical data from broker into candle_cache
/// so the scanner has data to evaluate conditions against.
#[tauri::command]
pub async fn run_algo_scan(
    app: tauri::AppHandle,
    conditions: String,
    symbols: String,
    timeframe: Option<String>,
    provider: Option<String>,
) -> Result<String, String> {
    use std::process::Command;

    let scripts_dir = get_algo_scripts_dir(&app)?;
    let scanner_path = scripts_dir.join("scanner_engine.py");
    let db_path = get_main_db_path_str()?;
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());
    let broker = provider.as_deref().unwrap_or("fyers").to_string();

    // Collect debug info throughout the scan process
    let mut debug_log: Vec<String> = Vec::new();
    debug_log.push(format!("[scan] broker={}, timeframe={}, db={}", broker, tf, db_path));

    if !scanner_path.exists() {
        debug_log.push(format!("[scan] ERROR: scanner_engine.py not found at {:?}", scanner_path));
        return Ok(json!({
            "success": false,
            "error": "scanner_engine.py not found",
            "debug": debug_log
        }).to_string());
    }
    debug_log.push(format!("[scan] scanner_path={:?}", scanner_path));

    // Parse symbol list
    let symbol_list: Vec<String> = serde_json::from_str(&symbols).unwrap_or_default();
    debug_log.push(format!("[scan] symbols parsed: {} symbols", symbol_list.len()));

    // Pre-fetch historical data from broker into candle_cache for each symbol
    let mut prefetch_warning: Option<String> = None;
    if !symbol_list.is_empty() {
        debug_log.push(format!("[prefetch] starting for broker={}", broker));
        match prefetch_historical_candles(&db_path, &symbol_list, &tf, &broker).await {
            Ok(prefetch_debug) => {
                debug_log.extend(prefetch_debug);
                debug_log.push("[prefetch] completed successfully".to_string());
            }
            Err(e) => {
                let err_msg = format!("Data prefetch failed: {}", e);
                eprintln!("[run_algo_scan] {}", err_msg);
                debug_log.push(format!("[prefetch] ERROR: {}", e));
                prefetch_warning = Some(err_msg);
            }
        }
    }

    // Check candle_cache has data before running scanner
    let candle_count = check_candle_cache_count(&db_path, &symbol_list, &tf);
    debug_log.push(format!("[scan] candle_cache total rows for these symbols: {}", candle_count));

    if candle_count == 0 {
        // No data at all — return a clear error instead of running Python for nothing
        let error_msg = if let Some(ref pw) = prefetch_warning {
            format!("No candle data available. {}", pw)
        } else {
            format!("No candle data in cache for the given symbols and timeframe '{}'. Ensure broker is authenticated and symbols are valid.", tf)
        };
        return Ok(json!({
            "success": false,
            "error": error_msg,
            "debug": debug_log
        }).to_string());
    }

    // Run the Python scanner
    debug_log.push("[scan] launching scanner_engine.py".to_string());
    let output = Command::new("python")
        .arg(&scanner_path)
        .arg("--conditions")
        .arg(&conditions)
        .arg("--symbols")
        .arg(&symbols)
        .arg("--timeframe")
        .arg(&tf)
        .arg("--db")
        .arg(&db_path)
        .output()
        .map_err(|e| format!("Failed to run scanner: {}", e))?;

    let stderr_str = String::from_utf8_lossy(&output.stderr).to_string();
    if !stderr_str.trim().is_empty() {
        debug_log.push(format!("[scanner.py stderr] {}", stderr_str.trim()));
    }

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        debug_log.push(format!("[scan] scanner returned {} bytes", stdout.len()));

        if let Ok(mut parsed) = serde_json::from_str::<serde_json::Value>(&stdout) {
            // Merge scanner_debug from Python into our debug log
            if let Some(scanner_debug) = parsed.get("scanner_debug").and_then(|v| v.as_array()) {
                for entry in scanner_debug {
                    if let Some(s) = entry.as_str() {
                        debug_log.push(s.to_string());
                    }
                }
            }
            // Inject debug log and prefetch warning into the response
            if let Some(obj) = parsed.as_object_mut() {
                obj.remove("scanner_debug"); // remove raw Python debug, we merged it
                obj.insert("debug".to_string(), json!(debug_log));
                if let Some(pw) = prefetch_warning {
                    obj.insert("prefetch_warning".to_string(), json!(pw));
                }
            }
            Ok(parsed.to_string())
        } else {
            debug_log.push(format!("[scan] WARNING: could not parse scanner output as JSON"));
            Ok(json!({
                "success": true,
                "raw": stdout.to_string(),
                "debug": debug_log
            }).to_string())
        }
    } else {
        debug_log.push(format!("[scan] scanner process exited with error"));
        Ok(json!({
            "success": false,
            "error": format!("Scanner failed: {}", stderr_str.trim()),
            "debug": debug_log
        }).to_string())
    }
}

/// Quick helper to count candle_cache rows for the given symbols+timeframe
fn check_candle_cache_count(db_path: &str, symbols: &[String], timeframe: &str) -> i64 {
    let conn = match rusqlite::Connection::open(db_path) {
        Ok(c) => c,
        Err(_) => return 0,
    };
    let mut total: i64 = 0;
    for symbol in symbols {
        let count: i64 = conn
            .query_row(
                "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2",
                rusqlite::params![symbol, timeframe],
                |row| row.get(0),
            )
            .unwrap_or(0);
        total += count;
    }
    total
}

/// Pre-fetch historical candles from broker API and insert into candle_cache table.
/// Maps timeframe strings to Fyers resolution values and fetches ~1 year of daily data
/// or appropriate ranges for other timeframes.
/// Returns a Vec of debug log lines for the caller to include in the response.
async fn prefetch_historical_candles(
    db_path: &str,
    symbols: &[String],
    timeframe: &str,
    broker: &str,
) -> Result<Vec<String>, String> {
    use crate::commands::brokers;

    let mut debug_log: Vec<String> = Vec::new();

    // Only support Fyers for now
    if broker != "fyers" {
        debug_log.push(format!("[prefetch] broker '{}' not supported for prefetch, only 'fyers' is supported", broker));
        return Ok(debug_log);
    }

    // Get Fyers credentials
    debug_log.push("[prefetch] fetching Fyers credentials...".to_string());
    let creds = brokers::get_indian_broker_credentials("fyers".to_string())
        .await
        .map_err(|e| format!("Failed to get Fyers credentials: {}", e))?;

    if !creds.success || creds.data.is_none() {
        return Err("Fyers credentials not found. Please authenticate with Fyers first (Settings > Brokers > Fyers).".to_string());
    }

    let cred_data = creds.data.unwrap();
    let api_key = cred_data.get("apiKey")
        .and_then(|v| v.as_str())
        .ok_or("Fyers API key not found in stored credentials")?
        .to_string();
    let access_token = cred_data.get("accessToken")
        .and_then(|v| v.as_str())
        .ok_or("Fyers access token not found. Please re-authenticate with Fyers — tokens expire daily.")?
        .to_string();

    if api_key.is_empty() {
        return Err("Fyers API key is empty. Please re-enter credentials in Settings > Brokers.".to_string());
    }
    if access_token.is_empty() {
        return Err("Fyers access token is empty. Please re-authenticate — Fyers tokens expire daily.".to_string());
    }

    debug_log.push(format!("[prefetch] credentials OK (api_key len={}, token len={})", api_key.len(), access_token.len()));

    // Map timeframe to Fyers resolution and lookback days
    let (resolution, lookback_days) = match timeframe {
        "1m" => ("1", 5),
        "3m" => ("3", 10),
        "5m" => ("5", 15),
        "10m" => ("10", 20),
        "15m" => ("15", 30),
        "30m" => ("30", 60),
        "1h" => ("60", 90),
        "4h" => ("240", 180),
        "1d" | "1D" | "D" => ("1D", 365),
        _ => ("1D", 365),
    };

    let now = chrono::Utc::now();
    let from_date = (now - chrono::Duration::days(lookback_days)).format("%Y-%m-%d").to_string();
    let to_date = now.format("%Y-%m-%d").to_string();

    debug_log.push(format!("[prefetch] resolution={}, lookback={}d, range={} to {}", resolution, lookback_days, from_date, to_date));

    // Open DB connection for inserting candles
    let conn = rusqlite::Connection::open(db_path)
        .map_err(|e| format!("Failed to open DB at {}: {}", db_path, e))?;

    // Ensure candle_cache table exists
    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS candle_cache (
            symbol TEXT NOT NULL,
            provider TEXT NOT NULL DEFAULT 'fyers',
            timeframe TEXT NOT NULL,
            open_time INTEGER NOT NULL,
            o REAL NOT NULL,
            h REAL NOT NULL,
            l REAL NOT NULL,
            c REAL NOT NULL,
            volume REAL NOT NULL DEFAULT 0,
            is_closed INTEGER NOT NULL DEFAULT 1,
            PRIMARY KEY (symbol, provider, timeframe, open_time)
        );"
    ).map_err(|e| format!("Failed to create candle_cache table: {}", e))?;

    let mut total_fetched = 0;
    let mut total_inserted = 0;
    let mut fetch_errors: Vec<String> = Vec::new();

    for symbol in symbols {
        // Build Fyers symbol format: "NSE:SYMBOL-EQ" if not already formatted
        let fyers_symbol = if symbol.contains(':') {
            symbol.clone()
        } else {
            format!("NSE:{}-EQ", symbol)
        };

        // Check if we already have recent data for this symbol+timeframe
        let existing_count: i64 = conn.query_row(
            "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2 AND is_closed = 1",
            rusqlite::params![symbol, timeframe],
            |row| row.get(0),
        ).unwrap_or(0);

        if existing_count >= 50 {
            debug_log.push(format!("[prefetch] {} already cached ({} candles), skipping", symbol, existing_count));
            continue;
        }

        debug_log.push(format!("[prefetch] {} -> fetching as '{}' (existing={})", symbol, fyers_symbol, existing_count));

        // Fetch from Fyers API
        match brokers::fyers_get_history(
            api_key.clone(),
            access_token.clone(),
            fyers_symbol.clone(),
            resolution.to_string(),
            from_date.clone(),
            to_date.clone(),
        ).await {
            Ok(response) => {
                if response.success {
                    if let Some(candles) = response.data.and_then(|d| d.as_array().cloned()) {
                        total_fetched += candles.len();
                        debug_log.push(format!("[prefetch] {} -> {} candles from API", symbol, candles.len()));

                        // Insert candles into candle_cache
                        // Fyers format: [timestamp, O, H, L, C, V]
                        let mut inserted = 0;
                        for candle in &candles {
                            if let Some(arr) = candle.as_array() {
                                if arr.len() >= 6 {
                                    let open_time = arr[0].as_i64().unwrap_or(0);
                                    let o = arr[1].as_f64().unwrap_or(0.0);
                                    let h = arr[2].as_f64().unwrap_or(0.0);
                                    let l = arr[3].as_f64().unwrap_or(0.0);
                                    let c = arr[4].as_f64().unwrap_or(0.0);
                                    let v = arr[5].as_f64().unwrap_or(0.0);

                                    let _ = conn.execute(
                                        "INSERT OR REPLACE INTO candle_cache
                                         (symbol, provider, timeframe, open_time, o, h, l, c, volume, is_closed)
                                         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 1)",
                                        rusqlite::params![symbol, "fyers", timeframe, open_time, o, h, l, c, v],
                                    );
                                    inserted += 1;
                                }
                            }
                        }
                        total_inserted += inserted;
                        debug_log.push(format!("[prefetch] {} -> {} candles inserted", symbol, inserted));
                    } else {
                        let msg = format!("{}: API returned success but no candle data", symbol);
                        debug_log.push(format!("[prefetch] {} -> WARNING: {}", symbol, msg));
                        fetch_errors.push(msg);
                    }
                } else {
                    let api_err = response.error.unwrap_or_else(|| "Unknown API error".to_string());
                    let msg = format!("{}: Fyers API error — {}", symbol, api_err);
                    debug_log.push(format!("[prefetch] {} -> ERROR: {}", symbol, api_err));
                    fetch_errors.push(msg);
                }
            }
            Err(e) => {
                let msg = format!("{}: request failed — {}", symbol, e);
                debug_log.push(format!("[prefetch] {} -> FETCH ERROR: {}", symbol, e));
                fetch_errors.push(msg);
            }
        }
    }

    debug_log.push(format!("[prefetch] summary: fetched={}, inserted={}, errors={}", total_fetched, total_inserted, fetch_errors.len()));

    // If ALL symbols failed, return an error so the caller knows
    if total_inserted == 0 && !fetch_errors.is_empty() {
        let combined = fetch_errors.join("; ");
        return Err(format!("All symbol fetches failed: {}", combined));
    }

    Ok(debug_log)
}

// ============================================================================
// CANDLE CACHE
// ============================================================================

/// Read aggregated candles from cache
#[tauri::command]
pub async fn get_candle_cache(
    symbol: String,
    timeframe: Option<String>,
    limit: Option<i32>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());
    let row_limit = limit.unwrap_or(200);

    let mut stmt = conn
        .prepare(
            "SELECT symbol, provider, timeframe, open_time, o, h, l, c, volume, is_closed
             FROM candle_cache
             WHERE symbol = ?1 AND timeframe = ?2
             ORDER BY open_time DESC LIMIT ?3",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map(rusqlite::params![symbol, tf, row_limit], |row| {
            Ok(json!({
                "symbol": row.get::<_, String>(0)?,
                "provider": row.get::<_, String>(1)?,
                "timeframe": row.get::<_, String>(2)?,
                "open_time": row.get::<_, i64>(3)?,
                "o": row.get::<_, f64>(4)?,
                "h": row.get::<_, f64>(5)?,
                "l": row.get::<_, f64>(6)?,
                "c": row.get::<_, f64>(7)?,
                "volume": row.get::<_, f64>(8)?,
                "is_closed": row.get::<_, i32>(9)?,
            }))
        })
        .map_err(|e| format!("Failed to query candles: {}", e))?;

    let candles: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": candles,
        "count": candles.len()
    }).to_string())
}

// ============================================================================
// CONDITION EVALUATION (ONE-SHOT PREVIEW)
// ============================================================================

/// Evaluate conditions once against current data (for builder UI preview)
#[tauri::command]
pub async fn evaluate_conditions_once(
    app: tauri::AppHandle,
    conditions: String,
    symbol: String,
    timeframe: Option<String>,
) -> Result<String, String> {
    use std::process::Command;

    let scripts_dir = get_algo_scripts_dir(&app)?;
    let evaluator_path = scripts_dir.join("condition_evaluator.py");
    let db_path = get_main_db_path_str()?;
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());

    if !evaluator_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "condition_evaluator.py not found"
        }).to_string());
    }

    let output = Command::new("python")
        .arg(&evaluator_path)
        .arg("--mode")
        .arg("once")
        .arg("--conditions")
        .arg(&conditions)
        .arg("--symbol")
        .arg(&symbol)
        .arg("--timeframe")
        .arg(&tf)
        .arg("--db")
        .arg(&db_path)
        .output()
        .map_err(|e| format!("Failed to evaluate conditions: {}", e))?;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        if let Ok(parsed) = serde_json::from_str::<serde_json::Value>(&stdout) {
            Ok(parsed.to_string())
        } else {
            Ok(json!({ "success": true, "raw": stdout.to_string() }).to_string())
        }
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Ok(json!({
            "success": false,
            "error": format!("Evaluation failed: {}", stderr)
        }).to_string())
    }
}

// ============================================================================
// CANDLE AGGREGATION CONTROL
// ============================================================================

/// Start candle aggregation for a symbol+timeframe
/// (Registers the symbol with the CandleAggregator service)
#[tauri::command]
pub async fn start_candle_aggregation(
    state: tauri::State<'_, crate::WebSocketState>,
    symbol: String,
    timeframe: Option<String>,
) -> Result<String, String> {
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());

    // Register with the live CandleAggregator service
    let services = state.services.read().await;
    services
        .candle_aggregator
        .add_subscription(symbol.clone(), tf.clone())
        .await;

    Ok(json!({
        "success": true,
        "symbol": symbol,
        "timeframe": tf,
        "message": "Candle aggregation started"
    }).to_string())
}

/// Stop candle aggregation for a symbol+timeframe
#[tauri::command]
pub async fn stop_candle_aggregation(
    state: tauri::State<'_, crate::WebSocketState>,
    symbol: String,
    timeframe: Option<String>,
) -> Result<String, String> {
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());

    // Unregister from the live CandleAggregator service
    let services = state.services.read().await;
    services
        .candle_aggregator
        .remove_subscription(symbol.clone(), tf.clone())
        .await;

    Ok(json!({
        "success": true,
        "symbol": symbol,
        "timeframe": tf,
        "message": "Candle aggregation stopped"
    }).to_string())
}

// ============================================================================
// BACKTEST
// ============================================================================

/// Run a walk-forward backtest using historical data (via yfinance)
#[tauri::command]
pub async fn run_algo_backtest(
    app: tauri::AppHandle,
    symbol: String,
    entry_conditions: String,
    exit_conditions: String,
    timeframe: Option<String>,
    period: Option<String>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
    initial_capital: Option<f64>,
) -> Result<String, String> {
    use std::process::Command;

    let scripts_dir = get_algo_scripts_dir(&app)?;
    let backtest_path = scripts_dir.join("backtest_engine.py");

    if !backtest_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "backtest_engine.py not found"
        }).to_string());
    }

    let tf = timeframe.unwrap_or_else(|| "1d".to_string());
    let pd = period.unwrap_or_else(|| "1y".to_string());
    let sl = stop_loss.unwrap_or(0.0);
    let tp = take_profit.unwrap_or(0.0);
    let capital = initial_capital.unwrap_or(100000.0);

    let output = Command::new("python")
        .arg(&backtest_path)
        .arg("--symbol")
        .arg(&symbol)
        .arg("--entry-conditions")
        .arg(&entry_conditions)
        .arg("--exit-conditions")
        .arg(&exit_conditions)
        .arg("--timeframe")
        .arg(&tf)
        .arg("--period")
        .arg(&pd)
        .arg("--stop-loss")
        .arg(sl.to_string())
        .arg("--take-profit")
        .arg(tp.to_string())
        .arg("--initial-capital")
        .arg(capital.to_string())
        .output()
        .map_err(|e| format!("Failed to run backtest: {}", e))?;

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    if !output.status.success() {
        return Ok(json!({
            "success": false,
            "error": format!("Backtest process failed: {}", stderr.trim())
        }).to_string());
    }

    // Return the raw JSON from Python
    if stdout.trim().is_empty() {
        return Ok(json!({
            "success": false,
            "error": "Backtest returned no output"
        }).to_string());
    }

    Ok(stdout.trim().to_string())
}

// ============================================================================
// ORDER SIGNAL BRIDGE
// ============================================================================
// Background task that polls `algo_order_signals` for pending signals written
// by Python live runners in "live" mode. Executes orders via the existing
// unified trading system and updates signal status.

use std::sync::atomic::{AtomicBool, Ordering};

static ORDER_BRIDGE_RUNNING: AtomicBool = AtomicBool::new(false);

/// Start the order signal polling bridge (idempotent — only one instance runs)
#[tauri::command]
pub async fn start_order_signal_bridge(
    app: tauri::AppHandle,
) -> Result<String, String> {
    if ORDER_BRIDGE_RUNNING.swap(true, Ordering::SeqCst) {
        return Ok(json!({
            "success": true,
            "message": "Order bridge already running"
        }).to_string());
    }

    let app_handle = app.clone();

    tokio::spawn(async move {
        println!("[AlgoOrderBridge] Starting order signal polling...");

        loop {
            // Check if we should still be running
            if !ORDER_BRIDGE_RUNNING.load(Ordering::SeqCst) {
                break;
            }

            // Poll for pending signals
            match poll_and_execute_signals(&app_handle).await {
                Ok(count) => {
                    if count > 0 {
                        println!("[AlgoOrderBridge] Executed {} order signal(s)", count);
                    }
                }
                Err(e) => {
                    eprintln!("[AlgoOrderBridge] Error polling signals: {}", e);
                }
            }

            tokio::time::sleep(std::time::Duration::from_secs(1)).await;
        }

        println!("[AlgoOrderBridge] Order signal polling stopped");
        ORDER_BRIDGE_RUNNING.store(false, Ordering::SeqCst);
    });

    Ok(json!({
        "success": true,
        "message": "Order bridge started"
    }).to_string())
}

/// Stop the order signal polling bridge
#[tauri::command]
pub async fn stop_order_signal_bridge() -> Result<String, String> {
    ORDER_BRIDGE_RUNNING.store(false, Ordering::SeqCst);
    Ok(json!({
        "success": true,
        "message": "Order bridge stopping"
    }).to_string())
}

/// Poll `algo_order_signals` for pending signals and execute them
async fn poll_and_execute_signals(
    app_handle: &tauri::AppHandle,
) -> Result<usize, String> {
    use crate::commands::unified_trading::{UnifiedOrder, unified_place_order};

    // Read pending signals from DB
    let signals = tokio::task::spawn_blocking(|| -> Result<Vec<(String, String, String, f64, String, Option<f64>)>, String> {
        let conn = get_db().map_err(|e| e.to_string())?;
        let mut stmt = conn
            .prepare(
                "SELECT id, symbol, side, quantity, order_type, price
                 FROM algo_order_signals
                 WHERE status = 'pending'
                 ORDER BY created_at ASC
                 LIMIT 10",
            )
            .map_err(|e| format!("Failed to query signals: {}", e))?;

        let rows = stmt
            .query_map([], |row| {
                Ok((
                    row.get::<_, String>(0)?,
                    row.get::<_, String>(1)?,
                    row.get::<_, String>(2)?,
                    row.get::<_, f64>(3)?,
                    row.get::<_, String>(4)?,
                    row.get::<_, Option<f64>>(5)?,
                ))
            })
            .map_err(|e| format!("Failed to read signals: {}", e))?;

        let mut result = Vec::new();
        for row in rows {
            if let Ok(r) = row {
                result.push(r);
            }
        }
        Ok(result)
    })
    .await
    .map_err(|e| format!("Spawn blocking error: {}", e))??;

    if signals.is_empty() {
        return Ok(0);
    }

    let mut executed = 0;

    for (signal_id, symbol, side, quantity, order_type, price) in &signals {
        // Emit algo_signal event to notify frontend that a condition matched
        let _ = app_handle.emit(
            "algo_signal",
            json!({
                "signal_id": signal_id,
                "symbol": symbol,
                "side": side,
                "quantity": quantity,
                "order_type": order_type,
            }),
        );

        // Mark as processing
        let sid = signal_id.clone();
        let _ = tokio::task::spawn_blocking(move || {
            if let Ok(conn) = get_db() {
                let _ = conn.execute(
                    "UPDATE algo_order_signals SET status = 'processing' WHERE id = ?1",
                    rusqlite::params![sid],
                );
            }
        })
        .await;

        // Build the UnifiedOrder
        // Parse exchange from symbol (e.g., "NSE:RELIANCE" -> exchange="NSE", symbol="RELIANCE")
        let (exchange, clean_symbol) = if symbol.contains(':') {
            let parts: Vec<&str> = symbol.splitn(2, ':').collect();
            (parts[0].to_string(), parts[1].to_string())
        } else {
            ("NSE".to_string(), symbol.clone())
        };

        let order = UnifiedOrder {
            symbol: clean_symbol,
            exchange,
            side: side.to_lowercase(),
            order_type: order_type.to_lowercase(),
            quantity: *quantity,
            price: *price,
            stop_price: None,
            product_type: Some("MIS".to_string()),
        };

        // Execute via unified trading system
        let result = unified_place_order(order).await;

        let success = result.is_ok() && result.as_ref().map(|r| r.success).unwrap_or(false);
        let result_msg = match &result {
            Ok(r) => r.message.clone().unwrap_or_default(),
            Err(e) => e.clone(),
        };

        // Update signal status
        let sid = signal_id.clone();
        let status = if success { "filled" } else { "failed" };
        let result_json = json!({
            "success": success,
            "message": result_msg,
        })
        .to_string();

        let _ = tokio::task::spawn_blocking(move || {
            if let Ok(conn) = get_db() {
                let _ = conn.execute(
                    "UPDATE algo_order_signals SET status = ?1, result = ?2, executed_at = CURRENT_TIMESTAMP WHERE id = ?3",
                    rusqlite::params![status, result_json, sid],
                );
            }
        })
        .await;

        // Emit event to frontend
        let _ = app_handle.emit(
            "algo_trade_executed",
            json!({
                "signal_id": signal_id,
                "symbol": symbol,
                "side": side,
                "quantity": quantity,
                "success": success,
                "message": result_msg,
            }),
        );

        if success {
            executed += 1;
        }
    }

    Ok(executed)
}
