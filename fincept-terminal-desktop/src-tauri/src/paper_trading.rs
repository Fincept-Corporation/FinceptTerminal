// Universal Paper Trading Module
// Asset-agnostic: works with crypto, stocks, futures, options, forex

use anyhow::{Result, bail};
use rusqlite::params;
use serde::{Deserialize, Serialize};
use uuid::Uuid;
use chrono::Utc;
use crate::database::pool::get_pool;

// ============================================================================
// Core Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Portfolio {
    pub id: String,
    pub name: String,
    pub initial_balance: f64,
    pub balance: f64,
    pub currency: String,
    pub leverage: f64,
    pub margin_mode: String, // "cross" | "isolated"
    pub fee_rate: f64,
    pub created_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Order {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub side: String,           // "buy" | "sell"
    pub order_type: String,     // "market" | "limit" | "stop" | "stop_limit"
    pub quantity: f64,
    pub price: Option<f64>,
    pub stop_price: Option<f64>,
    pub filled_qty: f64,
    pub avg_price: Option<f64>,
    pub status: String,         // "pending" | "filled" | "partial" | "cancelled"
    pub reduce_only: bool,
    pub created_at: String,
    pub filled_at: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Position {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub side: String,           // "long" | "short"
    pub quantity: f64,
    pub entry_price: f64,
    pub current_price: f64,
    pub unrealized_pnl: f64,
    pub realized_pnl: f64,
    pub leverage: f64,
    pub liquidation_price: Option<f64>,
    pub opened_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Trade {
    pub id: String,
    pub portfolio_id: String,
    pub order_id: String,
    pub symbol: String,
    pub side: String,
    pub price: f64,
    pub quantity: f64,
    pub fee: f64,
    pub pnl: f64,
    pub timestamp: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Stats {
    pub total_pnl: f64,
    pub win_rate: f64,
    pub total_trades: i64,
    pub winning_trades: i64,
    pub losing_trades: i64,
    pub largest_win: f64,
    pub largest_loss: f64,
}

// ============================================================================
// Database Schema
// ============================================================================

pub fn init_tables() -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute_batch(
        "CREATE TABLE IF NOT EXISTS pt_portfolios (
            id TEXT PRIMARY KEY,
            name TEXT NOT NULL,
            initial_balance REAL NOT NULL,
            balance REAL NOT NULL,
            currency TEXT DEFAULT 'USD',
            leverage REAL DEFAULT 1.0,
            margin_mode TEXT DEFAULT 'cross',
            fee_rate REAL DEFAULT 0.001,
            created_at TEXT NOT NULL
        );

        CREATE TABLE IF NOT EXISTS pt_orders (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            order_type TEXT NOT NULL,
            quantity REAL NOT NULL,
            price REAL,
            stop_price REAL,
            filled_qty REAL DEFAULT 0,
            avg_price REAL,
            status TEXT DEFAULT 'pending',
            reduce_only INTEGER DEFAULT 0,
            created_at TEXT NOT NULL,
            filled_at TEXT,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id)
        );

        CREATE TABLE IF NOT EXISTS pt_positions (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            quantity REAL NOT NULL,
            entry_price REAL NOT NULL,
            current_price REAL NOT NULL,
            unrealized_pnl REAL DEFAULT 0,
            realized_pnl REAL DEFAULT 0,
            leverage REAL DEFAULT 1.0,
            liquidation_price REAL,
            opened_at TEXT NOT NULL,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
            UNIQUE(portfolio_id, symbol, side)
        );

        CREATE TABLE IF NOT EXISTS pt_trades (
            id TEXT PRIMARY KEY,
            portfolio_id TEXT NOT NULL,
            order_id TEXT NOT NULL,
            symbol TEXT NOT NULL,
            side TEXT NOT NULL,
            price REAL NOT NULL,
            quantity REAL NOT NULL,
            fee REAL NOT NULL,
            pnl REAL DEFAULT 0,
            timestamp TEXT NOT NULL,
            FOREIGN KEY (portfolio_id) REFERENCES pt_portfolios(id),
            FOREIGN KEY (order_id) REFERENCES pt_orders(id)
        );

        CREATE INDEX IF NOT EXISTS idx_orders_portfolio ON pt_orders(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_positions_portfolio ON pt_positions(portfolio_id);
        CREATE INDEX IF NOT EXISTS idx_trades_portfolio ON pt_trades(portfolio_id);"
    )?;

    Ok(())
}

// ============================================================================
// Portfolio Operations
// ============================================================================

pub fn create_portfolio(name: &str, balance: f64, currency: &str, leverage: f64, margin_mode: &str, fee_rate: f64) -> Result<Portfolio> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let id = Uuid::new_v4().to_string();
    let now = Utc::now().to_rfc3339();

    conn.execute(
        "INSERT INTO pt_portfolios (id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, created_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
        params![id, name, balance, balance, currency, leverage, margin_mode, fee_rate, now],
    )?;

    Ok(Portfolio { id, name: name.to_string(), initial_balance: balance, balance, currency: currency.to_string(), leverage, margin_mode: margin_mode.to_string(), fee_rate, created_at: now })
}

