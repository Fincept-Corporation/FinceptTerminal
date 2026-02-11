use crate::python;
use tauri::command;

/// Helper to execute a TALIpp indicator calculation via descriptive_service.py
fn execute_talipp(
    app: &tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let args = vec![operation.to_string(), data.to_string()];
    python::execute_sync(app, "Analytics/talipp_wrapper/talipp_service.py", args)
}

// ==================== TREND INDICATORS ====================
// SMA, EMA, WMA, DEMA, TEMA, HMA, KAMA, ALMA, T3, ZLEMA

#[command]
pub async fn talipp_trend(
    app: tauri::AppHandle,
    indicator: String,
    prices: Vec<f64>,
    period: Option<i32>,
    fast_period: Option<i32>,
    slow_period: Option<i32>,
    sigma: Option<f64>,
    offset: Option<f64>,
    vfactor: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "period": period.unwrap_or(20),
        "fast_period": fast_period.unwrap_or(2),
        "slow_period": slow_period.unwrap_or(30),
        "sigma": sigma.unwrap_or(6.0),
        "offset": offset.unwrap_or(0.85),
        "vfactor": vfactor.unwrap_or(0.7)
    });
    execute_talipp(&app, &indicator, payload)
}

// ==================== TREND OTHER (ADX, Aroon, Ichimoku, PSAR, SuperTrend) ====================

#[command]
pub async fn talipp_trend_advanced(
    app: tauri::AppHandle,
    indicator: String,
    ohlcv: Vec<serde_json::Value>,
    period: Option<i32>,
    di_period: Option<i32>,
    adx_period: Option<i32>,
    multiplier: Option<f64>,
    af_start: Option<f64>,
    af_max: Option<f64>,
    af_increment: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "ohlcv": ohlcv,
        "period": period.unwrap_or(14),
        "di_period": di_period.unwrap_or(14),
        "adx_period": adx_period.unwrap_or(14),
        "multiplier": multiplier.unwrap_or(3.0),
        "af_start": af_start.unwrap_or(0.02),
        "af_max": af_max.unwrap_or(0.2),
        "af_increment": af_increment.unwrap_or(0.02)
    });
    execute_talipp(&app, &indicator, payload)
}

// ==================== MOMENTUM INDICATORS ====================
// RSI, MACD, Stochastic, StochRSI, CCI, ROC, TSI, Williams

#[command]
pub async fn talipp_momentum(
    app: tauri::AppHandle,
    indicator: String,
    prices: Option<Vec<f64>>,
    ohlcv: Option<Vec<serde_json::Value>>,
    period: Option<i32>,
    fast_period: Option<i32>,
    slow_period: Option<i32>,
    signal_period: Option<i32>,
    smoothing_period: Option<i32>,
    stoch_period: Option<i32>,
    k_smoothing: Option<i32>,
    d_smoothing: Option<i32>,
    long_period: Option<i32>,
    short_period: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "ohlcv": ohlcv,
        "period": period.unwrap_or(14),
        "fast_period": fast_period.unwrap_or(12),
        "slow_period": slow_period.unwrap_or(26),
        "signal_period": signal_period.unwrap_or(9),
        "smoothing_period": smoothing_period.unwrap_or(3),
        "stoch_period": stoch_period.unwrap_or(14),
        "k_smoothing": k_smoothing.unwrap_or(3),
        "d_smoothing": d_smoothing.unwrap_or(3),
        "long_period": long_period.unwrap_or(25),
        "short_period": short_period.unwrap_or(13)
    });
    execute_talipp(&app, &indicator, payload)
}

// ==================== VOLATILITY INDICATORS ====================
// ATR, Bollinger Bands, Keltner, Donchian, Chandelier Stop, NATR

#[command]
pub async fn talipp_volatility(
    app: tauri::AppHandle,
    indicator: String,
    prices: Option<Vec<f64>>,
    ohlcv: Option<Vec<serde_json::Value>>,
    period: Option<i32>,
    std_dev: Option<f64>,
    atr_period: Option<i32>,
    atr_multi: Option<f64>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "ohlcv": ohlcv,
        "period": period.unwrap_or(20),
        "std_dev": std_dev.unwrap_or(2.0),
        "atr_period": atr_period.unwrap_or(10),
        "atr_multi": atr_multi.unwrap_or(2.0)
    });
    execute_talipp(&app, &indicator, payload)
}

// ==================== VOLUME INDICATORS ====================
// OBV, VWAP, VWMA, MFI, Chaikin Oscillator, Force Index

#[command]
pub async fn talipp_volume(
    app: tauri::AppHandle,
    indicator: String,
    ohlcv: Vec<serde_json::Value>,
    period: Option<i32>,
    fast_period: Option<i32>,
    slow_period: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "ohlcv": ohlcv,
        "period": period.unwrap_or(14),
        "fast_period": fast_period.unwrap_or(3),
        "slow_period": slow_period.unwrap_or(10)
    });
    execute_talipp(&app, &indicator, payload)
}

// ==================== SPECIALIZED INDICATORS ====================
// AO, AccuDist, BOP, CHOP, CoppockCurve, DPO, EMV, Fibonacci, IBS,
// KST, KVO, MassIndex, McGinley, MeanDev, PivotsHL, RogersSatchell,
// SFX, SMMA, SOBV, STC, StdDev, TRIX, TTM, UO, VTX, ZigZag

#[command]
pub async fn talipp_specialized(
    app: tauri::AppHandle,
    indicator: String,
    prices: Option<Vec<f64>>,
    ohlcv: Option<Vec<serde_json::Value>>,
    period: Option<i32>,
    fast_period: Option<i32>,
    slow_period: Option<i32>,
    signal_period: Option<i32>,
    short_period: Option<i32>,
    medium_period: Option<i32>,
    long_period: Option<i32>,
    roc1_period: Option<i32>,
    roc2_period: Option<i32>,
    wma_period: Option<i32>,
    ema_period: Option<i32>,
    atr_period: Option<i32>,
    atr_mult: Option<f64>,
    bb_period: Option<i32>,
    kc_period: Option<i32>,
    cycle: Option<i32>,
    k: Option<f64>,
    change: Option<f64>,
    left_bars: Option<i32>,
    right_bars: Option<i32>,
) -> Result<String, String> {
    let payload = serde_json::json!({
        "prices": prices,
        "ohlcv": ohlcv,
        "period": period.unwrap_or(14),
        "fast_period": fast_period.unwrap_or(5),
        "slow_period": slow_period.unwrap_or(34),
        "signal_period": signal_period.unwrap_or(9),
        "short_period": short_period.unwrap_or(7),
        "medium_period": medium_period.unwrap_or(14),
        "long_period": long_period.unwrap_or(28),
        "roc1_period": roc1_period.unwrap_or(14),
        "roc2_period": roc2_period.unwrap_or(11),
        "wma_period": wma_period.unwrap_or(10),
        "ema_period": ema_period.unwrap_or(9),
        "atr_period": atr_period.unwrap_or(10),
        "atr_mult": atr_mult.unwrap_or(4.0),
        "bb_period": bb_period.unwrap_or(20),
        "kc_period": kc_period.unwrap_or(20),
        "cycle": cycle.unwrap_or(10),
        "k": k.unwrap_or(0.6),
        "change": change.unwrap_or(5.0),
        "left_bars": left_bars.unwrap_or(5),
        "right_bars": right_bars.unwrap_or(5)
    });
    execute_talipp(&app, &indicator, payload)
}
