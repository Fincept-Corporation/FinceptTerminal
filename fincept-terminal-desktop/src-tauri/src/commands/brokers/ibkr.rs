//! Interactive Brokers (IBKR) Broker Integration
//!
//! Complete API implementation for IBKR Client Portal API including:
//! - Session Management (authentication, tickle, logout)
//! - Account Management (accounts, summary, ledger)
//! - Order Management (place, modify, cancel)
//! - Portfolio (positions, account summary)
//! - Market Data (snapshots, historical data)
//! - Contract Search (symbol lookup, security definitions)
//!
//! Supports both live (U-prefix) and paper (DU-prefix) trading accounts.
//! Works with Client Portal Gateway (localhost) or IBKR API (api.ibkr.com).

use reqwest::header::{HeaderMap, HeaderValue, CONTENT_TYPE};
use serde_json::{json, Value};

use super::common::ApiResponse;

// ============================================================================
// IBKR API Configuration
// ============================================================================

// IBKR Client Portal API (requires OAuth authentication)
const IBKR_API_BASE: &str = "https://api.ibkr.com/v1/api";

// IBKR Gateway (runs locally, requires IB Gateway or TWS)
const IBKR_GATEWAY_BASE: &str = "https://localhost:5000/v1/api";

fn get_api_base(use_gateway: bool) -> &'static str {
    if use_gateway {
        IBKR_GATEWAY_BASE
    } else {
        IBKR_API_BASE
    }
}

fn create_ibkr_headers(access_token: Option<&str>) -> HeaderMap {
    let mut headers = HeaderMap::new();

    if let Some(token) = access_token {
        if let Ok(auth_value) = HeaderValue::from_str(&format!("Bearer {}", token)) {
            headers.insert("Authorization", auth_value);
        }
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers
}

// Create a reqwest client that accepts self-signed certificates (for gateway)
fn create_client(use_gateway: bool) -> reqwest::Client {
    if use_gateway {
        reqwest::Client::builder()
            .danger_accept_invalid_certs(true) // Gateway uses self-signed cert
            .build()
            .unwrap_or_else(|_| reqwest::Client::new())
    } else {
        reqwest::Client::new()
    }
}

// ============================================================================
// IBKR Session Management
// ============================================================================

/// Check authentication status
#[tauri::command]
pub async fn ibkr_get_auth_status(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_auth_status] Checking authentication status");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/auth/status", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Not authenticated".to_string()),
            timestamp,
        })
    }
}

/// Ping/tickle the server to keep session alive
#[tauri::command]
pub async fn ibkr_tickle(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_tickle] Sending tickle to keep session alive");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/tickle", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Tickle failed".to_string()),
            timestamp,
        })
    }
}

/// Logout from IBKR session
#[tauri::command]
pub async fn ibkr_logout(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_logout] Logging out");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/logout", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Logout failed".to_string()),
            timestamp,
        })
    }
}

/// Validate SSO token (Gateway mode)
#[tauri::command]
pub async fn ibkr_validate_sso(
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_validate_sso] Validating SSO token");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(None);
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/sso/validate", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("SSO validation failed".to_string()),
            timestamp,
        })
    }
}

/// Reauthenticate session
#[tauri::command]
pub async fn ibkr_reauthenticate(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_reauthenticate] Reauthenticating session");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/reauthenticate", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Reauthentication failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Account Commands
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Failed to fetch accounts")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to switch account".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch account summary".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch account ledger".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch account allocation".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Position Commands
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch positions".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Position not found".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Failed to invalidate positions".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Order Commands
// ============================================================================

/// Get live orders
#[tauri::command]
pub async fn ibkr_get_orders(
    access_token: Option<String>,
    use_gateway: bool,
    filters: Option<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_orders] Fetching live orders");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!("{}/iserver/account/orders", base_url);
    if let Some(f) = filters {
        url = format!("{}?filters={}", url, f);
    }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch orders".to_string()),
            timestamp,
        })
    }
}

/// Get specific order by order ID
#[tauri::command]
pub async fn ibkr_get_order(
    access_token: Option<String>,
    use_gateway: bool,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_order] Fetching order: {}", order_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/account/order/status/{}", base_url, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Order not found".to_string()),
            timestamp,
        })
    }
}

