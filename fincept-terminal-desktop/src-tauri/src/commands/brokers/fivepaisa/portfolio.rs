use super::{FIVEPAISA_BASE_URL, FivePaisaResponse, FivePaisaApiResponse, create_client};
use serde_json::json;

/// Get positions
#[tauri::command]
pub async fn fivepaisa_get_positions(
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
        .post(format!("{}/VendorsAPI/Service1.svc/V2/NetPositionNetWise", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .timeout(std::time::Duration::from_secs(60))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Positions request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse positions: {}", e))?;

    let positions = data.body.get("NetPositionDetail")
        .cloned()
        .unwrap_or(serde_json::json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(positions),
        error: None,
    })
}

/// Get holdings
#[tauri::command]
pub async fn fivepaisa_get_holdings(
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
        .post(format!("{}/VendorsAPI/Service1.svc/V3/Holding", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Holdings request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse holdings: {}", e))?;

    let holdings = data.body.get("Data")
        .cloned()
        .unwrap_or(serde_json::json!([]));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(holdings),
        error: None,
    })
}

/// Get margin/funds
#[tauri::command]
pub async fn fivepaisa_get_margins(
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
        .post(format!("{}/VendorsAPI/Service1.svc/V4/Margin", FIVEPAISA_BASE_URL))
        .header("Content-Type", "application/json")
        .header("Authorization", format!("bearer {}", access_token))
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Margin request failed: {}", e))?;

    let data: FivePaisaApiResponse = response
        .json()
        .await
        .map_err(|e| format!("Failed to parse margins: {}", e))?;

    // Extract equity margin from array
    let equity_margin = data.body.get("EquityMargin")
        .and_then(|v| v.as_array())
        .and_then(|arr| arr.first())
        .cloned()
        .unwrap_or(serde_json::json!({}));

    Ok(FivePaisaResponse {
        success: true,
        data: Some(equity_margin),
        error: None,
    })
}
