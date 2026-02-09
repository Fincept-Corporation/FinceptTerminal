//! Upstox Master Contract Download
//!
//! Downloads and parses Upstox instruments.
//! Transforms to unified symbol format following OpenAlgo pattern.

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "upstox";

// Upstox instrument files (gzipped JSON)
const UPSTOX_URLS: &[(&str, &str)] = &[
    ("NSE_EQ", "https://assets.upstox.com/market-quote/instruments/exchange/NSE.json.gz"),
    ("BSE_EQ", "https://assets.upstox.com/market-quote/instruments/exchange/BSE.json.gz"),
    ("NFO", "https://assets.upstox.com/market-quote/instruments/exchange/NFO.json.gz"),
    ("BFO", "https://assets.upstox.com/market-quote/instruments/exchange/BFO.json.gz"),
    ("CDS", "https://assets.upstox.com/market-quote/instruments/exchange/CDS.json.gz"),
    ("MCX", "https://assets.upstox.com/market-quote/instruments/exchange/MCX.json.gz"),
];

/// Upstox JSON instrument structure
#[derive(Debug, Deserialize)]
struct UpstoxInstrument {
    instrument_key: String,
    trading_symbol: String,
    name: Option<String>,
    expiry: Option<String>,
    strike_price: Option<f64>,
    lot_size: Option<i32>,
    instrument_type: Option<String>,
    exchange: String,
    tick_size: Option<f64>,
    #[serde(default)]
    underlying_symbol: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Upstox master contract
/// No authentication required for instrument files
#[command]
pub async fn upstox_download_symbols() -> DownloadResponse {
    match download_upstox_symbols().await {
        Ok(count) => DownloadResponse {
            success: true,
            message: format!("Successfully downloaded {} symbols", count),
            total_symbols: count,
        },
        Err(e) => {
            let _ = update_status(
                BROKER_ID,
                DownloadStatus::Error,
                &format!("Download failed: {}", e),
                None,
            );
            DownloadResponse {
                success: false,
                message: format!("Download failed: {}", e),
                total_symbols: 0,
            }
        }
    }
}

async fn download_upstox_symbols() -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Upstox symbol download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()?;

    let mut all_symbols: Vec<SymbolRecord> = Vec::new();

    for (segment, url) in UPSTOX_URLS {
        update_status(
            BROKER_ID,
            DownloadStatus::Downloading,
            &format!("Downloading {}...", segment),
            None,
        )?;

        match download_and_parse_upstox(&client, url, segment).await {
            Ok(symbols) => {
                eprintln!("[upstox] Downloaded {} symbols from {}", symbols.len(), segment);
                all_symbols.extend(symbols);
            }
            Err(e) => {
                eprintln!("[upstox] Error downloading {}: {}", segment, e);
                // Continue with other segments
            }
        }
    }

    if all_symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols downloaded from any segment"));
    }

    let count = complete_master_contract_download(BROKER_ID, &all_symbols)?;
    Ok(count)
}

async fn download_and_parse_upstox(
    client: &reqwest::Client,
    url: &str,
    segment: &str,
) -> Result<Vec<SymbolRecord>> {
    let response = client
        .get(url)
        .send()
        .await
        .context(format!("Failed to fetch {}", url))?;

    let bytes = response
        .bytes()
        .await
        .context(format!("Failed to read response from {}", url))?;

    // Decompress gzip
    let mut decoder = flate2::read::GzDecoder::new(&bytes[..]);
    let mut json_text = String::new();
    std::io::Read::read_to_string(&mut decoder, &mut json_text)
        .context("Failed to decompress gzip")?;

    // Parse JSON
    let instruments: Vec<UpstoxInstrument> =
        serde_json::from_str(&json_text).context("Failed to parse Upstox JSON")?;

    let symbols: Vec<SymbolRecord> = instruments
        .into_iter()
        .filter_map(|inst| convert_upstox_instrument(inst, segment))
        .collect();

    Ok(symbols)
}

