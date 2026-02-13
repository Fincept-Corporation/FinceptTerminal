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

    println!("[AlgoDeploy] ====================================================");
    println!("[AlgoDeploy] Strategy: {}, Symbol: {}, Mode: {}, TF: {}, Qty: {}", strategy_id, symbol, deploy_mode, deploy_timeframe, deploy_qty);
    println!("[AlgoDeploy] Provider: '{}', Params: {}", deploy_provider, deploy_params);

    // Generate deployment ID
    let deploy_id = format!("algo-{}", uuid::Uuid::new_v4());
    println!("[AlgoDeploy] Deploy ID: {}", deploy_id);

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

    // Pre-fetch historical candles so the runner has data to evaluate conditions against.
    // Without this, the candle_cache is empty and the runner just loops forever seeing no data.
    // We do this before spawning the runner so data is available immediately.
    let broker_for_prefetch = if deploy_provider.is_empty() { "fyers".to_string() } else { deploy_provider.clone() };
    let symbol_list = vec![symbol.clone()];
    match prefetch_historical_candles(&db_path, &symbol_list, &deploy_timeframe, &broker_for_prefetch, None).await {
        Ok(_debug) => {
            println!("[deploy] Prefetched historical candles for {} @ {}", symbol, deploy_timeframe);
        }
        Err(e) => {
            // Non-fatal: log warning but proceed with deployment.
            // The candle aggregator will build candles from live ticks going forward.
            eprintln!("[deploy] Warning: candle prefetch failed for {}: {}", symbol, e);
        }
    }

    // Spawn the Python runner as a background process
    println!("[AlgoDeploy] Spawning Python runner: {:?}", runner_path);
    println!("[AlgoDeploy] DB path: {}", db_path);
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
            println!("[AlgoDeploy] Python runner spawned OK, PID={}", pid);

            // Update deployment with PID and running status
            let _ = conn.execute(
                "UPDATE algo_deployments SET status = 'running', pid = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
                rusqlite::params![pid as i64, deploy_id],
            );

            // Auto-start candle aggregation for this symbol+timeframe
            println!("[AlgoDeploy] Starting candle aggregation for {} @ {}", symbol, deploy_timeframe);
            let services = state.services.read().await;
            services
                .candle_aggregator
                .add_subscription(symbol.clone(), deploy_timeframe.clone())
                .await;
            println!("[AlgoDeploy] Candle aggregation subscription added");

            // -- Auto-connect broker WebSocket if needed --
            // When deploying for Indian stock brokers (e.g., angelone), the user may
            // not have visited the Equity Trading tab, so the broker WS is not connected.
            // We auto-connect from stored credentials so ticks flow to CandleAggregator.
            if deploy_provider == "angelone" {
                // Parse token and exchange from deploy params (sent by frontend SymbolSearch)
                let params_json: serde_json::Value = serde_json::from_str(&deploy_params).unwrap_or_default();
                let direct_token = params_json.get("token").and_then(|v| v.as_str()).map(String::from);
                let direct_exchange = params_json.get("exchange").and_then(|v| v.as_str()).unwrap_or("NSE").to_string();

                println!("[AlgoDeploy] Params: token={:?}, exchange={}", direct_token, direct_exchange);

                let router = state.router.clone();
                match crate::commands::brokers::websocket_commands::ensure_angelone_ws_connected(&app, router).await {
                    Ok(_) => {
                        println!("[AlgoDeploy] Angel One WS connected, subscribing to {}", symbol);
                        // Use the token from frontend if available, otherwise symbol master lookup
                        match crate::commands::brokers::websocket_commands::ensure_angelone_ws_subscribed(
                            &symbol, &direct_exchange, direct_token.as_deref()
                        ).await {
                            Ok(_) => println!("[AlgoDeploy] Angel One WS subscribed to {}", symbol),
                            Err(e) => {
                                // If direct exchange failed and it wasn't BSE, try BSE as fallback
                                if direct_exchange != "BSE" {
                                    eprintln!("[AlgoDeploy] {} subscribe failed ({}), trying BSE...", direct_exchange, e);
                                    match crate::commands::brokers::websocket_commands::ensure_angelone_ws_subscribed(
                                        &symbol, "BSE", None
                                    ).await {
                                        Ok(_) => println!("[AlgoDeploy] Angel One WS subscribed to {} on BSE", symbol),
                                        Err(e2) => eprintln!("[AlgoDeploy] WARNING: Could not subscribe: {}={}, BSE={}", direct_exchange, e, e2),
                                    }
                                } else {
                                    eprintln!("[AlgoDeploy] WARNING: Could not subscribe to {}: {}", symbol, e);
                                }
                            }
                        }
                    }
                    Err(e) => {
                        eprintln!("[AlgoDeploy] WARNING: Could not auto-connect Angel One WS: {}", e);
                        eprintln!("[AlgoDeploy] Ticks will not reach CandleAggregator until WS is connected from the UI");
                    }
                }
            }

            // Auto-start order signal bridge for live deployments
            // (idempotent — only one instance runs regardless of how many times called)
            if deploy_mode == "live" && !ORDER_BRIDGE_RUNNING.swap(true, Ordering::SeqCst) {
                let bridge_app = app.clone();
                tokio::spawn(async move {
                    println!("[AlgoOrderBridge] Auto-started for live deployment");
                    loop {
                        if !ORDER_BRIDGE_RUNNING.load(Ordering::SeqCst) {
                            break;
                        }
                        match poll_and_execute_signals(&bridge_app).await {
                            Ok(count) => {
                                if count > 0 {
                                    println!("[AlgoOrderBridge] Executed {} order signal(s)", count);
                                }
                            }
                            Err(e) => {
                                eprintln!("[AlgoOrderBridge] Error: {}", e);
                            }
                        }
                        tokio::time::sleep(std::time::Duration::from_secs(1)).await;
                    }
                    ORDER_BRIDGE_RUNNING.store(false, Ordering::SeqCst);
                });
            }

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

