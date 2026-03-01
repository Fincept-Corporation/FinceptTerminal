// AngelOne Authentication Commands

use reqwest::Client;
use serde_json::{json, Value};
use super::super::common::ApiResponse;
use super::{ANGELONE_BASE_URL, API_TIMEOUT, build_angel_request, angel_api_call, generate_totp_code};

/// AngelOne login - authenticate with client code, password (PIN), and TOTP secret
#[tauri::command]
pub async fn angelone_login(
    api_key: String,
    client_code: String,
    password: String,
    totp: String,
) -> ApiResponse<Value> {
    let client = match Client::builder().timeout(API_TIMEOUT).build() {
        Ok(c) => c,
        Err(e) => {
            return ApiResponse {
                success: false,
                data: None,
                error: Some(format!("Failed to create HTTP client: {}", e)),
                timestamp: chrono::Utc::now().timestamp_millis(),
            };
        }
    };
    let timestamp = chrono::Utc::now().timestamp_millis();

    let totp_code = match generate_totp_code(&totp) {
        Ok(code) => code,
        Err(e) => {
            return ApiResponse {
                success: false,
                data: None,
                error: Some(format!("TOTP error: {}", e)),
                timestamp,
            };
        }
    };

    let payload = json!({
        "clientcode": client_code,
        "password": password,
        "totp": totp_code
    });

    let url = format!("{}/rest/auth/angelbroking/user/v1/loginByPassword", ANGELONE_BASE_URL);

    match build_angel_request(&client, &url, "POST", &api_key, None)
        .json(&payload)
        .send()
        .await
    {
        Ok(response) => {
            let status = response.status();

            match response.json::<Value>().await {
                Ok(data) => {
                    if status.is_success() {
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
    let client = super::shared_client();
    let timestamp = chrono::Utc::now().timestamp_millis();

    let url = format!("{}/rest/secure/angelbroking/user/v1/getProfile", ANGELONE_BASE_URL);

    match build_angel_request(client, &url, "GET", &api_key, Some(&auth_token))
        .send()
        .await
    {
        Ok(response) => {
            let status = response.status();

            // Check HTTP status first
            if !status.is_success() {
                return ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("HTTP error: {}", status)),
                    timestamp,
                };
            }

            // Parse response body to check for API-level errors
            match response.text().await {
                Ok(body) => {
                    // Try to parse as JSON and check for error indicators
                    if let Ok(json) = serde_json::from_str::<Value>(&body) {
                        // Check for common error patterns in AngelOne API
                        let is_error = json.get("status").map(|s| s.as_bool() == Some(false)).unwrap_or(false)
                            || json.get("message").map(|m| {
                                let msg = m.as_str().unwrap_or("");
                                msg.contains("Invalid Token") ||
                                msg.contains("Session expired") ||
                                msg.contains("Unauthorized")
                            }).unwrap_or(false)
                            || json.get("errorcode").is_some();

                        if is_error {
                            let error_msg = json.get("message")
                                .and_then(|m| m.as_str())
                                .unwrap_or("Token validation failed");
                            return ApiResponse {
                                success: false,
                                data: Some(false),
                                error: Some(error_msg.to_string()),
                                timestamp,
                            };
                        }
                    }

                    // If we got here, token is valid
                    ApiResponse {
                        success: true,
                        data: Some(true),
                        error: None,
                        timestamp,
                    }
                }
                Err(e) => ApiResponse {
                    success: false,
                    data: Some(false),
                    error: Some(format!("Failed to read response: {}", e)),
                    timestamp,
                },
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

/// Refresh AngelOne session token
#[tauri::command]
pub async fn angelone_refresh_token(
    api_key: String,
    refresh_token: String,
) -> ApiResponse<Value> {
    let timestamp = chrono::Utc::now().timestamp_millis();

    let payload = json!({
        "refreshToken": refresh_token
    });

    match angel_api_call(
        &api_key,
        &refresh_token,
        "/rest/auth/angelbroking/jwt/v1/generateTokens",
        "POST",
        Some(payload),
    ).await {
        Ok(body) => {
            if let Some(data) = body.get("data") {
                let jwt = data.get("jwtToken")
                    .and_then(|v| v.as_str())
                    .unwrap_or("");
                let feed = data.get("feedToken")
                    .and_then(|v| v.as_str());
                let refresh = data.get("refreshToken")
                    .and_then(|v| v.as_str());

                if !jwt.is_empty() {
                    return ApiResponse {
                        success: true,
                        data: Some(json!({
                            "access_token": jwt,
                            "feed_token": feed,
                            "refresh_token": refresh,
                        })),
                        error: None,
                        timestamp,
                    };
                }
            }
            ApiResponse {
                success: false,
                data: None,
                error: Some("Token refresh failed".to_string()),
                timestamp,
            }
        }
        Err(e) => ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        },
    }
}