pub fn get_portfolio(id: &str) -> Result<Portfolio> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.query_row(
        "SELECT id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, created_at FROM pt_portfolios WHERE id = ?1",
        params![id],
        |row| Ok(Portfolio {
            id: row.get(0)?, name: row.get(1)?, initial_balance: row.get(2)?, balance: row.get(3)?,
            currency: row.get(4)?, leverage: row.get(5)?, margin_mode: row.get(6)?, fee_rate: row.get(7)?, created_at: row.get(8)?
        })
    ).map_err(|e| anyhow::anyhow!("Portfolio not found: {}", e))
}

pub fn list_portfolios() -> Result<Vec<Portfolio>> {
    let pool = get_pool()?;
    let conn = pool.get()?;
    let mut stmt = conn.prepare("SELECT id, name, initial_balance, balance, currency, leverage, margin_mode, fee_rate, created_at FROM pt_portfolios")?;
    let rows = stmt.query_map([], |row| Ok(Portfolio {
        id: row.get(0)?, name: row.get(1)?, initial_balance: row.get(2)?, balance: row.get(3)?,
        currency: row.get(4)?, leverage: row.get(5)?, margin_mode: row.get(6)?, fee_rate: row.get(7)?, created_at: row.get(8)?
    }))?;
    rows.collect::<std::result::Result<Vec<_>, _>>().map_err(|e| anyhow::anyhow!(e))
}

pub fn delete_portfolio(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;
    conn.execute("DELETE FROM pt_trades WHERE portfolio_id = ?1", params![id])?;
    conn.execute("DELETE FROM pt_orders WHERE portfolio_id = ?1", params![id])?;
    conn.execute("DELETE FROM pt_positions WHERE portfolio_id = ?1", params![id])?;
    conn.execute("DELETE FROM pt_portfolios WHERE id = ?1", params![id])?;
    Ok(())
}

pub fn reset_portfolio(id: &str) -> Result<Portfolio> {
    let _portfolio = get_portfolio(id)?;
    let pool = get_pool()?;
    let conn = pool.get()?;
    conn.execute("DELETE FROM pt_trades WHERE portfolio_id = ?1", params![id])?;
    conn.execute("DELETE FROM pt_orders WHERE portfolio_id = ?1", params![id])?;
    conn.execute("DELETE FROM pt_positions WHERE portfolio_id = ?1", params![id])?;
    conn.execute("UPDATE pt_portfolios SET balance = initial_balance WHERE id = ?1", params![id])?;
    get_portfolio(id)
}

// ============================================================================
// Order Operations
// ============================================================================

