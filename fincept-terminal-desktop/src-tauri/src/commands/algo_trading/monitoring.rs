//! Monitoring and diagnostics

use crate::database::pool::get_db;
use serde_json::json;
use super::order_bridge::ORDER_BRIDGE_RUNNING;
use std::sync::atomic::Ordering;

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

