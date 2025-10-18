// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

use std::collections::HashMap;
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;
use std::io::{BufRead, BufReader, Write};
use serde::Serialize;
use sha2::{Sha256, Digest};

// Data sources and commands modules
mod data_sources;
mod commands;

// Global state to manage MCP server processes
struct MCPState {
    processes: Mutex<HashMap<String, Child>>,
}

#[derive(Debug, Serialize)]
struct SpawnResult {
    pid: u32,
    success: bool,
    error: Option<String>,
}

#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

// Spawn an MCP server process
#[tauri::command]
fn spawn_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
    command: String,
    args: Vec<String>,
    env: HashMap<String, String>,
) -> Result<SpawnResult, String> {
    println!("[Tauri] Spawning MCP server: {} with command: {} {:?}", server_id, command, args);

    // Build command
    let mut cmd = Command::new(&command);
    cmd.args(&args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    // Add environment variables
    for (key, value) in env {
        cmd.env(key, value);
    }

    // Spawn process
    match cmd.spawn() {
        Ok(child) => {
            let pid = child.id();
            println!("[Tauri] MCP server spawned successfully with PID: {}", pid);

            // Store process
            let mut processes = state.processes.lock().unwrap();
            processes.insert(server_id.clone(), child);

            Ok(SpawnResult {
                pid,
                success: true,
                error: None,
            })
        }
        Err(e) => {
            eprintln!("[Tauri] Failed to spawn MCP server: {}", e);
            Ok(SpawnResult {
                pid: 0,
                success: false,
                error: Some(format!("Failed to spawn process: {}", e)),
            })
        }
    }
}

// Send JSON-RPC request to MCP server
#[tauri::command]
fn send_mcp_request(
    state: tauri::State<MCPState>,
    server_id: String,
    request: String,
) -> Result<String, String> {
    println!("[Tauri] Sending request to server {}: {}", server_id, request);

    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        // Get stdin handle
        if let Some(ref mut stdin) = child.stdin {
            // Write request
            if let Err(e) = writeln!(stdin, "{}", request) {
                return Err(format!("Failed to write to stdin: {}", e));
            }

            if let Err(e) = stdin.flush() {
                return Err(format!("Failed to flush stdin: {}", e));
            }

            // Read response from stdout
            if let Some(ref mut stdout) = child.stdout {
                let reader = BufReader::new(stdout);
                if let Some(Ok(line)) = reader.lines().next() {
                    println!("[Tauri] Received response: {}", line);
                    return Ok(line);
                } else {
                    return Err("No response from server".to_string());
                }
            } else {
                return Err("No stdout available".to_string());
            }
        } else {
            return Err("No stdin available".to_string());
        }
    } else {
        return Err(format!("Server {} not found", server_id));
    }
}

// Send notification (fire and forget)
#[tauri::command]
fn send_mcp_notification(
    state: tauri::State<MCPState>,
    server_id: String,
    notification: String,
) -> Result<(), String> {
    println!("[Tauri] Sending notification to server {}: {}", server_id, notification);

    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        if let Some(ref mut stdin) = child.stdin {
            writeln!(stdin, "{}", notification)
                .map_err(|e| format!("Failed to write notification: {}", e))?;
            stdin.flush()
                .map_err(|e| format!("Failed to flush: {}", e))?;
            Ok(())
        } else {
            Err("No stdin available".to_string())
        }
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

// Ping MCP server to check if alive
#[tauri::command]
fn ping_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<bool, String> {
    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        // Check if process is still running
        match child.try_wait() {
            Ok(Some(_)) => Ok(false), // Process has exited
            Ok(None) => Ok(true),      // Process is still running
            Err(_) => Ok(false),       // Error checking status
        }
    } else {
        Ok(false) // Server not found
    }
}

// Kill MCP server
#[tauri::command]
fn kill_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<(), String> {
    println!("[Tauri] Killing MCP server: {}", server_id);

    let mut processes = state.processes.lock().unwrap();

    if let Some(mut child) = processes.remove(&server_id) {
        match child.kill() {
            Ok(_) => {
                println!("[Tauri] Server {} killed successfully", server_id);
                Ok(())
            }
            Err(e) => Err(format!("Failed to kill server: {}", e)),
        }
    } else {
        Ok(()) // Server not found, consider it killed
    }
}

