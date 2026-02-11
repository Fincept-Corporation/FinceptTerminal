#![allow(dead_code)]
// PyPortfolioOpt - Portfolio Optimization using Worker Pool
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct PyPortfolioOptConfig {
    // Optimization Method
    pub optimization_method: String,  // efficient_frontier, hrp, cla, black_litterman, efficient_semivariance, efficient_cvar, efficient_cdar

    // Objective Function
    pub objective: String,  // max_sharpe, min_volatility, max_quadratic_utility, efficient_risk, efficient_return

    // Expected Returns Method
    pub expected_returns_method: String,  // mean_historical_return, ema_historical_return, capm_return

    // Risk Model Method
    pub risk_model_method: String,  // sample_cov, semicovariance, exp_cov, shrunk_covariance, ledoit_wolf

    // Parameters
    pub risk_free_rate: f64,
    pub risk_aversion: f64,
    pub market_neutral: bool,

    // Constraints
    pub weight_bounds: (f64, f64),
    pub gamma: f64,  // L2 regularization

    // Expected Returns Parameters
    pub span: i32,  // For EMA
    pub frequency: i32,  // Trading days per year

    // Risk Model Parameters
    pub delta: f64,  // Exponential decay
    pub shrinkage_target: String,

    // CVaR/CDaR Parameters
    pub beta: f64,  // Confidence level

    // Black-Litterman Parameters
    pub tau: f64,

    // HRP Parameters
    pub linkage_method: String,
    pub distance_metric: String,

    // Discrete Allocation
    pub total_portfolio_value: f64,
}

impl Default for PyPortfolioOptConfig {
    fn default() -> Self {
        PyPortfolioOptConfig {
            optimization_method: "efficient_frontier".to_string(),
            objective: "max_sharpe".to_string(),
            expected_returns_method: "mean_historical_return".to_string(),
            risk_model_method: "sample_cov".to_string(),
            risk_free_rate: 0.02,
            risk_aversion: 1.0,
            market_neutral: false,
            weight_bounds: (0.0, 1.0),
            gamma: 0.0,
            span: 500,
            frequency: 252,
            delta: 0.95,
            shrinkage_target: "constant_variance".to_string(),
            beta: 0.95,
            tau: 0.1,
            linkage_method: "ward".to_string(),
            distance_metric: "euclidean".to_string(),
            total_portfolio_value: 10000.0,
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct OptimizationResult {
    pub weights: HashMap<String, f64>,
    pub performance_metrics: PerformanceMetrics,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PerformanceMetrics {
    pub expected_annual_return: f64,
    pub annual_volatility: f64,
    pub sharpe_ratio: f64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct EfficientFrontierData {
    pub returns: Vec<f64>,
    pub volatility: Vec<f64>,
    pub sharpe_ratios: Vec<f64>,
    pub num_points: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DiscreteAllocation {
    pub allocation: HashMap<String, i32>,
    pub leftover_cash: f64,
    pub total_value: f64,
    pub allocated_value: f64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct BacktestResults {
    pub annual_return: f64,
    pub annual_volatility: f64,
    pub sharpe_ratio: f64,
    pub max_drawdown: f64,
    pub calmar_ratio: f64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct RiskDecomposition {
    pub marginal_contribution: HashMap<String, f64>,
    pub component_contribution: HashMap<String, f64>,
    pub percentage_contribution: HashMap<String, f64>,
    pub portfolio_volatility: f64,
}

/// Execute PyPortfolioOpt operation via worker pool
async fn execute_pyportfolioopt_operation(
    app: tauri::AppHandle,
    operation: &str,
    data: serde_json::Value,
) -> Result<String, String> {
    let data_json = serde_json::to_string(&data)
        .map_err(|e| format!("Failed to serialize data: {}", e))?;

    let args = vec![operation.to_string(), data_json];

    crate::python::execute(&app, "Analytics/pyportfolioopt_wrapper/pyportfolioopt_service.py", args).await
}

/// Optimize portfolio using PyPortfolioOpt
#[tauri::command]
pub async fn pyportfolioopt_optimize(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    println!("[PyPortfolioOpt] Starting portfolio optimization");
    println!("[PyPortfolioOpt] Prices JSON length: {} bytes", prices_json.len());
    println!("[PyPortfolioOpt] Config: {:?}", config);

    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config
    });

    let result = execute_pyportfolioopt_operation(app, "optimize", data).await?;

    println!("[PyPortfolioOpt] SUCCESS! Result length: {} bytes", result.len());
    Ok(result)
}

/// Generate efficient frontier
#[tauri::command]
pub async fn pyportfolioopt_efficient_frontier(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
    num_points: i32,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config,
        "num_points": num_points
    });

    execute_pyportfolioopt_operation(app, "efficient_frontier", data).await
}

/// Calculate discrete allocation
#[tauri::command]
pub async fn pyportfolioopt_discrete_allocation(
    app: tauri::AppHandle,
    prices_json: String,
    weights: HashMap<String, f64>,
    total_portfolio_value: f64,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "weights": weights,
        "total_portfolio_value": total_portfolio_value
    });

    execute_pyportfolioopt_operation(app, "discrete_allocation", data).await
}

/// Run rolling-window backtest
#[tauri::command]
pub async fn pyportfolioopt_backtest(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
    rebalance_frequency: i32,
    lookback_period: i32,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config,
        "rebalance_frequency": rebalance_frequency,
        "lookback_period": lookback_period
    });

    execute_pyportfolioopt_operation(app, "backtest", data).await
}

