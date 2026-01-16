// Stock-Specific Paper Trading Operations
// Extends paper_trading.rs with stock market features

use crate::database::pool::get_pool;
use anyhow::{Result, bail};
use rusqlite::params;
use serde::{Deserialize, Serialize};
use chrono::{Local, NaiveTime};

// ============================================================================
// Stock-Specific Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum ProductType {
    CNC,   // Cash and Carry (Delivery)
    MIS,   // Margin Intraday Square-off
    NRML,  // Normal (F&O)
}

impl ProductType {
    pub fn as_str(&self) -> &str {
        match self {
            ProductType::CNC => "CNC",
            ProductType::MIS => "MIS",
            ProductType::NRML => "NRML",
        }
    }

    pub fn from_str(s: &str) -> Result<Self> {
        match s {
            "CNC" => Ok(ProductType::CNC),
            "MIS" => Ok(ProductType::MIS),
            "NRML" => Ok(ProductType::NRML),
            _ => bail!("Invalid product type: {}", s),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StockOrderRequest {
    pub symbol: String,
    pub exchange: String,
    pub side: String,
    pub order_type: String,
    pub quantity: f64,
    pub price: Option<f64>,
    pub trigger_price: Option<f64>,
    pub product: String,
    pub validity: String,
    pub current_price: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StockPosition {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub exchange: String,
    pub product: String,
    pub quantity: f64,
    pub average_price: f64,
    pub current_price: Option<f64>,
    pub unrealized_pnl: Option<f64>,
    pub realized_pnl: f64,
    pub today_realized_pnl: f64,
    pub created_at: String,
    pub updated_at: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StockHolding {
    pub id: String,
    pub portfolio_id: String,
    pub symbol: String,
    pub exchange: String,
    pub quantity: f64,
    pub average_price: f64,
    pub current_price: Option<f64>,
    pub pnl: Option<f64>,
    pub pnl_percent: Option<f64>,
    pub settlement_date: String,
    pub created_at: String,
}

// ============================================================================
// Stock Order Validation
// ============================================================================

pub fn validate_stock_order(order: &StockOrderRequest, portfolio_id: &str) -> Result<()> {
    let product = ProductType::from_str(&order.product)?;

    // Time validation for MIS
    if product == ProductType::MIS {
        let now = Local::now();
        let cutoff_time = NaiveTime::from_hms_opt(15, 15, 0).unwrap();

        if now.time() > cutoff_time {
            // Check if this is a closing order
            let is_closing = is_closing_order(portfolio_id, &order.symbol, &order.exchange, &order.product, order.quantity, &order.side)?;

            if !is_closing {
                bail!("Cannot place new MIS orders after 3:15 PM. Only closing orders allowed.");
            }
        }
    }

    // CNC SELL validation - requires existing holdings
    if product == ProductType::CNC && order.side == "SELL" {
        let available_qty = get_available_quantity_for_sale(
            portfolio_id,
            &order.symbol,
            &order.exchange,
        )?;

        if available_qty < order.quantity {
            bail!(
                "Insufficient holdings for CNC SELL. Available: {}, Requested: {}",
                available_qty,
                order.quantity
            );
        }
    }

    Ok(())
}

fn is_closing_order(
    portfolio_id: &str,
    symbol: &str,
    exchange: &str,
    product: &str,
    quantity: f64,
    side: &str,
) -> Result<bool> {
    let position = get_stock_position(portfolio_id, symbol, exchange, product)?;

    match position {
        Some(pos) => {
            // Check if order reduces existing position
            if pos.quantity > 0.0 && side == "SELL" && quantity <= pos.quantity {
                return Ok(true);
            }
            if pos.quantity < 0.0 && side == "BUY" && quantity <= pos.quantity.abs() {
                return Ok(true);
            }
            Ok(false)
        }
        None => Ok(false),
    }
}

fn get_available_quantity_for_sale(
    portfolio_id: &str,
    symbol: &str,
    exchange: &str,
) -> Result<f64> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    // Get quantity from holdings
    let holdings_qty: f64 = conn
        .query_row(
            "SELECT COALESCE(SUM(quantity), 0) FROM stock_holdings
             WHERE portfolio_id = ?1 AND symbol = ?2 AND exchange = ?3",
            params![portfolio_id, symbol, exchange],
            |row| row.get(0),
        )
        .unwrap_or(0.0);

    // Get quantity from CNC positions
    let position_qty: f64 = conn
        .query_row(
            "SELECT COALESCE(SUM(quantity), 0) FROM stock_positions
             WHERE portfolio_id = ?1 AND symbol = ?2 AND exchange = ?3 AND product = 'CNC'",
            params![portfolio_id, symbol, exchange],
            |row| row.get(0),
        )
        .unwrap_or(0.0);

    Ok(holdings_qty + position_qty)
}

// ============================================================================
// Margin Calculation (Stock-Specific)
// ============================================================================

pub fn calculate_stock_margin(order: &StockOrderRequest) -> Result<f64> {
    let product = ProductType::from_str(&order.product)?;
    let base_value = order.quantity * order.current_price;

    let leverage = match product {
        ProductType::CNC => 1.0,    // Full margin
        ProductType::MIS => 5.0,    // 5x leverage (configurable)
        ProductType::NRML => 10.0,  // 10x leverage for F&O
    };

    let margin = base_value / leverage;
    Ok(margin)
}

// ============================================================================
// Stock Position Operations
// ============================================================================

pub fn create_stock_position(
    portfolio_id: &str,
    symbol: &str,
    exchange: &str,
    product: &str,
    quantity: f64,
    average_price: f64,
) -> Result<StockPosition> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let id = uuid::Uuid::new_v4().to_string();

    conn.execute(
        "INSERT INTO stock_positions
         (id, portfolio_id, symbol, exchange, product, quantity, average_price, realized_pnl, today_realized_pnl)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, 0, 0)",
        params![id, portfolio_id, symbol, exchange, product, quantity, average_price],
    )?;

    get_stock_position(portfolio_id, symbol, exchange, product)?
        .ok_or_else(|| anyhow::anyhow!("Failed to retrieve created position"))
}

pub fn get_stock_position(
    portfolio_id: &str,
    symbol: &str,
    exchange: &str,
    product: &str,
) -> Result<Option<StockPosition>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let position = conn.query_row(
        "SELECT id, portfolio_id, symbol, exchange, product, quantity, average_price,
                current_price, unrealized_pnl, realized_pnl, today_realized_pnl, created_at, updated_at
         FROM stock_positions
         WHERE portfolio_id = ?1 AND symbol = ?2 AND exchange = ?3 AND product = ?4",
        params![portfolio_id, symbol, exchange, product],
        |row| {
            Ok(StockPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                exchange: row.get(3)?,
                product: row.get(4)?,
                quantity: row.get(5)?,
                average_price: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                today_realized_pnl: row.get(10)?,
                created_at: row.get(11)?,
                updated_at: row.get(12)?,
            })
        },
    );

