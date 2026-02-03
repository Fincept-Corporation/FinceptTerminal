// Analytics Module Commands - Portfolio optimization and technical analysis
#![allow(dead_code)]
use crate::python;


#[tauri::command]
pub async fn execute_python_command(
    app: tauri::AppHandle,
    script: String,
    args: Vec<String>,
) -> Result<String, String> {
    eprintln!("[execute_python_command] Script: {}", script);
    eprintln!("[execute_python_command] Args: {:?}", args);

    let result = python::execute(&app, &script, args).await;

    match &result {
        Ok(output) => {
            eprintln!("[execute_python_command] Success - Output length: {}", output.len());
            eprintln!("[execute_python_command] Output preview: {}",
                if output.len() > 200 { &output[..200] } else { output });
        }
        Err(e) => {
            eprintln!("[execute_python_command] Error: {}", e);
        }
    }

    result
}


#[tauri::command]
pub async fn execute_technical_indicators(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);


    python::execute(&app, "Analytics/technical_indicators.py", cmd_args).await
}


#[tauri::command]
pub async fn execute_pyportfolioopt(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    python::execute(&app, "Analytics/pyportfolioopt_wrapper/core.py", cmd_args).await
}


#[tauri::command]
pub async fn execute_riskfolio(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    python::execute(&app, "Analytics/riskfoliolib_wrapper.py", cmd_args).await
}


#[tauri::command]
pub async fn execute_skfolio(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    python::execute(&app, "Analytics/skfolio_wrapper.py", cmd_args).await
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
    python::execute(&app, "Analytics/derivatives/options.py", args).await
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
    python::execute(&app, "Analytics/derivatives/arbitrage.py", args).await
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
    python::execute(&app, "Analytics/derivatives/forward_commitments.py", args).await
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
    python::execute(&app, "Analytics/economics/currency_analysis.py", args).await
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
    python::execute(&app, "Analytics/economics/growth_analysis.py", args).await
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
    python::execute(&app, "Analytics/economics/policy_analysis.py", args).await
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
    python::execute(&app, "Analytics/economics/market_cycles.py", args).await
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
    python::execute(&app, "Analytics/economics/trade_geopolitics.py", args).await
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
    python::execute(&app, "Analytics/economics/capital_flows.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/equity_valuation/dcf_models.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/equity_valuation/dividend_models.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/equity_valuation/multiples_valuation.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/company_analysis/fundamental_analysis.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/company_analysis/industry_analysis.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/company_analysis/forecasting.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/market_analysis/market_efficiency.py", args).await
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
    python::execute(&app, "Analytics/equityInvestment/market_analysis/index_analysis.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/risk_management.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/portfolio_analytics.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/portfolio_management.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/portfolio_planning.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/active_management.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/behavioral_finance.py", args).await
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
    python::execute(&app, "Analytics/portfolioManagement/etf_analytics.py", args).await
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
    python::execute(&app, "Analytics/quant/quant_modules_3042.py", args).await
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
    python::execute(&app, "Analytics/quant/rate_calculations.py", args).await
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
    python::execute(&app, "Analytics/economics_wrapper.py", args).await
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
    python::execute(&app, "Analytics/statsmodels_cli.py", args).await
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
    python::execute(&app, "Analytics/quant_analytics_cli.py", args).await
}

// Alternative Investment analytics commands
#[tauri::command]
pub async fn analyze_digital_assets(
    app: tauri::AppHandle,
    asset_data: String,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["digital-assets".to_string(), "--data".to_string(), asset_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

#[tauri::command]
pub async fn analyze_hedge_funds(
    app: tauri::AppHandle,
    fund_data: String,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["hedge-funds".to_string(), "--data".to_string(), fund_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

#[tauri::command]
pub async fn analyze_real_estate(
    app: tauri::AppHandle,
    property_data: String,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["real-estate".to_string(), "--data".to_string(), property_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

#[tauri::command]
pub async fn analyze_private_capital(
    app: tauri::AppHandle,
    investment_data: String,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["private-capital".to_string(), "--data".to_string(), investment_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

#[tauri::command]
pub async fn analyze_natural_resources(
    app: tauri::AppHandle,
    resource_data: String,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["natural-resources".to_string(), "--data".to_string(), resource_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

// Performance metrics command
#[tauri::command]
pub async fn analyze_performance_metrics(
    app: tauri::AppHandle,
    returns_data: String,
    benchmark_data: Option<String>,
    method: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["performance".to_string(), "--returns".to_string(), returns_data];
    if let Some(b) = benchmark_data {
        args.push("--benchmark".to_string());
        args.push(b);
    }
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}

// Risk analysis command
#[tauri::command]
pub async fn analyze_investment_risk(
    app: tauri::AppHandle,
    returns_data: String,
    method: Option<String>,
    confidence_level: Option<f64>,
) -> Result<String, String> {
    let mut args = vec!["risk".to_string(), "--returns".to_string(), returns_data];
    if let Some(m) = method {
        args.push("--method".to_string());
        args.push(m);
    }
    if let Some(cl) = confidence_level {
        args.push("--confidence-level".to_string());
        args.push(cl.to_string());
    }
    python::execute(&app, "Analytics/alternateInvestment/cli.py", args).await
}
