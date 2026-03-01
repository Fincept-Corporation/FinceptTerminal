// Zerodha Authentication Commands

use super::{ZERODHA_API_BASE, ZERODHA_API_VERSION, create_zerodha_headers};
use super::super::common::{ApiResponse, TokenExchangeResponse};
use reqwest::header::CONTENT_TYPE;
use serde_json::Value;
use sha2::{Sha256, Digest};

/// Exchange request token for access token
#[tauri::command]
pub async fn zerodha_exchange_token(
    api_key: String,
    api_secret: String,
    request_token: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[zerodha_exchange_token] Exchanging request token");

    let checksum_input = format!("{}{}{}", api_key, request_token, api_secret);
    let mut hasher = Sha256::new();
    hasher.update(checksum_input.as_bytes());
    let checksum = format!("{:x}", hasher.finalize());

    let client = reqwest::Client::new();

    let params = [
        ("api_key", api_key.as_str()),
        ("request_token", request_token.as_str()),
        ("checksum", checksum.as_str()),
    ];

    let response = client
        .post(format!("{}/session/token", ZERODHA_API_BASE))
        .header("X-Kite-Version", ZERODHA_API_VERSION)
        .header(CONTENT_TYPE, "application/x-www-form-urlencoded")
        .form(&params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("status").and_then(|s| s.as_str()) == Some("success") {
        let data = body.get("data").ok_or("No data in response")?;
        Ok(TokenExchangeResponse {
            success: true,
            access_token: data.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: data.get("user_id").and_then(|v| v.as_str()).map(String::from),
            error: None,
        })
    } else {
        let error_msg = body.get("message")
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

/// Validate access token by fetching user profile
#[tauri::command]
pub async fn zerodha_validate_token(
    api_key: String,
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[zerodha_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_zerodha_headers(&api_key, &access_token);

    let response = client
        .get(format!("{}/user/profile", ZERODHA_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Token validation failed".to_string()),
            timestamp,
        })
    }
}