    match position {
        Ok(pos) => Ok(Some(pos)),
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}

pub fn list_stock_positions(portfolio_id: &str) -> Result<Vec<StockPosition>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, portfolio_id, symbol, exchange, product, quantity, average_price,
                current_price, unrealized_pnl, realized_pnl, today_realized_pnl, created_at, updated_at
         FROM stock_positions
         WHERE portfolio_id = ?1 AND quantity != 0
         ORDER BY updated_at DESC",
    )?;

    let positions = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(StockPosition {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                exchange: row.get(3)?,
                product: row.get(4)?,
                quantity: row.get(5)?,
                average_price: row.get(6)?,
                current_price: row.get(7)?,
                unrealized_pnl: row.get(8)?,
                realized_pnl: row.get(9)?,
                today_realized_pnl: row.get(10)?,
                created_at: row.get(11)?,
                updated_at: row.get(12)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(positions)
}

// ============================================================================
// Holdings Operations (T+1 Settlement)
// ============================================================================

pub fn create_or_update_holding(
    portfolio_id: &str,
    symbol: &str,
    exchange: &str,
    quantity: f64,
    average_price: f64,
) -> Result<StockHolding> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    // Check if holding exists
    let existing: Option<StockHolding> = conn
        .query_row(
            "SELECT id, portfolio_id, symbol, exchange, quantity, average_price,
                    current_price, pnl, pnl_percent, settlement_date, created_at
             FROM stock_holdings
             WHERE portfolio_id = ?1 AND symbol = ?2 AND exchange = ?3",
            params![portfolio_id, symbol, exchange],
            |row| {
                Ok(StockHolding {
                    id: row.get(0)?,
                    portfolio_id: row.get(1)?,
                    symbol: row.get(2)?,
                    exchange: row.get(3)?,
                    quantity: row.get(4)?,
                    average_price: row.get(5)?,
                    current_price: row.get(6)?,
                    pnl: row.get(7)?,
                    pnl_percent: row.get(8)?,
                    settlement_date: row.get(9)?,
                    created_at: row.get(10)?,
                })
            },
        )
        .ok();

    if let Some(holding) = existing {
        // Update weighted average
        let total_qty = holding.quantity + quantity;
        let new_avg_price = (holding.quantity * holding.average_price + quantity * average_price) / total_qty;

        conn.execute(
            "UPDATE stock_holdings
             SET quantity = ?1, average_price = ?2
             WHERE id = ?3",
            params![total_qty, new_avg_price, holding.id],
        )?;

        get_holding_by_id(&holding.id)
    } else {
        // Create new holding
        let id = uuid::Uuid::new_v4().to_string();
        let settlement_date = Local::now().format("%Y-%m-%d").to_string();

        conn.execute(
            "INSERT INTO stock_holdings
             (id, portfolio_id, symbol, exchange, quantity, average_price, settlement_date)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
            params![id, portfolio_id, symbol, exchange, quantity, average_price, settlement_date],
        )?;

        get_holding_by_id(&id)
    }
}

