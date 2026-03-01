// commands/brokers/groww/auth.rs - Authentication commands

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use serde_json::{json, Value};
use super::super::common::{ApiResponse, TokenExchangeResponse};
use super::{GROWW_API_BASE, create_groww_headers, generate_totp};

/// Get access token using API key and secret with TOTP flow
#[tauri::command]
pub async fn groww_get_access_token(
    api_key: String,
    api_secret: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[groww_get_access_token] Getting access token via TOTP");

    // Generate TOTP from API secret
    let totp = generate_totp(&api_secret)?;

    let client = reqwest::Client::new();

    let payload = json!({
        "totp": totp
    });

    let mut headers = HeaderMap::new();
    let auth_value = format!("Bearer {}", api_key);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));

    let response = client
        .post(format!("{}/v1/token/api/access", GROWW_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    if status.is_success() {
        // Based on Groww SDK, expect 'token' field in response
        if let Some(token) = body.get("token").and_then(|t| t.as_str()) {
            Ok(TokenExchangeResponse {
                success: true,
                access_token: Some(token.to_string()),
                user_id: None,
                error: None,
            })
        } else {
            Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some("No token in response".to_string()),
            })
        }
    } else {
        let error_msg = body.get("message")
            .or_else(|| body.get("error"))
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

/// Validate access token
#[tauri::command]
pub async fn groww_validate_token(
    access_token: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[groww_validate_token] Validating access token");

    let client = reqwest::Client::new();
    let headers = create_groww_headers(&access_token);

    // Use margins endpoint to validate token (lightweight check)
    let response = client
        .get(format!("{}/v1/margins/detail/user", GROWW_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let body: Value = response.json().await.unwrap_or(json!({}));
        let is_valid = body.get("status").and_then(|s| s.as_str()) == Some("SUCCESS");

        Ok(ApiResponse {
            success: is_valid,
            data: Some(is_valid),
            error: if is_valid { None } else { Some("Token validation failed".to_string()) },
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