// SHA256 hash for Fyers authentication
#[tauri::command]
fn sha256_hash(input: String) -> String {
    let mut hasher = Sha256::new();
    hasher.update(input.as_bytes());
    let result = hasher.finalize();
    format!("{:x}", result)
}

// Execute Python script with arguments and environment variables
#[tauri::command]
fn execute_python_script(
    script_name: String,
    args: Vec<String>,
    env: Option<HashMap<String, String>>,
) -> Result<String, String> {
    println!("[Tauri] Executing Python script: {} with args: {:?}", script_name, args);

    let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("resources")
        .join("python")
        .join("python.exe");

    let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("resources")
        .join("scripts")
        .join(&script_name);

    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path).args(&args);

    // Add environment variables if provided
    if let Some(env_vars) = env {
        for (key, value) in env_vars {
            cmd.env(key, value);
        }
    }

    match cmd.output() {
        Ok(output) => {
            if output.status.success() {
                String::from_utf8(output.stdout)
                    .map_err(|e| format!("Failed to parse output: {}", e))
            } else {
                let error = String::from_utf8_lossy(&output.stderr);
                Err(format!("Python script failed: {}", error))
            }
        }
        Err(e) => Err(format!("Failed to execute Python script: {}", e)),
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_cors_fetch::init())
        .plugin(tauri_plugin_sql::Builder::default().build())
        .manage(MCPState {
            processes: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![
            greet,
            spawn_mcp_server,
            send_mcp_request,
            send_mcp_notification,
            ping_mcp_server,
            kill_mcp_server,
            sha256_hash,
            execute_python_script,
            commands::market_data::get_market_quote,
            commands::market_data::get_market_quotes,
            commands::market_data::get_period_returns,
            commands::market_data::check_market_data_health,
            commands::market_data::get_historical_data,
            commands::market_data::get_stock_info,
            commands::market_data::get_financials,
            commands::polygon::execute_polygon_command,
            commands::yfinance::execute_yfinance_command,
            commands::alphavantage::execute_alphavantage_command,
            commands::alphavantage::get_alphavantage_quote,
            commands::alphavantage::get_alphavantage_daily,
            commands::alphavantage::get_alphavantage_intraday,
            commands::alphavantage::get_alphavantage_overview,
            commands::alphavantage::search_alphavantage_symbols,
            commands::alphavantage::get_alphavantage_comprehensive,
            commands::alphavantage::get_alphavantage_market_movers,
            commands::government_us::execute_government_us_command,
            commands::government_us::get_treasury_prices,
            commands::government_us::get_treasury_auctions,
            commands::government_us::get_comprehensive_treasury_data,
            commands::government_us::get_treasury_summary,
            commands::congress_gov::execute_congress_gov_command,
            commands::congress_gov::get_congress_bills,
            commands::congress_gov::get_bill_info,
            commands::congress_gov::get_bill_text,
            commands::congress_gov::download_bill_text,
            commands::congress_gov::get_comprehensive_bill_data,
            commands::congress_gov::get_bill_summary_by_congress,
            commands::oecd::execute_oecd_command,
            commands::oecd::get_oecd_gdp_real,
            commands::oecd::get_oecd_consumer_price_index,
            commands::oecd::get_oecd_gdp_forecast,
            commands::oecd::get_oecd_unemployment,
            commands::oecd::get_oecd_economic_summary,
            commands::oecd::get_oecd_country_list,
            commands::imf::execute_imf_command,
            commands::imf::get_imf_economic_indicators,
            commands::imf::get_imf_direction_of_trade,
            commands::imf::get_imf_available_indicators,
            commands::imf::get_imf_comprehensive_economic_data,
            commands::imf::get_imf_reserves_data,
            commands::imf::get_imf_trade_summary,
            commands::fmp::execute_fmp_command,
            commands::fmp::get_fmp_equity_quote,
            commands::fmp::get_fmp_company_profile,
            commands::fmp::get_fmp_historical_prices,
            commands::fmp::get_fmp_income_statement,
            commands::fmp::get_fmp_balance_sheet,
            commands::fmp::get_fmp_cash_flow_statement,
            commands::fmp::get_fmp_financial_ratios,
            commands::fmp::get_fmp_key_metrics,
            commands::fmp::get_fmp_market_snapshots,
            commands::fmp::get_fmp_treasury_rates,
            commands::fmp::get_fmp_etf_info,
            commands::fmp::get_fmp_etf_holdings,
            commands::fmp::get_fmp_crypto_list,
            commands::fmp::get_fmp_crypto_historical,
            commands::fmp::get_fmp_company_news,
            commands::fmp::get_fmp_general_news,
            commands::fmp::get_fmp_economic_calendar,
            commands::fmp::get_fmp_insider_trading,
            commands::fmp::get_fmp_institutional_ownership,
            commands::fmp::get_fmp_company_overview,
            commands::fmp::get_fmp_financial_statements,
            commands::fmp::get_fmp_market_quotes,
            commands::fmp::get_fmp_price_performance,
            commands::fmp::get_fmp_stock_screener,
            commands::fmp::get_fmp_price_targets,
            commands::federal_reserve::execute_federal_reserve_command,
            commands::federal_reserve::get_federal_funds_rate,
            commands::federal_reserve::get_sofr_rate,
            commands::federal_reserve::get_treasury_rates,
            commands::federal_reserve::get_yield_curve,
            commands::federal_reserve::get_money_measures,
            commands::federal_reserve::get_central_bank_holdings,
            commands::federal_reserve::get_overnight_bank_funding_rate,
            commands::federal_reserve::get_comprehensive_monetary_data,
            commands::federal_reserve::get_fed_market_overview,
            commands::sec::execute_sec_command,
            commands::sec::get_sec_company_filings,
            commands::sec::get_sec_cik_map,
            commands::sec::get_sec_symbol_map,
            commands::sec::get_sec_filing_content,
            commands::sec::parse_sec_filing_html,
            commands::sec::get_sec_insider_trading,
            commands::sec::get_sec_institutional_ownership,
            commands::sec::search_sec_companies,
            commands::sec::search_sec_etfs_mutual_funds,
            commands::sec::get_sec_available_form_types,
            commands::sec::get_sec_company_facts,
            commands::sec::get_sec_financial_statements,
            commands::sec::get_sec_company_overview,
            commands::sec::get_sec_filings_by_form_type,
            commands::sec::search_sec_equities,
            commands::sec::search_sec_mutual_funds_etfs,
            commands::sec::get_sec_filing_metadata,
            commands::sec::get_sec_recent_current_reports,
            commands::sec::get_sec_annual_reports,
            commands::sec::get_sec_quarterly_reports,
            commands::sec::get_sec_registration_statements,
            commands::sec::get_sec_beneficial_ownership,
            commands::cftc::execute_cftc_command,
            commands::cftc::get_cftc_cot_data,
            commands::cftc::search_cftc_cot_markets,
            commands::cftc::get_cftc_available_report_types,
            commands::cftc::analyze_cftc_market_sentiment,
            commands::cftc::get_cftc_position_summary,
            commands::cftc::get_cftc_comprehensive_cot_overview,
            commands::cftc::get_cftc_cot_historical_trend,
            commands::cftc::get_cftc_legacy_cot,
            commands::cftc::get_cftc_disaggregated_cot,
            commands::cftc::get_cftc_tff_reports,
            commands::cftc::get_cftc_supplemental_cot,
            commands::cftc::get_cftc_precious_metals_cot,
            commands::cftc::get_cftc_energy_cot,
            commands::cftc::get_cftc_agricultural_cot,
            commands::cftc::get_cftc_financial_cot,
            commands::cftc::get_cftc_crypto_cot,
            commands::cftc::get_cftc_position_changes,
            commands::cftc::get_cftc_extreme_positions,
            commands::cboe::execute_cboe_command,
            commands::cboe::get_cboe_equity_quote,
            commands::cboe::get_cboe_equity_historical,
            commands::cboe::search_cboe_equities,
            commands::cboe::get_cboe_index_constituents,
            commands::cboe::get_cboe_index_historical,
            commands::cboe::search_cboe_indices,
            commands::cboe::get_cboe_index_snapshots,
            commands::cboe::get_cboe_available_indices,
            commands::cboe::get_cboe_futures_curve,
            commands::cboe::get_cboe_options_chains,
            commands::cboe::get_cboe_us_market_overview,
            commands::cboe::get_cboe_european_market_overview,
            commands::cboe::get_cboe_vix_analysis,
            commands::cboe::get_cboe_equity_analysis,
            commands::cboe::get_cboe_index_analysis,
            commands::cboe::get_cboe_market_sentiment,
            commands::cboe::search_cboe_symbols,
            commands::cboe::get_cboe_directory_info,
            commands::bls::execute_bls_command,
            commands::bls::search_bls_series,
            commands::bls::get_bls_series_data,
            commands::bls::get_bls_popular_series,
            commands::bls::get_bls_labor_market_overview,
            commands::bls::get_bls_inflation_overview,
            commands::bls::get_bls_employment_cost_index,
            commands::bls::get_bls_productivity_costs,
            commands::bls::get_bls_survey_categories,
            commands::bls::get_bls_cpi_data,
            commands::bls::get_bls_unemployment_data,
            commands::bls::get_bls_payrolls_data,
            commands::bls::get_bls_participation_data,
            commands::bls::get_bls_ppi_data,
            commands::bls::get_bls_jolts_data,
            commands::bls::get_bls_economic_dashboard,
            commands::bls::get_bls_inflation_analysis,
            commands::bls::get_bls_labor_analysis,
            commands::bls::get_bls_series_comparison,
            commands::bls::get_bls_data_directory,
            commands::nasdaq::execute_nasdaq_command,
            commands::nasdaq::search_nasdaq_equities,
            commands::nasdaq::get_nasdaq_equity_screener,
            commands::nasdaq::get_nasdaq_top_performers,
            commands::nasdaq::get_nasdaq_stocks_by_market_cap,
            commands::nasdaq::get_nasdaq_stocks_by_sector,
            commands::nasdaq::get_nasdaq_stocks_by_exchange,
            commands::nasdaq::search_nasdaq_etfs,
            commands::nasdaq::get_nasdaq_dividend_calendar,
            commands::nasdaq::get_nasdaq_upcoming_dividends,
            commands::nasdaq::get_nasdaq_earnings_calendar,
            commands::nasdaq::get_nasdaq_upcoming_earnings,
            commands::nasdaq::get_nasdaq_ipo_calendar,
            commands::nasdaq::get_nasdaq_upcoming_ipos,
            commands::nasdaq::get_nasdaq_recent_ipos,
            commands::nasdaq::get_nasdaq_filed_ipos,
            commands::nasdaq::get_nasdaq_withdrawn_ipos,
            commands::nasdaq::get_nasdaq_spo_calendar,
            commands::nasdaq::get_nasdaq_economic_calendar,
            commands::nasdaq::get_nasdaq_today_economic_events,
            commands::nasdaq::get_nasdaq_week_economic_events,
            commands::nasdaq::get_nasdaq_top_retail_activity,
            commands::nasdaq::get_nasdaq_retail_sentiment,
            commands::nasdaq::get_nasdaq_market_overview,
            commands::nasdaq::get_nasdaq_dividend_overview,
            commands::nasdaq::get_nasdaq_ipo_overview,
            commands::nasdaq::get_nasdaq_earnings_overview,
            commands::nasdaq::get_nasdaq_sector_analysis,
            commands::nasdaq::get_nasdaq_market_cap_analysis,
            commands::nasdaq::get_nasdaq_exchange_comparison,
            commands::nasdaq::get_nasdaq_date_range_analysis,
            commands::nasdaq::get_nasdaq_directory_info,
            commands::bis::execute_bis_command,
            commands::bis::get_bis_data,
            commands::bis::get_bis_available_constraints,
            commands::bis::get_bis_data_structures,
            commands::bis::get_bis_dataflows,
            commands::bis::get_bis_categorisations,
            commands::bis::get_bis_content_constraints,
            commands::bis::get_bis_actual_constraints,
            commands::bis::get_bis_allowed_constraints,
            commands::bis::get_bis_structures,
            commands::bis::get_bis_concept_schemes,
            commands::bis::get_bis_codelists,
            commands::bis::get_bis_category_schemes,
            commands::bis::get_bis_hierarchical_codelists,
            commands::bis::get_bis_agency_schemes,
            commands::bis::get_bis_concepts,
            commands::bis::get_bis_codes,
            commands::bis::get_bis_categories,
            commands::bis::get_bis_hierarchies,
            commands::bis::get_bis_effective_exchange_rates,
            commands::bis::get_bis_central_bank_policy_rates,
            commands::bis::get_bis_long_term_interest_rates,
            commands::bis::get_bis_short_term_interest_rates,
            commands::bis::get_bis_exchange_rates,
            commands::bis::get_bis_credit_to_non_financial_sector,
            commands::bis::get_bis_house_prices,
            commands::bis::get_bis_available_datasets,
            commands::bis::search_bis_datasets,
            commands::bis::get_bis_economic_overview,
            commands::bis::get_bis_dataset_metadata,
            commands::bis::get_bis_monetary_policy_overview,
            commands::bis::get_bis_exchange_rate_analysis,
            commands::bis::get_bis_credit_conditions_overview,
            commands::bis::get_bis_housing_market_overview,
            commands::bis::get_bis_data_directory,
            commands::bis::get_bis_comprehensive_summary,
            commands::multpl::execute_multpl_command,
            commands::multpl::get_multpl_series,
            commands::multpl::get_multpl_multiple_series,
            commands::multpl::get_multpl_available_series,
            commands::multpl::get_multpl_shiller_pe,
            commands::multpl::get_multpl_pe_ratio,
            commands::multpl::get_multpl_dividend_yield,
            commands::multpl::get_multpl_earnings_yield,
            commands::multpl::get_multpl_price_to_sales,
            commands::multpl::get_multpl_shiller_pe_monthly,
            commands::multpl::get_multpl_shiller_pe_yearly,
            commands::multpl::get_multpl_pe_yearly,
            commands::multpl::get_multpl_dividend_yearly,
            commands::multpl::get_multpl_dividend_monthly,
            commands::multpl::get_multpl_dividend_growth_yearly,
            commands::multpl::get_multpl_dividend_yield_yearly,
            commands::multpl::get_multpl_earnings_yearly,
            commands::multpl::get_multpl_earnings_monthly,
            commands::multpl::get_multpl_earnings_growth_yearly,
            commands::multpl::get_multpl_earnings_yield_yearly,
            commands::multpl::get_multpl_real_price_yearly,
            commands::multpl::get_multpl_inflation_adjusted_price_yearly,
            commands::multpl::get_multpl_sales_yearly,
            commands::multpl::get_multpl_sales_growth_yearly,
            commands::multpl::get_multpl_real_sales_yearly,
            commands::multpl::get_multpl_price_to_sales_yearly,
            commands::multpl::get_multpl_price_to_book_yearly,
            commands::multpl::get_multpl_book_value_yearly,
            commands::multpl::get_multpl_valuation_overview,
            commands::multpl::get_multpl_dividend_overview,
            commands::multpl::get_multpl_earnings_overview,
            commands::multpl::get_multpl_comprehensive_overview,
            commands::multpl::get_multpl_valuation_analysis,
            commands::multpl::get_multpl_dividend_analysis,
            commands::multpl::get_multpl_earnings_analysis,
            commands::multpl::get_multpl_sales_analysis,
            commands::multpl::get_multpl_price_analysis,
            commands::multpl::get_multpl_custom_analysis,
            commands::multpl::get_multpl_year_over_year_comparison,
            commands::multpl::get_multpl_current_valuation,
            commands::eia::execute_eia_command,
            commands::eia::get_eia_petroleum_status_report,
            commands::eia::get_eia_petroleum_categories,
            commands::eia::get_eia_crude_stocks,
            commands::eia::get_eia_gasoline_stocks,
            commands::eia::get_eia_petroleum_imports,
            commands::eia::get_eia_spot_prices,
            commands::eia::get_eia_short_term_energy_outlook,
            commands::eia::get_eia_steo_tables,
            commands::eia::get_eia_energy_markets_summary,
            commands::eia::get_eia_natural_gas_summary,
            commands::eia::get_eia_balance_sheet,
            commands::eia::get_eia_inputs_production,
            commands::eia::get_eia_refiner_production,
            commands::eia::get_eia_distillate_stocks,
            commands::eia::get_eia_imports_by_country,
            commands::eia::get_eia_weekly_estimates,
            commands::eia::get_eia_retail_prices,
            commands::eia::get_eia_nominal_prices,
            commands::eia::get_eia_petroleum_supply,
            commands::eia::get_eia_electricity_overview,
            commands::eia::get_eia_macroeconomic_indicators,
            commands::eia::get_eia_energy_overview,
            commands::eia::get_eia_petroleum_markets_overview,
            commands::eia::get_eia_natural_gas_markets_overview,
            commands::eia::get_eia_energy_price_trends,
            commands::eia::get_eia_energy_supply_analysis,
            commands::eia::get_eia_energy_consumption_analysis,
            commands::eia::get_eia_custom_analysis,
            commands::eia::get_eia_current_market_snapshot,
            // ECB (European Central Bank) Commands
            commands::ecb::execute_ecb_command,
            commands::ecb::get_ecb_available_categories,
            commands::ecb::get_ecb_currency_reference_rates,
            commands::ecb::get_ecb_overview,
            commands::ecb::get_ecb_yield_curve,
            commands::ecb::get_ecb_yield_curve_aaa_spot,
            commands::ecb::get_ecb_yield_curve_aaa_forward,
            commands::ecb::get_ecb_yield_curve_aaa_par_yield,
            commands::ecb::get_ecb_yield_curve_all_spot,
            commands::ecb::get_ecb_yield_curve_all_forward,
            commands::ecb::get_ecb_yield_curve_all_par_yield,
            commands::ecb::get_ecb_balance_of_payments,
            commands::ecb::get_ecb_bop_main,
            commands::ecb::get_ecb_bop_summary,
            commands::ecb::get_ecb_bop_services,
            commands::ecb::get_ecb_bop_investment_income,
            commands::ecb::get_ecb_bop_direct_investment,
            commands::ecb::get_ecb_bop_portfolio_investment,
            commands::ecb::get_ecb_bop_other_investment,
            commands::ecb::get_ecb_bop_united_states,
            commands::ecb::get_ecb_bop_japan,
            commands::ecb::get_ecb_bop_united_kingdom,
            commands::ecb::get_ecb_bop_switzerland,
            commands::ecb::get_ecb_bop_canada,
            commands::ecb::get_ecb_bop_australia,
            commands::ecb::get_ecb_financial_markets_analysis,
            commands::ecb::get_ecb_yield_curve_analysis,
            commands::ecb::get_ecb_major_economies_bop_analysis,
            commands::ecb::get_ecb_custom_analysis,
            commands::ecb::get_ecb_current_market_snapshot,
            // BEA (Bureau of Economic Analysis) Commands
            commands::bea::bea_get_dataset_list,
            commands::bea::bea_get_parameter_list,
            commands::bea::bea_get_parameter_values,
            commands::bea::bea_get_parameter_values_filtered,
            commands::bea::bea_get_nipa_data,
            commands::bea::bea_get_ni_underlying_detail,
            commands::bea::bea_get_fixed_assets,
            commands::bea::bea_get_mne_data,
            commands::bea::bea_get_gdp_by_industry,
            commands::bea::bea_get_international_transactions,
            commands::bea::bea_get_international_investment_position,
            commands::bea::bea_get_input_output,
            commands::bea::bea_get_underlying_gdp_by_industry,
            commands::bea::bea_get_international_services_trade,
            commands::bea::bea_get_regional_data,
            commands::bea::bea_get_economic_overview,
            commands::bea::bea_get_regional_snapshot,
            // WTO (World Trade Organization) Commands
            commands::wto::execute_wto_command,
            commands::wto::get_wto_available_apis,
            commands::wto::get_wto_overview,
            commands::wto::get_wto_qr_members,
            commands::wto::get_wto_qr_products,
            commands::wto::get_wto_qr_notifications,
            commands::wto::get_wto_qr_details,
            commands::wto::get_wto_qr_list,
            commands::wto::get_wto_qr_hs_versions,
            commands::wto::get_wto_eping_members,
            commands::wto::search_wto_eping_notifications,
            commands::wto::get_wto_timeseries_topics,
            commands::wto::get_wto_timeseries_indicators,
            commands::wto::get_wto_timeseries_data,
            commands::wto::get_wto_timeseries_reporters,
            commands::wto::get_wto_tfad_data,
            commands::wto::get_wto_trade_restrictions_analysis,
            commands::wto::get_wto_notifications_analysis,
            commands::wto::get_wto_trade_statistics_analysis,
            commands::wto::get_wto_comprehensive_analysis,
            commands::wto::get_wto_us_trade_analysis,
            commands::wto::get_wto_china_trade_analysis,
            commands::wto::get_wto_germany_trade_analysis,
            commands::wto::get_wto_japan_trade_analysis,
            commands::wto::get_wto_uk_trade_analysis,
            commands::wto::get_wto_recent_tbt_notifications,
            commands::wto::get_wto_recent_sps_notifications,
            commands::wto::get_wto_member_analysis,
            commands::wto::get_wto_global_trade_overview,
            commands::wto::get_wto_sector_trade_analysis,
            commands::wto::get_wto_comparative_trade_analysis,
            // UNESCO UIS (UNESCO Institute for Statistics) Commands
            commands::unesco::unesco_get_indicator_data,
            commands::unesco::unesco_export_indicator_data,
            commands::unesco::unesco_list_geo_units,
            commands::unesco::unesco_export_geo_units,
            commands::unesco::unesco_list_indicators,
            commands::unesco::unesco_export_indicators,
            commands::unesco::unesco_get_default_version,
            commands::unesco::unesco_list_versions,
            commands::unesco::unesco_get_education_overview,
            commands::unesco::unesco_get_global_education_trends,
            commands::unesco::unesco_get_country_comparison,
            commands::unesco::unesco_search_indicators_by_theme,
            commands::unesco::unesco_get_regional_education_data,
            commands::unesco::unesco_get_science_technology_data,
            commands::unesco::unesco_get_culture_data,
            commands::unesco::unesco_export_country_dataset,
            // OSCAR (Observing Systems Capability Analysis and Review Tool) Commands
            commands::oscar::oscar_get_instruments,
            commands::oscar::oscar_get_instrument_by_slug,
            commands::oscar::oscar_search_instruments_by_type,
            commands::oscar::oscar_search_instruments_by_agency,
            commands::oscar::oscar_search_instruments_by_year_range,
            commands::oscar::oscar_get_instrument_classifications,
            commands::oscar::oscar_get_satellites,
            commands::oscar::oscar_get_satellite_by_slug,
            commands::oscar::oscar_search_satellites_by_orbit,
            commands::oscar::oscar_search_satellites_by_agency,
            commands::oscar::oscar_get_variables,
            commands::oscar::oscar_get_variable_by_slug,
            commands::oscar::oscar_search_variables_by_instrument,
            commands::oscar::oscar_get_ecv_variables,
            commands::oscar::oscar_get_space_agencies,
            commands::oscar::oscar_get_instrument_types,
            commands::oscar::oscar_search_weather_instruments,
            commands::oscar::oscar_get_climate_monitoring_instruments,
            commands::oscar::oscar_get_overview_statistics
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
