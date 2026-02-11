//! Common broker utility functions
//! 
//! Shared credential management functions for all Indian brokers

use serde_json::{json, Value};
use super::common::ApiResponse;

#[tauri::command]
pub async fn store_indian_broker_credentials(
    broker_id: String,
    credentials: Value,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[store_indian_broker_credentials] Storing credentials for: {}", broker_id);
    eprintln!("[store_indian_broker_credentials] Received JSON: {}", credentials);

    // This wraps the existing broker_credentials functionality
    // with Indian broker specific handling
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Extract fields from credentials JSON
    // IMPORTANT: Handle BOTH camelCase (from TypeScript) and snake_case (from Rust)
    // Tauri may or may not convert camelCase to snake_case depending on serialization
    let api_key = credentials.get("apiKey")
        .or_else(|| credentials.get("api_key"))
        .and_then(|v| v.as_str())
        .map(String::from);

    eprintln!("[store_indian_broker_credentials] Extracted api_key: {:?} (len: {})",
        api_key.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
        api_key.as_ref().map(|s| s.len()).unwrap_or(0));

    let api_secret = credentials.get("apiSecret")
        .or_else(|| credentials.get("api_secret"))
        .and_then(|v| v.as_str())
        .map(String::from);

    eprintln!("[store_indian_broker_credentials] Extracted api_secret: {:?} (len: {})",
        api_secret.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
        api_secret.as_ref().map(|s| s.len()).unwrap_or(0));

    let access_token = credentials.get("accessToken")
        .or_else(|| credentials.get("access_token"))
        .and_then(|v| v.as_str())
        .map(String::from);

    eprintln!("[store_indian_broker_credentials] Extracted access_token: {:?}",
        access_token.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }));

    let user_id = credentials.get("userId")
        .or_else(|| credentials.get("user_id"))
        .and_then(|v| v.as_str())
        .map(String::from);

    // Get token timestamp for Indian broker midnight expiry check
    let token_timestamp = credentials.get("tokenTimestamp")
        .or_else(|| credentials.get("token_timestamp"))
        .and_then(|v| v.as_i64())
        .unwrap_or_else(|| chrono::Utc::now().timestamp_millis());

    // Store additional data as JSON (includes tokenTimestamp for expiry check)
    let additional_data = json!({
        "userId": user_id,
        "expiresAt": credentials.get("expiresAt").or_else(|| credentials.get("expires_at")),
        "tokenTimestamp": token_timestamp,
    }).to_string();

    // Use the existing save_broker_credentials command
    use crate::commands::broker_credentials::save_broker_credentials;

    save_broker_credentials(
        broker_id.clone(),
        api_key,
        api_secret,
        access_token,
        None, // refresh_token
        Some(additional_data),
    ).await.map_err(|e| format!("Failed to save credentials: {}", e))?;

    eprintln!("[store_indian_broker_credentials] ✓ Successfully stored credentials for: {}", broker_id);

    Ok(ApiResponse {
        success: true,
        data: Some(true),
        error: None,
        timestamp,
    })
}

/// Get broker credentials
#[tauri::command]
pub async fn get_indian_broker_credentials(
    broker_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[get_indian_broker_credentials] Getting credentials for: {}", broker_id);

    use crate::commands::broker_credentials::get_broker_credentials;

    let timestamp = chrono::Utc::now().timestamp_millis();

    match get_broker_credentials(broker_id.clone()).await {
        Ok(creds) => {
            // Debug: Print actual credential values before JSON serialization
            eprintln!("[get_indian_broker_credentials] Retrieved credentials:");
            eprintln!("  - broker_id: {}", creds.broker_id);
            eprintln!("  - api_key: {:?} (len: {})",
                creds.api_key.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
                creds.api_key.as_ref().map(|s| s.len()).unwrap_or(0));
            eprintln!("  - api_secret: {:?} (len: {})",
                creds.api_secret.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
                creds.api_secret.as_ref().map(|s| s.len()).unwrap_or(0));
            eprintln!("  - access_token: {:?}",
                creds.access_token.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }));

            // Parse additional_data JSON to extract userId and tokenTimestamp
            let additional_data: Option<Value> = creds.additional_data.as_ref()
                .and_then(|data| serde_json::from_str(data).ok());

            let user_id = additional_data.as_ref()
                .and_then(|data| data.get("userId"))
                .and_then(|v| v.as_str())
                .map(String::from);

            let token_timestamp = additional_data.as_ref()
                .and_then(|data| data.get("tokenTimestamp"))
                .and_then(|v| v.as_i64());

            eprintln!("[get_indian_broker_credentials] Extracted user_id: {:?}, token_timestamp: {:?}", user_id, token_timestamp);

            // Return camelCase JSON for TypeScript
            let creds_json = json!({
                "brokerId": creds.broker_id,
                "apiKey": creds.api_key,
                "apiSecret": creds.api_secret,
                "accessToken": creds.access_token,
                "userId": user_id,
                "tokenTimestamp": token_timestamp,
                "additionalData": creds.additional_data,
            });

            // Debug: Print the JSON that will be returned
            eprintln!("[get_indian_broker_credentials] Returning JSON with {} keys", creds_json.as_object().map(|o| o.len()).unwrap_or(0));

            Ok(ApiResponse {
                success: true,
                data: Some(creds_json),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            eprintln!("[get_indian_broker_credentials] Error: {}", e);
            Ok(ApiResponse {
                success: false,
                data: None,
                error: Some(e),
                timestamp,
            })
        }
    }
}

/// Delete broker credentials
#[tauri::command]
pub async fn delete_indian_broker_credentials(
    broker_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[delete_indian_broker_credentials] Deleting credentials for: {}", broker_id);

    use crate::commands::broker_credentials::delete_broker_credentials;

    let timestamp = chrono::Utc::now().timestamp_millis();

    match delete_broker_credentials(broker_id).await {
        Ok(_) => {
            Ok(ApiResponse {
                success: true,
                data: Some(true),
                error: None,
                timestamp,
            })
        }
        Err(e) => {
            Ok(ApiResponse {
                success: false,
                data: Some(false),
                error: Some(e),
                timestamp,
            })
        }
    }
}
