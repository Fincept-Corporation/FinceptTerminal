//! Saxo Bank Broker Module
//!
//! Full implementation for Saxo Bank OpenAPI integration
//! Supports OAuth 2.0 authentication, trading, positions, and real-time streaming
//!
//! Documentation: https://www.developer.saxo/openapi/learn

use serde_json::{json, Value};
use tauri::command;

// ============================================================================
// API CONFIGURATION
// ============================================================================

const SAXO_SIM_API_BASE: &str = "https://gateway.saxobank.com/sim/openapi";
const SAXO_LIVE_API_BASE: &str = "https://gateway.saxobank.com/openapi";

fn get_api_base(is_paper: bool) -> &'static str {
    if is_paper {
        SAXO_SIM_API_BASE
    } else {
        SAXO_LIVE_API_BASE
    }
}

// ============================================================================
// HTTP CLIENT HELPER
// ============================================================================

async fn saxo_request(
    url: &str,
    method: &str,
    access_token: &str,
    body: Option<&str>,
) -> Result<Value, String> {
    let client = reqwest::Client::new();

    let mut request = match method {
        "GET" => client.get(url),
        "POST" => client.post(url),
        "PUT" => client.put(url),
        "PATCH" => client.patch(url),
        "DELETE" => client.delete(url),
        _ => return Err(format!("Unsupported HTTP method: {}", method)),
    };

    request = request
        .header("Authorization", format!("Bearer {}", access_token))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json");

    if let Some(body_str) = body {
        request = request.body(body_str.to_string());
    }

    let response = request.send().await.map_err(|e| e.to_string())?;
    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if status.is_success() {
        if text.is_empty() {
            Ok(json!({"success": true}))
        } else {
            serde_json::from_str(&text).map_err(|e| e.to_string())
        }
    } else {
        Err(format!("Saxo API error ({}): {}", status, text))
    }
}

// ============================================================================
// OAUTH AUTHENTICATION
// ============================================================================

/// Exchange authorization code for tokens
#[command]
pub async fn saxo_exchange_token(
    token_endpoint: String,
    code: String,
    client_id: String,
    client_secret: String,
    redirect_uri: String,
) -> Result<Value, String> {
    let client = reqwest::Client::new();

    let params = [
        ("grant_type", "authorization_code"),
        ("code", &code),
        ("client_id", &client_id),
        ("client_secret", &client_secret),
        ("redirect_uri", &redirect_uri),
    ];

    let response = client
        .post(&token_endpoint)
        .form(&params)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if status.is_success() {
        serde_json::from_str(&text).map_err(|e| e.to_string())
    } else {
        Err(format!("Token exchange failed ({}): {}", status, text))
    }
}

/// Refresh access token
#[command]
pub async fn saxo_refresh_token(
    token_endpoint: String,
    refresh_token: String,
    client_id: String,
    client_secret: String,
) -> Result<Value, String> {
    let client = reqwest::Client::new();

    let params = [
        ("grant_type", "refresh_token"),
        ("refresh_token", &refresh_token),
        ("client_id", &client_id),
        ("client_secret", &client_secret),
    ];

    let response = client
        .post(&token_endpoint)
        .form(&params)
        .send()
        .await
        .map_err(|e| e.to_string())?;

    let status = response.status();
    let text = response.text().await.map_err(|e| e.to_string())?;

    if status.is_success() {
        serde_json::from_str(&text).map_err(|e| e.to_string())
    } else {
        Err(format!("Token refresh failed ({}): {}", status, text))
    }
}

// ============================================================================
// GENERIC API REQUEST
// ============================================================================

/// Generic Saxo API request
#[command]
pub async fn saxo_api_request(
    url: String,
    method: String,
    access_token: String,
    body: Option<String>,
) -> Result<Value, String> {
    saxo_request(&url, &method, &access_token, body.as_deref()).await
}

// ============================================================================
// ROOT SERVICES
// ============================================================================

/// Get current user info
#[command]
pub async fn saxo_get_user(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/root/v1/user", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get active sessions
#[command]
pub async fn saxo_get_sessions(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/root/v1/sessions", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get available features
#[command]
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
#[command]
pub async fn saxo_get_client(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/port/v1/clients/me", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get all accounts for client
#[command]
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
#[command]
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
#[command]
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
// ORDERS
// ============================================================================

/// Get all orders
#[command]
pub async fn saxo_get_orders(
    access_token: String,
    client_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/orders?ClientKey={}",
        get_api_base(is_paper),
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get specific order
#[command]
pub async fn saxo_get_order(
    access_token: String,
    client_key: String,
    order_id: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/orders/{}?ClientKey={}",
        get_api_base(is_paper),
        order_id,
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Place an order
#[command]
pub async fn saxo_place_order(
    access_token: String,
    order_request: Value,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/trade/v2/orders", get_api_base(is_paper));
    let body = serde_json::to_string(&order_request).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body)).await
}

/// Modify an order
#[command]
pub async fn saxo_modify_order(
    access_token: String,
    order_id: String,
    modify_request: Value,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/trade/v2/orders/{}", get_api_base(is_paper), order_id);
    let body = serde_json::to_string(&modify_request).map_err(|e| e.to_string())?;
    saxo_request(&url, "PATCH", &access_token, Some(&body)).await
}

/// Cancel an order
#[command]
pub async fn saxo_cancel_order(
    access_token: String,
    order_id: String,
    account_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/trade/v2/orders/{}?AccountKey={}",
        get_api_base(is_paper),
        order_id,
        account_key
    );
    saxo_request(&url, "DELETE", &access_token, None).await
}

/// Preview order (get margin impact, costs, etc.)
#[command]
pub async fn saxo_preview_order(
    access_token: String,
    order_request: Value,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/trade/v2/orders/precheck", get_api_base(is_paper));
    let body = serde_json::to_string(&order_request).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body)).await
}

