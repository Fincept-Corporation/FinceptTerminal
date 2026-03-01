use serde_json::Value;

/// Exchange authorization code for tokens
#[tauri::command]
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
#[tauri::command]
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
