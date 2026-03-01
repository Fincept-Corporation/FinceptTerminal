use super::{get_api_base, saxo_request};
use serde_json::Value;

// ============================================================================
// ORDERS
// ============================================================================

/// Get all orders
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
#[tauri::command]
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
