// agents.rs - Python Agent Execution Commands
use serde::{Deserialize, Serialize};
use tauri::Manager;
use crate::utils::python::get_python_path;

#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AgentParameter {
    pub name: String,
    pub label: String,
    #[serde(rename = "type")]
    pub param_type: String,
    pub required: bool,
    pub default_value: Option<serde_json::Value>,
    pub description: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentMetadata {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub agent_type: String,
    pub category: String,
    pub description: String,
    pub script_path: String,
    pub parameters: Vec<AgentParameter>,
    pub required_inputs: Vec<String>,
    pub outputs: Vec<String>,
    pub icon: String,
    pub color: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct AgentExecutionResult {
    pub success: bool,
    pub data: Option<serde_json::Value>,
    pub error: Option<String>,
    pub execution_time_ms: u64,
    pub agent_type: String,
}

/// Map agent type to script path
fn get_agent_script_path(agent_type: &str) -> Option<&'static str> {
    match agent_type {
        // Trader/Investor Agents (CLI versions for Node Editor)
        "warren_buffett" => Some("agents/TraderInvestorsAgent/warren_buffett_agent_cli.py"),
        "charlie_munger" => Some("agents/TraderInvestorsAgent/charlie_munger_agent_cli.py"),
        "benjamin_graham" => Some("agents/TraderInvestorsAgent/benjamin_graham_agent_cli.py"),
        "seth_klarman" => Some("agents/TraderInvestorsAgent/seth_klarman_agent_cli.py"),
        "howard_marks" => Some("agents/TraderInvestorsAgent/howard_marks_agent_cli.py"),
        "joel_greenblatt" => Some("agents/TraderInvestorsAgent/joel_greenblatt_agent_cli.py"),
        "david_einhorn" => Some("agents/TraderInvestorsAgent/david_einhorn_agent_cli.py"),
        "bill_miller" => Some("agents/TraderInvestorsAgent/bill_miller_agent_cli.py"),
        "jean_marie_eveillard" => Some("agents/TraderInvestorsAgent/jean_marie_eveillard_agent_cli.py"),
        "marty_whitman" => Some("agents/TraderInvestorsAgent/marty_whitman_agent_cli.py"),

        // Hedge Fund Agents - All use unified CLI wrapper
        "macro_cycle" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "central_bank" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "behavioral" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "institutional_flow" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "innovation" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "geopolitical" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "currency" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "supply_chain" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "sentiment" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "regulatory" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "bridgewater" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "citadel" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "renaissance" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "two_sigma" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "de_shaw" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "elliott" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "pershing_square" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),
        "arq_capital" => Some("agents/hedgeFundAgents/hedge_fund_agent_cli.py"),

        // Economic Agents (CLI versions for Node Editor)
        "capitalism" => Some("agents/EconomicAgents/capitalism_agent_cli.py"),
        "keynesian" => Some("agents/EconomicAgents/keynesian_agent_cli.py"),
        "neoliberal" => Some("agents/EconomicAgents/neoliberal_agent_cli.py"),
        "socialism" => Some("agents/EconomicAgents/socialism_agent_cli.py"),
        "mixed_economy" => Some("agents/EconomicAgents/mixed_economy_agent_cli.py"),
        "mercantilist" => Some("agents/EconomicAgents/mercantilist_agent_cli.py"),

        // Geopolitics Agents - Prisoners of Geography (Tim Marshall)
        "russia_geography" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "china_geography" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "usa_geography" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "europe_geography" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "middle_east_geography" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),

        // Geopolitics Agents - World Order (Kissinger)
        "american_world" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "chinese_world" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "balance_power" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "islamic_world" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "nuclear_order" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),

        // Geopolitics Agents - Grand Chessboard (Brzezinski)
        "eurasian_balkans" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "democratic_bridgehead" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),
        "far_eastern_anchor" => Some("agents/GeopoliticsAgents/geopolitics_agent_cli.py"),

        _ => None,
    }
}

