// commands/brokers/dhan/auth.rs - Authentication commands

use serde_json::{json, Value};
use super::{ApiResponse, DHAN_AUTH_BASE, get_timestamp, dhan_request};

/// Generate consent URL for Dhan OAuth login
#[tauri::command]
pub async fn dhan_generate_consent(
    api_key: String,
    api_secret: String,
    client_id: String,
) -> Result<ApiResponse<Value>, String> {
    let client = reqwest::Client::new();
    let url = format!(
        "{}/app/generate-consent?client_id={}",
        DHAN_AUTH_BASE, client_id
    );

    let response = client
        .post(&url)
        .header("app_id", &api_key)
        .header("app_secret", &api_secret)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let data: Value = response.json().await.map_err(|e| e.to_string())?;

    if data.get("status").and_then(|s| s.as_str()) == Some("success") {
        let consent_app_id = data.get("consentAppId").cloned();
        let login_url = consent_app_id.as_ref().and_then(|id| id.as_str()).map(|id| {
            format!(
                "{}/login/consentApp-login?consentAppId={}",
                DHAN_AUTH_BASE, id
            )
        });

        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "consent_app_id": consent_app_id,
                "login_url": login_url
            })),
            error: None,
            timestamp: get_timestamp(),
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to generate consent: {:?}", data)),
            timestamp: get_timestamp(),
        })
    }
}

/// Exchange token ID for access token
#[tauri::command]
pub async fn dhan_exchange_token(
    api_key: String,
    api_secret: String,
    token_id: String,
) -> Result<ApiResponse<Value>, String> {
    let client = reqwest::Client::new();
    let url = format!(
        "{}/app/consumeApp-consent?tokenId={}",
        DHAN_AUTH_BASE, token_id
    );

    let response = client
        .post(&url)
        .header("app_id", &api_key)
        .header("app_secret", &api_secret)
        .header("Content-Type", "application/json")
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let data: Value = response.json().await.map_err(|e| e.to_string())?;

    if let Some(access_token) = data.get("accessToken").and_then(|t| t.as_str()) {
        Ok(ApiResponse {
            success: true,
            data: Some(json!({
                "access_token": access_token,
                "client_id": data.get("dhanClientId"),
                "client_name": data.get("dhanClientName"),
                "expiry": data.get("expiryTime")
            })),
            error: None,
            timestamp: get_timestamp(),
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to get access token".to_string()),
            timestamp: get_timestamp(),
        })
    }
}

/// Validate access token by fetching funds
#[tauri::command]
pub async fn dhan_validate_token(
    access_token: String,
    client_id: String,
) -> Result<ApiResponse<bool>, String> {
    match dhan_request("/v2/fundlimit", "GET", &access_token, &client_id, None).await {
        Ok(data) => {
            let is_error = data.get("errorType").is_some()
                || data.get("status").and_then(|s| s.as_str()) == Some("error");

            Ok(ApiResponse {
                success: !is_error,
                data: Some(!is_error),
                error: if is_error {
                    Some("Invalid or expired token".to_string())
                } else {
                    None
                },
                timestamp: get_timestamp(),
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some(e),
            timestamp: get_timestamp(),
        }),
    }
}
