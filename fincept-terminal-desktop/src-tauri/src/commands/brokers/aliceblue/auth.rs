use super::{ALICEBLUE_API_BASE, create_aliceblue_headers, generate_checksum};
use super::super::common::{ApiResponse, TokenExchangeResponse};
use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use serde_json::{json, Value};

/// Get session ID using user ID, API secret, and encryption key
#[tauri::command]
pub async fn aliceblue_get_session(
    user_id: String,
    api_secret: String,
    enc_key: String,
) -> Result<TokenExchangeResponse, String> {
    eprintln!("[aliceblue_get_session] Getting session for user {}", user_id);

    let client = reqwest::Client::new();

    // Generate SHA256 checksum: userId + apiSecret + encKey
    let checksum = generate_checksum(&user_id, &api_secret, &enc_key);

    let payload = json!({
        "userId": user_id,
        "userData": checksum,
        "source": "WEB"
    });

    let mut headers = HeaderMap::new();
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));

    let response = client
        .post(format!("{}/customer/getUserSID", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;

    eprintln!("[aliceblue_get_session] Response: {:?}", body);

    if status.is_success() {
        // Check for sessionID in response
        if let Some(session_id) = body.get("sessionID").and_then(|s| s.as_str()) {
            return Ok(TokenExchangeResponse {
                success: true,
                access_token: Some(session_id.to_string()),
                user_id: Some(user_id),
                error: None,
            });
        }

        // Check for error message
        if let Some(error_msg) = body.get("emsg").and_then(|e| e.as_str()) {
            return Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some(error_msg.to_string()),
            });
        }

        // Check for stat field
        if body.get("stat").and_then(|s| s.as_str()) == Some("Not ok") {
            let error_msg = body.get("emsg")
                .and_then(|m| m.as_str())
                .unwrap_or("Authentication failed")
                .to_string();
            return Ok(TokenExchangeResponse {
                success: false,
                access_token: None,
                user_id: None,
                error: Some(error_msg),
            });
        }
    }

    Ok(TokenExchangeResponse {
        success: false,
        access_token: None,
        user_id: None,
        error: Some("Failed to get session ID".to_string()),
    })
}

/// Validate session by checking margins endpoint
#[tauri::command]
pub async fn aliceblue_validate_session(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[aliceblue_validate_session] Validating session");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/limits/getRmsLimits", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        let body: Value = response.json().await.unwrap_or(json!([]));

        // Check if response is an array with valid data
        let is_valid = if let Some(arr) = body.as_array() {
            !arr.is_empty() && arr.get(0).and_then(|v| v.get("stat")).and_then(|s| s.as_str()) != Some("Not_Ok")
        } else {
            false
        };

        Ok(ApiResponse {
            success: is_valid,
            data: Some(is_valid),
            error: if is_valid { None } else { Some("Session validation failed".to_string()) },
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Session validation failed".to_string()),
            timestamp,
        })
    }
}
