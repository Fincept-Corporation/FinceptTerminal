//! Unified Symbol Master Commands
//!
//! Tauri commands for managing the unified symbol database across all brokers.
//! Provides search, status tracking, and download functionality.

use crate::database::symbol_master::{
    self, DownloadStatus, MasterContractStatus, SymbolSearchResult, CACHE_TTL_SECONDS,
};
use serde::{Deserialize, Serialize};
use tauri::command;

// ============================================================================
// RESPONSE TYPES
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct SymbolSearchResponse {
    pub success: bool,
    pub symbols: Vec<SymbolSearchResult>,
    pub total: usize,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StatusResponse {
    pub success: bool,
    pub status: Option<MasterContractStatus>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct CacheValidityResponse {
    pub success: bool,
    pub is_valid: bool,
    pub age_seconds: Option<i64>,
    pub ttl_seconds: i64,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExchangesResponse {
    pub success: bool,
    pub exchanges: Vec<String>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ExpiriesResponse {
    pub success: bool,
    pub expiries: Vec<String>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct UnderlyingsResponse {
    pub success: bool,
    pub underlyings: Vec<String>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SymbolCountResponse {
    pub success: bool,
    pub count: i64,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TokenLookupResponse {
    pub success: bool,
    pub token: Option<String>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SymbolLookupResponse {
    pub success: bool,
    pub symbol: Option<SymbolSearchResult>,
    pub error: Option<String>,
}

// ============================================================================
// SEARCH COMMANDS
// ============================================================================

/// Search symbols for a broker
#[command]
pub fn symbol_master_search(
    broker_id: String,
    query: String,
    exchange: Option<String>,
    instrument_type: Option<String>,
    limit: Option<i32>,
) -> SymbolSearchResponse {
    let limit = limit.unwrap_or(50);

    match symbol_master::search_symbols(
        &broker_id,
        &query,
        exchange.as_deref(),
        instrument_type.as_deref(),
        limit,
    ) {
        Ok(symbols) => {
            let total = symbols.len();
            SymbolSearchResponse {
                success: true,
                symbols,
                total,
                error: None,
            }
        }
        Err(e) => SymbolSearchResponse {
            success: false,
            symbols: vec![],
            total: 0,
            error: Some(format!("Search failed: {}", e)),
        },
    }
}

/// Get symbol by exact match
#[command]
pub fn symbol_master_get_symbol(
    broker_id: String,
    symbol: String,
    exchange: String,
) -> SymbolLookupResponse {
    match symbol_master::get_symbol(&broker_id, &symbol, &exchange) {
        Ok(symbol) => SymbolLookupResponse {
            success: true,
            symbol,
            error: None,
        },
        Err(e) => SymbolLookupResponse {
            success: false,
            symbol: None,
            error: Some(format!("Lookup failed: {}", e)),
        },
    }
}

/// Get symbol by token
#[command]
pub fn symbol_master_get_by_token(broker_id: String, token: String) -> SymbolLookupResponse {
    match symbol_master::get_symbol_by_token(&broker_id, &token) {
        Ok(symbol) => SymbolLookupResponse {
            success: true,
            symbol,
            error: None,
        },
        Err(e) => SymbolLookupResponse {
            success: false,
            symbol: None,
            error: Some(format!("Lookup failed: {}", e)),
        },
    }
}

/// Get token for a symbol (for WebSocket subscriptions)
#[command]
pub fn symbol_master_get_token(
    broker_id: String,
    symbol: String,
    exchange: String,
) -> TokenLookupResponse {
    match symbol_master::get_token_by_symbol(&broker_id, &symbol, &exchange) {
        Ok(token) => TokenLookupResponse {
            success: true,
            token,
            error: None,
        },
        Err(e) => TokenLookupResponse {
            success: false,
            token: None,
            error: Some(format!("Lookup failed: {}", e)),
        },
    }
}

// ============================================================================
// STATUS COMMANDS
// ============================================================================

/// Get master contract status for a broker
#[command]
pub fn symbol_master_get_status(broker_id: String) -> StatusResponse {
    match symbol_master::get_status(&broker_id) {
        Ok(status) => StatusResponse {
            success: true,
            status: Some(status),
            error: None,
        },
        Err(e) => StatusResponse {
            success: false,
            status: None,
            error: Some(format!("Failed to get status: {}", e)),
        },
    }
}

/// Check if master contracts are ready (cached and valid)
#[command]
pub fn symbol_master_is_ready(broker_id: String) -> CacheValidityResponse {
    let is_valid = symbol_master::is_cache_valid(&broker_id, CACHE_TTL_SECONDS).unwrap_or(false);
    let age_seconds = symbol_master::get_cache_age(&broker_id).unwrap_or(None);

    CacheValidityResponse {
        success: true,
        is_valid,
        age_seconds,
        ttl_seconds: CACHE_TTL_SECONDS,
        error: None,
    }
}

/// Initialize broker status (call on login)
#[command]
pub fn symbol_master_init_broker(broker_id: String) -> StatusResponse {
    // First check if we have a valid cache
    let is_valid = symbol_master::is_cache_valid(&broker_id, CACHE_TTL_SECONDS).unwrap_or(false);

    if is_valid {
        // Cache is still valid, don't reinitialize
        match symbol_master::get_status(&broker_id) {
            Ok(status) => {
                return StatusResponse {
                    success: true,
                    status: Some(status),
                    error: None,
                }
            }
            Err(_) => {}
        }
    }

    // Initialize or reinitialize
    match symbol_master::init_broker_status(&broker_id) {
        Ok(_) => match symbol_master::get_status(&broker_id) {
            Ok(status) => StatusResponse {
                success: true,
                status: Some(status),
                error: None,
            },
            Err(e) => StatusResponse {
                success: false,
                status: None,
                error: Some(format!("Failed to get status after init: {}", e)),
            },
        },
        Err(e) => StatusResponse {
            success: false,
            status: None,
            error: Some(format!("Failed to initialize broker: {}", e)),
        },
    }
}

/// Update master contract status (used by download commands)
#[command]
pub fn symbol_master_update_status(
    broker_id: String,
    status: String,
    message: String,
    total_symbols: Option<i64>,
) -> StatusResponse {
    let status_enum = DownloadStatus::from(status.as_str());

    match symbol_master::update_status(&broker_id, status_enum, &message, total_symbols) {
        Ok(_) => match symbol_master::get_status(&broker_id) {
            Ok(status) => StatusResponse {
                success: true,
                status: Some(status),
                error: None,
            },
            Err(e) => StatusResponse {
                success: false,
                status: None,
                error: Some(format!("Failed to get updated status: {}", e)),
            },
        },
        Err(e) => StatusResponse {
            success: false,
            status: None,
            error: Some(format!("Failed to update status: {}", e)),
        },
    }
}

// ============================================================================
// METADATA COMMANDS
// ============================================================================

/// Get symbol count for a broker
#[command]
pub fn symbol_master_get_count(broker_id: String) -> SymbolCountResponse {
    match symbol_master::get_symbol_count(&broker_id) {
        Ok(count) => SymbolCountResponse {
            success: true,
            count,
            error: None,
        },
        Err(e) => SymbolCountResponse {
            success: false,
            count: 0,
            error: Some(format!("Failed to get count: {}", e)),
        },
    }
}

/// Get distinct exchanges for a broker
#[command]
pub fn symbol_master_get_exchanges(broker_id: String) -> ExchangesResponse {
    match symbol_master::get_exchanges(&broker_id) {
        Ok(exchanges) => ExchangesResponse {
            success: true,
            exchanges,
            error: None,
        },
        Err(e) => ExchangesResponse {
            success: false,
            exchanges: vec![],
            error: Some(format!("Failed to get exchanges: {}", e)),
        },
    }
}

/// Get distinct expiries for FNO symbols
#[command]
pub fn symbol_master_get_expiries(
    broker_id: String,
    exchange: Option<String>,
    underlying: Option<String>,
) -> ExpiriesResponse {
    match symbol_master::get_expiries(&broker_id, exchange.as_deref(), underlying.as_deref()) {
        Ok(expiries) => ExpiriesResponse {
            success: true,
            expiries,
            error: None,
        },
        Err(e) => ExpiriesResponse {
            success: false,
            expiries: vec![],
            error: Some(format!("Failed to get expiries: {}", e)),
        },
    }
}

/// Get distinct underlyings (for option chain)
#[command]
pub fn symbol_master_get_underlyings(
    broker_id: String,
    exchange: Option<String>,
) -> UnderlyingsResponse {
    match symbol_master::get_underlyings(&broker_id, exchange.as_deref()) {
        Ok(underlyings) => UnderlyingsResponse {
            success: true,
            underlyings,
            error: None,
        },
        Err(e) => UnderlyingsResponse {
            success: false,
            underlyings: vec![],
            error: Some(format!("Failed to get underlyings: {}", e)),
        },
    }
}

// ============================================================================
// UTILITY COMMANDS
// ============================================================================

/// Normalize index symbol to unified format
#[command]
pub fn symbol_master_normalize_index(symbol: String) -> String {
    symbol_master::normalize_index_symbol(&symbol)
}

/// Format expiry date to DD-MMM-YY format
#[command]
pub fn symbol_master_format_expiry(expiry: String) -> String {
    symbol_master::format_expiry(&expiry)
}

/// Build unified futures symbol
#[command]
pub fn symbol_master_build_futures_symbol(underlying: String, expiry: String) -> String {
    symbol_master::build_futures_symbol(&underlying, &expiry)
}

/// Build unified options symbol
#[command]
pub fn symbol_master_build_options_symbol(
    underlying: String,
    expiry: String,
    strike: f64,
    option_type: String,
) -> String {
    symbol_master::build_options_symbol(&underlying, &expiry, strike, &option_type)
}