/// Delete an algo deployment and its associated trades/metrics
#[tauri::command]
pub async fn delete_algo_deployment(deploy_id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // First check if deployment exists and stop it if running
    let status_result: Result<(String, Option<i64>), _> = conn.query_row(
        "SELECT status, pid FROM algo_deployments WHERE id = ?1",
        rusqlite::params![deploy_id],
        |row| Ok((row.get::<_, String>(0)?, row.get::<_, Option<i64>>(1)?)),
    );

    match status_result {
        Ok((status, pid)) => {
            // Kill process if still running
            if status == "running" {
                if let Some(pid) = pid {
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
                }
            }

            // Delete associated records (trades, metrics, order signals)
            let _ = conn.execute(
                "DELETE FROM algo_trades WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            );
            let _ = conn.execute(
                "DELETE FROM algo_metrics WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            );
            let _ = conn.execute(
                "DELETE FROM algo_order_signals WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            );

            // Delete the deployment itself
            conn.execute(
                "DELETE FROM algo_deployments WHERE id = ?1",
                rusqlite::params![deploy_id],
            )
            .map_err(|e| format!("Failed to delete deployment: {}", e))?;

            println!("[AlgoTrading] Deleted deployment: {}", deploy_id);
            Ok(json!({ "success": true, "deleted": true }).to_string())
        }
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Ok(json!({ "success": false, "error": "Deployment not found" }).to_string())
        }
        Err(e) => {
            Ok(json!({ "success": false, "error": e.to_string() }).to_string())
        }
    }
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

    // Log deployment list summary to console (only when there are running deployments)
    let running_count = deployments.iter().filter(|d| d.get("status").and_then(|s| s.as_str()) == Some("running")).count();
    if running_count > 0 {
        println!("[AlgoMonitor] list_deployments: {} total, {} running", deployments.len(), running_count);
    }

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
    lookback_days: Option<i64>,
) -> Result<String, String> {
    use std::process::Command;

    let scripts_dir = get_algo_scripts_dir(&app)?;
    let scanner_path = scripts_dir.join("scanner_engine.py");
    let db_path = get_main_db_path_str()?;
    let tf = timeframe.unwrap_or_else(|| "5m".to_string());
    let broker = provider.as_deref().unwrap_or("fyers").to_string();
    let lookback = lookback_days;

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
        match prefetch_historical_candles(&db_path, &symbol_list, &tf, &broker, lookback).await {
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
/// Maps timeframe strings to Fyers resolution values and fetches data based on lookback.
/// If lookback_days is None, uses default lookback based on timeframe.
/// Returns a Vec of debug log lines for the caller to include in the response.
async fn prefetch_historical_candles(
    db_path: &str,
    symbols: &[String],
    timeframe: &str,
    broker: &str,
    lookback_override: Option<i64>,
) -> Result<Vec<String>, String> {
    use crate::commands::brokers;

    let mut debug_log: Vec<String> = Vec::new();

    // For non-Fyers brokers, fall back to Fyers API for historical data prefetch.
    // Fyers supports all NSE/BSE symbols regardless of which broker is used for execution.
    if broker != "fyers" {
        debug_log.push(format!("[prefetch] broker '{}' doesn't have prefetch support, falling back to Fyers API for historical data", broker));
    }

    // Get Fyers credentials (always use Fyers for historical data)
    debug_log.push("[prefetch] fetching Fyers credentials...".to_string());
    let creds = match brokers::get_indian_broker_credentials("fyers".to_string()).await {
        Ok(c) => c,
        Err(e) => {
            debug_log.push(format!("[prefetch] could not fetch Fyers credentials: {}. Skipping prefetch — algo will rely on live ticks.", e));
            return Ok(debug_log);
        }
    };

    if !creds.success || creds.data.is_none() {
        debug_log.push("[prefetch] Fyers credentials not found. Skipping prefetch — algo will rely on live ticks. For historical data, authenticate with Fyers in Settings > Brokers.".to_string());
        return Ok(debug_log);
    }

    let cred_data = creds.data.unwrap();
    let api_key = match cred_data.get("apiKey").and_then(|v| v.as_str()) {
        Some(k) if !k.is_empty() => k.to_string(),
        _ => {
            debug_log.push("[prefetch] Fyers API key not found or empty. Skipping prefetch.".to_string());
            return Ok(debug_log);
        }
    };
    let access_token = match cred_data.get("accessToken").and_then(|v| v.as_str()) {
        Some(t) if !t.is_empty() => t.to_string(),
        _ => {
            debug_log.push("[prefetch] Fyers access token not found or empty. Skipping prefetch — tokens expire daily.".to_string());
            return Ok(debug_log);
        }
    };

    debug_log.push(format!("[prefetch] credentials OK (api_key len={}, token len={})", api_key.len(), access_token.len()));

    // Map timeframe to Fyers resolution and default lookback days
    let (resolution, default_lookback) = match timeframe {
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

    // Use override if provided, otherwise use default for the timeframe
    let lookback_days = lookback_override.unwrap_or(default_lookback);

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

/// Run a walk-forward backtest using historical data (via yfinance or candle_cache)
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
    provider: Option<String>,
) -> Result<String, String> {
    use std::process::Command;

    let mut debug_log: Vec<String> = Vec::new();
    debug_log.push("[backtest] Starting backtest...".to_string());

    // Get scripts directory
    let scripts_dir = match get_algo_scripts_dir(&app) {
        Ok(dir) => {
            debug_log.push(format!("[backtest] scripts_dir={:?}", dir));
            dir
        }
        Err(e) => {
            debug_log.push(format!("[backtest] ERROR getting scripts_dir: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to get scripts directory: {}", e),
                "debug": debug_log
            }).to_string());
        }
    };

    let backtest_path = scripts_dir.join("backtest_engine.py");
    debug_log.push(format!("[backtest] backtest_path={:?}, exists={}", backtest_path, backtest_path.exists()));

    if !backtest_path.exists() {
        return Ok(json!({
            "success": false,
            "error": "backtest_engine.py not found",
            "debug": debug_log
        }).to_string());
    }

    let tf = timeframe.unwrap_or_else(|| "1d".to_string());
    let pd = period.unwrap_or_else(|| "1y".to_string());
    let sl = stop_loss.unwrap_or(0.0);
    let tp = take_profit.unwrap_or(0.0);
    let capital = initial_capital.unwrap_or(100000.0);
    let data_provider = provider.unwrap_or_else(|| "yfinance".to_string());

    let db_path = match get_main_db_path_str() {
        Ok(path) => {
            debug_log.push(format!("[backtest] db_path={}", path));
            path
        }
        Err(e) => {
            debug_log.push(format!("[backtest] ERROR getting db_path: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to get database path: {}", e),
                "debug": debug_log
            }).to_string());
        }
    };

    debug_log.push(format!(
        "[backtest] params: symbol={}, tf={}, period={}, sl={}, tp={}, capital={}, provider={}",
        symbol, tf, pd, sl, tp, capital, data_provider
    ));
    debug_log.push(format!("[backtest] entry_conditions={}", entry_conditions));
    debug_log.push(format!("[backtest] exit_conditions={}", exit_conditions));

    // Check if Python is available
    let python_check = Command::new("python")
        .arg("--version")
        .output();

    match &python_check {
        Ok(output) => {
            let version = String::from_utf8_lossy(&output.stdout);
            let stderr = String::from_utf8_lossy(&output.stderr);
            debug_log.push(format!("[backtest] Python version: {} {}", version.trim(), stderr.trim()));
        }
        Err(e) => {
            debug_log.push(format!("[backtest] ERROR: Python not found: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Python not found: {}. Make sure Python is installed and in PATH.", e),
                "debug": debug_log
            }).to_string());
        }
    }

    debug_log.push("[backtest] Launching Python backtest_engine.py...".to_string());

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
        .arg("--provider")
        .arg(&data_provider)
        .arg("--db")
        .arg(&db_path)
        .output();

    let output = match output {
        Ok(out) => out,
        Err(e) => {
            debug_log.push(format!("[backtest] ERROR: Failed to spawn Python process: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to run backtest: {}", e),
                "debug": debug_log
            }).to_string());
        }
    };

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    let stderr = String::from_utf8_lossy(&output.stderr).to_string();

    debug_log.push(format!("[backtest] exit_code={:?}", output.status.code()));
    debug_log.push(format!("[backtest] stdout_len={}, stderr_len={}", stdout.len(), stderr.len()));

    if !stderr.trim().is_empty() {
        debug_log.push(format!("[backtest] stderr: {}", stderr.trim()));
    }

    if !output.status.success() {
        debug_log.push("[backtest] Process exited with error".to_string());
        return Ok(json!({
            "success": false,
            "error": format!("Backtest process failed: {}", if stderr.trim().is_empty() { "Unknown error" } else { stderr.trim() }),
            "debug": debug_log
        }).to_string());
    }

    // Return the raw JSON from Python
    if stdout.trim().is_empty() {
        debug_log.push("[backtest] ERROR: Empty output from Python".to_string());
        return Ok(json!({
            "success": false,
            "error": "Backtest returned no output",
            "debug": debug_log
        }).to_string());
    }

    debug_log.push(format!("[backtest] Raw output (first 500 chars): {}", &stdout[..stdout.len().min(500)]));

    // Try to parse and inject debug log
    if let Ok(mut parsed) = serde_json::from_str::<serde_json::Value>(&stdout) {
        if let Some(obj) = parsed.as_object_mut() {
            // Merge Python debug if present
            if let Some(py_debug) = obj.get("debug").and_then(|v| v.as_array()) {
                for entry in py_debug {
                    if let Some(s) = entry.as_str() {
                        debug_log.push(s.to_string());
                    }
                }
            }
            obj.insert("debug".to_string(), json!(debug_log));
        }
        debug_log.push("[backtest] Successfully parsed JSON output".to_string());
        Ok(parsed.to_string())
    } else {
        debug_log.push(format!("[backtest] WARNING: Failed to parse output as JSON"));
        debug_log.push(format!("[backtest] Raw stdout: {}", stdout));
        Ok(json!({
            "success": false,
            "error": "Failed to parse backtest output as JSON",
            "raw_output": stdout,
            "debug": debug_log
        }).to_string())
    }
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

