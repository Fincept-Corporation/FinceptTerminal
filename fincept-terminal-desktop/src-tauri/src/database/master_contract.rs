// Master Contract Database Operations
// Stores broker symbol data for fast lookup and offline access

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

// ============================================================================
// Data Structures
// ============================================================================

/// Symbol record for database storage
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Symbol {
    pub id: Option<i64>,
    pub broker_id: String,
    pub symbol: String,
    pub br_symbol: String,
    pub name: Option<String>,
    pub exchange: String,
    pub br_exchange: Option<String>,
    pub token: String,
    pub expiry: Option<String>,
    pub strike: Option<f64>,
    pub lot_size: Option<i32>,
    pub instrument_type: Option<String>,
    pub tick_size: Option<f64>,
    pub segment: Option<String>,
}

/// Master contract cache metadata
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MasterContractCache {
    pub broker_id: String,
    pub symbols_data: String,
    pub symbol_count: i64,
    pub last_updated: i64,
    pub created_at: i64,
}

/// Search result with relevance score
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SearchResult {
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
}

// ============================================================================
// Symbol Operations (New searchable table)
// ============================================================================

/// Delete all symbols for a broker
pub fn delete_symbols_for_broker(broker_id: &str) -> Result<i64> {
    let db = get_db()?;
    let deleted = db.execute(
        "DELETE FROM symbols WHERE broker_id = ?",
        params![broker_id],
    ).context("Failed to delete symbols")?;

    Ok(deleted as i64)
}

