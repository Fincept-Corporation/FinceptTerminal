// IBKR Session Management + Credential Storage

use serde_json::Value;
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_ibkr_headers, create_client};

/// Check authentication status
#[tauri::command]
pub async fn ibkr_get_auth_status(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_auth_status] Checking authentication status");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/auth/status", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Not authenticated".to_string()), timestamp })
    }
}

/// Ping/tickle the server to keep session alive
#[tauri::command]
pub async fn ibkr_tickle(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_tickle] Sending tickle to keep session alive");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/tickle", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Tickle failed".to_string()), timestamp })
    }
}

/// Logout from IBKR session
#[tauri::command]
pub async fn ibkr_logout(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_logout] Logging out");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/logout", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Logout failed".to_string()), timestamp })
    }
}

/// Validate SSO token (Gateway mode)
#[tauri::command]
pub async fn ibkr_validate_sso(
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_validate_sso] Validating SSO token");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(None);
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/sso/validate", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("SSO validation failed".to_string()), timestamp })
    }
}

/// Reauthenticate session
#[tauri::command]
pub async fn ibkr_reauthenticate(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_reauthenticate] Reauthenticating session");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/reauthenticate", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Reauthentication failed".to_string()), timestamp })
    }
}

// ============================================================================
// Credential Storage
// ============================================================================

/// Store IBKR credentials (validates by checking auth status)
#[tauri::command]
pub async fn store_ibkr_credentials(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[store_ibkr_credentials] Validating IBKR credentials");

    let validate = ibkr_get_auth_status(access_token, use_gateway).await?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: validate.success,
        data: Some(validate.success),
        error: validate.error,
        timestamp,
    })
}