/// Debug diagnostic command: returns the state of candle_cache, strategy_price_cache,
/// deployment status, and Python process health for a given deployment.
#[tauri::command]
pub async fn debug_algo_deployment(
    deploy_id: String,
) -> Result<String, String> {
    let result = tokio::task::spawn_blocking(move || -> Result<serde_json::Value, String> {
        let conn = get_db().map_err(|e| e.to_string())?;
        let mut diag = serde_json::Map::new();

        // 1. Deployment record
        let deploy_row = conn.query_row(
            "SELECT id, strategy_id, symbol, provider, mode, status, timeframe, quantity, pid, error_message, created_at, updated_at
             FROM algo_deployments WHERE id = ?1",
            rusqlite::params![deploy_id],
            |row| {
                Ok(json!({
                    "id": row.get::<_, String>(0)?,
                    "strategy_id": row.get::<_, String>(1)?,
                    "symbol": row.get::<_, String>(2)?,
                    "provider": row.get::<_, String>(3)?,
                    "mode": row.get::<_, String>(4)?,
                    "status": row.get::<_, String>(5)?,
                    "timeframe": row.get::<_, String>(6)?,
                    "quantity": row.get::<_, f64>(7)?,
                    "pid": row.get::<_, Option<i64>>(8)?,
                    "error_message": row.get::<_, Option<String>>(9)?,
                    "created_at": row.get::<_, String>(10)?,
                    "updated_at": row.get::<_, String>(11)?,
                }))
            },
        );
        match deploy_row {
            Ok(v) => { diag.insert("deployment".to_string(), v); }
            Err(e) => { diag.insert("deployment_error".to_string(), json!(e.to_string())); }
        }

        // Extract symbol and timeframe from deployment
        let (symbol, timeframe) = {
            let s: String = conn.query_row(
                "SELECT symbol FROM algo_deployments WHERE id = ?1", rusqlite::params![deploy_id], |r| r.get(0),
            ).unwrap_or_default();
            let t: String = conn.query_row(
                "SELECT timeframe FROM algo_deployments WHERE id = ?1", rusqlite::params![deploy_id], |r| r.get(0),
            ).unwrap_or_else(|_| "5m".to_string());
            (s, t)
        };

        // 2. Check if Python process is alive (check PID)
        let pid: Option<i64> = conn.query_row(
            "SELECT pid FROM algo_deployments WHERE id = ?1", rusqlite::params![deploy_id], |r| r.get(0),
        ).unwrap_or(None);

        if let Some(pid_val) = pid {
            #[cfg(target_os = "windows")]
            {
                let output = std::process::Command::new("tasklist")
                    .args(["/FI", &format!("PID eq {}", pid_val), "/FO", "CSV", "/NH"])
                    .output();
                match output {
                    Ok(o) => {
                        let stdout = String::from_utf8_lossy(&o.stdout);
                        let is_alive = stdout.contains(&pid_val.to_string());
                        diag.insert("python_process_alive".to_string(), json!(is_alive));
                        diag.insert("python_process_output".to_string(), json!(stdout.trim()));
                    }
                    Err(e) => {
                        diag.insert("python_process_check_error".to_string(), json!(e.to_string()));
                    }
                }
            }
            #[cfg(not(target_os = "windows"))]
            {
                let alive = std::path::Path::new(&format!("/proc/{}", pid_val)).exists();
                diag.insert("python_process_alive".to_string(), json!(alive));
            }
            diag.insert("python_pid".to_string(), json!(pid_val));
        } else {
            diag.insert("python_pid".to_string(), json!(null));
            diag.insert("python_process_alive".to_string(), json!(false));
        }

        // 3. Candle cache stats
        let candle_count: i64 = conn.query_row(
            "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2 AND is_closed = 1",
            rusqlite::params![symbol, timeframe],
            |r| r.get(0),
        ).unwrap_or(0);
        diag.insert("candle_cache_count".to_string(), json!(candle_count));

        // Also check with normalized symbol (strip colon prefix)
        let clean_sym = if symbol.contains(':') {
            symbol.split(':').last().unwrap_or(&symbol).to_string()
        } else {
            symbol.clone()
        };
        let candle_count_clean: i64 = conn.query_row(
            "SELECT COUNT(*) FROM candle_cache WHERE symbol = ?1 AND timeframe = ?2 AND is_closed = 1",
            rusqlite::params![clean_sym, timeframe],
            |r| r.get(0),
        ).unwrap_or(0);
        diag.insert("candle_cache_count_clean_symbol".to_string(), json!(candle_count_clean));

        // Get all distinct symbols in candle_cache for this timeframe
        let mut stmt = conn.prepare(
            "SELECT DISTINCT symbol, COUNT(*) as cnt FROM candle_cache WHERE timeframe = ?1 GROUP BY symbol"
        ).unwrap();
        let cached_symbols: Vec<serde_json::Value> = stmt.query_map(
            rusqlite::params![timeframe],
            |r| Ok(json!({"symbol": r.get::<_, String>(0)?, "count": r.get::<_, i64>(1)?}))
        ).unwrap().filter_map(|r| r.ok()).collect();
        diag.insert("candle_cache_symbols".to_string(), json!(cached_symbols));

        // Latest candle timestamp
        let latest_candle: Option<i64> = conn.query_row(
            "SELECT MAX(open_time) FROM candle_cache WHERE timeframe = ?1",
            rusqlite::params![timeframe],
            |r| r.get(0),
        ).unwrap_or(None);
        diag.insert("candle_cache_latest_ts".to_string(), json!(latest_candle));

        // 4. Strategy price cache
        let price_count: i64 = conn.query_row(
            "SELECT COUNT(*) FROM strategy_price_cache",
            [],
            |r| r.get(0),
        ).unwrap_or(0);
        diag.insert("price_cache_total_entries".to_string(), json!(price_count));

        let mut stmt2 = conn.prepare(
            "SELECT symbol, price, updated_at FROM strategy_price_cache ORDER BY updated_at DESC LIMIT 20"
        ).unwrap();
        let price_entries: Vec<serde_json::Value> = stmt2.query_map(
            [],
            |r| Ok(json!({"symbol": r.get::<_, String>(0)?, "price": r.get::<_, f64>(1)?, "updated_at": r.get::<_, i64>(2)?}))
        ).unwrap().filter_map(|r| r.ok()).collect();
        diag.insert("price_cache_entries".to_string(), json!(price_entries));

        // 5. Metrics
        let metrics = conn.query_row(
            "SELECT total_pnl, unrealized_pnl, total_trades, win_rate, max_drawdown, current_position_qty, current_position_side, current_position_entry, updated_at
             FROM algo_metrics WHERE deployment_id = ?1",
            rusqlite::params![deploy_id],
            |r| Ok(json!({
                "total_pnl": r.get::<_, f64>(0)?,
                "unrealized_pnl": r.get::<_, f64>(1)?,
                "total_trades": r.get::<_, i32>(2)?,
                "win_rate": r.get::<_, f64>(3)?,
                "max_drawdown": r.get::<_, f64>(4)?,
                "current_position_qty": r.get::<_, f64>(5)?,
                "current_position_side": r.get::<_, String>(6)?,
                "current_position_entry": r.get::<_, f64>(7)?,
                "updated_at": r.get::<_, String>(8)?,
            })),
        );
        match metrics {
            Ok(v) => { diag.insert("metrics".to_string(), v); }
            Err(e) => { diag.insert("metrics_error".to_string(), json!(e.to_string())); }
        }

        // 6. Recent trades
        let mut stmt3 = conn.prepare(
            "SELECT id, side, quantity, price, pnl, signal_reason, created_at FROM algo_trades WHERE deployment_id = ?1 ORDER BY created_at DESC LIMIT 10"
        ).unwrap();
        let trades: Vec<serde_json::Value> = stmt3.query_map(
            rusqlite::params![deploy_id],
            |r| Ok(json!({
                "id": r.get::<_, String>(0)?,
                "side": r.get::<_, String>(1)?,
                "quantity": r.get::<_, f64>(2)?,
                "price": r.get::<_, f64>(3)?,
                "pnl": r.get::<_, f64>(4)?,
                "reason": r.get::<_, String>(5)?,
                "created_at": r.get::<_, String>(6)?,
            })),
        ).unwrap().filter_map(|r| r.ok()).collect();
        diag.insert("recent_trades".to_string(), json!(trades));

        // 7. Pending order signals
        let pending_signals: i64 = conn.query_row(
            "SELECT COUNT(*) FROM algo_order_signals WHERE deployment_id = ?1 AND status = 'pending'",
            rusqlite::params![deploy_id],
            |r| r.get(0),
        ).unwrap_or(0);
        diag.insert("pending_order_signals".to_string(), json!(pending_signals));

        // 8. Order bridge status
        diag.insert("order_bridge_running".to_string(), json!(ORDER_BRIDGE_RUNNING.load(Ordering::SeqCst)));

        // 9. Strategy conditions (for context)
        let strategy_id: String = conn.query_row(
            "SELECT strategy_id FROM algo_deployments WHERE id = ?1", rusqlite::params![deploy_id], |r| r.get(0),
        ).unwrap_or_default();
        let entry_conds: Option<String> = conn.query_row(
            "SELECT entry_conditions FROM algo_strategies WHERE id = ?1", rusqlite::params![strategy_id], |r| r.get(0),
        ).ok();
        let exit_conds: Option<String> = conn.query_row(
            "SELECT exit_conditions FROM algo_strategies WHERE id = ?1", rusqlite::params![strategy_id], |r| r.get(0),
        ).ok();
        diag.insert("entry_conditions".to_string(), json!(entry_conds));
        diag.insert("exit_conditions".to_string(), json!(exit_conds));

        Ok(json!({"success": true, "data": diag}))
    })
    .await
    .map_err(|e| format!("Spawn error: {}", e))??;

    Ok(result.to_string())
}

