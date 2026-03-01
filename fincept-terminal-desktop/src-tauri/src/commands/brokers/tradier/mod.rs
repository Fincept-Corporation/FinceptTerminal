#![allow(dead_code)]
//! Tradier (US) Broker Integration
//!
//! Supports both live (production) and paper (sandbox) trading modes.

pub mod account;
pub mod orders;
pub mod market_data;
pub mod websocket;

pub use account::*;
pub use orders::*;
pub use market_data::*;
pub use websocket::*;

use reqwest::header::{HeaderMap, HeaderValue, ACCEPT, AUTHORIZATION, CONTENT_TYPE};
use serde_json::Value;

// Tradier API Configuration
pub(super) const TRADIER_LIVE_API_BASE: &str = "https://api.tradier.com/v1";
pub(super) const TRADIER_SANDBOX_API_BASE: &str = "https://sandbox.tradier.com/v1";
pub(super) const TRADIER_STREAM_URL: &str = "https://stream.tradier.com/v1";

pub(super) fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper { TRADIER_SANDBOX_API_BASE } else { TRADIER_LIVE_API_BASE }
}

pub(super) fn create_tradier_headers(token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    if let Ok(auth) = HeaderValue::from_str(&format!("Bearer {}", token)) {
        headers.insert(AUTHORIZATION, auth);
    }
    headers.insert(ACCEPT, HeaderValue::from_static("application/json"));
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}

pub(super) fn extract_tradier_error(body: &Value) -> String {
    if let Some(fault) = body.get("fault") {
        if let Some(faultstring) = fault.get("faultstring").and_then(|f| f.as_str()) {
            return faultstring.to_string();
        }
    }
    if let Some(errors) = body.get("errors") {
        if let Some(error) = errors.get("error") {
            if let Some(s) = error.as_str() { return s.to_string(); }
            if let Some(arr) = error.as_array() {
                let msgs: Vec<String> = arr.iter().filter_map(|v| v.as_str().map(String::from)).collect();
                if !msgs.is_empty() { return msgs.join(", "); }
            }
        }
    }
    "Unknown error".to_string()
}
