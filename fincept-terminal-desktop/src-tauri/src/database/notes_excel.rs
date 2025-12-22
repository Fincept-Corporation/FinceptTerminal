// Notes and Excel Operations - Separate databases for notes and excel files

use crate::database::{pool::{get_notes_db_path, get_excel_db_path}, types::{Note, NoteTemplate, ExcelFile, ExcelSnapshot}};
use anyhow::{Context, Result};
use rusqlite::{Connection, params, OptionalExtension};
use parking_lot::Mutex;
use once_cell::sync::Lazy;

// Separate connections for notes and excel (lighter databases)
static NOTES_CONN: Lazy<Mutex<Option<Connection>>> = Lazy::new(|| Mutex::new(None));
static EXCEL_CONN: Lazy<Mutex<Option<Connection>>> = Lazy::new(|| Mutex::new(None));

// ============================================================================
// Notes Database Initialization
// ============================================================================

fn get_notes_conn() -> Result<()> {
    let mut conn_lock = NOTES_CONN.lock();
    if conn_lock.is_none() {
        let path = get_notes_db_path()?;
        let conn = Connection::open(&path).context("Failed to open notes database")?;

        // Create schema
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS financial_notes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                title TEXT NOT NULL,
                content TEXT NOT NULL,
                category TEXT NOT NULL,
                priority TEXT DEFAULT 'MEDIUM',
                tags TEXT,
                tickers TEXT,
                sentiment TEXT DEFAULT 'NEUTRAL',
                is_favorite INTEGER DEFAULT 0,
                is_archived INTEGER DEFAULT 0,
                color_code TEXT DEFAULT '#FF8800',
                attachments TEXT,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                reminder_date TEXT,
                word_count INTEGER DEFAULT 0
            );

            CREATE TABLE IF NOT EXISTS note_templates (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                description TEXT,
                content TEXT NOT NULL,
                category TEXT NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_notes_category ON financial_notes(category);
            CREATE INDEX IF NOT EXISTS idx_notes_updated ON financial_notes(updated_at DESC);
            CREATE INDEX IF NOT EXISTS idx_notes_favorite ON financial_notes(is_favorite);
            CREATE INDEX IF NOT EXISTS idx_notes_archived ON financial_notes(is_archived);"
        )?;

        *conn_lock = Some(conn);
    }
    Ok(())
}

// ============================================================================
// Notes Operations
// ============================================================================

pub fn create_note(note: &Note) -> Result<i64> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    conn.execute(
        "INSERT INTO financial_notes
         (title, content, category, priority, tags, tickers, sentiment, is_favorite, is_archived,
          color_code, attachments, created_at, updated_at, reminder_date, word_count)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15)",
        params![
            note.title,
            note.content,
            note.category,
            note.priority,
            note.tags,
            note.tickers,
            note.sentiment,
            if note.is_favorite { 1 } else { 0 },
            if note.is_archived { 1 } else { 0 },
            note.color_code,
            note.attachments,
            note.created_at,
            note.updated_at,
            note.reminder_date,
            note.word_count,
        ],
    )?;

    Ok(conn.last_insert_rowid())
}

