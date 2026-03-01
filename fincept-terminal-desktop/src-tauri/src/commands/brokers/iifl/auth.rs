// IIFL Authentication Commands

use super::{IIFL_INTERACTIVE_URL, IIFL_MARKET_DATA_URL, create_iifl_headers};
use super::super::common::{ApiResponse, TokenExchangeResponse};
use reqwest::header::CONTENT_TYPE;
use serde_json::{json, Value};

/// Exchange credentials for session token
#[tauri::command]
pub async fn iifl_exchange_token(
    api_key: String,
    api_secret: String,
    _request_token: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[iifl_exchange_token] Authenticating with IIFL");

    let client = reqwest::Client::new();

    let payload = json!({
        "appKey": api_key,
        "secretKey": api_secret,
        "source": "WebAPI"
    });

    let response = client
        .post(format!("{}/user/session", IIFL_INTERACTIVE_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").ok_or("No result in response")?;
        let token = result
            .get("token")
            .and_then(|v| v.as_str())
            .map(String::from);
        let user_id = result
            .get("userID")
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(TokenExchangeResponse {
            success: true,
            access_token: token,
            user_id,
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .or(body.get("message"))
            .and_then(|m| m.as_str())
            .unwrap_or("Authentication failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Get feed token for market data
#[tauri::command]
pub async fn iifl_get_feed_token(
    api_key: String,
    api_secret: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[iifl_get_feed_token] Getting feed token");

    let client = reqwest::Client::new();

    let payload = json!({
        "appKey": api_key,
        "secretKey": api_secret,
        "source": "WebAPI"
    });

    let response = client
        .post(format!("{}/auth/login", IIFL_MARKET_DATA_URL))
        .header(CONTENT_TYPE, "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() && body.get("type").and_then(|t| t.as_str()) == Some("success") {
        let result = body.get("result").ok_or("No result in response")?;
        let token = result
            .get("token")
            .and_then(|v| v.as_str())
            .map(String::from);
        let user_id = result
            .get("userID")
            .and_then(|v| v.as_str())
            .map(String::from);

        Ok(TokenExchangeResponse {
            success: true,
            access_token: token,
            user_id,
            error: None,
        })
    } else {
        let error_msg = body
            .get("description")
            .and_then(|m| m.as_str())
            .unwrap_or("Feed token request failed")
            .to_string();
        Ok(TokenExchangeResponse {
            success: false,
            access_token: None,
            user_id: None,
            error: Some(error_msg),
        })
    }
}

/// Validate access token by fetching user balance
#[tauri::command]
pub async fn iifl_validate_token(access_token: String) -> Result<ApiResponse<bool>, String> {
    eprintln!("[iifl_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_iifl_headers(&access_token);

    let response = client
        .get(format!("{}/user/balance", IIFL_INTERACTIVE_URL))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Token validation failed".to_string()),
            timestamp,
        })
    }
}
