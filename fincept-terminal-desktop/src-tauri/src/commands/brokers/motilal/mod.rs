#![allow(dead_code)]
//! Motilal Oswal Broker Commands
//!
//! REST API integration for Motilal Oswal broker

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;
pub mod bulk;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;
pub use bulk::*;

use reqwest::Client;
use serde::{Deserialize, Serialize};
use serde_json::Value;
use std::collections::HashMap;

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
    pub timestamp: u64,
}

pub(super) fn get_timestamp() -> u64 {
    std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .unwrap()
        .as_millis() as u64
}

// ============================================================================
// CONSTANTS
// ============================================================================

pub(super) const MOTILAL_BASE_URL: &str = "https://openapi.motilaloswal.com";

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

pub(super) fn get_motilal_headers(auth_token: &str, api_key: &str, vendor_code: &str) -> HashMap<String, String> {
    let mut headers = HashMap::new();
    headers.insert("Authorization".to_string(), auth_token.to_string());
    headers.insert("Content-Type".to_string(), "application/json".to_string());
    headers.insert("Accept".to_string(), "application/json".to_string());
    headers.insert("User-Agent".to_string(), "MOSL/V.1.1.0".to_string());
    headers.insert("ApiKey".to_string(), api_key.to_string());
    headers.insert("ClientLocalIp".to_string(), "127.0.0.1".to_string());
    headers.insert("ClientPublicIp".to_string(), "127.0.0.1".to_string());
    headers.insert("MacAddress".to_string(), "00:00:00:00:00:00".to_string());
    headers.insert("SourceId".to_string(), "WEB".to_string());
    headers.insert("vendorinfo".to_string(), vendor_code.to_string());
    headers.insert("osname".to_string(), "Windows".to_string());
    headers.insert("osversion".to_string(), "10.0".to_string());
    headers.insert("devicemodel".to_string(), "PC".to_string());
    headers.insert("manufacturer".to_string(), "Generic".to_string());
    headers.insert("productname".to_string(), "FinceptTerminal".to_string());
    headers.insert("productversion".to_string(), "1.0.0".to_string());
    headers.insert("browsername".to_string(), "Chrome".to_string());
    headers.insert("browserversion".to_string(), "120.0".to_string());
    headers
}

pub(super) async fn motilal_request(
    client: &Client,
    method: &str,
    endpoint: &str,
    headers: &HashMap<String, String>,
    body: Option<Value>,
) -> Result<Value, String> {
    let url = format!("{}{}", MOTILAL_BASE_URL, endpoint);

    let mut request = match method {
        "GET" => client.get(&url),
        "POST" => client.post(&url),
        "PUT" => client.put(&url),
        "DELETE" => client.delete(&url),
        _ => return Err("Invalid HTTP method".to_string()),
    };

    // Add headers
    for (key, value) in headers {
        request = request.header(key.as_str(), value.as_str());
    }

    // Add body if present
    if let Some(body_data) = body {
        request = request.json(&body_data);
    }

    let response = request.send().await.map_err(|e| e.to_string())?;

    if response.status().is_success() {
        response.json::<Value>().await.map_err(|e| e.to_string())
    } else {
        let status = response.status();
        let error_text = response.text().await.unwrap_or_default();
        Err(format!("HTTP {}: {}", status, error_text))
    }
}
