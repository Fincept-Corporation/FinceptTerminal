// Database Module - High-performance SQLite with connection pooling
// Structure: 8 files for optimal organization and performance

pub mod pool;
pub mod schema;
pub mod types;
pub mod operations;
pub mod queries;
pub mod cache;
pub mod notes_excel;
pub mod broker_credentials;
pub mod master_contract;
pub mod storage;
pub mod fyers_symbols;
pub mod shoonya_symbols;

pub use pool::init_database;
pub use pool::init_cache_database;
pub use types::*;

use anyhow::Result;

/// Initialize all databases and connection pools
pub async fn initialize() -> Result<()> {
    init_database().await?;

    // Initialize cache database (separate file: fincept_cache.db)
    if let Err(e) = init_cache_database().await {
    } else {
    }

    // Initialize paper trading tables
    if let Err(e) = crate::paper_trading::init_tables() {
    } else {
    }

    // Initialize Fyers symbols table
    if let Err(e) = fyers_symbols::init_fyers_symbols_table() {
    } else {
    }

    // Initialize Shoonya symbols table
    if let Err(e) = shoonya_symbols::init_shoonya_symbols_table() {
    } else {
    }

    Ok(())
}