/// Get live prices from strategy_price_cache for a list of symbols.
/// Returns a map of symbol -> { price, bid, ask, change_percent, updated_at }.
#[tauri::command]
pub async fn get_deployment_prices(
    symbols: Vec<String>,
) -> Result<String, String> {
    let result = tokio::task::spawn_blocking(move || -> Result<serde_json::Value, String> {
        let conn = get_db().map_err(|e| e.to_string())?;
        let mut prices = serde_json::Map::new();

        // Normalize helper: strip /, -, _, : and uppercase
        fn normalize(s: &str) -> String {
            s.replace('/', "").replace('-', "").replace('_', "").replace(':', "").to_uppercase()
        }

        // Load all cached prices once
        let mut stmt = conn.prepare(
            "SELECT symbol, price, bid, ask, volume, high, low, open, change_percent, updated_at FROM strategy_price_cache"
        ).map_err(|e| format!("Query failed: {}", e))?;

        let rows: Vec<(String, f64, Option<f64>, Option<f64>, Option<f64>, Option<f64>, Option<f64>, Option<f64>, Option<f64>, i64)> = stmt
            .query_map([], |row| {
                Ok((
                    row.get::<_, String>(0)?,
                    row.get::<_, f64>(1)?,
                    row.get::<_, Option<f64>>(2)?,
                    row.get::<_, Option<f64>>(3)?,
                    row.get::<_, Option<f64>>(4)?,
                    row.get::<_, Option<f64>>(5)?,
                    row.get::<_, Option<f64>>(6)?,
                    row.get::<_, Option<f64>>(7)?,
                    row.get::<_, Option<f64>>(8)?,
                    row.get::<_, i64>(9)?,
                ))
            })
            .map_err(|e| format!("Read failed: {}", e))?
            .filter_map(|r| r.ok())
            .collect();

        for req_sym in &symbols {
            let norm_req = normalize(req_sym);
            for (cached_sym, price, bid, ask, volume, high, low, open, change_pct, updated) in &rows {
                if normalize(cached_sym) == norm_req {
                    prices.insert(req_sym.clone(), json!({
                        "price": price,
                        "bid": bid,
                        "ask": ask,
                        "volume": volume,
                        "high": high,
                        "low": low,
                        "open": open,
                        "change_percent": change_pct,
                        "updated_at": updated,
                    }));
                    break;
                }
            }
        }

        Ok(json!({ "success": true, "data": prices }))
    })
    .await
    .map_err(|e| format!("Spawn error: {}", e))??;

    Ok(result.to_string())
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

// ============================================================================
// PYTHON STRATEGY LIBRARY COMMANDS
// ============================================================================

/// Get the strategies directory (for Python library strategies)
fn get_strategies_dir(app: &tauri::AppHandle) -> Result<PathBuf, String> {
    let resource_dir = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let strategies_dir = resource_dir
        .join("resources")
        .join("scripts")
        .join("strategies");

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
                            let data_str = &dict_content[data_start..data_end];

                            // Parse the inner dict manually
                            let name = extract_field(data_str, "name");
                            let category = extract_field(data_str, "category");
                            let path = extract_field(data_str, "path");

                            if let (Some(name), Some(category), Some(path)) = (name, category, path) {
                                strategies.push(json!({
                                    "id": strategy_id,
                                    "name": name,
                                    "category": category,
                                    "path": path,
                                    "description": "",
                                    "compatibility": ["backtest", "paper", "live"]
                                }));
                            }

                            current_pos = data_end;
                            continue;
                        }
                    }
                }
            }
            break;
        }
    }

    Ok(strategies)
}

