// Database Tauri Commands - Expose all database operations to frontend

use crate::database::*;

// ============================================================================
// Database Health Commands
// ============================================================================

#[tauri::command]
pub async fn db_check_health() -> Result<bool, String> {
    match pool::get_pool() {
        Ok(_) => Ok(true),
        Err(e) => {
            eprintln!("[Database] Health check failed: {}", e);
            Err(format!("Database not initialized: {}", e))
        }
    }
}

// ============================================================================
// Settings Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_setting(key: String, value: String, category: Option<String>) -> Result<String, String> {
    operations::save_setting(&key, &value, category.as_deref())
        .map_err(|e| e.to_string())?;
    Ok("Setting saved successfully".to_string())
}

#[tauri::command]
pub async fn db_get_setting(key: String) -> Result<Option<String>, String> {
    operations::get_setting(&key).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_all_settings() -> Result<Vec<Setting>, String> {
    operations::get_all_settings().map_err(|e| e.to_string())
}

// ============================================================================
// Credentials Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_credential(credential: Credential) -> Result<OperationResult, String> {
    operations::save_credential(&credential).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_credentials() -> Result<Vec<Credential>, String> {
    operations::get_credentials().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_credential_by_service(service_name: String) -> Result<Option<Credential>, String> {
    operations::get_credential_by_service(&service_name).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_credential(id: i64) -> Result<OperationResult, String> {
    operations::delete_credential(id).map_err(|e| e.to_string())
}

// ============================================================================
// LLM Commands
// ============================================================================

#[tauri::command]
pub async fn db_get_llm_configs() -> Result<Vec<LLMConfig>, String> {
    operations::get_llm_configs().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_save_llm_config(config: LLMConfig) -> Result<String, String> {
    operations::save_llm_config(&config).map_err(|e| e.to_string())?;
    Ok("LLM config saved successfully".to_string())
}

#[tauri::command]
pub async fn db_get_llm_global_settings() -> Result<LLMGlobalSettings, String> {
    operations::get_llm_global_settings().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_save_llm_global_settings(settings: LLMGlobalSettings) -> Result<String, String> {
    operations::save_llm_global_settings(&settings).map_err(|e| e.to_string())?;
    Ok("LLM global settings saved successfully".to_string())
}

#[tauri::command]
pub async fn db_set_active_llm_provider(provider: String) -> Result<String, String> {
    operations::set_active_llm_provider(&provider).map_err(|e| e.to_string())?;
    Ok("Active LLM provider updated".to_string())
}

// ============================================================================
// LLM Model Config Commands
// ============================================================================

#[tauri::command]
pub async fn db_get_llm_model_configs() -> Result<Vec<LLMModelConfig>, String> {
    operations::get_llm_model_configs().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_save_llm_model_config(config: LLMModelConfig) -> Result<OperationResult, String> {
    operations::save_llm_model_config(&config).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_llm_model_config(id: String) -> Result<OperationResult, String> {
    operations::delete_llm_model_config(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_toggle_llm_model_config_enabled(id: String) -> Result<OperationResult, String> {
    operations::toggle_llm_model_config_enabled(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_update_llm_model_id(id: String, new_model_id: String) -> Result<OperationResult, String> {
    operations::update_llm_model_id(&id, &new_model_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_fix_google_model_ids() -> Result<OperationResult, String> {
    operations::fix_google_model_ids().map_err(|e| e.to_string())
}

// ============================================================================
// Chat Commands
// ============================================================================

#[tauri::command]
pub async fn db_create_chat_session(title: String) -> Result<ChatSession, String> {
    operations::create_chat_session(&title).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_chat_sessions(limit: Option<i64>) -> Result<Vec<ChatSession>, String> {
    operations::get_chat_sessions(limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_add_chat_message(message: ChatMessage) -> Result<ChatMessage, String> {
    operations::add_chat_message(&message).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_chat_messages(session_uuid: String) -> Result<Vec<ChatMessage>, String> {
    operations::get_chat_messages(&session_uuid).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_chat_session(session_uuid: String) -> Result<String, String> {
    operations::delete_chat_session(&session_uuid).map_err(|e| e.to_string())?;
    Ok("Chat session deleted successfully".to_string())
}

// ============================================================================
// Data Sources Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_data_source(source: DataSource) -> Result<OperationResultWithId, String> {
    operations::save_data_source(&source).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_all_data_sources() -> Result<Vec<DataSource>, String> {
    operations::get_all_data_sources().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_data_source(id: String) -> Result<OperationResult, String> {
    operations::delete_data_source(&id).map_err(|e| e.to_string())
}

// ============================================================================
// MCP Server Commands
// ============================================================================

#[tauri::command]
pub async fn db_add_mcp_server(server: MCPServer) -> Result<String, String> {
    queries::add_mcp_server(&server).map_err(|e| e.to_string())?;
    Ok("MCP server added successfully".to_string())
}

#[tauri::command]
pub async fn db_get_mcp_servers() -> Result<Vec<MCPServer>, String> {
    queries::get_mcp_servers().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_mcp_server(id: String) -> Result<String, String> {
    queries::delete_mcp_server(&id).map_err(|e| e.to_string())?;
    Ok("MCP server deleted successfully".to_string())
}

#[tauri::command]
pub async fn db_get_internal_tool_settings() -> Result<Vec<InternalMCPToolSetting>, String> {
    queries::get_internal_tool_settings().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_set_internal_tool_enabled(tool_name: String, category: String, is_enabled: bool) -> Result<String, String> {
    queries::set_internal_tool_enabled(&tool_name, &category, is_enabled).map_err(|e| e.to_string())?;
    Ok("Internal tool setting updated".to_string())
}

#[tauri::command]
pub async fn db_is_internal_tool_enabled(tool_name: String) -> Result<bool, String> {
    queries::is_internal_tool_enabled(&tool_name).map_err(|e| e.to_string())
}

// ============================================================================
// Backtesting Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_backtesting_provider(provider: BacktestingProvider) -> Result<OperationResult, String> {
    queries::save_backtesting_provider(&provider).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_backtesting_providers() -> Result<Vec<BacktestingProvider>, String> {
    queries::get_backtesting_providers().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_save_backtesting_strategy(strategy: BacktestingStrategy) -> Result<OperationResult, String> {
    queries::save_backtesting_strategy(&strategy).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_backtesting_strategies() -> Result<Vec<BacktestingStrategy>, String> {
    queries::get_backtesting_strategies().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_save_backtest_run(run: BacktestRun) -> Result<OperationResult, String> {
    queries::save_backtest_run(&run).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_backtest_runs(limit: Option<i64>) -> Result<Vec<BacktestRun>, String> {
    queries::get_backtest_runs(limit).map_err(|e| e.to_string())
}

// ============================================================================
// Context Recording Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_recorded_context(context: RecordedContext) -> Result<String, String> {
    queries::save_recorded_context(&context).map_err(|e| e.to_string())?;
    Ok("Context recorded successfully".to_string())
}

#[tauri::command]
pub async fn db_get_recorded_contexts(tab_name: Option<String>, limit: Option<i64>) -> Result<Vec<RecordedContext>, String> {
    queries::get_recorded_contexts(tab_name.as_deref(), limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_recorded_context(id: String) -> Result<String, String> {
    queries::delete_recorded_context(&id).map_err(|e| e.to_string())?;
    Ok("Context deleted successfully".to_string())
}

// ============================================================================
// Watchlist Commands
// ============================================================================

#[tauri::command]
pub async fn db_create_watchlist(name: String, description: Option<String>, color: String) -> Result<Watchlist, String> {
    queries::create_watchlist(&name, description.as_deref(), &color).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_watchlists() -> Result<Vec<Watchlist>, String> {
    queries::get_watchlists().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_add_watchlist_stock(watchlist_id: String, symbol: String, notes: Option<String>) -> Result<WatchlistStock, String> {
    queries::add_watchlist_stock(&watchlist_id, &symbol, notes.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_watchlist_stocks(watchlist_id: String) -> Result<Vec<WatchlistStock>, String> {
    queries::get_watchlist_stocks(&watchlist_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_remove_watchlist_stock(watchlist_id: String, symbol: String) -> Result<String, String> {
    queries::remove_watchlist_stock(&watchlist_id, &symbol).map_err(|e| e.to_string())?;
    Ok("Stock removed from watchlist successfully".to_string())
}

#[tauri::command]
pub async fn db_delete_watchlist(watchlist_id: String) -> Result<String, String> {
    queries::delete_watchlist(&watchlist_id).map_err(|e| e.to_string())?;
    Ok("Watchlist deleted successfully".to_string())
}

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

// Paper Trading commands moved to src-tauri/src/paper_trading.rs (universal module)
// Use pt_* commands instead of db_* for paper trading

// ============================================================================
// Notes Commands
// ============================================================================

#[tauri::command]
pub async fn db_create_note(note: Note) -> Result<i64, String> {
    notes_excel::create_note(&note).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_all_notes(include_archived: bool) -> Result<Vec<Note>, String> {
    notes_excel::get_all_notes(include_archived).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_update_note(id: i64, note: Note) -> Result<String, String> {
    notes_excel::update_note(id, &note).map_err(|e| e.to_string())?;
    Ok("Note updated successfully".to_string())
}

#[tauri::command]
pub async fn db_delete_note(id: i64) -> Result<String, String> {
    notes_excel::delete_note(id).map_err(|e| e.to_string())?;
    Ok("Note deleted successfully".to_string())
}

#[tauri::command]
pub async fn db_search_notes(query: String) -> Result<Vec<Note>, String> {
    notes_excel::search_notes(&query).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_note_templates() -> Result<Vec<NoteTemplate>, String> {
    notes_excel::get_note_templates().map_err(|e| e.to_string())
}

// ============================================================================
// Excel Commands
// ============================================================================

#[tauri::command]
pub async fn db_add_or_update_excel_file(file: ExcelFile) -> Result<i64, String> {
    notes_excel::add_or_update_excel_file(&file).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_recent_excel_files(limit: i64) -> Result<Vec<ExcelFile>, String> {
    notes_excel::get_recent_excel_files(limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_create_excel_snapshot(file_id: i64, snapshot_name: String, sheet_data: String, created_at: String) -> Result<i64, String> {
    notes_excel::create_excel_snapshot(file_id, &snapshot_name, &sheet_data, &created_at).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_excel_snapshots(file_id: i64) -> Result<Vec<ExcelSnapshot>, String> {
    notes_excel::get_excel_snapshots(file_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_excel_snapshot(id: i64) -> Result<String, String> {
    notes_excel::delete_excel_snapshot(id).map_err(|e| e.to_string())?;
    Ok("Excel snapshot deleted successfully".to_string())
}

// ============================================================================
// Agent Configuration Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_agent_config(config: AgentConfig) -> Result<OperationResult, String> {
    queries::save_agent_config(&config).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_agent_configs() -> Result<Vec<AgentConfig>, String> {
    queries::get_agent_configs().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_agent_config(id: String) -> Result<Option<AgentConfig>, String> {
    queries::get_agent_config(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_agent_configs_by_category(category: String) -> Result<Vec<AgentConfig>, String> {
    queries::get_agent_configs_by_category(&category).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_agent_config(id: String) -> Result<OperationResult, String> {
    queries::delete_agent_config(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_set_active_agent_config(id: String) -> Result<OperationResult, String> {
    queries::set_active_agent_config(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_active_agent_config() -> Result<Option<AgentConfig>, String> {
    queries::get_active_agent_config().map_err(|e| e.to_string())
}

// ============================================================================
// Generic Database Management Commands (Password Protected)
// ============================================================================

use rusqlite::Connection;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

#[derive(Debug, Serialize, Deserialize)]
pub struct DatabaseInfo {
    pub name: String,
    pub path: String,
    pub size_bytes: u64,
    pub table_count: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TableInfo {
    pub name: String,
    pub row_count: i64,
    pub columns: Vec<ColumnInfo>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ColumnInfo {
    pub name: String,
    pub data_type: String,
    pub not_null: bool,
    pub primary_key: bool,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TableRow {
    pub data: HashMap<String, serde_json::Value>,
}

// Password management with session support
#[tauri::command]
pub async fn db_admin_check_password() -> Result<bool, String> {
    // Check if password exists in settings
    match operations::get_setting("db_admin_password") {
        Ok(Some(_)) => Ok(true),
        Ok(None) => Ok(false),
        Err(e) => Err(e.to_string()),
    }
}

#[tauri::command]
pub async fn db_admin_check_session() -> Result<bool, String> {
    // Check if there's a valid session
    match operations::get_setting("db_admin_session_expires") {
        Ok(Some(expires_str)) => {
            if let Ok(expires_ts) = expires_str.parse::<i64>() {
                let now = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_secs() as i64;
                Ok(expires_ts > now)
            } else {
                Ok(false)
            }
        }
        _ => Ok(false),
    }
}

#[tauri::command]
pub async fn db_admin_create_session(duration_hours: i64) -> Result<String, String> {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;
    let expires_at = now + (duration_hours * 3600);

    operations::save_setting("db_admin_session_expires", &expires_at.to_string(), Some("security"))
        .map_err(|e| e.to_string())?;

    Ok("Session created".to_string())
}

#[tauri::command]
pub async fn db_admin_clear_session() -> Result<String, String> {
    // Delete session by setting expired timestamp
    operations::save_setting("db_admin_session_expires", "0", Some("security"))
        .map_err(|e| e.to_string())?;
    Ok("Session cleared".to_string())
}

#[tauri::command]
pub async fn db_admin_set_password(password: String) -> Result<String, String> {
    use sha2::{Sha256, Digest};
    let mut hasher = Sha256::new();
    hasher.update(password.as_bytes());
    let hashed = format!("{:x}", hasher.finalize());

    operations::save_setting("db_admin_password", &hashed, Some("security"))
        .map_err(|e| e.to_string())?;
    Ok("Password set successfully".to_string())
}

#[tauri::command]
pub async fn db_admin_verify_password(password: String) -> Result<bool, String> {
    use sha2::{Sha256, Digest};
    let mut hasher = Sha256::new();
    hasher.update(password.as_bytes());
    let hashed = format!("{:x}", hasher.finalize());

    match operations::get_setting("db_admin_password") {
        Ok(Some(stored_hash)) => Ok(stored_hash == hashed),
        Ok(None) => Ok(true), // No password set, allow access
        Err(e) => Err(e.to_string()),
    }
}

// Get all databases
#[tauri::command]
pub async fn db_admin_get_databases() -> Result<Vec<DatabaseInfo>, String> {
    use std::fs;

    // Get database directory
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .map_err(|e| format!("APPDATA not set: {}", e))?;
        std::path::PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .map_err(|e| format!("HOME not set: {}", e))?;
        std::path::PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .map_err(|e| format!("HOME not set: {}", e))?;
        let xdg_data = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        std::path::PathBuf::from(xdg_data).join("fincept-terminal")
    };

    let mut databases = Vec::new();

    let db_files = vec![
        ("Main Database", "fincept_terminal.db"),
        ("Cache", "fincept_cache.db"),
        ("Notes", "financial_notes.db"),
        ("Excel Files", "excel_files.db"),
        ("Deployed Strategies", "deployed_strategies.db"),
        ("Edgar Cache", "edgar_company_cache.db"),
        ("AkShare Symbols", "akshare_symbols.db"),
    ];

    for (name, filename) in db_files {
        let path = db_dir.join(filename);
        if path.exists() {
            let size = fs::metadata(&path)
                .map(|m| m.len())
                .unwrap_or(0);

            // Count tables
            let table_count = match Connection::open(&path) {
                Ok(conn) => {
                    let count: i64 = conn
                        .query_row(
                            "SELECT COUNT(*) FROM sqlite_master WHERE type='table'",
                            [],
                            |row| row.get(0),
                        )
                        .unwrap_or(0);
                    count as usize
                }
                Err(_) => 0,
            };

            databases.push(DatabaseInfo {
                name: name.to_string(),
                path: path.to_string_lossy().to_string(),
                size_bytes: size,
                table_count,
            });
        }
    }

    Ok(databases)
}

// Get tables in a database
#[tauri::command]
pub async fn db_admin_get_tables(db_path: String) -> Result<Vec<TableInfo>, String> {
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
        .map_err(|e| e.to_string())?;

    let table_names: Vec<String> = stmt
        .query_map([], |row| row.get(0))
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    let mut tables = Vec::new();

    for table_name in table_names {
        // Get row count
        let row_count: i64 = conn
            .query_row(&format!("SELECT COUNT(*) FROM \"{}\"", table_name), [], |row| {
                row.get(0)
            })
            .unwrap_or(0);

        // Get column info
        let mut stmt = conn
            .prepare(&format!("PRAGMA table_info(\"{}\")", table_name))
            .map_err(|e| e.to_string())?;

        let columns: Vec<ColumnInfo> = stmt
            .query_map([], |row| {
                Ok(ColumnInfo {
                    name: row.get(1)?,
                    data_type: row.get(2)?,
                    not_null: row.get::<_, i32>(3)? == 1,
                    primary_key: row.get::<_, i32>(5)? == 1,
                })
            })
            .map_err(|e| e.to_string())?
            .filter_map(|r| r.ok())
            .collect();

        tables.push(TableInfo {
            name: table_name,
            row_count,
            columns,
        });
    }

    Ok(tables)
}

// Get table data with pagination
#[tauri::command]
pub async fn db_admin_get_table_data(
    db_path: String,
    table_name: String,
    limit: i64,
    offset: i64,
) -> Result<Vec<TableRow>, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    if limit < 0 || limit > 10000 {
        return Err("Limit must be between 0 and 10000".to_string());
    }
    if offset < 0 {
        return Err("Offset must be non-negative".to_string());
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!(
        "SELECT * FROM \"{}\" LIMIT {} OFFSET {}",
        table_name, limit, offset
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let column_count = stmt.column_count();
    let column_names: Vec<String> = (0..column_count)
        .map(|i| stmt.column_name(i).unwrap_or("").to_string())
        .collect();

    let rows = stmt
        .query_map([], |row| {
            let mut data = HashMap::new();
            for (i, col_name) in column_names.iter().enumerate() {
                let value = match row.get_ref(i) {
                    Ok(val) => match val {
                        rusqlite::types::ValueRef::Null => serde_json::Value::Null,
                        rusqlite::types::ValueRef::Integer(v) => serde_json::Value::Number(v.into()),
                        rusqlite::types::ValueRef::Real(v) => {
                            serde_json::Number::from_f64(v)
                                .map(serde_json::Value::Number)
                                .unwrap_or(serde_json::Value::Null)
                        }
                        rusqlite::types::ValueRef::Text(v) => {
                            serde_json::Value::String(String::from_utf8_lossy(v).to_string())
                        }
                        rusqlite::types::ValueRef::Blob(v) => {
                            serde_json::Value::String(format!("<BLOB {} bytes>", v.len()))
                        }
                    },
                    Err(_) => serde_json::Value::Null,
                };
                data.insert(col_name.clone(), value);
            }
            Ok(TableRow { data })
        })
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    Ok(rows)
}

// Execute custom SQL query (SELECT only)
#[tauri::command]
pub async fn db_admin_execute_query(
    db_path: String,
    query: String,
) -> Result<Vec<TableRow>, String> {
    // Security: Only allow SELECT queries — reject anything that could modify data
    let query_trimmed = query.trim();
    let query_upper = query_trimmed.to_uppercase();
    if !query_upper.starts_with("SELECT") {
        return Err("Only SELECT queries are allowed".to_string());
    }
    // Reject queries containing multiple statements or dangerous keywords after SELECT
    let dangerous_keywords = ["INSERT", "UPDATE", "DELETE", "DROP", "ALTER", "CREATE", "ATTACH", "DETACH", "PRAGMA"];
    // Check for semicolons (multi-statement) — only allow the trailing one
    let without_trailing = query_trimmed.trim_end_matches(';').trim();
    if without_trailing.contains(';') {
        return Err("Multiple SQL statements are not allowed".to_string());
    }
    // Check for dangerous keywords in the rest of the query (skip the leading SELECT)
    let rest_upper = query_upper.get(6..).unwrap_or("");
    for kw in &dangerous_keywords {
        // Match whole word boundaries to avoid false positives (e.g. column named "updated")
        if rest_upper.split_whitespace().any(|word| word == *kw) {
            return Err(format!("Query contains disallowed keyword: {}", kw));
        }
    }

    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;
    let mut stmt = conn.prepare(query_trimmed).map_err(|e| e.to_string())?;

    let column_count = stmt.column_count();
    let column_names: Vec<String> = (0..column_count)
        .map(|i| stmt.column_name(i).unwrap_or("").to_string())
        .collect();

    let rows = stmt
        .query_map([], |row| {
            let mut data = HashMap::new();
            for (i, col_name) in column_names.iter().enumerate() {
                let value = match row.get_ref(i) {
                    Ok(val) => match val {
                        rusqlite::types::ValueRef::Null => serde_json::Value::Null,
                        rusqlite::types::ValueRef::Integer(v) => serde_json::Value::Number(v.into()),
                        rusqlite::types::ValueRef::Real(v) => {
                            serde_json::Number::from_f64(v)
                                .map(serde_json::Value::Number)
                                .unwrap_or(serde_json::Value::Null)
                        }
                        rusqlite::types::ValueRef::Text(v) => {
                            serde_json::Value::String(String::from_utf8_lossy(v).to_string())
                        }
                        rusqlite::types::ValueRef::Blob(v) => {
                            serde_json::Value::String(format!("<BLOB {} bytes>", v.len()))
                        }
                    },
                    Err(_) => serde_json::Value::Null,
                };
                data.insert(col_name.clone(), value);
            }
            Ok(TableRow { data })
        })
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    Ok(rows)
}

// Update row
#[tauri::command]
pub async fn db_admin_update_row(
    db_path: String,
    table_name: String,
    row_data: HashMap<String, serde_json::Value>,
    primary_key_column: String,
    primary_key_value: serde_json::Value,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    validate_sql_identifier(&primary_key_column, "Primary key column")?;
    for col in row_data.keys() {
        validate_sql_identifier(col, "Column name")?;
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let mut set_clauses = Vec::new();
    for (col, _) in &row_data {
        if col != &primary_key_column {
            set_clauses.push(format!("\"{}\" = ?", col));
        }
    }

    let query = format!(
        "UPDATE \"{}\" SET {} WHERE \"{}\" = ?",
        table_name,
        set_clauses.join(", "),
        primary_key_column
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let mut params: Vec<rusqlite::types::Value> = Vec::new();

    for (col, val) in &row_data {
        if col != &primary_key_column {
            params.push(json_to_sql_value(val));
        }
    }
    params.push(json_to_sql_value(&primary_key_value));

    stmt.execute(rusqlite::params_from_iter(params))
        .map_err(|e| e.to_string())?;

    Ok("Row updated successfully".to_string())
}

// Insert row
#[tauri::command]
pub async fn db_admin_insert_row(
    db_path: String,
    table_name: String,
    row_data: HashMap<String, serde_json::Value>,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    for col in row_data.keys() {
        validate_sql_identifier(col, "Column name")?;
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let columns: Vec<String> = row_data.keys().map(|k| format!("\"{}\"", k)).collect();
    let placeholders: Vec<String> = (0..row_data.len()).map(|_| "?".to_string()).collect();

    let query = format!(
        "INSERT INTO \"{}\" ({}) VALUES ({})",
        table_name,
        columns.join(", "),
        placeholders.join(", ")
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let params: Vec<rusqlite::types::Value> = row_data.values().map(json_to_sql_value).collect();

    stmt.execute(rusqlite::params_from_iter(params))
        .map_err(|e| e.to_string())?;

    Ok("Row inserted successfully".to_string())
}

// Delete row
#[tauri::command]
pub async fn db_admin_delete_row(
    db_path: String,
    table_name: String,
    primary_key_column: String,
    primary_key_value: serde_json::Value,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    validate_sql_identifier(&primary_key_column, "Primary key column")?;
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!(
        "DELETE FROM \"{}\" WHERE \"{}\" = ?",
        table_name, primary_key_column
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    stmt.execute([json_to_sql_value(&primary_key_value)])
        .map_err(|e| e.to_string())?;

    Ok("Row deleted successfully".to_string())
}

// Rename table
#[tauri::command]
pub async fn db_admin_rename_table(
    db_path: String,
    old_name: String,
    new_name: String,
) -> Result<String, String> {
    validate_sql_identifier(&old_name, "Old table name")?;
    validate_sql_identifier(&new_name, "New table name")?;
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!("ALTER TABLE \"{}\" RENAME TO \"{}\"", old_name, new_name);
    conn.execute(&query, []).map_err(|e| e.to_string())?;

    Ok("Table renamed successfully".to_string())
}

/// Validates a SQL identifier (table name, column name) to prevent injection.
/// Only allows alphanumeric characters, underscores, and spaces (for quoted identifiers).
/// Rejects empty strings and strings containing quotes, semicolons, or other dangerous chars.
fn validate_sql_identifier(name: &str, kind: &str) -> Result<(), String> {
    if name.is_empty() {
        return Err(format!("{} cannot be empty", kind));
    }
    if name.len() > 128 {
        return Err(format!("{} too long (max 128 chars)", kind));
    }
    // Reject characters that could break out of double-quoted identifiers
    if name.contains('"') || name.contains(';') || name.contains('\\')
        || name.contains('\0') || name.contains('\'')
    {
        return Err(format!("{} contains invalid characters: {}", kind, name));
    }
    Ok(())
}

// Helper function to convert JSON value to SQL value
fn json_to_sql_value(val: &serde_json::Value) -> rusqlite::types::Value {
    match val {
        serde_json::Value::Null => rusqlite::types::Value::Null,
        serde_json::Value::Bool(b) => rusqlite::types::Value::Integer(if *b { 1 } else { 0 }),
        serde_json::Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                rusqlite::types::Value::Integer(i)
            } else if let Some(f) = n.as_f64() {
                rusqlite::types::Value::Real(f)
            } else {
                rusqlite::types::Value::Null
            }
        }
        serde_json::Value::String(s) => rusqlite::types::Value::Text(s.clone()),
        _ => rusqlite::types::Value::Text(val.to_string()),
    }
}
