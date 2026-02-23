// Common types and utilities for Indian broker integrations

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