pub fn place_order(portfolio_id: &str, symbol: &str, side: &str, order_type: &str, quantity: f64, price: Option<f64>, stop_price: Option<f64>, reduce_only: bool) -> Result<Order> {
    let portfolio = get_portfolio(portfolio_id)?;

    // Validate order_type
    let valid_order_types = ["market", "limit", "stop", "stop_limit"];
    if !valid_order_types.contains(&order_type) {
        bail!("Invalid order type: {}. Must be one of: market, limit, stop, stop_limit", order_type);
    }

    // Validate side
    if side != "buy" && side != "sell" {
        bail!("Invalid side: {}. Must be 'buy' or 'sell'", side);
    }

    // Validate quantity
    if !quantity.is_finite() || quantity <= 0.0 { bail!("Invalid quantity"); }

    // Validate price fields
    if let Some(p) = price { if !p.is_finite() || p <= 0.0 { bail!("Invalid price"); } }
    if let Some(sp) = stop_price { if !sp.is_finite() || sp <= 0.0 { bail!("Invalid stop price"); } }

    if order_type == "limit" && price.is_none() { bail!("Limit order requires price"); }
    if (order_type == "stop" || order_type == "stop_limit") && stop_price.is_none() { bail!("Stop order requires stop_price"); }

    // Check margin for new positions
    if !reduce_only {
        let reference_price = price.or(stop_price).unwrap_or(0.0);
        if reference_price <= 0.0 {
            bail!("Market orders require a reference price for margin calculation");
        }
        let required_margin = quantity * reference_price / portfolio.leverage;
        if required_margin > portfolio.balance { bail!("Insufficient margin"); }
    }

    let pool = get_pool()?;
    let conn = pool.get()?;

    let id = Uuid::new_v4().to_string();
    let now = Utc::now().to_rfc3339();

    conn.execute(
        "INSERT INTO pt_orders (id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, reduce_only, created_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
        params![id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, reduce_only, now],
    )?;

    Ok(Order { id, portfolio_id: portfolio_id.to_string(), symbol: symbol.to_string(), side: side.to_string(), order_type: order_type.to_string(), quantity, price, stop_price, filled_qty: 0.0, avg_price: None, status: "pending".to_string(), reduce_only, created_at: now, filled_at: None })
}

pub fn cancel_order(order_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;
    conn.execute("UPDATE pt_orders SET status = 'cancelled' WHERE id = ?1 AND status = 'pending'", params![order_id])?;
    Ok(())
}

pub fn get_orders(portfolio_id: &str, status: Option<&str>) -> Result<Vec<Order>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let (sql, query_params): (String, Vec<Box<dyn rusqlite::types::ToSql>>) = match status {
        Some(s) => (
            "SELECT id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, filled_qty, avg_price, status, reduce_only, created_at, filled_at FROM pt_orders WHERE portfolio_id = ?1 AND status = ?2".to_string(),
            vec![Box::new(portfolio_id.to_string()) as Box<dyn rusqlite::types::ToSql>, Box::new(s.to_string())],
        ),
        None => (
            "SELECT id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, filled_qty, avg_price, status, reduce_only, created_at, filled_at FROM pt_orders WHERE portfolio_id = ?1".to_string(),
            vec![Box::new(portfolio_id.to_string()) as Box<dyn rusqlite::types::ToSql>],
        ),
    };

    let mut stmt = conn.prepare(&sql)?;
    let params_refs: Vec<&dyn rusqlite::types::ToSql> = query_params.iter().map(|p| p.as_ref()).collect();
    let rows = stmt.query_map(params_refs.as_slice(), |row| Ok(Order {
        id: row.get(0)?, portfolio_id: row.get(1)?, symbol: row.get(2)?, side: row.get(3)?, order_type: row.get(4)?,
        quantity: row.get(5)?, price: row.get(6)?, stop_price: row.get(7)?, filled_qty: row.get(8)?, avg_price: row.get(9)?,
        status: row.get(10)?, reduce_only: row.get(11)?, created_at: row.get(12)?, filled_at: row.get(13)?
    }))?;
    rows.collect::<std::result::Result<Vec<_>, _>>().map_err(|e| anyhow::anyhow!(e))
}

// ============================================================================
// Fill Order & Position Management
// ============================================================================

