// commands/brokers/upstox/auth.rs - Authentication commands

use serde_json::Value;
use super::super::common::{ApiResponse, TokenExchangeResponse};
use super::{UPSTOX_API_BASE_V2, create_upstox_headers};

/// Exchange authorization code for access token
#[tauri::command]
pub async fn upstox_exchange_token(
    api_key: String,
    api_secret: String,
    auth_code: String,
    redirect_uri: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[upstox_exchange_token] Exchanging authorization code");

    let client = reqwest::Client::new();

    let params = [
        ("code", auth_code.as_str()),
        ("client_id", api_key.as_str()),
        ("client_secret", api_secret.as_str()),
        ("redirect_uri", redirect_uri.as_str()),
        ("grant_type", "authorization_code"),
    ];

    let response = client
        .post(format!("{}/login/authorization/token", UPSTOX_API_BASE_V2))
        .form(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[upstox_exchange_token] Response status: {}, body: {:?}", status, body);

    if status.is_success() && body.get("access_token").is_some() {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: body.get("user_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("errors")
            .and_then(|e| e.as_array())
            .and_then(|arr| arr.first())
            .and_then(|e| e.get("message"))
            .and_then(|m| m.as_str())
            .unwrap_or("Token exchange failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Validate existing token by fetching profile
#[tauri::command]
pub async fn upstox_validate_token(access_token: String) -> Result<ApiResponse<Value>, String> {
    eprintln!("[upstox_validate_token] Validating token");

    let client = reqwest::Client::new();
    let headers = create_upstox_headers(&access_token);

    let response = client
        .get(format!("{}/user/profile", UPSTOX_API_BASE_V2))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        Ok(ApiResponse {
            success: true,
            data: body.get("data").cloned(),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Token validation failed".to_string()),
            timestamp,
        })
    }
}
