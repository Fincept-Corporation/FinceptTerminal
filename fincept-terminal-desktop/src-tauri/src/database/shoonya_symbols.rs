// Shoonya Symbol Master Database
// Stores Shoonya symbols with token mapping for fast lookup
// Based on Fyers symbols pattern but adapted for Shoonya's data structure

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/// Shoonya symbol data structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShoonyaSymbol {
    pub symbol: String,        // OpenAlgo-style symbol (e.g., RELIANCE)
    pub br_symbol: String,     // Broker symbol (e.g., RELIANCE-EQ)
    pub name: String,          // Company name
    pub exchange: String,      // NSE, BSE, NFO, MCX, CDS, BFO, NSE_INDEX, BSE_INDEX
    pub br_exchange: String,   // Broker exchange
    pub token: String,         // Shoonya token (string format)
    pub expiry: Option<String>, // Expiry date for derivatives (DD-MMM-YY)
    pub strike: Option<f64>,   // Strike price for options
    pub lot_size: i32,         // Lot size
    pub instrument_type: String, // EQ, INDEX, FUT, CE, PE
    pub tick_size: f64,        // Tick size
}

/// Symbol search result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ShoonyaSymbolSearchResult {
    pub symbol: String,
    pub br_symbol: String,
    pub name: String,
    pub exchange: String,
    pub token: String,
    pub instrument_type: String,
    pub lot_size: i32,
    pub relevance_score: f64,
}

// ============================================================================
// DATABASE SCHEMA
// ============================================================================

/// Initialize Shoonya symbols table
pub fn init_shoonya_symbols_table() -> Result<()> {
    let db = get_db()?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS shoonya_symbols (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            symbol TEXT NOT NULL,
            br_symbol TEXT NOT NULL,
            name TEXT NOT NULL,
            exchange TEXT NOT NULL,
            br_exchange TEXT NOT NULL,
            token TEXT NOT NULL,
            expiry TEXT,
            strike REAL,
            lot_size INTEGER NOT NULL DEFAULT 1,
            instrument_type TEXT NOT NULL,
            tick_size REAL NOT NULL DEFAULT 0.05,
            last_updated INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            UNIQUE(token, exchange)
        )",
        [],
    )
    .context("Failed to create shoonya_symbols table")?;

    // Create indexes for fast lookup
    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_symbol ON shoonya_symbols(symbol)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_br_symbol ON shoonya_symbols(br_symbol)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_exchange ON shoonya_symbols(exchange)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_token_exchange ON shoonya_symbols(token, exchange)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_name ON shoonya_symbols(name)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_shoonya_symbol_exchange ON shoonya_symbols(symbol, exchange)",
        [],
    )?;

    // Create metadata table to track last update
    db.execute(
        "CREATE TABLE IF NOT EXISTS shoonya_symbols_metadata (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            last_updated INTEGER NOT NULL,
            symbol_count INTEGER NOT NULL
        )",
        [],
    )?;

    Ok(())
}

// ============================================================================
// WRITE OPERATIONS
// ============================================================================

/// Save a single Shoonya symbol
pub fn save_symbol(symbol: &ShoonyaSymbol) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO shoonya_symbols
         (symbol, br_symbol, name, exchange, br_exchange, token, expiry, strike, lot_size, instrument_type, tick_size, last_updated, created_at)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                COALESCE((SELECT created_at FROM shoonya_symbols WHERE token = ? AND exchange = ?), ?))",
        params![
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
            now,
            symbol.token,
            symbol.exchange,
            now
        ],
    )
    .context("Failed to save Shoonya symbol")?;

    Ok(())
}

/// Save multiple symbols in a transaction (fast bulk insert)
pub fn save_symbols_bulk(symbols: &[ShoonyaSymbol]) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let tx = db.unchecked_transaction()?;

    {
        let mut stmt = tx.prepare(
            "INSERT OR REPLACE INTO shoonya_symbols
             (symbol, br_symbol, name, exchange, br_exchange, token, expiry, strike, lot_size, instrument_type, tick_size, last_updated, created_at)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                    COALESCE((SELECT created_at FROM shoonya_symbols WHERE token = ? AND exchange = ?), ?))",
        )?;

        for symbol in symbols {
            stmt.execute(params![
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
                now,
                symbol.token,
                symbol.exchange,
                now
            ])?;
        }
    }

    tx.commit()?;

    // Update metadata with accumulated count
    let total_count = get_symbol_count().unwrap_or(0);
    update_metadata(total_count)?;

    Ok(())
}

