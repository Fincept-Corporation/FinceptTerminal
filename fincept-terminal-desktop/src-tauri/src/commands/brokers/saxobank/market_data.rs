use super::{get_api_base, saxo_request};
use serde_json::{json, Value};

// ============================================================================
// MARKET DATA
// ============================================================================

/// Search for instruments
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
pub async fn saxo_get_exchanges(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/exchanges", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get exchange details
#[tauri::command]
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
#[tauri::command]
pub async fn saxo_get_currencies(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/currencies", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get countries
#[tauri::command]
pub async fn saxo_get_countries(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/countries", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

/// Get time zones
#[tauri::command]
pub async fn saxo_get_timezones(
    access_token: String,
    is_paper: bool,
) -> Result<Value, String> {
    let url = format!("{}/ref/v1/timezones", get_api_base(is_paper));
    saxo_request(&url, "GET", &access_token, None).await
}

// ============================================================================
// OPTIONS
// ============================================================================

/// Get option chain
#[tauri::command]
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
#[tauri::command]
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

// ============================================================================
// STREAMING SUBSCRIPTIONS
// ============================================================================

/// Create price subscription
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
