// Market data Tauri commands
// Frontend can call these commands to fetch live market data

use crate::data_sources::yfinance::{YFinanceProvider, QuoteData};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct QuoteResponse {
    pub success: bool,
    pub data: Option<QuoteData>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct QuotesResponse {
    pub success: bool,
    pub data: Vec<QuoteData>,
    pub error: Option<String>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PeriodReturns {
    pub symbol: String,
    pub seven_day: f64,
    pub thirty_day: f64,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PeriodReturnsResponse {
    pub success: bool,
    pub data: Option<PeriodReturns>,
    pub error: Option<String>,
}

/// Fetch a single quote
#[tauri::command]
pub async fn get_market_quote(symbol: String) -> Result<QuoteResponse, String> {

    let provider = YFinanceProvider::new();

    match provider.get_quote(&symbol).await {
        Some(quote) => Ok(QuoteResponse {
            success: true,
            data: Some(quote),
            error: None,
        }),
        None => Ok(QuoteResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch quote for {}", symbol)),
        }),
    }
}

/// Fetch multiple quotes (batch)
#[tauri::command]
pub async fn get_market_quotes(symbols: Vec<String>) -> Result<QuotesResponse, String> {

    let provider = YFinanceProvider::new();
    let quotes = provider.get_quotes(symbols).await;

    Ok(QuotesResponse {
        success: true,
        data: quotes,
        error: None,
    })
}

/// Fetch period returns (7D, 30D)
#[tauri::command]
pub async fn get_period_returns(symbol: String) -> Result<PeriodReturnsResponse, String> {

    let provider = YFinanceProvider::new();

    match provider.get_period_returns(&symbol).await {
        Some((seven_day, thirty_day)) => Ok(PeriodReturnsResponse {
            success: true,
            data: Some(PeriodReturns {
                symbol,
                seven_day,
                thirty_day,
            }),
            error: None,
        }),
        None => Ok(PeriodReturnsResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch returns for {}", symbol)),
        }),
    }
}

/// Health check for market data provider
#[tauri::command]
pub async fn check_market_data_health() -> Result<bool, String> {

    let provider = YFinanceProvider::new();
    Ok(provider.health_check().await)
}