/// Get geopolitics agent parameters (shared across all geopolitics agents)
fn get_geopolitics_agent_parameters() -> Vec<AgentParameter> {
    vec![
        AgentParameter {
            name: "agent_type".to_string(),
            label: "Agent Type".to_string(),
            param_type: "string".to_string(),
            required: true,
            default_value: Some(serde_json::json!("russia_geography")),
            description: Some("Specific geopolitics agent to use".to_string()),
        },
        AgentParameter {
            name: "event".to_string(),
            label: "Geopolitical Event".to_string(),
            param_type: "string".to_string(),
            required: true,
            default_value: Some(serde_json::json!("Ukraine conflict 2024")),
            description: Some("Event or situation to analyze".to_string()),
        },
        AgentParameter {
            name: "framework".to_string(),
            label: "Analysis Framework".to_string(),
            param_type: "string".to_string(),
            required: false,
            default_value: Some(serde_json::json!("auto")),
            description: Some("Framework: auto, geography, world_order, chessboard".to_string()),
        },
    ]
}

/// Get hedge fund agent parameters (shared across all hedge fund agents)
fn get_hedge_fund_agent_parameters() -> Vec<AgentParameter> {
    vec![
        AgentParameter {
            name: "agent_type".to_string(),
            label: "Agent Type".to_string(),
            param_type: "string".to_string(),
            required: true,
            default_value: Some(serde_json::json!("bridgewater")),
            description: Some("Specific hedge fund agent to use".to_string()),
        },
        AgentParameter {
            name: "tickers".to_string(),
            label: "Stock Tickers".to_string(),
            param_type: "array".to_string(),
            required: true,
            default_value: Some(serde_json::json!(["SPY"])),
            description: Some("List of stock tickers to analyze".to_string()),
        },
    ]
}

/// Get economic agent parameters (shared across all economic agents)
fn get_economic_agent_parameters() -> Vec<AgentParameter> {
    vec![
        AgentParameter {
            name: "gdp".to_string(),
            label: "GDP Growth (%)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(3.0)),
            description: Some("Annual GDP growth rate".to_string()),
        },
        AgentParameter {
            name: "inflation".to_string(),
            label: "Inflation Rate (%)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(2.5)),
            description: Some("Annual inflation rate".to_string()),
        },
        AgentParameter {
            name: "unemployment".to_string(),
            label: "Unemployment Rate (%)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(5.0)),
            description: Some("Current unemployment rate".to_string()),
        },
        AgentParameter {
            name: "interest_rate".to_string(),
            label: "Interest Rate (%)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(5.0)),
            description: Some("Central bank interest rate".to_string()),
        },
        AgentParameter {
            name: "trade_balance".to_string(),
            label: "Trade Balance (% GDP)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(0.0)),
            description: Some("Trade balance as % of GDP".to_string()),
        },
        AgentParameter {
            name: "government_spending".to_string(),
            label: "Gov Spending (% GDP)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(35.0)),
            description: Some("Government spending as % of GDP".to_string()),
        },
        AgentParameter {
            name: "tax_rate".to_string(),
            label: "Tax Rate (%)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(30.0)),
            description: Some("Average tax rate".to_string()),
        },
        AgentParameter {
            name: "private_investment".to_string(),
            label: "Private Investment (% GDP)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(20.0)),
            description: Some("Private sector investment as % of GDP".to_string()),
        },
        AgentParameter {
            name: "consumer_confidence".to_string(),
            label: "Consumer Confidence (0-100)".to_string(),
            param_type: "number".to_string(),
            required: false,
            default_value: Some(serde_json::json!(65.0)),
            description: Some("Consumer confidence index".to_string()),
        },
    ]
}