pub fn fill_order(order_id: &str, fill_price: f64, fill_qty: Option<f64>) -> Result<Trade> {
    if !fill_price.is_finite() || fill_price <= 0.0 {
        bail!("Invalid fill price");
    }
    if let Some(fq) = fill_qty {
        if !fq.is_finite() || fq <= 0.0 {
            bail!("Invalid fill quantity");
        }
    }

    let pool = get_pool()?;
    let mut conn = pool.get()?;
    let tx = conn.transaction()?;

    // Get order
    let order: Order = tx.query_row(
        "SELECT id, portfolio_id, symbol, side, order_type, quantity, price, stop_price, filled_qty, avg_price, status, reduce_only, created_at, filled_at FROM pt_orders WHERE id = ?1",
        params![order_id],
        |row| Ok(Order { id: row.get(0)?, portfolio_id: row.get(1)?, symbol: row.get(2)?, side: row.get(3)?, order_type: row.get(4)?, quantity: row.get(5)?, price: row.get(6)?, stop_price: row.get(7)?, filled_qty: row.get(8)?, avg_price: row.get(9)?, status: row.get(10)?, reduce_only: row.get(11)?, created_at: row.get(12)?, filled_at: row.get(13)? })
    )?;

    if order.status != "pending" && order.status != "partial" { bail!("Order not fillable"); }

    let portfolio = get_portfolio(&order.portfolio_id)?;
    let qty = fill_qty.unwrap_or(order.quantity - order.filled_qty);
    let fee = qty * fill_price * portfolio.fee_rate;
    let now = Utc::now().to_rfc3339();

    // Update or create position
    let position_side = if order.side == "buy" { "long" } else { "short" };
    let opposite_side = if order.side == "buy" { "short" } else { "long" };
    let mut pnl = 0.0;

    // Check if closing opposite position
    let existing: Option<Position> = tx.query_row(
        "SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at FROM pt_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND side = ?3",
        params![order.portfolio_id, order.symbol, opposite_side],
        |row| Ok(Position { id: row.get(0)?, portfolio_id: row.get(1)?, symbol: row.get(2)?, side: row.get(3)?, quantity: row.get(4)?, entry_price: row.get(5)?, current_price: row.get(6)?, unrealized_pnl: row.get(7)?, realized_pnl: row.get(8)?, leverage: row.get(9)?, liquidation_price: row.get(10)?, opened_at: row.get(11)? })
    ).ok();

    if let Some(pos) = existing {
        // Closing/reducing opposite position
        let close_qty = qty.min(pos.quantity);
        pnl = if pos.side == "long" { (fill_price - pos.entry_price) * close_qty } else { (pos.entry_price - fill_price) * close_qty };

        if close_qty >= pos.quantity {
            tx.execute("DELETE FROM pt_positions WHERE id = ?1", params![pos.id])?;
        } else {
            tx.execute("UPDATE pt_positions SET quantity = quantity - ?1, realized_pnl = realized_pnl + ?2 WHERE id = ?3", params![close_qty, pnl, pos.id])?;
        }

        // Open remaining as new position if qty > close_qty (flip position)
        if qty > close_qty {
            let new_qty = qty - close_qty;
            let pos_id = Uuid::new_v4().to_string();
            tx.execute(
                "INSERT INTO pt_positions (id, portfolio_id, symbol, side, quantity, entry_price, current_price, leverage, opened_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
                params![pos_id, order.portfolio_id, order.symbol, position_side, new_qty, fill_price, fill_price, portfolio.leverage, now]
            )?;
        }
    } else {
        // Check for same-side position to add to
        let same_pos: Option<Position> = tx.query_row(
            "SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at FROM pt_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND side = ?3",
            params![order.portfolio_id, order.symbol, position_side],
            |row| Ok(Position { id: row.get(0)?, portfolio_id: row.get(1)?, symbol: row.get(2)?, side: row.get(3)?, quantity: row.get(4)?, entry_price: row.get(5)?, current_price: row.get(6)?, unrealized_pnl: row.get(7)?, realized_pnl: row.get(8)?, leverage: row.get(9)?, liquidation_price: row.get(10)?, opened_at: row.get(11)? })
        ).ok();

        if let Some(pos) = same_pos {
            // Average into existing position
            let new_qty = pos.quantity + qty;
            let new_entry = (pos.entry_price * pos.quantity + fill_price * qty) / new_qty;
            tx.execute("UPDATE pt_positions SET quantity = ?1, entry_price = ?2, current_price = ?3 WHERE id = ?4", params![new_qty, new_entry, fill_price, pos.id])?;
        } else {
            // Brand new position
            let pos_id = Uuid::new_v4().to_string();
            tx.execute(
                "INSERT INTO pt_positions (id, portfolio_id, symbol, side, quantity, entry_price, current_price, leverage, opened_at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9)",
                params![pos_id, order.portfolio_id, order.symbol, position_side, qty, fill_price, fill_price, portfolio.leverage, now]
            )?;
        }
    }

    // Update wallet balance (industry standard - Binance/Bybit style):
    // Wallet balance only changes on: realized P&L and fees
    // Opening positions does NOT affect wallet balance (margin is reserved from available balance)
    // Closing positions adds/subtracts realized P&L minus fees
    let balance_change = pnl - fee;
    tx.execute("UPDATE pt_portfolios SET balance = balance + ?1 WHERE id = ?2", params![balance_change, order.portfolio_id])?;

    // Update order
    let new_filled = order.filled_qty + qty;
    let new_status = if new_filled >= order.quantity { "filled" } else { "partial" };
    let new_avg = Some((order.avg_price.unwrap_or(0.0) * order.filled_qty + fill_price * qty) / new_filled);
    tx.execute("UPDATE pt_orders SET filled_qty = ?1, avg_price = ?2, status = ?3, filled_at = ?4 WHERE id = ?5", params![new_filled, new_avg, new_status, now, order_id])?;

    // Create trade
    let trade_id = Uuid::new_v4().to_string();
    tx.execute(
        "INSERT INTO pt_trades (id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
        params![trade_id, order.portfolio_id, order_id, order.symbol, order.side, fill_price, qty, fee, pnl, now]
    )?;

    let trade = Trade { id: trade_id, portfolio_id: order.portfolio_id, order_id: order_id.to_string(), symbol: order.symbol, side: order.side, price: fill_price, quantity: qty, fee, pnl, timestamp: now };
    tx.commit()?;
    Ok(trade)
}

