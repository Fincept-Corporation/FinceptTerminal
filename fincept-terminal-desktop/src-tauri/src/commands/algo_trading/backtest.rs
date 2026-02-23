//! Backtesting operations

use serde_json::json;

use super::helpers::{get_algo_scripts_dir, get_main_db_path_str};

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

    let child = match Command::new("python")
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
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::piped())
        .spawn() {
        Ok(c) => c,
        Err(e) => {
            debug_log.push(format!("[backtest] ERROR: Failed to spawn Python process: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to run backtest: {}", e),
                "debug": debug_log
            }).to_string());
        }
    };

    // Wait for output with a 120-second timeout to prevent hanging
    let timeout_duration = std::time::Duration::from_secs(120);
    let output = match tokio::time::timeout(timeout_duration, tokio::task::spawn_blocking(move || child.wait_with_output())).await {
        Ok(Ok(Ok(out))) => out,
        Ok(Ok(Err(e))) => {
            debug_log.push(format!("[backtest] ERROR: Failed to read process output: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to run backtest: {}", e),
                "debug": debug_log
            }).to_string());
        }
        Ok(Err(e)) => {
            debug_log.push(format!("[backtest] ERROR: Spawn blocking error: {}", e));
            return Ok(json!({
                "success": false,
                "error": format!("Failed to run backtest: {}", e),
                "debug": debug_log
            }).to_string());
        }
        Err(_) => {
            debug_log.push("[backtest] ERROR: Backtest timed out after 120 seconds".to_string());
            return Ok(json!({
                "success": false,
                "error": "Backtest timed out after 120 seconds. Try a shorter period or simpler conditions.",
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