/// Place order
#[tauri::command]
pub async fn ibkr_place_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_place_order] Placing order for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "orders": orders });

    let response = client
        .post(format!("{}/iserver/account/{}/orders", base_url, account_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        eprintln!("[ibkr_place_order] Order response: {:?}", body);
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Order placement failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Reply to order confirmation message
#[tauri::command]
pub async fn ibkr_reply_to_order(
    access_token: Option<String>,
    use_gateway: bool,
    reply_id: String,
    confirmed: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_reply_to_order] Replying to order message: {} (confirmed: {})", reply_id, confirmed);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "confirmed": confirmed });

    let response = client
        .post(format!("{}/iserver/reply/{}", base_url, reply_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to confirm order".to_string()),
            timestamp,
        })
    }
}

/// Modify order
#[tauri::command]
pub async fn ibkr_modify_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    order_id: String,
    order_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_modify_order] Modifying order: {} for account: {}", order_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/account/{}/order/{}", base_url, account_id, order_id))
        .headers(headers)
        .json(&order_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Order modification failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Cancel order
#[tauri::command]
pub async fn ibkr_cancel_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    order_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_cancel_order] Cancelling order: {} for account: {}", order_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .delete(format!("{}/iserver/account/{}/order/{}", base_url, account_id, order_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Order cancellation failed")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

/// Preview order (what-if)
#[tauri::command]
pub async fn ibkr_preview_order(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    orders: Vec<Value>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_preview_order] Previewing order for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "orders": orders });

    let response = client
        .post(format!("{}/iserver/account/{}/orders/whatif", base_url, account_id))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Order preview failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Contract/Symbol Commands
// ============================================================================

/// Search for contracts by symbol
#[tauri::command]
pub async fn ibkr_search_contracts(
    access_token: Option<String>,
    use_gateway: bool,
    symbol: String,
    sec_type: Option<String>,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_search_contracts] Searching for: {}", symbol);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut body = json!({ "symbol": symbol });
    if let Some(st) = sec_type {
        body["secType"] = json!(st);
    }

    let response = client
        .post(format!("{}/iserver/secdef/search", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Contract search failed".to_string()),
            timestamp,
        })
    }
}

/// Get contract details by conid
#[tauri::command]
pub async fn ibkr_get_contract(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_contract] Fetching contract: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/contract/{}/info", base_url, conid))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Contract not found".to_string()),
            timestamp,
        })
    }
}

/// Get contract rules (trading rules, hours)
#[tauri::command]
pub async fn ibkr_get_contract_rules(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
    is_buy: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_contract_rules] Fetching rules for conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({
        "conid": conid,
        "isBuy": is_buy
    });

    let response = client
        .post(format!("{}/iserver/contract/rules", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch contract rules".to_string()),
            timestamp,
        })
    }
}

/// Get security definition info for multiple conids
#[tauri::command]
pub async fn ibkr_get_secdef_info(
    access_token: Option<String>,
    use_gateway: bool,
    conids: Vec<i64>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_secdef_info] Fetching secdef for {} conids", conids.len());

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let body = json!({ "conids": conids });

    let response = client
        .post(format!("{}/trsrv/secdef", base_url))
        .headers(headers)
        .json(&body)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch security definitions".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Market Data Commands
// ============================================================================

/// Get market data snapshot for conids
#[tauri::command]
pub async fn ibkr_get_snapshot(
    access_token: Option<String>,
    use_gateway: bool,
    conids: Vec<i64>,
    fields: Vec<String>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_snapshot] Fetching snapshot for {} conids", conids.len());

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let conids_param = conids.iter().map(|c| c.to_string()).collect::<Vec<_>>().join(",");
    let fields_param = fields.join(",");

    let response = client
        .get(format!("{}/iserver/marketdata/snapshot?conids={}&fields={}", base_url, conids_param, fields_param))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch snapshot".to_string()),
            timestamp,
        })
    }
}

/// Unsubscribe from market data
#[tauri::command]
pub async fn ibkr_unsubscribe_market_data(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_unsubscribe_market_data] Unsubscribing from conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/marketdata/{}/unsubscribe", base_url, conid))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Failed to unsubscribe".to_string()),
            timestamp,
        })
    }
}

