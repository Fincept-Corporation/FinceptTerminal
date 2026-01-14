// Analytics Module Commands - Portfolio optimization and technical analysis
#![allow(dead_code)]
use crate::utils::python::get_script_path;
use crate::python_runtime;

/// Execute Technical Indicators command with PyO3
#[tauri::command]
pub async fn execute_technical_indicators(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);

    // Get script path and execute with PyO3
    let script_path = get_script_path(&app, "Analytics/technical_indicators.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

/// Execute PyPortfolioOpt wrapper command with PyO3
#[tauri::command]
pub async fn execute_pyportfolioopt(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    let script_path = get_script_path(&app, "Analytics/pyportfolioopt_wrapper/core.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

/// Execute RiskFolio wrapper command with PyO3
#[tauri::command]
pub async fn execute_riskfolio(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    let script_path = get_script_path(&app, "Analytics/riskfoliolib_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

/// Execute Skfolio wrapper command with PyO3
#[tauri::command]
pub async fn execute_skfolio(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    let script_path = get_script_path(&app, "Analytics/skfolio_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, cmd_args)
}

// Module-specific commands for alternateInvestment
#[tauri::command]
pub async fn analyze_digital_assets(
    app: tauri::AppHandle,
    asset_data: String,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["digital_assets".to_string(), asset_data];
    if let Some(at) = analysis_type {
        args.push(at);
    }
    let script_path = get_script_path(&app, "Analytics/alternateInvestment/digital_assets.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_hedge_funds(
    app: tauri::AppHandle,
    fund_data: String,
    metrics: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["hedge_funds".to_string(), fund_data];
    if let Some(m) = metrics {
        args.push(m);
    }
    let script_path = get_script_path(&app, "Analytics/alternateInvestment/hedge_funds.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_real_estate(
    app: tauri::AppHandle,
    property_data: String,
    valuation_method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["real_estate".to_string(), property_data];
    if let Some(vm) = valuation_method {
        args.push(vm);
    }
    let script_path = get_script_path(&app, "Analytics/alternateInvestment/real_estate.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_private_capital(
    app: tauri::AppHandle,
    investment_data: String,
    analysis_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["private_capital".to_string(), investment_data];
    if let Some(ap) = analysis_params {
        args.push(ap);
    }
    let script_path = get_script_path(&app, "Analytics/alternateInvestment/private_capital.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_natural_resources(
    app: tauri::AppHandle,
    resource_data: String,
    commodity_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["natural_resources".to_string(), resource_data];
    if let Some(ct) = commodity_type {
        args.push(ct);
    }
    let script_path = get_script_path(&app, "Analytics/alternateInvestment/natural_resources.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// Derivatives analytics commands
#[tauri::command]
pub async fn price_options(
    app: tauri::AppHandle,
    option_params: String,
    pricing_model: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["price".to_string(), option_params];
    if let Some(pm) = pricing_model {
        args.push(pm);
    }
    let script_path = get_script_path(&app, "Analytics/derivatives/options.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_arbitrage(
    app: tauri::AppHandle,
    market_data: String,
    arbitrage_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(at) = arbitrage_type {
        args.push(at);
    }
    let script_path = get_script_path(&app, "Analytics/derivatives/arbitrage.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_forward_commitments(
    app: tauri::AppHandle,
    contract_data: String,
    contract_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), contract_data];
    if let Some(ct) = contract_type {
        args.push(ct);
    }
    let script_path = get_script_path(&app, "Analytics/derivatives/forward_commitments.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// Economics analytics commands
#[tauri::command]
pub async fn analyze_currency(
    app: tauri::AppHandle,
    currency_data: String,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), currency_data];
    if let Some(at) = analysis_type {
        args.push(at);
    }
    let script_path = get_script_path(&app, "Analytics/economics/currency_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_growth(
    app: tauri::AppHandle,
    economic_data: String,
    indicators: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), economic_data];
    if let Some(ind) = indicators {
        args.push(ind);
    }
    let script_path = get_script_path(&app, "Analytics/economics/growth_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_policy(
    app: tauri::AppHandle,
    policy_data: String,
    policy_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), policy_data];
    if let Some(pt) = policy_type {
        args.push(pt);
    }
    let script_path = get_script_path(&app, "Analytics/economics/policy_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_market_cycles(
    app: tauri::AppHandle,
    market_data: String,
    cycle_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(cp) = cycle_params {
        args.push(cp);
    }
    let script_path = get_script_path(&app, "Analytics/economics/market_cycles.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_trade_geopolitics(
    app: tauri::AppHandle,
    trade_data: String,
    region: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), trade_data];
    if let Some(r) = region {
        args.push(r);
    }
    let script_path = get_script_path(&app, "Analytics/economics/trade_geopolitics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_capital_flows(
    app: tauri::AppHandle,
    flow_data: String,
    flow_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), flow_data];
    if let Some(ft) = flow_type {
        args.push(ft);
    }
    let script_path = get_script_path(&app, "Analytics/economics/capital_flows.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// Equity Investment analytics commands
#[tauri::command]
pub async fn calculate_dcf_valuation(
    app: tauri::AppHandle,
    company_data: String,
    dcf_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["dcf".to_string(), company_data];
    if let Some(dp) = dcf_params {
        args.push(dp);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/equity_valuation/dcf_models.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn calculate_dividend_valuation(
    app: tauri::AppHandle,
    dividend_data: String,
    model_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["dividend".to_string(), dividend_data];
    if let Some(mt) = model_type {
        args.push(mt);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/equity_valuation/dividend_models.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn calculate_multiples_valuation(
    app: tauri::AppHandle,
    company_data: String,
    multiples: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["multiples".to_string(), company_data];
    if let Some(m) = multiples {
        args.push(m);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/equity_valuation/multiples_valuation.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_fundamental(
    app: tauri::AppHandle,
    financial_data: String,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["fundamental".to_string(), financial_data];
    if let Some(at) = analysis_type {
        args.push(at);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/company_analysis/fundamental_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_industry(
    app: tauri::AppHandle,
    industry_data: String,
    industry_code: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["industry".to_string(), industry_data];
    if let Some(ic) = industry_code {
        args.push(ic);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/company_analysis/industry_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn forecast_financials(
    app: tauri::AppHandle,
    historical_data: String,
    forecast_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["forecast".to_string(), historical_data];
    if let Some(fp) = forecast_params {
        args.push(fp);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/company_analysis/forecasting.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_market_efficiency(
    app: tauri::AppHandle,
    market_data: String,
    efficiency_tests: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["efficiency".to_string(), market_data];
    if let Some(et) = efficiency_tests {
        args.push(et);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/market_analysis/market_efficiency.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_index(
    app: tauri::AppHandle,
    index_data: String,
    index_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["index".to_string(), index_data];
    if let Some(it) = index_type {
        args.push(it);
    }
    let script_path = get_script_path(&app, "Analytics/equityInvestment/market_analysis/index_analysis.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// Portfolio Management analytics commands
#[tauri::command]
pub async fn analyze_portfolio_risk(
    app: tauri::AppHandle,
    portfolio_data: String,
    risk_measures: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["risk".to_string(), portfolio_data];
    if let Some(rm) = risk_measures {
        args.push(rm);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/risk_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_portfolio_performance(
    app: tauri::AppHandle,
    portfolio_data: String,
    benchmark: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["performance".to_string(), portfolio_data];
    if let Some(b) = benchmark {
        args.push(b);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_analytics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn optimize_portfolio_management(
    app: tauri::AppHandle,
    portfolio_data: String,
    optimization_method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["optimize".to_string(), portfolio_data];
    if let Some(om) = optimization_method {
        args.push(om);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn plan_portfolio(
    app: tauri::AppHandle,
    client_profile: String,
    planning_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["plan".to_string(), client_profile];
    if let Some(pp) = planning_params {
        args.push(pp);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/portfolio_planning.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_active_management(
    app: tauri::AppHandle,
    fund_data: String,
    strategy_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["active".to_string(), fund_data];
    if let Some(st) = strategy_type {
        args.push(st);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/active_management.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_behavioral_finance(
    app: tauri::AppHandle,
    investor_data: String,
    bias_analysis: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["behavioral".to_string(), investor_data];
    if let Some(ba) = bias_analysis {
        args.push(ba);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/behavioral_finance.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn analyze_etf(
    app: tauri::AppHandle,
    etf_data: String,
    analysis_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["etf".to_string(), etf_data];
    if let Some(ap) = analysis_params {
        args.push(ap);
    }
    let script_path = get_script_path(&app, "Analytics/portfolioManagement/etf_analytics.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

// Quantitative analytics commands
#[tauri::command]
pub async fn calculate_quant_metrics(
    app: tauri::AppHandle,
    market_data: String,
    metrics: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["calculate".to_string(), market_data];
    if let Some(m) = metrics {
        args.push(m);
    }
    let script_path = get_script_path(&app, "Analytics/quant/quant_modules_3042.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn calculate_rates(
    app: tauri::AppHandle,
    rate_data: String,
    calculation_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["rates".to_string(), rate_data];
    if let Some(ct) = calculation_type {
        args.push(ct);
    }
    let script_path = get_script_path(&app, "Analytics/quant/rate_calculations.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn execute_economics_analytics(
    app: tauri::AppHandle,
    command: String,
    params: Option<String>,
    config: Option<String>,
) -> Result<String, String> {
    let mut args = vec![command];
    if let Some(p) = params {
        args.push(p);
    }
    if let Some(c) = config {
        args.push(c);
    }
    let script_path = get_script_path(&app, "Analytics/economics_wrapper.py")?;
    python_runtime::execute_python_script(&script_path, args)
}

#[tauri::command]
pub async fn execute_statsmodels_analytics(
    app: tauri::AppHandle,
    command: String,
    params: Option<String>,
) -> Result<String, String> {
    let mut args = vec![command];
    if let Some(p) = params {
        args.push(p);
    }
    crate::utils::python::execute_python_subprocess(
        &app,
        "Analytics/statsmodels_cli.py",
        &args,
        Some("statsmodels")
    )
}

#[tauri::command]
pub async fn execute_quant_analytics(
    app: tauri::AppHandle,
    command: String,
    params: Option<String>,
) -> Result<String, String> {
    let mut args = vec![command];
    if let Some(p) = params {
        args.push(p);
    }
    crate::utils::python::execute_python_subprocess(
        &app,
        "Analytics/quant_analytics_cli.py",
        &args,
        Some("quant_analytics")
    )
}
