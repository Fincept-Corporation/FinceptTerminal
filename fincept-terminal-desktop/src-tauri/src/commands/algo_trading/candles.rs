//! Candle cache and aggregation operations

use crate::database::pool::get_db;
use serde_json::json;
use super::helpers::{get_algo_scripts_dir, get_main_db_path_str};

/// Pre-fetch historical candles from broker API and insert into candle_cache table.
/// Maps timeframe strings to Fyers resolution values and fetches data based on lookback.
/// If lookback_days is None, uses default lookback based on timeframe.
/// Returns a Vec of debug log lines for the caller to include in the response.
pub(super) async fn prefetch_historical_candles(
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

    let child = Command::new("python")
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
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to evaluate conditions: {}", e))?;

    // Wait with a 30-second timeout to prevent hanging
    let timeout_duration = std::time::Duration::from_secs(30);
    let output = match tokio::time::timeout(timeout_duration, tokio::task::spawn_blocking(move || child.wait_with_output())).await {
        Ok(Ok(Ok(out))) => out,
        Ok(Ok(Err(e))) => {
            return Ok(json!({ "success": false, "error": format!("Process error: {}", e) }).to_string());
        }
        Ok(Err(e)) => {
            return Ok(json!({ "success": false, "error": format!("Spawn error: {}", e) }).to_string());
        }
        Err(_) => {
            return Ok(json!({ "success": false, "error": "Condition evaluation timed out after 30 seconds" }).to_string());
        }
    };

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

