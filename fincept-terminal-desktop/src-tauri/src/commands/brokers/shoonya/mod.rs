#![allow(dead_code)]
//! Shoonya (Finvasia) Broker Integration
//!
//! Supports TOTP-based authentication, order management,
//! portfolio, and market data with jData/jKey payload format.

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use sha2::{Sha256, Digest};

// Shoonya API Configuration
pub(super) const SHOONYA_API_BASE: &str = "https://api.shoonya.com";

/// Create Shoonya-specific headers (form-urlencoded)
pub(super) fn create_shoonya_headers() -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}

/// Generate SHA256 hash
pub(super) fn sha256_hash(text: &str) -> String {
    let mut hasher = Sha256::new();
    hasher.update(text.as_bytes());
    format!("{:x}", hasher.finalize())
}

/// Create Shoonya payload string with jData and jKey
pub(super) fn create_payload_with_auth(data: &serde_json::Value, auth_token: &str) -> String {
    format!("jData={}&jKey={}", data.to_string(), auth_token)
}

/// Create Shoonya payload string (jData only, for auth)
pub(super) fn create_payload(data: &serde_json::Value) -> String {
    format!("jData={}", data.to_string())
}
