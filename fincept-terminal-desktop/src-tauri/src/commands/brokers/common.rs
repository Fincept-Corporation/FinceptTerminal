#![allow(dead_code)]
// Common types and utilities for Indian broker integrations

use reqwest::header::{HeaderMap, HeaderValue};
use serde::{Deserialize, Serialize};

// ============================================================================
// Common Response Types
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct ApiResponse<T> {
    pub success: bool,
    pub data: Option<T>,
    pub error: Option<String>,
    pub timestamp: i64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TokenExchangeResponse {
    pub success: bool,
    pub access_token: Option<String>,
    pub user_id: Option<String>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct OrderPlaceResponse {
    pub success: bool,
    pub order_id: Option<String>,
    pub error: Option<String>,
}

// ============================================================================
// Helper Functions
// ============================================================================

/// Create common HTTP headers for API requests
pub fn create_common_headers() -> HeaderMap {
    let mut headers = HeaderMap::new();
    headers.insert(
        "Content-Type",
        HeaderValue::from_static("application/json"),
    );
    headers
}
