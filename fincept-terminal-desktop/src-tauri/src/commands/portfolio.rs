// Portfolio Analytics commands
use crate::utils::python::execute_python_command;
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

    execute_python_command(&app, "portfolio_analytics_service.py", &args)
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

    execute_python_command(&app, "portfolio_analytics_service.py", &args)
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

    execute_python_command(&app, "portfolio_analytics_service.py", &args)
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
    execute_python_command(&app, "portfolio_management_service.py", &args)
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
    execute_python_command(&app, "portfolio_management_service.py", &args)
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
    execute_python_command(&app, "portfolio_management_service.py", &args)
}

/// Analyze behavioral biases
#[tauri::command]
pub async fn analyze_behavioral_biases(
    app: tauri::AppHandle,
    portfolio_data: String,
) -> Result<String, String> {
    let args = vec!["behavioral_analysis".to_string(), portfolio_data];
    execute_python_command(&app, "portfolio_management_service.py", &args)
}

/// Analyze ETF costs
#[tauri::command]
pub async fn analyze_etf_costs(
    app: tauri::AppHandle,
    symbols: String,
    expense_ratios: String,
) -> Result<String, String> {
    let args = vec!["etf_analysis".to_string(), symbols, expense_ratios];
    execute_python_command(&app, "portfolio_management_service.py", &args)
}
