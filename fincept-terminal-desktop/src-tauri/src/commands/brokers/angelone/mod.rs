//! AngelOne (Angel Broking) API Commands
//!
//! Authentication, order management, market data, and portfolio for AngelOne
//! API Reference: https://smartapi.angelone.in/docs

#![allow(dead_code)]

pub mod auth;
pub mod symbols;
pub mod market_data;
pub mod orders;
pub mod portfolio;

pub use auth::*;
pub use symbols::*;
pub use market_data::*;
pub use orders::*;
pub use portfolio::*;

use reqwest::Client;
use serde_json::Value;
use std::time::Duration;
use std::sync::OnceLock;
use totp_rs::{Algorithm, TOTP, Secret};

/// Default timeout for API requests (15 seconds)
pub(super) const API_TIMEOUT: Duration = Duration::from_secs(15);

/// Base URL for AngelOne API
pub(super) const ANGELONE_BASE_URL: &str = "https://apiconnect.angelone.in";

/// Shared HTTP client — reuses connections and TLS sessions across calls
pub(super) fn shared_client() -> &'static Client {
    static CLIENT: OnceLock<Client> = OnceLock::new();
    CLIENT.get_or_init(|| {
        Client::builder()
            .timeout(API_TIMEOUT)
            .pool_max_idle_per_host(4)
            .build()
            .expect("Failed to create HTTP client")
    })
}

/// Build a reqwest Client with standard AngelOne headers
pub(super) fn build_angel_request(
    client: &Client,
    url: &str,
    method: &str,
    api_key: &str,
    access_token: Option<&str>,
) -> reqwest::RequestBuilder {
    let builder = match method {
        "POST" => client.post(url),
        "PUT" => client.put(url),
        "DELETE" => client.delete(url),
        _ => client.get(url),
    };

    let mut builder = builder
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("X-UserType", "USER")
        .header("X-SourceID", "WEB")
        .header("X-ClientLocalIP", "127.0.0.1")
        .header("X-ClientPublicIP", "127.0.0.1")
        .header("X-MACAddress", "00:00:00:00:00:00")
        .header("X-PrivateKey", api_key);

    if let Some(token) = access_token {
        builder = builder.header("Authorization", format!("Bearer {}", token));
    }

    builder
}

/// Make an authenticated API call and return parsed JSON
pub(super) async fn angel_api_call(
    api_key: &str,
    access_token: &str,
    endpoint: &str,
    method: &str,
    payload: Option<Value>,
) -> Result<Value, String> {
    let client = shared_client();

    let url = format!("{}{}", ANGELONE_BASE_URL, endpoint);

    let mut builder = build_angel_request(client, &url, method, api_key, Some(access_token));

    if let Some(body) = payload {
        builder = builder.json(&body);
    }

    let response = builder
        .send()
        .await
        .map_err(|e| {
            if e.is_timeout() {
                format!("Request timeout after {}s", API_TIMEOUT.as_secs())
            } else {
                format!("Request failed: {}", e)
            }
        })?;

    let status = response.status();
    let text = response
        .text()
        .await
        .map_err(|e| format!("Failed to read response body: {}", e))?;

    if text.is_empty() {
        return Err(format!("Empty response from AngelOne API (HTTP {})", status.as_u16()));
    }

    let body: Value = serde_json::from_str(&text)
        .map_err(|e| format!("Failed to parse JSON (HTTP {}): {} — body: {}", status.as_u16(), e, &text[..text.len().min(200)]))?;

    if !status.is_success() {
        let msg = body.get("message")
            .and_then(|v| v.as_str())
            .unwrap_or("API request failed");
        return Err(format!("{} (HTTP {})", msg, status.as_u16()));
    }

    Ok(body)
}

/// Generate a 6-digit TOTP code from a base32-encoded secret string.
/// If the input is already a 6-digit code, returns it as-is (backward compat).
pub(super) fn generate_totp_code(totp_input: &str) -> Result<String, String> {
    let trimmed = totp_input.trim();

    // If it's already a 6-digit numeric code, pass through directly
    if trimmed.len() == 6 && trimmed.chars().all(|c| c.is_ascii_digit()) {
        return Ok(trimmed.to_string());
    }

    // Otherwise, treat as a base32 TOTP secret and generate the code
    let secret_bytes = Secret::Encoded(trimmed.to_string())
        .to_bytes()
        .map_err(|e| format!("Invalid TOTP secret: {}", e))?;

    let totp = TOTP::new(
        Algorithm::SHA1,
        6,   // digits
        1,   // skew
        30,  // step (seconds)
        secret_bytes,
    ).map_err(|e| format!("TOTP init failed: {}", e))?;

    totp.generate_current()
        .map_err(|e| format!("TOTP generation failed: {}", e))
}
