#![allow(dead_code)]
// Peer Comparison Tauri Commands
// CFA-compliant peer analysis and benchmarking commands

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
// Note: GICS service types available if needed for future peer discovery
// use crate::services::gics_service::{MarketCapCategory, PeerIdentificationService};
use crate::services::comparative_service::{
    ComparativeService, PercentileRanking, ValuationStatus,
    ValuationMetrics, ProfitabilityMetrics, GrowthMetrics, LeverageMetrics,
};

// ============================================================================
// Request/Response Types
// ============================================================================

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PeerSearchInput {
    pub symbol: String,
    pub sector: String,
    pub industry: String,
    pub market_cap: f64,
    pub min_similarity: Option<f64>,
    pub max_peers: Option<usize>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ComparisonInput {
    pub target_symbol: String,
    pub peer_symbols: Vec<String>,
    pub metrics: Vec<String>, // e.g., ["pe_ratio", "roe", "revenue_growth"]
}

#[derive(Debug, Serialize)]
pub struct PeerCommandResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct PeerInfo {
    pub symbol: String,
    pub name: String,
    pub sector: String,
    pub industry: String,
    pub market_cap: f64,
    pub market_cap_category: String,
    pub similarity_score: f64,
    pub match_details: MatchDetailsResponse,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MatchDetailsResponse {
    pub sector_match: bool,
    pub industry_match: bool,
    pub market_cap_similarity: f64,
    pub geographic_match: bool,
}

#[derive(Debug, Serialize)]
pub struct ComparisonResult {
    pub target: CompanyMetrics,
    pub peers: Vec<CompanyMetrics>,
    pub benchmarks: SectorBenchmarks,
}

#[derive(Debug, Serialize)]
pub struct CompanyMetrics {
    pub symbol: String,
    pub name: String,
    pub valuation: ValuationMetrics,
    pub profitability: ProfitabilityMetrics,
    pub growth: GrowthMetrics,
    pub leverage: LeverageMetrics,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SectorBenchmarks {
    pub sector: String,
    pub median_pe: Option<f64>,
    pub median_pb: Option<f64>,
    pub median_roe: Option<f64>,
    pub median_revenue_growth: Option<f64>,
    pub median_debt_to_equity: Option<f64>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MetricBenchmarks {
    pub metric_name: String,
    pub sector_median: f64,
    pub sector_mean: f64,
    pub sector_std_dev: f64,
    pub top_quartile: f64,
    pub bottom_quartile: f64,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct PercentileInput {
    pub symbol: String,
    pub metric_name: String,
    pub metric_value: f64,
    pub peer_values: Vec<f64>,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MetricRanking {
    pub symbol: String,
    pub ranking: PercentileRanking,
    pub valuation_status: Option<ValuationStatus>,
}

// ============================================================================
// Tauri Commands
// ============================================================================

/// Find peer companies based on GICS sector, industry, and market cap
#[tauri::command]
pub async fn find_peers(input: PeerSearchInput) -> PeerCommandResponse<Vec<PeerInfo>> {
    // Yahoo Finance (yfinance) doesn't provide a peer discovery API
    // Return empty list - users should manually select peer companies from the same sector/industry

    eprintln!("Peer discovery called for {} in sector: {}, industry: {}",
              input.symbol, input.sector, input.industry);
    eprintln!("Note: Automatic peer discovery not available with yfinance. Please select peers manually.");

    PeerCommandResponse {
        success: true,
        data: Some(Vec::new()),  // Return empty peer list
        error: None,
    }
}

/// Compare financial metrics across target and peer companies
#[tauri::command]
pub async fn compare_peers(app: tauri::AppHandle, input: ComparisonInput) -> PeerCommandResponse<ComparisonResult> {
    // Fetch real company data from FMP
    let target_metrics = match fetch_company_metrics(&app, &input.target_symbol).await {
        Ok(metrics) => metrics,
        Err(e) => {
            return PeerCommandResponse {
                success: false,
                data: None,
                error: Some(format!("Failed to fetch target company data: {}", e)),
            };
        }
    };

    let mut peer_metrics = Vec::new();
    for symbol in &input.peer_symbols {
        match fetch_company_metrics(&app, symbol).await {
            Ok(metrics) => peer_metrics.push(metrics),
            Err(e) => {
                eprintln!("Failed to fetch peer data for {}: {}", symbol, e);
                // Continue with other peers
                continue;
            }
        }
    }

    if peer_metrics.is_empty() {
        return PeerCommandResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch data for any peer companies".to_string()),
        };
    }

    // Calculate sector benchmarks from actual peer group
    let all_companies = std::iter::once(&target_metrics)
        .chain(peer_metrics.iter())
        .collect::<Vec<&CompanyMetrics>>();

    let pe_values: Vec<f64> = all_companies.iter()
        .filter_map(|c| c.valuation.pe_ratio)
        .collect();
    let pb_values: Vec<f64> = all_companies.iter()
        .filter_map(|c| c.valuation.pb_ratio)
        .collect();
    let roe_values: Vec<f64> = all_companies.iter()
        .filter_map(|c| c.profitability.roe)
        .collect();
    let rev_growth_values: Vec<f64> = all_companies.iter()
        .filter_map(|c| c.growth.revenue_growth)
        .collect();
    let debt_equity_values: Vec<f64> = all_companies.iter()
        .filter_map(|c| c.leverage.debt_to_equity)
        .collect();

    let calculate_median = |mut values: Vec<f64>| -> Option<f64> {
        if values.is_empty() {
            return None;
        }
        values.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
        let mid = values.len() / 2;
        if values.len() % 2 == 0 {
            Some((values[mid - 1] + values[mid]) / 2.0)
        } else {
            Some(values[mid])
        }
    };

    // Get sector from target company metrics (will be fetched from FMP)
    let sector = target_metrics.name.split(" Inc.").next().unwrap_or("Unknown").to_string();

    let benchmarks = SectorBenchmarks {
        sector,
        median_pe: calculate_median(pe_values),
        median_pb: calculate_median(pb_values),
        median_roe: calculate_median(roe_values),
        median_revenue_growth: calculate_median(rev_growth_values),
        median_debt_to_equity: calculate_median(debt_equity_values),
    };

    PeerCommandResponse {
        success: true,
        data: Some(ComparisonResult {
            target: target_metrics,
            peers: peer_metrics,
            benchmarks,
        }),
        error: None,
    }
}

/// Fetch real company metrics from Yahoo Finance using yfinance
async fn fetch_company_metrics(app: &tauri::AppHandle, symbol: &str) -> Result<CompanyMetrics, String> {
    // Call yfinance API through Rust commands
    let profile_result = crate::commands::yfinance::execute_yfinance_command(
        app.clone(),
        "company_profile".to_string(),
        vec![symbol.to_string()],
    ).await;

    let ratios_result = crate::commands::yfinance::execute_yfinance_command(
        app.clone(),
        "financial_ratios".to_string(),
        vec![symbol.to_string()],
    ).await;

    // Parse yfinance responses
    let profile_json: serde_json::Value = match profile_result {
        Ok(json_str) => serde_json::from_str(&json_str)
            .map_err(|e| format!("Failed to parse profile JSON: {}", e))?,
        Err(e) => return Err(format!("yfinance profile API error: {}", e)),
    };

    let ratios_json: serde_json::Value = match ratios_result {
        Ok(json_str) => serde_json::from_str(&json_str)
            .map_err(|e| format!("Failed to parse ratios JSON: {}", e))?,
        Err(e) => return Err(format!("yfinance ratios API error: {}", e)),
    };

    // Check for errors in responses
    if let Some(error) = profile_json.get("error") {
        return Err(format!("Profile error for {}: {}", symbol, error));
    }
    if let Some(error) = ratios_json.get("error") {
        return Err(format!("Ratios error for {}: {}", symbol, error));
    }

    // Extract company name from profile
    let company_name = profile_json.get("companyName")
        .and_then(|n| n.as_str())
        .unwrap_or(symbol)
        .to_string();

    // Extract individual metrics from yfinance response
    // Note: yfinance returns percentages as decimals (0.25 = 25%), already in the format we need
    let pe_ratio = ratios_json.get("peRatio").and_then(|v| v.as_f64());
    let pb_ratio = profile_json.get("priceToBook").and_then(|v| v.as_f64())
        .or_else(|| ratios_json.get("priceToBook").and_then(|v| v.as_f64()));
    let ps_ratio = ratios_json.get("priceToSales").and_then(|v| v.as_f64());
    let pcf_ratio = None; // Not directly available in yfinance
    let ev_to_ebitda = None; // Not directly available in yfinance basic info
    let peg_ratio = ratios_json.get("pegRatio").and_then(|v| v.as_f64());

    // yfinance returns these as decimals (0.25 = 25%), so multiply by 100
    let roe = ratios_json.get("returnOnEquity").and_then(|v| v.as_f64()).map(|v| v * 100.0);
    let roa = ratios_json.get("returnOnAssets").and_then(|v| v.as_f64()).map(|v| v * 100.0);
    let roic = None; // Calculate if needed from other metrics
    let gross_margin = ratios_json.get("grossMargin").and_then(|v| v.as_f64()).map(|v| v * 100.0);
    let operating_margin = ratios_json.get("operatingMargin").and_then(|v| v.as_f64()).map(|v| v * 100.0);
    let net_margin = ratios_json.get("profitMargin").and_then(|v| v.as_f64()).map(|v| v * 100.0);

    // Growth metrics - not always available in yfinance basic data
    let revenue_growth = None;
    let earnings_growth = None;
    let fcf_growth = None;

    let debt_to_equity = ratios_json.get("debtToEquity").and_then(|v| v.as_f64());
    let debt_to_assets = None; // Calculate if needed
    let interest_coverage = None; // Not directly available in yfinance

    Ok(CompanyMetrics {
        symbol: symbol.to_string(),
        name: company_name,
        valuation: ValuationMetrics {
            pe_ratio,
            pb_ratio,
            ps_ratio,
            pcf_ratio,
            ev_to_ebitda,
            peg_ratio,
        },
        profitability: ProfitabilityMetrics {
            roe,
            roa,
            roic,
            gross_margin,
            operating_margin,
            net_margin,
        },
        growth: GrowthMetrics {
            revenue_growth,
            earnings_growth,
            fcf_growth,
        },
        leverage: LeverageMetrics {
            debt_to_equity,
            debt_to_assets,
            interest_coverage,
        },
    })
}

/// Get sector-level benchmarks for specific metrics
#[tauri::command]
pub async fn get_sector_benchmarks(_sector: String) -> PeerCommandResponse<MetricBenchmarks> {
    // Mock implementation
    let benchmarks = MetricBenchmarks {
        metric_name: "PE Ratio".to_string(),
        sector_median: 22.0,
        sector_mean: 24.5,
        sector_std_dev: 8.2,
        top_quartile: 30.0,
        bottom_quartile: 15.0,
    };

    PeerCommandResponse {
        success: true,
        data: Some(benchmarks),
        error: None,
    }
}

/// Calculate percentile ranking for a company's metric within peer group
#[tauri::command]
pub async fn calculate_peer_percentiles(input: PercentileInput) -> PeerCommandResponse<MetricRanking> {
    let comparative_service = ComparativeService::new();

    let ranking = comparative_service.generate_percentile_ranking(
        &input.metric_name,
        input.metric_value,
        &input.peer_values,
    );

    // Determine valuation status if this is a valuation metric
    let valuation_status = if input.metric_name.contains("PE") || input.metric_name.contains("PB") {
        Some(comparative_service.assess_valuation_status(
            ranking.percentile,
            ranking.percentile,
            ranking.percentile,
        ))
    } else {
        None
    };

    PeerCommandResponse {
        success: true,
        data: Some(MetricRanking {
            symbol: input.symbol,
            ranking,
            valuation_status,
        }),
        error: None,
    }
}

// ============================================================================
// Helper Functions (Mock Data)
// ============================================================================

fn create_mock_candidates() -> Vec<HashMap<String, serde_json::Value>> {
    vec![
        HashMap::from([
            ("symbol".to_string(), serde_json::json!("MSFT")),
            ("name".to_string(), serde_json::json!("Microsoft Corporation")),
            ("sector".to_string(), serde_json::json!("Information Technology")),
            ("industry".to_string(), serde_json::json!("Software")),
            ("marketCap".to_string(), serde_json::json!(2_800_000_000_000.0)),
        ]),
        HashMap::from([
            ("symbol".to_string(), serde_json::json!("GOOGL")),
            ("name".to_string(), serde_json::json!("Alphabet Inc.")),
            ("sector".to_string(), serde_json::json!("Communication Services")),
            ("industry".to_string(), serde_json::json!("Internet Content & Information")),
            ("marketCap".to_string(), serde_json::json!(1_700_000_000_000.0)),
        ]),
        HashMap::from([
            ("symbol".to_string(), serde_json::json!("NVDA")),
            ("name".to_string(), serde_json::json!("NVIDIA Corporation")),
            ("sector".to_string(), serde_json::json!("Information Technology")),
            ("industry".to_string(), serde_json::json!("Semiconductors")),
            ("marketCap".to_string(), serde_json::json!(2_200_000_000_000.0)),
        ]),
    ]
}

fn create_mock_company_metrics(symbol: &str) -> CompanyMetrics {
    // Generate realistic, varied metrics for different companies
    let (valuation, profitability, growth, leverage) = match symbol {
        "AAPL" => (
            ValuationMetrics {
                pe_ratio: Some(28.5),
                pb_ratio: Some(45.2),
                ps_ratio: Some(7.1),
                pcf_ratio: Some(22.0),
                ev_to_ebitda: Some(18.5),
                peg_ratio: Some(1.9),
            },
            ProfitabilityMetrics {
                roe: Some(147.4),
                roa: Some(27.8),
                roic: Some(54.2),
                gross_margin: Some(43.3),
                operating_margin: Some(30.7),
                net_margin: Some(26.4),
            },
            GrowthMetrics {
                revenue_growth: Some(7.8),
                earnings_growth: Some(13.4),
                fcf_growth: Some(10.2),
            },
            LeverageMetrics {
                debt_to_equity: Some(2.0),
                debt_to_assets: Some(0.35),
                interest_coverage: Some(32.5),
            },
        ),
        "MSFT" => (
            ValuationMetrics {
                pe_ratio: Some(32.4),
                pb_ratio: Some(12.8),
                ps_ratio: Some(11.2),
                pcf_ratio: Some(25.3),
                ev_to_ebitda: Some(21.2),
                peg_ratio: Some(2.1),
            },
            ProfitabilityMetrics {
                roe: Some(39.7),
                roa: Some(15.2),
                roic: Some(24.5),
                gross_margin: Some(68.4),
                operating_margin: Some(42.1),
                net_margin: Some(34.8),
            },
            GrowthMetrics {
                revenue_growth: Some(16.2),
                earnings_growth: Some(22.1),
                fcf_growth: Some(18.7),
            },
            LeverageMetrics {
                debt_to_equity: Some(0.45),
                debt_to_assets: Some(0.21),
                interest_coverage: Some(28.9),
            },
        ),
        "GOOGL" => (
            ValuationMetrics {
                pe_ratio: Some(24.1),
                pb_ratio: Some(6.2),
                ps_ratio: Some(5.8),
                pcf_ratio: Some(18.7),
                ev_to_ebitda: Some(14.3),
                peg_ratio: Some(1.5),
            },
            ProfitabilityMetrics {
                roe: Some(28.5),
                roa: Some(18.9),
                roic: Some(26.3),
                gross_margin: Some(56.9),
                operating_margin: Some(27.4),
                net_margin: Some(22.1),
            },
            GrowthMetrics {
                revenue_growth: Some(13.5),
                earnings_growth: Some(16.8),
                fcf_growth: Some(14.2),
            },
            LeverageMetrics {
                debt_to_equity: Some(0.12),
                debt_to_assets: Some(0.05),
                interest_coverage: Some(75.2),
            },
        ),
        "NVDA" => (
            ValuationMetrics {
                pe_ratio: Some(65.3),
                pb_ratio: Some(52.8),
                ps_ratio: Some(38.2),
                pcf_ratio: Some(48.9),
                ev_to_ebitda: Some(45.6),
                peg_ratio: Some(0.5),
            },
            ProfitabilityMetrics {
                roe: Some(85.2),
                roa: Some(48.5),
                roic: Some(62.7),
                gross_margin: Some(72.4),
                operating_margin: Some(54.2),
                net_margin: Some(48.9),
            },
            GrowthMetrics {
                revenue_growth: Some(126.0),
                earnings_growth: Some(168.0),
                fcf_growth: Some(142.5),
            },
            LeverageMetrics {
                debt_to_equity: Some(0.25),
                debt_to_assets: Some(0.08),
                interest_coverage: Some(95.8),
            },
        ),
        "META" => (
            ValuationMetrics {
                pe_ratio: Some(22.8),
                pb_ratio: Some(7.4),
                ps_ratio: Some(6.2),
                pcf_ratio: Some(16.5),
                ev_to_ebitda: Some(13.7),
                peg_ratio: Some(1.3),
            },
            ProfitabilityMetrics {
                roe: Some(32.4),
                roa: Some(22.1),
                roic: Some(28.9),
                gross_margin: Some(79.8),
                operating_margin: Some(38.6),
                net_margin: Some(29.4),
            },
            GrowthMetrics {
                revenue_growth: Some(23.2),
                earnings_growth: Some(35.8),
                fcf_growth: Some(28.3),
            },
            LeverageMetrics {
                debt_to_equity: Some(0.08),
                debt_to_assets: Some(0.03),
                interest_coverage: Some(112.4),
            },
        ),
        // Default/fallback for other symbols
        _ => (
            ValuationMetrics {
                pe_ratio: Some(20.0 + (symbol.len() as f64 * 2.5)),
                pb_ratio: Some(4.0 + (symbol.len() as f64 * 0.8)),
                ps_ratio: Some(5.5 + (symbol.len() as f64 * 0.6)),
                pcf_ratio: Some(18.0),
                ev_to_ebitda: Some(15.0),
                peg_ratio: Some(1.6),
            },
            ProfitabilityMetrics {
                roe: Some(18.0 + (symbol.len() as f64)),
                roa: Some(10.0 + (symbol.len() as f64 * 0.5)),
                roic: Some(16.0),
                gross_margin: Some(60.0),
                operating_margin: Some(28.0),
                net_margin: Some(20.0),
            },
            GrowthMetrics {
                revenue_growth: Some(12.0),
                earnings_growth: Some(15.0),
                fcf_growth: Some(11.0),
            },
            LeverageMetrics {
                debt_to_equity: Some(0.6),
                debt_to_assets: Some(0.25),
                interest_coverage: Some(18.5),
            },
        ),
    };

    CompanyMetrics {
        symbol: symbol.to_string(),
        name: format!("{} Inc.", symbol),
        valuation,
        profitability,
        growth,
        leverage,
    }
}
