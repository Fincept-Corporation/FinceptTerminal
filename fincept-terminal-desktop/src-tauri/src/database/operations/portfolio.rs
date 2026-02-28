// Portfolio, Asset, and Transaction Operations

use crate::database::pool::get_pool;
use anyhow::Result;
use rusqlite::{params, OptionalExtension};

pub fn create_portfolio(id: &str, name: &str, owner: &str, currency: &str, description: Option<&str>) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO portfolios (id, name, owner, currency, description, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
        params![id, name, owner, currency, description],
    )?;

    Ok(())
}

pub fn get_all_portfolios() -> Result<Vec<serde_json::Value>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, owner, currency, description, created_at, updated_at
         FROM portfolios ORDER BY created_at DESC"
    )?;

    let portfolios = stmt
        .query_map([], |row| {
            Ok(serde_json::json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "owner": row.get::<_, String>(2)?,
                "currency": row.get::<_, String>(3)?,
                "description": row.get::<_, Option<String>>(4)?,
                "created_at": row.get::<_, String>(5)?,
                "updated_at": row.get::<_, String>(6)?
            }))
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(portfolios)
}

pub fn get_portfolio_by_id(portfolio_id: &str) -> Result<Option<serde_json::Value>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT id, name, owner, currency, description, created_at, updated_at
             FROM portfolios WHERE id = ?1",
            params![portfolio_id],
            |row| {
                Ok(serde_json::json!({
                    "id": row.get::<_, String>(0)?,
                    "name": row.get::<_, String>(1)?,
                    "owner": row.get::<_, String>(2)?,
                    "currency": row.get::<_, String>(3)?,
                    "description": row.get::<_, Option<String>>(4)?,
                    "created_at": row.get::<_, String>(5)?,
                    "updated_at": row.get::<_, String>(6)?
                }))
            },
        )
        .optional()?;

    Ok(result)
}

pub fn delete_portfolio(portfolio_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM portfolios WHERE id = ?1", params![portfolio_id])?;

    Ok(())
}

pub fn add_portfolio_asset(
    id: &str,
    portfolio_id: &str,
    symbol: &str,
    quantity: f64,
    price: f64,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let existing: Option<(String, f64, f64)> = conn
        .query_row(
            "SELECT id, quantity, avg_buy_price FROM portfolio_assets
             WHERE portfolio_id = ?1 AND symbol = ?2",
            params![portfolio_id, symbol],
            |row| Ok((row.get(0)?, row.get(1)?, row.get(2)?)),
        )
        .optional()?;

    if let Some((existing_id, existing_qty, existing_avg_price)) = existing {
        let total_qty = existing_qty + quantity;
        let new_avg_price = ((existing_avg_price * existing_qty) + (price * quantity)) / total_qty;

        conn.execute(
            "UPDATE portfolio_assets
             SET quantity = ?1, avg_buy_price = ?2, last_updated = CURRENT_TIMESTAMP
             WHERE id = ?3",
            params![total_qty, new_avg_price, existing_id],
        )?;
    } else {
        conn.execute(
            "INSERT INTO portfolio_assets (id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated)
             VALUES (?1, ?2, ?3, ?4, ?5, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
            params![id, portfolio_id, symbol, quantity, price],
        )?;
    }

    Ok(())
}

pub fn sell_portfolio_asset(
    portfolio_id: &str,
    symbol: &str,
    quantity: f64,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let existing: Option<(String, f64)> = conn
        .query_row(
            "SELECT id, quantity FROM portfolio_assets
             WHERE portfolio_id = ?1 AND symbol = ?2",
            params![portfolio_id, symbol],
            |row| Ok((row.get(0)?, row.get(1)?)),
        )
        .optional()?;

    if let Some((asset_id, existing_qty)) = existing {
        if quantity >= existing_qty {
            conn.execute("DELETE FROM portfolio_assets WHERE id = ?1", params![asset_id])?;
        } else {
            let new_qty = existing_qty - quantity;
            conn.execute(
                "UPDATE portfolio_assets SET quantity = ?1, last_updated = CURRENT_TIMESTAMP WHERE id = ?2",
                params![new_qty, asset_id],
            )?;
        }
    } else {
        return Err(anyhow::anyhow!("Asset not found in portfolio"));
    }

    Ok(())
}

pub fn add_portfolio_transaction(
    id: &str,
    portfolio_id: &str,
    symbol: &str,
    transaction_type: &str,
    quantity: f64,
    price: f64,
    notes: Option<&str>,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let total_value = quantity * price;

    conn.execute(
        "INSERT INTO portfolio_transactions (id, portfolio_id, symbol, transaction_type, quantity, price, total_value, notes, transaction_date)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, CURRENT_TIMESTAMP)",
        params![id, portfolio_id, symbol, transaction_type, quantity, price, total_value, notes],
    )?;

    Ok(())
}

pub fn get_portfolio_assets(portfolio_id: &str) -> Result<Vec<serde_json::Value>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, portfolio_id, symbol, quantity, avg_buy_price, first_purchase_date, last_updated
         FROM portfolio_assets WHERE portfolio_id = ?1 ORDER BY symbol"
    )?;

    let assets = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(serde_json::json!({
                "id": row.get::<_, String>(0)?,
                "portfolio_id": row.get::<_, String>(1)?,
                "symbol": row.get::<_, String>(2)?,
                "quantity": row.get::<_, f64>(3)?,
                "avg_buy_price": row.get::<_, f64>(4)?,
                "first_purchase_date": row.get::<_, String>(5)?,
                "last_updated": row.get::<_, String>(6)?
            }))
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(assets)
}

