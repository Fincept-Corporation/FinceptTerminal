// commands/brokers/dhan/mod.rs - Dhan broker integration

#![allow(dead_code)]

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use serde::{Deserialize, Serialize};
use serde_json::Value;
use crate::database::pool::get_db;

// ============================================================================
// CONSTANTS
// ============================================================================

pub(super) const DHAN_API_BASE: &str = "https://api.dhan.co";
pub(super) const DHAN_AUTH_BASE: &str = "https://auth.dhan.co";
pub(super) const DHAN_MASTER_CONTRACT_URL: &str = "https://images.dhan.co/api-data/api-scrip-master.csv";

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
    pub timestamp: i64,
}

#[derive(Debug, Serialize, Deserialize)]
pub(super) struct DhanOrderParams {
    #[serde(rename = "dhanClientId")]
    pub dhan_client_id: String,
    #[serde(rename = "transactionType")]
    pub transaction_type: String,
    #[serde(rename = "exchangeSegment")]
    pub exchange_segment: String,
    #[serde(rename = "productType")]
    pub product_type: String,
    #[serde(rename = "orderType")]
    pub order_type: String,
    pub validity: String,
    #[serde(rename = "securityId")]
    pub security_id: String,
    pub quantity: i32,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "triggerPrice")]
    pub trigger_price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "disclosedQuantity")]
    pub disclosed_quantity: Option<i32>,
}

#[derive(Debug, Serialize, Deserialize)]
pub(super) struct DhanModifyParams {
    #[serde(rename = "dhanClientId")]
    pub dhan_client_id: String,
    #[serde(rename = "orderId")]
    pub order_id: String,
    #[serde(rename = "orderType")]
    pub order_type: String,
    #[serde(rename = "legName")]
    pub leg_name: String,
    pub quantity: i32,
    pub validity: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    #[serde(rename = "triggerPrice")]
    pub trigger_price: Option<f64>,
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

pub(super) fn get_timestamp() -> i64 {
    chrono::Utc::now().timestamp()
}

pub(super) async fn dhan_request(
    endpoint: &str,
    method: &str,
    access_token: &str,
    client_id: &str,
    body: Option<Value>,
) -> Result<Value, String> {
    let client = reqwest::Client::new();
    let url = format!("{}{}", DHAN_API_BASE, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err(format!("Unsupported HTTP method: {}", method)),
    };

    request = request
        .header("access-token", access_token)
        .header("client-id", client_id)
        .header("Content-Type", "application/json")
        .header("Accept", "application/json");

    if let Some(body_value) = body {
        request = request.json(&body_value);
    }

    let response = request.send().await.map_err(|e| e.to_string())?;
    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if !status.is_success() {
        return Err(format!("HTTP {}: {}", status, text));
    }

    serde_json::from_str(&text).map_err(|e| format!("JSON parse error: {} - {}", e, text))
}

pub(super) fn map_exchange_to_dhan(exchange: &str) -> &str {
    match exchange {
        "NSE" => "NSE_EQ",
        "BSE" => "BSE_EQ",
        "NFO" => "NSE_FNO",
        "BFO" => "BSE_FNO",
        "MCX" => "MCX_COMM",
        "CDS" => "NSE_CURRENCY",
        "BCD" => "BSE_CURRENCY",
        "NSE_INDEX" => "IDX_I",
        "BSE_INDEX" => "IDX_I",
        _ => exchange,
    }
}

pub(super) fn map_exchange_from_dhan(exchange: &str) -> &str {
    match exchange {
        "NSE_EQ" => "NSE",
        "BSE_EQ" => "BSE",
        "NSE_FNO" => "NFO",
        "BSE_FNO" => "BFO",
        "MCX_COMM" => "MCX",
        "NSE_CURRENCY" => "CDS",
        "BSE_CURRENCY" => "BCD",
        "IDX_I" => "NSE_INDEX",
        _ => exchange,
    }
}

pub(super) fn map_product_to_dhan(product: &str) -> &str {
    match product {
        "CNC" | "CASH" => "CNC",
        "MIS" | "INTRADAY" => "INTRADAY",
        "NRML" | "MARGIN" => "MARGIN",
        _ => "INTRADAY",
    }
}

pub(super) fn map_order_type_to_dhan(order_type: &str) -> &str {
    match order_type {
        "MARKET" => "MARKET",
        "LIMIT" => "LIMIT",
        "SL" | "STOP" | "STOP_LOSS" => "STOP_LOSS",
        "SL-M" | "STOP_LOSS_MARKET" => "STOP_LOSS_MARKET",
        _ => "MARKET",
    }
}

/// Initialize Dhan symbols table
pub(super) fn init_dhan_symbols_table() -> Result<(), String> {
    let db = get_db().map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS dhan_symbols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            name TEXT,
            exchange TEXT NOT NULL,
            br_exchange TEXT,
            security_id TEXT NOT NULL,
            instrument_type TEXT,
            lot_size INTEGER DEFAULT 1,
            tick_size REAL DEFAULT 0.05,
            expiry TEXT,
            strike REAL
        )",
        [],
    )
    .map_err(|e| e.to_string())?;

    // Create metadata table
    db.execute(
        "CREATE TABLE IF NOT EXISTS dhan_metadata (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        )",
        [],
    )
    .map_err(|e| e.to_string())?;

    // Create indexes for faster search
    let _ = db.execute(
        "CREATE INDEX IF NOT EXISTS idx_dhan_symbol ON dhan_symbols(symbol)",
        [],
    );
    let _ = db.execute(
        "CREATE INDEX IF NOT EXISTS idx_dhan_exchange ON dhan_symbols(exchange)",
        [],
    );

    Ok(())
}
