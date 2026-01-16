// Connection Pool Management - Singleton with r2d2 for concurrent access

use anyhow::{Context, Result};
use once_cell::sync::OnceCell;
use parking_lot::RwLock;
use r2d2::Pool;
use r2d2_sqlite::SqliteConnectionManager;
use rusqlite::OpenFlags;
use std::sync::Arc;

pub type DbPool = Arc<Pool<SqliteConnectionManager>>;

static POOL: OnceCell<RwLock<Option<DbPool>>> = OnceCell::new();

/// Get database connection pool (lazy initialized)
pub fn get_pool() -> Result<DbPool> {
    let pool_lock = POOL.get_or_init(|| RwLock::new(None));
    let pool_read = pool_lock.read();

    if let Some(pool) = pool_read.as_ref() {
        return Ok(Arc::clone(pool));
    }

    drop(pool_read);
    Err(anyhow::anyhow!("Database not initialized. Call init_database first."))
}

/// Get a database connection from the pool
pub fn get_db() -> Result<r2d2::PooledConnection<SqliteConnectionManager>> {
    let pool = get_pool()?;
    pool.get().context("Failed to get database connection from pool")
}

/// Initialize database connection pool with optimal settings
pub async fn init_database() -> Result<DbPool> {
    eprintln!("[Database] init_database() called");
    let pool_lock = POOL.get_or_init(|| RwLock::new(None));

    // Check if already initialized
    {
        let pool_read = pool_lock.read();
        if let Some(pool) = pool_read.as_ref() {
            eprintln!("[Database] Pool already initialized, reusing");
            return Ok(Arc::clone(pool));
        }
    }

    eprintln!("[Database] Initializing new pool");

    // Initialize pool
    let mut pool_write = pool_lock.write();

    // Get database path
    let db_path = get_db_path()?;
    eprintln!("[Database] Database path resolved: {:?}", db_path);

    // Create connection manager with optimizations
    let manager = SqliteConnectionManager::file(&db_path)
        .with_flags(
            OpenFlags::SQLITE_OPEN_READ_WRITE
                | OpenFlags::SQLITE_OPEN_CREATE
                | OpenFlags::SQLITE_OPEN_NO_MUTEX, // Faster with connection pool
        )
        .with_init(|conn| {
            // Performance optimizations
            conn.execute_batch(
                "PRAGMA journal_mode = WAL;
                 PRAGMA synchronous = NORMAL;
                 PRAGMA cache_size = -64000;
                 PRAGMA temp_store = MEMORY;
                 PRAGMA mmap_size = 30000000000;
                 PRAGMA page_size = 4096;
                 PRAGMA foreign_keys = ON;",
            )?;
            Ok(())
        });

    // Create pool with optimal settings
    eprintln!("[Database] Building connection pool...");
    let pool = Pool::builder()
        .max_size(16) // Support 16 concurrent connections
        .min_idle(Some(2)) // Keep 2 connections warm
        .connection_timeout(std::time::Duration::from_secs(5))
        .build(manager)
        .context("Failed to create connection pool")?;

    eprintln!("[Database] Connection pool created successfully");

    let pool_arc = Arc::new(pool);

    // Initialize schema
    {
        eprintln!("[Database] Getting connection to initialize schema...");
        let conn = pool_arc.get().context("Failed to get connection")?;
        eprintln!("[Database] Creating database schema...");
        crate::database::schema::create_schema(&conn)?;
        eprintln!("[Database] Database schema created successfully");
    }

    *pool_write = Some(Arc::clone(&pool_arc));

    eprintln!("[Database] ✓ Database initialization complete!");
    eprintln!("[Database] Database location: {:?}", db_path);

    Ok(pool_arc)
}

/// Get database file path - platform-specific app data directories
fn get_db_path() -> Result<std::path::PathBuf> {
    let db_dir = if cfg!(target_os = "windows") {
        // Windows: %APPDATA%\fincept-terminal
        let appdata = std::env::var("APPDATA")
            .context("APPDATA environment variable not set")?;
        eprintln!("[Database] APPDATA: {}", appdata);
        std::path::PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        // macOS: ~/Library/Application Support/fincept-terminal
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        eprintln!("[Database] HOME: {}", home);
        std::path::PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        // Linux: ~/.local/share/fincept-terminal (XDG Base Directory)
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        let xdg_data = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        eprintln!("[Database] XDG_DATA_HOME: {}", xdg_data);
        std::path::PathBuf::from(xdg_data).join("fincept-terminal")
    };

    eprintln!("[Database] Database directory: {:?}", db_dir);
    std::fs::create_dir_all(&db_dir).context("Failed to create database directory")?;

    let db_path = db_dir.join("fincept_terminal.db");
    eprintln!("[Database] Database path: {:?}", db_path);
    eprintln!("[Database] Database exists: {}", db_path.exists());

    Ok(db_path)
}

/// Get separate database paths for notes and excel
pub fn get_notes_db_path() -> Result<std::path::PathBuf> {
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .context("APPDATA environment variable not set")?;
        std::path::PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        std::path::PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        let xdg_data = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        std::path::PathBuf::from(xdg_data).join("fincept-terminal")
    };

    std::fs::create_dir_all(&db_dir).context("Failed to create database directory")?;
    Ok(db_dir.join("financial_notes.db"))
}

pub fn get_excel_db_path() -> Result<std::path::PathBuf> {
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .context("APPDATA environment variable not set")?;
        std::path::PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        std::path::PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .context("HOME environment variable not set")?;
        let xdg_data = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        std::path::PathBuf::from(xdg_data).join("fincept-terminal")
    };

    std::fs::create_dir_all(&db_dir).context("Failed to create database directory")?;
    Ok(db_dir.join("excel_files.db"))
}
