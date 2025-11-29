// AI Agents Commands - Economic, Geopolitical, Hedge Fund, and Investor agents
#![allow(dead_code)]
use crate::utils::python::execute_python_command;

// ECONOMIC AGENTS

/// Execute Economic Agent CLI
#[tauri::command]
pub async fn execute_economic_agent(
    app: tauri::AppHandle,
    agent_type: String,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let script_path = format!("agents/EconomicAgents/{}_agent_cli.py", agent_type);
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    execute_python_command(&app, &script_path, &cmd_args)
}

#[tauri::command]
pub async fn run_capitalism_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/capitalism_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_keynesian_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/keynesian_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_neoliberal_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/neoliberal_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_socialism_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/socialism_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_mercantilist_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/mercantilist_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_mixed_economy_agent(
    app: tauri::AppHandle,
    analysis_data: String,
    parameters: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), analysis_data];
    if let Some(p) = parameters {
        args.push(p);
    }
    execute_python_command(&app, "agents/EconomicAgents/mixed_economy_agent_cli.py", &args)
}

// GEOPOLITICS AGENTS

#[tauri::command]
pub async fn execute_geopolitics_agent(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    execute_python_command(&app, "agents/GeopoliticsAgents/geopolitics_agent_cli.py", &cmd_args)
}

// Grand Chessboard Agents
#[tauri::command]
pub async fn run_eurasian_chessboard_agent(
    app: tauri::AppHandle,
    region_data: String,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), region_data];
    if let Some(at) = analysis_type {
        args.push(at);
    }
    execute_python_command(&app, "agents/GeopoliticsAgents/GrandChessboardAgents/chessboard_agents/eurasian_chessboard_master_agent.py", &args)
}

#[tauri::command]
pub async fn run_geostrategic_players_agent(
    app: tauri::AppHandle,
    player_data: String,
    strategy_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), player_data];
    if let Some(sp) = strategy_params {
        args.push(sp);
    }
    execute_python_command(&app, "agents/GeopoliticsAgents/GrandChessboardAgents/chessboard_agents/geostrategic_players_agent.py", &args)
}

#[tauri::command]
pub async fn run_eurasian_balkans_agent(
    app: tauri::AppHandle,
    region_data: String,
    analysis_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), region_data];
    if let Some(ap) = analysis_params {
        args.push(ap);
    }
    execute_python_command(&app, "agents/GeopoliticsAgents/GrandChessboardAgents/chessboard_agents/eurasian_balkans_agent.py", &args)
}

// Prisoners of Geography Agents
#[tauri::command]
pub async fn run_geography_agent(
    app: tauri::AppHandle,
    region: String,
    geographic_data: String,
    analysis_type: Option<String>,
) -> Result<String, String> {
    let script_path = format!("agents/GeopoliticsAgents/PrisonersOfGeographyAgents/region_agents/{}_geography_agent.py", region);
    let mut args = vec!["analyze".to_string(), geographic_data];
    if let Some(at) = analysis_type {
        args.push(at);
    }
    execute_python_command(&app, &script_path, &args)
}

// World Order Agents
#[tauri::command]
pub async fn run_world_order_agent(
    app: tauri::AppHandle,
    civilization: String,
    order_data: String,
    analysis_params: Option<String>,
) -> Result<String, String> {
    let script_path = format!("agents/GeopoliticsAgents/World_Order_Agents/civilization_agents/{}_world_agent.py", civilization);
    let mut args = vec!["analyze".to_string(), order_data];
    if let Some(ap) = analysis_params {
        args.push(ap);
    }
    execute_python_command(&app, &script_path, &args)
}

#[tauri::command]
pub async fn run_westphalian_europe_agent(
    app: tauri::AppHandle,
    europe_data: String,
    diplomatic_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), europe_data];
    if let Some(dp) = diplomatic_params {
        args.push(dp);
    }
    execute_python_command(&app, "agents/GeopoliticsAgents/World_Order_Agents/civilization_agents/westphalian_europe_agent.py", &args)
}

#[tauri::command]
pub async fn run_balance_power_agent(
    app: tauri::AppHandle,
    power_data: String,
    balance_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), power_data];
    if let Some(bp) = balance_params {
        args.push(bp);
    }
    execute_python_command(&app, "agents/GeopoliticsAgents/World_Order_Agents/civilization_agents/balance_power_agent.py", &args)
}

