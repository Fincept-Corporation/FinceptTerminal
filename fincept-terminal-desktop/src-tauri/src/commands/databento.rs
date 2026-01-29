// Databento market data commands
// Provides institutional-grade market data for Surface Analytics
use crate::python;

/// Test Databento API connection
/// Frontend sends: { apiKey: "..." }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_test_connection(
    app: tauri::AppHandle,
    apiKey: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["test_connection".to_string(), args_json]).await
}

/// Fetch options chain for volatility surface (3D Visualization)
/// Frontend sends: { apiKey: "...", symbol: "SPY", date: "2024-01-15" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_options_chain(
    app: tauri::AppHandle,
    apiKey: String,
    symbol: String,
    date: Option<String>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "symbol": symbol,
        "date": date
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["options_chain".to_string(), args_json]).await
}

/// Fetch options definitions (instrument metadata)
/// Frontend sends: { apiKey: "...", symbol: "SPY", date: "2024-01-15" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_options_definitions(
    app: tauri::AppHandle,
    apiKey: String,
    symbol: String,
    date: Option<String>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "symbol": symbol,
        "date": date
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["options_definitions".to_string(), args_json]).await
}

/// Fetch multi-asset historical OHLCV data for correlation matrix (3D Visualization)
/// Frontend sends: { apiKey: "...", symbols: ["SPY", "QQQ"], days: 60, schema: "ohlcv-1d" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_multi_asset_data(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    days: Option<i32>,
    schema: Option<String>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(60),
        "schema": schema.unwrap_or_else(|| "ohlcv-1d".to_string())
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["historical_ohlcv".to_string(), args_json]).await
}

/// Fetch historical OHLCV data for specific symbols
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], days: 30, dataset: "DBEQ.BASIC", schema: "ohlcv-1d" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_historical_ohlcv(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    days: Option<i32>,
    dataset: Option<String>,
    schema: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(30),
        "schema": schema.unwrap_or_else(|| "ohlcv-1d".to_string())
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["historical_ohlcv".to_string(), args.to_string()]).await
}

// ============================================================================
// Legacy commands for backward compatibility with explore_databento_data.py
// ============================================================================

/// Execute Databento Python script command (legacy)
#[tauri::command]
pub async fn execute_databento_command(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    python::execute(&app, "explore_databento_data.py", cmd_args).await
}

/// Get market data (legacy)
#[tauri::command]
pub async fn get_databento_market_data(
    app: tauri::AppHandle,
    symbol: String,
    dataset: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol];
    if let Some(ds) = dataset {
        args.push(ds);
    }
    execute_databento_command(app, "market_data".to_string(), args).await
}

/// Get available datasets (legacy)
#[tauri::command]
pub async fn get_databento_datasets(
    app: tauri::AppHandle,
) -> Result<String, String> {
    execute_databento_command(app, "datasets".to_string(), vec![]).await
}

/// Get historical data (legacy)
#[tauri::command]
pub async fn get_databento_historical(
    app: tauri::AppHandle,
    symbol: String,
    start_date: String,
    end_date: Option<String>,
) -> Result<String, String> {
    let mut args = vec![symbol, start_date];
    if let Some(end) = end_date {
        args.push(end);
    }
    execute_databento_command(app, "historical".to_string(), args).await
}

/// Resolve symbols using Databento symbology API
/// Frontend sends: { apiKey: "...", symbols: ["SPY", "AAPL"], dataset: "XNAS.ITCH" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_resolve_symbols(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    stypeIn: Option<String>,
    stypeOut: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }
    if let Some(stype) = stypeIn {
        args["stype_in"] = serde_json::Value::String(stype);
    }
    if let Some(stype) = stypeOut {
        args["stype_out"] = serde_json::Value::String(stype);
    }

    python::execute(&app, "databento_provider.py", vec!["resolve_symbols".to_string(), args.to_string()]).await
}

