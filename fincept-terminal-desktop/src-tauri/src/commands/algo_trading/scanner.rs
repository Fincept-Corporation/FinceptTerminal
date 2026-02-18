//! Market scanner operations

use serde_json::json;

use super::helpers::{get_algo_scripts_dir, get_main_db_path_str};
use super::candles::prefetch_historical_candles;

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
