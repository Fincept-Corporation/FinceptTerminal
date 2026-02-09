//! Zerodha (Kite) Master Contract Download
//!
//! Downloads and parses Zerodha instruments from the Kite API.
//! Transforms to unified symbol format following OpenAlgo pattern.
//! Note: Requires authentication token (API key + access token).

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "zerodha";
const INSTRUMENTS_URL: &str = "https://api.kite.trade/instruments";

/// Zerodha instrument CSV columns
/// instrument_token,exchange_token,tradingsymbol,name,last_price,expiry,strike,tick_size,lot_size,instrument_type,segment,exchange
const COL_INSTRUMENT_TOKEN: usize = 0;
const COL_EXCHANGE_TOKEN: usize = 1;
const COL_TRADINGSYMBOL: usize = 2;
const COL_NAME: usize = 3;
const COL_EXPIRY: usize = 5;
const COL_STRIKE: usize = 6;
const COL_TICK_SIZE: usize = 7;
const COL_LOT_SIZE: usize = 8;
const COL_INSTRUMENT_TYPE: usize = 9;
const COL_SEGMENT: usize = 10;
const COL_EXCHANGE: usize = 11;

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Zerodha master contract
/// Requires API key and access token for authentication
#[command]
pub async fn zerodha_download_symbols(
    api_key: String,
    access_token: String,
) -> DownloadResponse {
    match download_zerodha_symbols(&api_key, &access_token).await {
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

async fn download_zerodha_symbols(api_key: &str, access_token: &str) -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Zerodha instrument download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()?;

    // Build authorization header: "token api_key:access_token"
    let auth_header = format!("token {}:{}", api_key, access_token);

    let response = client
        .get(INSTRUMENTS_URL)
        .header("X-Kite-Version", "3")
        .header("Authorization", &auth_header)
        .send()
        .await
        .context("Failed to fetch Zerodha instruments")?;

    if !response.status().is_success() {
        return Err(anyhow::anyhow!(
            "Zerodha API returned status: {}",
            response.status()
        ));
    }

    let csv_text = response
        .text()
        .await
        .context("Failed to read Zerodha response")?;

    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Parsing instruments...",
        None,
    )?;

    let symbols = parse_zerodha_csv(&csv_text)?;

    if symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols parsed from Zerodha response"));
    }

    eprintln!("[zerodha] Parsed {} symbols", symbols.len());

    // Use the unified complete_master_contract_download
    let count = complete_master_contract_download(BROKER_ID, &symbols)?;

    Ok(count)
}

fn parse_zerodha_csv(csv_text: &str) -> Result<Vec<SymbolRecord>> {
    let mut symbols = Vec::new();
    let mut lines = csv_text.lines();

    // Skip header line
    if let Some(header) = lines.next() {
        eprintln!("[zerodha] CSV header: {}", header);
    }

    for line in lines {
        if line.trim().is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 12 {
            continue;
        }

        if let Some(symbol) = parse_zerodha_row(&fields) {
            symbols.push(symbol);
        }
    }

    Ok(symbols)
}

