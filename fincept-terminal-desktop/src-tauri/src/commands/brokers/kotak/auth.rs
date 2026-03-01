// Kotak Authentication Commands

use super::{KOTAK_AUTH_BASE, ApiResponse, KotakAuthResponse, kotak_request, parse_auth_token};
use reqwest::Client;
use serde_json::json;
use tauri::command;

/// Step 1: Login with TOTP
#[command]
pub async fn kotak_totp_login(
    ucc: String,
    access_token: String,
    mobile_number: String,
    totp: String,
) -> ApiResponse<serde_json::Value> {
    let client = Client::new();

    let mobile = if mobile_number.starts_with("+91") {
        mobile_number
    } else if mobile_number.starts_with("91") && mobile_number.len() == 12 {
        format!("+{}", mobile_number)
    } else {
        format!("+91{}", mobile_number.trim_start_matches("91"))
    };

    let payload = json!({ "mobileNumber": mobile, "ucc": ucc, "totp": totp });
    let url = format!("{}/login/1.0/tradeApiLogin", KOTAK_AUTH_BASE);

    let response = client
        .post(&url)
        .header("Authorization", &access_token)
        .header("neo-fin-key", "neotradeapi")
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<serde_json::Value>(&text) {
                Ok(data) => {
                    if data.get("data").and_then(|d| d.get("status")).and_then(|s| s.as_str()) == Some("success") {
                        ApiResponse {
                            success: true,
                            data: Some(json!({ "view_token": data["data"]["token"], "view_sid": data["data"]["sid"] })),
                            error: None,
                        }
                    } else {
                        let err_msg = data.get("errMsg").or(data.get("message")).and_then(|v| v.as_str()).unwrap_or("TOTP login failed");
                        ApiResponse { success: false, data: None, error: Some(err_msg.to_string()) }
                    }
                }
                Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Failed to parse response: {}", e)) },
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Request failed: {}", e)) },
    }
}

/// Step 2: Validate with MPIN
#[command]
pub async fn kotak_mpin_validate(
    access_token: String,
    view_token: String,
    view_sid: String,
    mpin: String,
) -> ApiResponse<KotakAuthResponse> {
    let client = Client::new();

    let payload = json!({ "mpin": mpin });
    let url = format!("{}/login/1.0/tradeApiValidate", KOTAK_AUTH_BASE);

    let response = client
        .post(&url)
        .header("Authorization", &access_token)
        .header("neo-fin-key", "neotradeapi")
        .header("sid", &view_sid)
        .header("Auth", &view_token)
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<serde_json::Value>(&text) {
                Ok(data) => {
                    if data.get("data").and_then(|d| d.get("status")).and_then(|s| s.as_str()) == Some("success") {
                        let trading_token = data["data"]["token"].as_str().unwrap_or("").to_string();
                        let trading_sid = data["data"]["sid"].as_str().unwrap_or("").to_string();
                        let base_url = data["data"]["baseUrl"].as_str().unwrap_or("").to_string();
                        let auth_string = format!("{}:::{}:::{}:::{}", trading_token, trading_sid, base_url, access_token);

                        ApiResponse {
                            success: true,
                            data: Some(KotakAuthResponse { trading_token, trading_sid, base_url, access_token, auth_string }),
                            error: None,
                        }
                    } else {
                        let err_msg = data.get("errMsg").or(data.get("message")).and_then(|v| v.as_str()).unwrap_or("MPIN validation failed");
                        ApiResponse { success: false, data: None, error: Some(err_msg.to_string()) }
                    }
                }
                Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Failed to parse response: {}", e)) },
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Request failed: {}", e)) },
    }
}

/// Validate existing auth token
#[command]
pub async fn kotak_validate_token(auth_token: String) -> ApiResponse<bool> {
    let (trading_token, trading_sid, base_url, _access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: Some(false), error: Some(e) },
    };

    let client = Client::new();

    match kotak_request(&client, "GET", &base_url, "/quick/user/orders", &trading_token, &trading_sid, None).await {
        Ok(data) => {
            let is_valid = data.get("stat").and_then(|s| s.as_str()) == Some("Ok") || data.get("data").is_some();
            ApiResponse { success: true, data: Some(is_valid), error: None }
        }
        Err(_) => ApiResponse { success: true, data: Some(false), error: None },
    }
}