/// Helper to extract a field value from a Python dict string
fn extract_field(dict_str: &str, field: &str) -> Option<String> {
    let pattern = format!("\"{}\":", field);
    if let Some(start) = dict_str.find(&pattern) {
        let after_key = &dict_str[start + pattern.len()..];
        // Find the opening quote
        if let Some(quote_start) = after_key.find("\"") {
            let after_quote = &after_key[quote_start + 1..];
            // Find the closing quote
            if let Some(quote_end) = after_quote.find("\"") {
                return Some(after_quote[..quote_end].to_string());
            }
        }
    }
    None
}

/// List all Python strategies from the library
#[tauri::command]
pub async fn list_python_strategies(
    app: tauri::AppHandle,
    category: Option<String>,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Err("Strategy registry not found".to_string());
    }

    let mut strategies = parse_strategy_registry(&registry_path)?;

    // Filter by category if specified
    if let Some(cat) = category {
        strategies.retain(|s| {
            s.get("category")
                .and_then(|c| c.as_str())
                .map(|c| c.to_lowercase() == cat.to_lowercase())
                .unwrap_or(false)
        });
    }

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    })
    .to_string())
}

/// Get all unique strategy categories
#[tauri::command]
pub async fn get_strategy_categories(app: tauri::AppHandle) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Err("Strategy registry not found".to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;

    let mut categories: Vec<String> = strategies
        .iter()
        .filter_map(|s| s.get("category").and_then(|c| c.as_str()).map(|s| s.to_string()))
        .collect();

    categories.sort();
    categories.dedup();

    Ok(json!({
        "success": true,
        "data": categories,
        "count": categories.len()
    })
    .to_string())
}