/// List all available datasets
/// Frontend sends: { apiKey: "..." }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_list_datasets(
    app: tauri::AppHandle,
    apiKey: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["list_datasets".to_string(), args_json]).await
}

/// Get dataset info including date range and available schemas
/// Frontend sends: { apiKey: "...", dataset: "XNAS.ITCH" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_dataset_info(
    app: tauri::AppHandle,
    apiKey: String,
    dataset: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "dataset": dataset
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["get_dataset_info".to_string(), args_json]).await
}

/// Get cost estimate before executing a query
/// Frontend sends: { apiKey: "...", dataset: "XNAS.ITCH", symbols: ["SPY"], schema: "ohlcv-1d", startDate: "2024-01-01", endDate: "2024-01-31" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_cost_estimate(
    app: tauri::AppHandle,
    apiKey: String,
    dataset: String,
    symbols: Vec<String>,
    schema: String,
    startDate: String,
    endDate: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "dataset": dataset,
        "symbols": symbols,
        "schema": schema,
        "start_date": startDate,
        "end_date": endDate
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["get_cost_estimate".to_string(), args_json]).await
}

/// List available schemas for a dataset
/// Frontend sends: { apiKey: "...", dataset: "XNAS.ITCH" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_list_schemas(
    app: tauri::AppHandle,
    apiKey: String,
    dataset: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["list_schemas".to_string(), args.to_string()]).await
}

// ============================================================================
// Reference Data API Commands
// ============================================================================

/// Get security master data (instrument metadata)
/// Frontend sends: { apiKey: "...", symbols: ["SPY", "AAPL"], dataset: "XNAS.ITCH" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_security_master(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["security_master".to_string(), args.to_string()]).await
}

/// Get corporate actions (dividends, splits, etc.)
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], startDate: "2024-01-01", endDate: "2024-12-31", actionType: "dividend" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_corporate_actions(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    startDate: Option<String>,
    endDate: Option<String>,
    actionType: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols
    });

    if let Some(start) = startDate {
        args["start_date"] = serde_json::Value::String(start);
    }
    if let Some(end) = endDate {
        args["end_date"] = serde_json::Value::String(end);
    }
    if let Some(action) = actionType {
        args["action_type"] = serde_json::Value::String(action);
    }

    python::execute(&app, "databento_provider.py", vec!["corporate_actions".to_string(), args.to_string()]).await
}

/// Get adjustment factors for historical price adjustments
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], startDate: "2020-01-01", endDate: "2024-12-31" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_adjustment_factors(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    startDate: Option<String>,
    endDate: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols
    });

    if let Some(start) = startDate {
        args["start_date"] = serde_json::Value::String(start);
    }
    if let Some(end) = endDate {
        args["end_date"] = serde_json::Value::String(end);
    }

    python::execute(&app, "databento_provider.py", vec!["adjustment_factors".to_string(), args.to_string()]).await
}

// ============================================================================
// Order Book API Commands
// ============================================================================

/// Get order book data (MBP or MBO schemas)
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", schema: "mbp-10", startTime: "2024-01-15T09:30:00", durationSeconds: 60 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_order_book(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    schema: Option<String>,
    startTime: Option<String>,
    durationSeconds: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "schema": schema.unwrap_or_else(|| "mbp-1".to_string()),
        "duration_seconds": durationSeconds.unwrap_or(60)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }
    if let Some(start) = startTime {
        args["start_time"] = serde_json::Value::String(start);
    }

    python::execute(&app, "databento_provider.py", vec!["order_book".to_string(), args.to_string()]).await
}

/// Get order book snapshot at a specific time
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", levels: 10, snapshotTime: "2024-01-15T15:59:00" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_order_book_snapshot(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    levels: Option<i32>,
    snapshotTime: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "levels": levels.unwrap_or(10)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }
    if let Some(time) = snapshotTime {
        args["snapshot_time"] = serde_json::Value::String(time);
    }

    python::execute(&app, "databento_provider.py", vec!["order_book_snapshot".to_string(), args.to_string()]).await
}

