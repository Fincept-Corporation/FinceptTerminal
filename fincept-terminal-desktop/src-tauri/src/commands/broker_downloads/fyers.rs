//! Fyers Master Contract Download
//!
//! Downloads and parses Fyers symbol master from public CSVs.
//! Transforms to unified symbol format following OpenAlgo pattern.

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "fyers";

/// Fyers CSV URLs
const FYERS_CSV_URLS: &[(&str, &str)] = &[
    ("NSE_CM", "https://public.fyers.in/sym_details/NSE_CM.csv"),
    ("BSE_CM", "https://public.fyers.in/sym_details/BSE_CM.csv"),
    ("NSE_FO", "https://public.fyers.in/sym_details/NSE_FO.csv"),
    ("BSE_FO", "https://public.fyers.in/sym_details/BSE_FO.csv"),
    ("NSE_CD", "https://public.fyers.in/sym_details/NSE_CD.csv"),
    ("MCX_COM", "https://public.fyers.in/sym_details/MCX_COM.csv"),
];

/// Fyers CSV column indices (0-based)
/// Headers: Fytoken, Symbol Details, Exchange Instrument type, Minimum lot size,
/// Tick size, ISIN, Trading Session, Last update date, Expiry date, Symbol ticker,
/// Exchange, Segment, Scrip code, Underlying symbol, Underlying scrip code,
/// Strike price, Option type, Underlying FyToken, Reserved1, Reserved2, Reserved3
const COL_FYTOKEN: usize = 0;
const COL_SYMBOL_DETAILS: usize = 1;
const COL_EXCHANGE_INST_TYPE: usize = 2;
const COL_LOT_SIZE: usize = 3;
const COL_TICK_SIZE: usize = 4;
const COL_EXPIRY: usize = 8;
const COL_SYMBOL_TICKER: usize = 9;
const COL_UNDERLYING: usize = 13;
const COL_STRIKE: usize = 15;
const COL_OPTION_TYPE: usize = 16;

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Fyers master contract
#[command]
pub async fn fyers_download_symbols() -> DownloadResponse {
    match download_fyers_symbols().await {
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

async fn download_fyers_symbols() -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Fyers symbol download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(60))
        .build()?;

    let mut all_symbols: Vec<SymbolRecord> = Vec::new();

    for (segment, url) in FYERS_CSV_URLS {
        update_status(
            BROKER_ID,
            DownloadStatus::Downloading,
            &format!("Downloading {}...", segment),
            None,
        )?;

        match download_and_parse_csv(&client, url, segment).await {
            Ok(symbols) => {
                eprintln!(
                    "[fyers] Downloaded {} symbols from {}",
                    symbols.len(),
                    segment
                );
                all_symbols.extend(symbols);
            }
            Err(e) => {
                eprintln!("[fyers] Error downloading {}: {}", segment, e);
                // Continue with other segments
            }
        }
    }

    if all_symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols downloaded from any segment"));
    }

    // Use the unified complete_master_contract_download
    let count = complete_master_contract_download(BROKER_ID, &all_symbols)?;

    Ok(count)
}

async fn download_and_parse_csv(
    client: &reqwest::Client,
    url: &str,
    segment: &str,
) -> Result<Vec<SymbolRecord>> {
    let response = client
        .get(url)
        .send()
        .await
        .context(format!("Failed to fetch {}", url))?;

    let csv_text = response
        .text()
        .await
        .context(format!("Failed to read response from {}", url))?;

    parse_fyers_csv(&csv_text, segment)
}

fn parse_fyers_csv(csv_text: &str, segment: &str) -> Result<Vec<SymbolRecord>> {
    let mut symbols = Vec::new();

    for line in csv_text.lines() {
        if line.trim().is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 17 {
            continue;
        }

        if let Some(symbol) = parse_fyers_row(&fields, segment) {
            symbols.push(symbol);
        }
    }

    Ok(symbols)
}

fn parse_fyers_row(fields: &[&str], segment: &str) -> Option<SymbolRecord> {
    let fytoken = fields.get(COL_FYTOKEN)?.trim();
    let symbol_details = fields.get(COL_SYMBOL_DETAILS)?.trim();
    let exchange_inst_type: i32 = fields.get(COL_EXCHANGE_INST_TYPE)?.trim().parse().ok()?;
    let lot_size: i32 = fields.get(COL_LOT_SIZE)?.trim().parse().unwrap_or(1);
    let tick_size: f64 = fields.get(COL_TICK_SIZE)?.trim().parse().unwrap_or(0.05);
    let expiry_raw = fields.get(COL_EXPIRY)?.trim();
    let symbol_ticker = fields.get(COL_SYMBOL_TICKER)?.trim();
    let underlying = fields.get(COL_UNDERLYING)?.trim();
    let strike: f64 = fields.get(COL_STRIKE)?.trim().parse().unwrap_or(0.0);
    let option_type = fields.get(COL_OPTION_TYPE)?.trim();

    // Determine exchange and instrument type based on segment and exchange_inst_type
    let (exchange, instrument_type) = match segment {
        "NSE_CM" => {
            match exchange_inst_type {
                0 | 9 => ("NSE", "EQ"),
                2 if symbol_ticker.ends_with("-GB") => ("NSE", "GB"),
                10 => ("NSE_INDEX", "INDEX"),
                _ => return None, // Skip unsupported types
            }
        }
        "BSE_CM" => {
            match exchange_inst_type {
                0 | 4 | 50 => ("BSE", "EQ"),
                10 => ("BSE_INDEX", "INDEX"),
                _ => return None,
            }
        }
        "NSE_FO" => ("NFO", get_instrument_type(option_type)),
        "BSE_FO" => ("BFO", get_instrument_type(option_type)),
        "NSE_CD" => ("CDS", get_instrument_type(option_type)),
        "MCX_COM" => ("MCX", get_instrument_type(option_type)),
        _ => return None,
    };

    // Build unified symbol
    let unified_symbol = build_unified_symbol(
        underlying,
        symbol_details,
        expiry_raw,
        strike,
        option_type,
        instrument_type,
    );

    // Format expiry date
    let expiry = format_expiry_timestamp(expiry_raw);

    // Get the underlying name (normalized for indices)
    let name = if instrument_type == "INDEX" || instrument_type == "EQ" {
        normalize_index_symbol(underlying)
    } else {
        get_underlying_from_symbol_details(symbol_details)
    };

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: symbol_ticker.to_string(),
        name: Some(name),
        exchange: exchange.to_string(),
        br_exchange: Some(segment.to_string()),
        token: fytoken.to_string(),
        expiry,
        strike: if strike > 0.0 { Some(strike) } else { None },
        lot_size: Some(lot_size),
        instrument_type: Some(instrument_type.to_string()),
        tick_size: Some(tick_size),
        segment: Some(segment.to_string()),
    })
}