/// Get a single Python strategy metadata by ID
#[tauri::command]
pub async fn get_python_strategy(
    app: tauri::AppHandle,
    strategy_id: String,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Err("Strategy registry not found".to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;

    let strategy = strategies
        .into_iter()
        .find(|s| s.get("id").and_then(|id| id.as_str()) == Some(&strategy_id));

    match strategy {
        Some(s) => Ok(json!({
            "success": true,
            "data": s
        })
        .to_string()),
        None => Err(format!("Strategy {} not found", strategy_id)),
    }
}

/// Get the Python source code for a strategy
#[tauri::command]
pub async fn get_python_strategy_code(
    app: tauri::AppHandle,
    strategy_id: String,
) -> Result<String, String> {
    let strategies_dir = get_strategies_dir(&app)?;
    let registry_path = strategies_dir.join("_registry.py");

    if !registry_path.exists() {
        return Err("Strategy registry not found".to_string());
    }

    let strategies = parse_strategy_registry(&registry_path)?;

    let strategy = strategies
        .iter()
        .find(|s| s.get("id").and_then(|id| id.as_str()) == Some(&strategy_id));

    match strategy {
        Some(s) => {
            let path = s
                .get("path")
                .and_then(|p| p.as_str())
                .ok_or("Strategy path not found")?;

            let strategy_path = strategies_dir.join(path);

            if !strategy_path.exists() {
                return Err(format!("Strategy file not found: {}", path));
            }

            let code = std::fs::read_to_string(&strategy_path)
                .map_err(|e| format!("Failed to read strategy file: {}", e))?;

            // Extract description from docstring
            let description = extract_docstring(&code).unwrap_or_default();

            Ok(json!({
                "success": true,
                "data": {
                    "id": strategy_id,
                    "path": path,
                    "code": code,
                    "description": description
                }
            })
            .to_string())
        }
        None => Err(format!("Strategy {} not found", strategy_id)),
    }
}

/// Extract docstring from Python code
fn extract_docstring(code: &str) -> Option<String> {
    // Look for triple-quoted docstring at class level
    let patterns = ["'''", "\"\"\""];

    for pattern in patterns {
        // Find docstring after class definition
        if let Some(class_idx) = code.find("class ") {
            let after_class = &code[class_idx..];
            if let Some(doc_start) = after_class.find(pattern) {
                let doc_content = &after_class[doc_start + pattern.len()..];
                if let Some(doc_end) = doc_content.find(pattern) {
                    let docstring = doc_content[..doc_end].trim().to_string();
                    if !docstring.is_empty() {
                        return Some(docstring);
                    }
                }
            }
        }
    }

    // Also check header comment
    if let Some(desc_line) = code.lines().find(|l| l.contains("# Description:")) {
        return Some(desc_line.replace("# Description:", "").trim().to_string());
    }

    None
}

// ============================================================================
// CUSTOM PYTHON STRATEGY CRUD
// ============================================================================

/// Save a custom Python strategy (user-modified copy)
#[tauri::command]
pub async fn save_custom_python_strategy(
    base_strategy_id: String,
    name: String,
    description: Option<String>,
    code: String,
    parameters: Option<String>,
    category: Option<String>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let id = format!("custom-{}", uuid::Uuid::new_v4().to_string().replace("-", "")[..12].to_uppercase());

    conn.execute(
        "INSERT INTO custom_python_strategies (id, base_strategy_id, name, description, code, parameters, category, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, CURRENT_TIMESTAMP)
         ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            code = excluded.code,
            parameters = excluded.parameters,
            category = excluded.category,
            updated_at = CURRENT_TIMESTAMP",
        rusqlite::params![
            id,
            base_strategy_id,
            name,
            description.unwrap_or_default(),
            code,
            parameters.unwrap_or_else(|| "{}".to_string()),
            category.unwrap_or_else(|| "Custom".to_string()),
        ],
    )
    .map_err(|e| format!("Failed to save custom strategy: {}", e))?;

    Ok(json!({
        "success": true,
        "id": id
    })
    .to_string())
}

/// List all custom Python strategies
#[tauri::command]
pub async fn list_custom_python_strategies() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, base_strategy_id, name, description, code, parameters, category, is_active, created_at, updated_at
             FROM custom_python_strategies ORDER BY updated_at DESC",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map([], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "base_strategy_id": row.get::<_, String>(1)?,
                "name": row.get::<_, String>(2)?,
                "description": row.get::<_, String>(3)?,
                "code": row.get::<_, String>(4)?,
                "parameters": row.get::<_, String>(5)?,
                "category": row.get::<_, String>(6)?,
                "is_active": row.get::<_, i32>(7)?,
                "created_at": row.get::<_, String>(8)?,
                "updated_at": row.get::<_, String>(9)?,
                "strategy_type": "python"
            }))
        })
        .map_err(|e| format!("Failed to query custom strategies: {}", e))?;

    let strategies: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    })
    .to_string())
}

