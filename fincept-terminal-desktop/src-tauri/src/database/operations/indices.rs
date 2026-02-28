// Custom Index, Constituent, and Snapshot Operations

use crate::database::{pool::get_pool, types::{CustomIndex, IndexConstituent, IndexSnapshot}};
use anyhow::Result;
use rusqlite::{params, OptionalExtension};

// ============================================================================
// Custom Index Operations
// ============================================================================

pub fn create_custom_index(index: &CustomIndex) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT INTO custom_indices (id, name, description, calculation_method, base_value, base_date, historical_start_date, divisor, current_value, previous_close, cap_weight, currency, portfolio_id, is_active, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
        params![
            index.id,
            index.name,
            index.description,
            index.calculation_method,
            index.base_value,
            index.base_date,
            index.historical_start_date,
            index.divisor,
            index.current_value,
            index.previous_close,
            index.cap_weight,
            index.currency,
            index.portfolio_id,
            if index.is_active { 1 } else { 0 }
        ],
    )?;

    Ok(())
}

pub fn get_all_custom_indices() -> Result<Vec<CustomIndex>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, name, description, calculation_method, base_value, base_date, historical_start_date, divisor, current_value, previous_close, cap_weight, currency, portfolio_id, is_active, created_at, updated_at
         FROM custom_indices WHERE is_active = 1 ORDER BY created_at DESC"
    )?;

    let indices = stmt
        .query_map([], |row| {
            Ok(CustomIndex {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                calculation_method: row.get(3)?,
                base_value: row.get(4)?,
                base_date: row.get(5)?,
                historical_start_date: row.get(6)?,
                divisor: row.get(7)?,
                current_value: row.get(8)?,
                previous_close: row.get(9)?,
                cap_weight: row.get(10)?,
                currency: row.get(11)?,
                portfolio_id: row.get(12)?,
                is_active: row.get::<_, i32>(13)? != 0,
                created_at: row.get(14)?,
                updated_at: row.get(15)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(indices)
}

pub fn get_custom_index_by_id(index_id: &str) -> Result<Option<CustomIndex>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT id, name, description, calculation_method, base_value, base_date, historical_start_date, divisor, current_value, previous_close, cap_weight, currency, portfolio_id, is_active, created_at, updated_at
             FROM custom_indices WHERE id = ?1",
            params![index_id],
            |row| {
                Ok(CustomIndex {
                    id: row.get(0)?,
                    name: row.get(1)?,
                    description: row.get(2)?,
                    calculation_method: row.get(3)?,
                    base_value: row.get(4)?,
                    base_date: row.get(5)?,
                    historical_start_date: row.get(6)?,
                    divisor: row.get(7)?,
                    current_value: row.get(8)?,
                    previous_close: row.get(9)?,
                    cap_weight: row.get(10)?,
                    currency: row.get(11)?,
                    portfolio_id: row.get(12)?,
                    is_active: row.get::<_, i32>(13)? != 0,
                    created_at: row.get(14)?,
                    updated_at: row.get(15)?,
                })
            },
        )
        .optional()?;

    Ok(result)
}

pub fn update_custom_index(index_id: &str, current_value: f64, previous_close: f64, divisor: f64) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE custom_indices SET current_value = ?1, previous_close = ?2, divisor = ?3, updated_at = CURRENT_TIMESTAMP WHERE id = ?4",
        params![current_value, previous_close, divisor, index_id],
    )?;

    Ok(())
}

pub fn delete_custom_index(index_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE custom_indices SET is_active = 0, updated_at = CURRENT_TIMESTAMP WHERE id = ?1",
        params![index_id],
    )?;

    Ok(())
}

pub fn hard_delete_custom_index(index_id: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute("DELETE FROM custom_indices WHERE id = ?1", params![index_id])?;

    Ok(())
}

// ============================================================================
// Index Constituent Operations
// ============================================================================

pub fn add_index_constituent(constituent: &IndexConstituent) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO index_constituents (id, index_id, symbol, shares, weight, market_cap, fundamental_score, custom_price, price_date, added_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP)",
        params![
            constituent.id,
            constituent.index_id,
            constituent.symbol,
            constituent.shares,
            constituent.weight,
            constituent.market_cap,
            constituent.fundamental_score,
            constituent.custom_price,
            constituent.price_date
        ],
    )?;

    Ok(())
}

