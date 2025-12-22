// Paper Trading Operations - Portfolio, positions, orders, trades

use crate::database::pool::get_pool;
use anyhow::Result;
use rusqlite::params;
use serde::{Deserialize, Serialize};

// ============================================================================
// Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PaperTradingPortfolio {
    pub id: String,
    pub name: String,
    pub provider: String,
    pub initial_balance: f64,
    pub current_balance: f64,
    pub currency: String,
    pub margin_mode: String,
    pub leverage: f64,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PaperTradingPosition {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub side: String,
    pub entry_price: f64,
    pub quantity: f64,
    pub position_value: Option<f64>,
    pub current_price: Option<f64>,
    pub unrealized_pnl: Option<f64>,
    pub realized_pnl: f64,
    pub leverage: f64,
    pub margin_mode: String,
    pub liquidation_price: Option<f64>,
    pub opened_at: String,
    pub closed_at: Option<String>,
    pub status: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PaperTradingOrder {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub side: String,
    pub order_type: String,
    pub quantity: f64,
    pub price: Option<f64>,
    pub stop_price: Option<f64>,
    pub filled_quantity: f64,
    pub avg_fill_price: Option<f64>,
    pub status: String,
    pub time_in_force: String,
    pub post_only: bool,
    pub reduce_only: bool,
    pub created_at: String,
    pub filled_at: Option<String>,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PaperTradingTrade {
    pub id: String,
    pub portfolio_id: String,
    pub order_id: String,
    pub symbol: String,
    pub side: String,
    pub price: f64,
    pub quantity: f64,
    pub fee: f64,
    pub fee_rate: f64,
    pub is_maker: bool,
    pub timestamp: String,
}

// ============================================================================
// Portfolio Operations
// ============================================================================

pub fn create_portfolio(
    id: &str,
    name: &str,
    provider: &str,
    initial_balance: f64,
    currency: &str,
    margin_mode: &str,
    leverage: f64,
) -> Result<PaperTradingPortfolio> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO paper_trading_portfolios
         (id, name, provider, initial_balance, current_balance, currency, margin_mode, leverage)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8)",
        params![id, name, provider, initial_balance, initial_balance, currency, margin_mode, leverage],
    )?;

    get_portfolio(id)
}

pub fn get_portfolio(id: &str) -> Result<PaperTradingPortfolio> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let portfolio = conn.query_row(
        "SELECT id, name, provider, initial_balance, current_balance, currency, margin_mode, leverage, created_at, updated_at
         FROM paper_trading_portfolios WHERE id = ?1",
        params![id],
        |row| {
            Ok(PaperTradingPortfolio {
                id: row.get(0)?,
                name: row.get(1)?,
                provider: row.get(2)?,
                initial_balance: row.get(3)?,
                current_balance: row.get(4)?,
                currency: row.get(5)?,
                margin_mode: row.get(6)?,
                leverage: row.get(7)?,
                created_at: row.get(8)?,
                updated_at: row.get(9)?,
            })
        },
    )?;

    Ok(portfolio)
}

pub fn update_portfolio_balance(id: &str, new_balance: f64) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE paper_trading_portfolios SET current_balance = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
        params![new_balance, id],
    )?;

    Ok(())
}

