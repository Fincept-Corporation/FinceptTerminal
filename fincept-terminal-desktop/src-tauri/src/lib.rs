// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

use std::collections::HashMap;
use std::process::{Child, Command, Stdio, ChildStdin};
use std::sync::{Arc, Mutex};
use std::io::{BufRead, BufReader, Write};
use std::thread;
use std::time::Duration;
use std::sync::mpsc::{channel, Sender, Receiver};
use serde::Serialize;
use sha2::{Sha256, Digest};

// Data sources and commands modules
mod data_sources;
mod commands;
mod utils;
mod setup;
// mod finscript; // TODO: Implement FinScript module

// MCP Server Process with communication channels
struct MCPProcess {
    child: Child,
    stdin: Arc<Mutex<ChildStdin>>,
    response_rx: Receiver<String>,
}

// Global state to manage MCP server processes
struct MCPState {
    processes: Mutex<HashMap<String, MCPProcess>>,
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

// Cleanup workflow on app close (called from frontend)
#[tauri::command]
async fn cleanup_running_workflows() -> Result<(), String> {
    // This is just a marker command - the actual cleanup happens in the frontend
    // via workflowService.cleanupRunningWorkflows()
    Ok(())
}

// Spawn an MCP server process with background stdout reader
#[tauri::command]
fn spawn_mcp_server(
    app: tauri::AppHandle,
    state: tauri::State<MCPState>,
    server_id: String,
    command: String,
    args: Vec<String>,
    env: HashMap<String, String>,
) -> Result<SpawnResult, String> {
    // Determine if we should use bundled Bun (for npx/bunx commands)
    let (fixed_command, fixed_args) = if command == "npx" || command == "bunx" {
        // Try to get bundled Bun path
        match utils::python::get_bundled_bun_path(&app) {
            Ok(bun_path) => {
                // Use 'bun x' which is equivalent to 'bunx' or 'npx'
                let mut new_args = vec!["x".to_string()];
                new_args.extend(args.clone());
                (bun_path.to_string_lossy().to_string(), new_args)
            }
            Err(_) => {
                // Fall back to system npx
                #[cfg(target_os = "windows")]
                let cmd = "npx.cmd".to_string();
                #[cfg(not(target_os = "windows"))]
                let cmd = "npx".to_string();
                (cmd, args.clone())
            }
        }
    } else {
        // Fix command for Windows - node/python need .exe extension
        #[cfg(target_os = "windows")]
        let cmd = if command == "node" {
            "node.exe".to_string()
        } else if command == "python" {
            "python.exe".to_string()
        } else {
            command.clone()
        };

        #[cfg(not(target_os = "windows"))]
        let cmd = command.clone();

        (cmd, args.clone())
    };

    // Build command
    let mut cmd = Command::new(&fixed_command);
    cmd.args(&fixed_args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    // Add environment variables
    for (key, value) in env {
        cmd.env(key, value);
    }

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    // Spawn process
    match cmd.spawn() {
        Ok(mut child) => {
            let pid = child.id();

            // Extract stdin and stdout
            let stdin = child.stdin.take().ok_or("Failed to get stdin")?;
            let stdout = child.stdout.take().ok_or("Failed to get stdout")?;
            let stderr = child.stderr.take();

            // Create channel for responses
            let (response_tx, response_rx): (Sender<String>, Receiver<String>) = channel();

            // Spawn background thread to read stdout
            let server_id_clone = server_id.clone();
            thread::spawn(move || {
                let reader = BufReader::new(stdout);

                for line in reader.lines() {
                    match line {
                        Ok(content) => {
                            if !content.trim().is_empty() {
                                if response_tx.send(content).is_err() {
                                    break;
                                }
                            }
                        }
                        Err(_) => {
                            break;
                        }
                    }
                }
            });

            // Spawn background thread to read stderr (for debugging)
            if let Some(stderr) = stderr {
                let _server_id_clone = server_id.clone();
                thread::spawn(move || {
                    let reader = BufReader::new(stderr);
                    for line in reader.lines() {
                        if let Ok(content) = line {
                            if !content.trim().is_empty() {
                                eprintln!("[MCP] {}", content);
                            }
                        }
                    }
                });
            }

            // Store process with communication channels
            let mcp_process = MCPProcess {
                child,
                stdin: Arc::new(Mutex::new(stdin)),
                response_rx,
            };

            let mut processes = state.processes.lock().unwrap();
            processes.insert(server_id.clone(), mcp_process);

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

// Send JSON-RPC request to MCP server with timeout
#[tauri::command]
fn send_mcp_request(
    state: tauri::State<MCPState>,
    server_id: String,
    request: String,
) -> Result<String, String> {
    println!("[Tauri] Sending request to server {}: {}", server_id, request);

    let mut processes = state.processes.lock().unwrap();

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        // Write request to stdin
        {
            let mut stdin = mcp_process.stdin.lock().unwrap();
            writeln!(stdin, "{}", request)
                .map_err(|e| format!("Failed to write to stdin: {}", e))?;
            stdin.flush()
                .map_err(|e| format!("Failed to flush stdin: {}", e))?;
        }

        // Wait for response with timeout (30 seconds for initial package download)
        match mcp_process.response_rx.recv_timeout(Duration::from_secs(30)) {
            Ok(response) => {
                Ok(response)
            }
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                Err("Timeout: No response from server within 30 seconds".to_string())
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                Err("Server process has terminated unexpectedly".to_string())
            }
        }
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

// Send notification (fire and forget)
#[tauri::command]
fn send_mcp_notification(
    state: tauri::State<MCPState>,
    server_id: String,
    notification: String,
) -> Result<(), String> {
    let mut processes = state.processes.lock().unwrap();

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        let mut stdin = mcp_process.stdin.lock().unwrap();
        writeln!(stdin, "{}", notification)
            .map_err(|e| format!("Failed to write notification: {}", e))?;
        stdin.flush()
            .map_err(|e| format!("Failed to flush: {}", e))?;
        Ok(())
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

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        // Check if process is still running
        match mcp_process.child.try_wait() {
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
    let mut processes = state.processes.lock().unwrap();

    if let Some(mut mcp_process) = processes.remove(&server_id) {
        match mcp_process.child.kill() {
            Ok(_) => {
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

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// Windows creation flags to hide console window
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

// Execute Python script with arguments and environment variables
#[tauri::command]
fn execute_python_script(
    app: tauri::AppHandle,
    script_name: String,
    args: Vec<String>,
    env: std::collections::HashMap<String, String>,
) -> Result<String, String> {
    let python_path = utils::python::get_python_path(&app)?;
    let script_path = utils::python::get_script_path(&app, &script_name)?;

    // Verify paths exist
    // Skip existence check for system Python commands (like "python" or "python3")
    // which are found in PATH but not as file paths
    let is_system_command = python_path.to_string_lossy() == "python"
        || python_path.to_string_lossy() == "python3"
        || python_path.to_string_lossy() == "python.exe";

    if !is_system_command && !python_path.exists() {
        return Err(format!("Python executable not found at: {:?}", python_path));
    }
    if !script_path.exists() {
        return Err(format!("Script not found at: {:?}", script_path));
    }

    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path).args(&args);

    // Add environment variables
    for (key, value) in env {
        cmd.env(key, value);
    }

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

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
        .plugin(tauri_plugin_http::init())
        .plugin(tauri_plugin_cors_fetch::init())
        .plugin(tauri_plugin_sql::Builder::default().build())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .plugin(tauri_plugin_process::init())
        .manage(MCPState {
            processes: Mutex::new(HashMap::new()),
        })
        .manage(commands::backtesting::BacktestingState::default())
        .invoke_handler(tauri::generate_handler![
            greet,
            cleanup_running_workflows,
            setup::check_setup_status,
            setup::run_setup,
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
            // WITS (World Integrated Trade Solution) Commands
            commands::wits::execute_wits_command,
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
            commands::oscar::oscar_get_overview_statistics,
            // FRED (Federal Reserve Economic Data) Commands
            commands::fred::execute_fred_command,
            commands::fred::get_fred_series,
            commands::fred::search_fred_series,
            commands::fred::get_fred_series_info,
            commands::fred::get_fred_category,
            commands::fred::get_fred_multiple_series,
            // CoinGecko Commands
            commands::coingecko::execute_coingecko_command,
            commands::coingecko::get_coingecko_price,
            commands::coingecko::get_coingecko_market_data,
            commands::coingecko::get_coingecko_historical,
            commands::coingecko::search_coingecko_coins,
            commands::coingecko::get_coingecko_trending,
            commands::coingecko::get_coingecko_global_data,
            commands::coingecko::get_coingecko_top_coins,
            // World Bank Commands
            commands::worldbank::execute_worldbank_command,
            commands::worldbank::get_worldbank_indicator,
            commands::worldbank::search_worldbank_indicators,
            commands::worldbank::get_worldbank_country_info,
            commands::worldbank::get_worldbank_countries,
            commands::worldbank::get_worldbank_gdp,
            commands::worldbank::get_worldbank_economic_overview,
            commands::worldbank::get_worldbank_development_indicators,
            // Trading Economics Commands
            commands::trading_economics::execute_trading_economics_command,
            commands::trading_economics::get_trading_economics_indicators,
            commands::trading_economics::get_trading_economics_historical,
            commands::trading_economics::get_trading_economics_calendar,
            commands::trading_economics::get_trading_economics_markets,
            commands::trading_economics::get_trading_economics_ratings,
            commands::trading_economics::get_trading_economics_forecasts,
            // EconDB Commands
            commands::econdb::execute_econdb_command,
            commands::econdb::get_econdb_series,
            commands::econdb::search_econdb_indicators,
            commands::econdb::get_econdb_datasets,
            commands::econdb::get_econdb_country_data,
            commands::econdb::get_econdb_multiple_series,
            // Canada Government Commands
            commands::canada_gov::execute_canada_gov_command,
            commands::canada_gov::search_canada_gov_datasets,
            commands::canada_gov::get_canada_gov_dataset,
            commands::canada_gov::get_canada_gov_economic_data,
            // UK Government Commands
            commands::datagovuk::execute_datagovuk_command,
            commands::datagovuk::search_datagovuk_datasets,
            commands::datagovuk::get_datagovuk_dataset,
            // Australian Government Commands
            commands::datagov_au::execute_datagov_au_command,
            commands::datagov_au::search_datagov_au_datasets,
            commands::datagov_au::get_datagov_au_dataset,
            // Japan Statistics Commands
            commands::estat_japan::execute_estat_japan_command,
            commands::estat_japan::get_estat_japan_data,
            commands::estat_japan::search_estat_japan_stats,
            // German Government Commands
            commands::govdata_de::execute_govdata_de_command,
            commands::govdata_de::search_govdata_de_datasets,
            commands::govdata_de::get_govdata_de_dataset,
            // Singapore Government Commands
            commands::datagovsg::execute_datagovsg_command,
            commands::datagovsg::search_datagovsg_datasets,
            commands::datagovsg::get_datagovsg_dataset,
            // Hong Kong Government Commands
            commands::data_gov_hk::execute_data_gov_hk_command,
            commands::data_gov_hk::search_data_gov_hk_datasets,
            commands::data_gov_hk::get_data_gov_hk_dataset,
            // French Government Commands
            commands::french_gov::execute_french_gov_command,
            commands::french_gov::search_french_gov_datasets,
            commands::french_gov::get_french_gov_dataset,
            // Swiss Government Commands
            commands::swiss_gov::execute_swiss_gov_command,
            commands::swiss_gov::search_swiss_gov_datasets,
            commands::swiss_gov::get_swiss_gov_dataset,
            // Spain Data Commands
            commands::spain_data::execute_spain_data_command,
            commands::spain_data::get_spain_economic_data,
            commands::spain_data::search_spain_datasets,
            // Asian Development Bank Commands
            commands::adb::execute_adb_command,
            commands::adb::get_adb_indicators,
            commands::adb::search_adb_datasets,
            commands::adb::get_adb_development_data,
            // OpenAFRICA Commands
            commands::openafrica::execute_openafrica_command,
            commands::openafrica::search_openafrica_datasets,
            commands::openafrica::get_openafrica_dataset,
            commands::openafrica::get_openafrica_country_datasets,
            // Economic Calendar Commands
            commands::economic_calendar::execute_economic_calendar_command,
            commands::economic_calendar::get_economic_calendar_today,
            commands::economic_calendar::get_economic_calendar_upcoming,
            commands::economic_calendar::get_economic_calendar_by_country,
            commands::economic_calendar::get_economic_calendar_high_impact,
            // Databento Commands
            commands::databento::execute_databento_command,
            commands::databento::get_databento_market_data,
            commands::databento::get_databento_datasets,
            commands::databento::get_databento_historical,
            // Sentinel Hub Commands
            commands::sentinelhub::execute_sentinelhub_command,
            commands::sentinelhub::get_sentinelhub_imagery,
            commands::sentinelhub::get_sentinelhub_collections,
            commands::sentinelhub::search_sentinelhub_data,
            // Jupyter Notebook Commands
            commands::jupyter::execute_python_code,
            commands::jupyter::open_notebook,
            commands::jupyter::save_notebook,
            commands::jupyter::create_new_notebook,
            commands::jupyter::get_python_version,
            commands::jupyter::install_python_package,
            commands::jupyter::list_python_packages,
            // commands::finscript_cmd::execute_finscript, // TODO: Implement FinScript
            // Python Agent Commands
            commands::agents::list_available_agents,
            commands::agents::execute_python_agent,
            commands::agents::get_agent_metadata,
            // Portfolio Analytics Commands
            commands::portfolio::calculate_portfolio_metrics,
            commands::portfolio::optimize_portfolio,
            commands::portfolio::generate_efficient_frontier,
            commands::portfolio::get_portfolio_overview,
            commands::portfolio::calculate_risk_metrics,
            commands::portfolio::generate_asset_allocation,
            commands::portfolio::calculate_retirement_plan,
            commands::portfolio::analyze_behavioral_biases,
            commands::portfolio::analyze_etf_costs,
            // Technical Analysis Commands
            commands::technical_analysis::execute_technical_analysis_command,
            commands::technical_analysis::get_technical_analysis_help,
            commands::technical_analysis::calculate_indicators_yfinance,
            commands::technical_analysis::calculate_indicators_csv,
            commands::technical_analysis::calculate_indicators_json,
            commands::technical_analysis::compute_all_technicals,
            // FiscalData Commands
            commands::fiscaldata::execute_fiscaldata_command,
            commands::fiscaldata::get_fiscaldata_debt_to_penny,
            commands::fiscaldata::get_fiscaldata_avg_interest_rates,
            commands::fiscaldata::get_fiscaldata_interest_expense,
            commands::fiscaldata::get_fiscaldata_datasets,
            // NASA GIBS Commands
            commands::nasa_gibs::execute_nasa_gibs_command,
            commands::nasa_gibs::get_nasa_gibs_layers,
            commands::nasa_gibs::get_nasa_gibs_layer_details,
            commands::nasa_gibs::search_nasa_gibs_layers,
            commands::nasa_gibs::get_nasa_gibs_time_periods,
            commands::nasa_gibs::get_nasa_gibs_tile,
            commands::nasa_gibs::get_nasa_gibs_tile_batch,
            commands::nasa_gibs::get_nasa_gibs_popular_layers,
            commands::nasa_gibs::get_nasa_gibs_supported_projections,
            commands::nasa_gibs::test_nasa_gibs_all_endpoints,
            // Universal CKAN Commands
            commands::ckan::execute_ckan_command,
            commands::ckan::get_ckan_supported_countries,
            commands::ckan::test_ckan_portal_connection,
            commands::ckan::list_ckan_organizations,
            commands::ckan::get_ckan_organization_details,
            commands::ckan::list_ckan_datasets,
            commands::ckan::search_ckan_datasets,
            commands::ckan::get_ckan_dataset_details,
            commands::ckan::get_ckan_datasets_by_organization,
            commands::ckan::get_ckan_dataset_resources,
            commands::ckan::get_ckan_resource_details,
            // Analytics Commands
            commands::analytics::execute_technical_indicators,
            commands::analytics::execute_pyportfolioopt,
            commands::analytics::execute_riskfolio,
            commands::analytics::execute_skfolio,
            commands::analytics::analyze_digital_assets,
            commands::analytics::analyze_hedge_funds,
            commands::analytics::analyze_real_estate,
            commands::analytics::analyze_private_capital,
            commands::analytics::analyze_natural_resources,
            commands::analytics::price_options,
            commands::analytics::analyze_arbitrage,
            commands::analytics::analyze_forward_commitments,
            commands::analytics::analyze_currency,
            commands::analytics::analyze_growth,
            commands::analytics::analyze_policy,
            commands::analytics::analyze_market_cycles,
            commands::analytics::analyze_trade_geopolitics,
            commands::analytics::analyze_capital_flows,
            commands::analytics::calculate_dcf_valuation,
            commands::analytics::calculate_dividend_valuation,
            commands::analytics::calculate_multiples_valuation,
            commands::analytics::analyze_fundamental,
            commands::analytics::analyze_industry,
            commands::analytics::forecast_financials,
            commands::analytics::analyze_market_efficiency,
            commands::analytics::analyze_index,
            commands::analytics::analyze_portfolio_risk,
            commands::analytics::analyze_portfolio_performance,
            commands::analytics::optimize_portfolio_management,
            commands::analytics::plan_portfolio,
            commands::analytics::analyze_active_management,
            commands::analytics::analyze_behavioral_finance,
            commands::analytics::analyze_etf,
            commands::analytics::calculate_quant_metrics,
            commands::analytics::calculate_rates,
            // AI Agent Commands
            commands::ai_agents::execute_economic_agent,
            commands::ai_agents::run_capitalism_agent,
            commands::ai_agents::run_keynesian_agent,
            commands::ai_agents::run_neoliberal_agent,
            commands::ai_agents::run_socialism_agent,
            commands::ai_agents::run_mercantilist_agent,
            commands::ai_agents::run_mixed_economy_agent,
            commands::ai_agents::execute_geopolitics_agent,
            commands::ai_agents::run_eurasian_chessboard_agent,
            commands::ai_agents::run_geostrategic_players_agent,
            commands::ai_agents::run_eurasian_balkans_agent,
            commands::ai_agents::run_geography_agent,
            commands::ai_agents::run_world_order_agent,
            commands::ai_agents::run_westphalian_europe_agent,
            commands::ai_agents::run_balance_power_agent,
            commands::ai_agents::execute_hedge_fund_agent,
            commands::ai_agents::run_bridgewater_agent,
            commands::ai_agents::run_citadel_agent,
            commands::ai_agents::run_renaissance_agent,
            commands::ai_agents::run_de_shaw_agent,
            commands::ai_agents::run_two_sigma_agent,
            commands::ai_agents::run_elliott_management_agent,
            commands::ai_agents::run_fincept_hedge_fund_orchestrator,
            commands::ai_agents::run_warren_buffett_agent,
            commands::ai_agents::run_benjamin_graham_agent,
            commands::ai_agents::run_charlie_munger_agent,
            commands::ai_agents::run_seth_klarman_agent,
            commands::ai_agents::run_howard_marks_agent,
            commands::ai_agents::run_joel_greenblatt_agent,
            commands::ai_agents::run_david_einhorn_agent,
            commands::ai_agents::run_bill_miller_agent,
            // Report Generator Commands
            commands::report_generator::generate_report_html,
            commands::report_generator::generate_report_pdf,
            commands::report_generator::create_default_report_template,
            commands::report_generator::open_folder,
            commands::backtesting::execute_lean_cli,
            commands::backtesting::execute_command,
            commands::backtesting::kill_lean_process,
            commands::backtesting::execute_python_backtest,
            commands::backtesting::check_file_exists,
            commands::backtesting::create_directory,
            commands::backtesting::write_file,
            commands::backtesting::read_file,
            // Company News Commands
            commands::company_news::fetch_company_news,
            commands::company_news::fetch_news_by_topic,
            commands::company_news::get_full_article,
            commands::company_news::get_company_news_help
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
