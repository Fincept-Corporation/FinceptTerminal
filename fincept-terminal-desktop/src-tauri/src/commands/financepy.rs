use crate::utils::python::get_script_path;
use crate::python_runtime;
use tauri::command;

// ==================== UTILITIES ====================

#[command]
pub async fn financepy_create_date(
    app: tauri::AppHandle,
    date_str: String,
) -> Result<String, String> {
    let args = vec!["utils_create_date".to_string(), date_str];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[command]
pub async fn financepy_date_range(
    app: tauri::AppHandle,
    start_date: String,
    end_date: String,
    freq: String,
) -> Result<String, String> {
    let args = vec!["utils_date_range".to_string(), start_date, end_date, freq];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ==================== BONDS ====================

#[command]
pub async fn financepy_bond_price(
    app: tauri::AppHandle,
    issue_date: String,
    settlement_date: String,
    maturity_date: String,
    coupon_rate: f64,
    ytm: f64,
    freq: i32,
) -> Result<String, String> {
    let args = vec![
        "bond_price".to_string(),
        issue_date,
        settlement_date,
        maturity_date,
        coupon_rate.to_string(),
        ytm.to_string(),
        freq.to_string(),
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[command]
pub async fn financepy_bond_ytm(
    app: tauri::AppHandle,
    issue_date: String,
    settlement_date: String,
    maturity_date: String,
    coupon_rate: f64,
    clean_price: f64,
    freq: i32,
) -> Result<String, String> {
    let args = vec![
        "bond_ytm".to_string(),
        issue_date,
        settlement_date,
        maturity_date,
        coupon_rate.to_string(),
        clean_price.to_string(),
        freq.to_string(),
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ==================== EQUITY OPTIONS ====================

#[command]
pub async fn financepy_equity_option_price(
    app: tauri::AppHandle,
    valuation_date: String,
    expiry_date: String,
    strike: f64,
    spot: f64,
    volatility: f64,
    risk_free_rate: f64,
    dividend_yield: f64,
    option_type: String,
) -> Result<String, String> {
    let args = vec![
        "equity_option_price".to_string(),
        valuation_date,
        expiry_date,
        strike.to_string(),
        spot.to_string(),
        volatility.to_string(),
        risk_free_rate.to_string(),
        dividend_yield.to_string(),
        option_type,
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[command]
pub async fn financepy_equity_option_implied_vol(
    app: tauri::AppHandle,
    valuation_date: String,
    expiry_date: String,
    strike: f64,
    spot: f64,
    option_price: f64,
    risk_free_rate: f64,
    dividend_yield: f64,
    option_type: String,
) -> Result<String, String> {
    let args = vec![
        "equity_option_implied_vol".to_string(),
        valuation_date,
        expiry_date,
        strike.to_string(),
        spot.to_string(),
        option_price.to_string(),
        risk_free_rate.to_string(),
        dividend_yield.to_string(),
        option_type,
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ==================== FX OPTIONS ====================

#[command]
pub async fn financepy_fx_option_price(
    app: tauri::AppHandle,
    valuation_date: String,
    expiry_date: String,
    strike: f64,
    spot: f64,
    volatility: f64,
    domestic_rate: f64,
    foreign_rate: f64,
    option_type: String,
    notional: f64,
) -> Result<String, String> {
    let args = vec![
        "fx_option_price".to_string(),
        valuation_date,
        expiry_date,
        strike.to_string(),
        spot.to_string(),
        volatility.to_string(),
        domestic_rate.to_string(),
        foreign_rate.to_string(),
        option_type,
        notional.to_string(),
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ==================== INTEREST RATE SWAPS ====================

#[command]
pub async fn financepy_ibor_swap_price(
    app: tauri::AppHandle,
    effective_date: String,
    maturity_date: String,
    fixed_rate: f64,
    freq: i32,
    notional: f64,
    discount_rate: f64,
) -> Result<String, String> {
    let args = vec![
        "ibor_swap_price".to_string(),
        effective_date,
        maturity_date,
        fixed_rate.to_string(),
        freq.to_string(),
        notional.to_string(),
        discount_rate.to_string(),
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ==================== CREDIT DERIVATIVES ====================

#[command]
pub async fn financepy_cds_spread(
    app: tauri::AppHandle,
    valuation_date: String,
    maturity_date: String,
    recovery_rate: f64,
    notional: f64,
    spread_bps: f64,
) -> Result<String, String> {
    let args = vec![
        "cds_spread".to_string(),
        valuation_date,
        maturity_date,
        recovery_rate.to_string(),
        notional.to_string(),
        spread_bps.to_string(),
    ];
    let script_path = get_script_path(&app, "financepy_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
