use super::{ALICEBLUE_API_BASE, create_aliceblue_headers};
use super::super::common::ApiResponse;
use serde_json::{json, Value};

/// Get positions
#[tauri::command]
pub async fn aliceblue_get_positions(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_positions] Fetching positions");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let payload = json!({
        "ret": "NET"
    });

    let response = client
        .post(format!("{}/positionAndHoldings/positionBook", ALICEBLUE_API_BASE))
        .headers(headers)
        .json(&payload)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(positions) = body.as_array() {
        if let Some(first) = positions.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(positions.clone()),
            error: None,
            timestamp,
        })
    } else if body.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}

/// Get holdings
#[tauri::command]
pub async fn aliceblue_get_holdings(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Vec<Value>>, String> {
    eprintln!("[aliceblue_get_holdings] Fetching holdings");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/positionAndHoldings/holdings", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(holdings) = body.as_array() {
        if let Some(first) = holdings.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: true,
                    data: Some(vec![]),
                    error: None,
                    timestamp,
                });
            }
        }

        Ok(ApiResponse {
            success: true,
            data: Some(holdings.clone()),
            error: None,
            timestamp,
        })
    } else {
        Ok(ApiResponse {
            success: true,
            data: Some(vec![]),
            error: None,
            timestamp,
        })
    }
}

/// Get margin/funds data
#[tauri::command]
pub async fn aliceblue_get_margins(
    api_secret: String,
    session_id: String,
) -> Result<ApiResponse<Value>, String> {
    eprintln!("[aliceblue_get_margins] Fetching margin data");

    let client = reqwest::Client::new();
    let headers = create_aliceblue_headers(&api_secret, &session_id);

    let response = client
        .get(format!("{}/limits/getRmsLimits", ALICEBLUE_API_BASE))
        .headers(headers)
        .send()
        .await
        .map_err(|e| format!("Request failed: {}", e))?;

    let body: Value = response.json().await.map_err(|e| format!("Failed to parse response: {}", e))?;
    let timestamp = chrono::Utc::now().timestamp_millis();

    if let Some(margins) = body.as_array() {
        if let Some(first) = margins.get(0) {
            if first.get("stat").and_then(|s| s.as_str()) == Some("Not_Ok") {
                return Ok(ApiResponse {
                    success: false,
                    data: None,
                    error: first.get("emsg").and_then(|m| m.as_str()).map(String::from),
                    timestamp,
                });
            }

            // Transform to standard format
            let available_cash = first.get("net")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let collateral = first.get("collateralvalue")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let margin_used = first.get("cncMarginUsed")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let unrealized_mtm = first.get("unrealizedMtomPrsnt")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);
            let realized_mtm = first.get("realizedMtomPrsnt")
                .and_then(|v| v.as_str())
                .and_then(|s| s.parse::<f64>().ok())
                .unwrap_or(0.0);

            let formatted_margins = json!({
                "availablecash": format!("{:.2}", available_cash),
                "collateral": format!("{:.2}", collateral),
                "utiliseddebits": format!("{:.2}", margin_used),
                "m2munrealized": format!("{:.2}", unrealized_mtm),
                "m2mrealized": format!("{:.2}", realized_mtm),
                "raw": first,
            });

            return Ok(ApiResponse {
                success: true,
                data: Some(formatted_margins),
                error: None,
                timestamp,
            });
        }
    }

    Ok(ApiResponse {
        success: false,
        data: None,
        error: Some("Failed to fetch margins".to_string()),
        timestamp,
    })
}
