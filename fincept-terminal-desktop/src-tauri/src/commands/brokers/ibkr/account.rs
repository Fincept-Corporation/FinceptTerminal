// IBKR Account, Positions, Trades, and Analytics Commands

use serde_json::{json, Value};
use crate::commands::brokers::common::ApiResponse;
use super::{get_api_base, create_ibkr_headers, create_client};

// ============================================================================
// Account Commands
// ============================================================================

/// Get list of accounts
#[tauri::command]
pub async fn ibkr_get_accounts(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_accounts] Fetching accounts");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/accounts", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        let error_msg = body.get("error").and_then(|e| e.as_str())
            .unwrap_or("Failed to fetch accounts").to_string();
        Ok(ApiResponse { success: false, data: None, error: Some(error_msg), timestamp })
    }
}

/// Switch active account
#[tauri::command]
pub async fn ibkr_switch_account(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_switch_account] Switching to account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "acctId": account_id });

    let response = client
        .post(format!("{}/iserver/account", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to switch account".to_string()), timestamp })
    }
}

/// Get account summary
#[tauri::command]
pub async fn ibkr_get_account_summary(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_account_summary] Fetching summary for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/portfolio/{}/summary", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch account summary".to_string()), timestamp })
    }
}

/// Get account ledger
#[tauri::command]
pub async fn ibkr_get_account_ledger(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_account_ledger] Fetching ledger for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/portfolio/{}/ledger", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch account ledger".to_string()), timestamp })
    }
}

/// Get account allocation
#[tauri::command]
pub async fn ibkr_get_account_allocation(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_account_allocation] Fetching allocation for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/portfolio/{}/allocation", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch account allocation".to_string()), timestamp })
    }
}

// ============================================================================
// Position Commands
// ============================================================================

/// Get positions for an account
#[tauri::command]
pub async fn ibkr_get_positions(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    page_id: Option<i32>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_get_positions] Fetching positions for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!("{}/portfolio/{}/positions/0", base_url, account_id);
    if let Some(page) = page_id {
        url = format!("{}/portfolio/{}/positions/{}", base_url, account_id, page);
    }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch positions".to_string()), timestamp })
    }
}

/// Get position by conid
#[tauri::command]
pub async fn ibkr_get_position_by_conid(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    conid: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_position_by_conid] Fetching position for conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/portfolio/{}/position/{}", base_url, account_id, conid))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Position not found".to_string()), timestamp })
    }
}

/// Invalidate position cache (force refresh)
#[tauri::command]
pub async fn ibkr_invalidate_positions(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_invalidate_positions] Invalidating position cache for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/portfolio/{}/positions/invalidate", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(true), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: Some(false), error: Some("Failed to invalidate positions".to_string()), timestamp })
    }
}

// ============================================================================
// Trade Log Commands
// ============================================================================

/// Get trades/executions
#[tauri::command]
pub async fn ibkr_get_trades(
    access_token: Option<String>,
    use_gateway: bool,
    days: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_get_trades] Fetching trades");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!("{}/iserver/account/trades", base_url);
    if let Some(d) = days {
        url = format!("{}?days={}", url, d);
    }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch trades".to_string()), timestamp })
    }
}

// ============================================================================
// Performance & Reporting Commands
// ============================================================================

/// Get performance data
#[tauri::command]
pub async fn ibkr_get_performance(
    access_token: Option<String>,
    use_gateway: bool,
    account_ids: Vec<String>,
    period: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_performance] Fetching performance data");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut body = json!({ "acctIds": account_ids });
    if let Some(p) = period {
        body["period"] = json!(p);
    }

    let response = client
        .post(format!("{}/pa/performance", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch performance".to_string()), timestamp })
    }
}

/// Get transaction history
#[tauri::command]
pub async fn ibkr_get_transactions(
    access_token: Option<String>,
    use_gateway: bool,
    account_ids: Vec<String>,
    conids: Option<Vec<i64>>,
    currency: Option<String>,
    days: Option<i32>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_transactions] Fetching transaction history");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut body = json!({ "acctIds": account_ids });
    if let Some(c) = conids {
        body["conids"] = json!(c);
    }
    if let Some(cur) = currency {
        body["currency"] = json!(cur);
    }
    if let Some(d) = days {
        body["days"] = json!(d);
    }

    let response = client
        .post(format!("{}/pa/transactions", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse { success: true, data: Some(body), error: None, timestamp })
    } else {
        Ok(ApiResponse { success: false, data: None, error: Some("Failed to fetch transactions".to_string()), timestamp })
    }
}