pub fn get_all_notes(include_archived: bool) -> Result<Vec<Note>> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    let query = if include_archived {
        "SELECT * FROM financial_notes ORDER BY updated_at DESC"
    } else {
        "SELECT * FROM financial_notes WHERE is_archived = 0 ORDER BY updated_at DESC"
    };

    let mut stmt = conn.prepare(query)?;
    let notes = stmt
        .query_map([], |row| {
            Ok(Note {
                id: row.get(0)?,
                title: row.get(1)?,
                content: row.get(2)?,
                category: row.get(3)?,
                priority: row.get(4)?,
                tags: row.get(5)?,
                tickers: row.get(6)?,
                sentiment: row.get(7)?,
                is_favorite: row.get::<_, i32>(8)? != 0,
                is_archived: row.get::<_, i32>(9)? != 0,
                color_code: row.get(10)?,
                attachments: row.get(11)?,
                created_at: row.get(12)?,
                updated_at: row.get(13)?,
                reminder_date: row.get(14)?,
                word_count: row.get(15)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(notes)
}

pub fn update_note(id: i64, updates: &Note) -> Result<()> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    conn.execute(
        "UPDATE financial_notes SET
         title = ?1, content = ?2, category = ?3, priority = ?4, tags = ?5, tickers = ?6,
         sentiment = ?7, is_favorite = ?8, is_archived = ?9, color_code = ?10,
         attachments = ?11, updated_at = ?12, reminder_date = ?13, word_count = ?14
         WHERE id = ?15",
        params![
            updates.title,
            updates.content,
            updates.category,
            updates.priority,
            updates.tags,
            updates.tickers,
            updates.sentiment,
            if updates.is_favorite { 1 } else { 0 },
            if updates.is_archived { 1 } else { 0 },
            updates.color_code,
            updates.attachments,
            updates.updated_at,
            updates.reminder_date,
            updates.word_count,
            id,
        ],
    )?;

    Ok(())
}

pub fn delete_note(id: i64) -> Result<()> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    conn.execute("DELETE FROM financial_notes WHERE id = ?1", params![id])?;

    Ok(())
}

pub fn search_notes(query: &str) -> Result<Vec<Note>> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    let search_pattern = format!("%{}%", query);

    let mut stmt = conn.prepare(
        "SELECT * FROM financial_notes
         WHERE (title LIKE ?1 OR content LIKE ?1 OR tags LIKE ?1 OR tickers LIKE ?1)
         AND is_archived = 0
         ORDER BY updated_at DESC"
    )?;

    let notes = stmt
        .query_map(params![search_pattern], |row| {
            Ok(Note {
                id: row.get(0)?,
                title: row.get(1)?,
                content: row.get(2)?,
                category: row.get(3)?,
                priority: row.get(4)?,
                tags: row.get(5)?,
                tickers: row.get(6)?,
                sentiment: row.get(7)?,
                is_favorite: row.get::<_, i32>(8)? != 0,
                is_archived: row.get::<_, i32>(9)? != 0,
                color_code: row.get(10)?,
                attachments: row.get(11)?,
                created_at: row.get(12)?,
                updated_at: row.get(13)?,
                reminder_date: row.get(14)?,
                word_count: row.get(15)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(notes)
}

pub fn get_note_templates() -> Result<Vec<NoteTemplate>> {
    get_notes_conn()?;
    let conn_lock = NOTES_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    let mut stmt = conn.prepare("SELECT * FROM note_templates ORDER BY name ASC")?;
    let templates = stmt
        .query_map([], |row| {
            Ok(NoteTemplate {
                id: row.get(0)?,
                name: row.get(1)?,
                description: row.get(2)?,
                content: row.get(3)?,
                category: row.get(4)?,
                created_at: row.get(5)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(templates)
}

// ============================================================================
// Excel Database Initialization
// ============================================================================

fn get_excel_conn() -> Result<()> {
    let mut conn_lock = EXCEL_CONN.lock();
    if conn_lock.is_none() {
        let path = get_excel_db_path()?;
        let conn = Connection::open(&path).context("Failed to open excel database")?;

        // Create schema
        conn.execute_batch(
            "CREATE TABLE IF NOT EXISTS excel_files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_name TEXT NOT NULL,
                file_path TEXT NOT NULL,
                sheet_count INTEGER DEFAULT 1,
                last_opened TEXT NOT NULL,
                last_modified TEXT NOT NULL,
                created_at TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS excel_snapshots (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                file_id INTEGER NOT NULL,
                snapshot_name TEXT NOT NULL,
                sheet_data TEXT NOT NULL,
                created_at TEXT NOT NULL,
                FOREIGN KEY (file_id) REFERENCES excel_files(id) ON DELETE CASCADE
            );

            CREATE INDEX IF NOT EXISTS idx_excel_last_opened ON excel_files(last_opened DESC);
            CREATE INDEX IF NOT EXISTS idx_snapshot_file ON excel_snapshots(file_id, created_at DESC);"
        )?;

        *conn_lock = Some(conn);
    }
    Ok(())
}

// ============================================================================
// Excel Operations
// ============================================================================

pub fn add_or_update_excel_file(file: &ExcelFile) -> Result<i64> {
    get_excel_conn()?;
    let conn_lock = EXCEL_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    // Check if exists
    let existing: Option<i64> = conn
        .query_row(
            "SELECT id FROM excel_files WHERE file_path = ?1",
            params![file.file_path],
            |row| row.get(0),
        )
        .optional()?;

    if let Some(id) = existing {
        // Update
        conn.execute(
            "UPDATE excel_files SET file_name = ?1, sheet_count = ?2, last_opened = ?3, last_modified = ?4
             WHERE file_path = ?5",
            params![file.file_name, file.sheet_count, file.last_opened, file.last_modified, file.file_path],
        )?;
        Ok(id)
    } else {
        // Insert
        conn.execute(
            "INSERT INTO excel_files (file_name, file_path, sheet_count, last_opened, last_modified, created_at)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![file.file_name, file.file_path, file.sheet_count, file.last_opened, file.last_modified, file.created_at],
        )?;
        Ok(conn.last_insert_rowid())
    }
}

pub fn get_recent_excel_files(limit: i64) -> Result<Vec<ExcelFile>> {
    get_excel_conn()?;
    let conn_lock = EXCEL_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    let mut stmt = conn.prepare(
        "SELECT * FROM excel_files ORDER BY last_opened DESC LIMIT ?1"
    )?;

    let files = stmt
        .query_map(params![limit], |row| {
            Ok(ExcelFile {
                id: row.get(0)?,
                file_name: row.get(1)?,
                file_path: row.get(2)?,
                sheet_count: row.get(3)?,
                last_opened: row.get(4)?,
                last_modified: row.get(5)?,
                created_at: row.get(6)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(files)
}

pub fn create_excel_snapshot(file_id: i64, snapshot_name: &str, sheet_data: &str, created_at: &str) -> Result<i64> {
    get_excel_conn()?;
    let conn_lock = EXCEL_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    conn.execute(
        "INSERT INTO excel_snapshots (file_id, snapshot_name, sheet_data, created_at)
         VALUES (?1, ?2, ?3, ?4)",
        params![file_id, snapshot_name, sheet_data, created_at],
    )?;

    Ok(conn.last_insert_rowid())
}

pub fn get_excel_snapshots(file_id: i64) -> Result<Vec<ExcelSnapshot>> {
    get_excel_conn()?;
    let conn_lock = EXCEL_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    let mut stmt = conn.prepare(
        "SELECT * FROM excel_snapshots WHERE file_id = ?1 ORDER BY created_at DESC"
    )?;

    let snapshots = stmt
        .query_map(params![file_id], |row| {
            Ok(ExcelSnapshot {
                id: row.get(0)?,
                file_id: row.get(1)?,
                snapshot_name: row.get(2)?,
                sheet_data: row.get(3)?,
                created_at: row.get(4)?,
            })
        })?
        .collect::<std::result::Result<Vec<_>, _>>()?;

    Ok(snapshots)
}

pub fn delete_excel_snapshot(id: i64) -> Result<()> {
    get_excel_conn()?;
    let conn_lock = EXCEL_CONN.lock();
    let conn = conn_lock.as_ref().unwrap();

    conn.execute("DELETE FROM excel_snapshots WHERE id = ?1", params![id])?;

    Ok(())
}
