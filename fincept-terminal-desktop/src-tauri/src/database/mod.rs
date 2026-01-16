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

pub use pool::init_database;
pub use types::*;

use anyhow::Result;

/// Initialize all databases and connection pools
pub async fn initialize() -> Result<()> {
    init_database().await?;

    // Initialize paper trading tables
    eprintln!("[Database] Initializing paper trading tables...");
    if let Err(e) = crate::paper_trading::init_tables() {
        eprintln!("[Database] Warning: Failed to initialize paper trading tables: {}", e);
    } else {
        eprintln!("[Database] ✓ Paper trading tables initialized");
    }

    Ok(())
}
