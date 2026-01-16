// Stock Paper Trading Commands
// Tauri commands for stock-specific paper trading operations

use crate::database::stock_paper_trading::{
    StockOrderRequest, StockPosition, StockHolding, ProductType,
    validate_stock_order, calculate_stock_margin, create_stock_position,
    get_stock_position, list_stock_positions,
    list_holdings,
};
use crate::database::paper_trading::{
    PaperTradingPortfolio, PaperTradingOrder,
    create_portfolio as create_pt_portfolio, get_portfolio as get_pt_portfolio,
    update_portfolio_balance,
    update_order, get_order, get_portfolio_orders,
    create_trade as create_pt_trade, get_available_margin,
};
use crate::database::pool::get_pool;
use rusqlite::params;
use serde::{Deserialize, Serialize};
use anyhow::{Result, bail};

// ============================================================================
// Response Types
// ============================================================================

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OrderResult {
    pub success: bool,
    pub order_id: Option<String>,
    pub message: String,
    pub order: Option<PaperTradingOrder>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConvertResult {
    pub success: bool,
    pub message: String,
    pub from_position: Option<StockPosition>,
    pub to_position: Option<StockPosition>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Funds {
    pub available_balance: f64,
    pub used_margin: f64,
    pub total_balance: f64,
    pub unrealized_pnl: f64,
    pub realized_pnl: f64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Statistics {
    pub total_orders: usize,
    pub filled_orders: usize,
    pub total_trades: usize,
    pub total_volume: f64,
    pub win_rate: f64,
    pub profit_factor: f64,
    pub sharpe_ratio: f64,
}

// ============================================================================
// Portfolio Commands
// ============================================================================

#[tauri::command]
pub async fn stock_paper_trading_get_portfolio(portfolio_id: String) -> Result<PaperTradingPortfolio, String> {
    get_pt_portfolio(&portfolio_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn stock_paper_trading_create_portfolio(
    id: String,
    name: String,
    provider: String,
    initial_balance: f64,
    currency: String,
    margin_mode: String,
    leverage: f64,
) -> Result<PaperTradingPortfolio, String> {
    create_pt_portfolio(&id, &name, &provider, initial_balance, &currency, &margin_mode, leverage)
        .map_err(|e| e.to_string())
}

// ============================================================================
// Order Commands
// ============================================================================

#[tauri::command]
pub async fn stock_paper_trading_place_order(
    portfolio_id: String,
    order: StockOrderRequest,
) -> Result<OrderResult, String> {
    // Validate order
    if let Err(e) = validate_stock_order(&order, &portfolio_id) {
        return Ok(OrderResult {
            success: false,
            order_id: None,
            message: e.to_string(),
            order: None,
        });
    }

    // Calculate margin requirement
    let margin_required = calculate_stock_margin(&order)
        .map_err(|e| e.to_string())?;

    // Check available balance
    let available = get_available_margin(&portfolio_id)
        .map_err(|e| e.to_string())?;

    if available < margin_required {
        return Ok(OrderResult {
            success: false,
            order_id: None,
            message: format!(
                "Insufficient funds. Required: {:.2}, Available: {:.2}",
                margin_required, available
            ),
            order: None,
        });
    }

    // Create order
    let order_id = uuid::Uuid::new_v4().to_string();
    let price = if order.order_type == "market" {
        Some(order.current_price)
    } else {
        order.price
    };

    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    // Insert order with stock-specific fields
    conn.execute(
        "INSERT INTO paper_trading_orders
         (id, portfolio_id, symbol, side, type, quantity, price, stop_price, status, product, exchange)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, 'pending', ?9, ?10)",
        params![
            &order_id,
            &portfolio_id,
            &order.symbol,
            &order.side,
            &order.order_type,
            order.quantity,
            price,
            order.trigger_price,
            &order.product,
            &order.exchange,
        ],
    ).map_err(|e| e.to_string())?;

    // If market order, execute immediately
    if order.order_type == "market" {
        execute_stock_order(&portfolio_id, &order_id, order.current_price)
            .map_err(|e| e.to_string())?;
    }

    let created_order = get_order(&order_id)
        .map_err(|e| e.to_string())?;

    Ok(OrderResult {
        success: true,
        order_id: Some(order_id.clone()),
        message: "Order placed successfully".to_string(),
        order: Some(created_order),
    })
}

#[tauri::command]
pub async fn stock_paper_trading_modify_order(
    _portfolio_id: String,
    order_id: String,
    modifications: serde_json::Value,
) -> Result<OrderResult, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    // Check if order exists and is pending
    let order = get_order(&order_id)
        .map_err(|e| e.to_string())?;

    if order.status != "pending" {
        return Ok(OrderResult {
            success: false,
            order_id: Some(order_id),
            message: format!("Cannot modify order with status: {}", order.status),
            order: None,
        });
    }

    // Update modifiable fields
    if let Some(new_price) = modifications.get("price").and_then(|v| v.as_f64()) {
        conn.execute(
            "UPDATE paper_trading_orders SET price = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
            params![new_price, &order_id],
        ).map_err(|e| e.to_string())?;
    }

    if let Some(new_quantity) = modifications.get("quantity").and_then(|v| v.as_f64()) {
        conn.execute(
            "UPDATE paper_trading_orders SET quantity = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
            params![new_quantity, &order_id],
        ).map_err(|e| e.to_string())?;
    }

    let updated_order = get_order(&order_id)
        .map_err(|e| e.to_string())?;

    Ok(OrderResult {
        success: true,
        order_id: Some(order_id),
        message: "Order modified successfully".to_string(),
        order: Some(updated_order),
    })
}

#[tauri::command]
pub async fn stock_paper_trading_cancel_order(
    _portfolio_id: String,
    order_id: String,
) -> Result<OrderResult, String> {
    let order = get_order(&order_id)
        .map_err(|e| e.to_string())?;

    if order.status != "pending" {
        return Ok(OrderResult {
            success: false,
            order_id: Some(order_id),
            message: format!("Cannot cancel order with status: {}", order.status),
            order: None,
        });
    }

    update_order(&order_id, None, None, Some("cancelled"), None)
        .map_err(|e| e.to_string())?;

    Ok(OrderResult {
        success: true,
        order_id: Some(order_id.clone()),
        message: "Order cancelled successfully".to_string(),
        order: None,
    })
}

#[tauri::command]
pub async fn stock_paper_trading_get_orders(
    portfolio_id: String,
    filter: Option<String>,
) -> Result<Vec<PaperTradingOrder>, String> {
    get_portfolio_orders(&portfolio_id, filter.as_deref())
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn stock_paper_trading_get_order_by_id(
    _portfolio_id: String,
    order_id: String,
) -> Result<PaperTradingOrder, String> {
    get_order(&order_id)
        .map_err(|e| e.to_string())
}

// ============================================================================
// Position Commands
// ============================================================================

#[tauri::command]
pub async fn stock_paper_trading_get_positions(
    portfolio_id: String,
) -> Result<Vec<StockPosition>, String> {
    list_stock_positions(&portfolio_id)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn stock_paper_trading_get_holdings(
    portfolio_id: String,
) -> Result<Vec<StockHolding>, String> {
    list_holdings(&portfolio_id)
        .map_err(|e| e.to_string())
}

// ============================================================================
// Funds & Statistics Commands
// ============================================================================

#[tauri::command]
pub async fn stock_paper_trading_get_funds(
    portfolio_id: String,
) -> Result<Funds, String> {
    let portfolio = get_pt_portfolio(&portfolio_id)
        .map_err(|e| e.to_string())?;

    let available = get_available_margin(&portfolio_id)
        .map_err(|e| e.to_string())?;

    let used_margin = portfolio.current_balance - available;

    Ok(Funds {
        available_balance: available,
        used_margin,
        total_balance: portfolio.current_balance,
        unrealized_pnl: 0.0, // Calculate from positions
        realized_pnl: 0.0,   // Calculate from closed positions
    })
}

#[tauri::command]
pub async fn stock_paper_trading_get_statistics(
    portfolio_id: String,
) -> Result<Statistics, String> {
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let total_orders: i64 = conn.query_row(
        "SELECT COUNT(*) FROM paper_trading_orders WHERE portfolio_id = ?1",
        params![&portfolio_id],
        |row| row.get(0),
    ).unwrap_or(0);

    let filled_orders: i64 = conn.query_row(
        "SELECT COUNT(*) FROM paper_trading_orders WHERE portfolio_id = ?1 AND status = 'filled'",
        params![&portfolio_id],
        |row| row.get(0),
    ).unwrap_or(0);

    let total_trades: i64 = conn.query_row(
        "SELECT COUNT(*) FROM paper_trading_trades WHERE portfolio_id = ?1",
        params![&portfolio_id],
        |row| row.get(0),
    ).unwrap_or(0);

    let total_volume: f64 = conn.query_row(
        "SELECT COALESCE(SUM(quantity * price), 0) FROM paper_trading_trades WHERE portfolio_id = ?1",
        params![&portfolio_id],
        |row| row.get(0),
    ).unwrap_or(0.0);

    Ok(Statistics {
        total_orders: total_orders as usize,
        filled_orders: filled_orders as usize,
        total_trades: total_trades as usize,
        total_volume,
        win_rate: 0.0,
        profit_factor: 0.0,
        sharpe_ratio: 0.0,
    })
}

#[tauri::command]
pub async fn stock_paper_trading_reset_portfolio(
    portfolio_id: String,
) -> Result<(), String> {
    let portfolio = get_pt_portfolio(&portfolio_id)
        .map_err(|e| e.to_string())?;

    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    // Delete all orders, trades, positions, and holdings
    conn.execute(
        "DELETE FROM paper_trading_trades WHERE portfolio_id = ?1",
        params![&portfolio_id],
    ).map_err(|e| e.to_string())?;

    conn.execute(
        "DELETE FROM paper_trading_orders WHERE portfolio_id = ?1",
        params![&portfolio_id],
    ).map_err(|e| e.to_string())?;

    conn.execute(
        "DELETE FROM stock_positions WHERE portfolio_id = ?1",
        params![&portfolio_id],
    ).map_err(|e| e.to_string())?;

    conn.execute(
        "DELETE FROM stock_holdings WHERE portfolio_id = ?1",
        params![&portfolio_id],
    ).map_err(|e| e.to_string())?;

    // Reset balance
    update_portfolio_balance(&portfolio_id, portfolio.initial_balance)
        .map_err(|e| e.to_string())?;

    Ok(())
}

// ============================================================================
// Position Conversion (CNC <-> MIS <-> NRML)
// ============================================================================

#[tauri::command]
pub async fn stock_paper_trading_convert_position(
    portfolio_id: String,
    symbol: String,
    exchange: String,
    from_product: String,
    to_product: String,
    quantity: f64,
) -> Result<ConvertResult, String> {
    // Validate products
    let _from_prod = ProductType::from_str(&from_product)
        .map_err(|e| e.to_string())?;
    let _to_prod = ProductType::from_str(&to_product)
        .map_err(|e| e.to_string())?;

    // Get source position
    let from_position = get_stock_position(&portfolio_id, &symbol, &exchange, &from_product)
        .map_err(|e| e.to_string())?;

    if from_position.is_none() {
        return Ok(ConvertResult {
            success: false,
            message: format!("No {} position found for {}", from_product, symbol),
            from_position: None,
            to_position: None,
        });
    }

    let from_pos = from_position.unwrap();

    if from_pos.quantity < quantity {
        return Ok(ConvertResult {
            success: false,
            message: format!(
                "Insufficient quantity. Available: {}, Requested: {}",
                from_pos.quantity, quantity
            ),
            from_position: Some(from_pos),
            to_position: None,
        });
    }

    // Update source position
    let pool = get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let new_quantity = from_pos.quantity - quantity;

    if new_quantity == 0.0 {
        // Remove position
        conn.execute(
            "DELETE FROM stock_positions WHERE id = ?1",
            params![&from_pos.id],
        ).map_err(|e| e.to_string())?;
    } else {
        // Update quantity
        conn.execute(
            "UPDATE stock_positions SET quantity = ?1, updated_at = CURRENT_TIMESTAMP WHERE id = ?2",
            params![new_quantity, &from_pos.id],
        ).map_err(|e| e.to_string())?;
    }

    // Create or update target position
    let to_position = get_stock_position(&portfolio_id, &symbol, &exchange, &to_product)
        .map_err(|e| e.to_string())?;

    let result_position = if let Some(mut to_pos) = to_position {
        // Update existing position
        let total_qty = to_pos.quantity + quantity;
        let avg_price = (to_pos.average_price * to_pos.quantity + from_pos.average_price * quantity) / total_qty;

        conn.execute(
            "UPDATE stock_positions SET quantity = ?1, average_price = ?2, updated_at = CURRENT_TIMESTAMP WHERE id = ?3",
            params![total_qty, avg_price, &to_pos.id],
        ).map_err(|e| e.to_string())?;

        to_pos.quantity = total_qty;
        to_pos.average_price = avg_price;
        to_pos
    } else {
        // Create new position
        create_stock_position(&portfolio_id, &symbol, &exchange, &to_product, quantity, from_pos.average_price)
            .map_err(|e| e.to_string())?
    };

    Ok(ConvertResult {
        success: true,
        message: format!("Successfully converted {} {} from {} to {}", quantity, symbol, from_product, to_product),
        from_position: None,
        to_position: Some(result_position),
    })
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

fn execute_stock_order(portfolio_id: &str, order_id: &str, execution_price: f64) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    // Get order details
    let order = get_order(order_id)?;

    // Create trade
    let trade_id = uuid::Uuid::new_v4().to_string();
    create_pt_trade(
        &trade_id,
        portfolio_id,
        order_id,
        &order.symbol,
        &order.side,
        execution_price,
        order.quantity,
        0.0, // Fee
        0.0, // Fee rate
        false, // is_maker
    )?;

    // Update order status
    update_order(order_id, Some(order.quantity), Some(execution_price), Some("filled"), None)?;

    // Get product and exchange from order
    let (product, exchange): (String, String) = conn.query_row(
        "SELECT product, exchange FROM paper_trading_orders WHERE id = ?1",
        params![order_id],
        |row| Ok((row.get(0)?, row.get(1)?)),
    )?;

    // Update or create position
    let existing_position = get_stock_position(portfolio_id, &order.symbol, &exchange, &product)?;

    if let Some(pos) = existing_position {
        // Update existing position
        let quantity_change = if order.side == "buy" {
            order.quantity
        } else {
            -order.quantity
        };

        let new_quantity = pos.quantity + quantity_change;

        if new_quantity == 0.0 {
            // Position closed
            conn.execute(
                "DELETE FROM stock_positions WHERE id = ?1",
                params![&pos.id],
            )?;
        } else {
            // Update position
            let new_avg_price = if new_quantity.abs() > pos.quantity.abs() {
                // Increasing position
                (pos.average_price * pos.quantity + execution_price * quantity_change) / new_quantity
            } else {
                // Decreasing position - keep same average
                pos.average_price
            };

            conn.execute(
                "UPDATE stock_positions SET quantity = ?1, average_price = ?2, updated_at = CURRENT_TIMESTAMP WHERE id = ?3",
                params![new_quantity, new_avg_price, &pos.id],
            )?;
        }
    } else if order.side == "buy" {
        // Create new position
        create_stock_position(portfolio_id, &order.symbol, &exchange, &product, order.quantity, execution_price)?;
    } else {
        bail!("Cannot sell without existing position");
    }

    // Update balance
    let portfolio = get_pt_portfolio(portfolio_id)?;
    let cost = order.quantity * execution_price;
    let new_balance = if order.side == "buy" {
        portfolio.current_balance - cost
    } else {
        portfolio.current_balance + cost
    };

    update_portfolio_balance(&portfolio.id, new_balance)?;

    Ok(())
}
