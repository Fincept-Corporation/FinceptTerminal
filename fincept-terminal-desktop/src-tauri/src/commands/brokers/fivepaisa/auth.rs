use super::{FIVEPAISA_BASE_URL, FivePaisaResponse, FivePaisaApiResponse, create_client};
use serde_json::json;

/// Step 1: TOTP Login - Get request token
#[tauri::command]
pub async fn fivepaisa_totp_login(
    api_key: String,
    email: String,
    pin: String,
    totp: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "Key": api_key },
        "body": {
            "Email_ID": email,
            "TOTP": totp,
            "PIN": pin
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/TOTPLogin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("TOTP login request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse TOTP response: {}", e))?;

    if let Some(request_token) = data.body.get("RequestToken").and_then(|v| v.as_str()) {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "request_token": request_token })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Failed to obtain request token");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Step 2: Exchange request token for access token
#[tauri::command]
pub async fn fivepaisa_get_access_token(
    api_key: String,
    api_secret: String,
    user_id: String,
    request_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "Key": api_key },
        "body": {
            "RequestToken": request_token,
            "EncryKey": api_secret,
            "UserId": user_id
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/GetAccessToken", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Access token request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse access token response: {}", e))?;

    if let Some(access_token) = data.body.get("AccessToken").and_then(|v| v.as_str()) {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({
                "access_token": access_token,
                "user_id": user_id
            })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Failed to obtain access token");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Validate access token by fetching margin
#[tauri::command]
pub async fn fivepaisa_validate_token(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V4/Margin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Token validation failed: {}", e))?;

    let status = response.status();
    if status.is_success() {
        let data: FivePaisaApiResponse = response
            .json()
            .await
            .map_err(|e| format!("Failed to parse validation response: {}", e))?;

        let is_valid = data.head.status_description.as_deref() == Some("Success") ||
                       data.head.status.as_deref() == Some("0");

        Ok(FivePaisaResponse {
            success: is_valid,
            data: None,
            error: if is_valid { None } else { Some("Token validation failed".to_string()) },
        })
    } else {
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(format!("HTTP error: {}", status)),
        })
    }
}
