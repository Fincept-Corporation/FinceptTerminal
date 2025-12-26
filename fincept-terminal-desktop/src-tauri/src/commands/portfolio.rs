// Portfolio Analytics commands
use crate::utils::python::get_script_path;
use crate::python_runtime;
use serde_json::Value;

/// Calculate portfolio metrics
#[tauri::command]
pub async fn calculate_portfolio_metrics(
    app: tauri::AppHandle,
    returns_data: String,
    weights: Option<String>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["calculate_metrics".to_string(), returns_data];

    if let Some(w) = weights {
        args.push(w);
    } else {
        args.push("null".to_string());
    }

    if let Some(rfr) = risk_free_rate {
        args.push(rfr.to_string());
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_analytics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Optimize portfolio weights
#[tauri::command]
pub async fn optimize_portfolio(
    app: tauri::AppHandle,
    returns_data: String,
    method: Option<String>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["optimize".to_string(), returns_data];

    if let Some(m) = method {
        args.push(m);
    } else {
        args.push("max_sharpe".to_string());
    }

    if let Some(rfr) = risk_free_rate {
        args.push(rfr.to_string());
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_analytics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate efficient frontier
#[tauri::command]
pub async fn generate_efficient_frontier(
    app: tauri::AppHandle,
    returns_data: String,
    num_points: Option<i32>,
    risk_free_rate: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["efficient_frontier".to_string(), returns_data];

    if let Some(np) = num_points {
        args.push(np.to_string());
    } else {
        args.push("50".to_string());
    }

    if let Some(rfr) = risk_free_rate {
        args.push(rfr.to_string());
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_analytics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Get portfolio analytics overview
#[tauri::command]
pub async fn get_portfolio_overview(
    app: tauri::AppHandle,
    returns_data: String,
    weights: Option<String>,
) -> Result<String, String> {
    let metrics = calculate_portfolio_metrics(
        app.clone(),
        returns_data.clone(),
        weights.clone(),
        None,
    ).await?;

    let optimization = optimize_portfolio(
        app.clone(),
        returns_data.clone(),
        Some("max_sharpe".to_string()),
        None,
    ).await?;

    let metrics_json: Value = serde_json::from_str(&metrics)
        .map_err(|e| format!("Failed to parse metrics: {}", e))?;
    let optimization_json: Value = serde_json::from_str(&optimization)
        .map_err(|e| format!("Failed to parse optimization: {}", e))?;

    let combined = serde_json::json!({
        "current_portfolio": metrics_json,
        "optimized_portfolio": optimization_json
    });

    serde_json::to_string(&combined)
        .map_err(|e| format!("Failed to serialize overview: {}", e))
}

/// Calculate risk metrics
#[tauri::command]
pub async fn calculate_risk_metrics(
    app: tauri::AppHandle,
    returns_data: String,
    weights: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["risk_metrics".to_string(), returns_data];
    if let Some(w) = weights {
        args.push(w);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Generate asset allocation plan
#[tauri::command]
pub async fn generate_asset_allocation(
    app: tauri::AppHandle,
    age: i32,
    risk_tolerance: String,
    years_to_retirement: i32,
) -> Result<String, String> {
    let args = vec![
        "asset_allocation".to_string(),
        age.to_string(),
        risk_tolerance,
        years_to_retirement.to_string(),
    ];
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate retirement planning
#[tauri::command]
pub async fn calculate_retirement_plan(
    app: tauri::AppHandle,
    current_age: i32,
    retirement_age: i32,
    current_savings: f64,
    annual_contribution: f64,
) -> Result<String, String> {
    let args = vec![
        "retirement_planning".to_string(),
        current_age.to_string(),
        retirement_age.to_string(),
        current_savings.to_string(),
        annual_contribution.to_string(),
    ];
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Analyze behavioral biases
#[tauri::command]
pub async fn analyze_behavioral_biases(
    app: tauri::AppHandle,
    portfolio_data: String,
) -> Result<String, String> {
    let args = vec!["behavioral_analysis".to_string(), portfolio_data];
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Analyze ETF costs
#[tauri::command]
pub async fn analyze_etf_costs(
    app: tauri::AppHandle,
    symbols: String,
    expense_ratios: String,
) -> Result<String, String> {
    let args = vec!["etf_analysis".to_string(), symbols, expense_ratios];
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ============================================================================
// Active Management Commands
// ============================================================================

/// Calculate value added by active management
#[tauri::command]
pub async fn calculate_active_value_added(
    app: tauri::AppHandle,
    portfolio_returns: String,
    benchmark_returns: String,
    portfolio_weights: Option<String>,
) -> Result<String, String> {
    let mut args = vec![
        "value_added".to_string(),
        portfolio_returns,
        benchmark_returns,
    ];

    if let Some(weights) = portfolio_weights {
        args.push(weights);
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate information ratio analysis
#[tauri::command]
pub async fn calculate_active_information_ratio(
    app: tauri::AppHandle,
    portfolio_returns: String,
    benchmark_returns: String,
) -> Result<String, String> {
    let args = vec![
        "information_ratio".to_string(),
        portfolio_returns,
        benchmark_returns,
    ];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Calculate tracking risk analysis
#[tauri::command]
pub async fn calculate_active_tracking_risk(
    app: tauri::AppHandle,
    portfolio_returns: String,
    benchmark_returns: String,
) -> Result<String, String> {
    let args = vec![
        "tracking_risk".to_string(),
        portfolio_returns,
        benchmark_returns,
    ];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Comprehensive active management analysis
#[tauri::command]
pub async fn comprehensive_active_analysis(
    app: tauri::AppHandle,
    portfolio_data: String,
    benchmark_data: String,
) -> Result<String, String> {
    let args = vec![
        "comprehensive_analysis".to_string(),
        portfolio_data,
        benchmark_data,
    ];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Manager selection analysis
#[tauri::command]
pub async fn analyze_manager_selection(
    app: tauri::AppHandle,
    manager_candidates: String,
) -> Result<String, String> {
    let args = vec!["manager_selection".to_string(), manager_candidates];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ============================================================================
// Risk Management Commands
// ============================================================================

/// Comprehensive risk analysis
#[tauri::command]
pub async fn comprehensive_risk_analysis(
    app: tauri::AppHandle,
    returns_data: String,
    weights: Option<String>,
    portfolio_value: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["comprehensive_risk_analysis".to_string(), returns_data];

    if let Some(w) = weights {
        args.push(w);
    } else {
        args.push("null".to_string());
    }

    if let Some(pv) = portfolio_value {
        args.push(pv.to_string());
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/risk_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ============================================================================
// Portfolio Planning Commands
// ============================================================================

/// Strategic asset allocation
#[tauri::command]
pub async fn strategic_asset_allocation(
    app: tauri::AppHandle,
    age: i32,
    risk_tolerance: String,
    time_horizon: i32,
) -> Result<String, String> {
    let args = vec![
        "asset_allocation".to_string(),
        age.to_string(),
        risk_tolerance,
        time_horizon.to_string(),
    ];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_planning.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Retirement planning calculator
#[tauri::command]
pub async fn calculate_retirement_planning(
    app: tauri::AppHandle,
    current_age: i32,
    retirement_age: i32,
    current_savings: f64,
    annual_contribution: f64,
) -> Result<String, String> {
    let args = vec![
        "retirement_planning".to_string(),
        current_age.to_string(),
        retirement_age.to_string(),
        current_savings.to_string(),
        annual_contribution.to_string(),
    ];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_planning.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// ============================================================================
// Economics & Markets Commands
// ============================================================================

/// Comprehensive economics and markets analysis
#[tauri::command]
pub async fn comprehensive_economics_analysis(
    app: tauri::AppHandle,
    cycle_phase: String,
    economic_data: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["comprehensive_analysis".to_string(), cycle_phase];

    if let Some(data) = economic_data {
        args.push(data);
    } else {
        args.push("{}".to_string());
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/economics_markets.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Business cycle analysis
#[tauri::command]
pub async fn analyze_business_cycle(
    app: tauri::AppHandle,
    cycle_phase: String,
) -> Result<String, String> {
    let args = vec!["business_cycle_analysis".to_string(), cycle_phase];

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/economics_markets.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

/// Equity risk premium analysis
#[tauri::command]
pub async fn analyze_equity_risk_premium(
    app: tauri::AppHandle,
    risk_free_rate: Option<f64>,
    market_risk_premium: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["equity_risk_premium".to_string()];

    if let Some(rfr) = risk_free_rate {
        args.push(rfr.to_string());

        if let Some(mrp) = market_risk_premium {
            args.push(mrp.to_string());
        }
    }

    let script_path = get_script_path(&app, "Analytics/portfolioManagement/economics_markets.py")?;
    python_runtime::execute_python_script(&script_path, args)
}
