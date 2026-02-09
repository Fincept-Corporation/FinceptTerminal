//! Dhan Master Contract Download
//!
//! Downloads and parses Dhan instruments from public CSV endpoint.
//! No authentication required for instrument download.
//! Transforms to unified symbol format following OpenAlgo pattern.

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "dhan";

// Dhan CSV endpoints (compact format)
const DHAN_CSV_URLS: &[(&str, &str)] = &[
    ("NSE_EQ", "https://images.dhan.co/api-data/api-scrip-master-NSE_EQ.csv"),
    ("NSE_FNO", "https://images.dhan.co/api-data/api-scrip-master-NSE_FNO.csv"),
    ("NSE_CURRENCY", "https://images.dhan.co/api-data/api-scrip-master-NSE_CURRENCY.csv"),
    ("BSE_EQ", "https://images.dhan.co/api-data/api-scrip-master-BSE_EQ.csv"),
    ("BSE_FNO", "https://images.dhan.co/api-data/api-scrip-master-BSE_FNO.csv"),
    ("BSE_CURRENCY", "https://images.dhan.co/api-data/api-scrip-master-BSE_CURRENCY.csv"),
    ("MCX_COMM", "https://images.dhan.co/api-data/api-scrip-master-MCX_COMM.csv"),
    ("IDX_NSE", "https://images.dhan.co/api-data/api-scrip-master-IDX_NSE.csv"),
    ("IDX_BSE", "https://images.dhan.co/api-data/api-scrip-master-IDX_BSE.csv"),
];

/// Dhan CSV columns (0-based)
/// SEM_EXM_EXCH_ID,SEM_SEGMENT,SEM_SMST_SECURITY_ID,SEM_INSTRUMENT_NAME,SEM_EXCH_INSTRUMENT_TYPE,
/// SEM_TRADING_SYMBOL,SEM_LOT_UNITS,SEM_TICK_SIZE,SEM_EXPIRY_FLAG,SEM_EXPIRY_DATE,SEM_STRIKE_PRICE,
/// SEM_OPTION_TYPE,SEM_CUSTOM_SYMBOL,SEM_ISIN
const COL_EXCHANGE: usize = 0;
const COL_SEGMENT: usize = 1;
const COL_SECURITY_ID: usize = 2;
const COL_INSTRUMENT_NAME: usize = 3;
const COL_INSTRUMENT_TYPE: usize = 4;
const COL_TRADING_SYMBOL: usize = 5;
const COL_LOT_SIZE: usize = 6;
const COL_TICK_SIZE: usize = 7;
const COL_EXPIRY_DATE: usize = 9;
const COL_STRIKE: usize = 10;
const COL_OPTION_TYPE: usize = 11;
const COL_CUSTOM_SYMBOL: usize = 12;

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Dhan master contract
#[command]
pub async fn dhan_download_symbols() -> DownloadResponse {
    match download_dhan_symbols().await {
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

async fn download_dhan_symbols() -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Dhan symbol download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()?;

    let mut all_symbols: Vec<SymbolRecord> = Vec::new();

    for (segment, url) in DHAN_CSV_URLS {
        update_status(
            BROKER_ID,
            DownloadStatus::Downloading,
            &format!("Downloading {}...", segment),
            None,
        )?;

        match download_and_parse_dhan(&client, url, segment).await {
            Ok(symbols) => {
                eprintln!("[dhan] Downloaded {} symbols from {}", symbols.len(), segment);
                all_symbols.extend(symbols);
            }
            Err(e) => {
                eprintln!("[dhan] Error downloading {}: {}", segment, e);
            }
        }
    }

    if all_symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols downloaded from any segment"));
    }

    let count = complete_master_contract_download(BROKER_ID, &all_symbols)?;
    Ok(count)
}

async fn download_and_parse_dhan(
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

    parse_dhan_csv(&csv_text, segment)
}

