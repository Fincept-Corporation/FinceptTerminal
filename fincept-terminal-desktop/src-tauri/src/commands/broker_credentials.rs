#![allow(dead_code)]
// Broker Credentials Tauri Commands
// Expose secure credential storage to the frontend

use crate::database::broker_credentials::{self, BrokerCredentials};
use crate::database::pool;
use serde::{Deserialize, Serialize};

/// Request structure for saving credentials
#[derive(Debug, Deserialize)]
pub struct SaveCredentialsRequest {
    pub broker_id: String,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub access_token: Option<String>,
    pub refresh_token: Option<String>,
    pub additional_data: Option<String>,
}

/// Response structure for credentials (without sensitive data for logging)
#[derive(Debug, Serialize)]
pub struct CredentialsResponse {
    pub broker_id: String,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub access_token: Option<String>,
    pub refresh_token: Option<String>,
    pub additional_data: Option<String>,
    pub created_at: i64,
    pub updated_at: i64,
}

impl From<BrokerCredentials> for CredentialsResponse {
    fn from(creds: BrokerCredentials) -> Self {
        Self {
            broker_id: creds.broker_id,
            api_key: creds.api_key,
            api_secret: creds.api_secret,
            access_token: creds.access_token,
            refresh_token: creds.refresh_token,
            additional_data: creds.additional_data,
            created_at: creds.created_at,
            updated_at: creds.updated_at,
        }
    }
}

/// Save broker credentials (encrypted)
#[tauri::command]
pub async fn save_broker_credentials(
    broker_id: String,
    api_key: Option<String>,
    api_secret: Option<String>,
    access_token: Option<String>,
    refresh_token: Option<String>,
    additional_data: Option<String>,
) -> Result<(), String> {
    let pool = pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let creds = BrokerCredentials {
        id: None,
        broker_id,
        api_key,
        api_secret,
        access_token,
        refresh_token,
        additional_data,
        encrypted: true,
        created_at: 0, // Will be set by save_credentials
        updated_at: 0, // Will be set by save_credentials
    };

    broker_credentials::save_credentials(&conn, &creds).map_err(|e| e.to_string())?;

    Ok(())
}

/// Get broker credentials by broker_id
#[tauri::command]
pub async fn get_broker_credentials(broker_id: String) -> Result<CredentialsResponse, String> {
    let pool = pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let creds = broker_credentials::get_credentials(&conn, &broker_id)
        .map_err(|e| e.to_string())?
        .ok_or_else(|| format!("No credentials found for broker: {}", broker_id))?;

    Ok(creds.into())
}

/// Delete broker credentials
#[tauri::command]
pub async fn delete_broker_credentials(broker_id: String) -> Result<(), String> {
    let pool = pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    broker_credentials::delete_credentials(&conn, &broker_id).map_err(|e| e.to_string())?;

    Ok(())
}

/// List all saved broker IDs
#[tauri::command]
pub async fn list_broker_credentials() -> Result<Vec<String>, String> {
    let pool = pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let broker_ids = broker_credentials::list_all_credentials(&conn).map_err(|e| e.to_string())?;

    Ok(broker_ids)
}
