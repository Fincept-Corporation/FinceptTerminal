// Generic Key-Value Storage Commands
// Tauri commands for SQLite key-value storage

use crate::database::storage;
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct StorageEntry {
    pub key: String,
    pub value: Option<String>,
}

/// Set a storage value
#[tauri::command]
pub async fn storage_set(key: String, value: String) -> Result<(), String> {
    storage::storage_set(&key, &value).map_err(|e| format!("Storage set failed: {}", e))
}

/// Get a storage value
#[tauri::command]
pub async fn storage_get(key: String) -> Result<String, String> {
    let value = storage::storage_get(&key).map_err(|e| format!("Storage get failed: {}", e))?;

    value.ok_or_else(|| format!("Key not found: {}", key))
}

/// Get a storage value (returns null if not found)
#[tauri::command]
pub async fn storage_get_optional(key: String) -> Result<Option<String>, String> {
    storage::storage_get(&key).map_err(|e| format!("Storage get failed: {}", e))
}

/// Remove a storage value
#[tauri::command]
pub async fn storage_remove(key: String) -> Result<(), String> {
    storage::storage_remove(&key).map_err(|e| format!("Storage remove failed: {}", e))
}

/// Clear all storage (use with caution)
#[tauri::command]
pub async fn storage_clear() -> Result<(), String> {
    storage::storage_clear().map_err(|e| format!("Storage clear failed: {}", e))
}

/// Check if a key exists
#[tauri::command]
pub async fn storage_has(key: String) -> Result<bool, String> {
    storage::storage_has(&key).map_err(|e| format!("Storage has failed: {}", e))
}

/// Get all storage keys
#[tauri::command]
pub async fn storage_keys() -> Result<Vec<String>, String> {
    storage::storage_keys().map_err(|e| format!("Storage keys failed: {}", e))
}

/// Get storage size (number of entries)
#[tauri::command]
pub async fn storage_size() -> Result<usize, String> {
    storage::storage_size().map_err(|e| format!("Storage size failed: {}", e))
}

/// Get multiple values at once (batch operation)
#[tauri::command]
pub async fn storage_get_many(keys: Vec<String>) -> Result<Vec<StorageEntry>, String> {
    let results = storage::storage_get_many(&keys)
        .map_err(|e| format!("Storage get_many failed: {}", e))?;

    Ok(results
        .into_iter()
        .map(|(key, value)| StorageEntry { key, value })
        .collect())
}

/// Set multiple values at once (batch operation)
#[tauri::command]
pub async fn storage_set_many(entries: Vec<(String, String)>) -> Result<(), String> {
    storage::storage_set_many(&entries).map_err(|e| format!("Storage set_many failed: {}", e))
}
