//! Fyers Broker Integration

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;
pub mod websocket;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;
pub use websocket::*;

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};

pub(super) const FYERS_API_BASE: &str = "https://api-t1.fyers.in";

pub(super) fn create_fyers_headers(api_key: &str, access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    let auth_value = format!("{}:{}", api_key, access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

pub(super) fn validate_credentials(api_key: &str, access_token: &str) -> Result<(), String> {
    if api_key.is_empty() {
        return Err("API key is empty. Please authenticate first.".to_string());
    }
    if access_token.is_empty() {
        return Err("Access token is empty. Please authenticate first.".to_string());
    }
    if access_token.len() < 40 {
        return Err("Access token appears invalid or expired. Please re-authenticate.".to_string());
    }
    Ok(())
}
