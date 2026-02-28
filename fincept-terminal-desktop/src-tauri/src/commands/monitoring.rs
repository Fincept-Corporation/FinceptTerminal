// Price-alert monitoring Tauri commands.
// Conditions and alerts are persisted in SQLite; the MonitoringService
// evaluates them against the live ticker broadcast stream.

use crate::websocket::services::monitoring::{
    MonitorAlert, MonitorCondition, MonitorField, MonitorOperator,
};
use crate::WebSocketState;
use rusqlite::params;

// ============================================================================
// COMMANDS
// ============================================================================

/// Persist a new monitoring condition and reload the service.
#[tauri::command]
pub async fn monitor_add_condition(
    _app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    condition: MonitorCondition,
) -> Result<i64, String> {
    let pool = crate::database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO monitor_conditions
         (provider, symbol, field, operator, value, value2, enabled)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        params![
            &condition.provider,
            &condition.symbol,
            match condition.field {
                MonitorField::Price => "price",
                MonitorField::Volume => "volume",
                MonitorField::ChangePercent => "change_percent",
                MonitorField::Spread => "spread",
            },
            match condition.operator {
                MonitorOperator::GreaterThan => ">",
                MonitorOperator::LessThan => "<",
                MonitorOperator::GreaterThanOrEqual => ">=",
                MonitorOperator::LessThanOrEqual => "<=",
                MonitorOperator::Equal => "==",
                MonitorOperator::Between => "between",
            },
            condition.value,
            condition.value2,
            if condition.enabled { 1 } else { 0 },
        ],
    )
    .map_err(|e| e.to_string())?;

    let id = conn.last_insert_rowid();

    let services = state.services.read().await;
    let _ = services.monitoring.load_conditions().await;

    Ok(id)
}

/// Return all monitoring conditions ordered by creation time.
#[tauri::command]
pub async fn monitor_get_conditions(
    _app: tauri::AppHandle,
) -> Result<Vec<MonitorCondition>, String> {
    let pool = crate::database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, provider, symbol, field, operator, value, value2, enabled
             FROM monitor_conditions
             ORDER BY created_at DESC",
        )
        .map_err(|e| e.to_string())?;

    let conditions = stmt
        .query_map([], |row| {
            Ok(MonitorCondition {
                id: Some(row.get(0)?),
                provider: row.get(1)?,
                symbol: row.get(2)?,
                field: MonitorField::from_str(&row.get::<_, String>(3)?).unwrap(),
                operator: MonitorOperator::from_str(&row.get::<_, String>(4)?).unwrap(),
                value: row.get(5)?,
                value2: row.get(6)?,
                enabled: row.get::<_, i32>(7)? == 1,
            })
        })
        .map_err(|e| e.to_string())?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|e| e.to_string())?;

    Ok(conditions)
}

/// Delete a monitoring condition by ID and reload the service.
#[tauri::command]
pub async fn monitor_delete_condition(
    _app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    id: i64,
) -> Result<(), String> {
    let pool = crate::database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    conn.execute("DELETE FROM monitor_conditions WHERE id = ?1", params![id])
        .map_err(|e| e.to_string())?;

    let services = state.services.read().await;
    services
        .monitoring
        .load_conditions()
        .await
        .map_err(|e| e.to_string())?;

    Ok(())
}

/// Return the most recent N alerts.
#[tauri::command]
pub async fn monitor_get_alerts(
    _app: tauri::AppHandle,
    limit: i64,
) -> Result<Vec<MonitorAlert>, String> {
    let pool = crate::database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let mut stmt = conn
        .prepare(
            "SELECT id, condition_id, provider, symbol, field, triggered_value, triggered_at
             FROM monitor_alerts
             ORDER BY triggered_at DESC
             LIMIT ?1",
        )
        .map_err(|e| e.to_string())?;

    let alerts = stmt
        .query_map(params![limit], |row| {
            Ok(MonitorAlert {
                id: Some(row.get(0)?),
                condition_id: row.get(1)?,
                provider: row.get(2)?,
                symbol: row.get(3)?,
                field: MonitorField::from_str(&row.get::<_, String>(4)?).unwrap(),
                triggered_value: row.get(5)?,
                triggered_at: row.get::<_, i64>(6)? as u64,
            })
        })
        .map_err(|e| e.to_string())?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|e| e.to_string())?;

    Ok(alerts)
}

/// (Re)load all enabled conditions into the in-memory monitoring service.
#[tauri::command]
pub async fn monitor_load_conditions(
    state: tauri::State<'_, WebSocketState>,
) -> Result<(), String> {
    let services = state.services.read().await;
    services
        .monitoring
        .load_conditions()
        .await
        .map_err(|e| e.to_string())
}
