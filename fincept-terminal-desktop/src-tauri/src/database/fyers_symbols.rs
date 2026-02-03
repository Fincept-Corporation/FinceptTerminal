// Fyers Symbol Master Database
// Stores individual Fyers symbols with token mapping for fast lookup

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/// Fyers symbol data structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct FyersSymbol {
    pub token: i32,
    pub symbol: String,
    pub name: String,
    pub exchange: String,
    pub segment: String,
    pub instrument_type: String,
    pub isin: Option<String>,
    pub tick_size: f64,
    pub lot_size: i32,
}

/// Symbol search result
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SymbolSearchResult {
    pub token: i32,
    pub symbol: String,
    pub name: String,
    pub exchange: String,
    pub relevance_score: f64,
}

// ============================================================================
// DATABASE SCHEMA
// ============================================================================

/// Initialize Fyers symbols table
pub fn init_fyers_symbols_table() -> Result<()> {
    let db = get_db()?;

    db.execute(
        "CREATE TABLE IF NOT EXISTS fyers_symbols (
            token INTEGER PRIMARY KEY,
            symbol TEXT NOT NULL,
            name TEXT NOT NULL,
            exchange TEXT NOT NULL,
            segment TEXT NOT NULL,
            instrument_type TEXT NOT NULL,
            isin TEXT,
            tick_size REAL NOT NULL DEFAULT 0.05,
            lot_size INTEGER NOT NULL DEFAULT 1,
            last_updated INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        )",
        [],
    )
    .context("Failed to create fyers_symbols table")?;

    // Create indexes for fast lookup
    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_fyers_symbol ON fyers_symbols(symbol, exchange)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_fyers_exchange ON fyers_symbols(exchange)",
        [],
    )?;

    db.execute(
        "CREATE INDEX IF NOT EXISTS idx_fyers_name ON fyers_symbols(name)",
        [],
    )?;

    // Create metadata table to track last update
    db.execute(
        "CREATE TABLE IF NOT EXISTS fyers_symbols_metadata (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            last_updated INTEGER NOT NULL,
            symbol_count INTEGER NOT NULL,
            csv_data TEXT
        )",
        [],
    )?;

    Ok(())
}

// ============================================================================
// WRITE OPERATIONS
// ============================================================================

/// Save a single Fyers symbol
pub fn save_symbol(symbol: &FyersSymbol) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO fyers_symbols
         (token, symbol, name, exchange, segment, instrument_type, isin, tick_size, lot_size, last_updated, created_at)
         VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                COALESCE((SELECT created_at FROM fyers_symbols WHERE token = ?), ?))",
        params![
            symbol.token,
            symbol.symbol,
            symbol.name,
            symbol.exchange,
            symbol.segment,
            symbol.instrument_type,
            symbol.isin,
            symbol.tick_size,
            symbol.lot_size,
            now,
            symbol.token,
            now
        ],
    )
    .context("Failed to save Fyers symbol")?;

    Ok(())
}

/// Save multiple symbols in a transaction (fast bulk insert)
pub fn save_symbols_bulk(symbols: &[FyersSymbol]) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let tx = db.unchecked_transaction()?;

    {
        let mut stmt = tx.prepare(
            "INSERT OR REPLACE INTO fyers_symbols
             (token, symbol, name, exchange, segment, instrument_type, isin, tick_size, lot_size, last_updated, created_at)
             VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?,
                    COALESCE((SELECT created_at FROM fyers_symbols WHERE token = ?), ?))",
        )?;

        for symbol in symbols {
            stmt.execute(params![
                symbol.token,
                symbol.symbol,
                symbol.name,
                symbol.exchange,
                symbol.segment,
                symbol.instrument_type,
                symbol.isin,
                symbol.tick_size,
                symbol.lot_size,
                now,
                symbol.token,
                now
            ])?;
        }
    }

    tx.commit()?;

    // Update metadata
    update_metadata(symbols.len() as i64, None)?;

    Ok(())
}

/// Update metadata after bulk insert
pub fn update_metadata(symbol_count: i64, csv_data: Option<&str>) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO fyers_symbols_metadata (id, last_updated, symbol_count, csv_data)
         VALUES (1, ?, ?, ?)",
        params![now, symbol_count, csv_data],
    )?;

    Ok(())
}

// ============================================================================
// READ OPERATIONS
// ============================================================================

/// Get symbol by token (exact match)
pub fn get_symbol_by_token(token: i32) -> Result<Option<FyersSymbol>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT token, symbol, name, exchange, segment, instrument_type, isin, tick_size, lot_size
         FROM fyers_symbols WHERE token = ?",
    )?;

    let result = stmt
        .query_row(params![token], |row| {
            Ok(FyersSymbol {
                token: row.get(0)?,
                symbol: row.get(1)?,
                name: row.get(2)?,
                exchange: row.get(3)?,
                segment: row.get(4)?,
                instrument_type: row.get(5)?,
                isin: row.get(6)?,
                tick_size: row.get(7)?,
                lot_size: row.get(8)?,
            })
        })
        .optional()
        .context("Failed to get symbol by token")?;

    Ok(result)
}

