//! Alice Blue Broker Integration
//!
//! Complete API implementation for Alice Blue broker including:
//! - Authentication (SHA256 checksum-based session)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, holdings, margins)
//! - Market Data (quotes, historical)

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};
use sha2::{Sha256, Digest};

// Alice Blue API Configuration
// ============================================================================

pub(super) const ALICEBLUE_API_BASE: &str = "https://ant.aliceblueonline.com/rest/AliceBlueAPIService/api";

pub(super) fn create_aliceblue_headers(api_secret: &str, session_id: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    let auth_value = format!("Bearer {} {}", api_secret, session_id);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

/// Generate SHA256 checksum for authentication
pub(super) fn generate_checksum(user_id: &str, api_secret: &str, enc_key: &str) -> String {
    let input = format!("{}{}{}", user_id, api_secret, enc_key);
    let mut hasher = Sha256::new();
    hasher.update(input.as_bytes());
    let result = hasher.finalize();
    hex::encode(result)
}