// HEDGE FUND AGENTS

#[tauri::command]
pub async fn execute_hedge_fund_agent(
    app: tauri::AppHandle,
    command: String,
    args: Vec<String>,
) -> Result<String, String> {
    let mut cmd_args = vec![command];
    cmd_args.extend(args);
    execute_python_command(&app, "agents/hedgeFundAgents/hedge_fund_agent_cli.py", &cmd_args)
}

#[tauri::command]
pub async fn run_bridgewater_agent(
    app: tauri::AppHandle,
    market_data: String,
    strategy_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(sp) = strategy_params {
        args.push(sp);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/bridgewater_associates_hedge_fund_agent/bridgewater_associates_agent.py", &args)
}

#[tauri::command]
pub async fn run_citadel_agent(
    app: tauri::AppHandle,
    market_data: String,
    trading_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(tp) = trading_params {
        args.push(tp);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/citadel_hedge_fund_agent/citadel_agent.py", &args)
}

#[tauri::command]
pub async fn run_renaissance_agent(
    app: tauri::AppHandle,
    market_data: String,
    quant_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(qp) = quant_params {
        args.push(qp);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/renaissance_technologies_hedge_fund_agent/renaissance_technologies_agent.py", &args)
}

#[tauri::command]
pub async fn run_de_shaw_agent(
    app: tauri::AppHandle,
    market_data: String,
    computational_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(cp) = computational_params {
        args.push(cp);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/de_shaw_hedge_fund_agent/de_shaw_agent.py", &args)
}

#[tauri::command]
pub async fn run_two_sigma_agent(
    app: tauri::AppHandle,
    market_data: String,
    ml_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(mlp) = ml_params {
        args.push(mlp);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/two_sigma_hedge_fund_agent/two_sigma_agent.py", &args)
}

#[tauri::command]
pub async fn run_elliott_management_agent(
    app: tauri::AppHandle,
    market_data: String,
    activist_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), market_data];
    if let Some(ap) = activist_params {
        args.push(ap);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/elliott_management_hedge_fund_agent/elliott_management_agent.py", &args)
}

#[tauri::command]
pub async fn run_fincept_hedge_fund_orchestrator(
    app: tauri::AppHandle,
    market_data: String,
    orchestration_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["orchestrate".to_string(), market_data];
    if let Some(op) = orchestration_params {
        args.push(op);
    }
    execute_python_command(&app, "agents/hedgeFundAgents/fincept_hedge_fund/agent_orchestrator.py", &args)
}

// TRADER/INVESTOR AGENTS

#[tauri::command]
pub async fn run_warren_buffett_agent(
    app: tauri::AppHandle,
    investment_data: String,
    value_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(vp) = value_params {
        args.push(vp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/warren_buffett_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_benjamin_graham_agent(
    app: tauri::AppHandle,
    investment_data: String,
    value_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(vp) = value_params {
        args.push(vp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/benjamin_graham_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_charlie_munger_agent(
    app: tauri::AppHandle,
    investment_data: String,
    mental_models: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(mm) = mental_models {
        args.push(mm);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/charlie_munger_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_seth_klarman_agent(
    app: tauri::AppHandle,
    investment_data: String,
    margin_of_safety: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(mos) = margin_of_safety {
        args.push(mos);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/seth_klarman_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_howard_marks_agent(
    app: tauri::AppHandle,
    investment_data: String,
    cycle_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(cp) = cycle_params {
        args.push(cp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/howard_marks_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_joel_greenblatt_agent(
    app: tauri::AppHandle,
    investment_data: String,
    magic_formula_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(mfp) = magic_formula_params {
        args.push(mfp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/joel_greenblatt_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_david_einhorn_agent(
    app: tauri::AppHandle,
    investment_data: String,
    short_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(sp) = short_params {
        args.push(sp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/david_einhorn_agent_cli.py", &args)
}

#[tauri::command]
pub async fn run_bill_miller_agent(
    app: tauri::AppHandle,
    investment_data: String,
    contrarian_params: Option<String>,
) -> Result<String, String> {
    let mut args = vec!["analyze".to_string(), investment_data];
    if let Some(cp) = contrarian_params {
        args.push(cp);
    }
    execute_python_command(&app, "agents/TraderInvestorsAgent/bill_miller_agent_cli.py", &args)
}