/// Get a custom Python strategy by ID
#[tauri::command]
pub async fn get_custom_python_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let result = conn.query_row(
        "SELECT id, base_strategy_id, name, description, code, parameters, category, is_active, created_at, updated_at
         FROM custom_python_strategies WHERE id = ?1",
        rusqlite::params![id],
        |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "base_strategy_id": row.get::<_, String>(1)?,
                "name": row.get::<_, String>(2)?,
                "description": row.get::<_, String>(3)?,
                "code": row.get::<_, String>(4)?,
                "parameters": row.get::<_, String>(5)?,
                "category": row.get::<_, String>(6)?,
                "is_active": row.get::<_, i32>(7)?,
                "created_at": row.get::<_, String>(8)?,
                "updated_at": row.get::<_, String>(9)?,
                "strategy_type": "python"
            }))
        },
    );

    match result {
        Ok(strategy) => Ok(json!({
            "success": true,
            "data": strategy
        })
        .to_string()),
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Err(format!("Custom strategy {} not found", id))
        }
        Err(e) => Err(format!("Failed to get custom strategy: {}", e)),
    }
}

/// Update a custom Python strategy
#[tauri::command]
pub async fn update_custom_python_strategy(
    id: String,
    name: Option<String>,
    description: Option<String>,
    code: Option<String>,
    parameters: Option<String>,
    category: Option<String>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // Build dynamic update query
    let mut updates = vec!["updated_at = CURRENT_TIMESTAMP".to_string()];
    let mut params: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();

    if let Some(ref n) = name {
        updates.push(format!("name = ?{}", params.len() + 1));
        params.push(Box::new(n.clone()));
    }
    if let Some(ref d) = description {
        updates.push(format!("description = ?{}", params.len() + 1));
        params.push(Box::new(d.clone()));
    }
    if let Some(ref c) = code {
        updates.push(format!("code = ?{}", params.len() + 1));
        params.push(Box::new(c.clone()));
    }
    if let Some(ref p) = parameters {
        updates.push(format!("parameters = ?{}", params.len() + 1));
        params.push(Box::new(p.clone()));
    }
    if let Some(ref cat) = category {
        updates.push(format!("category = ?{}", params.len() + 1));
        params.push(Box::new(cat.clone()));
    }

    let query = format!(
        "UPDATE custom_python_strategies SET {} WHERE id = ?{}",
        updates.join(", "),
        params.len() + 1
    );

    // Use a simpler approach with direct parameters
    let affected = if name.is_some() || description.is_some() || code.is_some() || parameters.is_some() || category.is_some() {
        conn.execute(
            "UPDATE custom_python_strategies SET
             name = COALESCE(?1, name),
             description = COALESCE(?2, description),
             code = COALESCE(?3, code),
             parameters = COALESCE(?4, parameters),
             category = COALESCE(?5, category),
             updated_at = CURRENT_TIMESTAMP
             WHERE id = ?6",
            rusqlite::params![name, description, code, parameters, category, id],
        )
        .map_err(|e| format!("Failed to update custom strategy: {}", e))?
    } else {
        0
    };

    if affected == 0 {
        return Err(format!("Custom strategy {} not found or no changes made", id));
    }

    Ok(json!({
        "success": true,
        "id": id
    })
    .to_string())
}

/// Delete a custom Python strategy
#[tauri::command]
pub async fn delete_custom_python_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let affected = conn
        .execute(
            "DELETE FROM custom_python_strategies WHERE id = ?1",
            rusqlite::params![id],
        )
        .map_err(|e| format!("Failed to delete custom strategy: {}", e))?;

    if affected == 0 {
        return Err(format!("Custom strategy {} not found", id));
    }

    Ok(json!({
        "success": true,
        "id": id
    })
    .to_string())
}

