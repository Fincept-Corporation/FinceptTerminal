// Edgar Company Tickers Cache - SQLite storage for fast company lookup

use rusqlite::{params, Connection, Result as SqliteResult};
use serde::{Deserialize, Serialize};
use std::sync::Mutex;
use once_cell::sync::Lazy;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CompanyTicker {
    pub cik: i64,
    pub ticker: String,
    pub company: String,
}

static CACHE_DB_PATH: Lazy<Mutex<Option<PathBuf>>> = Lazy::new(|| Mutex::new(None));

// Set database path (called during app initialization)
pub fn set_cache_db_path(path: PathBuf) {
    let mut db_path = CACHE_DB_PATH.lock().unwrap();
    *db_path = Some(path);
}

// Get cache database connection
fn get_cache_db() -> SqliteResult<Connection> {
    let db_path_guard = CACHE_DB_PATH.lock().unwrap();

    let db_path = db_path_guard.as_ref()
        .ok_or_else(|| rusqlite::Error::InvalidPath("Cache database path not set".into()))?;

    let conn = Connection::open(db_path)?;

    // Create table if not exists
    conn.execute(
        "CREATE TABLE IF NOT EXISTS edgar_company_tickers (
            cik INTEGER PRIMARY KEY,
            ticker TEXT NOT NULL,
            company TEXT NOT NULL,
            updated_at INTEGER NOT NULL
        )",
        [],
    )?;

    // Create indexes for fast searching
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_edgar_ticker ON edgar_company_tickers(ticker)",
        [],
    )?;
    conn.execute(
        "CREATE INDEX IF NOT EXISTS idx_edgar_company ON edgar_company_tickers(company COLLATE NOCASE)",
        [],
    )?;

    Ok(conn)
}

#[tauri::command]
pub async fn edgar_cache_store_tickers(tickers: Vec<CompanyTicker>) -> Result<String, String> {
    let conn = get_cache_db().map_err(|e| e.to_string())?;
    let timestamp = chrono::Utc::now().timestamp();

    let tx = conn.unchecked_transaction().map_err(|e| e.to_string())?;

    // Clear existing data
    tx.execute("DELETE FROM edgar_company_tickers", []).map_err(|e| e.to_string())?;

    // Insert new data with REPLACE to handle duplicates
    let mut stmt = tx.prepare(
        "INSERT OR REPLACE INTO edgar_company_tickers (cik, ticker, company, updated_at) VALUES (?1, ?2, ?3, ?4)"
    ).map_err(|e| e.to_string())?;

    for ticker in &tickers {
        stmt.execute(params![ticker.cik, ticker.ticker, ticker.company, timestamp])
            .map_err(|e| e.to_string())?;
    }

    drop(stmt);
    tx.commit().map_err(|e| e.to_string())?;

    Ok(format!("Stored {} companies in cache", tickers.len()))
}

#[tauri::command]
pub async fn edgar_cache_search_companies(query: String, limit: usize) -> Result<Vec<CompanyTicker>, String> {
    let conn = get_cache_db().map_err(|e| e.to_string())?;

    let query_lower = query.to_lowercase();
    let query_pattern = format!("%{}%", query_lower);

    let mut stmt = conn.prepare(
        "SELECT cik, ticker, company FROM edgar_company_tickers
         WHERE LOWER(ticker) LIKE ?1 OR LOWER(company) LIKE ?1
         LIMIT ?2"
    ).map_err(|e| e.to_string())?;

    let rows = stmt.query_map(params![query_pattern, limit as i64], |row| {
        Ok(CompanyTicker {
            cik: row.get(0)?,
            ticker: row.get(1)?,
            company: row.get(2)?,
        })
    }).map_err(|e| e.to_string())?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row.map_err(|e| e.to_string())?);
    }

    Ok(results)
}

#[tauri::command]
pub async fn edgar_cache_get_all_companies() -> Result<Vec<CompanyTicker>, String> {
    let conn = get_cache_db().map_err(|e| e.to_string())?;

    let mut stmt = conn.prepare(
        "SELECT cik, ticker, company FROM edgar_company_tickers"
    ).map_err(|e| e.to_string())?;

    let rows = stmt.query_map([], |row| {
        Ok(CompanyTicker {
            cik: row.get(0)?,
            ticker: row.get(1)?,
            company: row.get(2)?,
        })
    }).map_err(|e| e.to_string())?;

    let mut results = Vec::new();
    for row in rows {
        results.push(row.map_err(|e| e.to_string())?);
    }

    Ok(results)
}

#[tauri::command]
pub async fn edgar_cache_get_count() -> Result<i64, String> {
    let conn = get_cache_db().map_err(|e| e.to_string())?;

    let count: i64 = conn.query_row(
        "SELECT COUNT(*) FROM edgar_company_tickers",
        [],
        |row| row.get(0)
    ).map_err(|e| e.to_string())?;

    Ok(count)
}

#[tauri::command]
pub async fn edgar_cache_is_populated() -> Result<bool, String> {
    let count = edgar_cache_get_count().await?;
    Ok(count > 1000) // Consider populated if >1000 companies
}
