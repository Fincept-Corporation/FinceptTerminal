use finscript::{FinScriptResult, OhlcvSeries};
use crate::data_sources::yfinance::{YFinanceProvider, HistoricalData};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Configuration for live data fetch
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DataConfig {
    pub start_date: String,
    pub end_date: String,
}

/// Execute FinScript with synthetic demo data (backward compatible)
#[tauri::command]
pub async fn execute_finscript(
    _app: tauri::AppHandle,
    code: String,
) -> Result<FinScriptResult, String> {
    tokio::task::spawn_blocking(move || finscript::execute(&code))
        .await
        .map_err(|e| format!("FinScript execution error: {}", e))
}

/// Execute FinScript with real market data from yfinance
#[tauri::command]
pub async fn execute_finscript_live(
    app: tauri::AppHandle,
    code: String,
    data_config: DataConfig,
) -> Result<FinScriptResult, String> {
    // Step 1: Extract symbols from code
    let symbols = finscript::extract_symbols(&code)
        .map_err(|e| format!("Symbol extraction failed: {}", e))?;

    if symbols.is_empty() {
        // No symbols referenced; run with empty data
        return tokio::task::spawn_blocking(move || {
            finscript::execute_with_data(&code, HashMap::new())
        })
        .await
        .map_err(|e| format!("FinScript execution error: {}", e));
    }

    // Step 2: Fetch real OHLCV data for each symbol
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;
    let mut symbol_data: HashMap<String, OhlcvSeries> = HashMap::new();

    for symbol in &symbols {
        match provider
            .get_historical(symbol, &data_config.start_date, &data_config.end_date)
            .await
        {
            Some(hist) => {
                symbol_data.insert(symbol.clone(), historical_to_ohlcv(symbol, hist));
            }
            None => {
                return Err(format!(
                    "Failed to fetch market data for '{}'. Check symbol name and date range.",
                    symbol
                ));
            }
        }
    }

    // Step 3: Execute with real data on a blocking thread
    tokio::task::spawn_blocking(move || finscript::execute_with_data(&code, symbol_data))
        .await
        .map_err(|e| format!("FinScript execution error: {}", e))
}

/// Result for a single symbol in batch mode
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BatchResult {
    pub symbol: String,
    pub result: FinScriptResult,
}

/// Execute FinScript against multiple symbols (screener/batch mode).
/// Runs the script once per symbol with that symbol's data loaded.
#[tauri::command]
pub async fn execute_finscript_batch(
    app: tauri::AppHandle,
    code: String,
    symbols: Vec<String>,
    data_config: DataConfig,
) -> Result<Vec<BatchResult>, String> {
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;
    let mut results = Vec::new();

    for symbol in &symbols {
        let mut symbol_data: HashMap<String, OhlcvSeries> = HashMap::new();

        match provider
            .get_historical(symbol, &data_config.start_date, &data_config.end_date)
            .await
        {
            Some(hist) => {
                symbol_data.insert(symbol.clone(), historical_to_ohlcv(symbol, hist));
            }
            None => {
                results.push(BatchResult {
                    symbol: symbol.clone(),
                    result: FinScriptResult {
                        success: false,
                        output: String::new(),
                        signals: vec![],
                        plots: vec![],
                        errors: vec![format!("Failed to fetch data for {}", symbol)],
                        alerts: vec![],
                        drawings: vec![],
                        integration_actions: vec![],
                        execution_time_ms: 0,
                    },
                });
                continue;
            }
        }

        let code_clone = code.clone();
        let res = tokio::task::spawn_blocking(move || {
            finscript::execute_with_data(&code_clone, symbol_data)
        })
        .await
        .map_err(|e| format!("Execution error: {}", e))?;

        results.push(BatchResult {
            symbol: symbol.clone(),
            result: res,
        });
    }

    Ok(results)
}

/// Convert Vec<HistoricalData> (from yfinance) to OhlcvSeries (for finscript)
fn historical_to_ohlcv(symbol: &str, data: Vec<HistoricalData>) -> OhlcvSeries {
    let len = data.len();
    let mut timestamps = Vec::with_capacity(len);
    let mut open = Vec::with_capacity(len);
    let mut high = Vec::with_capacity(len);
    let mut low = Vec::with_capacity(len);
    let mut close = Vec::with_capacity(len);
    let mut volume = Vec::with_capacity(len);

    for d in data {
        timestamps.push(d.timestamp);
        open.push(d.open);
        high.push(d.high);
        low.push(d.low);
        close.push(d.close);
        volume.push(d.volume as f64);
    }

    OhlcvSeries {
        symbol: symbol.to_string(),
        timestamps,
        open,
        high,
        low,
        close,
        volume,
    }
}