pub fn list_portfolios() -> Result<Vec<PaperTradingPortfolio>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, provider, initial_balance, current_balance, currency, margin_mode, leverage, created_at, updated_at
         FROM paper_trading_portfolios ORDER BY created_at DESC"
    )?;

    let portfolios = stmt
        .query_map([], |row| {
            Ok(PaperTradingPortfolio {
                id: row.get(0)?,
                name: row.get(1)?,
                provider: row.get(2)?,
                initial_balance: row.get(3)?,
                current_balance: row.get(4)?,
                currency: row.get(5)?,
                margin_mode: row.get(6)?,
                leverage: row.get(7)?,
                created_at: row.get(8)?,
                updated_at: row.get(9)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(portfolios)
}

pub fn delete_portfolio(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM paper_trading_portfolios WHERE id = ?1",
        params![id],
    )?;

    Ok(())
}

// ============================================================================
// Position Operations
// ============================================================================

pub fn create_position(
    id: &str,
    portfolio_id: &str,
    symbol: &str,
    side: &str,
    entry_price: f64,
    quantity: f64,
    leverage: f64,
    margin_mode: &str,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let position_value = entry_price * quantity;

    conn.execute(
        "INSERT INTO paper_trading_positions
         (id, portfolio_id, symbol, side, entry_price, quantity, position_value, leverage, margin_mode, status)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, 'open')",
        params![id, portfolio_id, symbol, side, entry_price, quantity, position_value, leverage, margin_mode],
    )?;

    Ok(())
}

pub fn get_portfolio_positions(portfolio_id: &str, status: Option<&str>) -> Result<Vec<PaperTradingPosition>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(st) = status {
        format!(
            "SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value, current_price,
                    unrealized_pnl, realized_pnl, leverage, margin_mode, liquidation_price, opened_at, closed_at, status
             FROM paper_trading_positions WHERE portfolio_id = ?1 AND status = '{}' ORDER BY opened_at DESC",
            st
        )
    } else {
        "SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value, current_price,
                unrealized_pnl, realized_pnl, leverage, margin_mode, liquidation_price, opened_at, closed_at, status
         FROM paper_trading_positions WHERE portfolio_id = ?1 ORDER BY opened_at DESC"
            .to_string()
    };

    let mut stmt = conn.prepare(&query)?;
    let positions = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(PaperTradingPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                entry_price: row.get(4)?,
                quantity: row.get(5)?,
                position_value: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                leverage: row.get(10)?,
                margin_mode: row.get(11)?,
                liquidation_price: row.get(12)?,
                opened_at: row.get(13)?,
                closed_at: row.get(14)?,
                status: row.get(15)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(positions)
}

pub fn get_position(id: &str) -> Result<PaperTradingPosition> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let position = conn.query_row(
        "SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value, current_price,
                unrealized_pnl, realized_pnl, leverage, margin_mode, liquidation_price, opened_at, closed_at, status
         FROM paper_trading_positions WHERE id = ?1",
        params![id],
        |row| {
            Ok(PaperTradingPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                entry_price: row.get(4)?,
                quantity: row.get(5)?,
                position_value: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                leverage: row.get(10)?,
                margin_mode: row.get(11)?,
                liquidation_price: row.get(12)?,
                opened_at: row.get(13)?,
                closed_at: row.get(14)?,
                status: row.get(15)?,
            })
        },
    )?;

    Ok(position)
}

