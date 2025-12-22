// Database Module - High-performance SQLite with connection pooling
// Structure: 8 files for optimal organization and performance

pub mod pool;
pub mod schema;
pub mod types;
pub mod operations;
pub mod queries;
pub mod cache;
pub mod paper_trading;
pub mod notes_excel;

pub use pool::init_database;
pub use types::*;

use anyhow::Result;

/// Initialize all databases and connection pools
pub async fn initialize() -> Result<()> {
    init_database().await?;
    Ok(())
}