/// Calculate risk decomposition
#[tauri::command]
pub async fn pyportfolioopt_risk_decomposition(
    app: tauri::AppHandle,
    prices_json: String,
    weights: HashMap<String, f64>,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "weights": weights,
        "config": config
    });

    execute_pyportfolioopt_operation(app, "risk_decomposition", data).await
}

/// Black-Litterman optimization
#[tauri::command]
pub async fn pyportfolioopt_black_litterman(
    app: tauri::AppHandle,
    prices_json: String,
    views: HashMap<String, f64>,
    view_confidences: Option<Vec<f64>>,
    market_caps_json: Option<String>,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "views": views,
        "view_confidences": view_confidences,
        "market_caps_json": market_caps_json,
        "config": config
    });

    execute_pyportfolioopt_operation(app, "black_litterman", data).await
}

/// HRP optimization
#[tauri::command]
pub async fn pyportfolioopt_hrp(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config
    });

    execute_pyportfolioopt_operation(app, "hrp", data).await
}

/// Generate comprehensive portfolio report
#[tauri::command]
pub async fn pyportfolioopt_generate_report(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config
    });

    execute_pyportfolioopt_operation(app, "generate_report", data).await
}

// ============================================================================
// Sensitivity Analysis
// ============================================================================

/// Run sensitivity analysis on a configuration parameter
#[tauri::command]
pub async fn pyportfolioopt_sensitivity_analysis(
    app: tauri::AppHandle,
    prices_json: String,
    config: PyPortfolioOptConfig,
    parameter: String,
    values: Option<Vec<f64>>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "config": config,
        "parameter": parameter,
        "values": values
    });

    execute_pyportfolioopt_operation(app, "sensitivity_analysis", data).await
}

// ============================================================================
// Additional Optimizers
// ============================================================================

/// Risk Parity optimization
#[tauri::command]
pub async fn pyportfolioopt_risk_parity(
    app: tauri::AppHandle,
    prices_json: String,
    risk_measure: Option<String>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "risk_measure": risk_measure.unwrap_or_else(|| "volatility".to_string())
    });

    execute_pyportfolioopt_operation(app, "risk_parity", data).await
}

/// Equal weighting (1/N) portfolio
#[tauri::command]
pub async fn pyportfolioopt_equal_weight(
    app: tauri::AppHandle,
    prices_json: String,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json
    });

    execute_pyportfolioopt_operation(app, "equal_weight", data).await
}

/// Inverse volatility weighting
#[tauri::command]
pub async fn pyportfolioopt_inverse_volatility(
    app: tauri::AppHandle,
    prices_json: String,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json
    });

    execute_pyportfolioopt_operation(app, "inverse_volatility", data).await
}

/// Market neutral (long/short) portfolio
#[tauri::command]
pub async fn pyportfolioopt_market_neutral(
    app: tauri::AppHandle,
    prices_json: String,
    long_exposure: Option<f64>,
    short_exposure: Option<f64>,
    objective: Option<String>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "long_exposure": long_exposure.unwrap_or(1.0),
        "short_exposure": short_exposure.unwrap_or(-1.0),
        "objective": objective.unwrap_or_else(|| "max_sharpe".to_string())
    });

    execute_pyportfolioopt_operation(app, "market_neutral", data).await
}

/// Minimum tracking error relative to benchmark
#[tauri::command]
pub async fn pyportfolioopt_min_tracking_error(
    app: tauri::AppHandle,
    prices_json: String,
    benchmark_weights: HashMap<String, f64>,
    target_return: Option<f64>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "benchmark_weights": benchmark_weights,
        "target_return": target_return
    });

    execute_pyportfolioopt_operation(app, "min_tracking_error", data).await
}

/// Maximum diversification portfolio
#[tauri::command]
pub async fn pyportfolioopt_max_diversification(
    app: tauri::AppHandle,
    prices_json: String,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json
    });

    execute_pyportfolioopt_operation(app, "max_diversification", data).await
}

// ============================================================================
// Advanced Objectives
// ============================================================================

/// Optimize with custom sector constraints
#[tauri::command]
pub async fn pyportfolioopt_custom_constraints(
    app: tauri::AppHandle,
    prices_json: String,
    objective: Option<String>,
    sector_mapper: Option<HashMap<String, String>>,
    sector_lower: Option<HashMap<String, f64>>,
    sector_upper: Option<HashMap<String, f64>>,
    weight_bounds: Option<(f64, f64)>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "objective": objective.unwrap_or_else(|| "max_sharpe".to_string()),
        "sector_mapper": sector_mapper,
        "sector_lower": sector_lower,
        "sector_upper": sector_upper,
        "weight_bounds": weight_bounds.unwrap_or((0.0, 1.0))
    });

    execute_pyportfolioopt_operation(app, "custom_constraints", data).await
}

/// Views-based Black-Litterman optimization (advanced objectives)
#[tauri::command]
pub async fn pyportfolioopt_views_optimization(
    app: tauri::AppHandle,
    prices_json: String,
    views: HashMap<String, f64>,
    view_confidences: Option<Vec<f64>>,
    market_caps_json: Option<String>,
    risk_aversion: Option<f64>,
) -> Result<String, String> {
    let data = serde_json::json!({
        "prices_json": prices_json,
        "views": views,
        "view_confidences": view_confidences,
        "market_caps_json": market_caps_json,
        "risk_aversion": risk_aversion.unwrap_or(1.0)
    });

    execute_pyportfolioopt_operation(app, "views_optimization", data).await
}
