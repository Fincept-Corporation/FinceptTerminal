// Database Module - High-performance SQLite with connection pooling
// Structure: Modular organization for optimal performance

pub mod pool;
pub mod schema;
pub mod types;
pub mod operations;
pub mod queries;
pub mod cache;
pub mod notes_excel;
pub mod broker_credentials;
// pub mod master_contract; // REMOVED - use symbol_master instead
pub mod storage;
pub mod symbol_master; // Unified symbol database (preferred)

// Legacy broker-specific symbol tables (deprecated - use symbol_master instead)
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
    if let Err(_e) = init_cache_database().await {
    } else {
    }

    // Initialize paper trading tables
    if let Err(_e) = crate::paper_trading::init_tables() {
    } else {
    }

    // Initialize unified symbol master status table
    if let Err(_e) = symbol_master::init_master_contract_status_table() {
        eprintln!("[database] Failed to initialize master_contract_status table");
    } else {
        eprintln!("[database] Master contract status table initialized");
    }

    // Legacy: Initialize Fyers symbols table (deprecated)
    if let Err(_e) = fyers_symbols::init_fyers_symbols_table() {
    } else {
    }

    // Legacy: Initialize Shoonya symbols table (deprecated)
    if let Err(_e) = shoonya_symbols::init_shoonya_symbols_table() {
    } else {
    }

    Ok(())
}