pub fn get_index_constituents(index_id: &str) -> Result<Vec<IndexConstituent>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let mut stmt = conn.prepare(
        "SELECT id, index_id, symbol, shares, weight, market_cap, fundamental_score, custom_price, price_date, added_at, updated_at
         FROM index_constituents WHERE index_id = ?1 ORDER BY symbol"
    )?;

    let constituents = stmt
        .query_map(params![index_id], |row| {
            Ok(IndexConstituent {
                id: row.get(0)?,
                index_id: row.get(1)?,
                symbol: row.get(2)?,
                shares: row.get(3)?,
                weight: row.get(4)?,
                market_cap: row.get(5)?,
                fundamental_score: row.get(6)?,
                custom_price: row.get(7)?,
                price_date: row.get(8)?,
                added_at: row.get(9)?,
                updated_at: row.get(10)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(constituents)
}

pub fn update_index_constituent(constituent_id: &str, shares: Option<f64>, weight: Option<f64>, market_cap: Option<f64>, fundamental_score: Option<f64>, custom_price: Option<f64>, price_date: Option<String>) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "UPDATE index_constituents SET shares = COALESCE(?1, shares), weight = COALESCE(?2, weight), market_cap = COALESCE(?3, market_cap), fundamental_score = COALESCE(?4, fundamental_score), custom_price = ?5, price_date = ?6, updated_at = CURRENT_TIMESTAMP WHERE id = ?7",
        params![shares, weight, market_cap, fundamental_score, custom_price, price_date, constituent_id],
    )?;

    Ok(())
}

pub fn remove_index_constituent(index_id: &str, symbol: &str) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "DELETE FROM index_constituents WHERE index_id = ?1 AND symbol = ?2",
        params![index_id, symbol],
    )?;

    Ok(())
}

// ============================================================================
// Index Snapshot Operations
// ============================================================================

pub fn save_index_snapshot(snapshot: &IndexSnapshot) -> Result<()> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    conn.execute(
        "INSERT OR REPLACE INTO index_snapshots (id, index_id, index_value, day_change, day_change_percent, total_market_value, divisor, constituents_data, snapshot_date, created_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, CURRENT_TIMESTAMP)",
        params![
            snapshot.id,
            snapshot.index_id,
            snapshot.index_value,
            snapshot.day_change,
            snapshot.day_change_percent,
            snapshot.total_market_value,
            snapshot.divisor,
            snapshot.constituents_data,
            snapshot.snapshot_date
        ],
    )?;

    Ok(())
}

pub fn get_index_snapshots(index_id: &str, limit: Option<i32>) -> Result<Vec<IndexSnapshot>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let query = if let Some(lim) = limit {
        format!(
            "SELECT id, index_id, index_value, day_change, day_change_percent, total_market_value, divisor, constituents_data, snapshot_date, created_at
             FROM index_snapshots WHERE index_id = ?1 ORDER BY snapshot_date DESC LIMIT {}",
            lim
        )
    } else {
        "SELECT id, index_id, index_value, day_change, day_change_percent, total_market_value, divisor, constituents_data, snapshot_date, created_at
         FROM index_snapshots WHERE index_id = ?1 ORDER BY snapshot_date DESC".to_string()
    };

    let mut stmt = conn.prepare(&query)?;

    let snapshots = stmt
        .query_map(params![index_id], |row| {
            Ok(IndexSnapshot {
                id: row.get(0)?,
                index_id: row.get(1)?,
                index_value: row.get(2)?,
                day_change: row.get(3)?,
                day_change_percent: row.get(4)?,
                total_market_value: row.get(5)?,
                divisor: row.get(6)?,
                constituents_data: row.get(7)?,
                snapshot_date: row.get(8)?,
                created_at: row.get(9)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(snapshots)
}

pub fn get_latest_index_snapshot(index_id: &str) -> Result<Option<IndexSnapshot>> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let result = conn
        .query_row(
            "SELECT id, index_id, index_value, day_change, day_change_percent, total_market_value, divisor, constituents_data, snapshot_date, created_at
             FROM index_snapshots WHERE index_id = ?1 ORDER BY snapshot_date DESC LIMIT 1",
            params![index_id],
            |row| {
                Ok(IndexSnapshot {
                    id: row.get(0)?,
                    index_id: row.get(1)?,
                    index_value: row.get(2)?,
                    day_change: row.get(3)?,
                    day_change_percent: row.get(4)?,
                    total_market_value: row.get(5)?,
                    divisor: row.get(6)?,
                    constituents_data: row.get(7)?,
                    snapshot_date: row.get(8)?,
                    created_at: row.get(9)?,
                })
            },
        )
        .optional()?;

    Ok(result)
}

pub fn delete_index_snapshots_older_than(index_id: &str, days: i32) -> Result<i32> {
    let pool = get_pool()?;
    let conn = pool.get()?;

    let deleted = conn.execute(
        &format!(
            "DELETE FROM index_snapshots WHERE index_id = ?1 AND snapshot_date < date('now', '-{} days')",
            days
        ),
        params![index_id],
    )?;

    Ok(deleted as i32)
}
