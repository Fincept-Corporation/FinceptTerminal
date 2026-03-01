use super::{FIVEPAISA_BASE_URL, FivePaisaResponse, FivePaisaApiResponse, create_client, map_exchange, map_exchange_type};
use serde_json::json;

/// Get market depth/quote
#[tauri::command]
pub async fn fivepaisa_get_quote(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    scrip_code: i64,
    scrip_data: Option<String>,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "Exchange": map_exchange(&exchange),
            "ExchangeType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "ScripData": scrip_data.unwrap_or_default()
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V2/MarketDepth", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Quote request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse quote: {}", e))?;

    if data.head.status_description.as_deref() == Some("Success") {
        Ok(FivePaisaResponse {
            success: true,
            data: Some(data.body),
            error: None,
        })
    } else {
        Ok(FivePaisaResponse {
            success: false,
            data: None,
            error: Some("Failed to fetch quote".to_string()),
        })
    }
}

/// Get historical data (OHLCV)
#[tauri::command]
pub async fn fivepaisa_get_historical(
    api_key: String,
    client_id: String,
    access_token: String,
    exchange: String,
    scrip_code: i64,
    resolution: String,
    from_timestamp: i64,
    to_timestamp: i64,
) -> Result<FivePaisaResponse, String> {
    let client = create_client()?;

    // Convert timestamps to date strings
    let from_date = chrono::DateTime::from_timestamp(from_timestamp, 0)
        .map(|dt| dt.format("%Y-%m-%d").to_string())
        .unwrap_or_default();
    let to_date = chrono::DateTime::from_timestamp(to_timestamp, 0)
        .map(|dt| dt.format("%Y-%m-%d").to_string())
        .unwrap_or_default();

    let payload = json!({
        "head": { "key": api_key },
        "body": {
            "ClientCode": client_id,
            "Exch": map_exchange(&exchange),
            "ExchType": map_exchange_type(&exchange),
            "ScripCode": scrip_code,
            "Interval": resolution,
            "FromDate": from_date,
            "ToDate": to_date
        }
    });

    let response = client
        .post(format!("{}/VendorsAPI/Service1.svc/V1/HistoricalData", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Historical data request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse historical data: {}", e))?;

    let candles = data.body.get("Data")
        .cloned()
        .unwrap_or(serde_json::json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(candles),
        error: None,
    })
}
