// Custom Index Commands - CRUD operations for aggregate index feature
use crate::database::operations;
use crate::database::types::{CustomIndex, IndexConstituent, IndexSnapshot};
use uuid::Uuid;

#[tauri::command]
pub async fn custom_index_create(
    name: String,
    description: Option<String>,
    calculation_method: String,
    base_value: f64,
    cap_weight: Option<f64>,
    currency: String,
    portfolio_id: Option<String>,
) -> Result<CustomIndex, String> {
    let id = Uuid::new_v4().to_string();
    let now = chrono::Utc::now().to_rfc3339();

    let index = CustomIndex {
        id: id.clone(),
        name,
        description,
        calculation_method,
        base_value,
        base_date: now.clone(),
        divisor: 1.0,
        current_value: base_value,
        previous_close: base_value,
        cap_weight,
        currency,
        portfolio_id,
        is_active: true,
        created_at: now.clone(),
        updated_at: now,
    };

    operations::create_custom_index(&index).map_err(|e| e.to_string())?;

    Ok(index)
}

#[tauri::command]
pub async fn custom_index_get_all() -> Result<Vec<CustomIndex>, String> {
    operations::get_all_custom_indices().map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn custom_index_get_by_id(index_id: String) -> Result<Option<CustomIndex>, String> {
    operations::get_custom_index_by_id(&index_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn custom_index_update(
    index_id: String,
    current_value: f64,
    previous_close: f64,
    divisor: f64,
) -> Result<(), String> {
    operations::update_custom_index(&index_id, current_value, previous_close, divisor)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn custom_index_delete(index_id: String) -> Result<(), String> {
    operations::delete_custom_index(&index_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn custom_index_hard_delete(index_id: String) -> Result<(), String> {
    operations::hard_delete_custom_index(&index_id).map_err(|e| e.to_string())
}

// ============================================================================
// Constituent Commands
// ============================================================================

#[tauri::command]
pub async fn index_constituent_add(
    index_id: String,
    symbol: String,
    shares: Option<f64>,
    weight: Option<f64>,
    market_cap: Option<f64>,
    fundamental_score: Option<f64>,
    custom_price: Option<f64>,
    price_date: Option<String>,
) -> Result<IndexConstituent, String> {
    let id = Uuid::new_v4().to_string();
    let now = chrono::Utc::now().to_rfc3339();

    let constituent = IndexConstituent {
        id: id.clone(),
        index_id: index_id.clone(),
        symbol: symbol.to_uppercase(),
        shares,
        weight,
        market_cap,
        fundamental_score,
        custom_price,
        price_date,
        added_at: now.clone(),
        updated_at: now,
    };

    operations::add_index_constituent(&constituent).map_err(|e| e.to_string())?;

    Ok(constituent)
}

#[tauri::command]
pub async fn index_constituent_get_all(index_id: String) -> Result<Vec<IndexConstituent>, String> {
    operations::get_index_constituents(&index_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn index_constituent_update(
    constituent_id: String,
    shares: Option<f64>,
    weight: Option<f64>,
    market_cap: Option<f64>,
    fundamental_score: Option<f64>,
    custom_price: Option<f64>,
    price_date: Option<String>,
) -> Result<(), String> {
    operations::update_index_constituent(&constituent_id, shares, weight, market_cap, fundamental_score, custom_price, price_date)
        .map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn index_constituent_remove(index_id: String, symbol: String) -> Result<(), String> {
    operations::remove_index_constituent(&index_id, &symbol).map_err(|e| e.to_string())
}

// ============================================================================
// Snapshot Commands
// ============================================================================

#[tauri::command]
pub async fn index_snapshot_save(
    index_id: String,
    index_value: f64,
    day_change: f64,
    day_change_percent: f64,
    total_market_value: f64,
    divisor: f64,
    constituents_data: String,
    snapshot_date: String,
) -> Result<IndexSnapshot, String> {
    let id = Uuid::new_v4().to_string();
    let now = chrono::Utc::now().to_rfc3339();

    let snapshot = IndexSnapshot {
        id: id.clone(),
        index_id,
        index_value,
        day_change,
        day_change_percent,
        total_market_value,
        divisor,
        constituents_data,
        snapshot_date,
        created_at: now,
    };

    operations::save_index_snapshot(&snapshot).map_err(|e| e.to_string())?;

    Ok(snapshot)
}

#[tauri::command]
pub async fn index_snapshot_get_all(
    index_id: String,
    limit: Option<i32>,
) -> Result<Vec<IndexSnapshot>, String> {
    operations::get_index_snapshots(&index_id, limit).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn index_snapshot_get_latest(index_id: String) -> Result<Option<IndexSnapshot>, String> {
    operations::get_latest_index_snapshot(&index_id).map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn index_snapshot_cleanup(index_id: String, days: i32) -> Result<i32, String> {
    operations::delete_index_snapshots_older_than(&index_id, days).map_err(|e| e.to_string())
}
