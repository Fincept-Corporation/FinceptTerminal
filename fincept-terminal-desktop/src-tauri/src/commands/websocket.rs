// WebSocket Tauri commands — thin wrappers over WebSocketManager and MessageRouter.
// State types live in crate root (lib.rs) to keep the Tauri builder simple.

use crate::WebSocketState;

// ============================================================================
// CONFIGURATION
// ============================================================================

#[tauri::command]
pub async fn ws_set_config(
    state: tauri::State<'_, WebSocketState>,
    config: crate::websocket::types::ProviderConfig,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.set_config(config);
    Ok(())
}

// ============================================================================
// CONNECTION
// ============================================================================

#[tauri::command]
pub async fn ws_connect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.connect(&provider).await.map_err(|e| {
        eprintln!("[ws_connect] Failed to connect to {}: {}", provider, e);
        e.to_string()
    })
}

/// Disconnect from a provider and clear all stale router subscriber entries.
#[tauri::command]
pub async fn ws_disconnect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    state.router.write().await.clear_provider_subscribers(&provider);
    let manager = state.manager.read().await;
    manager.disconnect(&provider).await.map_err(|e| e.to_string())
}

#[tauri::command]
pub async fn ws_reconnect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.reconnect(&provider).await.map_err(|e| e.to_string())
}

// ============================================================================
// SUBSCRIPTIONS
// ============================================================================

#[tauri::command]
pub async fn ws_subscribe(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
    symbol: String,
    channel: String,
    params: Option<serde_json::Value>,
) -> Result<(), String> {
    let topic = format!("{}.{}.{}", provider, channel, symbol);
    state.router.write().await.subscribe_frontend(&topic);

    let manager = state.manager.read().await;
    manager
        .subscribe(&provider, &symbol, &channel, params)
        .await
        .map_err(|e| {
            eprintln!(
                "[ws_subscribe] Failed to subscribe to {} {} {}: {}",
                provider, symbol, channel, e
            );
            e.to_string()
        })
}

#[tauri::command]
pub async fn ws_unsubscribe(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
    symbol: String,
    channel: String,
) -> Result<(), String> {
    state
        .router
        .write()
        .await
        .unsubscribe_frontend(&format!("{}.{}.{}", provider, channel, symbol));

    let manager = state.manager.read().await;
    manager
        .unsubscribe(&provider, &symbol, &channel)
        .await
        .map_err(|e| e.to_string())
}

/// Bulk unsubscribe all tracked symbols for a provider + channel using the
/// backend's own subscription registry — nothing gets missed.
#[tauri::command]
pub async fn ws_unsubscribe_all(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
    channel: String,
) -> Result<Vec<String>, String> {
    let manager = state.manager.read().await;
    let unsubscribed = manager.unsubscribe_all(&provider, &channel).await;

    {
        let router = state.router.write().await;
        for sym in &unsubscribed {
            router.unsubscribe_frontend(&format!("{}.{}.{}", provider, channel, sym));
        }
    }

    Ok(unsubscribed)
}

// ============================================================================
// METRICS
// ============================================================================

#[tauri::command]
pub async fn ws_get_metrics(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<Option<crate::websocket::types::ConnectionMetrics>, String> {
    let manager = state.manager.read().await;
    Ok(manager.get_metrics(&provider))
}

#[tauri::command]
pub async fn ws_get_all_metrics(
    state: tauri::State<'_, WebSocketState>,
) -> Result<Vec<crate::websocket::types::ConnectionMetrics>, String> {
    let manager = state.manager.read().await;
    Ok(manager.get_all_metrics())
}
