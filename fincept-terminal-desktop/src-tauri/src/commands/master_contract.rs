// Master Contract Cache Commands
// Tauri commands for broker symbol data caching

use crate::database::master_contract;
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct MasterContractInfo {
    pub broker_id: String,
    pub symbol_count: i64,
    pub last_updated: i64,
    pub created_at: i64,
    pub cache_age_seconds: i64,
}

/// Save master contract cache
#[tauri::command]
pub async fn save_master_contract_cache(
    broker_id: String,
    symbols_data: String,
    symbol_count: i64,
) -> Result<String, String> {
    master_contract::save_master_contract(&broker_id, &symbols_data, symbol_count)
        .map_err(|e| format!("Failed to save master contract: {}", e))?;

    Ok(format!(
        "Master contract saved for {} ({} symbols)",
        broker_id, symbol_count
    ))
}

/// Get master contract cache
#[tauri::command]
pub async fn get_master_contract_cache(broker_id: String) -> Result<String, String> {
    let cache = master_contract::get_master_contract(&broker_id)
        .map_err(|e| format!("Failed to get master contract: {}", e))?;

    match cache {
        Some(data) => Ok(data.symbols_data),
        None => Err(format!("No master contract found for {}", broker_id)),
    }
}

/// Delete master contract cache
#[tauri::command]
pub async fn delete_master_contract_cache(broker_id: String) -> Result<String, String> {
    master_contract::delete_master_contract(&broker_id)
        .map_err(|e| format!("Failed to delete master contract: {}", e))?;

    Ok(format!("Master contract deleted for {}", broker_id))
}

/// Check if cache is valid (not expired)
#[tauri::command]
pub async fn is_master_contract_cache_valid(
    broker_id: String,
    ttl_seconds: i64,
) -> Result<bool, String> {
    master_contract::is_cache_valid(&broker_id, ttl_seconds)
        .map_err(|e| format!("Failed to check cache validity: {}", e))
}

/// Get master contract cache info
#[tauri::command]
pub async fn get_master_contract_cache_info(
    broker_id: String,
) -> Result<MasterContractInfo, String> {
    let cache = master_contract::get_master_contract(&broker_id)
        .map_err(|e| format!("Failed to get master contract: {}", e))?;

    match cache {
        Some(data) => {
            let cache_age = master_contract::get_cache_age(&broker_id)
                .map_err(|e| format!("Failed to get cache age: {}", e))?
                .unwrap_or(0);

            Ok(MasterContractInfo {
                broker_id: data.broker_id,
                symbol_count: data.symbol_count,
                last_updated: data.last_updated,
                created_at: data.created_at,
                cache_age_seconds: cache_age,
            })
        }
        None => Err(format!("No master contract found for {}", broker_id)),
    }
}
