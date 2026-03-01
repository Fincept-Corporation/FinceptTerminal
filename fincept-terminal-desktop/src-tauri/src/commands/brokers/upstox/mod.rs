// commands/brokers/upstox/mod.rs - Upstox broker integration

#![allow(dead_code)]

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde::{Deserialize, Serialize};
use crate::database::pool::get_db;

// Upstox API Configuration
pub(super) const UPSTOX_API_BASE_V2: &str = "https://api.upstox.com/v2";
pub(super) const UPSTOX_API_BASE_V3: &str = "https://api.upstox.com/v3";

pub(super) fn create_upstox_headers(access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {}", access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

/// URL encode a string (simple implementation)
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

/// Upstox symbol data structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UpstoxSymbol {
    pub instrument_key: String,
    pub trading_symbol: String,
    pub name: String,
    pub exchange: String,
    pub segment: String,
    pub instrument_type: String,
    pub lot_size: i32,
    pub tick_size: f64,
    pub expiry: Option<String>,
    pub strike: Option<f64>,
}

/// Initialize Upstox symbols table in the shared database
pub(super) fn init_upstox_symbols_table() -> Result<(), String> {
    let db = get_db().map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS upstox_symbols (
            id INTEGER PRIMARY KEY,
            instrument_key TEXT NOT NULL UNIQUE,
            trading_symbol TEXT NOT NULL,
            name TEXT,
            exchange TEXT NOT NULL,
            segment TEXT NOT NULL,
            instrument_type TEXT,
            lot_size INTEGER DEFAULT 1,
            tick_size REAL DEFAULT 0.05,
            expiry TEXT,
            strike REAL
        )",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_upstox_trading_symbol ON upstox_symbols(trading_symbol)",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_upstox_exchange ON upstox_symbols(exchange)",
        [],
    ).map_err(|e| e.to_string())?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS upstox_metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        )",
        [],
    ).map_err(|e| e.to_string())?;

    Ok(())
}

pub(super) fn search_upstox_symbols(keyword: &str, exchange: Option<&str>, limit: i32) -> Result<Vec<UpstoxSymbol>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let query = if let Some(exch) = exchange {
        format!(
            "SELECT instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike
             FROM upstox_symbols WHERE trading_symbol LIKE '%{}%' AND exchange = '{}' LIMIT {}",
            keyword, exch, limit
        )
    } else {
        format!(
            "SELECT instrument_key, trading_symbol, name, exchange, segment, instrument_type, lot_size, tick_size, expiry, strike
             FROM upstox_symbols WHERE trading_symbol LIKE '%{}%' LIMIT {}",
            keyword, limit
        )
    };

    let mut stmt = db.prepare(&query).map_err(|e| e.to_string())?;
    let rows = stmt.query_map([], |row| {
        Ok(UpstoxSymbol {
            instrument_key: row.get(0)?,
            trading_symbol: row.get(1)?,
            name: row.get(2)?,
            exchange: row.get(3)?,
            segment: row.get(4)?,
            instrument_type: row.get(5)?,
            lot_size: row.get(6)?,
            tick_size: row.get(7)?,
            expiry: row.get(8)?,
            strike: row.get(9)?,
        })
    }).map_err(|e| e.to_string())?;

    let mut results = Vec::new();
    for row in rows {
        if let Ok(symbol) = row {
            results.push(symbol);
        }
    }

    Ok(results)
}

pub(super) fn get_upstox_instrument_key(symbol: &str, exchange: &str) -> Result<Option<String>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let result: Result<String, _> = db.query_row(
        "SELECT instrument_key FROM upstox_symbols WHERE trading_symbol = ?1 AND exchange = ?2",
        rusqlite::params![symbol, exchange],
        |row| row.get(0),
    );

    match result {
        Ok(key) => Ok(Some(key)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.to_string()),
    }
}

pub(super) fn get_upstox_metadata() -> Result<Option<(i64, i64)>, String> {
    init_upstox_symbols_table()?;
    let db = get_db().map_err(|e| e.to_string())?;

    let last_updated: Result<String, _> = db.query_row(
        "SELECT value FROM upstox_metadata WHERE key = 'last_updated'",
        [],
        |row| row.get(0),
    );

    let count: Result<String, _> = db.query_row(
        "SELECT value FROM upstox_metadata WHERE key = 'symbol_count'",
        [],
        |row| row.get(0),
    );

    match (last_updated, count) {
        (Ok(ts), Ok(cnt)) => {
            let timestamp = ts.parse::<i64>().unwrap_or(0);
            let symbol_count = cnt.parse::<i64>().unwrap_or(0);
            Ok(Some((timestamp, symbol_count)))
        }
        _ => Ok(None),
    }
}
