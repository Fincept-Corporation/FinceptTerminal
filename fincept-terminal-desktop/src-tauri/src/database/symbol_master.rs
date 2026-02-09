//! Unified Symbol Master Database
//!
//! This module provides a unified symbol database that works across all brokers.
//! It follows the OpenAlgo pattern with standardized symbol formats and unified storage.
//!
//! Key Features:
//! - Single `symbols` table for all brokers (with broker_id column)
//! - Standardized symbol format (e.g., NIFTY26DEC24FUT)
//! - Status tracking for master contract downloads
//! - TTL-based cache validation
//! - Unified search across exchanges

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

// ============================================================================
// CONSTANTS
// ============================================================================

/// Cache TTL in seconds (24 hours)
pub const CACHE_TTL_SECONDS: i64 = 86400;

/// Index symbol mappings (broker format -> unified format)
pub const INDEX_SYMBOL_MAPPINGS: &[(&str, &str)] = &[
    ("NIFTY 50", "NIFTY"),
    ("Nifty 50", "NIFTY"),
    ("NIFTY50", "NIFTY"),
    ("NIFTY NEXT 50", "NIFTYNXT50"),
    ("Nifty Next 50", "NIFTYNXT50"),
    ("NIFTY FIN SERVICE", "FINNIFTY"),
    ("Nifty Fin Service", "FINNIFTY"),
    ("FINNIFTY", "FINNIFTY"),
    ("NIFTY BANK", "BANKNIFTY"),
    ("Nifty Bank", "BANKNIFTY"),
    ("BANKNIFTY", "BANKNIFTY"),
    ("NIFTY MID SELECT", "MIDCPNIFTY"),
    ("NIFTY MIDCAP SELECT", "MIDCPNIFTY"),
    ("MIDCPNIFTY", "MIDCPNIFTY"),
    ("INDIA VIX", "INDIAVIX"),
    ("India VIX", "INDIAVIX"),
    ("SENSEX", "SENSEX"),
    ("SENSEX50", "SENSEX50"),
    ("SNSX50", "SENSEX50"),
    ("BANKEX", "BANKEX"),
];

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/// Unified symbol record matching OpenAlgo's SymToken structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SymbolRecord {
    pub id: Option<i64>,
    /// Broker identifier (e.g., "fyers", "zerodha", "angel")
    pub broker_id: String,
    /// Unified symbol format (e.g., "RELIANCE", "NIFTY26DEC24FUT", "NIFTY26DEC2424000CE")
    pub symbol: String,
    /// Broker-specific symbol (e.g., "NSE:RELIANCE-EQ", "RELIANCE-EQ")
    pub br_symbol: String,
    /// Company/underlying name (e.g., "Reliance Industries Ltd", "NIFTY")
    pub name: Option<String>,
    /// Unified exchange (NSE, BSE, NFO, BFO, CDS, MCX, NSE_INDEX, BSE_INDEX)
    pub exchange: String,
    /// Broker-specific exchange code
    pub br_exchange: Option<String>,
    /// Broker token/instrument ID
    pub token: String,
    /// Expiry date in DD-MMM-YY format (e.g., "26-DEC-24")
    pub expiry: Option<String>,
    /// Strike price for options
    pub strike: Option<f64>,
    /// Lot size
    pub lot_size: Option<i32>,
    /// Instrument type (EQ, FUT, CE, PE, INDEX)
    pub instrument_type: Option<String>,
    /// Tick size
    pub tick_size: Option<f64>,
    /// Segment (CM, FO, CD, etc.)
    pub segment: Option<String>,
}

/// Master contract download status
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum DownloadStatus {
    Pending,
    Downloading,
    Success,
    Error,
}

impl ToString for DownloadStatus {
    fn to_string(&self) -> String {
        match self {
            DownloadStatus::Pending => "pending".to_string(),
            DownloadStatus::Downloading => "downloading".to_string(),
            DownloadStatus::Success => "success".to_string(),
            DownloadStatus::Error => "error".to_string(),
        }
    }
}

impl From<&str> for DownloadStatus {
    fn from(s: &str) -> Self {
        match s.to_lowercase().as_str() {
            "pending" => DownloadStatus::Pending,
            "downloading" => DownloadStatus::Downloading,
            "success" => DownloadStatus::Success,
            "error" => DownloadStatus::Error,
            _ => DownloadStatus::Pending,
        }
    }
}