pub fn get_position_by_symbol(portfolio_id: &str, symbol: &str, status: &str) -> Result<Option<PaperTradingPosition>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let position = conn.query_row(
        "SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value, current_price,
                unrealized_pnl, realized_pnl, leverage, margin_mode, liquidation_price, opened_at, closed_at, status
         FROM paper_trading_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND status = ?3
         ORDER BY opened_at DESC LIMIT 1",
        params![portfolio_id, symbol, status],
        |row| {
            Ok(PaperTradingPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                entry_price: row.get(4)?,
                quantity: row.get(5)?,
                position_value: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                leverage: row.get(10)?,
                margin_mode: row.get(11)?,
                liquidation_price: row.get(12)?,
                opened_at: row.get(13)?,
                closed_at: row.get(14)?,
                status: row.get(15)?,
            })
        },
    );

    match position {
        Ok(p) => Ok(Some(p)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}

pub fn get_position_by_symbol_and_side(
    portfolio_id: &str,
    symbol: &str,
    side: &str,
    status: &str,
) -> Result<Option<PaperTradingPosition>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let position = conn.query_row(
        "SELECT id, portfolio_id, symbol, side, entry_price, quantity, position_value, current_price,
                unrealized_pnl, realized_pnl, leverage, margin_mode, liquidation_price, opened_at, closed_at, status
         FROM paper_trading_positions WHERE portfolio_id = ?1 AND symbol = ?2 AND side = ?3 AND status = ?4
         ORDER BY opened_at DESC LIMIT 1",
        params![portfolio_id, symbol, side, status],
        |row| {
            Ok(PaperTradingPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                entry_price: row.get(4)?,
                quantity: row.get(5)?,
                position_value: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                leverage: row.get(10)?,
                margin_mode: row.get(11)?,
                liquidation_price: row.get(12)?,
                opened_at: row.get(13)?,
                closed_at: row.get(14)?,
                status: row.get(15)?,
            })
        },
    );

    match position {
        Ok(p) => Ok(Some(p)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}

pub fn update_position(
    id: &str,
    quantity: Option<f64>,
    entry_price: Option<f64>,
    current_price: Option<f64>,
    unrealized_pnl: Option<f64>,
    realized_pnl: Option<f64>,
    liquidation_price: Option<f64>,
    status: Option<&str>,
    closed_at: Option<&str>,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut updates = Vec::new();
    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();

    if let Some(q) = quantity {
        updates.push("quantity = ?".to_string());
        params_vec.push(Box::new(q));
    }
    if let Some(ep) = entry_price {
        updates.push("entry_price = ?".to_string());
        params_vec.push(Box::new(ep));
    }
    if let Some(cp) = current_price {
        updates.push("current_price = ?".to_string());
        params_vec.push(Box::new(cp));
    }
    if let Some(upnl) = unrealized_pnl {
        updates.push("unrealized_pnl = ?".to_string());
        params_vec.push(Box::new(upnl));
    }
    if let Some(rpnl) = realized_pnl {
        updates.push("realized_pnl = ?".to_string());
        params_vec.push(Box::new(rpnl));
    }
    if let Some(lp) = liquidation_price {
        updates.push("liquidation_price = ?".to_string());
        params_vec.push(Box::new(lp));
    }
    if let Some(st) = status {
        updates.push("status = ?".to_string());
        params_vec.push(Box::new(st.to_string()));
    }
    if let Some(ca) = closed_at {
        updates.push("closed_at = ?".to_string());
        params_vec.push(Box::new(ca.to_string()));
    }

    if updates.is_empty() {
        return Ok(());
    }

    params_vec.push(Box::new(id.to_string()));
    let sql = format!("UPDATE paper_trading_positions SET {} WHERE id = ?", updates.join(", "));

    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();
    conn.execute(&sql, params_refs.as_slice())?;

    Ok(())
}

pub fn delete_position(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM paper_trading_positions WHERE id = ?1",
        params![id],
    )?;

    Ok(())
}

// ============================================================================
// Order Operations
// ============================================================================

pub fn create_order(
    id: &str,
    portfolio_id: &str,
    symbol: &str,
    side: &str,
    order_type: &str,
    quantity: f64,
    price: Option<f64>,
    time_in_force: &str,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO paper_trading_orders
         (id, portfolio_id, symbol, side, type, quantity, price, status, time_in_force, filled_quantity)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 'pending', ?8, 0)",
        params![id, portfolio_id, symbol, side, order_type, quantity, price, time_in_force],
    )?;

    Ok(())
}

pub fn get_portfolio_orders(portfolio_id: &str, status: Option<&str>) -> Result<Vec<PaperTradingOrder>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(st) = status {
        format!(
            "SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price, filled_quantity,
                    avg_fill_price, status, time_in_force, post_only, reduce_only, created_at, filled_at, updated_at
             FROM paper_trading_orders WHERE portfolio_id = ?1 AND status = '{}' ORDER BY created_at DESC",
            st
        )
    } else {
        "SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price, filled_quantity,
                avg_fill_price, status, time_in_force, post_only, reduce_only, created_at, filled_at, updated_at
         FROM paper_trading_orders WHERE portfolio_id = ?1 ORDER BY created_at DESC"
            .to_string()
    };

    let mut stmt = conn.prepare(&query)?;
    let orders = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(PaperTradingOrder {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                order_type: row.get(4)?,
                quantity: row.get(5)?,
                price: row.get(6)?,
                stop_price: row.get(7)?,
                filled_quantity: row.get(8)?,
                avg_fill_price: row.get(9)?,
                status: row.get(10)?,
                time_in_force: row.get(11)?,
                post_only: row.get::<_, i32>(12)? != 0,
                reduce_only: row.get::<_, i32>(13)? != 0,
                created_at: row.get(14)?,
                filled_at: row.get(15)?,
                updated_at: row.get(16)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(orders)
}

#[allow(dead_code)]
pub fn update_order_status(id: &str, status: &str, filled_quantity: f64, avg_fill_price: Option<f64>) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE paper_trading_orders
         SET status = ?1, filled_quantity = ?2, avg_fill_price = ?3, updated_at = CURRENT_TIMESTAMP
         WHERE id = ?4",
        params![status, filled_quantity, avg_fill_price, id],
    )?;

    Ok(())
}