/// Update metadata after bulk insert
pub fn update_metadata(symbol_count: i64) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO shoonya_symbols_metadata (id, last_updated, symbol_count)
         VALUES (1, ?, ?)",
        params![now, symbol_count],
    )?;

    Ok(())
}

// ============================================================================
// READ OPERATIONS
// ============================================================================

/// Get symbol by token and exchange (exact match)
pub fn get_symbol_by_token(token: &str, exchange: &str) -> Result<Option<ShoonyaSymbol>> {
    let db = get_db()?;

    // Normalize exchange for lookup (NSE_INDEX -> NSE for actual API)
    let lookup_exchange = match exchange {
        "NSE_INDEX" | "BSE_INDEX" => exchange,  // Keep index exchanges as-is in DB
        _ => exchange,
    };

    let mut stmt = db.prepare(
        "SELECT symbol, br_symbol, name, exchange, br_exchange, token, expiry, strike, lot_size, instrument_type, tick_size
         FROM shoonya_symbols WHERE token = ? AND exchange = ?",
    )?;

    let result = stmt
        .query_row(params![token, lookup_exchange], |row| {
            Ok(ShoonyaSymbol {
                symbol: row.get(0)?,
                br_symbol: row.get(1)?,
                name: row.get(2)?,
                exchange: row.get(3)?,
                br_exchange: row.get(4)?,
                token: row.get(5)?,
                expiry: row.get(6)?,
                strike: row.get(7)?,
                lot_size: row.get(8)?,
                instrument_type: row.get(9)?,
                tick_size: row.get(10)?,
            })
        })
        .optional()
        .context("Failed to get symbol by token")?;

    Ok(result)
}

/// Get token by symbol and exchange (exact match)
pub fn get_token_by_symbol(symbol: &str, exchange: &str) -> Result<Option<String>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT token FROM shoonya_symbols WHERE symbol = ? AND exchange = ? LIMIT 1",
    )?;

    let result = stmt
        .query_row(params![symbol, exchange], |row| row.get(0))
        .optional()
        .context("Failed to get token by symbol")?;

    Ok(result)
}

/// Get broker symbol by OpenAlgo symbol and exchange
pub fn get_br_symbol(symbol: &str, exchange: &str) -> Result<Option<String>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT br_symbol FROM shoonya_symbols WHERE symbol = ? AND exchange = ? LIMIT 1",
    )?;

    let result = stmt
        .query_row(params![symbol, exchange], |row| row.get(0))
        .optional()
        .context("Failed to get broker symbol")?;

    Ok(result)
}

