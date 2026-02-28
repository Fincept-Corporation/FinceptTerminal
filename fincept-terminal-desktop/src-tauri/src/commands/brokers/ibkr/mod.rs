//! Interactive Brokers (IBKR) Broker Integration
//!
//! Split into domain modules:
//! - auth.rs        — Session management + credential storage
//! - account.rs     — Account, positions, trades, analytics
//! - orders.rs      — Orders, contracts, alerts
//! - market_data.rs — Market data snapshots, historical data, scanner

pub mod auth;
pub mod account;
pub mod orders;
pub mod market_data;

pub use auth::*;
pub use account::*;
pub use orders::*;
pub use market_data::*;

// ============================================================================
// Shared IBKR API helpers (used by all submodules via super::)
// ============================================================================

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};

pub(super) const IBKR_API_BASE: &str = "https://api.ibkr.com/v1/api";
pub(super) const IBKR_GATEWAY_BASE: &str = "https://localhost:5000/v1/api";

pub(super) fn get_api_base(use_gateway: bool) -> &'static str {
    if use_gateway { IBKR_GATEWAY_BASE } else { IBKR_API_BASE }
}

pub(super) fn create_ibkr_headers(access_token: Option<&str>) -> HeaderMap {
    let mut headers = HeaderMap::new();

    if let Some(token) = access_token {
        if let Ok(auth_value) = HeaderValue::from_str(&format!("Bearer {}", token)) {
            headers.insert("Authorization", auth_value);
        }
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

pub(super) fn create_client(use_gateway: bool) -> reqwest::Client {
    if use_gateway {
        reqwest::Client::builder()
            .danger_accept_invalid_certs(true)
            .build()
            .unwrap_or_else(|_| reqwest::Client::new())
    } else {
        reqwest::Client::new()
    }
}
