// Shoonya Authentication Commands

use super::{SHOONYA_API_BASE, create_shoonya_headers, sha256_hash, create_payload};
use super::super::common::TokenExchangeResponse;
use serde_json::json;

/// Authenticate with Shoonya using TOTP
/// Shoonya uses QuickAuth with SHA256 hashed password and appkey
#[tauri::command]
pub async fn shoonya_authenticate(
    user_id: String,
    password: String,
    totp_code: String,
    api_key: String,      // vendor_code
    api_secret: String,   // api_secretkey
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[shoonya_authenticate] Authenticating user: {}", user_id);

    let client = reqwest::Client::new();
    let headers = create_shoonya_headers();

    // Hash password with SHA256
    let pwd_hash = sha256_hash(&password);

    // Hash appkey: SHA256(userid|api_secret)
    let appkey_input = format!("{}|{}", user_id, api_secret);
    let appkey_hash = sha256_hash(&appkey_input);

    let payload = json!({
        "uid": user_id,
        "pwd": pwd_hash,
        "factor2": totp_code,
        "apkversion": "1.0.0",
        "appkey": appkey_hash,
        "imei": "abc1234",
        "vc": api_key,
        "source": "API"
    });

    let payload_str = create_payload(&payload);

    let response = client
        .post(format!("{}/NorenWClientTP/QuickAuth", SHOONYA_API_BASE))
        .headers(headers)
        .body(payload_str)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: serde_json::Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[shoonya_authenticate] Response: {:?}", body);

    if status.is_success() && body.get("stat").and_then(|s| s.as_str()) == Some("Ok") {
        Ok(TokenExchangeResponse {
            success: true,
            access_token: body.get("susertoken").and_then(|v| v.as_str()).map(String::from),
            user_id: Some(user_id),
            error: None,
        })
    } else {
        let error_msg = body.get("emsg")
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
