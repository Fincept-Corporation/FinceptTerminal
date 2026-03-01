use super::{FIVEPAISA_BASE_URL, FivePaisaResponse, FivePaisaApiResponse, create_client, map_exchange, map_exchange_type, map_order_side, is_intraday};
use serde_json::json;

/// Place a new order
#[tauri::command]
pub async fn fivepaisa_place_order(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    _symbol: String,
    scrip_code: i64,
    side: String,
    quantity: i32,
    price: f64,
    trigger_price: f64,
    product: String,
    disclosed_quantity: Option<i32>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "OrderType": map_order_side(&side),
            "Exchange": map_exchange(&exchange),
            "ExchangeType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "Price": price,
            "Qty": quantity,
            "StopLossPrice": trigger_price,
            "DisQty": disclosed_quantity.unwrap_or(0),
            "IsIntraday": is_intraday(&product),
            "AHPlaced": "N",
            "RemoteOrderID": "FinceptTerminal"
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/PlaceOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Place order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse order response: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        let order_id = data.body.get("BrokerOrderID")
            .and_then(|v| v.as_i64())
            .map(|v| v.to_string())
            .or_else(|| data.body.get("BrokerOrderID").and_then(|v| v.as_str()).map(|s| s.to_string()));

        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": order_id })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .or_else(|| data.head.status_description.as_deref())
            .unwrap_or("Order placement failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Modify an existing order
#[tauri::command]
pub async fn fivepaisa_modify_order(
    api_key: String,
    access_token: String,
    exchange_order_id: String,
    quantity: i32,
    price: f64,
    trigger_price: f64,
    disclosed_quantity: Option<i32>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ExchOrderID": exchange_order_id,
            "Price": price,
            "Qty": quantity,
            "StopLossPrice": trigger_price,
            "DisQty": disclosed_quantity.unwrap_or(0)
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/ModifyOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Modify order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse modify response: {}", e))?;

    let is_success = data.head.status.as_deref() == Some("0") ||
                     data.head.status_description.as_deref() == Some("Success");

    if is_success {
        let order_id = data.body.get("BrokerOrderID")
            .and_then(|v| v.as_str())
            .unwrap_or(&exchange_order_id);
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": order_id })),
            error: None,
        })
    } else {
        let error_msg = data.head.status_description.as_deref()
            .unwrap_or("Order modification failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Cancel an order
#[tauri::command]
pub async fn fivepaisa_cancel_order(
    api_key: String,
    access_token: String,
    exchange_order_id: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ExchOrderID": exchange_order_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/CancelOrderRequest", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Cancel order request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse cancel response: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(json!({ "order_id": exchange_order_id })),
            error: None,
        })
    } else {
        let error_msg = data.body.get("Message")
            .and_then(|v| v.as_str())
            .unwrap_or("Order cancellation failed");
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some(error_msg.to_string()),
        })
    }
}

/// Get order book
#[tauri::command]
pub async fn fivepaisa_get_orders(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V3/OrderBook", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Order book request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse order book: {}", e))?;

    let orders = data.body.get("OrderBookDetail")
        .cloned()
        .unwrap_or(serde_json::json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(orders),
        error: None,
    })
}

/// Get trade book
#[tauri::command]
pub async fn fivepaisa_get_trades(
    api_key: String,
    client_id: String,
    access_token: String,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": { "ClientCode": client_id }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/TradeBook", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Trade book request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse trade book: {}", e))?;

    let trades = data.body.get("TradeBookDetail")
        .cloned()
        .unwrap_or(serde_json::json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(trades),
        error: None,
    })
}