pub fn get_order(id: &str) -> Result<PaperTradingOrder> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let order = conn.query_row(
        "SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price, filled_quantity,
                avg_fill_price, status, time_in_force, post_only, reduce_only, created_at, filled_at, updated_at
         FROM paper_trading_orders WHERE id = ?1",
        params![id],
        |row| {
            Ok(PaperTradingOrder {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                order_type: row.get(4)?,
                quantity: row.get(5)?,
                price: row.get(6)?,
                stop_price: row.get(7)?,
                filled_quantity: row.get(8)?,
                avg_fill_price: row.get(9)?,
                status: row.get(10)?,
                time_in_force: row.get(11)?,
                post_only: row.get::<_, i32>(12)? != 0,
                reduce_only: row.get::<_, i32>(13)? != 0,
                created_at: row.get(14)?,
                filled_at: row.get(15)?,
                updated_at: row.get(16)?,
            })
        },
    )?;

    Ok(order)
}

pub fn get_pending_orders(portfolio_id: Option<&str>) -> Result<Vec<PaperTradingOrder>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    if let Some(pid) = portfolio_id {
        let mut stmt = conn.prepare(
            "SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price, filled_quantity,
                    avg_fill_price, status, time_in_force, post_only, reduce_only, created_at, filled_at, updated_at
             FROM paper_trading_orders WHERE status IN ('pending', 'triggered', 'partial') AND portfolio_id = ?1
             ORDER BY created_at ASC"
        )?;

        let orders = stmt.query_map(params![pid], |row| {
            Ok(PaperTradingOrder {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                order_type: row.get(4)?,
                quantity: row.get(5)?,
                price: row.get(6)?,
                stop_price: row.get(7)?,
                filled_quantity: row.get(8)?,
                avg_fill_price: row.get(9)?,
                status: row.get(10)?,
                time_in_force: row.get(11)?,
                post_only: row.get::<_, i32>(12)? != 0,
                reduce_only: row.get::<_, i32>(13)? != 0,
                created_at: row.get(14)?,
                filled_at: row.get(15)?,
                updated_at: row.get(16)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

        Ok(orders)
    } else {
        let mut stmt = conn.prepare(
            "SELECT id, portfolio_id, symbol, side, type, quantity, price, stop_price, filled_quantity,
                    avg_fill_price, status, time_in_force, post_only, reduce_only, created_at, filled_at, updated_at
             FROM paper_trading_orders WHERE status IN ('pending', 'triggered', 'partial')
             ORDER BY created_at ASC"
        )?;

        let orders = stmt.query_map([], |row| {
            Ok(PaperTradingOrder {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                side: row.get(3)?,
                order_type: row.get(4)?,
                quantity: row.get(5)?,
                price: row.get(6)?,
                stop_price: row.get(7)?,
                filled_quantity: row.get(8)?,
                avg_fill_price: row.get(9)?,
                status: row.get(10)?,
                time_in_force: row.get(11)?,
                post_only: row.get::<_, i32>(12)? != 0,
                reduce_only: row.get::<_, i32>(13)? != 0,
                created_at: row.get(14)?,
                filled_at: row.get(15)?,
                updated_at: row.get(16)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

        Ok(orders)
    }
}

pub fn update_order(
    id: &str,
    filled_quantity: Option<f64>,
    avg_fill_price: Option<f64>,
    status: Option<&str>,
    filled_at: Option<&str>,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut updates = vec!["updated_at = CURRENT_TIMESTAMP".to_string()];
    let mut params_vec: Vec<Box<dyn rusqlite::ToSql>> = Vec::new();

    if let Some(fq) = filled_quantity {
        updates.push("filled_quantity = ?".to_string());
        params_vec.push(Box::new(fq));
    }
    if let Some(afp) = avg_fill_price {
        updates.push("avg_fill_price = ?".to_string());
        params_vec.push(Box::new(afp));
    }
    if let Some(st) = status {
        updates.push("status = ?".to_string());
        params_vec.push(Box::new(st.to_string()));
    }
    if let Some(fa) = filled_at {
        updates.push("filled_at = ?".to_string());
        params_vec.push(Box::new(fa.to_string()));
    }

    params_vec.push(Box::new(id.to_string()));
    let sql = format!("UPDATE paper_trading_orders SET {} WHERE id = ?", updates.join(", "));

    let params_refs: Vec<&dyn rusqlite::ToSql> = params_vec.iter().map(|p| p.as_ref()).collect();
    conn.execute(&sql, params_refs.as_slice())?;

    Ok(())
}

pub fn delete_order(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM paper_trading_orders WHERE id = ?1",
        params![id],
    )?;

    Ok(())
}

// ============================================================================
// Trade Operations
// ============================================================================

pub fn create_trade(
    id: &str,
    portfolio_id: &str,
    order_id: &str,
    symbol: &str,
    side: &str,
    price: f64,
    quantity: f64,
    fee: f64,
    fee_rate: f64,
    is_maker: bool,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO paper_trading_trades
         (id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10)",
        params![id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, if is_maker { 1 } else { 0 }],
    )?;

    Ok(())
}

pub fn get_portfolio_trades(portfolio_id: &str, limit: Option<i64>) -> Result<Vec<PaperTradingTrade>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(lim) = limit {
        format!(
            "SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker, timestamp
             FROM paper_trading_trades WHERE portfolio_id = ?1 ORDER BY timestamp DESC LIMIT {}",
            lim
        )
    } else {
        "SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker, timestamp
         FROM paper_trading_trades WHERE portfolio_id = ?1 ORDER BY timestamp DESC"
            .to_string()
    };

    let mut stmt = conn.prepare(&query)?;
    let trades = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(PaperTradingTrade {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                order_id: row.get(2)?,
                symbol: row.get(3)?,
                side: row.get(4)?,
                price: row.get(5)?,
                quantity: row.get(6)?,
                fee: row.get(7)?,
                fee_rate: row.get(8)?,
                is_maker: row.get::<_, i32>(9)? != 0,
                timestamp: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(trades)
}

pub fn get_trade(id: &str) -> Result<PaperTradingTrade> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let trade = conn.query_row(
        "SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker, timestamp
         FROM paper_trading_trades WHERE id = ?1",
        params![id],
        |row| {
            Ok(PaperTradingTrade {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                order_id: row.get(2)?,
                symbol: row.get(3)?,
                side: row.get(4)?,
                price: row.get(5)?,
                quantity: row.get(6)?,
                fee: row.get(7)?,
                fee_rate: row.get(8)?,
                is_maker: row.get::<_, i32>(9)? != 0,
                timestamp: row.get(10)?,
            })
        },
    )?;

    Ok(trade)
}

pub fn get_order_trades(order_id: &str) -> Result<Vec<PaperTradingTrade>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, portfolio_id, order_id, symbol, side, price, quantity, fee, fee_rate, is_maker, timestamp
         FROM paper_trading_trades WHERE order_id = ?1 ORDER BY timestamp ASC"
    )?;

    let trades = stmt
        .query_map(params![order_id], |row| {
            Ok(PaperTradingTrade {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                order_id: row.get(2)?,
                symbol: row.get(3)?,
                side: row.get(4)?,
                price: row.get(5)?,
                quantity: row.get(6)?,
                fee: row.get(7)?,
                fee_rate: row.get(8)?,
                is_maker: row.get::<_, i32>(9)? != 0,
                timestamp: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(trades)
}

pub fn delete_trade(id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM paper_trading_trades WHERE id = ?1",
        params![id],
    )?;

    Ok(())
}
