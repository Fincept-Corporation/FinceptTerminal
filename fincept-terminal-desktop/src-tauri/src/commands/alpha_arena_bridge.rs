/**
 * Alpha Arena Bridge Commands
 * Tauri commands to connect Alpha Arena Python service with Rust paper trading
 */

use tauri::command;
use serde::{Deserialize, Serialize};
use crate::database::paper_trading;

#[derive(Debug, Serialize, Deserialize)]
pub struct AlphaArenaDecision {
    pub competition_id: String,
    pub model_name: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub side: String,
    pub quantity: f64,
    pub price: f64,
    pub confidence: f64,
    pub reasoning: String,
    pub order_id: String,
    pub fee: f64,
    pub timestamp: String,
}

/// Create paper trading portfolio for Alpha Arena model
#[command]
pub async fn paper_trading_create_portfolio(
    id: String,
    name: String,
    provider: String,
    initial_balance: f64,
    currency: String,
    margin_mode: String,
    leverage: f64,
) -> Result<serde_json::Value, String> {
    paper_trading::create_portfolio(
        &id,
        &name,
        &provider,
        initial_balance,
        &currency,
        &margin_mode,
        leverage,
    )
    .map(|portfolio| serde_json::to_value(portfolio).unwrap())
    .map_err(|e| e.to_string())
}

/// Get paper trading portfolio
#[command]
pub async fn paper_trading_get_portfolio(id: String) -> Result<serde_json::Value, String> {
    paper_trading::get_portfolio(&id)
        .map(|portfolio| serde_json::to_value(portfolio).unwrap())
        .map_err(|e| e.to_string())
}

/// Update portfolio balance
#[command]
pub async fn paper_trading_update_balance(id: String, new_balance: f64) -> Result<(), String> {
    paper_trading::update_portfolio_balance(&id, new_balance).map_err(|e| e.to_string())
}

/// Get positions for portfolio
#[command]
pub async fn paper_trading_get_positions(
    portfolio_id: String,
    status: Option<String>,
) -> Result<serde_json::Value, String> {
    let status_ref = status.as_deref();
    paper_trading::get_portfolio_positions(&portfolio_id, status_ref)
        .map(|positions| serde_json::to_value(positions).unwrap())
        .map_err(|e| e.to_string())
}

/// Create position
#[command]
pub async fn paper_trading_create_position(
    id: String,
    portfolio_id: String,
    symbol: String,
    side: String,
    entry_price: f64,
    quantity: f64,
    leverage: f64,
    margin_mode: String,
) -> Result<(), String> {
    paper_trading::create_position(
        &id,
        &portfolio_id,
        &symbol,
        &side,
        entry_price,
        quantity,
        leverage,
        &margin_mode,
    )
    .map_err(|e| e.to_string())
}

/// Update position
#[command]
pub async fn paper_trading_update_position(
    id: String,
    quantity: Option<f64>,
    entry_price: Option<f64>,
    current_price: Option<f64>,
    unrealized_pnl: Option<f64>,
    realized_pnl: Option<f64>,
    liquidation_price: Option<f64>,
    status: Option<String>,
    closed_at: Option<String>,
) -> Result<(), String> {
    let status_ref = status.as_deref();
    let closed_at_ref = closed_at.as_deref();

    paper_trading::update_position(
        &id,
        quantity,
        entry_price,
        current_price,
        unrealized_pnl,
        realized_pnl,
        liquidation_price,
        status_ref,
        closed_at_ref,
    )
    .map_err(|e| e.to_string())
}

/// Create order
#[command]
pub async fn paper_trading_create_order(
    id: String,
    portfolio_id: String,
    symbol: String,
    side: String,
    order_type: String,
    quantity: f64,
    price: Option<f64>,
    time_in_force: String,
) -> Result<(), String> {
    paper_trading::create_order(
        &id,
        &portfolio_id,
        &symbol,
        &side,
        &order_type,
        quantity,
        price,
        &time_in_force,
    )
    .map_err(|e| e.to_string())
}

/// Get portfolio orders
#[command]
pub async fn paper_trading_get_orders(
    portfolio_id: String,
    status: Option<String>,
) -> Result<serde_json::Value, String> {
    let status_ref = status.as_deref();
    paper_trading::get_portfolio_orders(&portfolio_id, status_ref)
        .map(|orders| serde_json::to_value(orders).unwrap())
        .map_err(|e| e.to_string())
}

/// Update order
#[command]
pub async fn paper_trading_update_order(
    id: String,
    filled_quantity: Option<f64>,
    avg_fill_price: Option<f64>,
    status: Option<String>,
    filled_at: Option<String>,
) -> Result<(), String> {
    let status_ref = status.as_deref();
    let filled_at_ref = filled_at.as_deref();

    paper_trading::update_order(&id, filled_quantity, avg_fill_price, status_ref, filled_at_ref)
        .map_err(|e| e.to_string())
}

/// Create trade record
#[command]
pub async fn paper_trading_create_trade(
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
) -> Result<(), String> {
    paper_trading::create_trade(
        &id, &portfolio_id, &order_id, &symbol, &side, price, quantity, fee, fee_rate, is_maker,
    )
    .map_err(|e| e.to_string())
}

/// Get portfolio trades
#[command]
pub async fn paper_trading_get_trades(
    portfolio_id: String,
    limit: Option<i64>,
) -> Result<serde_json::Value, String> {
    paper_trading::get_portfolio_trades(&portfolio_id, limit)
        .map(|trades| serde_json::to_value(trades).unwrap())
        .map_err(|e| e.to_string())
}

/// Record Alpha Arena decision (stores in SQLite for UI display)
/// This is separate from paper trading tables - it's for Alpha Arena specific data
#[command]
pub async fn alpha_arena_record_decision(
    competition_id: String,
    model_name: String,
    _portfolio_id: String,
    symbol: String,
    side: String,
    quantity: f64,
    price: f64,
    confidence: f64,
    reasoning: String,
    _order_id: String,
    _fee: f64,
    timestamp: String,
) -> Result<(), String> {
    use crate::database::pool::get_pool;
    use rusqlite::params;

    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO alpha_decision_logs
         (id, competition_id, model_name, cycle_number, action, symbol, quantity, confidence, reasoning,
          trade_executed, price_at_decision, portfolio_value_before, portfolio_value_after, timestamp)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14)",
        params![
            format!("{}-{}", competition_id, chrono::Utc::now().timestamp_millis()),
            competition_id,
            model_name,
            0, // cycle_number - will be updated by Python service
            side.to_uppercase(),
            symbol,
            quantity,
            confidence,
            reasoning,
            true, // trade_executed
            price,
            0.0, // portfolio_value_before - will be calculated
            0.0, // portfolio_value_after - will be calculated
            timestamp,
        ],
    )
    .map_err(|e| e.to_string())?;

    Ok(())
}