// ============================================================================
// Position & Trade Queries
// ============================================================================

pub fn get_positions(portfolio_id: &str) -> Result<Vec<Position>> {
    let pool = get_pool()?;
    let conn = pool.get()?;
    let mut stmt = conn.prepare("SELECT id, portfolio_id, symbol, side, quantity, entry_price, current_price, unrealized_pnl, realized_pnl, leverage, liquidation_price, opened_at FROM pt_positions WHERE portfolio_id = ?1")?;
    let rows = stmt.query_map(params![portfolio_id], |row| Ok(Position {
        id: row.get(0)?, portfolio_id: row.get(1)?, symbol: row.get(2)?, side: row.get(3)?, quantity: row.get(4)?,
        entry_price: row.get(5)?, current_price: row.get(6)?, unrealized_pnl: row.get(7)?, realized_pnl: row.get(8)?,
        leverage: row.get(9)?, liquidation_price: row.get(10)?, opened_at: row.get(11)?
    }))?;
    rows.collect::<std::result::Result<Vec<_>, _>>().map_err(|e| anyhow::anyhow!(e))
}

pub fn update_position_price(portfolio_id: &str, symbol: &str, price: f64) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE pt_positions SET current_price = ?1, unrealized_pnl = CASE WHEN side = 'long' THEN (?1 - entry_price) * quantity ELSE (entry_price - ?1) * quantity END WHERE portfolio_id = ?2 AND symbol = ?3",
        params![price, portfolio_id, symbol]
    )?;
    Ok(())
}

