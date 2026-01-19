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
    eprintln!("[Database] Initializing cache database...");
    if let Err(e) = init_cache_database().await {
        eprintln!("[Database] Warning: Failed to initialize cache database: {}", e);
    } else {
        eprintln!("[Database] ✓ Cache database initialized");
    }

    // Initialize paper trading tables
    eprintln!("[Database] Initializing paper trading tables...");
    if let Err(e) = crate::paper_trading::init_tables() {
        eprintln!("[Database] Warning: Failed to initialize paper trading tables: {}", e);
    } else {
        eprintln!("[Database] ✓ Paper trading tables initialized");
    }

    // Initialize Fyers symbols table
    eprintln!("[Database] Initializing Fyers symbols table...");
    if let Err(e) = fyers_symbols::init_fyers_symbols_table() {
        eprintln!("[Database] Warning: Failed to initialize Fyers symbols table: {}", e);
    } else {
        eprintln!("[Database] ✓ Fyers symbols table initialized");
    }

    // Initialize Shoonya symbols table
    eprintln!("[Database] Initializing Shoonya symbols table...");
    if let Err(e) = shoonya_symbols::init_shoonya_symbols_table() {
        eprintln!("[Database] Warning: Failed to initialize Shoonya symbols table: {}", e);
    } else {
        eprintln!("[Database] ✓ Shoonya symbols table initialized");
    }

    Ok(())
}
