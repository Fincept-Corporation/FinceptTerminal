// Generic Key-Value Storage Database
// SQLite-based key-value storage

use crate::database::pool::get_db;
use anyhow::{Context, Result};
use rusqlite::{params, OptionalExtension};
use std::time::{SystemTime, UNIX_EPOCH};

/// Set a value in storage
pub fn storage_set(key: &str, value: &str) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    db.execute(
        "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?, ?, ?)",
        params![key, value, now],
    )
    .context("Failed to set storage value")?;

    Ok(())
}

/// Get a value from storage
pub fn storage_get(key: &str) -> Result<Option<String>> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT value FROM key_value_storage WHERE key = ?")?;

    let result = stmt
        .query_row(params![key], |row: &rusqlite::Row| row.get::<_, String>(0))
        .optional()
        .context("Failed to get storage value")?;

    Ok(result)
}

/// Remove a value from storage
pub fn storage_remove(key: &str) -> Result<()> {
    let db = get_db()?;

    db.execute("DELETE FROM key_value_storage WHERE key = ?", params![key])
        .context("Failed to remove storage value")?;

    Ok(())
}

/// Clear all storage (use with caution)
pub fn storage_clear() -> Result<()> {
    let db = get_db()?;

    db.execute("DELETE FROM key_value_storage", params![])
        .context("Failed to clear storage")?;

    Ok(())
}

/// Check if a key exists
pub fn storage_has(key: &str) -> Result<bool> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT 1 FROM key_value_storage WHERE key = ? LIMIT 1")?;

    let exists = stmt
        .query_row(params![key], |_| Ok(()))
        .optional()?
        .is_some();

    Ok(exists)
}

/// Get all keys (useful for debugging)
pub fn storage_keys() -> Result<Vec<String>> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT key FROM key_value_storage ORDER BY key")?;

    let keys = stmt
        .query_map(params![], |row: &rusqlite::Row| row.get::<_, String>(0))?
        .collect::<Result<Vec<String>, _>>()
        .context("Failed to get storage keys")?;

    Ok(keys)
}

/// Get storage size (number of entries)
pub fn storage_size() -> Result<usize> {
    let db = get_db()?;

    let mut stmt = db.prepare("SELECT COUNT(*) FROM key_value_storage")?;

    let count: i64 = stmt.query_row(params![], |row: &rusqlite::Row| row.get(0))?;

    Ok(count as usize)
}

/// Get multiple values at once (batch operation)
pub fn storage_get_many(keys: &[String]) -> Result<Vec<(String, Option<String>)>> {
    let _db = get_db()?;
    let mut results = Vec::new();

    for key in keys {
        let value = storage_get(key)?;
        results.push((key.clone(), value));
    }

    Ok(results)
}

/// Set multiple values at once (batch operation)
pub fn storage_set_many(pairs: &[(String, String)]) -> Result<()> {
    let db = get_db()?;
    let now = SystemTime::now()
        .duration_since(UNIX_EPOCH)?
        .as_secs() as i64;

    let tx = db.unchecked_transaction()?;

    for (key, value) in pairs {
        tx.execute(
            "INSERT OR REPLACE INTO key_value_storage (key, value, updated_at) VALUES (?, ?, ?)",
            params![key, value, now],
        )
        .context("Failed to set storage value in batch")?;
    }

    tx.commit()?;

    Ok(())
}