/// Unsubscribe from all market data
#[tauri::command]
pub async fn ibkr_unsubscribe_all_market_data(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_unsubscribe_all_market_data] Unsubscribing from all market data");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/marketdata/unsubscribeall", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Failed to unsubscribe all".to_string()),
            timestamp,
        })
    }
}

/// Get historical data (bars)
#[tauri::command]
pub async fn ibkr_get_historical_data(
    access_token: Option<String>,
    use_gateway: bool,
    conid: i64,
    period: String,        // e.g., "1d", "1w", "1m", "1y"
    bar_size: String,      // e.g., "1min", "5min", "1h", "1d"
    outside_rth: Option<bool>,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_historical_data] Fetching history for conid: {}", conid);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let mut url = format!(
        "{}/iserver/marketdata/history?conid={}&period={}&bar={}",
        base_url, conid, period, bar_size
    );

    if let Some(orh) = outside_rth {
        url = format!("{}&outsideRth={}", url, orh);
    }

    let response = client
        .get(&url)
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        let error_msg = body.get("error")
            .and_then(|e| e.as_str())
            .unwrap_or("Failed to fetch historical data")
            .to_string();
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some(error_msg),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Scanner Commands
// ============================================================================

/// Get available scanner parameters
#[tauri::command]
pub async fn ibkr_get_scanner_params(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_get_scanner_params] Fetching scanner parameters");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/scanner/params", base_url))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch scanner params".to_string()),
            timestamp,
        })
    }
}

/// Run market scanner
#[tauri::command]
pub async fn ibkr_run_scanner(
    access_token: Option<String>,
    use_gateway: bool,
    scanner_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_run_scanner] Running scanner");

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/scanner/run", base_url))
        .headers(headers)
        .json(&scanner_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Scanner run failed".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Alert Commands
// ============================================================================

/// Get alerts
#[tauri::command]
pub async fn ibkr_get_alerts(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[ibkr_get_alerts] Fetching alerts for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .get(format!("{}/iserver/account/{}/alerts", base_url, account_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Vec<Value> = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch alerts".to_string()),
            timestamp,
        })
    }
}

/// Create alert
#[tauri::command]
pub async fn ibkr_create_alert(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    alert_params: Value,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[ibkr_create_alert] Creating alert for account: {}", account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .post(format!("{}/iserver/account/{}/alert", base_url, account_id))
        .headers(headers)
        .json(&alert_params)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to create alert".to_string()),
            timestamp,
        })
    }
}

/// Delete alert
#[tauri::command]
pub async fn ibkr_delete_alert(
    access_token: Option<String>,
    use_gateway: bool,
    account_id: String,
    alert_id: String,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[ibkr_delete_alert] Deleting alert: {} for account: {}", alert_id, account_id);

    let client = create_client(use_gateway);
    let headers = create_ibkr_headers(access_token.as_deref());
    let base_url = get_api_base(use_gateway);

    let response = client
        .delete(format!("{}/iserver/account/{}/alert/{}", base_url, account_id, alert_id))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let status = response.status();
    let timestamp = chrono::Utc::now().timestamp_millis();

    if status.is_success() {
        Ok(ApiResponse {
            success: true,
            data: Some(true),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: Some(false),
            error: Some("Failed to delete alert".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Trade Log Commands
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch trades".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// IBKR Performance & Reporting Commands
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch performance".to_string()),
            timestamp,
        })
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
        Ok(ApiResponse {
            success: true,
            data: Some(body),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch transactions".to_string()),
            timestamp,
        })
    }
}

// ============================================================================
// Credential Storage
// ============================================================================

/// Store IBKR credentials (validates by checking auth status)
#[tauri::command]
pub async fn store_ibkr_credentials(
    access_token: Option<String>,
    use_gateway: bool,
) -> Result<ApiResponse<bool>, String> {
    eprintln!("[store_ibkr_credentials] Validating IBKR credentials");

    // Validate credentials by checking auth status
    let validate = ibkr_get_auth_status(access_token, use_gateway).await?;

    let timestamp = chrono::Utc::now().timestamp_millis();

    Ok(ApiResponse {
        success: validate.success,
        data: Some(validate.success),
        error: validate.error,
        timestamp,
    })
}
