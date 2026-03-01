//! Zerodha (Kite) Broker Integration

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

pub(super) const ZERODHA_API_BASE: &str = "https://api.kite.trade";
pub(super) const ZERODHA_API_VERSION: &str = "3";

pub(super) fn create_zerodha_headers(api_key: &str, access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert("X-Kite-Version", HeaderValue::from_static(ZERODHA_API_VERSION));

    let auth_value = format!("token {}:{}", api_key, access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/x-www-form-urlencoded"));
    headers
}