/// Master contract status record
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MasterContractStatus {
    pub broker_id: String,
    pub status: String,
    pub message: Option<String>,
    pub last_updated: i64,
    pub total_symbols: i64,
    pub is_ready: bool,
}

/// Symbol search result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SymbolSearchResult {
    pub symbol: String,
    pub br_symbol: String,
    pub name: Option<String>,
    pub exchange: String,
    pub token: String,
    pub expiry: Option<String>,
    pub strike: Option<f64>,
    pub lot_size: Option<i32>,
    pub instrument_type: Option<String>,
    pub tick_size: Option<f64>,
    pub relevance_score: f64,
}

// ============================================================================
// DATABASE INITIALIZATION
// ============================================================================

/// Initialize the master contract status table
pub fn init_master_contract_status_table() -> Result<()> {
    let db = get_db()?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS master_contract_status (
            broker_id TEXT PRIMARY KEY,
            status TEXT NOT NULL DEFAULT 'pending',
            message TEXT,
            last_updated INTEGER NOT NULL,
            total_symbols INTEGER NOT NULL DEFAULT 0,
            is_ready INTEGER NOT NULL DEFAULT 0
        )",
        [],
    )
    .context("Failed to create master_contract_status table")?;

    Ok(())
}

// ============================================================================
// STATUS TRACKING
// ============================================================================

/// Initialize broker status when they login
pub fn init_broker_status(broker_id: &str) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO master_contract_status
         (broker_id, status, message, last_updated, total_symbols, is_ready)
         VALUES (?, 'pending', 'Master contract download pending', ?, 0, 0)",
        params![broker_id, now],
    )
    .context("Failed to initialize broker status")?;

    Ok(())
}

/// Update download status for a broker
pub fn update_status(
    broker_id: &str,
    status: DownloadStatus,
    message: &str,
    total_symbols: Option<i64>,
) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let is_ready = status == DownloadStatus::Success;

    db.execute(
        "INSERT OR REPLACE INTO master_contract_status
         (broker_id, status, message, last_updated, total_symbols, is_ready)
         VALUES (?, ?, ?, ?, COALESCE(?, (SELECT total_symbols FROM master_contract_status WHERE broker_id = ?), 0), ?)",
        params![
            broker_id,
            status.to_string(),
            message,
            now,
            total_symbols,
            broker_id,
            is_ready as i32
        ],
    )
    .context("Failed to update status")?;

    Ok(())
}

/// Get current status for a broker
pub fn get_status(broker_id: &str) -> Result<MasterContractStatus> {
    let db = get_db()?;

    let result = db
        .query_row(
            "SELECT broker_id, status, message, last_updated, total_symbols, is_ready
             FROM master_contract_status WHERE broker_id = ?",
            params![broker_id],
            |row| {
                Ok(MasterContractStatus {
                    broker_id: row.get(0)?,
                    status: row.get(1)?,
                    message: row.get(2)?,
                    last_updated: row.get(3)?,
                    total_symbols: row.get(4)?,
                    is_ready: row.get::<_, i32>(5)? == 1,
                })
            },
        )
        .optional()
        .context("Failed to get status")?;

    Ok(result.unwrap_or(MasterContractStatus {
        broker_id: broker_id.to_string(),
        status: "unknown".to_string(),
        message: Some("No status available".to_string()),
        last_updated: 0,
        total_symbols: 0,
        is_ready: false,
    }))
}

/// Check if master contracts are ready for a broker
pub fn is_ready(broker_id: &str) -> Result<bool> {
    let db = get_db()?;

    let is_ready: Option<i32> = db
        .query_row(
            "SELECT is_ready FROM master_contract_status WHERE broker_id = ?",
            params![broker_id],
            |row| row.get(0),
        )
        .optional()?;

    Ok(is_ready.unwrap_or(0) == 1)
}

// ============================================================================
// SYMBOL OPERATIONS
// ============================================================================

/// Delete all symbols for a broker
pub fn delete_symbols_for_broker(broker_id: &str) -> Result<i64> {
    let db = get_db()?;
    let deleted = db
        .execute("DELETE FROM symbols WHERE broker_id = ?", params![broker_id])
        .context("Failed to delete symbols")?;

    Ok(deleted as i64)
}

