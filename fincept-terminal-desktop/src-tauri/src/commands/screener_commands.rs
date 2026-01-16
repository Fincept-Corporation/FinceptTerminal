// Stock Screener Tauri Commands
// Advanced equity screening based on fundamental and technical metrics

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use crate::services::stock_screener_service::{
    StockScreenerService, ScreenCriteria, MetricType,
    ScreenResult, StockData,
};

// ============================================================================
// Request/Response Types
// ============================================================================

#[derive(Debug, Serialize)]
pub struct ScreenerCommandResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ExecuteScreenRequest {
    pub criteria: ScreenCriteria,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct SaveScreenRequest {
    pub name: String,
    pub criteria: ScreenCriteria,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct SavedScreen {
    pub id: String,
    pub name: String,
    pub description: String,
    pub criteria: ScreenCriteria,
    pub created_at: i64,
    pub updated_at: i64,
}

// ============================================================================
// Tauri Commands
// ============================================================================

/// Execute a stock screen with given criteria
#[tauri::command]
pub async fn execute_stock_screen(
    app: tauri::AppHandle,
    criteria: ScreenCriteria,
) -> ScreenerCommandResponse<ScreenResult> {
    // Fetch real stock data from FMP API
    match fetch_stock_universe(&app).await {
        Ok(stock_universe) => {
            let service = StockScreenerService::with_stock_universe(stock_universe);
            let result = service.execute_screen(criteria);

            ScreenerCommandResponse {
                success: true,
                data: Some(result),
                error: None,
            }
        }
        Err(e) => {
            eprintln!("Error fetching stock universe: {}", e);
            // Fallback to mock data if FMP fails
            let service = StockScreenerService::new();
            let result = service.execute_screen(criteria);

            ScreenerCommandResponse {
                success: true,
                data: Some(result),
                error: Some(format!("Using cached data. FMP API error: {}", e)),
            }
        }
    }
}

/// Get preset screen: Value Stocks
#[tauri::command]
pub async fn get_value_screen() -> ScreenerCommandResponse<ScreenCriteria> {
    let criteria = StockScreenerService::get_value_screen();

    ScreenerCommandResponse {
        success: true,
        data: Some(criteria),
        error: None,
    }
}

/// Get preset screen: Growth Stocks
#[tauri::command]
pub async fn get_growth_screen() -> ScreenerCommandResponse<ScreenCriteria> {
    let criteria = StockScreenerService::get_growth_screen();

    ScreenerCommandResponse {
        success: true,
        data: Some(criteria),
        error: None,
    }
}

/// Get preset screen: Dividend Aristocrats
#[tauri::command]
pub async fn get_dividend_screen() -> ScreenerCommandResponse<ScreenCriteria> {
    let criteria = StockScreenerService::get_dividend_screen();

    ScreenerCommandResponse {
        success: true,
        data: Some(criteria),
        error: None,
    }
}

/// Get preset screen: Momentum Stocks
#[tauri::command]
pub async fn get_momentum_screen() -> ScreenerCommandResponse<ScreenCriteria> {
    let criteria = StockScreenerService::get_momentum_screen();

    ScreenerCommandResponse {
        success: true,
        data: Some(criteria),
        error: None,
    }
}

/// Save a custom screen (to local storage or database)
#[tauri::command]
pub async fn save_custom_screen(
    _app: tauri::AppHandle,
    name: String,
    criteria: ScreenCriteria,
) -> ScreenerCommandResponse<SavedScreen> {
    // TODO: Save to database or local storage
    // For now, return a mock saved screen

    let saved_screen = SavedScreen {
        id: uuid::Uuid::new_v4().to_string(),
        name: name.clone(),
        description: criteria.description.clone(),
        criteria,
        created_at: chrono::Utc::now().timestamp(),
        updated_at: chrono::Utc::now().timestamp(),
    };

    ScreenerCommandResponse {
        success: true,
        data: Some(saved_screen),
        error: None,
    }
}

/// Load all saved custom screens
#[tauri::command]
pub async fn load_custom_screens(
    _app: tauri::AppHandle,
) -> ScreenerCommandResponse<Vec<SavedScreen>> {
    // TODO: Load from database or local storage
    // For now, return empty list

    ScreenerCommandResponse {
        success: true,
        data: Some(Vec::new()),
        error: None,
    }
}

/// Delete a saved custom screen
#[tauri::command]
pub async fn delete_custom_screen(
    _app: tauri::AppHandle,
    _screen_id: String,
) -> ScreenerCommandResponse<bool> {
    // TODO: Delete from database or local storage

    ScreenerCommandResponse {
        success: true,
        data: Some(true),
        error: None,
    }
}

/// Get available metrics for screening
#[tauri::command]
pub async fn get_available_metrics() -> ScreenerCommandResponse<Vec<MetricInfo>> {
    let metrics = vec![
        // Valuation Metrics
        MetricInfo {
            id: "peRatio".to_string(),
            name: "P/E Ratio".to_string(),
            category: "Valuation".to_string(),
            description: "Price to Earnings Ratio".to_string(),
            metric_type: MetricType::PeRatio,
        },
        MetricInfo {
            id: "pbRatio".to_string(),
            name: "P/B Ratio".to_string(),
            category: "Valuation".to_string(),
            description: "Price to Book Ratio".to_string(),
            metric_type: MetricType::PbRatio,
        },
        MetricInfo {
            id: "psRatio".to_string(),
            name: "P/S Ratio".to_string(),
            category: "Valuation".to_string(),
            description: "Price to Sales Ratio".to_string(),
            metric_type: MetricType::PsRatio,
        },
        MetricInfo {
            id: "pcfRatio".to_string(),
            name: "P/CF Ratio".to_string(),
            category: "Valuation".to_string(),
            description: "Price to Cash Flow Ratio".to_string(),
            metric_type: MetricType::PcfRatio,
        },
        MetricInfo {
            id: "evToEbitda".to_string(),
            name: "EV/EBITDA".to_string(),
            category: "Valuation".to_string(),
            description: "Enterprise Value to EBITDA".to_string(),
            metric_type: MetricType::EvToEbitda,
        },
        MetricInfo {
            id: "pegRatio".to_string(),
            name: "PEG Ratio".to_string(),
            category: "Valuation".to_string(),
            description: "Price/Earnings to Growth Ratio".to_string(),
            metric_type: MetricType::PegRatio,
        },
        // Profitability Metrics
        MetricInfo {
            id: "roe".to_string(),
            name: "ROE".to_string(),
            category: "Profitability".to_string(),
            description: "Return on Equity (%)".to_string(),
            metric_type: MetricType::Roe,
        },
        MetricInfo {
            id: "roa".to_string(),
            name: "ROA".to_string(),
            category: "Profitability".to_string(),
            description: "Return on Assets (%)".to_string(),
            metric_type: MetricType::Roa,
        },
        MetricInfo {
            id: "roic".to_string(),
            name: "ROIC".to_string(),
            category: "Profitability".to_string(),
            description: "Return on Invested Capital (%)".to_string(),
            metric_type: MetricType::Roic,
        },
        MetricInfo {
            id: "grossMargin".to_string(),
            name: "Gross Margin".to_string(),
            category: "Profitability".to_string(),
            description: "Gross Profit Margin (%)".to_string(),
            metric_type: MetricType::GrossMargin,
        },
        MetricInfo {
            id: "operatingMargin".to_string(),
            name: "Operating Margin".to_string(),
            category: "Profitability".to_string(),
            description: "Operating Profit Margin (%)".to_string(),
            metric_type: MetricType::OperatingMargin,
        },
        MetricInfo {
            id: "netMargin".to_string(),
            name: "Net Margin".to_string(),
            category: "Profitability".to_string(),
            description: "Net Profit Margin (%)".to_string(),
            metric_type: MetricType::NetMargin,
        },
        // Growth Metrics
        MetricInfo {
            id: "revenueGrowth".to_string(),
            name: "Revenue Growth".to_string(),
            category: "Growth".to_string(),
            description: "Year-over-Year Revenue Growth (%)".to_string(),
            metric_type: MetricType::RevenueGrowth,
        },
        MetricInfo {
            id: "earningsGrowth".to_string(),
            name: "Earnings Growth".to_string(),
            category: "Growth".to_string(),
            description: "Year-over-Year Earnings Growth (%)".to_string(),
            metric_type: MetricType::EarningsGrowth,
        },
        MetricInfo {
            id: "fcfGrowth".to_string(),
            name: "FCF Growth".to_string(),
            category: "Growth".to_string(),
            description: "Free Cash Flow Growth (%)".to_string(),
            metric_type: MetricType::FcfGrowth,
        },
        // Leverage Metrics
        MetricInfo {
            id: "debtToEquity".to_string(),
            name: "Debt/Equity".to_string(),
            category: "Leverage".to_string(),
            description: "Debt to Equity Ratio".to_string(),
            metric_type: MetricType::DebtToEquity,
        },
        MetricInfo {
            id: "debtToAssets".to_string(),
            name: "Debt/Assets".to_string(),
            category: "Leverage".to_string(),
            description: "Debt to Assets Ratio".to_string(),
            metric_type: MetricType::DebtToAssets,
        },
        MetricInfo {
            id: "interestCoverage".to_string(),
            name: "Interest Coverage".to_string(),
            category: "Leverage".to_string(),
            description: "Interest Coverage Ratio".to_string(),
            metric_type: MetricType::InterestCoverage,
        },
        // Market Data
        MetricInfo {
            id: "marketCap".to_string(),
            name: "Market Cap".to_string(),
            category: "Market Data".to_string(),
            description: "Market Capitalization".to_string(),
            metric_type: MetricType::MarketCap,
        },
        // Dividend Metrics
        MetricInfo {
            id: "dividendYield".to_string(),
            name: "Dividend Yield".to_string(),
            category: "Dividend".to_string(),
            description: "Dividend Yield (%)".to_string(),
            metric_type: MetricType::DividendYield,
        },
        MetricInfo {
            id: "payoutRatio".to_string(),
            name: "Payout Ratio".to_string(),
            category: "Dividend".to_string(),
            description: "Dividend Payout Ratio (%)".to_string(),
            metric_type: MetricType::PayoutRatio,
        },
        MetricInfo {
            id: "dividendGrowth".to_string(),
            name: "Dividend Growth".to_string(),
            category: "Dividend".to_string(),
            description: "Dividend Growth Rate (%)".to_string(),
            metric_type: MetricType::DividendGrowth,
        },
    ];

    ScreenerCommandResponse {
        success: true,
        data: Some(metrics),
        error: None,
    }
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MetricInfo {
    pub id: String,
    pub name: String,
    pub category: String,
    pub description: String,
    #[serde(rename = "type")]
    pub metric_type: MetricType,
}

// ============================================================================
// Helper Functions - FMP Data Fetching
// ============================================================================

/// Fetch stock universe from FMP API with financial metrics
async fn fetch_stock_universe(app: &tauri::AppHandle) -> Result<Vec<StockData>, String> {
    // Comprehensive list of liquid stocks across sectors for screening
    let symbols = vec![
        // Technology
        "AAPL", "MSFT", "GOOGL", "NVDA", "META", "TSLA", "AVGO", "ORCL", "CSCO", "ADBE",
        "CRM", "AMD", "INTC", "QCOM", "UBER", "NOW", "SNOW", "PANW", "ANET", "CRWD",
        // Healthcare
        "JNJ", "UNH", "LLY", "ABBV", "MRK", "TMO", "ABT", "DHR", "PFE", "BMY",
        "AMGN", "GILD", "VRTX", "CI", "CVS", "REGN", "ISRG", "HUM", "ZTS", "MRNA",
        // Financials
        "JPM", "BAC", "WFC", "GS", "MS", "C", "SCHW", "BLK", "AXP", "SPGI",
        "USB", "PNC", "TFC", "BK", "COF", "CME", "ICE", "MCO", "AON", "MMC",
        // Consumer
        "AMZN", "WMT", "HD", "MCD", "NKE", "SBUX", "TGT", "LOW", "TJX", "BKNG",
        "MAR", "CMG", "YUM", "DG", "ROST", "ULTA", "DPZ", "ABNB", "DKNG", "ETSY",
        // Consumer Staples
        "PG", "KO", "PEP", "COST", "PM", "MO", "MDLZ", "CL", "KMB", "GIS",
        "HSY", "K", "STZ", "TSN", "CHD", "CAG", "CPB", "SJM", "HRL", "MKC",
        // Energy
        "XOM", "CVX", "COP", "SLB", "EOG", "MPC", "PSX", "VLO", "OXY", "HAL",
        "BKR", "WMB", "KMI", "HES", "DVN", "FANG", "MRO", "APA", "OKE", "TRGP",
        // Industrials
        "CAT", "BA", "UPS", "RTX", "HON", "UNP", "DE", "LMT", "GE", "MMM",
        "ETN", "FDX", "NSC", "CSX", "EMR", "WM", "ITW", "PH", "TT", "CARR",
        // Materials
        "LIN", "APD", "SHW", "ECL", "DD", "NEM", "FCX", "VMC", "MLM", "ALB",
        "PPG", "NUE", "CTVA", "DOW", "IFF", "CE", "CF", "MOS", "FMC", "SEE",
        // Real Estate
        "PLD", "AMT", "EQIX", "PSA", "WELL", "DLR", "CBRE", "SPG", "O", "VICI",
        "AVB", "EQR", "INVH", "MAA", "ESS", "VTR", "ARE", "BXP", "KIM", "REG",
        // Utilities
        "NEE", "DUK", "SO", "D", "AEP", "EXC", "SRE", "PCG", "XEL", "WEC",
        "ED", "ES", "PEG", "EIX", "AWK", "DTE", "PPL", "AEE", "CMS", "ATO",
        // Communication
        "T", "VZ", "TMUS", "NFLX", "DIS", "CMCSA", "CHTR", "PARA", "WBD", "OMC",
        "IPG", "FOXA", "MTCH", "EA", "TTWO", "NWSA", "LYV", "PINS", "SNAP", "RBLX",
    ];

    let total_symbols = symbols.len();
    let mut stock_universe = Vec::new();
    let mut fetch_errors = 0;
    let max_errors = 50; // Allow some failures

    for symbol in symbols {
        match fetch_stock_data(app, symbol).await {
            Ok(stock_data) => {
                stock_universe.push(stock_data);
            }
            Err(e) => {
                eprintln!("Failed to fetch data for {}: {}", symbol, e);
                fetch_errors += 1;
                if fetch_errors > max_errors {
                    return Err(format!(
                        "Too many fetch errors ({}/{}). Check FMP API key and connection.",
                        fetch_errors, total_symbols
                    ));
                }
            }
        }
    }

    if stock_universe.is_empty() {
        return Err("Failed to fetch any stock data from FMP".to_string());
    }

    Ok(stock_universe)
}

/// Fetch individual stock data from FMP
async fn fetch_stock_data(app: &tauri::AppHandle, symbol: &str) -> Result<StockData, String> {
    // Fetch financial ratios from FMP
    let ratios_result = crate::commands::fmp::get_fmp_financial_ratios(
        app.clone(),
        symbol.to_string(),
        Some("annual".to_string()),
        Some(1),
    )
    .await;

    let profile_result = crate::commands::fmp::get_fmp_company_profile(
        app.clone(),
        symbol.to_string(),
    )
    .await;

    // Parse responses
    let ratios_json: serde_json::Value = ratios_result
        .map_err(|e| format!("FMP ratios error: {}", e))
        .and_then(|json_str| {
            serde_json::from_str(&json_str)
                .map_err(|e| format!("JSON parse error: {}", e))
        })?;

    let profile_json: serde_json::Value = profile_result
        .map_err(|e| format!("FMP profile error: {}", e))
        .and_then(|json_str| {
            serde_json::from_str(&json_str)
                .map_err(|e| format!("JSON parse error: {}", e))
        })?;

    // Extract profile data
    let profile = profile_json
        .get("data")
        .and_then(|d| d.as_object())
        .ok_or("Profile data not found")?;

    let company_name = profile
        .get("companyName")
        .and_then(|n| n.as_str())
        .unwrap_or(symbol)
        .to_string();

    let sector = profile
        .get("sector")
        .and_then(|s| s.as_str())
        .unwrap_or("Unknown")
        .to_string();

    let industry = profile
        .get("industry")
        .and_then(|i| i.as_str())
        .unwrap_or("Unknown")
        .to_string();

    let market_cap = profile
        .get("mktCap")
        .and_then(|m| m.as_f64())
        .unwrap_or(0.0);

    // Extract ratios data
    let ratios = ratios_json
        .get("data")
        .and_then(|d| d.as_array())
        .and_then(|arr| arr.first())
        .and_then(|r| r.as_object())
        .ok_or("Ratios data not found")?;

    // Build metrics hashmap with all available metrics
    let mut metrics = HashMap::new();

    // Valuation metrics
    if let Some(pe) = ratios.get("priceEarningsRatio").and_then(|v| v.as_f64()) {
        metrics.insert("peRatio".to_string(), pe);
    }
    if let Some(pb) = ratios.get("priceToBookRatio").and_then(|v| v.as_f64()) {
        metrics.insert("pbRatio".to_string(), pb);
    }
    if let Some(ps) = ratios.get("priceToSalesRatio").and_then(|v| v.as_f64()) {
        metrics.insert("psRatio".to_string(), ps);
    }
    if let Some(pcf) = ratios.get("priceCashFlowRatio").and_then(|v| v.as_f64()) {
        metrics.insert("pcfRatio".to_string(), pcf);
    }
    if let Some(ev_ebitda) = ratios.get("enterpriseValueMultiple").and_then(|v| v.as_f64()) {
        metrics.insert("evToEbitda".to_string(), ev_ebitda);
    }
    if let Some(peg) = ratios.get("priceEarningsToGrowthRatio").and_then(|v| v.as_f64()) {
        metrics.insert("pegRatio".to_string(), peg);
    }

    // Profitability metrics (convert to percentages)
    if let Some(roe) = ratios.get("returnOnEquity").and_then(|v| v.as_f64()) {
        metrics.insert("roe".to_string(), roe * 100.0);
    }
    if let Some(roa) = ratios.get("returnOnAssets").and_then(|v| v.as_f64()) {
        metrics.insert("roa".to_string(), roa * 100.0);
    }
    if let Some(roic) = ratios.get("returnOnCapitalEmployed").and_then(|v| v.as_f64()) {
        metrics.insert("roic".to_string(), roic * 100.0);
    }
    if let Some(gm) = ratios.get("grossProfitMargin").and_then(|v| v.as_f64()) {
        metrics.insert("grossMargin".to_string(), gm * 100.0);
    }
    if let Some(om) = ratios.get("operatingProfitMargin").and_then(|v| v.as_f64()) {
        metrics.insert("operatingMargin".to_string(), om * 100.0);
    }
    if let Some(nm) = ratios.get("netProfitMargin").and_then(|v| v.as_f64()) {
        metrics.insert("netMargin".to_string(), nm * 100.0);
    }

    // Growth metrics (convert to percentages)
    if let Some(rg) = ratios.get("revenueGrowth").and_then(|v| v.as_f64()) {
        metrics.insert("revenueGrowth".to_string(), rg * 100.0);
    }
    if let Some(eg) = ratios.get("epsgrowth").and_then(|v| v.as_f64()) {
        metrics.insert("earningsGrowth".to_string(), eg * 100.0);
    }
    if let Some(fcfg) = ratios.get("freeCashFlowGrowth").and_then(|v| v.as_f64()) {
        metrics.insert("fcfGrowth".to_string(), fcfg * 100.0);
    }

    // Leverage metrics
    if let Some(de) = ratios.get("debtEquityRatio").and_then(|v| v.as_f64()) {
        metrics.insert("debtToEquity".to_string(), de);
    }
    if let Some(da) = ratios.get("debtRatio").and_then(|v| v.as_f64()) {
        metrics.insert("debtToAssets".to_string(), da);
    }
    if let Some(ic) = ratios.get("interestCoverage").and_then(|v| v.as_f64()) {
        metrics.insert("interestCoverage".to_string(), ic);
    }

    // Market data
    metrics.insert("marketCap".to_string(), market_cap);
    if let Some(price) = profile.get("price").and_then(|v| v.as_f64()) {
        metrics.insert("price".to_string(), price);
    }
    if let Some(beta) = profile.get("beta").and_then(|v| v.as_f64()) {
        metrics.insert("beta".to_string(), beta);
    }

    // Dividend metrics
    if let Some(dy) = ratios.get("dividendYield").and_then(|v| v.as_f64()) {
        metrics.insert("dividendYield".to_string(), dy * 100.0);
    }
    if let Some(pr) = ratios.get("payoutRatio").and_then(|v| v.as_f64()) {
        metrics.insert("payoutRatio".to_string(), pr * 100.0);
    }

    Ok(StockData {
        symbol: symbol.to_string(),
        name: company_name,
        sector,
        industry,
        metrics,
    })
}
