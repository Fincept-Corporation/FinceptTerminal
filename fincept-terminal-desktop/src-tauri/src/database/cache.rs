// Cache Operations - Market data and forum caching with expiration

use crate::database::pool::get_pool;
use anyhow::Result;
use rusqlite::{params, OptionalExtension};

// ============================================================================
// Market Data Cache
// ============================================================================

pub fn save_market_data_cache(symbol: &str, category: &str, quote_data: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO market_data_cache (symbol, category, quote_data, cached_at)
         VALUES (?1, ?2, ?3, CURRENT_TIMESTAMP)",
        params![symbol, category, quote_data],
    )?;

    Ok(())
}

pub fn get_cached_market_data(symbol: &str, category: &str, max_age_minutes: i64) -> Result<Option<String>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT quote_data FROM market_data_cache
             WHERE symbol = ?1 AND category = ?2
             AND datetime(cached_at) > datetime('now', ?3)",
            params![symbol, category, format!("-{} minutes", max_age_minutes)],
            |row| row.get(0),
        )
        .optional()?;

    Ok(result)
}

pub fn clear_market_data_cache() -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM market_data_cache", [])?;

    Ok(())
}

#[allow(dead_code)]
pub fn clear_expired_market_cache(max_age_minutes: i64) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM market_data_cache
         WHERE datetime(cached_at) <= datetime('now', ?1)",
        params![format!("-{} minutes", max_age_minutes)],
    )?;

    Ok(())
}

// ============================================================================
// Forum Cache
// ============================================================================

#[allow(dead_code)]
pub fn cache_forum_categories(categories: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO forum_categories_cache (id, data, cached_at)
         VALUES (1, ?1, CURRENT_TIMESTAMP)",
        params![categories],
    )?;

    Ok(())
}

#[allow(dead_code)]
pub fn get_cached_forum_categories(max_age_minutes: i64) -> Result<Option<String>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT data FROM forum_categories_cache
             WHERE id = 1 AND datetime(cached_at) > datetime('now', ?1)",
            params![format!("-{} minutes", max_age_minutes)],
            |row| row.get(0),
        )
        .optional()?;

    Ok(result)
}

#[allow(dead_code)]
pub fn cache_forum_posts(category_id: Option<i64>, sort_by: &str, posts: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let cache_key = format!("{}_{}", category_id.map_or("all".to_string(), |id| id.to_string()), sort_by);

    conn.execute(
        "INSERT OR REPLACE INTO forum_posts_cache (cache_key, category_id, sort_by, data, cached_at)
         VALUES (?1, ?2, ?3, ?4, CURRENT_TIMESTAMP)",
        params![cache_key, category_id, sort_by, posts],
    )?;

    Ok(())
}

#[allow(dead_code)]
pub fn get_cached_forum_posts(category_id: Option<i64>, sort_by: &str, max_age_minutes: i64) -> Result<Option<String>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let cache_key = format!("{}_{}", category_id.map_or("all".to_string(), |id| id.to_string()), sort_by);

    let result = conn
        .query_row(
            "SELECT data FROM forum_posts_cache
             WHERE cache_key = ?1 AND datetime(cached_at) > datetime('now', ?2)",
            params![cache_key, format!("-{} minutes", max_age_minutes)],
            |row| row.get(0),
        )
        .optional()?;

    Ok(result)
}

#[allow(dead_code)]
pub fn clear_forum_cache() -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM forum_categories_cache", [])?;
    conn.execute("DELETE FROM forum_posts_cache", [])?;
    conn.execute("DELETE FROM forum_stats_cache", [])?;
    conn.execute("DELETE FROM forum_post_details_cache", [])?;

    Ok(())
}