pub fn get_holding_by_id(id: &str) -> Result<StockHolding> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let holding = conn.query_row(
        "SELECT id, portfolio_id, symbol, exchange, quantity, average_price,
                current_price, pnl, pnl_percent, settlement_date, created_at
         FROM stock_holdings WHERE id = ?1",
        params![id],
        |row| {
            Ok(StockHolding {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                exchange: row.get(3)?,
                quantity: row.get(4)?,
                average_price: row.get(5)?,
                current_price: row.get(6)?,
                pnl: row.get(7)?,
                pnl_percent: row.get(8)?,
                settlement_date: row.get(9)?,
                created_at: row.get(10)?,
            })
        },
    )?;

    Ok(holding)
}

pub fn list_holdings(portfolio_id: &str) -> Result<Vec<StockHolding>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, portfolio_id, symbol, exchange, quantity, average_price,
                current_price, pnl, pnl_percent, settlement_date, created_at
         FROM stock_holdings
         WHERE portfolio_id = ?1 AND quantity > 0
         ORDER BY created_at DESC",
    )?;

    let holdings = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(StockHolding {
                id: row.get(0)?,
                portfolio_id: row.get(1)?,
                symbol: row.get(2)?,
                exchange: row.get(3)?,
                quantity: row.get(4)?,
                average_price: row.get(5)?,
                current_price: row.get(6)?,
                pnl: row.get(7)?,
                pnl_percent: row.get(8)?,
                settlement_date: row.get(9)?,
                created_at: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(holdings)
}

// ============================================================================
// T+1 Settlement
// ============================================================================

pub fn process_t1_settlement(portfolio_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    // Get all CNC positions from previous day
    let yesterday = (Local::now() - chrono::Duration::days(1))
        .format("%Y-%m-%d")
        .to_string();

    let mut stmt = conn.prepare(
        "SELECT id, symbol, exchange, quantity, average_price
         FROM stock_positions
         WHERE portfolio_id = ?1 AND product = 'CNC' AND DATE(created_at) <= ?2",
    )?;

    let old_positions: Vec<(String, String, String, f64, f64)> = stmt
        .query_map(params![portfolio_id, yesterday], |row| {
            Ok((row.get(0)?, row.get(1)?, row.get(2)?, row.get(3)?, row.get(4)?))
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    for (pos_id, symbol, exchange, quantity, avg_price) in old_positions {
        if quantity > 0.0 {
            // BUY position -> Move to holdings
            create_or_update_holding(portfolio_id, &symbol, &exchange, quantity, avg_price)?;
        } else {
            // SELL position -> Reduce holdings (already handled in order execution)
        }

        // Delete position
        conn.execute(
            "DELETE FROM stock_positions WHERE id = ?1",
            params![pos_id],
        )?;
    }

    Ok(())
}

// ============================================================================
// Auto Square-off (MIS)
// ============================================================================

pub fn auto_squareoff_mis_positions(portfolio_id: &str) -> Result<Vec<String>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut squared_off = Vec::new();

    // Get all MIS positions
    let mut stmt = conn.prepare(
        "SELECT id, symbol, exchange, quantity, average_price, current_price
         FROM stock_positions
         WHERE portfolio_id = ?1 AND product = 'MIS' AND quantity != 0",
    )?;

    let mis_positions: Vec<(String, String, String, f64, f64, Option<f64>)> = stmt
        .query_map(params![portfolio_id], |row| {
            Ok((row.get(0)?, row.get(1)?, row.get(2)?, row.get(3)?, row.get(4)?, row.get(5)?))
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    for (pos_id, symbol, exchange, quantity, avg_price, current_price) in mis_positions {
        let close_price = current_price.unwrap_or(avg_price);

        // Calculate P&L
        let pnl = if quantity > 0.0 {
            (close_price - avg_price) * quantity
        } else {
            (avg_price - close_price) * quantity.abs()
        };

        // Update position to closed
        conn.execute(
            "UPDATE stock_positions
             SET quantity = 0, realized_pnl = realized_pnl + ?1, today_realized_pnl = today_realized_pnl + ?1
             WHERE id = ?2",
            params![pnl, pos_id],
        )?;

        squared_off.push(format!("{} {} (P&L: {:.2})", symbol, exchange, pnl));
    }

    Ok(squared_off)
}