/// Bulk insert symbols using a transaction for performance
pub fn bulk_insert_symbols(symbols: &[SymbolRecord]) -> Result<i64> {
    if symbols.is_empty() {
        return Ok(0);
    }

    let mut db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let tx = db.transaction()?;

    {
        let mut stmt = tx.prepare_cached(
            "INSERT INTO symbols (broker_id, symbol, br_symbol, name, exchange, br_exchange, token, expiry, strike, lot_size, instrument_type, tick_size, segment, created_at)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
        )?;

        for symbol in symbols {
            stmt.execute(params![
                symbol.broker_id,
                symbol.symbol,
                symbol.br_symbol,
                symbol.name,
                symbol.exchange,
                symbol.br_exchange,
                symbol.token,
                symbol.expiry,
                symbol.strike,
                symbol.lot_size,
                symbol.instrument_type,
                symbol.tick_size,
                symbol.segment,
                now,
            ])?;
        }
    }

    tx.commit()?;

    Ok(symbols.len() as i64)
}

/// Search symbols by query string with relevance scoring
pub fn search_symbols(
    broker_id: &str,
    query: &str,
    exchange: Option<&str>,
    instrument_type: Option<&str>,
    limit: i32,
) -> Result<Vec<SymbolSearchResult>> {
    let db = get_db()?;

    let search_pattern = format!("%{}%", query.to_uppercase());
    let query_upper = query.to_uppercase();

    // Build dynamic query based on filters
    let mut sql = String::from(
        "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size,
                CASE
                    WHEN UPPER(symbol) = ? THEN 100.0
                    WHEN UPPER(symbol) LIKE ? || '%' THEN 90.0
                    WHEN UPPER(name) = ? THEN 85.0
                    WHEN UPPER(name) LIKE ? || '%' THEN 80.0
                    WHEN UPPER(symbol) LIKE ? THEN 70.0
                    WHEN UPPER(name) LIKE ? THEN 60.0
                    ELSE 50.0
                END as relevance_score
         FROM symbols
         WHERE broker_id = ? AND (
             UPPER(symbol) LIKE ? OR
             UPPER(name) LIKE ? OR
             UPPER(br_symbol) LIKE ? OR
             token LIKE ?
         )",
    );

    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = vec![
        Box::new(query_upper.clone()),
        Box::new(query_upper.clone()),
        Box::new(query_upper.clone()),
        Box::new(query_upper.clone()),
        Box::new(search_pattern.clone()),
        Box::new(search_pattern.clone()),
        Box::new(broker_id.to_string()),
        Box::new(search_pattern.clone()),
        Box::new(search_pattern.clone()),
        Box::new(search_pattern.clone()),
        Box::new(search_pattern.clone()),
    ];

    if let Some(exch) = exchange {
        sql.push_str(" AND exchange = ?");
        params_vec.push(Box::new(exch.to_string()));
    }

    if let Some(inst_type) = instrument_type {
        sql.push_str(" AND instrument_type = ?");
        params_vec.push(Box::new(inst_type.to_string()));
    }

    sql.push_str(" ORDER BY relevance_score DESC, symbol ASC LIMIT ?");
    params_vec.push(Box::new(limit));

    let mut stmt = db.prepare(&sql)?;
    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();

    let results = stmt.query_map(params_refs.as_slice(), |row| {
        Ok(SymbolSearchResult {
            symbol: row.get(0)?,
            br_symbol: row.get(1)?,
            name: row.get(2)?,
            exchange: row.get(3)?,
            token: row.get(4)?,
            expiry: row.get(5)?,
            strike: row.get(6)?,
            lot_size: row.get(7)?,
            instrument_type: row.get(8)?,
            tick_size: row.get(9)?,
            relevance_score: row.get(10)?,
        })
    })?;

    let mut symbols = Vec::new();
    for result in results {
        symbols.push(result?);
    }

    Ok(symbols)
}

/// Get symbol by exact match
pub fn get_symbol(broker_id: &str, symbol: &str, exchange: &str) -> Result<Option<SymbolSearchResult>> {
    let db = get_db()?;

    let result = db
        .query_row(
            "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size
             FROM symbols
             WHERE broker_id = ? AND UPPER(symbol) = UPPER(?) AND exchange = ?",
            params![broker_id, symbol, exchange],
            |row| {
                Ok(SymbolSearchResult {
                    symbol: row.get(0)?,
                    br_symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    token: row.get(4)?,
                    expiry: row.get(5)?,
                    strike: row.get(6)?,
                    lot_size: row.get(7)?,
                    instrument_type: row.get(8)?,
                    tick_size: row.get(9)?,
                    relevance_score: 100.0,
                })
            },
        )
        .optional()?;

    Ok(result)
}