// ============================================================================
// Live Streaming API Commands
// ============================================================================

use std::sync::Mutex;
use std::collections::HashMap;
use std::process::{Child, Command, Stdio};
use std::io::{BufRead, BufReader};
use tauri::{Emitter, Manager};
use once_cell::sync::Lazy;

// Global storage for live stream processes
static LIVE_STREAMS: Lazy<Mutex<HashMap<String, Child>>> = Lazy::new(|| Mutex::new(HashMap::new()));

/// Get live streaming info (available schemas, datasets, etc.)
/// Frontend sends: { apiKey: "..." }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_live_info(
    app: tauri::AppHandle,
) -> Result<String, String> {
    python::execute(&app, "databento_live.py", vec!["info".to_string()]).await
}

/// Start live data streaming
/// Frontend sends: { apiKey: "...", dataset: "XNAS.ITCH", schema: "trades", symbols: ["SPY"], stypeIn: "raw_symbol", snapshot: false }
/// Returns stream_id that can be used to stop the stream
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_live_start(
    app: tauri::AppHandle,
    apiKey: String,
    dataset: String,
    schema: String,
    symbols: Vec<String>,
    stypeIn: Option<String>,
    snapshot: Option<bool>,
) -> Result<String, String> {
    use std::thread;

    // Generate unique stream ID
    let stream_id = format!("stream_{}_{}", dataset.replace('.', "_"), chrono::Utc::now().timestamp_millis());

    let args = serde_json::json!({
        "api_key": apiKey,
        "dataset": dataset,
        "schema": schema,
        "symbols": symbols,
        "stype_in": stypeIn.unwrap_or_else(|| "raw_symbol".to_string()),
        "snapshot": snapshot.unwrap_or(false)
    });

    // Get Python path from resources
    let python_path = app.path().resolve("resources/python/python.exe", tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to resolve Python path: {}", e))?;

    let script_path = app.path().resolve("resources/scripts/databento_live.py", tauri::path::BaseDirectory::Resource)
        .map_err(|e| format!("Failed to resolve script path: {}", e))?;

    // Spawn the live streaming process
    let mut child = Command::new(&python_path)
        .arg(&script_path)
        .arg("start")
        .arg(args.to_string())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to start live stream: {}", e))?;

    let stdout = child.stdout.take()
        .ok_or_else(|| "Failed to capture stdout".to_string())?;

    // Clone values for the thread
    let app_clone = app.clone();
    let stream_id_clone = stream_id.clone();

    // Spawn thread to read stdout and emit events
    thread::spawn(move || {
        let reader = BufReader::new(stdout);
        for line in reader.lines() {
            match line {
                Ok(data) => {
                    // Parse JSON and emit as Tauri event
                    if let Ok(json) = serde_json::from_str::<serde_json::Value>(&data) {
                        let event_name = format!("databento-live-{}", stream_id_clone);
                        let _ = app_clone.emit(&event_name, json);
                    }
                }
                Err(_) => break,
            }
        }
        // Stream ended
        let _ = app_clone.emit(&format!("databento-live-{}", stream_id_clone), serde_json::json!({
            "type": "status",
            "message": "Stream ended",
            "connected": false
        }));
    });

    // Store the child process
    if let Ok(mut streams) = LIVE_STREAMS.lock() {
        streams.insert(stream_id.clone(), child);
    }

    Ok(serde_json::json!({
        "error": false,
        "stream_id": stream_id,
        "dataset": dataset,
        "schema": schema,
        "symbols": symbols,
        "message": "Stream started"
    }).to_string())
}

/// Stop a live data stream
/// Frontend sends: { streamId: "stream_XNAS_ITCH_123456" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_live_stop(
    streamId: String,
) -> Result<String, String> {
    if let Ok(mut streams) = LIVE_STREAMS.lock() {
        if let Some(mut child) = streams.remove(&streamId) {
            // Try to kill the process gracefully
            let _ = child.kill();
            let _ = child.wait();
            return Ok(serde_json::json!({
                "error": false,
                "stream_id": streamId,
                "message": "Stream stopped"
            }).to_string());
        }
    }

    Err(format!("Stream not found: {}", streamId))
}

