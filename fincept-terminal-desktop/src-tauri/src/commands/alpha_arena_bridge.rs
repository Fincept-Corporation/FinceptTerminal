/**
 * Alpha Arena Bridge Commands
 * Tauri commands to connect Alpha Arena Python service with Rust paper trading
 */

use tauri::command;
use serde::{Deserialize, Serialize};
use crate::paper_trading;

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
    name: String,
    initial_balance: f64,
    currency: String,
    margin_mode: String,
    leverage: f64,
    fee_rate: f64,
) -> Result<serde_json::Value, String> {
    paper_trading::create_portfolio(&name, initial_balance, &currency, leverage, &margin_mode, fee_rate)
        .map(|p| serde_json::to_value(p).unwrap())
        .map_err(|e| e.to_string())
}

/// Get paper trading portfolio
#[command]
pub async fn paper_trading_get_portfolio(id: String) -> Result<serde_json::Value, String> {
    paper_trading::get_portfolio(&id)
        .map(|p| serde_json::to_value(p).unwrap())
        .map_err(|e| e.to_string())
}

/// Get positions for portfolio
#[command]
pub async fn paper_trading_get_positions(portfolio_id: String) -> Result<serde_json::Value, String> {
    paper_trading::get_positions(&portfolio_id)
        .map(|p| serde_json::to_value(p).unwrap())
        .map_err(|e| e.to_string())
}

/// Place order
#[command]
pub async fn paper_trading_place_order(
    portfolio_id: String,
    symbol: String,
    side: String,
    order_type: String,
    quantity: f64,
    price: Option<f64>,
) -> Result<serde_json::Value, String> {
    paper_trading::place_order(&portfolio_id, &symbol, &side, &order_type, quantity, price, None, false)
        .map(|o| serde_json::to_value(o).unwrap())
        .map_err(|e| e.to_string())
}

/// Fill order at price
#[command]
pub async fn paper_trading_fill_order(
    order_id: String,
    fill_price: f64,
) -> Result<serde_json::Value, String> {
    paper_trading::fill_order(&order_id, fill_price, None)
        .map(|t| serde_json::to_value(t).unwrap())
        .map_err(|e| e.to_string())
}

/// Get portfolio orders
#[command]
pub async fn paper_trading_get_orders(
    portfolio_id: String,
    status: Option<String>,
) -> Result<serde_json::Value, String> {
    paper_trading::get_orders(&portfolio_id, status.as_deref())
        .map(|o| serde_json::to_value(o).unwrap())
        .map_err(|e| e.to_string())
}

/// Get portfolio trades
#[command]
pub async fn paper_trading_get_trades(
    portfolio_id: String,
    limit: Option<i64>,
) -> Result<serde_json::Value, String> {
    paper_trading::get_trades(&portfolio_id, limit.unwrap_or(100))
        .map(|t| serde_json::to_value(t).unwrap())
        .map_err(|e| e.to_string())
}

/// Record Alpha Arena decision (stores in SQLite for UI display)
#[command]
pub async fn alpha_arena_record_decision(
    competition_id: String,
    model_name: String,
    symbol: String,
    side: String,
    quantity: f64,
    price: f64,
    confidence: f64,
    reasoning: String,
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
            0,
            side.to_uppercase(),
            symbol,
            quantity,
            confidence,
            reasoning,
            true,
            price,
            0.0,
            0.0,
            timestamp,
        ],
    )
    .map_err(|e| e.to_string())?;

    Ok(())
}