pub fn update_portfolio_asset_symbol(asset_id: &str, new_symbol: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE portfolio_assets SET symbol = ?1, last_updated = CURRENT_TIMESTAMP WHERE id = ?2",
        params![new_symbol, asset_id],
    )?;

    Ok(())
}

pub fn get_portfolio_transactions(portfolio_id: &str, limit: Option<i32>) -> Result<Vec<serde_json::Value>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(lim) = limit {
        format!(
            "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, transaction_date, notes
             FROM portfolio_transactions WHERE portfolio_id = ?1 ORDER BY transaction_date DESC LIMIT {}",
            lim
        )
    } else {
        "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, transaction_date, notes
         FROM portfolio_transactions WHERE portfolio_id = ?1 ORDER BY transaction_date DESC".to_string()
    };

    let mut stmt = conn.prepare(&query)?;

    let transactions = stmt
        .query_map(params![portfolio_id], |row| {
            Ok(serde_json::json!({
                "id": row.get::<_, String>(0)?,
                "portfolio_id": row.get::<_, String>(1)?,
                "symbol": row.get::<_, String>(2)?,
                "transaction_type": row.get::<_, String>(3)?,
                "quantity": row.get::<_, f64>(4)?,
                "price": row.get::<_, f64>(5)?,
                "total_value": row.get::<_, f64>(6)?,
                "transaction_date": row.get::<_, String>(7)?,
                "notes": row.get::<_, Option<String>>(8)?
            }))
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(transactions)
}

pub fn update_portfolio_transaction(
    transaction_id: &str,
    quantity: f64,
    price: f64,
    transaction_date: &str,
    notes: Option<&str>,
) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let total_value = quantity * price;

    conn.execute(
        "UPDATE portfolio_transactions
         SET quantity = ?1, price = ?2, total_value = ?3, transaction_date = ?4, notes = ?5
         WHERE id = ?6",
        params![quantity, price, total_value, transaction_date, notes, transaction_id],
    )?;

    Ok(())
}

pub fn delete_portfolio_transaction(transaction_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM portfolio_transactions WHERE id = ?1", params![transaction_id])?;

    Ok(())
}

pub fn get_portfolio_transaction_by_id(transaction_id: &str) -> Result<Option<serde_json::Value>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT id, portfolio_id, symbol, transaction_type, quantity, price, total_value, transaction_date, notes
             FROM portfolio_transactions WHERE id = ?1",
            params![transaction_id],
            |row| {
                Ok(serde_json::json!({
                    "id": row.get::<_, String>(0)?,
                    "portfolio_id": row.get::<_, String>(1)?,
                    "symbol": row.get::<_, String>(2)?,
                    "transaction_type": row.get::<_, String>(3)?,
                    "quantity": row.get::<_, f64>(4)?,
                    "price": row.get::<_, f64>(5)?,
                    "total_value": row.get::<_, f64>(6)?,
                    "transaction_date": row.get::<_, String>(7)?,
                    "notes": row.get::<_, Option<String>>(8)?
                }))
            },
        )
        .optional()?;

    Ok(result)
}