/// Get predefined agent metadata (internal helper)
fn get_agent_metadata_internal(agent_type: &str) -> Option<AgentMetadata> {
    let script_path = get_agent_script_path(agent_type)?;

    // Standard ticker parameters for trader agents
    let standard_ticker_params = vec![
        AgentParameter {
            name: "tickers".to_string(),
            label: "Stock Tickers".to_string(),
            param_type: "array".to_string(),
            required: true,
            default_value: Some(serde_json::json!(["AAPL"])),
            description: Some("List of stock tickers to analyze".to_string()),
        },
        AgentParameter {
            name: "end_date".to_string(),
            label: "Analysis Date".to_string(),
            param_type: "string".to_string(),
            required: false,
            default_value: Some(serde_json::json!("2024-12-31")),
            description: Some("Date for analysis (YYYY-MM-DD)".to_string()),
        },
    ];

    match agent_type {
        // === TRADER/INVESTOR AGENTS ===
        "warren_buffett" => Some(AgentMetadata {
            id: "warren_buffett".to_string(),
            name: "Warren Buffett".to_string(),
            agent_type: "warren_buffett".to_string(),
            category: "trader".to_string(),
            description: "Value investing with economic moats and long-term focus".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💰".to_string(),
            color: "#22c55e".to_string(),
        }),

        "charlie_munger" => Some(AgentMetadata {
            id: "charlie_munger".to_string(),
            name: "Charlie Munger".to_string(),
            agent_type: "charlie_munger".to_string(),
            category: "trader".to_string(),
            description: "Multidisciplinary thinking and rational decision-making".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🧠".to_string(),
            color: "#3b82f6".to_string(),
        }),

        "benjamin_graham" => Some(AgentMetadata {
            id: "benjamin_graham".to_string(),
            name: "Benjamin Graham".to_string(),
            agent_type: "benjamin_graham".to_string(),
            category: "trader".to_string(),
            description: "Deep value investing with margin of safety".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "📚".to_string(),
            color: "#8b5cf6".to_string(),
        }),

        "seth_klarman" => Some(AgentMetadata {
            id: "seth_klarman".to_string(),
            name: "Seth Klarman".to_string(),
            agent_type: "seth_klarman".to_string(),
            category: "trader".to_string(),
            description: "Risk management and distressed value opportunities".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🛡️".to_string(),
            color: "#ef4444".to_string(),
        }),

        "howard_marks" => Some(AgentMetadata {
            id: "howard_marks".to_string(),
            name: "Howard Marks".to_string(),
            agent_type: "howard_marks".to_string(),
            category: "trader".to_string(),
            description: "Market cycles and second-level thinking".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🔄".to_string(),
            color: "#f59e0b".to_string(),
        }),

        "joel_greenblatt" => Some(AgentMetadata {
            id: "joel_greenblatt".to_string(),
            name: "Joel Greenblatt".to_string(),
            agent_type: "joel_greenblatt".to_string(),
            category: "trader".to_string(),
            description: "Magic formula: high returns on capital at low prices".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "✨".to_string(),
            color: "#10b981".to_string(),
        }),

        "david_einhorn" => Some(AgentMetadata {
            id: "david_einhorn".to_string(),
            name: "David Einhorn".to_string(),
            agent_type: "david_einhorn".to_string(),
            category: "trader".to_string(),
            description: "Forensic accounting and value/short analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🔍".to_string(),
            color: "#ec4899".to_string(),
        }),

        "bill_miller" => Some(AgentMetadata {
            id: "bill_miller".to_string(),
            name: "Bill Miller".to_string(),
            agent_type: "bill_miller".to_string(),
            category: "trader".to_string(),
            description: "Contrarian value with technology focus".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🎯".to_string(),
            color: "#f59e0b".to_string(),
        }),

        "jean_marie_eveillard" => Some(AgentMetadata {
            id: "jean_marie_eveillard".to_string(),
            name: "Jean-Marie Eveillard".to_string(),
            agent_type: "jean_marie_eveillard".to_string(),
            category: "trader".to_string(),
            description: "Global deep value and patient capital".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🌍".to_string(),
            color: "#06b6d4".to_string(),
        }),

        "marty_whitman" => Some(AgentMetadata {
            id: "marty_whitman".to_string(),
            name: "Marty Whitman".to_string(),
            agent_type: "marty_whitman".to_string(),
            category: "trader".to_string(),
            description: "Safe & cheap distressed value investing".to_string(),
            script_path: script_path.to_string(),
            parameters: standard_ticker_params.clone(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💎".to_string(),
            color: "#a855f7".to_string(),
        }),

        // === HEDGE FUND AGENTS - Multi-Strategy ===
        "macro_cycle" => Some(AgentMetadata {
            id: "macro_cycle".to_string(),
            name: "Macro Cycle Agent".to_string(),
            agent_type: "macro_cycle".to_string(),
            category: "hedge-fund".to_string(),
            description: "Business cycle analysis and economic phase identification".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🔄".to_string(),
            color: "#3b82f6".to_string(),
        }),

        "central_bank" => Some(AgentMetadata {
            id: "central_bank".to_string(),
            name: "Central Bank Agent".to_string(),
            agent_type: "central_bank".to_string(),
            category: "hedge-fund".to_string(),
            description: "Monetary policy analysis and interest rate forecasting".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🏦".to_string(),
            color: "#06b6d4".to_string(),
        }),

        "behavioral" => Some(AgentMetadata {
            id: "behavioral".to_string(),
            name: "Behavioral Agent".to_string(),
            agent_type: "behavioral".to_string(),
            category: "hedge-fund".to_string(),
            description: "Market psychology and sentiment analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🧠".to_string(),
            color: "#ec4899".to_string(),
        }),

        "institutional_flow" => Some(AgentMetadata {
            id: "institutional_flow".to_string(),
            name: "Institutional Flow Agent".to_string(),
            agent_type: "institutional_flow".to_string(),
            category: "hedge-fund".to_string(),
            description: "Smart money tracking and institutional positioning".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💼".to_string(),
            color: "#10b981".to_string(),
        }),

        "innovation" => Some(AgentMetadata {
            id: "innovation".to_string(),
            name: "Innovation Agent".to_string(),
            agent_type: "innovation".to_string(),
            category: "hedge-fund".to_string(),
            description: "Disruptive technology and innovation analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💡".to_string(),
            color: "#f59e0b".to_string(),
        }),

        "geopolitical" => Some(AgentMetadata {
            id: "geopolitical".to_string(),
            name: "Geopolitical Agent".to_string(),
            agent_type: "geopolitical".to_string(),
            category: "hedge-fund".to_string(),
            description: "Geopolitical risk and opportunity analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🌍".to_string(),
            color: "#8b5cf6".to_string(),
        }),

        "currency" => Some(AgentMetadata {
            id: "currency".to_string(),
            name: "Currency Agent".to_string(),
            agent_type: "currency".to_string(),
            category: "hedge-fund".to_string(),
            description: "FX markets and currency dynamics".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💱".to_string(),
            color: "#06b6d4".to_string(),
        }),

        "supply_chain" => Some(AgentMetadata {
            id: "supply_chain".to_string(),
            name: "Supply Chain Agent".to_string(),
            agent_type: "supply_chain".to_string(),
            category: "hedge-fund".to_string(),
            description: "Supply chain disruptions and logistics".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🚚".to_string(),
            color: "#22c55e".to_string(),
        }),

        "sentiment" => Some(AgentMetadata {
            id: "sentiment".to_string(),
            name: "Sentiment Agent".to_string(),
            agent_type: "sentiment".to_string(),
            category: "hedge-fund".to_string(),
            description: "News and social media sentiment tracking".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "📊".to_string(),
            color: "#a855f7".to_string(),
        }),

        "regulatory" => Some(AgentMetadata {
            id: "regulatory".to_string(),
            name: "Regulatory Agent".to_string(),
            agent_type: "regulatory".to_string(),
            category: "hedge-fund".to_string(),
            description: "Regulatory changes and compliance impact".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "⚖️".to_string(),
            color: "#ef4444".to_string(),
        }),

        // === HEDGE FUND AGENTS - Famous Firms ===
        "bridgewater" => Some(AgentMetadata {
            id: "bridgewater".to_string(),
            name: "Bridgewater Associates".to_string(),
            agent_type: "bridgewater".to_string(),
            category: "hedge-fund".to_string(),
            description: "Ray Dalio: All Weather Portfolio + Economic Machine analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🌊".to_string(),
            color: "#2563eb".to_string(),
        }),

        "citadel" => Some(AgentMetadata {
            id: "citadel".to_string(),
            name: "Citadel".to_string(),
            agent_type: "citadel".to_string(),
            category: "hedge-fund".to_string(),
            description: "Ken Griffin: Multi-strategy global macro and quantitative trading".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🏰".to_string(),
            color: "#dc2626".to_string(),
        }),

        "renaissance" => Some(AgentMetadata {
            id: "renaissance".to_string(),
            name: "Renaissance Technologies".to_string(),
            agent_type: "renaissance".to_string(),
            category: "hedge-fund".to_string(),
            description: "Jim Simons: Quantitative + mathematical models".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🔬".to_string(),
            color: "#8b5cf6".to_string(),
        }),

        "two_sigma" => Some(AgentMetadata {
            id: "two_sigma".to_string(),
            name: "Two Sigma".to_string(),
            agent_type: "two_sigma".to_string(),
            category: "hedge-fund".to_string(),
            description: "Data science + machine learning driven investing".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🤖".to_string(),
            color: "#06b6d4".to_string(),
        }),

        "de_shaw" => Some(AgentMetadata {
            id: "de_shaw".to_string(),
            name: "D.E. Shaw".to_string(),
            agent_type: "de_shaw".to_string(),
            category: "hedge-fund".to_string(),
            description: "David Shaw: Computational finance and systematic trading".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "💻".to_string(),
            color: "#10b981".to_string(),
        }),

        "elliott" => Some(AgentMetadata {
            id: "elliott".to_string(),
            name: "Elliott Management".to_string(),
            agent_type: "elliott".to_string(),
            category: "hedge-fund".to_string(),
            description: "Paul Singer: Activist + distressed debt investing".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "⚡".to_string(),
            color: "#f59e0b".to_string(),
        }),

        "pershing_square" => Some(AgentMetadata {
            id: "pershing_square".to_string(),
            name: "Pershing Square".to_string(),
            agent_type: "pershing_square".to_string(),
            category: "hedge-fund".to_string(),
            description: "Bill Ackman: Concentrated activist value investing".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "🎯".to_string(),
            color: "#ec4899".to_string(),
        }),

        "arq_capital" => Some(AgentMetadata {
            id: "arq_capital".to_string(),
            name: "ARQ Capital".to_string(),
            agent_type: "arq_capital".to_string(),
            category: "hedge-fund".to_string(),
            description: "Quantitative multi-strategy with systematic risk management".to_string(),
            script_path: script_path.to_string(),
            parameters: get_hedge_fund_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["signals".to_string(), "analysis".to_string()],
            icon: "📐".to_string(),
            color: "#a855f7".to_string(),
        }),

        // === ECONOMIC SYSTEM AGENTS ===
        "capitalism" => Some(AgentMetadata {
            id: "capitalism".to_string(),
            name: "Capitalism".to_string(),
            agent_type: "capitalism".to_string(),
            category: "economic".to_string(),
            description: "Free market and competitive dynamics analysis".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["market_efficiency".to_string(), "policy_recommendations".to_string()],
            icon: "💼".to_string(),
            color: "#22c55e".to_string(),
        }),

        "keynesian" => Some(AgentMetadata {
            id: "keynesian".to_string(),
            name: "Keynesian".to_string(),
            agent_type: "keynesian".to_string(),
            category: "economic".to_string(),
            description: "Government intervention and fiscal policy focus".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["fiscal_impact".to_string(), "policy_recommendations".to_string()],
            icon: "🏛️".to_string(),
            color: "#3b82f6".to_string(),
        }),

        "neoliberal" => Some(AgentMetadata {
            id: "neoliberal".to_string(),
            name: "Neoliberal".to_string(),
            agent_type: "neoliberal".to_string(),
            category: "economic".to_string(),
            description: "Free markets, deregulation, and privatization".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["market_freedom".to_string(), "policy_recommendations".to_string()],
            icon: "📈".to_string(),
            color: "#f97316".to_string(),
        }),

        "socialism" => Some(AgentMetadata {
            id: "socialism".to_string(),
            name: "Socialism".to_string(),
            agent_type: "socialism".to_string(),
            category: "economic".to_string(),
            description: "Social ownership, equality, and workers' rights".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["equality_index".to_string(), "policy_recommendations".to_string()],
            icon: "🔴".to_string(),
            color: "#dc2626".to_string(),
        }),

        "mixed_economy" => Some(AgentMetadata {
            id: "mixed_economy".to_string(),
            name: "Mixed Economy".to_string(),
            agent_type: "mixed_economy".to_string(),
            category: "economic".to_string(),
            description: "Balanced approach combining private and public sectors".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["balance_score".to_string(), "policy_recommendations".to_string()],
            icon: "⚖️".to_string(),
            color: "#8b5cf6".to_string(),
        }),

        "mercantilist" => Some(AgentMetadata {
            id: "mercantilist".to_string(),
            name: "Mercantilist".to_string(),
            agent_type: "mercantilist".to_string(),
            category: "economic".to_string(),
            description: "Trade surplus and national economic power focus".to_string(),
            script_path: script_path.to_string(),
            parameters: get_economic_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["trade_strength".to_string(), "policy_recommendations".to_string()],
            icon: "⚓".to_string(),
            color: "#0891b2".to_string(),
        }),

        // === GEOPOLITICS AGENTS - Prisoners of Geography ===
        "russia_geography" => Some(AgentMetadata {
            id: "russia_geography".to_string(),
            name: "Russia Geography".to_string(),
            agent_type: "russia_geography".to_string(),
            category: "geopolitics".to_string(),
            description: "Tim Marshall: Geographic determinism - plains vulnerability, warm-water ports".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["geographic_analysis".to_string()],
            icon: "🏔️".to_string(),
            color: "#dc2626".to_string(),
        }),

        "china_geography" => Some(AgentMetadata {
            id: "china_geography".to_string(),
            name: "China Geography".to_string(),
            agent_type: "china_geography".to_string(),
            category: "geopolitics".to_string(),
            description: "Tim Marshall: Maritime vulnerability, unity challenges, Himalayan barrier".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["geographic_analysis".to_string()],
            icon: "🏯".to_string(),
            color: "#ef4444".to_string(),
        }),

        "usa_geography" => Some(AgentMetadata {
            id: "usa_geography".to_string(),
            name: "USA Geography".to_string(),
            agent_type: "usa_geography".to_string(),
            category: "geopolitics".to_string(),
            description: "Tim Marshall: Geographic advantages, natural moats, resource abundance".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["geographic_analysis".to_string()],
            icon: "🗽".to_string(),
            color: "#3b82f6".to_string(),
        }),

        "europe_geography" => Some(AgentMetadata {
            id: "europe_geography".to_string(),
            name: "Europe Geography".to_string(),
            agent_type: "europe_geography".to_string(),
            category: "geopolitics".to_string(),
            description: "Tim Marshall: Fragmentation vs integration, geographic barriers".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["geographic_analysis".to_string()],
            icon: "🏰".to_string(),
            color: "#8b5cf6".to_string(),
        }),

        "middle_east_geography" => Some(AgentMetadata {
            id: "middle_east_geography".to_string(),
            name: "Middle East Geography".to_string(),
            agent_type: "middle_east_geography".to_string(),
            category: "geopolitics".to_string(),
            description: "Tim Marshall: Desert geography, resource curse, artificial borders".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["geographic_analysis".to_string()],
            icon: "🏜️".to_string(),
            color: "#f59e0b".to_string(),
        }),

        // === GEOPOLITICS AGENTS - World Order ===
        "american_world" => Some(AgentMetadata {
            id: "american_world".to_string(),
            name: "American World Order".to_string(),
            agent_type: "american_world".to_string(),
            category: "geopolitics".to_string(),
            description: "Kissinger: Liberal internationalism, democratic values, exceptionalism".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["world_order_analysis".to_string()],
            icon: "🦅".to_string(),
            color: "#2563eb".to_string(),
        }),

        "chinese_world" => Some(AgentMetadata {
            id: "chinese_world".to_string(),
            name: "Chinese World Order".to_string(),
            agent_type: "chinese_world".to_string(),
            category: "geopolitics".to_string(),
            description: "Kissinger: Middle Kingdom centrality, hierarchical system, tributary model".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["world_order_analysis".to_string()],
            icon: "🐉".to_string(),
            color: "#dc2626".to_string(),
        }),

        "balance_power" => Some(AgentMetadata {
            id: "balance_power".to_string(),
            name: "Balance of Power".to_string(),
            agent_type: "balance_power".to_string(),
            category: "geopolitics".to_string(),
            description: "Kissinger: Equilibrium maintenance, power transitions, realpolitik".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["world_order_analysis".to_string()],
            icon: "⚖️".to_string(),
            color: "#6366f1".to_string(),
        }),

        "islamic_world" => Some(AgentMetadata {
            id: "islamic_world".to_string(),
            name: "Islamic World Order".to_string(),
            agent_type: "islamic_world".to_string(),
            category: "geopolitics".to_string(),
            description: "Kissinger: Ummah unity, Sharia governance, caliphate legacy".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["world_order_analysis".to_string()],
            icon: "☪️".to_string(),
            color: "#10b981".to_string(),
        }),

        "nuclear_order" => Some(AgentMetadata {
            id: "nuclear_order".to_string(),
            name: "Nuclear Order".to_string(),
            agent_type: "nuclear_order".to_string(),
            category: "geopolitics".to_string(),
            description: "Kissinger: Deterrence theory, arms control, nuclear diplomacy".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["world_order_analysis".to_string()],
            icon: "☢️".to_string(),
            color: "#f59e0b".to_string(),
        }),

        // === GEOPOLITICS AGENTS - Grand Chessboard ===
        "eurasian_balkans" => Some(AgentMetadata {
            id: "eurasian_balkans".to_string(),
            name: "Eurasian Balkans".to_string(),
            agent_type: "eurasian_balkans".to_string(),
            category: "geopolitics".to_string(),
            description: "Brzezinski: Pivotal region, strategic instability, power competition".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["chessboard_analysis".to_string()],
            icon: "♟️".to_string(),
            color: "#a855f7".to_string(),
        }),

        "democratic_bridgehead" => Some(AgentMetadata {
            id: "democratic_bridgehead".to_string(),
            name: "Democratic Bridgehead".to_string(),
            agent_type: "democratic_bridgehead".to_string(),
            category: "geopolitics".to_string(),
            description: "Brzezinski: Western influence projection, democratic anchor".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["chessboard_analysis".to_string()],
            icon: "🌉".to_string(),
            color: "#06b6d4".to_string(),
        }),

        "far_eastern_anchor" => Some(AgentMetadata {
            id: "far_eastern_anchor".to_string(),
            name: "Far Eastern Anchor".to_string(),
            agent_type: "far_eastern_anchor".to_string(),
            category: "geopolitics".to_string(),
            description: "Brzezinski: US-Japan alliance, Pacific stability pillar".to_string(),
            script_path: script_path.to_string(),
            parameters: get_geopolitics_agent_parameters(),
            required_inputs: vec![],
            outputs: vec!["chessboard_analysis".to_string()],
            icon: "⚓".to_string(),
            color: "#ec4899".to_string(),
        }),

        _ => None,
    }
}

#[tauri::command]
pub async fn list_available_agents() -> Result<Vec<AgentMetadata>, String> {
    let mut agents = Vec::new();

    // Trader agents
    let trader_types = vec![
        "warren_buffett", "charlie_munger", "benjamin_graham",
        "seth_klarman", "howard_marks", "joel_greenblatt",
        "david_einhorn", "bill_miller", "jean_marie_eveillard", "marty_whitman"
    ];

    // Hedge fund agents
    let hedge_fund_types = vec![
        "macro_cycle", "central_bank", "behavioral", "institutional_flow",
        "innovation", "geopolitical", "currency", "supply_chain",
        "sentiment", "regulatory", "bridgewater", "citadel",
        "renaissance", "two_sigma", "de_shaw", "elliott",
        "pershing_square", "arq_capital"
    ];

    // Economic agents
    let economic_types = vec![
        "capitalism", "keynesian", "neoliberal", "socialism",
        "mixed_economy", "mercantilist"
    ];

    // Geopolitics agents
    let geopolitics_types = vec![
        "russia_geography", "china_geography", "usa_geography", "europe_geography", "middle_east_geography",
        "american_world", "chinese_world", "balance_power", "islamic_world", "nuclear_order",
        "eurasian_balkans", "democratic_bridgehead", "far_eastern_anchor"
    ];

    for agent_type in trader_types.iter()
        .chain(hedge_fund_types.iter())
        .chain(economic_types.iter())
        .chain(geopolitics_types.iter()) {
        if let Some(metadata) = get_agent_metadata_internal(agent_type) {
            agents.push(metadata);
        }
    }

    Ok(agents)
}

#[tauri::command]
pub async fn execute_python_agent(
    app: tauri::AppHandle,
    agent_type: String,
    parameters: serde_json::Value,
    inputs: serde_json::Value,
) -> Result<AgentExecutionResult, String> {
    let start_time = std::time::Instant::now();

    // Get script path
    let script_path = get_agent_script_path(&agent_type)
        .ok_or_else(|| format!("Unknown agent type: {}", agent_type))?;

    // Get Python executable
    let python_path = get_python_path(&app)?;

    // Get full script path - handle dev vs prod differently
    let full_script_path = if cfg!(debug_assertions) {
        // Development mode: use resources directly from project root
        let current_dir = std::env::current_dir()
            .map_err(|e| format!("Failed to get current directory: {}", e))?;

        // Check if we're already in src-tauri, if so go up one level
        let base_dir = if current_dir.ends_with("src-tauri") {
            current_dir
        } else {
            current_dir.join("src-tauri")
        };

        base_dir
            .join("resources")
            .join("scripts")
            .join(script_path)
    } else {
        // Production mode: use Tauri's resource directory
        let resource_dir = app.path().resource_dir()
            .map_err(|e| format!("Failed to get resource directory: {}", e))?;

        resource_dir
            .join("resources")
            .join("scripts")
            .join(script_path)
    };

    // Verify script exists
    if !full_script_path.exists() {
        return Err(format!("Script not found: {}", full_script_path.display()));
    }

    // Build command
    let mut cmd = std::process::Command::new(&python_path);
    cmd.arg(&full_script_path)
       .arg("--parameters")
       .arg(serde_json::to_string(&parameters).unwrap_or_default())
       .arg("--inputs")
       .arg(serde_json::to_string(&inputs).unwrap_or_default());

    // Hide console on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    // Execute
    let output = cmd.output()
        .map_err(|e| format!("Failed to execute Python: {}", e))?;

    let execution_time = start_time.elapsed().as_millis() as u64;

    if output.status.success() {
        let stdout = String::from_utf8_lossy(&output.stdout);

        // Parse JSON output from Python
        match serde_json::from_str::<serde_json::Value>(&stdout) {
            Ok(data) => Ok(AgentExecutionResult {
                success: true,
                data: Some(data),
                error: None,
                execution_time_ms: execution_time,
                agent_type,
            }),
            Err(e) => Ok(AgentExecutionResult {
                success: true,
                data: Some(serde_json::json!({ "raw_output": stdout.to_string() })),
                error: Some(format!("JSON parse warning: {}", e)),
                execution_time_ms: execution_time,
                agent_type,
            }),
        }
    } else {
        let stderr = String::from_utf8_lossy(&output.stderr);
        Ok(AgentExecutionResult {
            success: false,
            data: None,
            error: Some(stderr.to_string()),
            execution_time_ms: execution_time,
            agent_type,
        })
    }
}

#[tauri::command]
pub async fn get_agent_metadata(agent_type: String) -> Result<AgentMetadata, String> {
    get_agent_metadata_internal(&agent_type)
        .ok_or_else(|| format!("Agent not found: {}", agent_type))
}