/// Search symbols by keyword (fuzzy search on symbol and name)
pub fn search_symbols(keyword: &str, exchange: Option<&str>, limit: i32) -> Result<Vec<ShoonyaSymbolSearchResult>> {
    let db = get_db()?;

    let search_pattern = format!("%{}%", keyword.to_uppercase());

    let query = if let Some(_exch) = exchange {
        "SELECT symbol, br_symbol, name, exchange, token, instrument_type, lot_size,
                CASE
                    WHEN UPPER(symbol) = UPPER(?) THEN 100.0
                    WHEN UPPER(symbol) LIKE ? || '%' THEN 90.0
                    WHEN UPPER(name) LIKE ? || '%' THEN 80.0
                    WHEN UPPER(symbol) LIKE ? THEN 70.0
                    WHEN UPPER(name) LIKE ? THEN 60.0
                    ELSE 50.0
                END as relevance_score
         FROM shoonya_symbols
         WHERE (UPPER(symbol) LIKE ? OR UPPER(name) LIKE ?)
         AND exchange = ?
         ORDER BY relevance_score DESC, symbol ASC
         LIMIT ?"
    } else {
        "SELECT symbol, br_symbol, name, exchange, token, instrument_type, lot_size,
                CASE
                    WHEN UPPER(symbol) = UPPER(?) THEN 100.0
                    WHEN UPPER(symbol) LIKE ? || '%' THEN 90.0
                    WHEN UPPER(name) LIKE ? || '%' THEN 80.0
                    WHEN UPPER(symbol) LIKE ? THEN 70.0
                    WHEN UPPER(name) LIKE ? THEN 60.0
                    ELSE 50.0
                END as relevance_score
         FROM shoonya_symbols
         WHERE UPPER(symbol) LIKE ? OR UPPER(name) LIKE ?
         ORDER BY relevance_score DESC, symbol ASC
         LIMIT ?"
    };

    let mut stmt = db.prepare(query)?;

    let results = if let Some(exch) = exchange {
        let keyword_upper = keyword.to_uppercase();
        stmt.query_map(
            params![&keyword_upper, &keyword_upper, &keyword_upper, &search_pattern, &search_pattern, &search_pattern, &search_pattern, exch, limit],
            |row| {
                Ok(ShoonyaSymbolSearchResult {
                    symbol: row.get(0)?,
                    br_symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    token: row.get(4)?,
                    instrument_type: row.get(5)?,
                    lot_size: row.get(6)?,
                    relevance_score: row.get(7)?,
                })
            },
        )?
        .collect::<Result<Vec<_>, _>>()?
    } else {
        let keyword_upper = keyword.to_uppercase();
        stmt.query_map(
            params![&keyword_upper, &keyword_upper, &keyword_upper, &search_pattern, &search_pattern, &search_pattern, &search_pattern, limit],
            |row| {
                Ok(ShoonyaSymbolSearchResult {
                    symbol: row.get(0)?,
                    br_symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    token: row.get(4)?,
                    instrument_type: row.get(5)?,
                    lot_size: row.get(6)?,
                    relevance_score: row.get(7)?,
                })
            },
        )?
        .collect::<Result<Vec<_>, _>>()?
    };

    Ok(results)
}

/// Get all symbols for an exchange
pub fn get_symbols_by_exchange(exchange: &str, limit: i32) -> Result<Vec<ShoonyaSymbol>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT symbol, br_symbol, name, exchange, br_exchange, token, expiry, strike, lot_size, instrument_type, tick_size
         FROM shoonya_symbols WHERE exchange = ? ORDER BY symbol LIMIT ?",
    )?;

    let results = stmt
        .query_map(params![exchange, limit], |row| {
            Ok(ShoonyaSymbol {
                symbol: row.get(0)?,
                br_symbol: row.get(1)?,
                name: row.get(2)?,
                exchange: row.get(3)?,
                br_exchange: row.get(4)?,
                token: row.get(5)?,
                expiry: row.get(6)?,
                strike: row.get(7)?,
                lot_size: row.get(8)?,
                instrument_type: row.get(9)?,
                tick_size: row.get(10)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(results)
}

/// Get total symbol count
pub fn get_symbol_count() -> Result<i64> {
    let db = get_db()?;

    let count: i64 = db.query_row("SELECT COUNT(*) FROM shoonya_symbols", [], |row| row.get(0))?;

    Ok(count)
}

/// Get metadata (last updated timestamp and count)
pub fn get_metadata() -> Result<Option<(i64, i64)>> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT last_updated, symbol_count FROM shoonya_symbols_metadata WHERE id = 1")?;

    let result = stmt
        .query_row([], |row| Ok((row.get(0)?, row.get(1)?)))
        .optional()?;

    Ok(result)
}

/// Check if database is populated and recent
pub fn is_database_valid(max_age_seconds: i64) -> Result<bool> {
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    match get_metadata()? {
        Some((last_updated, count)) => {
            Ok(count > 0 && (now - last_updated) < max_age_seconds)
        }
        None => Ok(false),
    }
}

// ============================================================================
// DELETE OPERATIONS
// ============================================================================

/// Clear all symbols (for re-import)
pub fn clear_all_symbols() -> Result<()> {
    let db = get_db()?;

    db.execute("DELETE FROM shoonya_symbols", [])?;
    db.execute("DELETE FROM shoonya_symbols_metadata", [])?;

    Ok(())
}

/// Delete symbols by exchange
pub fn delete_symbols_by_exchange(exchange: &str) -> Result<()> {
    let db = get_db()?;

    db.execute("DELETE FROM shoonya_symbols WHERE exchange = ?", params![exchange])?;

    Ok(())
}
