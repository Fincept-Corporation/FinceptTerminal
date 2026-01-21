//! AngelOne (Angel Broking) API Commands
//!
//! Authentication and order management for Angel Broking (AngelOne)

use reqwest::Client;
use serde_json::{json, Value};
use super::common::ApiResponse;

/// Base URL for AngelOne API
const ANGELONE_BASE_URL: &str = "https://apiconnect.angelbroking.com";

/// AngelOne login - authenticate with client code, password (PIN), and TOTP
#[tauri::command]
pub async fn angelone_login(
    api_key: String,
    client_code: String,
    password: String,
    totp: String,
) -> ApiResponse<Value> {
    let client = Client::new();
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "clientcode": client_code,
        "password": password,
        "totp": totp
    });

    let url = format!("{}/rest/auth/angelbroking/user/v1/loginByPassword", ANGELONE_BASE_URL);

    match client
        .post(&url)
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-UserType", "USER")
        .header("X-SourceID", "WEB")
        .header("X-ClientLocalIP", "127.0.0.1")
        .header("X-ClientPublicIP", "127.0.0.1")
        .header("X-MACAddress", "00:00:00:00:00:00")
        .header("X-PrivateKey", &api_key)
        .json(&payload)
        .send()
        .await
    {
        Ok(response) => {
            let status = response.status();

            match response.json::<Value>().await {
                Ok(data) => {
                    if status.is_success() {
                        // Extract tokens from response
                        if let Some(data_obj) = data.get("data") {
                            let jwt_token = data_obj.get("jwtToken")
                                .and_then(|v| v.as_str())
                                .unwrap_or("");
                            let feed_token = data_obj.get("feedToken")
                                .and_then(|v| v.as_str());
                            let refresh_token = data_obj.get("refreshToken")
                                .and_then(|v| v.as_str());

                            if !jwt_token.is_empty() {
                                return ApiResponse {
                                    success: true,
                                    data: Some(json!({
                                        "access_token": jwt_token,
                                        "feed_token": feed_token,
                                        "refresh_token": refresh_token,
                                        "user_id": client_code
                                    })),
                                    error: None,
                                    timestamp,
                                };
                            }
                        }

                        // Authentication failed
                        let error_msg = data.get("message")
                            .and_then(|v| v.as_str())
                            .unwrap_or("Authentication failed");

                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(error_msg.to_string()),
                            timestamp,
                        }
                    } else {
                        let error_msg = data.get("message")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_else(|| format!("HTTP error: {}", status));

                        ApiResponse {
                            success: false,
                            data: None,
                            error: Some(error_msg),
                            timestamp,
                        }
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: None,
                    error: Some(format!("Failed to parse response: {}", e)),
                    timestamp,
                },
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(format!("Request failed: {}", e)),
            timestamp,
        },
    }
}

/// Validate AngelOne access token
#[tauri::command]
pub async fn angelone_validate_token(
    auth_token: String,
    api_key: String,
) -> ApiResponse<bool> {
    let client = Client::new();
    let timestamp = chrono::Utc::now().timestamp_millis();

    // Use profile endpoint to validate token
    let url = format!("{}/rest/secure/angelbroking/user/v1/getProfile", ANGELONE_BASE_URL);

    match client
        .get(&url)
        .header("Authorization", format!("Bearer {}", auth_token))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-UserType", "USER")
        .header("X-SourceID", "WEB")
        .header("X-ClientLocalIP", "127.0.0.1")
        .header("X-ClientPublicIP", "127.0.0.1")
        .header("X-MACAddress", "00:00:00:00:00:00")
        .header("X-PrivateKey", &api_key)
        .send()
        .await
    {
        Ok(response) => {
            let is_valid = response.status().is_success();
            ApiResponse {
                success: is_valid,
                data: Some(is_valid),
                error: if is_valid { None } else { Some("Token validation failed".to_string()) },
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: Some(false),
            error: Some(format!("Validation request failed: {}", e)),
            timestamp,
        },
    }
}
