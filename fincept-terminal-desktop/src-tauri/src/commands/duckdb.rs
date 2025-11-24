use duckdb::{Connection, Result as DuckDBResult};
use serde::{Deserialize, Serialize};
use serde_json::Value as JsonValue;
use std::sync::Mutex;

// State to hold DuckDB connection
pub struct DuckDBState {
    pub conn: Mutex<Connection>,
}

impl DuckDBState {
    pub fn new(db_path: &str) -> DuckDBResult<Self> {
        let conn = Connection::open(db_path)?;
        Ok(Self {
            conn: Mutex::new(conn),
        })
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct QueryResult {
    pub columns: Vec<String>,
    pub rows: Vec<Vec<JsonValue>>,
    pub row_count: usize,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExecuteResult {
    pub rows_affected: u64,
    pub success: bool,
}

/// Execute a query and return results as JSON
#[tauri::command]
pub async fn duckdb_query(
    state: tauri::State<'_, DuckDBState>,
    sql: String,
) -> Result<QueryResult, String> {
    let conn = state.conn.lock().map_err(|e| format!("Lock error: {}", e))?;

    let mut stmt = conn.prepare(&sql).map_err(|e| format!("Prepare error: {}", e))?;

    // Get column names
    let column_count = stmt.column_count();
    let columns: Vec<String> = (0..column_count)
        .map(|i| stmt.column_name(i).map_or("unknown".to_string(), |v| v.to_string()))
        .collect();

    // Execute query and collect rows
    let mut rows_result = stmt.query([]).map_err(|e| format!("Query error: {}", e))?;
    let mut rows: Vec<Vec<JsonValue>> = Vec::new();

    while let Some(row) = rows_result.next().map_err(|e| format!("Row iteration error: {}", e))? {
        let mut row_data: Vec<JsonValue> = Vec::new();

        for i in 0..column_count {
            let value: JsonValue = match row.get_ref(i).map_err(|e| format!("Get value error: {}", e))? {
                duckdb::types::ValueRef::Null => JsonValue::Null,
                duckdb::types::ValueRef::Boolean(b) => JsonValue::Bool(b),
                duckdb::types::ValueRef::TinyInt(i) => JsonValue::Number(i.into()),
                duckdb::types::ValueRef::SmallInt(i) => JsonValue::Number(i.into()),
                duckdb::types::ValueRef::Int(i) => JsonValue::Number(i.into()),
                duckdb::types::ValueRef::BigInt(i) => JsonValue::Number(i.into()),
                duckdb::types::ValueRef::Float(f) => {
                    serde_json::Number::from_f64(f as f64)
                        .map(JsonValue::Number)
                        .unwrap_or(JsonValue::Null)
                }
                duckdb::types::ValueRef::Double(f) => {
                    serde_json::Number::from_f64(f)
                        .map(JsonValue::Number)
                        .unwrap_or(JsonValue::Null)
                }
                duckdb::types::ValueRef::Text(s) => {
                    JsonValue::String(String::from_utf8_lossy(s).to_string())
                }
                duckdb::types::ValueRef::Blob(b) => {
                    JsonValue::String(format!("<blob {} bytes>", b.len()))
                }
                _ => JsonValue::String("<unsupported type>".to_string()),
            };
            row_data.push(value);
        }
        rows.push(row_data);
    }

    Ok(QueryResult {
        columns,
        row_count: rows.len(),
        rows,
    })
}

/// Execute a SQL statement (INSERT, UPDATE, DELETE, CREATE, etc.)
#[tauri::command]
pub async fn duckdb_execute(
    state: tauri::State<'_, DuckDBState>,
    sql: String,
) -> Result<ExecuteResult, String> {
    let conn = state.conn.lock().map_err(|e| format!("Lock error: {}", e))?;

    let rows_affected = conn
        .execute(&sql, [])
        .map_err(|e| format!("Execute error: {}", e))? as u64;

    Ok(ExecuteResult {
        rows_affected,
        success: true,
    })
}

/// Execute multiple SQL statements (for initialization)
#[tauri::command]
pub async fn duckdb_execute_batch(
    state: tauri::State<'_, DuckDBState>,
    sql_statements: Vec<String>,
) -> Result<Vec<ExecuteResult>, String> {
    let conn = state.conn.lock().map_err(|e| format!("Lock error: {}", e))?;

    let mut results = Vec::new();

    for sql in sql_statements {
        let rows_affected = conn
            .execute(&sql, [])
            .map_err(|e| format!("Execute batch error on statement '{}': {}", sql, e))? as u64;

        results.push(ExecuteResult {
            rows_affected,
            success: true,
        });
    }

    Ok(results)
}

/// Get database schema information
#[tauri::command]
pub async fn duckdb_get_tables(
    state: tauri::State<'_, DuckDBState>,
) -> Result<Vec<String>, String> {
    let sql = "SELECT table_name FROM information_schema.tables WHERE table_schema = 'main'";
    let result = duckdb_query(state, sql.to_string()).await?;

    let tables = result.rows
        .iter()
        .filter_map(|row| {
            row.get(0).and_then(|v| v.as_str()).map(|s| s.to_string())
        })
        .collect();

    Ok(tables)
}

/// Get table info (columns, types)
#[tauri::command]
pub async fn duckdb_get_table_info(
    state: tauri::State<'_, DuckDBState>,
    table_name: String,
) -> Result<QueryResult, String> {
    let sql = format!("DESCRIBE {}", table_name);
    duckdb_query(state, sql).await
}

/// Test connection
#[tauri::command]
pub async fn duckdb_test_connection(
    state: tauri::State<'_, DuckDBState>,
) -> Result<bool, String> {
    let sql = "SELECT 1 as test";
    let result = duckdb_query(state, sql.to_string()).await?;
    Ok(result.row_count > 0)
}