/// List all active live streams
#[tauri::command]
pub async fn databento_live_list() -> Result<String, String> {
    let active_streams: Vec<String> = if let Ok(streams) = LIVE_STREAMS.lock() {
        streams.keys().cloned().collect()
    } else {
        vec![]
    };

    Ok(serde_json::json!({
        "error": false,
        "streams": active_streams,
        "count": active_streams.len()
    }).to_string())
}

/// Stop all active live streams
#[tauri::command]
pub async fn databento_live_stop_all() -> Result<String, String> {
    let mut stopped = 0;

    if let Ok(mut streams) = LIVE_STREAMS.lock() {
        for (_, mut child) in streams.drain() {
            let _ = child.kill();
            let _ = child.wait();
            stopped += 1;
        }
    }

    Ok(serde_json::json!({
        "error": false,
        "message": format!("Stopped {} streams", stopped),
        "stopped": stopped
    }).to_string())
}

// ============================================================================
// Futures API Commands (GLBX.MDP3 dataset, SMART symbology)
// ============================================================================

/// List available futures contracts with specifications
/// Frontend sends: { apiKey: "..." }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_list_futures(
    app: tauri::AppHandle,
    apiKey: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["list_futures".to_string(), args_json]).await
}

/// Fetch futures data using SMART symbology
/// Frontend sends: { apiKey: "...", symbols: ["ES.c.0", "NQ.c.0"], days: 30, schema: "ohlcv-1d", stypeIn: "smart" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_futures_data(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    days: Option<i32>,
    schema: Option<String>,
    stypeIn: Option<String>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(30),
        "schema": schema.unwrap_or_else(|| "ohlcv-1d".to_string()),
        "stype_in": stypeIn.unwrap_or_else(|| "smart".to_string())
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["futures_data".to_string(), args_json]).await
}

/// Get futures contract definitions
/// Frontend sends: { apiKey: "...", symbols: ["ES.c.0"], date: "2024-01-15" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_futures_definitions(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    date: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols
    });

    if let Some(d) = date {
        args["date"] = serde_json::Value::String(d);
    }

    python::execute(&app, "databento_provider.py", vec!["futures_definitions".to_string(), args.to_string()]).await
}

/// Get futures term structure (curve)
/// Frontend sends: { apiKey: "...", rootSymbol: "ES", numContracts: 6 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_futures_term_structure(
    app: tauri::AppHandle,
    apiKey: String,
    rootSymbol: String,
    numContracts: Option<i32>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "root_symbol": rootSymbol,
        "num_contracts": numContracts.unwrap_or(6)
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["futures_term_structure".to_string(), args_json]).await
}

// ============================================================================
// Batch Download API Commands
// ============================================================================

/// Submit a batch download job for large data requests
/// Frontend sends: { apiKey: "...", dataset: "XNAS.ITCH", symbols: ["SPY"], schema: "trades", startDate: "2024-01-01", endDate: "2024-01-31", encoding: "dbn", compression: "zstd", stypeIn: "raw_symbol", splitDuration: "day", delivery: "download" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_submit_batch_job(
    app: tauri::AppHandle,
    apiKey: String,
    dataset: String,
    symbols: Vec<String>,
    schema: String,
    startDate: String,
    endDate: String,
    encoding: Option<String>,
    compression: Option<String>,
    stypeIn: Option<String>,
    splitDuration: Option<String>,
    delivery: Option<String>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "dataset": dataset,
        "symbols": symbols,
        "schema": schema,
        "start_date": startDate,
        "end_date": endDate,
        "encoding": encoding.unwrap_or_else(|| "dbn".to_string()),
        "compression": compression.unwrap_or_else(|| "zstd".to_string()),
        "stype_in": stypeIn.unwrap_or_else(|| "raw_symbol".to_string()),
        "split_duration": splitDuration.unwrap_or_else(|| "day".to_string()),
        "delivery": delivery.unwrap_or_else(|| "download".to_string())
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["submit_batch_job".to_string(), args_json]).await
}

