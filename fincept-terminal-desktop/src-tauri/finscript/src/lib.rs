pub mod ast;
pub mod indicators;
pub mod interpreter;
pub mod lexer;
pub mod parser;
pub mod types;

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::time::Instant;

// Re-export core types for consumers
pub use types::OhlcvSeries;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct IntegrationAction {
    pub action_type: String,
    pub payload: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FinScriptResult {
    pub success: bool,
    pub output: String,
    pub signals: Vec<Signal>,
    pub plots: Vec<PlotData>,
    pub errors: Vec<String>,
    pub alerts: Vec<AlertInfo>,
    pub drawings: Vec<DrawingInfo>,
    pub integration_actions: Vec<IntegrationAction>,
    pub execution_time_ms: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Signal {
    pub signal_type: String,
    pub message: String,
    pub timestamp: String,
    pub price: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlotData {
    pub plot_type: String,
    pub label: String,
    pub data: Vec<PlotPoint>,
    pub color: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlotPoint {
    pub timestamp: i64,
    pub value: Option<f64>,
    pub open: Option<f64>,
    pub high: Option<f64>,
    pub low: Option<f64>,
    pub close: Option<f64>,
    pub volume: Option<f64>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlertInfo {
    pub message: String,
    pub alert_type: String,
    pub timestamp: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DrawingInfo {
    pub drawing_type: String,
    pub x1: i64,
    pub y1: f64,
    pub x2: i64,
    pub y2: f64,
    pub color: String,
    pub text: String,
    pub style: String,
    pub width: f64,
}

fn build_error_result(start: Instant, msg: String) -> FinScriptResult {
    FinScriptResult {
        success: false,
        output: String::new(),
        signals: vec![],
        plots: vec![],
        errors: vec![msg],
        alerts: vec![],
        drawings: vec![],
        integration_actions: vec![],
        execution_time_ms: start.elapsed().as_millis() as u64,
    }
}

fn build_result(result: interpreter::InterpreterResult, start: Instant) -> FinScriptResult {
    FinScriptResult {
        success: result.errors.is_empty(),
        output: result.output,
        signals: result.signals,
        plots: result.plots,
        errors: result.errors,
        alerts: result.alerts,
        drawings: result.drawings,
        integration_actions: result.integration_actions,
        execution_time_ms: start.elapsed().as_millis() as u64,
    }
}

/// Execute a FinScript program with synthetic demo data.
/// Symbols referenced in the code get deterministic random-walk OHLCV data.
pub fn execute(code: &str) -> FinScriptResult {
    let start = Instant::now();

    let tokens = match lexer::tokenize(code) {
        Ok(t) => t,
        Err(e) => return build_error_result(start, format!("Lexer error: {}", e)),
    };

    let program = match parser::parse(tokens) {
        Ok(p) => p,
        Err(e) => return build_error_result(start, format!("Parse error: {}", e)),
    };

    let symbols = interpreter::collect_symbols(&program);
    let symbol_data = generate_symbol_data(&symbols);

    let mut interp = interpreter::Interpreter::new(symbol_data);
    let result = interp.execute(&program);

    build_result(result, start)
}

/// Execute a FinScript program with externally-provided OHLCV data.
/// Used for live market data integration from the Tauri host.
pub fn execute_with_data(code: &str, symbol_data: HashMap<String, OhlcvSeries>) -> FinScriptResult {
    let start = Instant::now();

    let tokens = match lexer::tokenize(code) {
        Ok(t) => t,
        Err(e) => return build_error_result(start, format!("Lexer error: {}", e)),
    };

    let program = match parser::parse(tokens) {
        Ok(p) => p,
        Err(e) => return build_error_result(start, format!("Parse error: {}", e)),
    };

    let mut interp = interpreter::Interpreter::new(symbol_data);
    let result = interp.execute(&program);

    build_result(result, start)
}

/// Parse code and return the list of ticker symbols referenced in the AST.
/// The host uses this to know which symbols to fetch data for before execution.
pub fn extract_symbols(code: &str) -> Result<Vec<String>, String> {
    let tokens = lexer::tokenize(code).map_err(|e| format!("Lexer error: {}", e))?;
    let program = parser::parse(tokens).map_err(|e| format!("Parse error: {}", e))?;
    Ok(interpreter::collect_symbols(&program))
}

/// Generate synthetic OHLCV data for symbols using a deterministic random walk.
fn generate_symbol_data(symbols: &[String]) -> HashMap<String, OhlcvSeries> {
    let mut result = HashMap::new();
    for symbol in symbols {
        result.insert(symbol.clone(), generate_ohlcv(symbol, 180));
    }
    result
}

fn generate_ohlcv(symbol: &str, days: usize) -> OhlcvSeries {
    let seed: u64 = symbol.bytes().fold(0u64, |acc, b| acc.wrapping_mul(31).wrapping_add(b as u64));
    let base_price = 50.0 + (seed % 450) as f64;
    let volatility = 0.015 + (seed % 20) as f64 * 0.001;

    let mut timestamps = Vec::with_capacity(days);
    let mut open = Vec::with_capacity(days);
    let mut high = Vec::with_capacity(days);
    let mut low = Vec::with_capacity(days);
    let mut close = Vec::with_capacity(days);
    let mut volume = Vec::with_capacity(days);

    let start_ts: i64 = 1719792000;
    let mut price = base_price;
    let mut rng_state = seed;

    for i in 0..days {
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r1 = ((rng_state >> 33) as f64) / (u32::MAX as f64) - 0.5;
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r2 = ((rng_state >> 33) as f64) / (u32::MAX as f64);
        rng_state = rng_state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
        let r3 = ((rng_state >> 33) as f64) / (u32::MAX as f64);

        let change = price * volatility * r1 * 2.0;
        let day_open = price;
        let day_close = price + change;
        let spread = (change.abs() + price * volatility * r2 * 0.5).max(price * 0.002);
        let day_high = day_open.max(day_close) + spread * r2;
        let day_low = day_open.min(day_close) - spread * r3;
        let base_vol = 1_000_000.0 + (seed % 9_000_000) as f64;
        let day_volume = base_vol * (1.0 + change.abs() / price * 10.0) * (0.5 + r3);

        timestamps.push(start_ts + (i as i64) * 86400);
        open.push((day_open.max(0.01) * 10000.0).round() / 10000.0);
        high.push((day_high.max(day_open.max(day_close)) * 10000.0).round() / 10000.0);
        low.push((day_low.min(day_open.min(day_close)).max(0.01) * 10000.0).round() / 10000.0);
        close.push((day_close.max(0.01) * 10000.0).round() / 10000.0);
        volume.push(day_volume.round());

        price = day_close.max(0.01);
    }

    OhlcvSeries { symbol: symbol.to_string(), timestamps, open, high, low, close, volume }
}
