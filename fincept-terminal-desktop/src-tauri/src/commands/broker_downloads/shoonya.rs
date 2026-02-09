//! Shoonya (Finvasia) Master Contract Download
//!
//! Downloads and parses Shoonya instruments from public endpoints.
//! Transforms to unified symbol format following OpenAlgo pattern.

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "shoonya";

// Shoonya uses the same format as Flattrade (both use NorenOMS)
// Symbol master endpoints
const SHOONYA_URLS: &[(&str, &str)] = &[
    ("NSE", "https://shoonya.finvasia.com/NSE_symbols.txt.zip"),
    ("NFO", "https://shoonya.finvasia.com/NFO_symbols.txt.zip"),
    ("BSE", "https://shoonya.finvasia.com/BSE_symbols.txt.zip"),
    ("BFO", "https://shoonya.finvasia.com/BFO_symbols.txt.zip"),
    ("CDS", "https://shoonya.finvasia.com/CDS_symbols.txt.zip"),
    ("MCX", "https://shoonya.finvasia.com/MCX_symbols.txt.zip"),
];

/// Shoonya symbol file columns (pipe-separated)
/// Exchange,Token,LotSize,Symbol,TradingSymbol,Expiry,Instrument,OptionType,StrikePrice,TickSize
const COL_EXCHANGE: usize = 0;
const COL_TOKEN: usize = 1;
const COL_LOT_SIZE: usize = 2;
const COL_SYMBOL: usize = 3;
const COL_TRADING_SYMBOL: usize = 4;
const COL_EXPIRY: usize = 5;
const COL_INSTRUMENT: usize = 6;
const COL_OPTION_TYPE: usize = 7;
const COL_STRIKE: usize = 8;
const COL_TICK_SIZE: usize = 9;

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Shoonya master contract
#[command]
pub async fn shoonya_download_symbols() -> DownloadResponse {
    match download_shoonya_symbols().await {
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

async fn download_shoonya_symbols() -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Shoonya symbol download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()?;

    let mut all_symbols: Vec<SymbolRecord> = Vec::new();

    for (segment, url) in SHOONYA_URLS {
        update_status(
            BROKER_ID,
            DownloadStatus::Downloading,
            &format!("Downloading {}...", segment),
            None,
        )?;

        match download_and_parse_shoonya(&client, url, segment).await {
            Ok(symbols) => {
                eprintln!("[shoonya] Downloaded {} symbols from {}", symbols.len(), segment);
                all_symbols.extend(symbols);
            }
            Err(e) => {
                eprintln!("[shoonya] Error downloading {}: {}", segment, e);
            }
        }
    }

    if all_symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols downloaded from any segment"));
    }

    let count = complete_master_contract_download(BROKER_ID, &all_symbols)?;
    Ok(count)
}

async fn download_and_parse_shoonya(
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

    // Decompress ZIP
    let cursor = std::io::Cursor::new(&bytes);
    let mut archive = zip::ZipArchive::new(cursor).context("Failed to read ZIP archive")?;

    let mut text_content = String::new();
    for i in 0..archive.len() {
        let mut file = archive.by_index(i)?;
        if file.name().ends_with(".txt") {
            std::io::Read::read_to_string(&mut file, &mut text_content)?;
            break;
        }
    }

    parse_shoonya_text(&text_content, segment)
}

fn parse_shoonya_text(text: &str, segment: &str) -> Result<Vec<SymbolRecord>> {
    let mut symbols = Vec::new();

    for line in text.lines() {
        if line.trim().is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 10 {
            continue;
        }

        if let Some(symbol) = parse_shoonya_row(&fields, segment) {
            symbols.push(symbol);
        }
    }

    Ok(symbols)
}

fn parse_shoonya_row(fields: &[&str], segment: &str) -> Option<SymbolRecord> {
    let exchange = fields.get(COL_EXCHANGE)?.trim();
    let token = fields.get(COL_TOKEN)?.trim();
    let lot_size: i32 = fields.get(COL_LOT_SIZE)?.trim().parse().unwrap_or(1);
    let symbol_name = fields.get(COL_SYMBOL)?.trim();
    let trading_symbol = fields.get(COL_TRADING_SYMBOL)?.trim();
    let expiry_raw = fields.get(COL_EXPIRY)?.trim();
    let instrument = fields.get(COL_INSTRUMENT)?.trim();
    let option_type = fields.get(COL_OPTION_TYPE)?.trim();
    let strike: f64 = fields.get(COL_STRIKE)?.trim().parse().unwrap_or(0.0);
    let tick_size: f64 = fields.get(COL_TICK_SIZE)?.trim().parse().unwrap_or(0.05);

    // Determine unified exchange
    let unified_exchange = match segment {
        "NSE" if instrument == "INDEX" => "NSE_INDEX",
        "BSE" if instrument == "INDEX" => "BSE_INDEX",
        "NFO" => "NFO",
        "BFO" => "BFO",
        "CDS" => "CDS",
        "MCX" => "MCX",
        _ => segment,
    };

    // Format expiry
    let expiry = format_shoonya_expiry(expiry_raw);

    // Determine instrument type
    let inst_type = match option_type {
        "CE" => "CE",
        "PE" => "PE",
        "" if instrument.contains("FUT") => "FUT",
        "" if instrument == "INDEX" => "INDEX",
        "" => "EQ",
        _ => option_type,
    };

    // Build unified symbol
    let name = normalize_index_symbol(symbol_name);
    let unified_symbol = build_unified_symbol(&name, &expiry, strike, inst_type);

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: trading_symbol.to_string(),
        name: Some(name),
        exchange: unified_exchange.to_string(),
        br_exchange: Some(exchange.to_string()),
        token: token.to_string(),
        expiry,
        strike: if strike > 0.0 { Some(strike) } else { None },
        lot_size: Some(lot_size),
        instrument_type: Some(inst_type.to_string()),
        tick_size: Some(tick_size),
        segment: Some(segment.to_string()),
    })
}

fn format_shoonya_expiry(expiry: &str) -> Option<String> {
    if expiry.is_empty() || expiry == "0" {
        return None;
    }

    // Shoonya format: DD-MMM-YYYY (e.g., 26-DEC-2024)
    let parts: Vec<&str> = expiry.split('-').collect();
    if parts.len() == 3 {
        let day = parts[0];
        let month = parts[1];
        let year = parts[2];

        let year_short = if year.len() >= 2 { &year[year.len() - 2..] } else { year };
        return Some(format!("{}-{}-{}", day, month.to_uppercase(), year_short));
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