// ============================================================================
// POSITIONS
// ============================================================================

/// Get all positions
#[command]
pub async fn saxo_get_positions(
    access_token: String,
    client_key: String,
    field_groups: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let fields = field_groups.unwrap_or_else(|| {
        "PositionBase,PositionView,DisplayAndFormat,ExchangeInfo".to_string()
    });
    let url = format!(
        "{}/port/v1/positions?ClientKey={}&FieldGroups={}",
        get_api_base(is_paper),
        client_key,
        fields
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get net positions (aggregated)
#[command]
pub async fn saxo_get_net_positions(
    access_token: String,
    client_key: String,
    field_groups: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let fields = field_groups.unwrap_or_else(|| {
        "NetPositionBase,NetPositionView,DisplayAndFormat,ExchangeInfo".to_string()
    });
    let url = format!(
        "{}/port/v1/netpositions?ClientKey={}&FieldGroups={}",
        get_api_base(is_paper),
        client_key,
        fields
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get specific position
#[command]
pub async fn saxo_get_position(
    access_token: String,
    position_id: String,
    client_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/positions/{}?ClientKey={}",
        get_api_base(is_paper),
        position_id,
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Close a position
#[command]
pub async fn saxo_close_position(
    access_token: String,
    position_id: String,
    account_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/trade/v1/positions/{}?AccountKey={}",
        get_api_base(is_paper),
        position_id,
        account_key
    );
    saxo_request(&url, "DELETE", &access_token, None).await
}

/// Get closed positions
#[command]
pub async fn saxo_get_closed_positions(
    access_token: String,
    client_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/closedpositions?ClientKey={}",
        get_api_base(is_paper),
        client_key
    );
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// MARKET DATA
// ============================================================================

/// Search for instruments
#[command]
pub async fn saxo_search_instruments(
    access_token: String,
    keywords: String,
    asset_types: Option<String>,
    exchange_id: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let mut url = format!(
        "{}/ref/v1/instruments?Keywords={}&IncludeNonTradable=false",
        get_api_base(is_paper),
        urlencoding::encode(&keywords)
    );

    if let Some(types) = asset_types {
        url.push_str(&format!("&AssetTypes={}", types));
    }

    if let Some(exchange) = exchange_id {
        url.push_str(&format!("&ExchangeId={}", exchange));
    }

    saxo_request(&url, "GET", &access_token, None).await
}

/// Get instrument details
#[command]
pub async fn saxo_get_instrument(
    access_token: String,
    uic: i64,
    asset_type: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/ref/v1/instruments/details/{}/{}",
        get_api_base(is_paper),
        uic,
        asset_type
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get quote/info price
#[command]
pub async fn saxo_get_quote(
    access_token: String,
    uic: i64,
    asset_type: String,
    field_groups: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let fields = field_groups.unwrap_or_else(|| {
        "DisplayAndFormat,InstrumentPriceDetails,Quote".to_string()
    });
    let url = format!(
        "{}/trade/v1/infoprices/{}/{}?FieldGroups={}",
        get_api_base(is_paper),
        uic,
        asset_type,
        fields
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get quotes for multiple instruments
#[command]
pub async fn saxo_get_quotes_batch(
    access_token: String,
    uics: Vec<i64>,
    asset_type: String,
    is_paper: bool,
) -> Result<Value, String> {
    let uics_str: Vec<String> = uics.iter().map(|u| u.to_string()).collect();
    let url = format!(
        "{}/trade/v1/infoprices/list?Uics={}&AssetType={}&FieldGroups=DisplayAndFormat,Quote",
        get_api_base(is_paper),
        uics_str.join(","),
        asset_type
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get chart data (OHLC)
#[command]
pub async fn saxo_get_chart_data(
    access_token: String,
    uic: i64,
    asset_type: String,
    horizon: i32,
    count: i32,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/chart/v1/charts?AssetType={}&Uic={}&Horizon={}&Count={}",
        get_api_base(is_paper),
        asset_type,
        uic,
        horizon,
        count
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get chart data with time range
#[command]
pub async fn saxo_get_chart_data_range(
    access_token: String,
    uic: i64,
    asset_type: String,
    horizon: i32,
    from_time: String,
    to_time: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let mut url = format!(
        "{}/chart/v1/charts?AssetType={}&Uic={}&Horizon={}&Mode=From&Time={}",
        get_api_base(is_paper),
        asset_type,
        uic,
        horizon,
        urlencoding::encode(&from_time)
    );

    if let Some(to) = to_time {
        url.push_str(&format!("&ToTime={}", urlencoding::encode(&to)));
    }

    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// REFERENCE DATA
// ============================================================================

/// Get available exchanges
#[command]
pub async fn saxo_get_exchanges(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/exchanges", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get exchange details
#[command]
pub async fn saxo_get_exchange(
    access_token: String,
    exchange_id: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/ref/v1/exchanges/{}",
        get_api_base(is_paper),
        exchange_id
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get available currencies
#[command]
pub async fn saxo_get_currencies(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/currencies", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get countries
#[command]
pub async fn saxo_get_countries(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/countries", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get time zones
#[command]
pub async fn saxo_get_timezones(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/timezones", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// ACCOUNT HISTORY
// ============================================================================

/// Get account performance
#[command]
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
#[command]
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
#[command]
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
// STREAMING SUBSCRIPTIONS
// ============================================================================

/// Create price subscription
#[command]
pub async fn saxo_create_price_subscription(
    access_token: String,
    context_id: String,
    reference_id: String,
    uic: i64,
    asset_type: String,
    field_groups: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/trade/v1/prices/subscriptions",
        get_api_base(is_paper)
    );

    let fields = field_groups.unwrap_or_else(|| "Quote".to_string());

    let body = json!({
        "ContextId": context_id,
        "ReferenceId": reference_id,
        "Arguments": {
            "AssetType": asset_type,
            "Uic": uic,
            "FieldGroups": fields.split(',').collect::<Vec<&str>>()
        }
    });

    let body_str = serde_json::to_string(&body).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body_str)).await
}

/// Delete price subscription
#[command]
pub async fn saxo_delete_price_subscription(
    access_token: String,
    context_id: String,
    reference_id: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/trade/v1/prices/subscriptions/{}/{}",
        get_api_base(is_paper),
        context_id,
        reference_id
    );
    saxo_request(&url, "DELETE", &access_token, None).await
}

/// Create order subscription
#[command]
pub async fn saxo_create_order_subscription(
    access_token: String,
    context_id: String,
    reference_id: String,
    client_key: String,
    account_key: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/orders/subscriptions",
        get_api_base(is_paper)
    );

    let mut args = json!({
        "ClientKey": client_key
    });

    if let Some(key) = account_key {
        args["AccountKey"] = json!(key);
    }

    let body = json!({
        "ContextId": context_id,
        "ReferenceId": reference_id,
        "Arguments": args
    });

    let body_str = serde_json::to_string(&body).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body_str)).await
}

/// Create position subscription
#[command]
pub async fn saxo_create_position_subscription(
    access_token: String,
    context_id: String,
    reference_id: String,
    client_key: String,
    account_key: Option<String>,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/positions/subscriptions",
        get_api_base(is_paper)
    );

    let mut args = json!({
        "ClientKey": client_key,
        "FieldGroups": ["PositionBase", "PositionView", "DisplayAndFormat"]
    });

    if let Some(key) = account_key {
        args["AccountKey"] = json!(key);
    }

    let body = json!({
        "ContextId": context_id,
        "ReferenceId": reference_id,
        "Arguments": args
    });

    let body_str = serde_json::to_string(&body).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body_str)).await
}

/// Create balance subscription
#[command]
pub async fn saxo_create_balance_subscription(
    access_token: String,
    context_id: String,
    reference_id: String,
    account_key: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/port/v1/balances/subscriptions",
        get_api_base(is_paper)
    );

    let body = json!({
        "ContextId": context_id,
        "ReferenceId": reference_id,
        "Arguments": {
            "AccountKey": account_key
        }
    });

    let body_str = serde_json::to_string(&body).map_err(|e| e.to_string())?;
    saxo_request(&url, "POST", &access_token, Some(&body_str)).await
}

// ============================================================================
// EXPOSURE & RISK
// ============================================================================

/// Get instrument exposure
#[command]
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

// ============================================================================
// OPTIONS
// ============================================================================

/// Get option chain
#[command]
pub async fn saxo_get_option_chain(
    access_token: String,
    uic: i64,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/ref/v1/instruments?OptionRootId={}&AssetTypes=StockOption,StockIndexOption",
        get_api_base(is_paper),
        uic
    );
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get option expiry dates
#[command]
pub async fn saxo_get_option_expiries(
    access_token: String,
    option_root_id: i64,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!(
        "{}/ref/v1/standarddates/forwarddates?OptionRootId={}",
        get_api_base(is_paper),
        option_root_id
    );
    saxo_request(&url, "GET", &access_token, None).await
}
