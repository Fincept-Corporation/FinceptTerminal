// Database Migrations — ALTER TABLE checks for schema evolution

use anyhow::Result;
use rusqlite::Connection;

pub fn run_migrations(conn: &Connection) -> Result<()> {
    // Add custom_price column to index_constituents
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('index_constituents') WHERE name='custom_price'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE index_constituents ADD COLUMN custom_price REAL", [])?;
        println!("[Migration] Added custom_price column to index_constituents");
    }

    // Add price_date column to index_constituents
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('index_constituents') WHERE name='price_date'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE index_constituents ADD COLUMN price_date TEXT", [])?;
        println!("[Migration] Added price_date column to index_constituents");
    }

    // Add historical_start_date column to custom_indices
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('custom_indices') WHERE name='historical_start_date'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE custom_indices ADD COLUMN historical_start_date TEXT", [])?;
        println!("[Migration] Added historical_start_date column to custom_indices");
    }

    // Add strategy_type column to algo_deployments
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('algo_deployments') WHERE name='strategy_type'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE algo_deployments ADD COLUMN strategy_type TEXT DEFAULT 'json'", [])?;
        println!("[Migration] Added strategy_type column to algo_deployments");
    }

    // Add python_strategy_id column to algo_deployments
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('algo_deployments') WHERE name='python_strategy_id'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE algo_deployments ADD COLUMN python_strategy_id TEXT", [])?;
        println!("[Migration] Added python_strategy_id column to algo_deployments");
    }

    // Add parameter_overrides column to algo_deployments
    let count: Result<i64, _> = conn.query_row(
        "SELECT COUNT(*) FROM pragma_table_info('algo_deployments') WHERE name='parameter_overrides'",
        [],
        |row| row.get(0),
    );
    if let Ok(0) = count {
        conn.execute("ALTER TABLE algo_deployments ADD COLUMN parameter_overrides TEXT DEFAULT '{}'", [])?;
        println!("[Migration] Added parameter_overrides column to algo_deployments");
    }

    Ok(())
}
