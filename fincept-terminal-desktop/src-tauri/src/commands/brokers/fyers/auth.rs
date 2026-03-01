// Fyers Authentication Commands

use super::FYERS_API_BASE;
use super::super::common::TokenExchangeResponse;
use reqwest::header::CONTENT_TYPE;
use serde_json::{json, Value};
use sha2::{Sha256, Digest};

/// Exchange authorization code for access token
#[tauri::command]
pub async fn fyers_exchange_token(
    api_key: String,
    api_secret: String,
    auth_code: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[fyers_exchange_token] Exchanging authorization code");

    let checksum_input = format!("{}:{}", api_key, api_secret);
    let mut hasher = Sha256::new();
    hasher.update(checksum_input.as_bytes());
    let app_id_hash = format!("{:x}", hasher.finalize());

    let client = reqwest::Client::new();
    let payload = json!({
        "grant_type": "authorization_code",
        "appIdHash": app_id_hash,
        "code": auth_code
    });

    let response = client
        .post(format!("{}/api/v3/validate-authcode", FYERS_API_BASE))
        .header(CONTENT_TYPE, "application/json")
        .header("Accept", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("s").and_then(|s| s.as_str()) == Some("ok") {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("access_token").and_then(|v| v.as_str()).map(String::from),
            user_id: body.get("user_id").and_then(|v| v.as_str()).map(String::from),
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