/// Get symbol by token
pub fn get_symbol_by_token(broker_id: &str, token: &str) -> Result<Option<SymbolSearchResult>> {
    let db = get_db()?;

    let result = db
        .query_row(
            "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size
             FROM symbols
             WHERE broker_id = ? AND token = ?",
            params![broker_id, token],
            |row| {
                Ok(SymbolSearchResult {
                    symbol: row.get(0)?,
                    br_symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    token: row.get(4)?,
                    expiry: row.get(5)?,
                    strike: row.get(6)?,
                    lot_size: row.get(7)?,
                    instrument_type: row.get(8)?,
                    tick_size: row.get(9)?,
                    relevance_score: 100.0,
                })
            },
        )
        .optional()?;

    Ok(result)
}

/// Get token by symbol (for WebSocket subscriptions)
pub fn get_token_by_symbol(broker_id: &str, symbol: &str, exchange: &str) -> Result<Option<String>> {
    let db = get_db()?;

    let token: Option<String> = db
        .query_row(
            "SELECT token FROM symbols WHERE broker_id = ? AND UPPER(symbol) = UPPER(?) AND exchange = ? LIMIT 1",
            params![broker_id, symbol, exchange],
            |row| row.get(0),
        )
        .optional()?;

    Ok(token)
}

/// Get symbol count for a broker
pub fn get_symbol_count(broker_id: &str) -> Result<i64> {
    let db = get_db()?;

    let count: i64 = db.query_row(
        "SELECT COUNT(*) FROM symbols WHERE broker_id = ?",
        params![broker_id],
        |row| row.get(0),
    )?;

    Ok(count)
}

/// Get distinct exchanges for a broker
pub fn get_exchanges(broker_id: &str) -> Result<Vec<String>> {
    let db = get_db()?;

    let mut stmt =
        db.prepare("SELECT DISTINCT exchange FROM symbols WHERE broker_id = ? ORDER BY exchange")?;

    let results = stmt.query_map(params![broker_id], |row| row.get(0))?;

    let mut exchanges = Vec::new();
    for result in results {
        exchanges.push(result?);
    }

    Ok(exchanges)
}

/// Get distinct expiries for FNO symbols
pub fn get_expiries(
    broker_id: &str,
    exchange: Option<&str>,
    underlying: Option<&str>,
) -> Result<Vec<String>> {
    let db = get_db()?;

    let mut sql = String::from(
        "SELECT DISTINCT expiry FROM symbols WHERE broker_id = ? AND expiry IS NOT NULL AND expiry != ''",
    );

    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = vec![Box::new(broker_id.to_string())];

    if let Some(exch) = exchange {
        sql.push_str(" AND exchange = ?");
        params_vec.push(Box::new(exch.to_string()));
    }

    if let Some(name) = underlying {
        sql.push_str(" AND UPPER(name) = UPPER(?)");
        params_vec.push(Box::new(name.to_string()));
    }

    sql.push_str(" ORDER BY expiry");

    let mut stmt = db.prepare(&sql)?;
    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();

    let results = stmt.query_map(params_refs.as_slice(), |row| row.get(0))?;

    let mut expiries = Vec::new();
    for result in results {
        expiries.push(result?);
    }

    Ok(expiries)
}

/// Get distinct underlyings (for option chain)
pub fn get_underlyings(broker_id: &str, exchange: Option<&str>) -> Result<Vec<String>> {
    let db = get_db()?;

    let mut sql = String::from(
        "SELECT DISTINCT name FROM symbols WHERE broker_id = ? AND name IS NOT NULL AND name != ''",
    );

    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = vec![Box::new(broker_id.to_string())];

    if let Some(exch) = exchange {
        sql.push_str(" AND exchange = ?");
        params_vec.push(Box::new(exch.to_string()));
    }

    sql.push_str(" ORDER BY name");

    let mut stmt = db.prepare(&sql)?;
    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();

    let results = stmt.query_map(params_refs.as_slice(), |row| row.get(0))?;

    let mut underlyings = Vec::new();
    for result in results {
        underlyings.push(result?);
    }

    Ok(underlyings)
}

// ============================================================================
// CACHE VALIDATION
// ============================================================================

/// Check if cache is valid (not expired based on TTL)
pub fn is_cache_valid(broker_id: &str, ttl_seconds: i64) -> Result<bool> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let last_updated: Option<i64> = db
        .query_row(
            "SELECT last_updated FROM master_contract_status WHERE broker_id = ? AND is_ready = 1",
            params![broker_id],
            |row| row.get(0),
        )
        .optional()?;

    match last_updated {
        Some(timestamp) => Ok((now - timestamp) < ttl_seconds),
        None => Ok(false),
    }
}