fn convert_upstox_instrument(inst: UpstoxInstrument, segment: &str) -> Option<SymbolRecord> {
    let instrument_type = inst.instrument_type.as_deref().unwrap_or("");

    // Determine exchange
    let exchange = match segment {
        "NSE_EQ" => {
            if instrument_type == "INDEX" {
                "NSE_INDEX"
            } else {
                "NSE"
            }
        }
        "BSE_EQ" => {
            if instrument_type == "INDEX" {
                "BSE_INDEX"
            } else {
                "BSE"
            }
        }
        _ => &inst.exchange,
    };

    // Format expiry
    let expiry = inst.expiry.as_ref().map(|e| format_upstox_expiry(e));

    // Get underlying name
    let name = inst
        .underlying_symbol
        .as_ref()
        .or(inst.name.as_ref())
        .map(|s| normalize_index_symbol(s))
        .unwrap_or_else(|| inst.trading_symbol.clone());

    // Build unified symbol
    let unified_symbol = build_unified_symbol(
        &name,
        &expiry,
        inst.strike_price.unwrap_or(0.0),
        instrument_type,
    );

    // Normalize instrument type
    let normalized_inst_type = normalize_instrument_type(instrument_type, &inst.trading_symbol);

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: inst.trading_symbol,
        name: Some(name),
        exchange: exchange.to_string(),
        br_exchange: Some(inst.exchange),
        token: inst.instrument_key,
        expiry,
        strike: inst.strike_price.filter(|&s| s > 0.0),
        lot_size: inst.lot_size,
        instrument_type: Some(normalized_inst_type),
        tick_size: inst.tick_size,
        segment: Some(segment.to_string()),
    })
}

fn format_upstox_expiry(expiry: &str) -> String {
    // Upstox format: YYYY-MM-DD
    let parts: Vec<&str> = expiry.split('-').collect();
    if parts.len() == 3 {
        let year = parts[0];
        let month: u32 = parts[1].parse().unwrap_or(1);
        let day = parts[2];

        let month_str = match month {
            1 => "JAN", 2 => "FEB", 3 => "MAR", 4 => "APR",
            5 => "MAY", 6 => "JUN", 7 => "JUL", 8 => "AUG",
            9 => "SEP", 10 => "OCT", 11 => "NOV", 12 => "DEC",
            _ => "JAN",
        };

        let year_short = if year.len() >= 2 { &year[year.len() - 2..] } else { year };
        format!("{}-{}-{}", day, month_str, year_short)
    } else {
        expiry.to_uppercase()
    }
}

fn build_unified_symbol(name: &str, expiry: &Option<String>, strike: f64, instrument_type: &str) -> String {
    match instrument_type {
        "EQ" | "INDEX" => name.to_string(),
        "FUT" | "FUTIDX" | "FUTSTK" => {
            if let Some(exp) = expiry {
                format!("{}{}FUT", name, exp.replace("-", ""))
            } else {
                name.to_string()
            }
        }
        "CE" | "PE" | "OPTIDX" | "OPTSTK" => {
            if let Some(exp) = expiry {
                let strike_str = if strike.fract() == 0.0 {
                    format!("{}", strike as i64)
                } else {
                    format!("{}", strike)
                };
                let opt_type = if instrument_type.contains("CE") || instrument_type == "CE" { "CE" } else { "PE" };
                format!("{}{}{}{}", name, exp.replace("-", ""), strike_str, opt_type)
            } else {
                name.to_string()
            }
        }
        _ => name.to_string(),
    }
}

fn normalize_instrument_type(instrument_type: &str, symbol: &str) -> String {
    match instrument_type {
        "FUTIDX" | "FUTSTK" | "FUT" => "FUT".to_string(),
        "OPTIDX" | "OPTSTK" => {
            if symbol.ends_with("CE") { "CE".to_string() }
            else if symbol.ends_with("PE") { "PE".to_string() }
            else { instrument_type.to_string() }
        }
        "CE" | "PE" => instrument_type.to_string(),
        "INDEX" => "INDEX".to_string(),
        "EQ" | "" => "EQ".to_string(),
        _ => instrument_type.to_string(),
    }
}