fn parse_zerodha_row(fields: &[&str]) -> Option<SymbolRecord> {
    let instrument_token = fields.get(COL_INSTRUMENT_TOKEN)?.trim();
    let exchange_token = fields.get(COL_EXCHANGE_TOKEN)?.trim();
    let trading_symbol = fields.get(COL_TRADINGSYMBOL)?.trim();
    let name = fields.get(COL_NAME)?.trim();
    let expiry_raw = fields.get(COL_EXPIRY)?.trim();
    let strike: f64 = fields.get(COL_STRIKE)?.trim().parse().unwrap_or(0.0);
    let tick_size: f64 = fields.get(COL_TICK_SIZE)?.trim().parse().unwrap_or(0.05);
    let lot_size: i32 = fields.get(COL_LOT_SIZE)?.trim().parse().unwrap_or(1);
    let instrument_type = fields.get(COL_INSTRUMENT_TYPE)?.trim();
    let segment = fields.get(COL_SEGMENT)?.trim();
    let exchange = fields.get(COL_EXCHANGE)?.trim();

    // Combine instrument_token and exchange_token for unique ID
    let token = format!("{}::::{}", instrument_token, exchange_token);

    // Determine exchange (handle indices)
    let unified_exchange = if segment == "INDICES" {
        match exchange {
            "NSE" => "NSE_INDEX",
            "BSE" => "BSE_INDEX",
            "MCX" => "MCX_INDEX",
            "CDS" => "CDS_INDEX",
            _ => exchange,
        }
    } else {
        exchange
    };

    // Format expiry date to DD-MMM-YY
    let expiry = format_expiry(expiry_raw);

    // Build unified symbol
    let unified_symbol = build_unified_symbol(name, &expiry, strike, instrument_type);

    // Normalize name for indices
    let normalized_name = normalize_index_symbol(name);

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: trading_symbol.to_string(),
        name: Some(normalized_name),
        exchange: unified_exchange.to_string(),
        br_exchange: Some(exchange.to_string()),
        token,
        expiry,
        strike: if strike > 0.0 { Some(strike) } else { None },
        lot_size: Some(lot_size),
        instrument_type: Some(instrument_type.to_string()),
        tick_size: Some(tick_size),
        segment: Some(segment.to_string()),
    })
}

/// Build unified symbol format following OpenAlgo pattern
fn build_unified_symbol(name: &str, expiry: &Option<String>, strike: f64, instrument_type: &str) -> String {
    let normalized_name = normalize_index_symbol(name);

    match instrument_type {
        "EQ" | "INDEX" => normalized_name,
        "FUT" => {
            if let Some(exp) = expiry {
                let exp_formatted = exp.replace("-", "");
                format!("{}{}FUT", normalized_name, exp_formatted)
            } else {
                normalized_name
            }
        }
        "CE" | "PE" => {
            if let Some(exp) = expiry {
                let exp_formatted = exp.replace("-", "");
                let strike_str = format_strike(strike);
                format!("{}{}{}{}", normalized_name, exp_formatted, strike_str, instrument_type)
            } else {
                normalized_name
            }
        }
        _ => normalized_name,
    }
}

/// Format expiry date from YYYY-MM-DD to DD-MMM-YY
fn format_expiry(expiry_raw: &str) -> Option<String> {
    if expiry_raw.is_empty() {
        return None;
    }

    // Expected format: YYYY-MM-DD
    let parts: Vec<&str> = expiry_raw.split('-').collect();
    if parts.len() == 3 {
        let year = parts[0];
        let month: u32 = parts[1].parse().unwrap_or(1);
        let day = parts[2];

        let month_str = match month {
            1 => "JAN",
            2 => "FEB",
            3 => "MAR",
            4 => "APR",
            5 => "MAY",
            6 => "JUN",
            7 => "JUL",
            8 => "AUG",
            9 => "SEP",
            10 => "OCT",
            11 => "NOV",
            12 => "DEC",
            _ => "JAN",
        };

        // Get last 2 digits of year
        let year_short = if year.len() >= 2 {
            &year[year.len() - 2..]
        } else {
            year
        };

        return Some(format!("{}-{}-{}", day, month_str, year_short));
    }

    // Return as-is if can't parse
    Some(expiry_raw.to_uppercase())
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
    fn test_format_expiry() {
        assert_eq!(format_expiry("2024-12-26"), Some("26-DEC-24".to_string()));
        assert_eq!(format_expiry("2025-01-30"), Some("30-JAN-25".to_string()));
        assert_eq!(format_expiry(""), None);
    }

    #[test]
    fn test_build_unified_symbol() {
        assert_eq!(
            build_unified_symbol("NIFTY", &Some("26-DEC-24".to_string()), 0.0, "FUT"),
            "NIFTY26DEC24FUT"
        );
        assert_eq!(
            build_unified_symbol("NIFTY", &Some("26-DEC-24".to_string()), 24000.0, "CE"),
            "NIFTY26DEC2424000CE"
        );
    }
}
