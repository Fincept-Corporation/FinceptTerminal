// commands/database/admin.rs - Generic database management commands (password protected)

use crate::database::*;
use rusqlite::Connection;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use sha2::Sha256;
use hmac::{Hmac, Mac};
use rand::Rng;

#[derive(Debug, Serialize, Deserialize)]
pub struct DatabaseInfo {
    pub name: String,
    pub path: String,
    pub size_bytes: u64,
    pub table_count: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TableInfo {
    pub name: String,
    pub row_count: i64,
    pub columns: Vec<ColumnInfo>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ColumnInfo {
    pub name: String,
    pub data_type: String,
    pub not_null: bool,
    pub primary_key: bool,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TableRow {
    pub data: HashMap<String, serde_json::Value>,
}

// Password management with session support
#[tauri::command]
pub async fn db_admin_check_password() -> Result<bool, String> {
    // Check if password exists in settings
    match operations::get_setting("db_admin_password") {
        Ok(Some(_)) => Ok(true),
        Ok(None) => Ok(false),
        Err(e) => Err(e.to_string()),
    }
}

#[tauri::command]
pub async fn db_admin_check_session() -> Result<bool, String> {
    // Check if there's a valid session
    match operations::get_setting("db_admin_session_expires") {
        Ok(Some(expires_str)) => {
            if let Ok(expires_ts) = expires_str.parse::<i64>() {
                let now = std::time::SystemTime::now()
                    .duration_since(std::time::UNIX_EPOCH)
                    .unwrap()
                    .as_secs() as i64;
                Ok(expires_ts > now)
            } else {
                Ok(false)
            }
        }
        _ => Ok(false),
    }
}

#[tauri::command]
pub async fn db_admin_create_session(duration_hours: i64) -> Result<String, String> {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64;
    let expires_at = now + (duration_hours * 3600);

    operations::save_setting("db_admin_session_expires", &expires_at.to_string(), Some("security"))
        .map_err(|e| e.to_string())?;

    Ok("Session created".to_string())
}

#[tauri::command]
pub async fn db_admin_clear_session() -> Result<String, String> {
    // Delete session by setting expired timestamp
    operations::save_setting("db_admin_session_expires", "0", Some("security"))
        .map_err(|e| e.to_string())?;
    Ok("Session cleared".to_string())
}

#[tauri::command]
pub async fn db_admin_set_password(password: String) -> Result<String, String> {
    // Generate a random 32-byte salt
    let mut salt = [0u8; 32];
    rand::thread_rng().fill(&mut salt);
    let salt_hex = hex::encode(salt);

    // Hash password with HMAC-SHA256 using the salt as key
    let hash_hex = hmac_sha256_hash(&password, &salt);

    // Store as "salt$hash"
    let stored = format!("{}${}", salt_hex, hash_hex);

    operations::save_setting("db_admin_password", &stored, Some("security"))
        .map_err(|e| e.to_string())?;
    Ok("Password set successfully".to_string())
}

#[tauri::command]
pub async fn db_admin_verify_password(password: String) -> Result<bool, String> {
    match operations::get_setting("db_admin_password") {
        Ok(Some(stored)) => {
            // Handle both new "salt$hash" format and legacy bare-hash format
            if let Some((salt_hex, stored_hash)) = stored.split_once('$') {
                // New salted format: verify with HMAC-SHA256
                if let Ok(salt) = hex::decode(salt_hex) {
                    let computed_hash = hmac_sha256_hash(&password, &salt);
                    // Constant-time comparison to prevent timing attacks
                    Ok(constant_time_eq(computed_hash.as_bytes(), stored_hash.as_bytes()))
                } else {
                    Ok(false)
                }
            } else {
                // Legacy unsalted SHA-256 format: verify then migrate
                use sha2::Digest;
                let mut hasher = Sha256::new();
                hasher.update(password.as_bytes());
                let legacy_hash = format!("{:x}", hasher.finalize());
                let matches = constant_time_eq(legacy_hash.as_bytes(), stored.as_bytes());
                if matches {
                    // Auto-migrate to salted format on successful login
                    let mut salt = [0u8; 32];
                    rand::thread_rng().fill(&mut salt);
                    let salt_hex = hex::encode(salt);
                    let hash_hex = hmac_sha256_hash(&password, &salt);
                    let new_stored = format!("{}${}", salt_hex, hash_hex);
                    let _ = operations::save_setting("db_admin_password", &new_stored, Some("security"));
                }
                Ok(matches)
            }
        }
        Ok(None) => Ok(true), // No password set, allow access
        Err(e) => Err(e.to_string()),
    }
}

/// Compute HMAC-SHA256(key=salt, message=password) and return hex string
fn hmac_sha256_hash(password: &str, salt: &[u8]) -> String {
    type HmacSha256 = Hmac<Sha256>;
    let mut mac = HmacSha256::new_from_slice(salt)
        .expect("HMAC can take key of any size");
    mac.update(password.as_bytes());
    hex::encode(mac.finalize().into_bytes())
}

/// Constant-time comparison of two byte slices to prevent timing attacks
fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    let mut result = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        result |= x ^ y;
    }
    result == 0
}

// Get all databases
#[tauri::command]
pub async fn db_admin_get_databases() -> Result<Vec<DatabaseInfo>, String> {
    use std::fs;

    // Get database directory
    let db_dir = if cfg!(target_os = "windows") {
        let appdata = std::env::var("APPDATA")
            .map_err(|e| format!("APPDATA not set: {}", e))?;
        std::path::PathBuf::from(appdata).join("fincept-terminal")
    } else if cfg!(target_os = "macos") {
        let home = std::env::var("HOME")
            .map_err(|e| format!("HOME not set: {}", e))?;
        std::path::PathBuf::from(home)
            .join("Library")
            .join("Application Support")
            .join("fincept-terminal")
    } else {
        let home = std::env::var("HOME")
            .map_err(|e| format!("HOME not set: {}", e))?;
        let xdg_data = std::env::var("XDG_DATA_HOME")
            .unwrap_or_else(|_| format!("{}/.local/share", home));
        std::path::PathBuf::from(xdg_data).join("fincept-terminal")
    };

    let mut databases = Vec::new();

    let db_files = vec![
        ("Main Database", "fincept_terminal.db"),
        ("Cache", "fincept_cache.db"),
        ("Notes", "financial_notes.db"),
        ("Excel Files", "excel_files.db"),
        ("Deployed Strategies", "deployed_strategies.db"),
        ("Edgar Cache", "edgar_company_cache.db"),
        ("AkShare Symbols", "akshare_symbols.db"),
    ];

    for (name, filename) in db_files {
        let path = db_dir.join(filename);
        if path.exists() {
            let size = fs::metadata(&path)
                .map(|m| m.len())
                .unwrap_or(0);

            // Count tables
            let table_count = match Connection::open(&path) {
                Ok(conn) => {
                    let count: i64 = conn
                        .query_row(
                            "SELECT COUNT(*) FROM sqlite_master WHERE type='table'",
                            [],
                            |row| row.get(0),
                        )
                        .unwrap_or(0);
                    count as usize
                }
                Err(_) => 0,
            };

            databases.push(DatabaseInfo {
                name: name.to_string(),
                path: path.to_string_lossy().to_string(),
                size_bytes: size,
                table_count,
            });
        }
    }

    Ok(databases)
}

// Get tables in a database
#[tauri::command]
pub async fn db_admin_get_tables(db_path: String) -> Result<Vec<TableInfo>, String> {
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")
        .map_err(|e| e.to_string())?;

    let table_names: Vec<String> = stmt
        .query_map([], |row| row.get(0))
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    let mut tables = Vec::new();

    for table_name in table_names {
        // Validate table name to prevent SQL injection via crafted sqlite_master entries
        if let Err(_) = validate_sql_identifier(&table_name, "Table name") {
            continue; // Skip tables with suspicious names
        }

        // Get row count
        let row_count: i64 = conn
            .query_row(&format!("SELECT COUNT(*) FROM \"{}\"", table_name), [], |row| {
                row.get(0)
            })
            .unwrap_or(0);

        // Get column info
        let mut stmt = conn
            .prepare(&format!("PRAGMA table_info(\"{}\")", table_name))
            .map_err(|e| e.to_string())?;

        let columns: Vec<ColumnInfo> = stmt
            .query_map([], |row| {
                Ok(ColumnInfo {
                    name: row.get(1)?,
                    data_type: row.get(2)?,
                    not_null: row.get::<_, i32>(3)? == 1,
                    primary_key: row.get::<_, i32>(5)? == 1,
                })
            })
            .map_err(|e| e.to_string())?
            .filter_map(|r| r.ok())
            .collect();

        tables.push(TableInfo {
            name: table_name,
            row_count,
            columns,
        });
    }

    Ok(tables)
}

// Get table data with pagination
#[tauri::command]
pub async fn db_admin_get_table_data(
    db_path: String,
    table_name: String,
    limit: i64,
    offset: i64,
) -> Result<Vec<TableRow>, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    if limit < 0 || limit > 10000 {
        return Err("Limit must be between 0 and 10000".to_string());
    }
    if offset < 0 {
        return Err("Offset must be non-negative".to_string());
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!(
        "SELECT * FROM \"{}\" LIMIT {} OFFSET {}",
        table_name, limit, offset
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let column_count = stmt.column_count();
    let column_names: Vec<String> = (0..column_count)
        .map(|i| stmt.column_name(i).unwrap_or("").to_string())
        .collect();

    let rows = stmt
        .query_map([], |row| {
            let mut data = HashMap::new();
            for (i, col_name) in column_names.iter().enumerate() {
                let value = match row.get_ref(i) {
                    Ok(val) => match val {
                        rusqlite::types::ValueRef::Null => serde_json::Value::Null,
                        rusqlite::types::ValueRef::Integer(v) => serde_json::Value::Number(v.into()),
                        rusqlite::types::ValueRef::Real(v) => {
                            serde_json::Number::from_f64(v)
                                .map(serde_json::Value::Number)
                                .unwrap_or(serde_json::Value::Null)
                        }
                        rusqlite::types::ValueRef::Text(v) => {
                            serde_json::Value::String(String::from_utf8_lossy(v).to_string())
                        }
                        rusqlite::types::ValueRef::Blob(v) => {
                            serde_json::Value::String(format!("<BLOB {} bytes>", v.len()))
                        }
                    },
                    Err(_) => serde_json::Value::Null,
                };
                data.insert(col_name.clone(), value);
            }
            Ok(TableRow { data })
        })
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    Ok(rows)
}

// Execute custom SQL query (SELECT only)
#[tauri::command]
pub async fn db_admin_execute_query(
    db_path: String,
    query: String,
) -> Result<Vec<TableRow>, String> {
    // Security: Only allow SELECT queries — reject anything that could modify data
    let query_trimmed = query.trim();
    let query_upper = query_trimmed.to_uppercase();
    if !query_upper.starts_with("SELECT") {
        return Err("Only SELECT queries are allowed".to_string());
    }
    // Reject queries containing multiple statements or dangerous keywords after SELECT
    let dangerous_keywords = ["INSERT", "UPDATE", "DELETE", "DROP", "ALTER", "CREATE", "ATTACH", "DETACH", "PRAGMA"];
    // Check for semicolons (multi-statement) — only allow the trailing one
    let without_trailing = query_trimmed.trim_end_matches(';').trim();
    if without_trailing.contains(';') {
        return Err("Multiple SQL statements are not allowed".to_string());
    }
    // Check for dangerous keywords in the rest of the query (skip the leading SELECT)
    let rest_upper = query_upper.get(6..).unwrap_or("");
    for kw in &dangerous_keywords {
        // Match whole word boundaries to avoid false positives (e.g. column named "updated")
        if rest_upper.split_whitespace().any(|word| word == *kw) {
            return Err(format!("Query contains disallowed keyword: {}", kw));
        }
    }

    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;
    let mut stmt = conn.prepare(query_trimmed).map_err(|e| e.to_string())?;

    let column_count = stmt.column_count();
    let column_names: Vec<String> = (0..column_count)
        .map(|i| stmt.column_name(i).unwrap_or("").to_string())
        .collect();

    let rows = stmt
        .query_map([], |row| {
            let mut data = HashMap::new();
            for (i, col_name) in column_names.iter().enumerate() {
                let value = match row.get_ref(i) {
                    Ok(val) => match val {
                        rusqlite::types::ValueRef::Null => serde_json::Value::Null,
                        rusqlite::types::ValueRef::Integer(v) => serde_json::Value::Number(v.into()),
                        rusqlite::types::ValueRef::Real(v) => {
                            serde_json::Number::from_f64(v)
                                .map(serde_json::Value::Number)
                                .unwrap_or(serde_json::Value::Null)
                        }
                        rusqlite::types::ValueRef::Text(v) => {
                            serde_json::Value::String(String::from_utf8_lossy(v).to_string())
                        }
                        rusqlite::types::ValueRef::Blob(v) => {
                            serde_json::Value::String(format!("<BLOB {} bytes>", v.len()))
                        }
                    },
                    Err(_) => serde_json::Value::Null,
                };
                data.insert(col_name.clone(), value);
            }
            Ok(TableRow { data })
        })
        .map_err(|e| e.to_string())?
        .filter_map(|r| r.ok())
        .collect();

    Ok(rows)
}

// Update row
#[tauri::command]
pub async fn db_admin_update_row(
    db_path: String,
    table_name: String,
    row_data: HashMap<String, serde_json::Value>,
    primary_key_column: String,
    primary_key_value: serde_json::Value,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    validate_sql_identifier(&primary_key_column, "Primary key column")?;
    for col in row_data.keys() {
        validate_sql_identifier(col, "Column name")?;
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let mut set_clauses = Vec::new();
    for (col, _) in &row_data {
        if col != &primary_key_column {
            set_clauses.push(format!("\"{}\" = ?", col));
        }
    }

    let query = format!(
        "UPDATE \"{}\" SET {} WHERE \"{}\" = ?",
        table_name,
        set_clauses.join(", "),
        primary_key_column
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let mut params: Vec<rusqlite::types::Value> = Vec::new();

    for (col, val) in &row_data {
        if col != &primary_key_column {
            params.push(json_to_sql_value(val));
        }
    }
    params.push(json_to_sql_value(&primary_key_value));

    stmt.execute(rusqlite::params_from_iter(params))
        .map_err(|e| e.to_string())?;

    Ok("Row updated successfully".to_string())
}

// Insert row
#[tauri::command]
pub async fn db_admin_insert_row(
    db_path: String,
    table_name: String,
    row_data: HashMap<String, serde_json::Value>,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    for col in row_data.keys() {
        validate_sql_identifier(col, "Column name")?;
    }
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let columns: Vec<String> = row_data.keys().map(|k| format!("\"{}\"", k)).collect();
    let placeholders: Vec<String> = (0..row_data.len()).map(|_| "?".to_string()).collect();

    let query = format!(
        "INSERT INTO \"{}\" ({}) VALUES ({})",
        table_name,
        columns.join(", "),
        placeholders.join(", ")
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    let params: Vec<rusqlite::types::Value> = row_data.values().map(json_to_sql_value).collect();

    stmt.execute(rusqlite::params_from_iter(params))
        .map_err(|e| e.to_string())?;

    Ok("Row inserted successfully".to_string())
}

// Delete row
#[tauri::command]
pub async fn db_admin_delete_row(
    db_path: String,
    table_name: String,
    primary_key_column: String,
    primary_key_value: serde_json::Value,
) -> Result<String, String> {
    validate_sql_identifier(&table_name, "Table name")?;
    validate_sql_identifier(&primary_key_column, "Primary key column")?;
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!(
        "DELETE FROM \"{}\" WHERE \"{}\" = ?",
        table_name, primary_key_column
    );

    let mut stmt = conn.prepare(&query).map_err(|e| e.to_string())?;
    stmt.execute([json_to_sql_value(&primary_key_value)])
        .map_err(|e| e.to_string())?;

    Ok("Row deleted successfully".to_string())
}

// Rename table
#[tauri::command]
pub async fn db_admin_rename_table(
    db_path: String,
    old_name: String,
    new_name: String,
) -> Result<String, String> {
    validate_sql_identifier(&old_name, "Old table name")?;
    validate_sql_identifier(&new_name, "New table name")?;
    let conn = Connection::open(&db_path).map_err(|e| e.to_string())?;

    let query = format!("ALTER TABLE \"{}\" RENAME TO \"{}\"", old_name, new_name);
    conn.execute(&query, []).map_err(|e| e.to_string())?;

    Ok("Table renamed successfully".to_string())
}

/// Validates a SQL identifier (table name, column name) to prevent injection.
/// Only allows alphanumeric characters, underscores, and spaces (for quoted identifiers).
/// Rejects empty strings and strings containing quotes, semicolons, or other dangerous chars.
fn validate_sql_identifier(name: &str, kind: &str) -> Result<(), String> {
    if name.is_empty() {
        return Err(format!("{} cannot be empty", kind));
    }
    if name.len() > 128 {
        return Err(format!("{} too long (max 128 chars)", kind));
    }
    // Reject characters that could break out of double-quoted identifiers
    if name.contains('"') || name.contains(';') || name.contains('\\')
        || name.contains('\0') || name.contains('\'')
    {
        return Err(format!("{} contains invalid characters: {}", kind, name));
    }
    Ok(())
}

// Helper function to convert JSON value to SQL value
fn json_to_sql_value(val: &serde_json::Value) -> rusqlite::types::Value {
    match val {
        serde_json::Value::Null => rusqlite::types::Value::Null,
        serde_json::Value::Bool(b) => rusqlite::types::Value::Integer(if *b { 1 } else { 0 }),
        serde_json::Value::Number(n) => {
            if let Some(i) = n.as_i64() {
                rusqlite::types::Value::Integer(i)
            } else if let Some(f) = n.as_f64() {
                rusqlite::types::Value::Real(f)
            } else {
                rusqlite::types::Value::Null
            }
        }
        serde_json::Value::String(s) => rusqlite::types::Value::Text(s.clone()),
        _ => rusqlite::types::Value::Text(val.to_string()),
    }
}