fn parse_dhan_csv(csv_text: &str, segment: &str) -> Result<Vec<SymbolRecord>> {
    let mut symbols = Vec::new();
    let mut lines = csv_text.lines();

    // Skip header
    if let Some(header) = lines.next() {
        eprintln!("[dhan] CSV header: {}", header);
    }

    for line in lines {
        if line.trim().is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 13 {
            continue;
        }

        if let Some(symbol) = parse_dhan_row(&fields, segment) {
            symbols.push(symbol);
        }
    }

    Ok(symbols)
}

fn parse_dhan_row(fields: &[&str], segment: &str) -> Option<SymbolRecord> {
    let exchange = fields.get(COL_EXCHANGE)?.trim();
    let segment_code = fields.get(COL_SEGMENT)?.trim();
    let security_id = fields.get(COL_SECURITY_ID)?.trim();
    let instrument_name = fields.get(COL_INSTRUMENT_NAME)?.trim();
    let instrument_type = fields.get(COL_INSTRUMENT_TYPE)?.trim();
    let trading_symbol = fields.get(COL_TRADING_SYMBOL)?.trim();
    let lot_size: i32 = fields.get(COL_LOT_SIZE)?.trim().parse().unwrap_or(1);
    let tick_size: f64 = fields.get(COL_TICK_SIZE)?.trim().parse().unwrap_or(0.05);
    let expiry_raw = fields.get(COL_EXPIRY_DATE)?.trim();
    let strike: f64 = fields.get(COL_STRIKE)?.trim().parse().unwrap_or(0.0);
    let option_type = fields.get(COL_OPTION_TYPE)?.trim();
    let custom_symbol = fields.get(COL_CUSTOM_SYMBOL)?.trim();

    // Determine unified exchange
    let unified_exchange = match segment {
        "IDX_NSE" => "NSE_INDEX",
        "IDX_BSE" => "BSE_INDEX",
        "NSE_EQ" => "NSE",
        "BSE_EQ" => "BSE",
        "NSE_FNO" => "NFO",
        "BSE_FNO" => "BFO",
        "NSE_CURRENCY" => "CDS",
        "BSE_CURRENCY" => "BCD",
        "MCX_COMM" => "MCX",
        _ => exchange,
    };

    // Format expiry
    let expiry = format_dhan_expiry(expiry_raw);

    // Determine instrument type
    let inst_type = match option_type {
        "CE" => "CE",
        "PE" => "PE",
        "" if instrument_type.contains("FUT") => "FUT",
        "" if instrument_type.contains("IDX") => "INDEX",
        "" => "EQ",
        _ => option_type,
    };

    // Build unified symbol
    let name = normalize_index_symbol(instrument_name);
    let unified_symbol = build_unified_symbol(&name, &expiry, strike, inst_type);

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: trading_symbol.to_string(),
        name: Some(name),
        exchange: unified_exchange.to_string(),
        br_exchange: Some(exchange.to_string()),
        token: security_id.to_string(),
        expiry,
        strike: if strike > 0.0 { Some(strike) } else { None },
        lot_size: Some(lot_size),
        instrument_type: Some(inst_type.to_string()),
        tick_size: Some(tick_size),
        segment: Some(segment.to_string()),
    })
}

fn format_dhan_expiry(expiry: &str) -> Option<String> {
    if expiry.is_empty() || expiry == "0" {
        return None;
    }

    // Dhan format: YYYY-MM-DD
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
        return Some(format!("{}-{}-{}", day, month_str, year_short));
    }

    Some(expiry.to_uppercase())
}

fn build_unified_symbol(name: &str, expiry: &Option<String>, strike: f64, inst_type: &str) -> String {
    match inst_type {
        "EQ" | "INDEX" => name.to_string(),
        "FUT" => {
            if let Some(exp) = expiry {
                format!("{}{}FUT", name, exp.replace("-", ""))
            } else {
                name.to_string()
            }
        }
        "CE" | "PE" => {
            if let Some(exp) = expiry {
                let strike_str = if strike.fract() == 0.0 {
                    format!("{}", strike as i64)
                } else {
                    format!("{}", strike)
                };
                format!("{}{}{}{}", name, exp.replace("-", ""), strike_str, inst_type)
            } else {
                name.to_string()
            }
        }
        _ => name.to_string(),
    }
}
