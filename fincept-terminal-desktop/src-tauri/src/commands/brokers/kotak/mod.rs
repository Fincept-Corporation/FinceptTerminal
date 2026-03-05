#![allow(dead_code)]
/**
 * Kotak Neo Broker Integration
 *
 * Auth Flow: TOTP + MPIN (2-step)
 * Auth token format: trading_token:::trading_sid:::base_url:::access_token
 */

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use crate::database::pool::get_db;
use rusqlite::params;

// ============================================================================
// CONSTANTS
// ============================================================================

pub(super) const KOTAK_AUTH_BASE: &str = "https://mis.kotaksecurities.com";

// ============================================================================
// RESPONSE TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct KotakAuthResponse {
    pub trading_token: String,
    pub trading_sid: String,
    pub base_url: String,
    pub access_token: String,
    pub auth_string: String,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct KotakOrderResponse {
    pub order_id: Option<String>,
    pub status: String,
    pub message: Option<String>,
}

// ============================================================================
// URL ENCODING UTILITY
// ============================================================================

pub(super) fn url_encode(s: &str) -> String {
    let mut result = String::new();
    for c in s.chars() {
        match c {
            'A'..='Z' | 'a'..='z' | '0'..='9' | '-' | '_' | '.' | '~' => result.push(c),
            _ => {
                for byte in c.to_string().as_bytes() {
                    result.push_str(&format!("%{:02X}", byte));
                }
            }
        }
    }
    result
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/// Make authenticated request to Kotak API
pub(super) async fn kotak_request(
    client: &Client,
    method: &str,
    base_url: &str,
    endpoint: &str,
    trading_token: &str,
    trading_sid: &str,
    payload: Option<String>,
) -> Result<Value, String> {
    let url = format!("{}{}", base_url, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err("Invalid HTTP method".to_string()),
    };

    request = request
        .header("accept", "application/json")
        .header("Sid", trading_sid)
        .header("Auth", trading_token)
        .header("neo-fin-key", "neotradeapi");

    if let Some(body) = payload {
        request = request
            .header("Content-Type", "application/x-www-form-urlencoded")
            .body(body);
    }

    let response = request.send().await.map_err(|e| format!("Request failed: {}", e))?;
    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;
    serde_json::from_str(&text).map_err(|e| format!("Failed to parse JSON: {} - Response: {}", e, text))
}

/// Make quotes request (uses Authorization header instead of Auth/Sid)
pub(super) async fn kotak_quotes_request(
    client: &Client,
    base_url: &str,
    endpoint: &str,
    access_token: &str,
) -> Result<Value, String> {
    let url = format!("{}{}", base_url, endpoint);

    let response = client
        .get(&url)
        .header("Authorization", access_token)
        .header("Content-Type", "application/json")
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let text = response.text().await.map_err(|e| format!("Failed to read response: {}", e))?;
    serde_json::from_str(&text).map_err(|e| format!("Failed to parse JSON: {} - Response: {}", e, text))
}

/// Parse auth token string into components
pub(super) fn parse_auth_token(auth_token: &str) -> Result<(String, String, String, String), String> {
    let parts: Vec<&str> = auth_token.split(":::").collect();
    if parts.len() != 4 {
        return Err("Invalid auth token format. Expected: trading_token:::trading_sid:::base_url:::access_token".to_string());
    }
    Ok((parts[0].to_string(), parts[1].to_string(), parts[2].to_string(), parts[3].to_string()))
}

/// URL encode JSON for Kotak API payload format: jData={encoded_json}
pub(super) fn encode_jdata(data: &Value) -> String {
    let json_str = serde_json::to_string(data).unwrap_or_default();
    format!("jData={}", url_encode(&json_str))
}

/// Map internal exchange to Kotak exchange format
pub(super) fn map_exchange_to_kotak(exchange: &str) -> &str {
    match exchange {
        "NSE" => "nse_cm",
        "BSE" => "bse_cm",
        "NFO" => "nse_fo",
        "BFO" => "bse_fo",
        "CDS" => "cde_fo",
        "BCD" => "bcs_fo",
        "MCX" => "mcx_fo",
        _ => exchange,
    }
}

/// Map Kotak exchange format to internal
pub(super) fn map_exchange_from_kotak(exchange: &str) -> &str {
    match exchange {
        "nse_cm" => "NSE",
        "bse_cm" => "BSE",
        "nse_fo" => "NFO",
        "bse_fo" => "BFO",
        "cde_fo" => "CDS",
        "bcs_fo" => "BCD",
        "mcx_fo" => "MCX",
        _ => exchange,
    }
}

/// Map internal order type to Kotak format
pub(super) fn map_order_type_to_kotak(order_type: &str) -> &str {
    match order_type {
        "MARKET" => "MKT",
        "LIMIT" => "L",
        "SL" | "STOP" | "STOP_LOSS" => "SL",
        "SL-M" | "STOP_LOSS_MARKET" => "SL-M",
        _ => "MKT",
    }
}

// ============================================================================
// DATABASE HELPER FUNCTIONS
// ============================================================================

/// Get broker symbol from database
pub(super) fn get_kotak_br_symbol(symbol: &str, exchange: &str) -> Option<String> {
    let db = get_db().ok()?;
    db.query_row(
        "SELECT brsymbol FROM kotak_master_contract WHERE symbol = ?1 AND exchange = ?2",
        params![symbol, exchange],
        |row| row.get(0),
    ).ok()
}

/// Get token (pSymbol) from database
pub(super) fn get_kotak_token(symbol: &str, exchange: &str) -> Option<String> {
    let db = get_db().ok()?;
    db.query_row(
        "SELECT token FROM kotak_master_contract WHERE symbol = ?1 AND exchange = ?2",
        params![symbol, exchange],
        |row| row.get(0),
    ).ok()
}

/// Store master contract data in SQLite
pub(super) fn store_kotak_master_contract(csv_data: &str, exchange: &str) -> Result<usize, String> {
    let db = get_db().map_err(|e| format!("Failed to get database: {}", e))?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS kotak_master_contract (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            brsymbol TEXT NOT NULL,
            token TEXT NOT NULL,
            exchange TEXT NOT NULL,
            brexchange TEXT NOT NULL,
            name TEXT,
            expiry TEXT,
            strike REAL,
            lotsize INTEGER,
            instrumenttype TEXT,
            tick_size REAL,
            UNIQUE(token, exchange)
        )",
        [],
    ).map_err(|e| format!("Failed to create table: {}", e))?;

    let mut count = 0;
    let mut lines = csv_data.lines();
    let _header = lines.next();

    for line in lines {
        let fields: Vec<&str> = line.split(',').collect();
        if fields.len() < 8 { continue; }

        let token = fields.get(0).unwrap_or(&"").trim();
        let brsymbol = fields.get(1).unwrap_or(&"").trim();
        let symbol = fields.get(2).unwrap_or(&"").trim();
        let name = fields.get(3).unwrap_or(&"").trim();

        if token.is_empty() || symbol.is_empty() { continue; }

        let brexchange = map_exchange_to_kotak(exchange);

        let result = db.execute(
            "INSERT OR REPLACE INTO kotak_master_contract
             (symbol, brsymbol, token, exchange, brexchange, name, expiry, strike, lotsize, instrumenttype, tick_size)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)",
            params![symbol, brsymbol, token, exchange, brexchange, name, "", 0.0_f64, 1_i32, "EQ", 0.05_f64],
        );

        if result.is_ok() { count += 1; }
    }

    let now = chrono::Utc::now().to_rfc3339();
    db.execute(
        "INSERT OR REPLACE INTO broker_metadata (broker, key, value) VALUES ('kotak', 'master_contract_updated', ?1)",
        params![now],
    ).ok();

    Ok(count)
}
