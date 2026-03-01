#![allow(dead_code)]
//! IIFL Broker Integration (XTS API)
//!
//! API Documentation: https://symphonyfintech.com/xts-trading-front-end-api/

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};

// IIFL API Configuration
pub(super) const IIFL_BASE_URL: &str = "https://ttblaze.iifl.com";
pub(super) const IIFL_INTERACTIVE_URL: &str = "https://ttblaze.iifl.com/interactive";
pub(super) const IIFL_MARKET_DATA_URL: &str = "https://ttblaze.iifl.com/apimarketdata";

pub(super) fn create_iifl_headers(token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();
    if let Ok(auth_value) = HeaderValue::from_str(token) {
        headers.insert(AUTHORIZATION, auth_value);
    }
    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

/// Exchange segment mapping for API calls
pub(super) fn get_exchange_segment(exchange: &str) -> &'static str {
    match exchange {
        "NSE" => "NSECM",
        "BSE" => "BSECM",
        "NFO" => "NSEFO",
        "BFO" => "BSEFO",
        "MCX" => "MCXFO",
        "CDS" => "NSECD",
        _ => "NSECM",
    }
}

/// Exchange segment ID for market data API
pub(super) fn get_exchange_segment_id(exchange: &str) -> i32 {
    match exchange {
        "NSE" | "NSE_INDEX" => 1,
        "NFO" => 2,
        "CDS" => 3,
        "BSE" | "BSE_INDEX" => 11,
        "BFO" => 12,
        "MCX" => 51,
        _ => 1,
    }
}
