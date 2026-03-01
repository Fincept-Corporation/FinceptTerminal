// commands/database/cache_commands.rs - Unified cache, tab session, and legacy cache commands

use crate::database::*;

// ============================================================================
// Unified Cache Commands (uses separate fincept_cache.db)
// ============================================================================

#[tauri::command]
pub async fn cache_get(key: String) -> Result<Option<cache::CacheEntry>, String> {
    cache::cache_get(&key).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_get_with_stale(key: String) -> Result<Option<cache::CacheEntry>, String> {
    cache::cache_get_with_stale(&key).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_set(key: String, data: String, category: String, ttl_seconds: i64) -> Result<(), String> {
    cache::cache_set(&key, &data, &category, ttl_seconds).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_delete(key: String) -> Result<bool, String> {
    cache::cache_delete(&key).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_get_many(keys: Vec<String>) -> Result<Vec<cache::CacheEntry>, String> {
    cache::cache_get_many(keys).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_invalidate_category(category: String) -> Result<i64, String> {
    cache::cache_invalidate_category(&category).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_invalidate_pattern(pattern: String) -> Result<i64, String> {
    cache::cache_invalidate_pattern(&pattern).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_cleanup() -> Result<i64, String> {
    cache::cache_cleanup().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_stats() -> Result<cache::CacheStats, String> {
    cache::cache_stats().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn cache_clear_all() -> Result<i64, String> {
    cache::cache_clear_all().map_err(|e| e.to_string())
}

// ============================================================================
// Tab Session Commands
// ============================================================================

#[tauri::command]
pub async fn tab_session_get(tab_id: String) -> Result<Option<cache::TabSession>, String> {
    cache::tab_session_get(&tab_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn tab_session_set(
    tab_id: String,
    tab_name: String,
    state: String,
    scroll_position: Option<String>,
    active_filters: Option<String>,
    selected_items: Option<String>,
) -> Result<(), String> {
    cache::tab_session_set(
        &tab_id,
        &tab_name,
        &state,
        scroll_position.as_deref(),
        active_filters.as_deref(),
        selected_items.as_deref(),
    )
    .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn tab_session_delete(tab_id: String) -> Result<bool, String> {
    cache::tab_session_delete(&tab_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn tab_session_get_all() -> Result<Vec<cache::TabSession>, String> {
    cache::tab_session_get_all().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn tab_session_cleanup(max_age_days: i64) -> Result<i64, String> {
    cache::tab_session_cleanup(max_age_days).map_err(|e| e.to_string())
}

// ============================================================================
// Legacy Cache Commands (kept for backward compatibility during migration)
// ============================================================================

#[tauri::command]
pub async fn db_save_market_data_cache(symbol: String, category: String, quote_data: String) -> Result<String, String> {
    // Redirect to unified cache with 10 minute TTL
    let key = format!("market-quotes:{}:{}", category, symbol);
    cache::cache_set(&key, &quote_data, "market-quotes", 600).map_err(|e| e.to_string())?;
    Ok("Market data cached successfully".to_string())
}

#[tauri::command]
pub async fn db_get_cached_market_data(symbol: String, category: String, _max_age_minutes: i64) -> Result<Option<String>, String> {
    // Redirect to unified cache
    let key = format!("market-quotes:{}:{}", category, symbol);
    match cache::cache_get(&key).map_err(|e| e.to_string())? {
        Some(entry) => Ok(Some(entry.data)),
        None => Ok(None),
    }
}

#[tauri::command]
pub async fn db_clear_market_data_cache() -> Result<String, String> {
    cache::cache_invalidate_category("market-quotes").map_err(|e| e.to_string())?;
    Ok("Market data cache cleared successfully".to_string())
}
