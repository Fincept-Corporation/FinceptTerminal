// Fortitudo.tech Analytics Commands - Portfolio risk analytics via PyO3
#![allow(dead_code)]
use crate::python_runtime;
use crate::utils::python::get_script_path;

// ============================================================================
// FORTITUDO ANALYTICS COMMANDS
// ============================================================================

/// Check fortitudo.tech library status and availability
#[tauri::command]
pub async fn fortitudo_check_status(app: tauri::AppHandle) -> Result<String, String> {
    let args = vec!["check_status".to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate comprehensive portfolio risk metrics (VaR, CVaR, volatility, Sharpe)
#[tauri::command]
pub async fn fortitudo_portfolio_metrics(
    app: tauri::AppHandle,
    returns_json: String,
    weights_json: String,
    alpha: Option<f64>,
    probabilities: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "weights": weights_json,
        "alpha": alpha.unwrap_or(0.05),
        "probabilities": probabilities
    });

    let args = vec!["portfolio_metrics".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate covariance and correlation matrices with statistical moments
#[tauri::command]
pub async fn fortitudo_covariance_analysis(
    app: tauri::AppHandle,
    returns_json: String,
    probabilities: Option<String>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "probabilities": probabilities
    });

    let args = vec!["covariance_analysis".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate exponentially decaying probabilities for scenario weighting
#[tauri::command]
pub async fn fortitudo_exp_decay_weighting(
    app: tauri::AppHandle,
    returns_json: String,
    half_life: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "half_life": half_life.unwrap_or(252)
    });

    let args = vec!["exp_decay_weighting".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Price options using Black-Scholes model
#[tauri::command]
pub async fn fortitudo_option_pricing(
    app: tauri::AppHandle,
    spot_price: f64,
    strike: f64,
    volatility: f64,
    risk_free_rate: Option<f64>,
    dividend_yield: Option<f64>,
    time_to_maturity: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "spot_price": spot_price,
        "strike": strike,
        "volatility": volatility,
        "risk_free_rate": risk_free_rate.unwrap_or(0.05),
        "dividend_yield": dividend_yield.unwrap_or(0.0),
        "time_to_maturity": time_to_maturity.unwrap_or(1.0)
    });

    let args = vec!["option_pricing".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate option chain for multiple strikes
#[tauri::command]
pub async fn fortitudo_option_chain(
    app: tauri::AppHandle,
    spot_price: f64,
    volatility: f64,
    strikes: Option<Vec<f64>>,
    risk_free_rate: Option<f64>,
    dividend_yield: Option<f64>,
    time_to_maturity: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "spot_price": spot_price,
        "volatility": volatility,
        "strikes": strikes,
        "risk_free_rate": risk_free_rate.unwrap_or(0.05),
        "dividend_yield": dividend_yield.unwrap_or(0.0),
        "time_to_maturity": time_to_maturity.unwrap_or(1.0)
    });

    let args = vec!["option_chain".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Apply entropy pooling for scenario probability adjustment
#[tauri::command]
pub async fn fortitudo_entropy_pooling(
    app: tauri::AppHandle,
    n_scenarios: i32,
    max_probability: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "n_scenarios": n_scenarios,
        "max_probability": max_probability
    });

    let args = vec!["entropy_pooling".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate exposure stacking portfolio from sample portfolios
#[tauri::command]
pub async fn fortitudo_exposure_stacking(
    app: tauri::AppHandle,
    sample_portfolios_json: String,
    n_partitions: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "sample_portfolios": sample_portfolios_json,
        "n_partitions": n_partitions.unwrap_or(4)
    });

    let args = vec!["exposure_stacking".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Full portfolio risk analysis combining all metrics
#[tauri::command]
pub async fn fortitudo_full_analysis(
    app: tauri::AppHandle,
    returns_json: String,
    weights_json: String,
    alpha: Option<f64>,
    half_life: Option<i32>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "weights": weights_json,
        "alpha": alpha.unwrap_or(0.05),
        "half_life": half_life.unwrap_or(252)
    });

    let args = vec!["full_analysis".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate option Greeks (Delta, Gamma, Vega, Theta, Rho)
#[tauri::command]
pub async fn fortitudo_option_greeks(
    app: tauri::AppHandle,
    spot_price: f64,
    strike: f64,
    volatility: f64,
    risk_free_rate: Option<f64>,
    dividend_yield: Option<f64>,
    time_to_maturity: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "spot_price": spot_price,
        "strike": strike,
        "volatility": volatility,
        "risk_free_rate": risk_free_rate.unwrap_or(0.05),
        "dividend_yield": dividend_yield.unwrap_or(0.0),
        "time_to_maturity": time_to_maturity.unwrap_or(1.0)
    });

    let args = vec!["option_greeks".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Mean-Variance portfolio optimization
#[tauri::command]
pub async fn fortitudo_optimize_mean_variance(
    app: tauri::AppHandle,
    returns_json: String,
    objective: Option<String>,
    long_only: Option<bool>,
    max_weight: Option<f64>,
    min_weight: Option<f64>,
    risk_free_rate: Option<f64>,
    target_return: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "objective": objective.unwrap_or_else(|| "min_variance".to_string()),
        "long_only": long_only.unwrap_or(true),
        "max_weight": max_weight,
        "min_weight": min_weight,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0),
        "target_return": target_return
    });

    let args = vec!["optimize_mean_variance".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Mean-CVaR portfolio optimization
#[tauri::command]
pub async fn fortitudo_optimize_mean_cvar(
    app: tauri::AppHandle,
    returns_json: String,
    objective: Option<String>,
    alpha: Option<f64>,
    long_only: Option<bool>,
    max_weight: Option<f64>,
    min_weight: Option<f64>,
    risk_free_rate: Option<f64>,
    target_return: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "objective": objective.unwrap_or_else(|| "min_cvar".to_string()),
        "alpha": alpha.unwrap_or(0.05),
        "long_only": long_only.unwrap_or(true),
        "max_weight": max_weight,
        "min_weight": min_weight,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0),
        "target_return": target_return
    });

    let args = vec!["optimize_mean_cvar".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate Mean-Variance efficient frontier
#[tauri::command]
pub async fn fortitudo_efficient_frontier_mv(
    app: tauri::AppHandle,
    returns_json: String,
    n_points: Option<i32>,
    long_only: Option<bool>,
    max_weight: Option<f64>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "n_points": n_points.unwrap_or(20),
        "long_only": long_only.unwrap_or(true),
        "max_weight": max_weight,
        "risk_free_rate": risk_free_rate.unwrap_or(0.0)
    });

    let args = vec!["efficient_frontier_mv".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate Mean-CVaR efficient frontier
#[tauri::command]
pub async fn fortitudo_efficient_frontier_cvar(
    app: tauri::AppHandle,
    returns_json: String,
    n_points: Option<i32>,
    alpha: Option<f64>,
    long_only: Option<bool>,
    max_weight: Option<f64>,
) -> Result<String, String> {
    let params = serde_json::json!({
        "returns": returns_json,
        "n_points": n_points.unwrap_or(20),
        "alpha": alpha.unwrap_or(0.05),
        "long_only": long_only.unwrap_or(true),
        "max_weight": max_weight
    });

    let args = vec!["efficient_frontier_cvar".to_string(), params.to_string()];
    let script_path = get_script_path(&app, "Analytics/fortitudo_tech_wrapper/fortitudo_service.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
