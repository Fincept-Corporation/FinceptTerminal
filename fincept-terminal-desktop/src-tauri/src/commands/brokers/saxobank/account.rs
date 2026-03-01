use super::{get_api_base, saxo_request};
use serde_json::Value;

// ============================================================================
// ROOT SERVICES
// ============================================================================

/// Get current user info
#[tauri::command]
pub async fn saxo_get_user(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/root/v1/user", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get active sessions
#[tauri::command]
pub async fn saxo_get_sessions(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/root/v1/sessions", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get available features
#[tauri::command]
pub async fn saxo_get_features(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/root/v1/features", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// CLIENT & ACCOUNT MANAGEMENT
// ============================================================================

/// Get client info (includes ClientKey and DefaultAccountKey)
#[tauri::command]
pub async fn saxo_get_client(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/port/v1/clients/me", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get all accounts for client
#[tauri::command]
pub async fn saxo_get_accounts(
    access_token: String,
    client_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/accounts?ClientKey={}",
        get_api_base(is_paper),
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get specific account details
#[tauri::command]
pub async fn saxo_get_account(
    access_token: String,
    account_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/accounts/{}",
        get_api_base(is_paper),
        account_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get account balance
#[tauri::command]
pub async fn saxo_get_balance(
    access_token: String,
    account_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/balances?AccountKey={}",
        get_api_base(is_paper),
        account_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// ACCOUNT HISTORY
// ============================================================================

/// Get account performance
#[tauri::command]
pub async fn saxo_get_performance(
    access_token: String,
    account_key: String,
    from_date: Option<String>,
    to_date: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let mut url = format!(
        "{}/hist/v1/performance/{}",
        get_api_base(is_paper),
        account_key
    );

    let mut params = Vec::new();
    if let Some(from) = from_date {
        params.push(format!("FromDate={}", from));
    }
    if let Some(to) = to_date {
        params.push(format!("ToDate={}", to));
    }

    if !params.is_empty() {
        url.push('?');
        url.push_str(&params.join("&"));
    }

    saxo_request(&url, "GET", &access_token, None).await
}

/// Get performance time series
#[tauri::command]
pub async fn saxo_get_performance_timeseries(
    access_token: String,
    account_key: String,
    from_date: String,
    to_date: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/hist/v1/performance/timeseries/{}?FromDate={}&ToDate={}",
        get_api_base(is_paper),
        account_key,
        from_date,
        to_date
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get order audit log
#[tauri::command]
pub async fn saxo_get_order_audit(
    access_token: String,
    client_key: String,
    from_date: String,
    to_date: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/hist/v2/audit/orderactivities?ClientKey={}&FromDateTime={}&ToDateTime={}",
        get_api_base(is_paper),
        client_key,
        urlencoding::encode(&from_date),
        urlencoding::encode(&to_date)
    );
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// EXPOSURE & RISK
// ============================================================================

/// Get instrument exposure
#[tauri::command]
pub async fn saxo_get_exposure(
    access_token: String,
    client_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/exposure/instruments?ClientKey={}",
        get_api_base(is_paper),
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}
