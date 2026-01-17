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
pub async fn db_delete_watchlist(id: String) -> Result<String, String> {
    queries::delete_watchlist(&id).map_err(|e| e.to_string())?;
    Ok("Watchlist deleted successfully".to_string())
}

// ============================================================================
// Cache Commands
// ============================================================================

#[tauri::command]
pub async fn db_save_market_data_cache(symbol: String, category: String, quote_data: String) -> Result<String, String> {
    cache::save_market_data_cache(&symbol, &category, &quote_data).map_err(|e| e.to_string())?;
    Ok("Market data cached successfully".to_string())
}

#[tauri::command]
pub async fn db_get_cached_market_data(symbol: String, category: String, max_age_minutes: i64) -> Result<Option<String>, String> {
    cache::get_cached_market_data(&symbol, &category, max_age_minutes).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_clear_market_data_cache() -> Result<String, String> {
    cache::clear_market_data_cache().map_err(|e| e.to_string())?;
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
