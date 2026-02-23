//! Deployment lifecycle management
//!
//! Handles algo strategy deployment, monitoring, and termination.
//! Includes process reaping, PID verification, and candle aggregation cleanup.

use crate::database::pool::get_db;
use serde_json::json;

use super::helpers::{get_algo_scripts_dir, get_main_db_path_str};
use super::candles::prefetch_historical_candles;

/// Maximum number of concurrent deployments allowed (rate limiting)
const MAX_CONCURRENT_DEPLOYMENTS: i64 = 10;

/// Verify that a PID belongs to a Python algo runner process (not a recycled PID).
/// Returns true if the process appears to be our algo runner, false otherwise.
fn verify_pid_is_algo_runner(pid: i64) -> bool {
    #[cfg(target_os = "windows")]
    {
        let output = std::process::Command::new("tasklist")
            .args(["/FI", &format!("PID eq {}", pid), "/FO", "CSV", "/NH"])
            .output();
        match output {
            Ok(o) => {
                let stdout = String::from_utf8_lossy(&o.stdout).to_lowercase();
                stdout.contains("python")
            }
            Err(_) => false,
        }
    }
    #[cfg(not(target_os = "windows"))]
    {
        let cmdline_path = format!("/proc/{}/cmdline", pid);
        match std::fs::read_to_string(&cmdline_path) {
            Ok(cmdline) => cmdline.contains("algo_live_runner"),
            Err(_) => false,
        }
    }
}

/// Safely kill a process after verifying it is our algo runner.
/// Returns true if the process was killed, false if verification failed or kill failed.
fn safe_kill_algo_process(pid: i64) -> bool {
    if !verify_pid_is_algo_runner(pid) {
        eprintln!("[AlgoDeploy] PID {} is not a Python algo runner, skipping kill", pid);
        return false;
    }

    #[cfg(target_os = "windows")]
    {
        let result = std::process::Command::new("taskkill")
            .args(["/F", "/PID", &pid.to_string()])
            .output();
        match result {
            Ok(o) => o.status.success(),
            Err(e) => {
                eprintln!("[AlgoDeploy] Failed to kill PID {}: {}", pid, e);
                false
            }
        }
    }
    #[cfg(not(target_os = "windows"))]
    {
        let result = std::process::Command::new("kill")
            .arg(pid.to_string())
            .output();
        match result {
            Ok(o) => o.status.success(),
            Err(e) => {
                eprintln!("[AlgoDeploy] Failed to kill PID {}: {}", pid, e);
                false
            }
        }
    }
}

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

    // Validate quantity - must be positive
    if deploy_qty <= 0.0 {
        return Ok(json!({
            "success": false,
            "error": "Quantity must be greater than 0"
        }).to_string());
    }

    println!("[AlgoDeploy] ====================================================");
    println!("[AlgoDeploy] Strategy: {}, Symbol: {}, Mode: {}, TF: {}, Qty: {}", strategy_id, symbol, deploy_mode, deploy_timeframe, deploy_qty);
    println!("[AlgoDeploy] Provider: '{}', Params: {}", deploy_provider, deploy_params);

    // Rate limit: max concurrent deployments
    let conn = get_db().map_err(|e| e.to_string())?;
    let running_count: i64 = conn.query_row(
        "SELECT COUNT(*) FROM algo_deployments WHERE status IN ('running', 'starting')",
        [],
        |r| r.get(0),
    ).unwrap_or(0);
    if running_count >= MAX_CONCURRENT_DEPLOYMENTS {
        return Ok(json!({
            "success": false,
            "error": format!("Maximum {} concurrent deployments allowed. Stop an existing deployment first.", MAX_CONCURRENT_DEPLOYMENTS)
        }).to_string());
    }

    // Generate deployment ID
    let deploy_id = format!("algo-{}", uuid::Uuid::new_v4());
    println!("[AlgoDeploy] Deploy ID: {}", deploy_id);
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
        if let Err(e) = conn.execute(
            "UPDATE algo_deployments SET status = 'error', error_message = 'Runner script not found' WHERE id = ?1",
            rusqlite::params![deploy_id],
        ) {
            eprintln!("[AlgoDeploy] DB update failed: {}", e);
        }
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
        Ok(mut child) => {
            let pid = child.id();
            println!("[AlgoDeploy] Python runner spawned OK, PID={}", pid);

            // Update deployment with PID and running status
            if let Err(e) = conn.execute(
                "UPDATE algo_deployments SET status = 'running', pid = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
                rusqlite::params![pid as i64, deploy_id],
            ) {
                eprintln!("[AlgoDeploy] DB update failed: {}", e);
            }

            // Spawn a background reaper task to wait for the child process.
            // This prevents zombie processes and auto-updates DB status if the runner crashes.
            let reaper_deploy_id = deploy_id.clone();
            tokio::spawn(async move {
                let exit_status = tokio::task::spawn_blocking(move || child.wait()).await;
                match exit_status {
                    Ok(Ok(status)) => {
                        let new_status = if status.success() { "stopped" } else { "error" };
                        let error_msg = if status.success() {
                            None
                        } else {
                            Some(format!("Runner exited with code: {:?}", status.code()))
                        };
                        println!("[AlgoReaper] Deployment {} runner exited: {:?}", reaper_deploy_id, status);
                        if let Ok(conn) = get_db() {
                            // Only update if still marked as 'running' (avoid overwriting manual stop)
                            if let Err(e) = conn.execute(
                                "UPDATE algo_deployments SET status = ?1, error_message = COALESCE(?2, error_message), updated_at = CURRENT_TIMESTAMP WHERE id = ?3 AND status = 'running'",
                                rusqlite::params![new_status, error_msg, reaper_deploy_id],
                            ) {
                                eprintln!("[AlgoReaper] DB update failed for {}: {}", reaper_deploy_id, e);
                            }
                        }
                    }
                    Ok(Err(e)) => {
                        eprintln!("[AlgoReaper] Failed to wait for deployment {}: {}", reaper_deploy_id, e);
                    }
                    Err(e) => {
                        eprintln!("[AlgoReaper] Spawn blocking error for {}: {}", reaper_deploy_id, e);
                    }
                }
            });

            // Auto-start candle aggregation for this symbol+timeframe
            // IMPORTANT: For Indian brokers, normalize symbol format to match WebSocket ticks
            // WebSocket may send "NSE:RELIANCE" while we deploy with "RELIANCE"
            // Candle aggregator already has fuzzy matching, but we also subscribe to both variants
            println!("[AlgoDeploy] Starting candle aggregation for {} @ {}", symbol, deploy_timeframe);
            let services = state.services.read().await;
            services
                .candle_aggregator
                .add_subscription(symbol.clone(), deploy_timeframe.clone())
                .await;

            // Also subscribe to exchange-prefixed variant if this is an Indian stock
            if deploy_provider == "angelone" || deploy_provider == "fyers" || deploy_provider == "zerodha" {
                let params_json: serde_json::Value = serde_json::from_str(&deploy_params).unwrap_or_default();
                let exchange = params_json.get("exchange").and_then(|v| v.as_str()).unwrap_or("NSE");
                let prefixed_symbol = format!("{}:{}", exchange, symbol);
                services
                    .candle_aggregator
                    .add_subscription(prefixed_symbol.clone(), deploy_timeframe.clone())
                    .await;
                println!("[AlgoDeploy] Candle aggregation subscriptions added: {} and {}", symbol, prefixed_symbol);
            } else {
                println!("[AlgoDeploy] Candle aggregation subscription added: {}", symbol);
            }

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
            if deploy_mode == "live" {
                let _ = super::order_bridge::start_order_signal_bridge(app.clone()).await;
            }

            Ok(json!({
                "success": true,
                "deploy_id": deploy_id,
                "pid": pid
            }).to_string())
        }
        Err(e) => {
            if let Err(db_err) = conn.execute(
                "UPDATE algo_deployments SET status = 'error', error_message = ?1 WHERE id = ?2",
                rusqlite::params![format!("Failed to spawn: {}", e), deploy_id],
            ) {
                eprintln!("[AlgoDeploy] DB update failed: {}", db_err);
            }
            Ok(json!({
                "success": false,
                "error": format!("Failed to spawn runner: {}", e)
            }).to_string())
        }
    }
}