pub fn get_trades(portfolio_id: &str, limit: i64) -> Result<Vec<Trade>> {
    let pool = get_pool()?;
    let conn = pool.get()?;
    let mut stmt = conn.prepare("SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, pnl, timestamp FROM pt_trades WHERE portfolio_id = ?1 ORDER BY timestamp DESC LIMIT ?2")?;
    let rows = stmt.query_map(params![portfolio_id, limit], |row| Ok(Trade {
        id: row.get(0)?, portfolio_id: row.get(1)?, order_id: row.get(2)?, symbol: row.get(3)?, side: row.get(4)?,
        price: row.get(5)?, quantity: row.get(6)?, fee: row.get(7)?, pnl: row.get(8)?, timestamp: row.get(9)?
    }))?;
    rows.collect::<std::result::Result<Vec<_>, _>>().map_err(|e| anyhow::anyhow!(e))
}

pub fn get_stats(portfolio_id: &str) -> Result<Stats> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let (total_pnl, total_trades, winning, losing, max_win, max_loss): (f64, i64, i64, i64, f64, f64) = conn.query_row(
        "SELECT COALESCE(SUM(pnl), 0), COUNT(*), COALESCE(SUM(CASE WHEN pnl > 0 THEN 1 ELSE 0 END), 0), COALESCE(SUM(CASE WHEN pnl < 0 THEN 1 ELSE 0 END), 0), COALESCE(MAX(pnl), 0), COALESCE(MIN(pnl), 0) FROM pt_trades WHERE portfolio_id = ?1",
        params![portfolio_id],
        |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?, row.get(3)?, row.get(4)?, row.get(5)?))
    )?;

    let win_rate = if total_trades > 0 { (winning as f64 / total_trades as f64) * 100.0 } else { 0.0 };

    Ok(Stats { total_pnl, win_rate, total_trades, winning_trades: winning, losing_trades: losing, largest_win: max_win, largest_loss: max_loss })
}

// ============================================================================
// Tauri Commands
// ============================================================================

#[tauri::command]
pub fn pt_create_portfolio(name: String, balance: f64, currency: String, leverage: f64, margin_mode: String, fee_rate: f64) -> Result<Portfolio, String> {
    create_portfolio(&name, balance, &currency, leverage, &margin_mode, fee_rate).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_get_portfolio(id: String) -> Result<Portfolio, String> {
    get_portfolio(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_list_portfolios() -> Result<Vec<Portfolio>, String> {
    list_portfolios().map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_delete_portfolio(id: String) -> Result<(), String> {
    delete_portfolio(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_reset_portfolio(id: String) -> Result<Portfolio, String> {
    reset_portfolio(&id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_place_order(portfolio_id: String, symbol: String, side: String, order_type: String, quantity: f64, price: Option<f64>, stop_price: Option<f64>, reduce_only: bool) -> Result<Order, String> {
    place_order(&portfolio_id, &symbol, &side, &order_type, quantity, price, stop_price, reduce_only).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_cancel_order(order_id: String) -> Result<(), String> {
    cancel_order(&order_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_get_orders(portfolio_id: String, status: Option<String>) -> Result<Vec<Order>, String> {
    get_orders(&portfolio_id, status.as_deref()).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_fill_order(order_id: String, fill_price: f64, fill_qty: Option<f64>) -> Result<Trade, String> {
    fill_order(&order_id, fill_price, fill_qty).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_get_positions(portfolio_id: String) -> Result<Vec<Position>, String> {
    get_positions(&portfolio_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_update_price(portfolio_id: String, symbol: String, price: f64) -> Result<(), String> {
    update_position_price(&portfolio_id, &symbol, price).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_get_trades(portfolio_id: String, limit: i64) -> Result<Vec<Trade>, String> {
    get_trades(&portfolio_id, limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub fn pt_get_stats(portfolio_id: String) -> Result<Stats, String> {
    get_stats(&portfolio_id).map_err(|e| e.to_string())
}