/// List batch download jobs
/// Frontend sends: { apiKey: "...", states: "queued,processing,done", sinceDays: 7 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_list_batch_jobs(
    app: tauri::AppHandle,
    apiKey: String,
    states: Option<String>,
    sinceDays: Option<i32>,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "states": states.unwrap_or_else(|| "queued,processing,done".to_string()),
        "since_days": sinceDays.unwrap_or(7)
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["list_batch_jobs".to_string(), args_json]).await
}

/// List files from a batch job
/// Frontend sends: { apiKey: "...", jobId: "job_123" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_list_batch_files(
    app: tauri::AppHandle,
    apiKey: String,
    jobId: String,
) -> Result<String, String> {
    let args_json = serde_json::json!({
        "api_key": apiKey,
        "job_id": jobId
    }).to_string();

    python::execute(&app, "databento_provider.py", vec!["list_batch_files".to_string(), args_json]).await
}

/// Download files from a completed batch job
/// Frontend sends: { apiKey: "...", jobId: "job_123", outputDir: "/path/to/dir", filename: "specific_file.dbn" }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_download_batch_job(
    app: tauri::AppHandle,
    apiKey: String,
    jobId: String,
    outputDir: Option<String>,
    filename: Option<String>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "job_id": jobId
    });

    if let Some(dir) = outputDir {
        args["output_dir"] = serde_json::Value::String(dir);
    }
    if let Some(file) = filename {
        args["filename"] = serde_json::Value::String(file);
    }

    python::execute(&app, "databento_provider.py", vec!["download_batch_job".to_string(), args.to_string()]).await
}

// ============================================================================
// Additional Schemas API Commands (TRADES, IMBALANCE, STATISTICS, STATUS)
// ============================================================================

/// Get trades data for symbols
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", days: 1, limit: 1000 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_trades(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    days: Option<i32>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(1),
        "limit": limit.unwrap_or(1000)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["trades".to_string(), args.to_string()]).await
}

/// Get auction imbalance data (opening/closing imbalances)
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", days: 1 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_imbalance(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    days: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(1)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["imbalance".to_string(), args.to_string()]).await
}

/// Get exchange statistics (trading halts, circuit breakers)
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", days: 5 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_statistics(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    days: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(5)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["statistics".to_string(), args.to_string()]).await
}

/// Get instrument status (trading status, reg SHO)
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], dataset: "XNAS.ITCH", days: 5 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_status(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    dataset: Option<String>,
    days: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "days": days.unwrap_or(5)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["status".to_string(), args.to_string()]).await
}

/// Generic schema data fetcher
/// Frontend sends: { apiKey: "...", symbols: ["SPY"], schema: "trades", dataset: "XNAS.ITCH", days: 1, limit: 1000 }
#[tauri::command]
#[allow(non_snake_case)]
pub async fn databento_get_schema_data(
    app: tauri::AppHandle,
    apiKey: String,
    symbols: Vec<String>,
    schema: String,
    dataset: Option<String>,
    days: Option<i32>,
    limit: Option<i32>,
) -> Result<String, String> {
    let mut args = serde_json::json!({
        "api_key": apiKey,
        "symbols": symbols,
        "schema": schema,
        "days": days.unwrap_or(1),
        "limit": limit.unwrap_or(1000)
    });

    if let Some(ds) = dataset {
        args["dataset"] = serde_json::Value::String(ds);
    }

    python::execute(&app, "databento_provider.py", vec!["schema_data".to_string(), args.to_string()]).await
}