/// Stop a running algo deployment (kill process, cleanup candle subscriptions, update DB)
#[tauri::command]
pub async fn stop_algo_deployment(
    state: tauri::State<'_, crate::WebSocketState>,
    deploy_id: String,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    // Get PID, symbol, and timeframe from deployment record
    let deploy_info: Result<(Option<i64>, String, String), _> = conn.query_row(
        "SELECT pid, symbol, timeframe FROM algo_deployments WHERE id = ?1 AND status = 'running'",
        rusqlite::params![deploy_id],
        |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?)),
    );

    match deploy_info {
        Ok((pid, symbol, timeframe)) => {
            if let Some(pid) = pid {
                // Verify PID is our runner before killing (prevents killing recycled PIDs)
                safe_kill_algo_process(pid);
            }

            // Cleanup candle aggregation subscriptions
            let services = state.services.read().await;
            services.candle_aggregator.remove_subscription(symbol.clone(), timeframe.clone()).await;
            // Also remove exchange-prefixed variant
            let prefixed = format!("NSE:{}", symbol);
            services.candle_aggregator.remove_subscription(prefixed, timeframe).await;

            // Update status
            conn.execute(
                "UPDATE algo_deployments SET status = 'stopped', stopped_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
                rusqlite::params![deploy_id],
            )
            .map_err(|e| format!("Failed to update deployment: {}", e))?;

            Ok(json!({ "success": true, "stopped": true }).to_string())
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
        safe_kill_algo_process(*pid);

        if let Err(e) = conn.execute(
            "UPDATE algo_deployments SET status = 'stopped', stopped_at = CURRENT_TIMESTAMP, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
            rusqlite::params![id],
        ) {
            eprintln!("[AlgoStopAll] DB update failed for {}: {}", id, e);
        }
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
            // Kill process if still running (with PID verification)
            if status == "running" {
                if let Some(pid) = pid {
                    safe_kill_algo_process(pid);
                }
            }

            // Delete associated records (trades, metrics, order signals)
            if let Err(e) = conn.execute(
                "DELETE FROM algo_trades WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            ) {
                eprintln!("[AlgoDelete] Failed to delete trades for {}: {}", deploy_id, e);
            }
            if let Err(e) = conn.execute(
                "DELETE FROM algo_metrics WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            ) {
                eprintln!("[AlgoDelete] Failed to delete metrics for {}: {}", deploy_id, e);
            }
            if let Err(e) = conn.execute(
                "DELETE FROM algo_order_signals WHERE deployment_id = ?1",
                rusqlite::params![deploy_id],
            ) {
                eprintln!("[AlgoDelete] Failed to delete signals for {}: {}", deploy_id, e);
            }

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

    let deployments: Vec<serde_json::Value> = rows
        .filter_map(|r| match r {
            Ok(v) => Some(v),
            Err(e) => {
                eprintln!("[AlgoList] Failed to parse deployment row: {}", e);
                None
            }
        })
        .collect();

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

