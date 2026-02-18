//! Strategy CRUD operations

use crate::database::pool::get_db;
use serde_json::json;

/// Save or update an algo strategy (condition-based)
#[tauri::command]
pub async fn save_algo_strategy(
    id: String,
    name: String,
    description: Option<String>,
    entry_conditions: String,
    exit_conditions: String,
    entry_logic: Option<String>,
    exit_logic: Option<String>,
    stop_loss: Option<f64>,
    take_profit: Option<f64>,
    trailing_stop: Option<f64>,
    trailing_stop_type: Option<String>,
    timeframe: Option<String>,
    symbols: Option<String>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO algo_strategies (id, name, description, entry_conditions, exit_conditions,
         entry_logic, exit_logic, stop_loss, take_profit, trailing_stop, trailing_stop_type,
         timeframe, symbols, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, CURRENT_TIMESTAMP)
         ON CONFLICT(id) DO UPDATE SET
            name = excluded.name,
            description = excluded.description,
            entry_conditions = excluded.entry_conditions,
            exit_conditions = excluded.exit_conditions,
            entry_logic = excluded.entry_logic,
            exit_logic = excluded.exit_logic,
            stop_loss = excluded.stop_loss,
            take_profit = excluded.take_profit,
            trailing_stop = excluded.trailing_stop,
            trailing_stop_type = excluded.trailing_stop_type,
            timeframe = excluded.timeframe,
            symbols = excluded.symbols,
            updated_at = CURRENT_TIMESTAMP",
        rusqlite::params![
            id,
            name,
            description.unwrap_or_default(),
            entry_conditions,
            exit_conditions,
            entry_logic.unwrap_or_else(|| "AND".to_string()),
            exit_logic.unwrap_or_else(|| "AND".to_string()),
            stop_loss,
            take_profit,
            trailing_stop,
            trailing_stop_type.unwrap_or_else(|| "percent".to_string()),
            timeframe.unwrap_or_else(|| "5m".to_string()),
            symbols.unwrap_or_else(|| "[]".to_string()),
        ],
    )
    .map_err(|e| format!("Failed to save strategy: {}", e))?;

    Ok(json!({
        "success": true,
        "id": id
    }).to_string())
}

/// List all algo strategies
#[tauri::command]
pub async fn list_algo_strategies() -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, name, description, entry_conditions, exit_conditions,
                    entry_logic, exit_logic, stop_loss, take_profit, trailing_stop,
                    trailing_stop_type, timeframe, symbols, is_active, created_at, updated_at
             FROM algo_strategies ORDER BY updated_at DESC",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map([], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "entry_conditions": row.get::<_, String>(3)?,
                "exit_conditions": row.get::<_, String>(4)?,
                "entry_logic": row.get::<_, String>(5)?,
                "exit_logic": row.get::<_, String>(6)?,
                "stop_loss": row.get::<_, Option<f64>>(7)?,
                "take_profit": row.get::<_, Option<f64>>(8)?,
                "trailing_stop": row.get::<_, Option<f64>>(9)?,
                "trailing_stop_type": row.get::<_, String>(10)?,
                "timeframe": row.get::<_, String>(11)?,
                "symbols": row.get::<_, String>(12)?,
                "is_active": row.get::<_, i32>(13)?,
                "created_at": row.get::<_, String>(14)?,
                "updated_at": row.get::<_, String>(15)?,
            }))
        })
        .map_err(|e| format!("Failed to query strategies: {}", e))?;

    let strategies: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": strategies,
        "count": strategies.len()
    }).to_string())
}

/// Get a single algo strategy by ID
#[tauri::command]
pub async fn get_algo_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let result = conn.query_row(
        "SELECT id, name, description, entry_conditions, exit_conditions,
                entry_logic, exit_logic, stop_loss, take_profit, trailing_stop,
                trailing_stop_type, timeframe, symbols, is_active, created_at, updated_at
         FROM algo_strategies WHERE id = ?1",
        rusqlite::params![id],
        |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "name": row.get::<_, String>(1)?,
                "description": row.get::<_, String>(2)?,
                "entry_conditions": row.get::<_, String>(3)?,
                "exit_conditions": row.get::<_, String>(4)?,
                "entry_logic": row.get::<_, String>(5)?,
                "exit_logic": row.get::<_, String>(6)?,
                "stop_loss": row.get::<_, Option<f64>>(7)?,
                "take_profit": row.get::<_, Option<f64>>(8)?,
                "trailing_stop": row.get::<_, Option<f64>>(9)?,
                "trailing_stop_type": row.get::<_, String>(10)?,
                "timeframe": row.get::<_, String>(11)?,
                "symbols": row.get::<_, String>(12)?,
                "is_active": row.get::<_, i32>(13)?,
                "created_at": row.get::<_, String>(14)?,
                "updated_at": row.get::<_, String>(15)?,
            }))
        },
    );

    match result {
        Ok(strategy) => Ok(json!({ "success": true, "data": strategy }).to_string()),
        Err(rusqlite::Error::QueryReturnedNoRows) => {
            Ok(json!({ "success": false, "error": "Strategy not found" }).to_string())
        }
        Err(e) => Ok(json!({ "success": false, "error": e.to_string() }).to_string()),
    }
}

/// Delete an algo strategy
#[tauri::command]
pub async fn delete_algo_strategy(id: String) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;

    let changes = conn
        .execute("DELETE FROM algo_strategies WHERE id = ?1", rusqlite::params![id])
        .map_err(|e| format!("Failed to delete strategy: {}", e))?;

    Ok(json!({
        "success": changes > 0,
        "deleted": changes > 0
    }).to_string())
}
