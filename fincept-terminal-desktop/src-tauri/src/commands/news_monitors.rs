// News Monitors — SQLite-backed keyword alert watches
// Mirrors the RSS feed CRUD pattern from news.rs

use serde::{Deserialize, Serialize};
use crate::database::pool::get_db;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NewsMonitor {
    pub id: String,
    pub label: String,
    pub keywords: Vec<String>,
    pub color: String,
    pub enabled: bool,
    pub created_at: String,
    pub updated_at: String,
}

// ─── helpers ────────────────────────────────────────────────────────────────

fn keywords_to_json(keywords: &[String]) -> String {
    serde_json::to_string(keywords).unwrap_or_else(|_| "[]".to_string())
}

fn json_to_keywords(json: &str) -> Vec<String> {
    serde_json::from_str(json).unwrap_or_default()
}

// ─── Tauri commands ──────────────────────────────────────────────────────────

/// Return all monitors (both enabled and disabled), ordered by creation date.
#[tauri::command]
pub fn get_news_monitors() -> Result<Vec<NewsMonitor>, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let mut stmt = conn
        .prepare(
            "SELECT id, label, keywords, color, enabled, created_at, updated_at
             FROM news_monitors
             ORDER BY created_at ASC",
        )
        .map_err(|e| format!("Failed to prepare statement: {}", e))?;

    let monitors: Vec<NewsMonitor> = stmt
        .query_map([], |row| {
            let keywords_json: String = row.get(2)?;
            Ok(NewsMonitor {
                id: row.get(0)?,
                label: row.get(1)?,
                keywords: json_to_keywords(&keywords_json),
                color: row.get(3)?,
                enabled: row.get::<_, i32>(4)? == 1,
                created_at: row.get(5)?,
                updated_at: row.get(6)?,
            })
        })
        .map_err(|e| format!("Query failed: {}", e))?
        .filter_map(|r| r.ok())
        .collect();

    Ok(monitors)
}

/// Add a new monitor and return the created record.
#[tauri::command]
pub fn add_news_monitor(
    label: String,
    keywords: Vec<String>,
    color: String,
) -> Result<NewsMonitor, String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let id = format!("monitor-{}", uuid::Uuid::new_v4());
    let now = chrono::Utc::now().to_rfc3339();
    let keywords_json = keywords_to_json(&keywords);

    conn.execute(
        "INSERT INTO news_monitors (id, label, keywords, color, enabled, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, 1, ?5, ?5)",
        rusqlite::params![id, label, keywords_json, color, now],
    )
    .map_err(|e| format!("Failed to insert monitor: {}", e))?;

    Ok(NewsMonitor {
        id,
        label,
        keywords,
        color,
        enabled: true,
        created_at: now.clone(),
        updated_at: now,
    })
}

/// Update an existing monitor's label, keywords, color, and enabled flag.
#[tauri::command]
pub fn update_news_monitor(
    id: String,
    label: String,
    keywords: Vec<String>,
    color: String,
    enabled: bool,
) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();
    let keywords_json = keywords_to_json(&keywords);

    let rows = conn
        .execute(
            "UPDATE news_monitors
             SET label = ?1, keywords = ?2, color = ?3, enabled = ?4, updated_at = ?5
             WHERE id = ?6",
            rusqlite::params![
                label,
                keywords_json,
                color,
                if enabled { 1 } else { 0 },
                now,
                id
            ],
        )
        .map_err(|e| format!("Failed to update monitor: {}", e))?;

    if rows == 0 {
        return Err("Monitor not found".to_string());
    }
    Ok(())
}

/// Delete a monitor permanently.
#[tauri::command]
pub fn delete_news_monitor(id: String) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;

    let rows = conn
        .execute(
            "DELETE FROM news_monitors WHERE id = ?1",
            rusqlite::params![id],
        )
        .map_err(|e| format!("Failed to delete monitor: {}", e))?;

    if rows == 0 {
        return Err("Monitor not found".to_string());
    }
    Ok(())
}

/// Toggle the enabled flag of a monitor.
#[tauri::command]
pub fn toggle_news_monitor(id: String, enabled: bool) -> Result<(), String> {
    let conn = get_db().map_err(|e| format!("Database error: {}", e))?;
    let now = chrono::Utc::now().to_rfc3339();

    let rows = conn
        .execute(
            "UPDATE news_monitors SET enabled = ?1, updated_at = ?2 WHERE id = ?3",
            rusqlite::params![if enabled { 1 } else { 0 }, now, id],
        )
        .map_err(|e| format!("Failed to toggle monitor: {}", e))?;

    if rows == 0 {
        return Err("Monitor not found".to_string());
    }
    Ok(())
}