/// Get token by symbol and exchange (exact match)
pub fn get_token_by_symbol(symbol: &str, exchange: &str) -> Result<Option<i32>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT token FROM fyers_symbols WHERE symbol = ? AND exchange = ? LIMIT 1",
    )?;

    let result = stmt
        .query_row(params![symbol, exchange], |row| row.get(0))
        .optional()
        .context("Failed to get token by symbol")?;

    Ok(result)
}

/// Search symbols by keyword (fuzzy search on symbol and name)
pub fn search_symbols(keyword: &str, exchange: Option<&str>, limit: i32) -> Result<Vec<SymbolSearchResult>> {
    let db = get_db()?;

    let search_pattern = format!("%{}%", keyword.to_uppercase());

    let query = if let Some(_exch) = exchange {
        "SELECT token, symbol, name, exchange,
                CASE
                    WHEN UPPER(symbol) = UPPER(?) THEN 100.0
                    WHEN UPPER(symbol) LIKE ? || '%' THEN 90.0
                    WHEN UPPER(name) LIKE ? || '%' THEN 80.0
                    WHEN UPPER(symbol) LIKE ? THEN 70.0
                    WHEN UPPER(name) LIKE ? THEN 60.0
                    ELSE 50.0
                END as relevance_score
         FROM fyers_symbols
         WHERE (UPPER(symbol) LIKE ? OR UPPER(name) LIKE ?)
         AND exchange = ?
         ORDER BY relevance_score DESC, symbol ASC
         LIMIT ?"
    } else {
        "SELECT token, symbol, name, exchange,
                CASE
                    WHEN UPPER(symbol) = UPPER(?) THEN 100.0
                    WHEN UPPER(symbol) LIKE ? || '%' THEN 90.0
                    WHEN UPPER(name) LIKE ? || '%' THEN 80.0
                    WHEN UPPER(symbol) LIKE ? THEN 70.0
                    WHEN UPPER(name) LIKE ? THEN 60.0
                    ELSE 50.0
                END as relevance_score
         FROM fyers_symbols
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
                Ok(SymbolSearchResult {
                    token: row.get(0)?,
                    symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    relevance_score: row.get(4)?,
                })
            },
        )?
        .collect::<Result<Vec<_>, _>>()?
    } else {
        let keyword_upper = keyword.to_uppercase();
        stmt.query_map(
            params![&keyword_upper, &keyword_upper, &keyword_upper, &search_pattern, &search_pattern, &search_pattern, &search_pattern, limit],
            |row| {
                Ok(SymbolSearchResult {
                    token: row.get(0)?,
                    symbol: row.get(1)?,
                    name: row.get(2)?,
                    exchange: row.get(3)?,
                    relevance_score: row.get(4)?,
                })
            },
        )?
        .collect::<Result<Vec<_>, _>>()?
    };

    Ok(results)
}

/// Get all symbols for an exchange
pub fn get_symbols_by_exchange(exchange: &str, limit: i32) -> Result<Vec<FyersSymbol>> {
    let db = get_db()?;

    let mut stmt = db.prepare(
        "SELECT token, symbol, name, exchange, segment, instrument_type, isin, tick_size, lot_size
         FROM fyers_symbols WHERE exchange = ? ORDER BY symbol LIMIT ?",
    )?;

    let results = stmt
        .query_map(params![exchange, limit], |row| {
            Ok(FyersSymbol {
                token: row.get(0)?,
                symbol: row.get(1)?,
                name: row.get(2)?,
                exchange: row.get(3)?,
                segment: row.get(4)?,
                instrument_type: row.get(5)?,
                isin: row.get(6)?,
                tick_size: row.get(7)?,
                lot_size: row.get(8)?,
            })
        })?
        .collect::<Result<Vec<_>, _>>()?;

    Ok(results)
}

/// Get total symbol count
pub fn get_symbol_count() -> Result<i64> {
    let db = get_db()?;

    let count: i64 = db.query_row("SELECT COUNT(*) FROM fyers_symbols", [], |row| row.get(0))?;

    Ok(count)
}

/// Get metadata (last updated timestamp and count)
pub fn get_metadata() -> Result<Option<(i64, i64)>> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT last_updated, symbol_count FROM fyers_symbols_metadata WHERE id = 1")?;

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

    db.execute("DELETE FROM fyers_symbols", [])?;
    db.execute("DELETE FROM fyers_symbols_metadata", [])?;

    Ok(())
}

/// Delete symbols by exchange
pub fn delete_symbols_by_exchange(exchange: &str) -> Result<()> {
    let db = get_db()?;

    db.execute("DELETE FROM fyers_symbols WHERE exchange = ?", params![exchange])?;

    Ok(())
}
