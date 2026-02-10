// GS-Quant Wrapper Commands - Goldman Sachs quant analytics via subprocess
use crate::python;

/// Helper to execute a gs_quant_wrapper operation via worker_handler.py
fn execute_gs_quant(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/gs_quant_wrapper/worker_handler.py", args)
}

// ============================================================================
// PERFORMANCE & RETURNS
// ============================================================================

/// Full performance analysis: returns, risk, distribution metrics
#[tauri::command]
pub async fn gs_quant_performance_analysis(
    app: tauri::AppHandle,
    prices: Vec<f64>,
    dates: Option<Vec<String>>,
    benchmark_prices: Option<Vec<f64>>,
    risk_free_rate: Option<f64>,
    window: Option<u32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "dates": dates,
        "benchmark_prices": benchmark_prices.unwrap_or_default(),
        "risk_free_rate": risk_free_rate.unwrap_or(0.02),
        "window": window.unwrap_or(20),
    });
    execute_gs_quant(&app, "performance_analysis", payload)
}

/// Technical analysis summary: SMA, EMA, RSI, MACD, Bollinger Bands, ATR
#[tauri::command]
pub async fn gs_quant_technical_analysis(
    app: tauri::AppHandle,
    prices: Vec<f64>,
    dates: Option<Vec<String>>,
    high: Option<Vec<f64>>,
    low: Option<Vec<f64>>,
    window: Option<u32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "dates": dates,
        "high": high.unwrap_or_default(),
        "low": low.unwrap_or_default(),
        "window": window.unwrap_or(20),
    });
    execute_gs_quant(&app, "technical_analysis", payload)
}

// ============================================================================
// RISK ANALYTICS
// ============================================================================

/// Comprehensive risk metrics: volatility, VaR, CVaR, drawdown, ratios
#[tauri::command]
pub async fn gs_quant_risk_metrics(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0),
    });
    execute_gs_quant(&app, "risk_metrics", payload)
}

/// Forward volatility and variance term structure
#[tauri::command]
pub async fn gs_quant_volatility_surface(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    tenors: Option<Vec<u32>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "tenors": tenors.unwrap_or_else(|| vec![21, 63, 126, 252]),
    });
    execute_gs_quant(&app, "volatility_surface", payload)
}

/// Value at Risk analysis: parametric, historical, Monte Carlo, CVaR
#[tauri::command]
pub async fn gs_quant_var_analysis(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    confidence: Option<f64>,
    position_value: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "confidence": confidence.unwrap_or(0.95),
        "position_value": position_value.unwrap_or(1000000.0),
    });
    execute_gs_quant(&app, "var_analysis", payload)
}

/// Stress testing with standard market scenarios
#[tauri::command]
pub async fn gs_quant_stress_test(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    position_value: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "position_value": position_value.unwrap_or(1000000.0),
    });
    execute_gs_quant(&app, "stress_test", payload)
}

/// Calculate Greeks for a derivative instrument
#[tauri::command]
pub async fn gs_quant_greeks(
    app: tauri::AppHandle,
    spot: f64,
    strike: f64,
    expiry: f64,
    rate: f64,
    vol: f64,
    option_type: Option<String>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "spot": spot,
        "strike": strike,
        "expiry": expiry,
        "rate": rate,
        "vol": vol,
        "option_type": option_type.unwrap_or_else(|| "call".to_string()),
    });
    execute_gs_quant(&app, "greeks", payload)
}

/// Comprehensive risk report combining Greeks, VaR, scenarios
#[tauri::command]
pub async fn gs_quant_comprehensive_risk_report(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    position_value: Option<f64>,
    confidence: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "position_value": position_value.unwrap_or(1000000.0),
        "confidence": confidence.unwrap_or(0.95),
    });
    execute_gs_quant(&app, "comprehensive_risk_report", payload)
}

// ============================================================================
// PORTFOLIO ANALYTICS
// ============================================================================

/// Portfolio analytics with benchmark comparison
#[tauri::command]
pub async fn gs_quant_portfolio_analytics(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    benchmark_returns: Vec<f64>,
    dates: Option<Vec<String>>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "benchmark_returns": benchmark_returns,
        "dates": dates,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0),
    });
    execute_gs_quant(&app, "portfolio_analytics", payload)
}

