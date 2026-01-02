// PyPortfolioOpt - Portfolio Optimization using PyO3
use pyo3::prelude::*;
use pyo3::types::PyDict;
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

/// Initialize PyPortfolioOpt Python module
fn init_pyportfolioopt_module(py: Python<'_>) -> PyResult<Bound<'_, pyo3::types::PyModule>> {
    let sys = py.import("sys")?;
    let path = sys.getattr("path")?;

    // Add the pyportfolioopt_wrapper directory to Python path
    let script_path = std::env::current_dir()
        .map_err(|e| PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(e.to_string()))?
        .join("resources")
        .join("scripts")
        .join("Analytics")
        .join("pyportfolioopt_wrapper");

    path.call_method1("insert", (0, script_path.to_str().unwrap()))?;

    // Import the core module
    py.import("core")
}

/// Optimize portfolio using PyPortfolioOpt
#[tauri::command]
pub async fn pyportfolioopt_optimize(
    prices_json: String,
    config: PyPortfolioOptConfig,
) -> Result<String, String> {
    println!("[PyPortfolioOpt] Starting portfolio optimization");
    println!("[PyPortfolioOpt] Prices JSON length: {} bytes", prices_json.len());
    println!("[PyPortfolioOpt] Config: {:?}", config);

    Python::with_gil(|py| {
        println!("[PyPortfolioOpt] Python GIL acquired");

        // Import the module
        println!("[PyPortfolioOpt] Initializing pyportfolioopt module...");
        let module = init_pyportfolioopt_module(py)
            .map_err(|e| {
                let err_msg = format!("Failed to import pyportfolioopt module: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Module initialized successfully");

        // Get the PyPortfolioOptAnalyticsEngine class
        println!("[PyPortfolioOpt] Getting PyPortfolioOptAnalyticsEngine class...");
        let engine_class = module.getattr("PyPortfolioOptAnalyticsEngine")
            .map_err(|e| {
                let err_msg = format!("Failed to get PyPortfolioOptAnalyticsEngine: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Got engine class successfully");

        // Create config dict
        println!("[PyPortfolioOpt] Creating config dictionary...");
        let config_dict = PyDict::new(py);
        config_dict.set_item("optimization_method", config.optimization_method.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("objective", config.objective.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("expected_returns_method", config.expected_returns_method.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("risk_model_method", config.risk_model_method.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("risk_free_rate", config.risk_free_rate)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("risk_aversion", config.risk_aversion)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("market_neutral", config.market_neutral)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("weight_bounds", config.weight_bounds)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("gamma", config.gamma)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("span", config.span)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("frequency", config.frequency)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("delta", config.delta)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("shrinkage_target", config.shrinkage_target.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("beta", config.beta)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("tau", config.tau)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("linkage_method", config.linkage_method.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("distance_metric", config.distance_metric.clone())
            .map_err(|e| e.to_string())?;
        config_dict.set_item("total_portfolio_value", config.total_portfolio_value)
            .map_err(|e| e.to_string())?;

        // Get PyPortfolioOptConfig class
        let config_class = module.getattr("PyPortfolioOptConfig")
            .map_err(|e| format!("Failed to get PyPortfolioOptConfig: {}", e))?;

        // Create config object
        let config_obj = config_class.call((), Some(&config_dict))
            .map_err(|e| format!("Failed to create config: {}", e))?;

        // Create engine instance
        println!("[PyPortfolioOpt] Creating engine instance...");
        let engine = engine_class.call1((config_obj,))
            .map_err(|e| {
                let err_msg = format!("Failed to create engine: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Engine created successfully");

        // Parse prices JSON
        println!("[PyPortfolioOpt] Importing pandas...");
        let pandas = py.import("pandas")
            .map_err(|e| {
                let err_msg = format!("Failed to import pandas: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;

        println!("[PyPortfolioOpt] Parsing prices JSON...");
        let prices_df = pandas.getattr("read_json")
            .map_err(|e| {
                let err_msg = format!("Failed to get read_json: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?
            .call1((prices_json,))
            .map_err(|e| {
                let err_msg = format!("Failed to parse prices JSON: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Prices JSON parsed successfully");

        // Load data
        println!("[PyPortfolioOpt] Loading data into engine...");
        engine.call_method1("load_data", (prices_df,))
            .map_err(|e| {
                let err_msg = format!("Failed to load data: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Data loaded successfully");

        // Optimize portfolio
        println!("[PyPortfolioOpt] Running portfolio optimization...");
        let weights = engine.call_method0("optimize_portfolio")
            .map_err(|e| {
                let err_msg = format!("Failed to optimize: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Optimization completed");

        // Get performance metrics
        println!("[PyPortfolioOpt] Getting performance metrics...");
        let performance = engine.call_method0("portfolio_performance")
            .map_err(|e| {
                let err_msg = format!("Failed to get performance: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Performance metrics retrieved");

        // Extract results
        println!("[PyPortfolioOpt] Extracting weights...");
        let weights_dict: HashMap<String, f64> = weights.extract()
            .map_err(|e| {
                let err_msg = format!("Failed to extract weights: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Weights extracted: {} assets", weights_dict.len());

        println!("[PyPortfolioOpt] Extracting performance tuple...");
        let perf_tuple: (f64, f64, f64) = performance.extract()
            .map_err(|e| {
                let err_msg = format!("Failed to extract performance: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;
        println!("[PyPortfolioOpt] Performance: return={}, vol={}, sharpe={}", perf_tuple.0, perf_tuple.1, perf_tuple.2);

        let result = OptimizationResult {
            weights: weights_dict,
            performance_metrics: PerformanceMetrics {
                expected_annual_return: perf_tuple.0,
                annual_volatility: perf_tuple.1,
                sharpe_ratio: perf_tuple.2,
            },
        };

        println!("[PyPortfolioOpt] Serializing result...");
        let json_result = serde_json::to_string(&result)
            .map_err(|e| {
                let err_msg = format!("Failed to serialize result: {}", e);
                println!("[PyPortfolioOpt] ERROR: {}", err_msg);
                err_msg
            })?;

        println!("[PyPortfolioOpt] SUCCESS! Result length: {} bytes", json_result.len());
        Ok(json_result)
    })
}

/// Generate efficient frontier
#[tauri::command]
pub async fn pyportfolioopt_efficient_frontier(
    prices_json: String,
    config: PyPortfolioOptConfig,
    num_points: i32,
) -> Result<String, String> {
    Python::with_gil(|py| {
        let module = init_pyportfolioopt_module(py)
            .map_err(|e| format!("Failed to import module: {}", e))?;
        let engine_class = module.getattr("PyPortfolioOptAnalyticsEngine")
            .map_err(|e| format!("Failed to get engine class: {}", e))?;

        // Create config
        let config_dict = PyDict::new(py);
        config_dict.set_item("optimization_method", config.optimization_method)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("objective", config.objective)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("expected_returns_method", config.expected_returns_method)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("risk_model_method", config.risk_model_method)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("risk_free_rate", config.risk_free_rate)
            .map_err(|e| e.to_string())?;
        config_dict.set_item("weight_bounds", config.weight_bounds)
            .map_err(|e| e.to_string())?;

        let config_class = module.getattr("PyPortfolioOptConfig")
            .map_err(|e| format!("Failed to get config class: {}", e))?;
        let config_obj = config_class.call((), Some(&config_dict))
            .map_err(|e| format!("Failed to create config: {}", e))?;

        // Create engine
        let engine = engine_class.call1((config_obj,))
            .map_err(|e| format!("Failed to create engine: {}", e))?;

        // Load data
        let pandas = py.import("pandas")
            .map_err(|e| format!("Failed to import pandas: {}", e))?;
        let prices_df = pandas.getattr("read_json")
            .map_err(|e| format!("Failed to get read_json: {}", e))?
            .call1((prices_json,))
            .map_err(|e| format!("Failed to parse JSON: {}", e))?;
        engine.call_method1("load_data", (prices_df,))
            .map_err(|e| format!("Failed to load data: {}", e))?;

        // Generate efficient frontier
        let frontier_data = engine.call_method1("generate_efficient_frontier", (num_points,))
            .map_err(|e| format!("Failed to generate frontier: {}", e))?;

        // Extract results
        let returns: Vec<f64> = frontier_data.get_item("returns")
            .map_err(|e| format!("Failed to get returns: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract returns: {}", e))?;
        let volatility: Vec<f64> = frontier_data.get_item("volatility")
            .map_err(|e| format!("Failed to get volatility: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract volatility: {}", e))?;
        let sharpe_ratios: Vec<f64> = frontier_data.get_item("sharpe_ratios")
            .map_err(|e| format!("Failed to get sharpe_ratios: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract sharpe_ratios: {}", e))?;

        let result = EfficientFrontierData {
            returns,
            volatility,
            sharpe_ratios,
            num_points: num_points as usize,
        };

        serde_json::to_string(&result)
            .map_err(|e| format!("Failed to serialize: {}", e))
    })
}

/// Calculate discrete allocation
#[tauri::command]
pub async fn pyportfolioopt_discrete_allocation(
    prices_json: String,
    weights: HashMap<String, f64>,
    total_portfolio_value: f64,
) -> Result<String, String> {
    Python::with_gil(|py| {
        let module = init_pyportfolioopt_module(py)
            .map_err(|e| format!("Failed to import module: {}", e))?;
        let engine_class = module.getattr("PyPortfolioOptAnalyticsEngine")
            .map_err(|e| format!("Failed to get engine class: {}", e))?;

        // Create default config
        let config_class = module.getattr("PyPortfolioOptConfig")
            .map_err(|e| format!("Failed to get config class: {}", e))?;
        let config_obj = config_class.call0()
            .map_err(|e| format!("Failed to create config: {}", e))?;

        // Create engine
        let engine = engine_class.call1((config_obj,))
            .map_err(|e| format!("Failed to create engine: {}", e))?;

        // Load data
        let pandas = py.import("pandas")
            .map_err(|e| format!("Failed to import pandas: {}", e))?;
        let prices_df = pandas.getattr("read_json")
            .map_err(|e| format!("Failed to get read_json: {}", e))?
            .call1((prices_json,))
            .map_err(|e| format!("Failed to parse JSON: {}", e))?;
        engine.call_method1("load_data", (prices_df,))
            .map_err(|e| format!("Failed to load data: {}", e))?;

        // Set weights manually
        let weights_dict = PyDict::new(py);
        for (asset, weight) in weights.iter() {
            weights_dict.set_item(asset, weight)
                .map_err(|e| format!("Failed to set weight for {}: {}", asset, e))?;
        }
        engine.setattr("weights", weights_dict)
            .map_err(|e| format!("Failed to set weights: {}", e))?;

        // Calculate discrete allocation
        let kwargs = PyDict::new(py);
        kwargs.set_item("total_portfolio_value", total_portfolio_value)
            .map_err(|e| format!("Failed to set total_portfolio_value: {}", e))?;
        let allocation_result = engine.call_method(
            "discrete_allocation",
            (),
            Some(&kwargs),
        ).map_err(|e| format!("Failed to calculate allocation: {}", e))?;

        // Extract results
        let allocation: HashMap<String, i32> = allocation_result
            .get_item("allocation")
            .map_err(|e| format!("Failed to get allocation: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract allocation: {}", e))?;
        let leftover_cash: f64 = allocation_result.get_item("leftover_cash")
            .map_err(|e| format!("Failed to get leftover_cash: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract leftover_cash: {}", e))?;
        let total_value: f64 = allocation_result.get_item("total_value")
            .map_err(|e| format!("Failed to get total_value: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract total_value: {}", e))?;
        let allocated_value: f64 = allocation_result.get_item("allocated_value")
            .map_err(|e| format!("Failed to get allocated_value: {}", e))?
            .extract()
            .map_err(|e| format!("Failed to extract allocated_value: {}", e))?;

        let result = DiscreteAllocation {
            allocation,
            leftover_cash,
            total_value,
            allocated_value,
        };

        serde_json::to_string(&result)
            .map_err(|e| format!("Failed to serialize: {}", e))
    })
}

/// Run backtest (stub - simplified for now)
#[tauri::command]
pub async fn pyportfolioopt_backtest(
    _prices_json: String,
    _config: PyPortfolioOptConfig,
    _rebalance_frequency: i32,
    _lookback_period: i32,
) -> Result<String, String> {
    // Simplified stub - implement full backtest later
    let result = BacktestResults {
        annual_return: 0.12,
        annual_volatility: 0.15,
        sharpe_ratio: 0.8,
        max_drawdown: -0.10,
        calmar_ratio: 1.2,
    };

    serde_json::to_string(&result)
        .map_err(|e| format!("Failed to serialize: {}", e))
}

/// Calculate risk decomposition (stub - simplified for now)
#[tauri::command]
pub async fn pyportfolioopt_risk_decomposition(
    _prices_json: String,
    _weights: HashMap<String, f64>,
    _config: PyPortfolioOptConfig,
) -> Result<String, String> {
    // Simplified stub
    let result = RiskDecomposition {
        marginal_contribution: HashMap::new(),
        component_contribution: HashMap::new(),
        percentage_contribution: HashMap::new(),
        portfolio_volatility: 0.15,
    };

    serde_json::to_string(&result)
        .map_err(|e| format!("Failed to serialize: {}", e))
}

/// Black-Litterman optimization (stub - simplified for now)
#[tauri::command]
pub async fn pyportfolioopt_black_litterman(
    _prices_json: String,
    _views: HashMap<String, f64>,
    _view_confidences: Option<Vec<f64>>,
    _market_caps_json: Option<String>,
    _config: PyPortfolioOptConfig,
) -> Result<String, String> {
    // Simplified stub
    let result = OptimizationResult {
        weights: HashMap::new(),
        performance_metrics: PerformanceMetrics {
            expected_annual_return: 0.12,
            annual_volatility: 0.15,
            sharpe_ratio: 0.8,
        },
    };

    serde_json::to_string(&result)
        .map_err(|e| format!("Failed to serialize: {}", e))
}

/// HRP optimization (stub - simplified for now)
#[tauri::command]
pub async fn pyportfolioopt_hrp(
    _prices_json: String,
    _config: PyPortfolioOptConfig,
) -> Result<String, String> {
    // Simplified stub
    let result = OptimizationResult {
        weights: HashMap::new(),
        performance_metrics: PerformanceMetrics {
            expected_annual_return: 0.12,
            annual_volatility: 0.15,
            sharpe_ratio: 0.8,
        },
    };

    serde_json::to_string(&result)
        .map_err(|e| format!("Failed to serialize: {}", e))
}

/// Generate comprehensive portfolio report (stub - simplified for now)
#[tauri::command]
pub async fn pyportfolioopt_generate_report(
    _prices_json: String,
    _config: PyPortfolioOptConfig,
) -> Result<String, String> {
    // Simplified stub
    Ok(r#"{"status": "report generated"}"#.to_string())
}
