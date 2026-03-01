#![allow(dead_code)]
//! 5Paisa Broker Commands
//!
//! REST API integration for 5Paisa Trading API
//! Based on OpenAlgo Python implementation

use reqwest::Client;
use serde::{Deserialize, Serialize};

mod auth;
mod orders;
mod portfolio;
mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

// ============================================================================
// CONSTANTS
// ============================================================================

pub(super) const FIVEPAISA_BASE_URL: &str = "https://Openapi.5paisa.com";

// ============================================================================
// TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaResponse {
    pub success: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub data: Option<serde_json::Value>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaApiResponse {
    pub head: FivePaisaHead,
    pub body: serde_json::Value,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FivePaisaHead {
    #[serde(rename = "statusDescription")]
    pub status_description: Option<String>,
    pub status: Option<String>,
    #[serde(rename = "Key")]
    pub key: Option<String>,
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

pub(super) fn create_client() -> Result<Client, String> {
    Client::builder()
        .timeout(std::time::Duration::from_secs(30))
        .build()
        .map_err(|e| format!("Failed to create HTTP client: {}", e))
}

/// Map exchange to 5Paisa format
pub(super) fn map_exchange(exchange: &str) -> &str {
    match exchange {
        "NSE" | "NSE_INDEX" => "N",
        "BSE" | "BSE_INDEX" => "B",
        "NFO" => "N",
        "BFO" => "B",
        "CDS" => "N",
        "BCD" => "B",
        "MCX" => "M",
        _ => "N",
    }
}

/// Map exchange type to 5Paisa format
pub(super) fn map_exchange_type(exchange: &str) -> &str {
    match exchange {
        "NSE" | "BSE" | "NSE_INDEX" | "BSE_INDEX" => "C",  // Cash
        "NFO" | "BFO" | "MCX" => "D",  // Derivative
        "CDS" | "BCD" => "U",  // Currency
        _ => "C",
    }
}

/// Map order side to 5Paisa format
pub(super) fn map_order_side(side: &str) -> &str {
    match side.to_uppercase().as_str() {
        "BUY" => "B",
        "SELL" => "S",
        _ => "B",
    }
}

/// Map product type to 5Paisa format (IsIntraday boolean)
pub(super) fn is_intraday(product: &str) -> bool {
    match product.to_uppercase().as_str() {
        "MIS" | "INTRADAY" => true,
        "CNC" | "NRML" | "CASH" | "MARGIN" => false,
        _ => false,
    }
}
