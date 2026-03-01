//! Saxo Bank Broker Module
//!
//! Full implementation for Saxo Bank OpenAPI integration
//! Supports OAuth 2.0 authentication, trading, positions, and real-time streaming
//!
//! Documentation: https://www.developer.saxo/openapi/learn

pub mod auth;
pub mod account;
pub mod orders;
pub mod market_data;

pub use auth::*;
pub use account::*;
pub use orders::*;
pub use market_data::*;

use serde_json::Value;

// ============================================================================
// API CONFIGURATION
// ============================================================================

pub(super) const SAXO_SIM_API_BASE: &str = "https://gateway.saxobank.com/sim/openapi";
pub(super) const SAXO_LIVE_API_BASE: &str = "https://gateway.saxobank.com/openapi";

pub(super) fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper {
        SAXO_SIM_API_BASE
    } else {
        SAXO_LIVE_API_BASE
    }
}

// ============================================================================
// HTTP CLIENT HELPER
// ============================================================================

pub(super) async fn saxo_request(
    url: &str,
    method: &str,
    access_token: &str,
    body: Option<&str>,
) -> Result<Value, String> {
    let client = reqwest::Client::new();

    let mut request = match method {
        "GET" => client.get(url),
        "POST" => client.post(url),
        "PUT" => client.put(url),
        "PATCH" => client.patch(url),
        "DELETE" => client.delete(url),
        _ => return Err(format!("Unsupported HTTP method: {}", method)),
    };

    request = request
        .header("Authorization", format!("Bearer {}", access_token))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json");

    if let Some(body_str) = body {
        request = request.body(body_str.to_string());
    }

    let response = request.send().await.map_err(|e| e.to_string())?;
    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if status.is_success() {
        if text.is_empty() {
            Ok(serde_json::json!({"success": true}))
        } else {
            serde_json::from_str(&text).map_err(|e| e.to_string())
        }
    } else {
        Err(format!("Saxo API error ({}): {}", status, text))
    }
}

// ============================================================================
// GENERIC API REQUEST
// ============================================================================

/// Generic Saxo API request
#[tauri::command]
pub async fn saxo_api_request(
    url: String,
    method: String,
    access_token: String,
    body: Option<String>,
) -> Result<Value, String> {
    saxo_request(&url, &method, &access_token, body.as_deref()).await
}