/// Validate Python syntax
#[tauri::command]
pub async fn validate_python_syntax(
    app: tauri::AppHandle,
    code: String,
) -> Result<String, String> {
    // Get Python executable
    let python_path = crate::python::get_python_path(&app, None)?;

    // Create a temp file with the code
    let temp_dir = std::env::temp_dir();
    let temp_file = temp_dir.join(format!("validate_{}.py", uuid::Uuid::new_v4()));

    std::fs::write(&temp_file, &code)
        .map_err(|e| format!("Failed to write temp file: {}", e))?;

    // Run Python to check syntax
    let output = std::process::Command::new(&python_path)
        .args(["-m", "py_compile", temp_file.to_str().unwrap()])
        .output()
        .map_err(|e| format!("Failed to run Python: {}", e))?;

    // Clean up temp file
    let _ = std::fs::remove_file(&temp_file);

    let is_valid = output.status.success();
    let error_msg = if !is_valid {
        String::from_utf8_lossy(&output.stderr).to_string()
    } else {
        String::new()
    };

    Ok(json!({
        "success": true,
        "is_valid": is_valid,
        "error": error_msg
    })
    .to_string())
}

// ============================================================================
// PYTHON STRATEGY BACKTEST
// ============================================================================

/// Run backtest on a Python strategy
#[tauri::command]
pub async fn run_python_backtest(
    app: tauri::AppHandle,
    strategy_id: String,
    custom_code: Option<String>,
    symbols: Vec<String>,
    start_date: String,
    end_date: String,
    initial_cash: f64,
    parameters: std::collections::HashMap<String, String>,
    data_provider: String,
) -> Result<String, String> {
    use std::io::Write;
    use tauri::Manager;

    // Get Python executable
    let python_path = crate::python::get_python_path(&app, None)?;

    // Get backtest script path
    let resource_path = app
        .path()
        .resource_dir()
        .map_err(|e| format!("Failed to get resource dir: {}", e))?;

    let script_path = resource_path
        .join("resources")
        .join("scripts")
        .join("algo_trading")
        .join("python_backtest_engine.py");

    if !script_path.exists() {
        return Err(format!(
            "Backtest engine not found: {}",
            script_path.display()
        ));
    }

    // Build command arguments
    let mut args = vec![
        script_path.to_str().unwrap().to_string(),
        "--strategy-id".to_string(),
        strategy_id,
        "--symbols".to_string(),
        symbols.join(","),
        "--start-date".to_string(),
        start_date,
        "--end-date".to_string(),
        end_date,
        "--initial-cash".to_string(),
        initial_cash.to_string(),
        "--data-provider".to_string(),
        data_provider,
    ];

    // Add parameters as JSON
    if !parameters.is_empty() {
        let params_json = serde_json::to_string(&parameters).unwrap_or_else(|_| "{}".to_string());
        args.push("--parameters".to_string());
        args.push(params_json);
    }

    // Handle custom code - write to temp file
    let mut temp_code_file: Option<std::path::PathBuf> = None;
    if let Some(code) = custom_code {
        let temp_dir = std::env::temp_dir();
        let temp_path = temp_dir.join(format!("custom_strategy_{}.py", uuid::Uuid::new_v4()));

        std::fs::write(&temp_path, &code)
            .map_err(|e| format!("Failed to write custom code: {}", e))?;

        args.push("--custom-code".to_string());
        args.push(temp_path.to_str().unwrap().to_string());
        temp_code_file = Some(temp_path);
    }

    // Run Python backtest
    let output = std::process::Command::new(&python_path)
        .args(&args)
        .output()
        .map_err(|e| format!("Failed to run backtest: {}", e))?;

    // Clean up temp file
    if let Some(temp_path) = temp_code_file {
        let _ = std::fs::remove_file(&temp_path);
    }

    let stdout = String::from_utf8_lossy(&output.stdout);
    let stderr = String::from_utf8_lossy(&output.stderr);

    if !output.status.success() {
        return Ok(json!({
            "success": false,
            "error": format!("Backtest failed: {}", stderr),
            "debug": [stderr.to_string()]
        })
        .to_string());
    }

    // Try to parse as JSON
    if let Ok(result) = serde_json::from_str::<serde_json::Value>(&stdout) {
        Ok(result.to_string())
    } else {
        Ok(json!({
            "success": false,
            "error": format!("Failed to parse backtest output"),
            "stdout": stdout.to_string(),
            "stderr": stderr.to_string()
        })
        .to_string())
    }
}

/// Extract strategy parameters from Python code
#[tauri::command]
pub async fn extract_strategy_parameters(
    app: tauri::AppHandle,
    code: String,
) -> Result<String, String> {
    // Simple regex-based extraction of get_parameter calls
    // Pattern: self.get_parameter("name", default_value)
    use regex::Regex;

    let mut parameters = Vec::new();

    // Match patterns like: self.get_parameter("name", default) or self.get_parameter('name', default)
    let re = Regex::new(
        r#"self\.get_parameter\s*\(\s*["']([^"']+)["']\s*,\s*([^)]+)\)"#
    ).map_err(|e| format!("Regex error: {}", e))?;

    for cap in re.captures_iter(&code) {
        let name = cap.get(1).map_or("", |m| m.as_str()).to_string();
        let default_str = cap.get(2).map_or("", |m| m.as_str()).trim().to_string();

        // Infer type from default value
        let (param_type, default_value) = if default_str.parse::<i64>().is_ok() {
            ("int", default_str.clone())
        } else if default_str.parse::<f64>().is_ok() {
            ("float", default_str.clone())
        } else if default_str == "True" || default_str == "False" {
            ("bool", default_str.to_lowercase())
        } else {
            ("string", default_str.trim_matches(|c| c == '"' || c == '\'').to_string())
        };

        parameters.push(json!({
            "name": name,
            "type": param_type,
            "default_value": default_value,
            "description": ""
        }));
    }

    Ok(json!({
        "success": true,
        "data": parameters
    })
    .to_string())
}
