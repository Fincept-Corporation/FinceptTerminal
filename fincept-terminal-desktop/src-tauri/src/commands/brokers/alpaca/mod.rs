#![allow(dead_code)]
//! Alpaca (US) Broker Integration
//!
//! Supports both live and paper trading modes.
//!
//! Split into domain modules:
//! - account.rs  — Account info, configurations, activities, watchlists
//! - orders.rs   — Order placement/management and positions
//! - market_data.rs — Market data (bars, quotes, trades, snapshots, assets)
//! - websocket.rs — WebSocket stubs and credential storage

pub mod account;
pub mod orders;
pub mod market_data;
pub mod websocket;

pub use account::*;
pub use orders::*;
pub use market_data::*;
pub use websocket::*;

// ============================================================================
// Shared Alpaca API helpers (used by all submodules via super::)
// ============================================================================

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};

pub(super) const ALPACA_LIVE_API_BASE: &str = "https://api.alpaca.markets";
pub(super) const ALPACA_PAPER_API_BASE: &str = "https://paper-api.alpaca.markets";
pub(super) const ALPACA_DATA_API_BASE: &str = "https://data.alpaca.markets";

pub(super) fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper {
        ALPACA_PAPER_API_BASE
    } else {
        ALPACA_LIVE_API_BASE
    }
}

pub(super) fn create_alpaca_headers(api_key: &str, api_secret: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    if let Ok(key) = HeaderValue::from_str(api_key) {
        headers.insert("APCA-API-KEY-ID", key);
    }

    if let Ok(secret) = HeaderValue::from_str(api_secret) {
        headers.insert("APCA-API-SECRET-KEY", secret);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}
