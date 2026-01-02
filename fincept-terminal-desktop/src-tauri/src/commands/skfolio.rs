// skfolio Portfolio Optimization Commands
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Optimize portfolio using skfolio
#[tauri::command]
pub async fn skfolio_optimize_portfolio(
    app: tauri::AppHandle,
    prices_data: String,
    optimization_method: Option<String>,
    objective_function: Option<String>,
    risk_measure: Option<String>,
    config: Option<String>,
) -> Result<String, String> {
    println!("[skfolio] Starting portfolio optimization");
    println!("[skfolio] Prices data: {}", prices_data);
    println!("[skfolio] Optimization method: {:?}", optimization_method);
    println!("[skfolio] Objective function: {:?}", objective_function);
    println!("[skfolio] Risk measure: {:?}", risk_measure);
    println!("[skfolio] Config: {:?}", config);

    let mut args = vec![
        "optimize".to_string(),
        prices_data.clone(),
    ];

    // Add optimization parameters
    let opt_method = optimization_method.unwrap_or_else(|| "mean_risk".to_string());
    let obj_func = objective_function.unwrap_or_else(|| "maximize_ratio".to_string());
    let risk_meas = risk_measure.unwrap_or_else(|| "cvar".to_string());

    args.push(opt_method.clone());
    args.push(obj_func.clone());
    args.push(risk_meas.clone());

    println!("[skfolio] Final args before config: {:?}", args);

    // Add additional config as JSON if provided
    if let Some(cfg) = config {
        println!("[skfolio] Adding config: {}", cfg);
        args.push(cfg);
    }

    println!("[skfolio] All args: {:?}", args);

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    println!("[skfolio] Script path: {:?}", script_path);

    println!("[skfolio] Executing Python script...");
    let result = python_runtime::execute_python_script(&script_path, args);

    match &result {
        Ok(output) => {
            println!("[skfolio] Python execution successful");
            println!("[skfolio] Output length: {} bytes", output.len());
            if output.len() < 500 {
                println!("[skfolio] Output: {}", output);
            } else {
                println!("[skfolio] Output (first 500 chars): {}...", &output[..500]);
            }
        }
        Err(e) => {
            println!("[skfolio] ERROR: Python execution failed: {}", e);
        }
    }

    result
}

/// Run hyperparameter tuning
#[tauri::command]
pub async fn skfolio_hyperparameter_tuning(
    app: tauri::AppHandle,
    prices_data: String,
    param_grid: Option<String>,
    cv_method: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "hyperparameter_tuning".to_string(),
        prices_data,
    ];

    if let Some(grid) = param_grid {
        args.push(grid);
    } else {
        args.push("null".to_string());
    }

    if let Some(cv) = cv_method {
        args.push(cv);
    } else {
        args.push("walk_forward".to_string());
    }

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Run portfolio backtest
#[tauri::command]
pub async fn skfolio_backtest_strategy(
    app: tauri::AppHandle,
    prices_data: String,
    rebalance_freq: Option<i32>,
    window_size: Option<i32>,
    config: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "backtest".to_string(),
        prices_data,
    ];

    args.push(rebalance_freq.unwrap_or(21).to_string());
    args.push(window_size.unwrap_or(252).to_string());

    if let Some(cfg) = config {
        args.push(cfg);
    }

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Perform stress testing
#[tauri::command]
pub async fn skfolio_stress_test(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    scenarios: Option<String>,
    n_simulations: Option<i32>,
) -> Result<String, String> {
    let mut args = vec![
        "stress_test".to_string(),
        prices_data,
        weights,
    ];

    if let Some(scen) = scenarios {
        args.push(scen);
    } else {
        args.push("null".to_string());
    }

    args.push(n_simulations.unwrap_or(10000).to_string());

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate efficient frontier
#[tauri::command]
pub async fn skfolio_efficient_frontier(
    app: tauri::AppHandle,
    prices_data: String,
    n_portfolios: Option<i32>,
    config: Option<String>,
) -> Result<String, String> {
    println!("[skfolio] Starting efficient frontier generation");
    println!("[skfolio] Prices data: {}", prices_data);
    println!("[skfolio] Number of portfolios: {:?}", n_portfolios);
    println!("[skfolio] Config: {:?}", config);

    let mut args = vec![
        "efficient_frontier".to_string(),
        prices_data.clone(),
    ];

    let n_ports = n_portfolios.unwrap_or(100);
    args.push(n_ports.to_string());
    println!("[skfolio] Number of portfolios arg: {}", n_ports);

    if let Some(cfg) = config {
        println!("[skfolio] Adding config: {}", cfg);
        args.push(cfg);
    }

    println!("[skfolio] All args: {:?}", args);

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    println!("[skfolio] Script path: {:?}", script_path);

    println!("[skfolio] Executing Python script...");
    let result = python_runtime::execute_python_script(&script_path, args);

    match &result {
        Ok(output) => {
            println!("[skfolio] Python execution successful");
            println!("[skfolio] Output length: {} bytes", output.len());
            if output.len() < 1000 {
                println!("[skfolio] Output: {}", output);
            } else {
                println!("[skfolio] Output (first 1000 chars): {}...", &output[..1000]);
            }
        }
        Err(e) => {
            println!("[skfolio] ERROR: Python execution failed: {}", e);
        }
    }

    result
}

/// Calculate risk attribution
#[tauri::command]
pub async fn skfolio_risk_attribution(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
) -> Result<String, String> {
    let args = vec![
        "risk_attribution".to_string(),
        prices_data,
        weights,
    ];

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Compare multiple strategies
#[tauri::command]
pub async fn skfolio_compare_strategies(
    app: tauri::AppHandle,
    prices_data: String,
    strategies: String,
    metric: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "compare_strategies".to_string(),
        prices_data,
        strategies,
    ];

    args.push(metric.unwrap_or_else(|| "sharpe_ratio".to_string()));

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate comprehensive portfolio report
#[tauri::command]
pub async fn skfolio_generate_report(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    config: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "generate_report".to_string(),
        prices_data,
        weights,
    ];

    if let Some(cfg) = config {
        args.push(cfg);
    }

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Export portfolio weights
#[tauri::command]
pub async fn skfolio_export_weights(
    app: tauri::AppHandle,
    weights: String,
    asset_names: String,
    filename: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "export_weights".to_string(),
        weights,
        asset_names,
    ];

    if let Some(fname) = filename {
        args.push(fname);
    }

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Scenario analysis
#[tauri::command]
pub async fn skfolio_scenario_analysis(
    app: tauri::AppHandle,
    prices_data: String,
    weights: String,
    scenarios: String,
) -> Result<String, String> {
    let args = vec![
        "scenario_analysis".to_string(),
        prices_data,
        weights,
        scenarios,
    ];

    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