/// Bulk insert symbols using a transaction for performance
pub fn bulk_insert_symbols(symbols: &[Symbol]) -> Result<i64> {
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

/// Search symbols by query string
pub fn search_symbols(
    broker_id: &str,
    query: &str,
    exchange: Option<&str>,
    instrument_type: Option<&str>,
    limit: i32,
) -> Result<Vec<SearchResult>> {
    let db = get_db()?;

    let search_pattern = format!("%{}%", query.to_uppercase());

    // Build dynamic query based on filters
    let mut sql = String::from(
        "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size
         FROM symbols
         WHERE broker_id = ? AND (
             UPPER(symbol) LIKE ? OR
             UPPER(name) LIKE ? OR
             UPPER(br_symbol) LIKE ? OR
             token LIKE ?
         )"
    );

    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = vec![
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

    // Order by relevance: exact matches first, then starts with, then contains
    sql.push_str(&format!(
        " ORDER BY
            CASE
                WHEN UPPER(symbol) = UPPER(?) THEN 0
                WHEN UPPER(symbol) LIKE UPPER(?) || '%' THEN 1
                WHEN UPPER(name) = UPPER(?) THEN 2
                WHEN UPPER(name) LIKE UPPER(?) || '%' THEN 3
                ELSE 4
            END,
            symbol
         LIMIT ?"
    ));

    params_vec.push(Box::new(query.to_uppercase()));
    params_vec.push(Box::new(query.to_uppercase()));
    params_vec.push(Box::new(query.to_uppercase()));
    params_vec.push(Box::new(query.to_uppercase()));
    params_vec.push(Box::new(limit));

    let mut stmt = db.prepare(&sql)?;

    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();

    let results = stmt.query_map(params_refs.as_slice(), |row| {
        Ok(SearchResult {
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
        })
    })?;

    let mut symbols = Vec::new();
    for result in results {
        symbols.push(result?);
    }

    Ok(symbols)
}

/// Get symbol by exact match
pub fn get_symbol(broker_id: &str, symbol: &str, exchange: &str) -> Result<Option<SearchResult>> {
    let db = get_db()?;

    let result = db.query_row(
        "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size
         FROM symbols
         WHERE broker_id = ? AND UPPER(symbol) = UPPER(?) AND exchange = ?",
        params![broker_id, symbol, exchange],
        |row| {
            Ok(SearchResult {
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
            })
        },
    ).optional()?;

    Ok(result)
}

/// Get symbol by token
pub fn get_symbol_by_token(broker_id: &str, token: &str) -> Result<Option<SearchResult>> {
    let db = get_db()?;

    let result = db.query_row(
        "SELECT symbol, br_symbol, name, exchange, token, expiry, strike, lot_size, instrument_type, tick_size
         FROM symbols
         WHERE broker_id = ? AND token = ?",
        params![broker_id, token],
        |row| {
            Ok(SearchResult {
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
            })
        },
    ).optional()?;

    Ok(result)
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

    let mut stmt = db.prepare(
        "SELECT DISTINCT exchange FROM symbols WHERE broker_id = ? ORDER BY exchange"
    )?;

    let results = stmt.query_map(params![broker_id], |row| row.get(0))?;

    let mut exchanges = Vec::new();
    for result in results {
        exchanges.push(result?);
    }

    Ok(exchanges)
}

/// Get distinct expiries for a broker/exchange/underlying
pub fn get_expiries(
    broker_id: &str,
    exchange: Option<&str>,
    underlying: Option<&str>,
) -> Result<Vec<String>> {
    let db = get_db()?;

    let mut sql = String::from(
        "SELECT DISTINCT expiry FROM symbols WHERE broker_id = ? AND expiry IS NOT NULL AND expiry != ''"
    );

    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = vec![Box::new(broker_id.to_string())];

    if let Some(exch) = exchange {
        sql.push_str(" AND exchange = ?");
        params_vec.push(Box::new(exch.to_string()));
    }

    if let Some(name) = underlying {
        sql.push_str(" AND name = ?");
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

// ============================================================================
// Cache Metadata Operations (Existing)
// ============================================================================

/// Save master contract cache metadata
pub fn save_master_contract(
    broker_id: &str,
    symbols_data: &str,
    symbol_count: i64,
) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO master_contract_cache
         (broker_id, symbols_data, symbol_count, last_updated, created_at)
         VALUES (?, ?, ?, ?, COALESCE((SELECT created_at FROM master_contract_cache WHERE broker_id = ?), ?))",
        params![broker_id, symbols_data, symbol_count, now, broker_id, now],
    )
    .context("Failed to save master contract")?;

    Ok(())
}

/// Get master contract cache metadata
pub fn get_master_contract(broker_id: &str) -> Result<Option<MasterContractCache>> {
    let db = get_db()?;

    let mut stmt = db
        .prepare("SELECT broker_id, symbols_data, symbol_count, last_updated, created_at FROM master_contract_cache WHERE broker_id = ?")?;

    let result = stmt
        .query_row(params![broker_id], |row: &rusqlite::Row| {
            Ok(MasterContractCache {
                broker_id: row.get(0)?,
                symbols_data: row.get(1)?,
                symbol_count: row.get(2)?,
                last_updated: row.get(3)?,
                created_at: row.get(4)?,
            })
        })
        .optional()
        .context("Failed to get master contract")?;

    Ok(result)
}

/// Delete master contract cache
pub fn delete_master_contract(broker_id: &str) -> Result<()> {
    let db = get_db()?;

    db.execute(
        "DELETE FROM master_contract_cache WHERE broker_id = ?",
        params![broker_id],
    )
    .context("Failed to delete master contract")?;

    Ok(())
}

/// Check if cache is valid (not expired)
pub fn is_cache_valid(broker_id: &str, ttl_seconds: i64) -> Result<bool> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let mut stmt = db.prepare(
        "SELECT last_updated FROM master_contract_cache WHERE broker_id = ?",
    )?;

    let last_updated: Option<i64> = stmt
        .query_row(params![broker_id], |row: &rusqlite::Row| row.get(0))
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

    let mut stmt = db.prepare(
        "SELECT last_updated FROM master_contract_cache WHERE broker_id = ?",
    )?;

    let last_updated: Option<i64> = stmt
        .query_row(params![broker_id], |row: &rusqlite::Row| row.get(0))
        .optional()?;

    Ok(last_updated.map(|ts| now - ts))
}
