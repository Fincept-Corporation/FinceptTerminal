// Motilal Oswal Authentication Commands

use reqwest::Client;
use serde_json::{json, Value};
use sha2::{Sha256, Digest};
use tauri::command;
use super::{ApiResponse, get_timestamp, get_motilal_headers, motilal_request};

/// Authenticate with Motilal Oswal and get auth token
#[command]
pub async fn motilal_authenticate(
    user_id: String,
    password: String,
    totp: Option<String>,
    dob: String,
    api_key: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();

    // SHA-256(password + apikey) as per Motilal Oswal API documentation
    let mut hasher = Sha256::new();
    hasher.update(format!("{}{}", password, api_key));
    let password_hash = format!("{:x}", hasher.finalize());

    let mut payload = json!({
        "userid": user_id,
        "password": password_hash,
        "2FA": dob
    });

    // Add TOTP if provided
    if let Some(totp_code) = totp {
        if !totp_code.is_empty() {
            payload["totp"] = json!(totp_code);
        }
    }

    let headers = get_motilal_headers("", &api_key, &user_id);

    match motilal_request(&client, "POST", "/rest/login/v3/authdirectapi", &headers, Some(payload)).await {
        Ok(data) => {
            if data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS") {
                if let Some(auth_token) = data.get("AuthToken") {
                    Ok(ApiResponse {
                        success: true,
                        data: Some(json!({
                            "auth_token": auth_token,
                            "user_id": user_id,
                            "raw": data
                        })),
                        error: None,
                        timestamp,
                    })
                } else {
                    Ok(ApiResponse {
                        success: false,
                        data: None,
                        error: Some("Auth token not found in response".to_string()),
                        timestamp,
                    })
                }
            } else {
                let error_msg = data.get("message")
                    .and_then(|m| m.as_str())
                    .unwrap_or("Authentication failed")
                    .to_string();
                Ok(ApiResponse {
                    success: false,
                    data: Some(data),
                    error: Some(error_msg),
                    timestamp,
                })
            }
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}

/// Validate auth token
#[command]
pub async fn motilal_validate_token(
    auth_token: String,
    api_key: String,
    vendor_code: String,
) -> Result<ApiResponse, String> {
    let timestamp = get_timestamp();
    let client = Client::new();
    let headers = get_motilal_headers(&auth_token, &api_key, &vendor_code);

    // Try to fetch order book to validate token
    match motilal_request(&client, "POST", "/rest/book/v2/getorderbook", &headers, Some(json!({}))).await {
        Ok(data) => {
            let is_valid = data.get("status").and_then(|s| s.as_str()) == Some("SUCCESS");
            Ok(ApiResponse {
                success: is_valid,
                data: Some(json!({ "valid": is_valid })),
                error: if is_valid { None } else { Some("Token validation failed".to_string()) },
                timestamp,
            })
        }
        Err(e) => Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(e),
            timestamp,
        }),
    }
}
