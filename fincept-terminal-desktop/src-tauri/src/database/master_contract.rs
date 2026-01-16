// Master Contract Cache Database
// Stores broker symbol data for fast lookup and offline access

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use std::time::{SystemTime, UNIX_EPOCH};

/// Save master contract data for a broker
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

/// Get master contract data for a broker
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

/// Delete master contract data for a broker
pub fn delete_master_contract(broker_id: &str) -> Result<()> {
    let db = get_db()?;

    db.execute(
        "DELETE FROM master_contract_cache WHERE broker_id = ?",
        params![broker_id],
    )
    .context("Failed to delete master contract")?;

    Ok(())
}

/// Check if master contract exists and is not expired
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

/// Master contract cache data structure
#[derive(Debug, Clone)]
pub struct MasterContractCache {
    pub broker_id: String,
    pub symbols_data: String,
    pub symbol_count: i64,
    pub last_updated: i64,
    pub created_at: i64,
}
