//! Order signal bridge for live trading
//!
//! Polls the `algo_order_signals` table for pending signals and executes them
//! via the unified trading system. Only processes signals from running deployments.

use crate::database::pool::get_db;
use serde_json::json;
use tauri::Emitter;

use std::sync::atomic::{AtomicBool, Ordering};

pub static ORDER_BRIDGE_RUNNING: AtomicBool = AtomicBool::new(false);

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

/// Poll `algo_order_signals` for pending signals and execute them.
/// Uses an atomic SELECT+UPDATE in a transaction to prevent double-processing.
/// Only processes signals from deployments that are still running.
async fn poll_and_execute_signals(
    app_handle: &tauri::AppHandle,
) -> Result<usize, String> {
    use crate::commands::unified_trading::{UnifiedOrder, unified_place_order};

    // Atomically read pending signals and mark them as 'processing' in a single transaction.
    // This prevents race conditions if multiple bridge instances run.
    let signals = tokio::task::spawn_blocking(|| -> Result<Vec<(String, String, String, String, f64, String, Option<f64>, String)>, String> {
        let conn = get_db().map_err(|e| e.to_string())?;

        // Use BEGIN IMMEDIATE to acquire a write lock upfront
        conn.execute_batch("BEGIN IMMEDIATE").map_err(|e| format!("Failed to begin transaction: {}", e))?;

        let mut stmt = conn
            .prepare(
                "SELECT s.id, s.deployment_id, s.symbol, s.side, s.quantity, s.order_type, s.price, COALESCE(d.params, '{}')
                 FROM algo_order_signals s
                 INNER JOIN algo_deployments d ON s.deployment_id = d.id
                 WHERE s.status = 'pending' AND d.status = 'running'
                 ORDER BY s.created_at ASC
                 LIMIT 10",
            )
            .map_err(|e| { let _ = conn.execute_batch("ROLLBACK"); format!("Failed to query signals: {}", e) })?;

        let rows = stmt
            .query_map([], |row| {
                Ok((
                    row.get::<_, String>(0)?,  // signal id
                    row.get::<_, String>(1)?,  // deployment_id
                    row.get::<_, String>(2)?,  // symbol
                    row.get::<_, String>(3)?,  // side
                    row.get::<_, f64>(4)?,     // quantity
                    row.get::<_, String>(5)?,  // order_type
                    row.get::<_, Option<f64>>(6)?,  // price
                    row.get::<_, String>(7)?,  // deploy params (for product_type)
                ))
            })
            .map_err(|e| { let _ = conn.execute_batch("ROLLBACK"); format!("Failed to read signals: {}", e) })?;

        let mut result = Vec::new();
        for row in rows {
            if let Ok(r) = row {
                result.push(r);
            }
        }

        // Mark all selected signals as 'processing' atomically
        for (ref signal_id, ..) in &result {
            if let Err(e) = conn.execute(
                "UPDATE algo_order_signals SET status = 'processing' WHERE id = ?1",
                rusqlite::params![signal_id],
            ) {
                eprintln!("[AlgoOrderBridge] Failed to mark signal {} as processing: {}", signal_id, e);
            }
        }

        conn.execute_batch("COMMIT").map_err(|e| format!("Failed to commit transaction: {}", e))?;

        Ok(result)
    })
    .await
    .map_err(|e| format!("Spawn blocking error: {}", e))??;

    if signals.is_empty() {
        return Ok(0);
    }

    // Track which symbols currently have a signal being processed to prevent conflicting orders
    let mut processing_symbols = std::collections::HashSet::new();
    let mut executed = 0;

    for (signal_id, deployment_id, symbol, side, quantity, order_type, price, deploy_params) in &signals {
        // Validate quantity before processing
        if *quantity <= 0.0 {
            eprintln!("[AlgoOrderBridge] Invalid quantity {} for signal {}, skipping", quantity, signal_id);
            continue;
        }

        // Concurrency check: skip if another signal for same symbol is already being processed
        let clean_sym_for_check = if symbol.contains(':') {
            symbol.splitn(2, ':').last().unwrap_or(symbol).to_string()
        } else {
            symbol.clone()
        };
        if !processing_symbols.insert(clean_sym_for_check.clone()) {
            eprintln!("[AlgoOrderBridge] Skipping signal {} — conflicting signal already processing for {}", signal_id, clean_sym_for_check);
            // Mark it back to pending so it gets picked up next cycle
            let sid = signal_id.clone();
            let _ = tokio::task::spawn_blocking(move || {
                if let Ok(conn) = get_db() {
                    let _ = conn.execute(
                        "UPDATE algo_order_signals SET status = 'pending' WHERE id = ?1",
                        rusqlite::params![sid],
                    );
                }
            }).await;
            continue;
        }

        // Emit algo_signal event to notify frontend that a condition matched
        let _ = app_handle.emit(
            "algo_signal",
            json!({
                "signal_id": signal_id,
                "deployment_id": deployment_id,
                "symbol": symbol,
                "side": side,
                "quantity": quantity,
                "order_type": order_type,
            }),
        );

        // Build the UnifiedOrder
        // Parse exchange from symbol (e.g., "NSE:RELIANCE" -> exchange="NSE", symbol="RELIANCE")
        let (exchange, clean_symbol) = if symbol.contains(':') {
            let parts: Vec<&str> = symbol.splitn(2, ':').collect();
            (parts[0].to_string(), parts[1].to_string())
        } else {
            ("NSE".to_string(), symbol.clone())
        };

        // Read product_type from deployment params instead of hardcoding
        let params_json: serde_json::Value = serde_json::from_str(deploy_params).unwrap_or_default();
        let product_type = params_json.get("product_type")
            .and_then(|v| v.as_str())
            .unwrap_or("MIS")
            .to_string();

        let order = UnifiedOrder {
            symbol: clean_symbol,
            exchange,
            side: side.to_lowercase(),
            order_type: order_type.to_lowercase(),
            quantity: *quantity,
            price: *price,
            stop_price: None,
            product_type: Some(product_type),
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
