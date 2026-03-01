// Kotak Market Data & Master Contract Commands

use super::{ApiResponse, kotak_quotes_request, parse_auth_token, url_encode, map_exchange_to_kotak, get_kotak_token, store_kotak_master_contract};
use crate::database::pool::get_db;
use reqwest::Client;
use rusqlite::params;
use serde_json::{json, Value};
use tauri::command;

/// Get quotes for a symbol
#[command]
pub async fn kotak_get_quotes(
    auth_token: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    let psymbol = get_kotak_token(&symbol, &exchange).unwrap_or(symbol.clone());
    let kotak_exchange = map_exchange_to_kotak(&exchange);
    let query = format!("{}|{}", kotak_exchange, psymbol);
    let encoded_query = url_encode(&query);
    let endpoint = format!("/script-details/1.0/quotes/neosymbol/{}/all", encoded_query);

    match kotak_quotes_request(&client, &base_url, &endpoint, &access_token).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get market depth for a symbol
#[command]
pub async fn kotak_get_depth(
    auth_token: String,
    symbol: String,
    exchange: String,
) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    let psymbol = get_kotak_token(&symbol, &exchange).unwrap_or(symbol.clone());
    let kotak_exchange = map_exchange_to_kotak(&exchange);
    let query = format!("{}|{}", kotak_exchange, psymbol);
    let encoded_query = url_encode(&query);
    let endpoint = format!("/script-details/1.0/quotes/neosymbol/{}/depth", encoded_query);

    match kotak_quotes_request(&client, &base_url, &endpoint, &access_token).await {
        Ok(data) => ApiResponse { success: true, data: Some(data), error: None },
        Err(e) => ApiResponse { success: false, data: None, error: Some(e) },
    }
}

/// Get historical data (NOT SUPPORTED by Kotak Neo)
#[command]
pub async fn kotak_get_history(
    _auth_token: String,
    _symbol: String,
    _exchange: String,
    _interval: String,
    _from_date: String,
    _to_date: String,
) -> ApiResponse<Value> {
    ApiResponse {
        success: false,
        data: None,
        error: Some("Historical data is not supported by Kotak Neo API".to_string()),
    }
}

/// Download master contract files
#[command]
pub async fn kotak_download_master_contract(auth_token: String) -> ApiResponse<Value> {
    let (_trading_token, _trading_sid, base_url, access_token) = match parse_auth_token(&auth_token) {
        Ok(parts) => parts,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(e) },
    };

    let client = Client::new();
    let endpoint = "/script-details/1.0/masterscrip/file-paths";

    let response = client
        .get(format!("{}{}", base_url, endpoint))
        .header("Authorization", &access_token)
        .header("Content-Type", "application/json")
        .send()
        .await;

    match response {
        Ok(resp) => {
            let text = resp.text().await.unwrap_or_default();
            match serde_json::from_str::<Value>(&text) {
                Ok(data) => {
                    if let Some(file_paths) = data.get("data").and_then(|d| d.get("filesPaths")).and_then(|f| f.as_array()) {
                        let mut total_records = 0;
                        let mut processed_files: Vec<String> = Vec::new();

                        for url_value in file_paths {
                            if let Some(url) = url_value.as_str() {
                                let filename = url.split('/').last().unwrap_or("unknown");

                                if let Ok(csv_resp) = client.get(url).send().await {
                                    if let Ok(csv_text) = csv_resp.text().await {
                                        let exchange = if filename.contains("nse_cm") { "NSE" }
                                            else if filename.contains("bse_cm") { "BSE" }
                                            else if filename.contains("nse_fo") { "NFO" }
                                            else if filename.contains("bse_fo") { "BFO" }
                                            else if filename.contains("cde_fo") { "CDS" }
                                            else if filename.contains("mcx_fo") { "MCX" }
                                            else { continue };

                                        match store_kotak_master_contract(&csv_text, exchange) {
                                            Ok(count) => { total_records += count; processed_files.push(filename.to_string()); }
                                            Err(e) => { eprintln!("Error processing {}: {}", filename, e); }
                                        }
                                    }
                                }
                            }
                        }

                        ApiResponse {
                            success: true,
                            data: Some(json!({ "total_records": total_records, "processed_files": processed_files })),
                            error: None,
                        }
                    } else {
                        ApiResponse { success: false, data: None, error: Some("Failed to get master contract file paths".to_string()) }
                    }
                }
                Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Failed to parse response: {}", e)) },
            }
        }
        Err(e) => ApiResponse { success: false, data: None, error: Some(format!("Request failed: {}", e)) },
    }
}

