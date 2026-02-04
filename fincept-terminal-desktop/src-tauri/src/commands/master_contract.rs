#![allow(dead_code)]
// Master Contract Commands
// Tauri commands for broker symbol data management and search

use crate::database::master_contract::{self, SearchResult, Symbol};
use serde::{Deserialize, Serialize};

// ============================================================================
// Data Structures
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct MasterContractInfo {
    pub broker_id: String,
    pub symbol_count: i64,
    pub last_updated: i64,
    pub created_at: i64,
    pub cache_age_seconds: i64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResult {
    pub success: bool,
    pub broker_id: String,
    pub symbol_count: i64,
    pub message: String,
}

/// Angel One symbol from their JSON API
#[derive(Debug, Deserialize)]
struct AngelOneSymbol {
    token: String,
    symbol: String,
    name: String,
    expiry: Option<String>,
    strike: Option<String>,
    lotsize: Option<String>,
    instrumenttype: Option<String>,
    exch_seg: String,
    tick_size: Option<String>,
}

/// Fyers symbol from their CSV
#[derive(Debug, Deserialize)]
struct FyersSymbol {
    #[serde(rename = "Fytoken")]
    fytoken: String,
    #[serde(rename = "Symbol Details")]
    symbol_details: Option<String>,
    #[serde(rename = "Exchange Instrument type")]
    exchange_instrument_type: Option<i32>,
    #[serde(rename = "Minimum lot size")]
    minimum_lot_size: Option<i32>,
    #[serde(rename = "Tick size")]
    tick_size: Option<f64>,
    #[serde(rename = "ISIN")]
    isin: Option<String>,
    #[serde(rename = "Trading Session")]
    trading_session: Option<String>,
    #[serde(rename = "Last update date")]
    last_update_date: Option<String>,
    #[serde(rename = "Expiry date")]
    expiry_date: Option<String>,
    #[serde(rename = "Symbol ticker")]
    symbol_ticker: String,
    #[serde(rename = "Exchange")]
    exchange: Option<i32>,
    #[serde(rename = "Segment")]
    segment: Option<i32>,
    #[serde(rename = "Scrip code")]
    scrip_code: Option<i32>,
    #[serde(rename = "Underlying symbol")]
    underlying_symbol: Option<String>,
    #[serde(rename = "Underlying scrip code")]
    underlying_scrip_code: Option<i64>,
    #[serde(rename = "Strike price")]
    strike_price: Option<f64>,
    #[serde(rename = "Option type")]
    option_type: Option<String>,
}

// ============================================================================
// Download Commands
// ============================================================================

/// Download master contract for Angel One
#[tauri::command]
pub async fn download_angelone_master_contract() -> Result<DownloadResult, String> {
    eprintln!("[MasterContract] Downloading Angel One master contract...");

    let url = "https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json";

    let response = reqwest::get(url)
        .await
        .map_err(|e| format!("Failed to download: {}", e))?;

    if !response.status().is_success() {
        return Err(format!("HTTP error: {}", response.status()));
    }

    let json_data: Vec<AngelOneSymbol> = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse JSON: {}", e))?;

    eprintln!("[MasterContract] Downloaded {} symbols from Angel One", json_data.len());

    // Transform to our schema
    let symbols: Vec<Symbol> = json_data
        .into_iter()
        .map(|s| {
            // Normalize exchange
            let exchange = normalize_angelone_exchange(&s.exch_seg, s.instrumenttype.as_deref());

            // Normalize symbol (remove suffixes like -EQ, -BE, etc.)
            let symbol = s.symbol
                .replace("-EQ", "")
                .replace("-BE", "")
                .replace("-MF", "")
                .replace("-SG", "");

            // Parse strike price
            let strike = s.strike
                .as_ref()
                .and_then(|st| st.parse::<f64>().ok())
                .map(|st| st / 100.0);

            // Parse lot size
            let lot_size = s.lotsize
                .as_ref()
                .and_then(|ls| ls.parse::<i32>().ok());

            // Parse tick size
            let tick_size = s.tick_size
                .as_ref()
                .and_then(|ts| ts.parse::<f64>().ok())
                .map(|ts| ts / 100.0);

            // Normalize instrument type
            let instrument_type = normalize_instrument_type(s.instrumenttype.as_deref());

            // Format expiry
            let expiry = s.expiry.as_ref().map(|e| format_angelone_expiry(e));

            Symbol {
                id: None,
                broker_id: "angelone".to_string(),
                symbol,
                br_symbol: s.symbol.clone(),
                name: Some(s.name),
                exchange,
                br_exchange: Some(s.exch_seg.clone()),
                token: s.token,
                expiry,
                strike,
                lot_size,
                instrument_type,
                tick_size,
                segment: Some(s.exch_seg),
            }
        })
        .collect();

    let symbol_count = symbols.len() as i64;

    // Delete old symbols and insert new ones
    master_contract::delete_symbols_for_broker("angelone")
        .map_err(|e| format!("Failed to delete old symbols: {}", e))?;

    master_contract::bulk_insert_symbols(&symbols)
        .map_err(|e| format!("Failed to insert symbols: {}", e))?;

    // Update cache metadata
    master_contract::save_master_contract("angelone", "{}", symbol_count)
        .map_err(|e| format!("Failed to save cache metadata: {}", e))?;

    eprintln!("[MasterContract] ✓ Angel One master contract saved ({} symbols)", symbol_count);

    Ok(DownloadResult {
        success: true,
        broker_id: "angelone".to_string(),
        symbol_count,
        message: format!("Successfully downloaded {} symbols", symbol_count),
    })
}

/// Download master contract for Fyers
#[tauri::command]
pub async fn download_fyers_master_contract() -> Result<DownloadResult, String> {
    eprintln!("[MasterContract] Downloading Fyers master contract...");

    let segments = vec![
        ("NSE_CM", "https://public.fyers.in/sym_details/NSE_CM.csv"),
        ("NSE_FO", "https://public.fyers.in/sym_details/NSE_FO.csv"),
        ("NSE_CD", "https://public.fyers.in/sym_details/NSE_CD.csv"),
        ("BSE_CM", "https://public.fyers.in/sym_details/BSE_CM.csv"),
        ("BSE_FO", "https://public.fyers.in/sym_details/BSE_FO.csv"),
        ("MCX_COM", "https://public.fyers.in/sym_details/MCX_COM.csv"),
    ];

    let mut all_symbols: Vec<Symbol> = Vec::new();

    for (segment_name, url) in segments {
        eprintln!("[MasterContract] Downloading Fyers {} segment...", segment_name);

        let response = match reqwest::get(url).await {
            Ok(r) => r,
            Err(e) => {
                eprintln!("[MasterContract] Warning: Failed to download {}: {}", segment_name, e);
                continue;
            }
        };

        if !response.status().is_success() {
            eprintln!("[MasterContract] Warning: HTTP error for {}: {}", segment_name, response.status());
            continue;
        }

        let csv_text = response
            .text()
            .await
            .map_err(|e| format!("Failed to read CSV: {}", e))?;

        // Parse CSV with flexible reader
        let mut reader = csv::ReaderBuilder::new()
            .has_headers(false)
            .flexible(true)
            .from_reader(csv_text.as_bytes());

        for result in reader.records() {
            let record: csv::StringRecord = match result {
                Ok(r) => r,
                Err(_) => continue,
            };

            // Fyers CSV format: Fytoken, Symbol Details, Exchange Instrument type, Lot size, Tick size, ISIN, Trading Session, Last update, Expiry, Symbol ticker, Exchange, Segment, Scrip code, Underlying, Underlying code, Strike, Option type, Underlying FyToken, Reserved...
            if record.len() < 10 {
                continue;
            }

            let fytoken = record.get(0).unwrap_or("").to_string();
            let symbol_ticker = record.get(9).unwrap_or("").to_string();

            if fytoken.is_empty() || symbol_ticker.is_empty() {
                continue;
            }

            let exchange = map_fyers_exchange(record.get(10).and_then(|s| s.parse::<i32>().ok()));
            let lot_size = record.get(3).and_then(|s| s.parse::<i32>().ok());
            let tick_size = record.get(4).and_then(|s| s.parse::<f64>().ok());
            let expiry = record.get(8).map(|s| s.to_string()).filter(|s| !s.is_empty());
            let underlying = record.get(13).map(|s| s.to_string()).filter(|s| !s.is_empty());
            let strike = record.get(15).and_then(|s| s.parse::<f64>().ok());
            let option_type = record.get(16).map(|s| s.to_string()).filter(|s| !s.is_empty());

            // Determine instrument type
            let instrument_type = determine_fyers_instrument_type(&symbol_ticker, option_type.as_deref());

            // Extract base symbol
            let symbol = extract_fyers_symbol(&symbol_ticker);

            all_symbols.push(Symbol {
                id: None,
                broker_id: "fyers".to_string(),
                symbol,
                br_symbol: symbol_ticker.clone(),
                name: underlying,
                exchange,
                br_exchange: Some(segment_name.to_string()),
                token: fytoken,
                expiry,
                strike,
                lot_size,
                instrument_type,
                tick_size,
                segment: Some(segment_name.to_string()),
            });
        }
    }

    let symbol_count = all_symbols.len() as i64;

    if symbol_count == 0 {
        return Err("No symbols downloaded from Fyers".to_string());
    }

    // Delete old symbols and insert new ones
    master_contract::delete_symbols_for_broker("fyers")
        .map_err(|e| format!("Failed to delete old symbols: {}", e))?;

    master_contract::bulk_insert_symbols(&all_symbols)
        .map_err(|e| format!("Failed to insert symbols: {}", e))?;

    // Update cache metadata
    master_contract::save_master_contract("fyers", "{}", symbol_count)
        .map_err(|e| format!("Failed to save cache metadata: {}", e))?;

    eprintln!("[MasterContract] ✓ Fyers master contract saved ({} symbols)", symbol_count);

    Ok(DownloadResult {
        success: true,
        broker_id: "fyers".to_string(),
        symbol_count,
        message: format!("Successfully downloaded {} symbols", symbol_count),
    })
}

/// Download master contract for any broker
#[tauri::command]
pub async fn download_master_contract(broker_id: String) -> Result<DownloadResult, String> {
    match broker_id.as_str() {
        "angelone" => download_angelone_master_contract().await,
        "fyers" => download_fyers_master_contract().await,
        "zerodha" => Err("Zerodha master contract requires authentication. Use zerodha_download_instruments command.".to_string()),
        _ => Err(format!("Unknown broker: {}", broker_id)),
    }
}

// ============================================================================
// Search Commands
// ============================================================================

/// Search symbols
#[tauri::command]
pub async fn search_symbols(
    broker_id: String,
    query: String,
    exchange: Option<String>,
    instrument_type: Option<String>,
    limit: Option<i32>,
) -> Result<Vec<SearchResult>, String> {
    let limit = limit.unwrap_or(50);

    master_contract::search_symbols(
        &broker_id,
        &query,
        exchange.as_deref(),
        instrument_type.as_deref(),
        limit,
    )
    .map_err(|e| format!("Search failed: {}", e))
}

/// Get symbol by exact match
#[tauri::command]
pub async fn get_symbol(
    broker_id: String,
    symbol: String,
    exchange: String,
) -> Result<Option<SearchResult>, String> {
    master_contract::get_symbol(&broker_id, &symbol, &exchange)
        .map_err(|e| format!("Failed to get symbol: {}", e))
}

/// Get symbol by token
#[tauri::command]
pub async fn get_symbol_by_token(
    broker_id: String,
    token: String,
) -> Result<Option<SearchResult>, String> {
    master_contract::get_symbol_by_token(&broker_id, &token)
        .map_err(|e| format!("Failed to get symbol: {}", e))
}

/// Get symbol count for a broker
#[tauri::command]
pub async fn get_symbol_count(broker_id: String) -> Result<i64, String> {
    master_contract::get_symbol_count(&broker_id)
        .map_err(|e| format!("Failed to get symbol count: {}", e))
}

/// Get available exchanges for a broker
#[tauri::command]
pub async fn get_exchanges(broker_id: String) -> Result<Vec<String>, String> {
    master_contract::get_exchanges(&broker_id)
        .map_err(|e| format!("Failed to get exchanges: {}", e))
}

/// Get available expiries
#[tauri::command]
pub async fn get_expiries(
    broker_id: String,
    exchange: Option<String>,
    underlying: Option<String>,
) -> Result<Vec<String>, String> {
    master_contract::get_expiries(&broker_id, exchange.as_deref(), underlying.as_deref())
        .map_err(|e| format!("Failed to get expiries: {}", e))
}

// ============================================================================
// Cache Management Commands
// ============================================================================

/// Save master contract cache (legacy)
#[tauri::command]
pub async fn save_master_contract_cache(
    broker_id: String,
    symbols_data: String,
    symbol_count: i64,
) -> Result<String, String> {
    master_contract::save_master_contract(&broker_id, &symbols_data, symbol_count)
        .map_err(|e| format!("Failed to save master contract: {}", e))?;

    Ok(format!(
        "Master contract saved for {} ({} symbols)",
        broker_id, symbol_count
    ))
}

/// Get master contract cache (legacy)
#[tauri::command]
pub async fn get_master_contract_cache(broker_id: String) -> Result<String, String> {
    let cache = master_contract::get_master_contract(&broker_id)
        .map_err(|e| format!("Failed to get master contract: {}", e))?;

    match cache {
        Some(data) => Ok(data.symbols_data),
        None => Err(format!("No master contract found for {}", broker_id)),
    }
}

/// Delete master contract cache
#[tauri::command]
pub async fn delete_master_contract_cache(broker_id: String) -> Result<String, String> {
    // Delete both cache and symbols
    master_contract::delete_symbols_for_broker(&broker_id)
        .map_err(|e| format!("Failed to delete symbols: {}", e))?;

    master_contract::delete_master_contract(&broker_id)
        .map_err(|e| format!("Failed to delete master contract: {}", e))?;

    Ok(format!("Master contract deleted for {}", broker_id))
}

/// Check if cache is valid
#[tauri::command]
pub async fn is_master_contract_cache_valid(
    broker_id: String,
    ttl_seconds: i64,
) -> Result<bool, String> {
    master_contract::is_cache_valid(&broker_id, ttl_seconds)
        .map_err(|e| format!("Failed to check cache validity: {}", e))
}

/// Get master contract cache info
#[tauri::command]
pub async fn get_master_contract_cache_info(
    broker_id: String,
) -> Result<MasterContractInfo, String> {
    let cache = master_contract::get_master_contract(&broker_id)
        .map_err(|e| format!("Failed to get master contract: {}", e))?;

    match cache {
        Some(data) => {
            let cache_age = master_contract::get_cache_age(&broker_id)
                .map_err(|e| format!("Failed to get cache age: {}", e))?
                .unwrap_or(0);

            Ok(MasterContractInfo {
                broker_id: data.broker_id,
                symbol_count: data.symbol_count,
                last_updated: data.last_updated,
                created_at: data.created_at,
                cache_age_seconds: cache_age,
            })
        }
        None => Err(format!("No master contract found for {}", broker_id)),
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

fn normalize_angelone_exchange(exch_seg: &str, instrument_type: Option<&str>) -> String {
    match (exch_seg, instrument_type) {
        ("NSE", Some("AMXIDX")) => "NSE_INDEX".to_string(),
        ("BSE", Some("AMXIDX")) => "BSE_INDEX".to_string(),
        ("MCX", Some("AMXIDX")) => "MCX_INDEX".to_string(),
        _ => exch_seg.to_string(),
    }
}

fn normalize_instrument_type(inst_type: Option<&str>) -> Option<String> {
    match inst_type {
        Some("OPTIDX") | Some("OPTSTK") => {
            // Will be determined by symbol suffix (CE/PE)
            None
        }
        Some("FUTIDX") | Some("FUTSTK") | Some("FUTCOM") | Some("FUTCUR") | Some("FUTIRC") => {
            Some("FUT".to_string())
        }
        Some("OPTFUT") | Some("OPTCUR") | Some("OPTIRC") => None,
        Some(t) => Some(t.to_string()),
        None => None,
    }
}

fn format_angelone_expiry(expiry: &str) -> String {
    // Convert from '19MAR2024' to '19-MAR-24'
    if expiry.len() >= 9 {
        let day = &expiry[0..2];
        let month = &expiry[2..5];
        let year = &expiry[7..9];
        format!("{}-{}-{}", day, month.to_uppercase(), year)
    } else {
        expiry.to_uppercase()
    }
}

fn map_fyers_exchange(exchange_code: Option<i32>) -> String {
    match exchange_code {
        Some(10) => "NSE".to_string(),
        Some(11) => "MCX".to_string(),
        Some(12) => "BSE".to_string(),
        _ => "NSE".to_string(),
    }
}

fn determine_fyers_instrument_type(symbol: &str, option_type: Option<&str>) -> Option<String> {
    if let Some(opt) = option_type {
        if opt == "CE" || opt == "PE" {
            return Some(opt.to_string());
        }
    }

    if symbol.ends_with("FUT") {
        Some("FUT".to_string())
    } else if symbol.ends_with("CE") {
        Some("CE".to_string())
    } else if symbol.ends_with("PE") {
        Some("PE".to_string())
    } else if symbol.contains("-EQ") || !symbol.contains('-') {
        Some("EQ".to_string())
    } else {
        None
    }
}

fn extract_fyers_symbol(symbol_ticker: &str) -> String {
    // Fyers symbols are like "NSE:RELIANCE-EQ" or "NSE:NIFTY24JAN25000CE"
    if let Some(pos) = symbol_ticker.find(':') {
        let after_colon = &symbol_ticker[pos + 1..];
        // Remove -EQ suffix for equity
        after_colon.replace("-EQ", "").replace("-BE", "")
    } else {
        symbol_ticker.replace("-EQ", "").replace("-BE", "")
    }
}