/// Historical simulation for PnL estimation
#[tauri::command]
pub async fn gs_quant_historical_simulation(
    app: tauri::AppHandle,
    returns: Vec<f64>,
    dates: Option<Vec<String>>,
    position_value: Option<f64>,
    scenarios: Option<u32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "returns": returns,
        "dates": dates,
        "position_value": position_value.unwrap_or(1000000.0),
        "scenarios": scenarios,
    });
    execute_gs_quant(&app, "historical_simulation", payload)
}

// ============================================================================
// STATISTICS & CORRELATION
// ============================================================================

/// Descriptive statistics: mean, std, skew, kurtosis, percentiles
#[tauri::command]
pub async fn gs_quant_statistics(
    app: tauri::AppHandle,
    values: Vec<f64>,
    dates: Option<Vec<String>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "values": values,
        "dates": dates,
    });
    execute_gs_quant(&app, "statistics", payload)
}

/// Correlation, beta, R-squared between two series
#[tauri::command]
pub async fn gs_quant_correlation_analysis(
    app: tauri::AppHandle,
    x: Vec<f64>,
    y: Vec<f64>,
    dates: Option<Vec<String>>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "x": x,
        "y": y,
        "dates": dates,
    });
    execute_gs_quant(&app, "correlation_analysis", payload)
}

// ============================================================================
// BACKTESTING
// ============================================================================

/// Run a backtest strategy
#[tauri::command]
pub async fn gs_quant_backtest(
    app: tauri::AppHandle,
    prices: String,
    strategy: Option<String>,
    initial_capital: Option<f64>,
    commission: Option<f64>,
    lookback: Option<u32>,
    ticker: Option<String>,
    dates: Option<Vec<String>>,
) -> Result<String, String> {
    let prices_val: serde_json::Value = serde_json::from_str(&prices).unwrap_or(serde_json::json!({}));
    let payload = serde_json::json!({
        "prices": prices_val,
        "strategy": strategy.unwrap_or_else(|| "buy_and_hold".to_string()),
        "initial_capital": initial_capital.unwrap_or(100000.0),
        "commission": commission.unwrap_or(0.001),
        "lookback": lookback.unwrap_or(20),
        "ticker": ticker,
        "dates": dates,
    });
    execute_gs_quant(&app, "backtest", payload)
}

/// Basket/composite portfolio backtest
#[tauri::command]
pub async fn gs_quant_basket_backtest(
    app: tauri::AppHandle,
    components: String,
    weights: Option<Vec<f64>>,
    initial_value: Option<f64>,
    rebalance: Option<String>,
    dates: Option<Vec<String>>,
) -> Result<String, String> {
    let components_val: serde_json::Value = serde_json::from_str(&components).unwrap_or(serde_json::json!({}));
    let payload = serde_json::json!({
        "components": components_val,
        "weights": weights,
        "initial_value": initial_value.unwrap_or(100.0),
        "rebalance": rebalance.unwrap_or_else(|| "daily".to_string()),
        "dates": dates,
    });
    execute_gs_quant(&app, "basket_backtest", payload)
}

// ============================================================================
// INSTRUMENTS & DATETIME
// ============================================================================

/// Create and summarize a financial instrument
#[tauri::command]
pub async fn gs_quant_instrument_create(
    app: tauri::AppHandle,
    instrument_type: String,
    params: String,
) -> Result<String, String> {
    let mut payload: serde_json::Value = serde_json::from_str(&params).unwrap_or(serde_json::json!({}));
    if let Some(obj) = payload.as_object_mut() {
        obj.insert("instrument_type".to_string(), serde_json::json!(instrument_type));
    }
    execute_gs_quant(&app, "instrument_create", payload)
}

/// Date/time utilities: business days, day count fractions, date ranges
#[tauri::command]
pub async fn gs_quant_datetime_utils(
    app: tauri::AppHandle,
    sub_operation: String,
    params: String,
) -> Result<String, String> {
    let mut payload: serde_json::Value = serde_json::from_str(&params).unwrap_or(serde_json::json!({}));
    if let Some(obj) = payload.as_object_mut() {
        obj.insert("sub_operation".to_string(), serde_json::json!(sub_operation));
    }
    execute_gs_quant(&app, "datetime_utils", payload)
}