/// Get cache age in seconds
pub fn get_cache_age(broker_id: &str) -> Result<Option<i64>> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let last_updated: Option<i64> = db
        .query_row(
            "SELECT last_updated FROM master_contract_status WHERE broker_id = ?",
            params![broker_id],
            |row| row.get(0),
        )
        .optional()?;

    Ok(last_updated.map(|ts| now - ts))
}

// ============================================================================
// SYMBOL FORMAT UTILITIES
// ============================================================================

/// Normalize index symbol to unified format
pub fn normalize_index_symbol(symbol: &str) -> String {
    let symbol_upper = symbol.to_uppercase();

    for (from, to) in INDEX_SYMBOL_MAPPINGS {
        if symbol_upper == from.to_uppercase() {
            return to.to_string();
        }
    }

    symbol.to_string()
}

/// Format expiry date to DD-MMM-YY format
pub fn format_expiry(expiry: &str) -> String {
    // Already in correct format
    if expiry.len() == 9 && expiry.chars().nth(2) == Some('-') && expiry.chars().nth(6) == Some('-') {
        return expiry.to_uppercase();
    }

    // Try to parse various formats
    // Format: DDMMMYY (e.g., 26DEC24)
    if expiry.len() == 7 {
        let day = &expiry[0..2];
        let month = &expiry[2..5];
        let year = &expiry[5..7];
        return format!("{}-{}-{}", day, month.to_uppercase(), year);
    }

    // Format: DDMMMYYYY (e.g., 26DEC2024)
    if expiry.len() == 9 && expiry.chars().all(|c| c.is_alphanumeric()) {
        let day = &expiry[0..2];
        let month = &expiry[2..5];
        let year = &expiry[7..9];
        return format!("{}-{}-{}", day, month.to_uppercase(), year);
    }

    // Return as-is if can't parse
    expiry.to_uppercase()
}

/// Build unified futures symbol
/// Format: UNDERLYING + EXPIRY + FUT (e.g., NIFTY26DEC24FUT)
pub fn build_futures_symbol(underlying: &str, expiry: &str) -> String {
    let normalized = normalize_index_symbol(underlying);
    let formatted_expiry = format_expiry(expiry).replace("-", "");
    format!("{}{}FUT", normalized, formatted_expiry)
}

/// Build unified options symbol
/// Format: UNDERLYING + EXPIRY + STRIKE + TYPE (e.g., NIFTY26DEC2424000CE)
pub fn build_options_symbol(underlying: &str, expiry: &str, strike: f64, option_type: &str) -> String {
    let normalized = normalize_index_symbol(underlying);
    let formatted_expiry = format_expiry(expiry).replace("-", "");
    let strike_str = if strike.fract() == 0.0 {
        format!("{}", strike as i64)
    } else {
        format!("{}", strike)
    };
    format!("{}{}{}{}", normalized, formatted_expiry, strike_str, option_type.to_uppercase())
}

/// Format strike price (remove trailing .0)
pub fn format_strike(strike: f64) -> String {
    if strike.fract() == 0.0 {
        format!("{}", strike as i64)
    } else {
        format!("{}", strike)
    }
}

// ============================================================================
// MASTER CONTRACT DOWNLOAD HELPER
// ============================================================================

/// Complete master contract download workflow
/// 1. Set status to downloading
/// 2. Delete old symbols
/// 3. Insert new symbols
/// 4. Update status to success/error
pub fn complete_master_contract_download(
    broker_id: &str,
    symbols: &[SymbolRecord],
) -> Result<i64> {
    // Update status to downloading
    update_status(broker_id, DownloadStatus::Downloading, "Downloading master contract...", None)?;

    // Delete old symbols
    let deleted = delete_symbols_for_broker(broker_id)?;
    eprintln!("[symbol_master] Deleted {} old symbols for {}", deleted, broker_id);

    // Insert new symbols
    let inserted = match bulk_insert_symbols(symbols) {
        Ok(count) => {
            update_status(
                broker_id,
                DownloadStatus::Success,
                &format!("Successfully loaded {} symbols", count),
                Some(count),
            )?;
            count
        }
        Err(e) => {
            update_status(
                broker_id,
                DownloadStatus::Error,
                &format!("Failed to insert symbols: {}", e),
                None,
            )?;
            return Err(e);
        }
    };

    eprintln!("[symbol_master] Inserted {} symbols for {}", inserted, broker_id);
    Ok(inserted)
}
