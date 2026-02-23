//! Angel One (SmartAPI) Master Contract Download
//!
//! Downloads and parses Angel One instruments from public JSON endpoint.
//! No authentication required for master contract download.
//! Transforms to unified symbol format following OpenAlgo pattern.

use crate::database::symbol_master::{
    complete_master_contract_download, normalize_index_symbol, update_status, DownloadStatus,
    SymbolRecord,
};
use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};
use tauri::command;

const BROKER_ID: &str = "angelone";
const INSTRUMENTS_URL: &str = "https://margincalculator.angelbroking.com/OpenAPI_File/files/OpenAPIScripMaster.json";

/// Angel One JSON instrument structure
#[derive(Debug, Deserialize)]
struct AngelInstrument {
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

#[derive(Debug, Serialize, Deserialize)]
pub struct DownloadResponse {
    pub success: bool,
    pub message: String,
    pub total_symbols: i64,
}

/// Download and process Angel One master contract
/// No authentication required
#[command]
pub async fn angelone_download_symbols() -> DownloadResponse {
    match download_angelone_symbols().await {
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

async fn download_angelone_symbols() -> Result<i64> {
    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Starting Angel One symbol download...",
        None,
    )?;

    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(120))
        .build()?;

    let response = client
        .get(INSTRUMENTS_URL)
        .send()
        .await
        .context("Failed to fetch Angel One instruments")?;

    if !response.status().is_success() {
        return Err(anyhow::anyhow!(
            "Angel One API returned status: {}",
            response.status()
        ));
    }

    update_status(
        BROKER_ID,
        DownloadStatus::Downloading,
        "Parsing instruments...",
        None,
    )?;

    let instruments: Vec<AngelInstrument> = response
        .json()
        .await
        .context("Failed to parse Angel One JSON")?;

    eprintln!("[angelone] Downloaded {} raw instruments", instruments.len());

    let symbols: Vec<SymbolRecord> = instruments
        .into_iter()
        .filter_map(|inst| convert_angel_instrument(inst))
        .collect();

    if symbols.is_empty() {
        return Err(anyhow::anyhow!("No symbols parsed from Angel One response"));
    }

    eprintln!("[angelone] Parsed {} symbols", symbols.len());

    // Use the unified complete_master_contract_download
    let count = complete_master_contract_download(BROKER_ID, &symbols)?;

    Ok(count)
}

fn convert_angel_instrument(inst: AngelInstrument) -> Option<SymbolRecord> {
    let exchange = &inst.exch_seg;
    let instrument_type = inst.instrumenttype.as_deref().unwrap_or("");

    // Determine unified exchange (handle indices)
    let unified_exchange = if instrument_type == "AMXIDX" {
        match exchange.as_str() {
            "NSE" => "NSE_INDEX",
            "BSE" => "BSE_INDEX",
            "MCX" => "MCX_INDEX",
            _ => exchange.as_str(),
        }
    } else {
        exchange.as_str()
    };

    // Parse strike price (Angel sends strike * 100)
    let strike: f64 = inst
        .strike
        .as_ref()
        .and_then(|s| s.parse::<f64>().ok())
        .unwrap_or(0.0)
        / 100.0;

    // Additional strike adjustment for CDS options
    let strike = if (instrument_type == "OPTCUR" || instrument_type == "OPTIRC")
        && exchange == "CDS"
    {
        strike / 1000.0
    } else {
        strike
    };

    // Parse lot size
    let lot_size: i32 = inst
        .lotsize
        .as_ref()
        .and_then(|s| s.parse().ok())
        .unwrap_or(1);

    // Parse tick size (Angel sends tick_size * 100)
    let tick_size: f64 = inst
        .tick_size
        .as_ref()
        .and_then(|s| s.parse::<f64>().ok())
        .unwrap_or(5.0)
        / 100.0;

    // Format expiry date from 19MAR2024 to 19-MAR-24
    let expiry = inst.expiry.as_ref().map(|e| format_angel_expiry(e));

    // Clean broker symbol (remove -EQ, -BE, -MF, -SG suffixes)
    let clean_symbol = inst
        .symbol
        .replace("-EQ", "")
        .replace("-BE", "")
        .replace("-MF", "")
        .replace("-SG", "");

    // Build unified symbol
    let unified_symbol = build_unified_symbol(
        &inst.name,
        &clean_symbol,
        &expiry,
        strike,
        instrument_type,
        exchange,
    );

    // Normalize instrument type to standard format (CE, PE, FUT, EQ)
    let normalized_inst_type = normalize_instrument_type(instrument_type, &unified_symbol);

    // Normalize name for indices
    let normalized_name = normalize_index_symbol(&inst.name);

    Some(SymbolRecord {
        id: None,
        broker_id: BROKER_ID.to_string(),
        symbol: unified_symbol,
        br_symbol: inst.symbol.clone(),
        name: Some(normalized_name),
        exchange: unified_exchange.to_string(),
        br_exchange: Some(exchange.to_string()),
        token: inst.token,
        expiry,
        strike: if strike > 0.0 { Some(strike) } else { None },
        lot_size: Some(lot_size),
        instrument_type: Some(normalized_inst_type),
        tick_size: Some(tick_size),
        segment: Some(exchange.to_string()),
    })
}

/// Format Angel expiry from 19MAR2024 to 19-MAR-24
fn format_angel_expiry(expiry: &str) -> String {
    if expiry.len() >= 9 {
        // Format: DDMMMYYYY (e.g., 19MAR2024)
        let day = &expiry[0..2];
        let month = &expiry[2..5];
        let year = &expiry[7..9]; // Last 2 digits

        format!("{}-{}-{}", day, month.to_uppercase(), year)
    } else if expiry.len() == 7 {
        // Format: DDMMMYY (e.g., 19MAR24)
        let day = &expiry[0..2];
        let month = &expiry[2..5];
        let year = &expiry[5..7];

        format!("{}-{}-{}", day, month.to_uppercase(), year)
    } else {
        expiry.to_uppercase()
    }
}

/// Build unified symbol format
fn build_unified_symbol(
    name: &str,
    symbol: &str,
    expiry: &Option<String>,
    strike: f64,
    instrument_type: &str,
    _exchange: &str,
) -> String {
    let normalized_name = normalize_index_symbol(name);

    match instrument_type {
        // Equity
        "" | "AMXIDX" => normalized_name,

        // Futures
        "FUTIDX" | "FUTSTK" | "FUTCOM" | "FUTCUR" | "FUTIRC" | "FUTIRT" => {
            if let Some(exp) = expiry {
                let exp_formatted = exp.replace("-", "");
                format!("{}{}FUT", normalized_name, exp_formatted)
            } else {
                normalized_name
            }
        }

        // Options
        "OPTIDX" | "OPTSTK" | "OPTFUT" | "OPTCUR" | "OPTIRC" => {
            if let Some(exp) = expiry {
                let exp_formatted = exp.replace("-", "");
                let strike_str = format_strike(strike);
                // Get option type from symbol suffix
                let opt_type = if symbol.ends_with("CE") {
                    "CE"
                } else if symbol.ends_with("PE") {
                    "PE"
                } else {
                    return normalized_name;
                };
                format!(
                    "{}{}{}{}",
                    normalized_name, exp_formatted, strike_str, opt_type
                )
            } else {
                normalized_name
            }
        }

        _ => normalized_name,
    }
}

/// Normalize instrument type to standard format (CE, PE, FUT, EQ)
fn normalize_instrument_type(instrument_type: &str, symbol: &str) -> String {
    match instrument_type {
        // Futures
        "FUTIDX" | "FUTSTK" | "FUTCOM" | "FUTCUR" | "FUTIRC" | "FUTIRT" => "FUT".to_string(),

        // Options - determine CE/PE from symbol
        "OPTIDX" | "OPTSTK" | "OPTFUT" | "OPTCUR" | "OPTIRC" => {
            if symbol.ends_with("CE") {
                "CE".to_string()
            } else if symbol.ends_with("PE") {
                "PE".to_string()
            } else {
                instrument_type.to_string()
            }
        }

        // Index
        "AMXIDX" => "INDEX".to_string(),

        // Equity or other
        "" => "EQ".to_string(),

        _ => instrument_type.to_string(),
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
    fn test_format_angel_expiry() {
        assert_eq!(format_angel_expiry("19MAR2024"), "19-MAR-24");
        assert_eq!(format_angel_expiry("26DEC2024"), "26-DEC-24");
        assert_eq!(format_angel_expiry("19MAR24"), "19-MAR-24");
    }

    #[test]
    fn test_format_strike() {
        assert_eq!(format_strike(24000.0), "24000");
        assert_eq!(format_strike(24000.5), "24000.5");
    }
}
