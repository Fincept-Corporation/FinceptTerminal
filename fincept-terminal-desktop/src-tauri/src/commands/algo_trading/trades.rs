//! Trade history operations

use crate::database::pool::get_db;
use serde_json::json;

#[tauri::command]
pub async fn get_algo_trades(
    deployment_id: String,
    limit: Option<i32>,
) -> Result<String, String> {
    let conn = get_db().map_err(|e| e.to_string())?;
    let row_limit = limit.unwrap_or(100);

    let mut stmt = conn
        .prepare(
            "SELECT id, deployment_id, symbol, side, quantity, price, pnl, timestamp, signal_reason
             FROM algo_trades WHERE deployment_id = ?1
             ORDER BY timestamp DESC LIMIT ?2",
        )
        .map_err(|e| format!("Failed to prepare query: {}", e))?;

    let rows = stmt
        .query_map(rusqlite::params![deployment_id, row_limit], |row| {
            Ok(json!({
                "id": row.get::<_, String>(0)?,
                "deployment_id": row.get::<_, String>(1)?,
                "symbol": row.get::<_, String>(2)?,
                "side": row.get::<_, String>(3)?,
                "quantity": row.get::<_, f64>(4)?,
                "price": row.get::<_, f64>(5)?,
                "pnl": row.get::<_, f64>(6)?,
                "timestamp": row.get::<_, String>(7)?,
                "signal_reason": row.get::<_, String>(8)?,
            }))
        })
        .map_err(|e| format!("Failed to query trades: {}", e))?;

    let trades: Vec<serde_json::Value> = rows.filter_map(|r| r.ok()).collect();

    Ok(json!({
        "success": true,
        "data": trades,
        "count": trades.len()
    }).to_string())
}