/// Search symbols in master contract
#[command]
pub async fn kotak_search_symbol(
    query: String,
    exchange: Option<String>,
) -> ApiResponse<Value> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(format!("Failed to get database: {}", e)) },
    };

    let sql = if let Some(exch) = exchange {
        format!(
            "SELECT symbol, brsymbol, token, exchange, brexchange, name, instrumenttype, lotsize
             FROM kotak_master_contract
             WHERE (symbol LIKE '%{}%' OR name LIKE '%{}%') AND exchange = '{}'
             LIMIT 50",
            query, query, exch
        )
    } else {
        format!(
            "SELECT symbol, brsymbol, token, exchange, brexchange, name, instrumenttype, lotsize
             FROM kotak_master_contract
             WHERE symbol LIKE '%{}%' OR name LIKE '%{}%'
             LIMIT 50",
            query, query
        )
    };

    let mut stmt = match db.prepare(&sql) {
        Ok(stmt) => stmt,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(format!("Failed to prepare statement: {}", e)) },
    };

    let results: Vec<Value> = match stmt.query_map([], |row| {
        Ok(json!({
            "symbol": row.get::<_, String>(0).unwrap_or_default(),
            "brsymbol": row.get::<_, String>(1).unwrap_or_default(),
            "token": row.get::<_, String>(2).unwrap_or_default(),
            "exchange": row.get::<_, String>(3).unwrap_or_default(),
            "brexchange": row.get::<_, String>(4).unwrap_or_default(),
            "name": row.get::<_, String>(5).unwrap_or_default(),
            "instrumenttype": row.get::<_, String>(6).unwrap_or_default(),
            "lotsize": row.get::<_, i32>(7).unwrap_or(1)
        }))
    }) {
        Ok(rows) => rows.filter_map(|r| r.ok()).collect(),
        Err(e) => { eprintln!("[kotak_search_symbol] Query failed: {}", e); vec![] }
    };

    ApiResponse { success: true, data: Some(json!(results)), error: None }
}

/// Get token (pSymbol) for a symbol
#[command]
pub async fn kotak_get_token_for_symbol(
    symbol: String,
    exchange: String,
) -> ApiResponse<String> {
    match get_kotak_token(&symbol, &exchange) {
        Some(token) => ApiResponse { success: true, data: Some(token), error: None },
        None => ApiResponse { success: false, data: None, error: Some(format!("Token not found for {} on {}", symbol, exchange)) },
    }
}

/// Get symbol by token
#[command]
pub async fn kotak_get_symbol_by_token(
    token: String,
    exchange: String,
) -> ApiResponse<String> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(format!("Failed to get database: {}", e)) },
    };

    let result: Option<String> = db
        .query_row(
            "SELECT symbol FROM kotak_master_contract WHERE token = ?1 AND exchange = ?2",
            params![token, exchange],
            |row| row.get(0),
        )
        .ok();

    match result {
        Some(symbol) => ApiResponse { success: true, data: Some(symbol), error: None },
        None => ApiResponse { success: false, data: None, error: Some(format!("Symbol not found for token {} on {}", token, exchange)) },
    }
}

/// Get master contract metadata
#[command]
pub async fn kotak_get_master_contract_metadata() -> ApiResponse<Value> {
    let db = match get_db() {
        Ok(db) => db,
        Err(e) => return ApiResponse { success: false, data: None, error: Some(format!("Failed to get database: {}", e)) },
    };

    let last_updated: Option<String> = db
        .query_row(
            "SELECT value FROM broker_metadata WHERE broker = 'kotak' AND key = 'master_contract_updated'",
            [],
            |row| row.get(0),
        )
        .ok();

    let count: i64 = db
        .query_row("SELECT COUNT(*) FROM kotak_master_contract", [], |row| row.get(0))
        .unwrap_or(0);

    ApiResponse {
        success: true,
        data: Some(json!({ "last_updated": last_updated, "total_records": count })),
        error: None,
    }
}
