// Market data Tauri commands
// Frontend can call these commands to fetch live market data

use crate::data_sources::yfinance::{YFinanceProvider, QuoteData, HistoricalData};
use serde::{Deserialize, Serialize};
use serde_json::Value;

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
pub async fn get_market_quote(app: tauri::AppHandle, symbol: String) -> Result<QuoteResponse, String> {

    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;

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
pub async fn get_market_quotes(app: tauri::AppHandle, symbols: Vec<String>) -> Result<QuotesResponse, String> {

    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;
    let quotes = provider.get_quotes(symbols).await;

    Ok(QuotesResponse {
        success: true,
        data: quotes,
        error: None,
    })
}

/// Fetch period returns (7D, 30D)
#[tauri::command]
pub async fn get_period_returns(app: tauri::AppHandle, symbol: String) -> Result<PeriodReturnsResponse, String> {

    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;

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
pub async fn check_market_data_health(app: tauri::AppHandle) -> Result<bool, String> {
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;
    Ok(provider.health_check().await)
}

#[derive(Debug, Serialize, Deserialize)]
pub struct HistoricalResponse {
    pub success: bool,
    pub data: Vec<HistoricalData>,
    pub error: Option<String>,
}

/// Fetch historical data for a symbol
#[tauri::command]
pub async fn get_historical_data(
    app: tauri::AppHandle,
    symbol: String,
    start_date: String,
    end_date: String,
) -> Result<HistoricalResponse, String> {
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;

    match provider.get_historical(&symbol, &start_date, &end_date).await {
        Some(data) => Ok(HistoricalResponse {
            success: true,
            data,
            error: None,
        }),
        None => Ok(HistoricalResponse {
            success: false,
            data: Vec::new(),
            error: Some(format!("Failed to fetch historical data for {}", symbol)),
        }),
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StockInfoResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
}

/// Fetch stock info (company data, metrics, etc.)
#[tauri::command]
pub async fn get_stock_info(app: tauri::AppHandle, symbol: String) -> Result<StockInfoResponse, String> {
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;

    match provider.get_info(&symbol).await {
        Some(info) => Ok(StockInfoResponse {
            success: true,
            data: Some(info),
            error: None,
        }),
        None => Ok(StockInfoResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch info for {}", symbol)),
        }),
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct FinancialsResponse {
    pub success: bool,
    pub data: Option<Value>,
    pub error: Option<String>,
}

/// Fetch financial statements (income, balance sheet, cash flow)
#[tauri::command]
pub async fn get_financials(app: tauri::AppHandle, symbol: String) -> Result<FinancialsResponse, String> {
    let provider = YFinanceProvider::new(&app).map_err(|e| e.to_string())?;

    match provider.get_financials(&symbol).await {
        Some(financials) => Ok(FinancialsResponse {
            success: true,
            data: Some(financials),
            error: None,
        }),
        None => Ok(FinancialsResponse {
            success: false,
            data: None,
            error: Some(format!("Failed to fetch financials for {}", symbol)),
        }),
    }
}
