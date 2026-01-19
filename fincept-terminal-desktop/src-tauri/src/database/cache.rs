// Unified Cache System - All caching operations using separate cache database
// Replaces: market_data_cache, forum_*_cache, in-memory caches

use crate::database::pool::get_cache_db;
use anyhow::Result;
use rusqlite::{params, Connection, OptionalExtension};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

// ============================================================================
// Schema Creation (called from pool.rs on init)
// ============================================================================

pub fn create_cache_schema(conn: &Connection) -> Result<()> {
    conn.execute_batch(
        "
        -- Unified cache table for all data caching
        CREATE TABLE IF NOT EXISTS unified_cache (
            cache_key TEXT PRIMARY KEY,
            category TEXT NOT NULL,
            data TEXT NOT NULL,
            ttl_seconds INTEGER NOT NULL,
            created_at INTEGER NOT NULL,
            expires_at INTEGER NOT NULL,
            last_accessed_at INTEGER NOT NULL,
            hit_count INTEGER DEFAULT 0,
            size_bytes INTEGER DEFAULT 0
        );

        CREATE INDEX IF NOT EXISTS idx_unified_cache_category ON unified_cache(category);
        CREATE INDEX IF NOT EXISTS idx_unified_cache_expires ON unified_cache(expires_at);

        -- Tab session state persistence
        CREATE TABLE IF NOT EXISTS tab_sessions (
            tab_id TEXT PRIMARY KEY,
            tab_name TEXT NOT NULL,
            state TEXT NOT NULL,
            scroll_position TEXT,
            active_filters TEXT,
            selected_items TEXT,
            updated_at INTEGER NOT NULL,
            created_at INTEGER NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_tab_sessions_updated ON tab_sessions(updated_at);
        ",
    )?;
    Ok(())
}

// ============================================================================
// Types
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct CacheEntry {
    pub cache_key: String,
    pub category: String,
    pub data: String,
    pub ttl_seconds: i64,
    pub created_at: i64,
    pub expires_at: i64,
    pub last_accessed_at: i64,
    pub hit_count: i64,
    pub is_expired: bool,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TabSession {
    pub tab_id: String,
    pub tab_name: String,
    pub state: String,
    pub scroll_position: Option<String>,
    pub active_filters: Option<String>,
    pub selected_items: Option<String>,
    pub updated_at: i64,
    pub created_at: i64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct CacheStats {
    pub total_entries: i64,
    pub total_size_bytes: i64,
    pub expired_entries: i64,
    pub categories: Vec<CategoryStats>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct CategoryStats {
    pub category: String,
    pub entry_count: i64,
    pub total_size: i64,
}

fn now_timestamp() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64
}

// ============================================================================
// Unified Cache Operations
// ============================================================================

/// Get a cache entry by key (returns None if expired or not found)
pub fn cache_get(key: &str) -> Result<Option<CacheEntry>> {
    let conn = get_cache_db()?;
    let now = now_timestamp();

    let result: Option<CacheEntry> = conn
        .query_row(
            "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at,
                    last_accessed_at, hit_count
             FROM unified_cache WHERE cache_key = ?1",
            params![key],
            |row| {
                let expires_at: i64 = row.get(5)?;
                Ok(CacheEntry {
                    cache_key: row.get(0)?,
                    category: row.get(1)?,
                    data: row.get(2)?,
                    ttl_seconds: row.get(3)?,
                    created_at: row.get(4)?,
                    expires_at,
                    last_accessed_at: row.get(6)?,
                    hit_count: row.get(7)?,
                    is_expired: now > expires_at,
                })
            },
        )
        .optional()?;

    // Update access time and hit count if found and not expired
    if let Some(ref entry) = result {
        if !entry.is_expired {
            conn.execute(
                "UPDATE unified_cache SET last_accessed_at = ?1, hit_count = hit_count + 1
                 WHERE cache_key = ?2",
                params![now, key],
            )?;
        }
    }

    // Return None if expired
    Ok(result.filter(|e| !e.is_expired))
}

/// Get cache entry even if expired (for stale-while-revalidate)
pub fn cache_get_with_stale(key: &str) -> Result<Option<CacheEntry>> {
    let conn = get_cache_db()?;
    let now = now_timestamp();

    conn.query_row(
        "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at,
                last_accessed_at, hit_count
         FROM unified_cache WHERE cache_key = ?1",
        params![key],
        |row| {
            let expires_at: i64 = row.get(5)?;
            Ok(CacheEntry {
                cache_key: row.get(0)?,
                category: row.get(1)?,
                data: row.get(2)?,
                ttl_seconds: row.get(3)?,
                created_at: row.get(4)?,
                expires_at,
                last_accessed_at: row.get(6)?,
                hit_count: row.get(7)?,
                is_expired: now > expires_at,
            })
        },
    )
    .optional()
    .map_err(Into::into)
}

/// Set a cache entry
pub fn cache_set(key: &str, data: &str, category: &str, ttl_seconds: i64) -> Result<()> {
    let conn = get_cache_db()?;
    let now = now_timestamp();
    let expires_at = now + ttl_seconds;
    let size_bytes = data.len() as i64;

    conn.execute(
        "INSERT OR REPLACE INTO unified_cache
         (cache_key, category, data, ttl_seconds, created_at, expires_at, last_accessed_at, hit_count, size_bytes)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?5, 0, ?7)",
        params![key, category, data, ttl_seconds, now, expires_at, size_bytes],
    )?;

    Ok(())
}

/// Delete a cache entry
pub fn cache_delete(key: &str) -> Result<bool> {
    let conn = get_cache_db()?;
    let deleted = conn.execute("DELETE FROM unified_cache WHERE cache_key = ?1", params![key])?;
    Ok(deleted > 0)
}

/// Get multiple cache entries by keys
pub fn cache_get_many(keys: Vec<String>) -> Result<Vec<CacheEntry>> {
    let conn = get_cache_db()?;
    let now = now_timestamp();

    let placeholders: Vec<String> = keys.iter().enumerate().map(|(i, _)| format!("?{}", i + 1)).collect();
    let query = format!(
        "SELECT cache_key, category, data, ttl_seconds, created_at, expires_at,
                last_accessed_at, hit_count
         FROM unified_cache WHERE cache_key IN ({}) AND expires_at > ?{}",
        placeholders.join(","),
        keys.len() + 1
    );

    let mut stmt = conn.prepare(&query)?;
    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = keys
        .iter()
        .map(|k| Box::new(k.clone()) as Box<dyn rusqlite::ToSql>)
        .collect();
    params_vec.push(Box::new(now));

    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();

    let entries = stmt
        .query_map(params_refs.as_slice(), |row| {
            let expires_at: i64 = row.get(5)?;
            Ok(CacheEntry {
                cache_key: row.get(0)?,
                category: row.get(1)?,
                data: row.get(2)?,
                ttl_seconds: row.get(3)?,
                created_at: row.get(4)?,
                expires_at,
                last_accessed_at: row.get(6)?,
                hit_count: row.get(7)?,
                is_expired: now > expires_at,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();

    Ok(entries)
}

/// Invalidate all entries in a category
pub fn cache_invalidate_category(category: &str) -> Result<i64> {
    let conn = get_cache_db()?;
    let deleted = conn.execute(
        "DELETE FROM unified_cache WHERE category = ?1",
        params![category],
    )?;
    Ok(deleted as i64)
}

/// Invalidate entries matching a pattern (uses SQL LIKE)
pub fn cache_invalidate_pattern(pattern: &str) -> Result<i64> {
    let conn = get_cache_db()?;
    let deleted = conn.execute(
        "DELETE FROM unified_cache WHERE cache_key LIKE ?1",
        params![pattern],
    )?;
    Ok(deleted as i64)
}

/// Remove all expired entries
pub fn cache_cleanup() -> Result<i64> {
    let conn = get_cache_db()?;
    let now = now_timestamp();
    let deleted = conn.execute(
        "DELETE FROM unified_cache WHERE expires_at <= ?1",
        params![now],
    )?;
    Ok(deleted as i64)
}

/// Get cache statistics
pub fn cache_stats() -> Result<CacheStats> {
    let conn = get_cache_db()?;
    let now = now_timestamp();

    let total_entries: i64 = conn.query_row(
        "SELECT COUNT(*) FROM unified_cache",
        [],
        |row| row.get(0),
    )?;

    let total_size_bytes: i64 = conn.query_row(
        "SELECT COALESCE(SUM(size_bytes), 0) FROM unified_cache",
        [],
        |row| row.get(0),
    )?;

    let expired_entries: i64 = conn.query_row(
        "SELECT COUNT(*) FROM unified_cache WHERE expires_at <= ?1",
        params![now],
        |row| row.get(0),
    )?;

    let mut stmt = conn.prepare(
        "SELECT category, COUNT(*), COALESCE(SUM(size_bytes), 0)
         FROM unified_cache GROUP BY category",
    )?;

    let categories: Vec<CategoryStats> = stmt
        .query_map([], |row| {
            Ok(CategoryStats {
                category: row.get(0)?,
                entry_count: row.get(1)?,
                total_size: row.get(2)?,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();

    Ok(CacheStats {
        total_entries,
        total_size_bytes,
        expired_entries,
        categories,
    })
}

/// Clear entire cache
pub fn cache_clear_all() -> Result<i64> {
    let conn = get_cache_db()?;
    let deleted = conn.execute("DELETE FROM unified_cache", [])?;
    Ok(deleted as i64)
}

// ============================================================================
// Tab Session Operations
// ============================================================================

/// Get tab session state
pub fn tab_session_get(tab_id: &str) -> Result<Option<TabSession>> {
    let conn = get_cache_db()?;

    conn.query_row(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters,
                selected_items, updated_at, created_at
         FROM tab_sessions WHERE tab_id = ?1",
        params![tab_id],
        |row| {
            Ok(TabSession {
                tab_id: row.get(0)?,
                tab_name: row.get(1)?,
                state: row.get(2)?,
                scroll_position: row.get(3)?,
                active_filters: row.get(4)?,
                selected_items: row.get(5)?,
                updated_at: row.get(6)?,
                created_at: row.get(7)?,
            })
        },
    )
    .optional()
    .map_err(Into::into)
}

/// Save tab session state
pub fn tab_session_set(
    tab_id: &str,
    tab_name: &str,
    state: &str,
    scroll_position: Option<&str>,
    active_filters: Option<&str>,
    selected_items: Option<&str>,
) -> Result<()> {
    let conn = get_cache_db()?;
    let now = now_timestamp();

    conn.execute(
        "INSERT INTO tab_sessions (tab_id, tab_name, state, scroll_position, active_filters, selected_items, updated_at, created_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?7)
         ON CONFLICT(tab_id) DO UPDATE SET
            tab_name = ?2,
            state = ?3,
            scroll_position = ?4,
            active_filters = ?5,
            selected_items = ?6,
            updated_at = ?7",
        params![tab_id, tab_name, state, scroll_position, active_filters, selected_items, now],
    )?;

    Ok(())
}

/// Delete tab session
pub fn tab_session_delete(tab_id: &str) -> Result<bool> {
    let conn = get_cache_db()?;
    let deleted = conn.execute("DELETE FROM tab_sessions WHERE tab_id = ?1", params![tab_id])?;
    Ok(deleted > 0)
}

/// Get all tab sessions
pub fn tab_session_get_all() -> Result<Vec<TabSession>> {
    let conn = get_cache_db()?;

    let mut stmt = conn.prepare(
        "SELECT tab_id, tab_name, state, scroll_position, active_filters,
                selected_items, updated_at, created_at
         FROM tab_sessions ORDER BY updated_at DESC",
    )?;

    let sessions: Vec<TabSession> = stmt
        .query_map([], |row| {
            Ok(TabSession {
                tab_id: row.get(0)?,
                tab_name: row.get(1)?,
                state: row.get(2)?,
                scroll_position: row.get(3)?,
                active_filters: row.get(4)?,
                selected_items: row.get(5)?,
                updated_at: row.get(6)?,
                created_at: row.get(7)?,
            })
        })?
        .filter_map(|r| r.ok())
        .collect();

    Ok(sessions)
}

/// Clear old tab sessions (older than X days)
pub fn tab_session_cleanup(max_age_days: i64) -> Result<i64> {
    let conn = get_cache_db()?;
    let cutoff = now_timestamp() - (max_age_days * 24 * 60 * 60);
    let deleted = conn.execute(
        "DELETE FROM tab_sessions WHERE updated_at < ?1",
        params![cutoff],
    )?;
    Ok(deleted as i64)
}