fn get_instrument_type(option_type: &str) -> &'static str {
    match option_type.to_uppercase().as_str() {
        "CE" => "CE",
        "PE" => "PE",
        "XX" | "" => "FUT",
        _ => "FUT",
    }
}

/// Build unified symbol format following OpenAlgo pattern
/// - Equity: RELIANCE
/// - Futures: NIFTY26DEC24FUT
/// - Options: NIFTY26DEC2424000CE
fn build_unified_symbol(
    underlying: &str,
    symbol_details: &str,
    expiry_raw: &str,
    strike: f64,
    _option_type: &str,
    instrument_type: &str,
) -> String {
    let normalized_underlying = normalize_index_symbol(underlying);

    match instrument_type {
        "EQ" | "INDEX" | "GB" => normalized_underlying,
        "FUT" | "CE" | "PE" => {
            // Parse "NIFTY 26 Dec 24 FUT" format
            if let Some(symbol) = reformat_symbol_detail(symbol_details) {
                if instrument_type == "CE" || instrument_type == "PE" {
                    format!("{}{}", symbol, instrument_type)
                } else {
                    symbol
                }
            } else {
                // Fallback: build from parts
                let expiry_formatted = format_expiry_for_symbol(expiry_raw);
                if strike > 0.0 && (instrument_type == "CE" || instrument_type == "PE") {
                    let strike_str = format_strike(strike);
                    format!(
                        "{}{}{}{}",
                        normalized_underlying, expiry_formatted, strike_str, instrument_type
                    )
                } else {
                    format!("{}{}FUT", normalized_underlying, expiry_formatted)
                }
            }
        }
        _ => normalized_underlying,
    }
}

/// Reformat "NIFTY 26 Dec 24 FUT" to "NIFTY26DEC24FUT"
fn reformat_symbol_detail(s: &str) -> Option<String> {
    let parts: Vec<&str> = s.split_whitespace().collect();
    if parts.len() >= 5 {
        // Format: Name DD Mon YY FUT/CE/PE
        Some(format!(
            "{}{}{}{}{}",
            parts[0],
            parts[3].to_uppercase(), // Year (YY)
            parts[2].to_uppercase(), // Month
            parts[1],                // Day
            parts[4].to_uppercase()  // FUT/CE/PE
        ))
    } else if parts.len() == 4 {
        // Some formats might be shorter
        Some(format!(
            "{}{}{}{}",
            parts[0],
            parts[2].to_uppercase(),
            parts[1],
            parts[3].to_uppercase()
        ))
    } else {
        None
    }
}

/// Get underlying name from symbol details (first word)
fn get_underlying_from_symbol_details(s: &str) -> String {
    s.split_whitespace()
        .next()
        .map(|s| normalize_index_symbol(s))
        .unwrap_or_else(|| s.to_string())
}

/// Format Unix timestamp to DD-MMM-YY
fn format_expiry_timestamp(expiry_raw: &str) -> Option<String> {
    if expiry_raw.is_empty() || expiry_raw == "0" {
        return None;
    }

    // Try to parse as Unix timestamp
    if let Ok(timestamp) = expiry_raw.parse::<i64>() {
        if timestamp > 0 {
            use chrono::{TimeZone, Utc};
            if let Some(dt) = Utc.timestamp_opt(timestamp, 0).single() {
                return Some(dt.format("%d-%b-%y").to_string().to_uppercase());
            }
        }
    }

    // Return as-is if not a timestamp
    Some(expiry_raw.to_uppercase())
}

/// Format expiry for symbol construction (DDMMMYY without dashes)
fn format_expiry_for_symbol(expiry_raw: &str) -> String {
    if let Some(formatted) = format_expiry_timestamp(expiry_raw) {
        formatted.replace("-", "")
    } else {
        String::new()
    }
}

/// Format strike price (remove trailing .0)
fn format_strike(strike: f64) -> String {
    if strike.fract() == 0.0 {
        format!("{}", strike as i64)
    } else {
        format!("{}", strike)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_reformat_symbol_detail() {
        assert_eq!(
            reformat_symbol_detail("NIFTY 26 Dec 24 FUT"),
            Some("NIFTY24DEC26FUT".to_string())
        );
        assert_eq!(
            reformat_symbol_detail("BANKNIFTY 25 Dec 24 CE"),
            Some("BANKNIFTY24DEC25CE".to_string())
        );
    }

    #[test]
    fn test_format_strike() {
        assert_eq!(format_strike(24000.0), "24000");
        assert_eq!(format_strike(24000.5), "24000.5");
    }
}
