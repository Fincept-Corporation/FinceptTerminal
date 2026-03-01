// WebSocket Commands for All Indian Stock Brokers
//
// Provides unified Tauri commands for WebSocket operations across all brokers.
// Each broker has: {broker}_ws_connect, {broker}_ws_disconnect, {broker}_ws_subscribe, {broker}_ws_unsubscribe

pub mod upstox;
pub mod dhan;
pub mod angelone;
pub mod kotak;
pub mod groww;
pub mod aliceblue;
pub mod fivepaisa;
pub mod iifl;
pub mod motilal;
pub mod shoonya;

pub use upstox::*;
pub use dhan::*;
pub use angelone::*;
pub use kotak::*;
pub use groww::*;
pub use aliceblue::*;
pub use fivepaisa::*;
pub use iifl::*;
pub use motilal::*;
pub use shoonya::*;

use crate::websocket::router::MessageRouter;
use crate::websocket::types::MarketMessage;
use std::sync::Arc;
use tauri::Emitter;

/// Creates WebSocket event callback for a broker.
/// Emits broker-specific events to frontend AND routes through the shared MessageRouter
/// so backend services (monitoring, paper trading, etc.) also receive the ticks.
pub(super) fn create_ws_callback(
    app: tauri::AppHandle,
    broker: &'static str,
    router: Option<Arc<tokio::sync::RwLock<MessageRouter>>>,
) -> Box<dyn Fn(MarketMessage) + Send + Sync> {
    Box::new(move |msg: MarketMessage| {
        // 1. Emit broker-specific events to frontend
        match &msg {
            MarketMessage::Ticker(data) => {
                let _ = app.emit(&format!("{}_ticker", broker), data);
                // Also emit a unified event so algo monitoring can listen to a single channel
                let _ = app.emit("algo_live_ticker", data);
            }
            MarketMessage::OrderBook(data) => {
                let _ = app.emit(&format!("{}_orderbook", broker), data);
            }
            MarketMessage::Trade(data) => {
                let _ = app.emit(&format!("{}_trade", broker), data);
            }
            MarketMessage::Status(data) => {
                let _ = app.emit(&format!("{}_status", broker), data);
            }
            MarketMessage::Candle(data) => {
                let _ = app.emit(&format!("{}_candle", broker), data);
            }
        }

        // 2. Route through MessageRouter for backend services (monitoring, paper trading, etc.)
        if let Some(ref router) = router {
            let router = router.clone();
            let msg = msg.clone();
            tokio::spawn(async move {
                router.read().await.route(msg).await;
            });
        }
    })
}
