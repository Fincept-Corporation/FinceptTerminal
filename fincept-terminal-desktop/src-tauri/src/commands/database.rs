// Database Tauri Commands - Expose all database operations to frontend

use crate::database::*;

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

// ============================================================================
// Paper Trading Commands
// ============================================================================

#[tauri::command]
pub async fn db_create_portfolio(
    id: String,
    name: String,
    provider: String,
    initial_balance: f64,
    currency: String,
    margin_mode: String,
    leverage: f64,
) -> Result<paper_trading::PaperTradingPortfolio, String> {
    paper_trading::create_portfolio(&id, &name, &provider, initial_balance, &currency, &margin_mode, leverage)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_portfolio(id: String) -> Result<paper_trading::PaperTradingPortfolio, String> {
    paper_trading::get_portfolio(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_list_portfolios() -> Result<Vec<paper_trading::PaperTradingPortfolio>, String> {
    paper_trading::list_portfolios().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_update_portfolio_balance(id: String, new_balance: f64) -> Result<String, String> {
    paper_trading::update_portfolio_balance(&id, new_balance).map_err(|e| e.to_string())?;
    Ok("Portfolio balance updated successfully".to_string())
}

#[tauri::command]
pub async fn db_create_position(
    id: String,
    portfolio_id: String,
    symbol: String,
    side: String,
    entry_price: f64,
    quantity: f64,
    leverage: f64,
    margin_mode: String,
) -> Result<String, String> {
    paper_trading::create_position(&id, &portfolio_id, &symbol, &side, entry_price, quantity, leverage, &margin_mode)
        .map_err(|e| e.to_string())?;
    Ok("Position created successfully".to_string())
}

#[tauri::command]
pub async fn db_get_portfolio_positions(portfolio_id: String, status: Option<String>) -> Result<Vec<paper_trading::PaperTradingPosition>, String> {
    paper_trading::get_portfolio_positions(&portfolio_id, status.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_create_order(
    id: String,
    portfolio_id: String,
    symbol: String,
    side: String,
    order_type: String,
    quantity: f64,
    price: Option<f64>,
    time_in_force: String,
) -> Result<String, String> {
    paper_trading::create_order(&id, &portfolio_id, &symbol, &side, &order_type, quantity, price, &time_in_force)
        .map_err(|e| e.to_string())?;
    Ok("Order created successfully".to_string())
}

#[tauri::command]
pub async fn db_get_portfolio_orders(portfolio_id: String, status: Option<String>) -> Result<Vec<paper_trading::PaperTradingOrder>, String> {
    paper_trading::get_portfolio_orders(&portfolio_id, status.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_portfolio_trades(portfolio_id: String, limit: Option<i64>) -> Result<Vec<paper_trading::PaperTradingTrade>, String> {
    paper_trading::get_portfolio_trades(&portfolio_id, limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_portfolio(id: String) -> Result<String, String> {
    paper_trading::delete_portfolio(&id).map_err(|e| e.to_string())?;
    Ok("Portfolio deleted successfully".to_string())
}

#[tauri::command]
pub async fn db_get_position(id: String) -> Result<paper_trading::PaperTradingPosition, String> {
    paper_trading::get_position(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_position_by_symbol(
    portfolio_id: String,
    symbol: String,
    status: String,
) -> Result<Option<paper_trading::PaperTradingPosition>, String> {
    paper_trading::get_position_by_symbol(&portfolio_id, &symbol, &status).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_position_by_symbol_and_side(
    portfolio_id: String,
    symbol: String,
    side: String,
    status: String,
) -> Result<Option<paper_trading::PaperTradingPosition>, String> {
    paper_trading::get_position_by_symbol_and_side(&portfolio_id, &symbol, &side, &status).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_update_position(
    id: String,
    quantity: Option<f64>,
    entry_price: Option<f64>,
    current_price: Option<f64>,
    unrealized_pnl: Option<f64>,
    realized_pnl: Option<f64>,
    liquidation_price: Option<f64>,
    status: Option<String>,
    closed_at: Option<String>,
) -> Result<String, String> {
    paper_trading::update_position(
        &id,
        quantity,
        entry_price,
        current_price,
        unrealized_pnl,
        realized_pnl,
        liquidation_price,
        status.as_deref(),
        closed_at.as_deref(),
    ).map_err(|e| e.to_string())?;
    Ok("Position updated successfully".to_string())
}

#[tauri::command]
pub async fn db_delete_position(id: String) -> Result<String, String> {
    paper_trading::delete_position(&id).map_err(|e| e.to_string())?;
    Ok("Position deleted successfully".to_string())
}

#[tauri::command]
pub async fn db_get_order(id: String) -> Result<paper_trading::PaperTradingOrder, String> {
    paper_trading::get_order(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_pending_orders(portfolio_id: Option<String>) -> Result<Vec<paper_trading::PaperTradingOrder>, String> {
    paper_trading::get_pending_orders(portfolio_id.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_update_order(
    id: String,
    filled_quantity: Option<f64>,
    avg_fill_price: Option<f64>,
    status: Option<String>,
    filled_at: Option<String>,
) -> Result<String, String> {
    paper_trading::update_order(
        &id,
        filled_quantity,
        avg_fill_price,
        status.as_deref(),
        filled_at.as_deref(),
    ).map_err(|e| e.to_string())?;
    Ok("Order updated successfully".to_string())
}

#[tauri::command]
pub async fn db_delete_order(id: String) -> Result<String, String> {
    paper_trading::delete_order(&id).map_err(|e| e.to_string())?;
    Ok("Order deleted successfully".to_string())
}

#[tauri::command]
pub async fn db_create_trade(
    id: String,
    portfolio_id: String,
    order_id: String,
    symbol: String,
    side: String,
    price: f64,
    quantity: f64,
    fee: f64,
    fee_rate: f64,
    is_maker: bool,
) -> Result<String, String> {
    paper_trading::create_trade(
        &id,
        &portfolio_id,
        &order_id,
        &symbol,
        &side,
        price,
        quantity,
        fee,
        fee_rate,
        is_maker,
    ).map_err(|e| e.to_string())?;
    Ok("Trade created successfully".to_string())
}

#[tauri::command]
pub async fn db_get_trade(id: String) -> Result<paper_trading::PaperTradingTrade, String> {
    paper_trading::get_trade(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_get_order_trades(order_id: String) -> Result<Vec<paper_trading::PaperTradingTrade>, String> {
    paper_trading::get_order_trades(&order_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn db_delete_trade(id: String) -> Result<String, String> {
    paper_trading::delete_trade(&id).map_err(|e| e.to_string())?;
    Ok("Trade deleted successfully".to_string())
}

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
