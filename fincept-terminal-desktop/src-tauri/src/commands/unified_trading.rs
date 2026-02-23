// Unified Trading Module
// Handles both LIVE and PAPER trading modes for all brokers
// Routes orders to either real broker API or paper trading engine

use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::sync::RwLock;
use once_cell::sync::Lazy;

use crate::paper_trading;

// ============================================================================
// Types
// ============================================================================

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum TradingMode {
    Live,
    Paper,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TradingSession {
    pub broker: String,
    pub mode: TradingMode,
    pub paper_portfolio_id: Option<String>,
    pub is_connected: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnifiedOrder {
    pub symbol: String,
    pub exchange: String,
    pub side: String,           // "buy" | "sell"
    pub order_type: String,     // "market" | "limit" | "stop" | "stop_limit"
    pub quantity: f64,
    pub price: Option<f64>,
    pub stop_price: Option<f64>,
    pub product_type: Option<String>, // "CNC" | "MIS" | "NRML"
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnifiedOrderResponse {
    pub success: bool,
    pub order_id: Option<String>,
    pub message: Option<String>,
    pub mode: TradingMode,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct UnifiedPosition {
    pub symbol: String,
    pub exchange: String,
    pub side: String,
    pub quantity: f64,
    pub entry_price: f64,
    pub current_price: f64,
    pub unrealized_pnl: f64,
    pub realized_pnl: f64,
    pub mode: TradingMode,
}

// ============================================================================
// Global State
// ============================================================================

static TRADING_SESSION: Lazy<Arc<RwLock<Option<TradingSession>>>> =
    Lazy::new(|| Arc::new(RwLock::new(None)));

// ============================================================================
// Session Management
// ============================================================================

/// Initialize trading session
#[tauri::command]
pub async fn init_trading_session(
    broker: String,
    mode: String,
    paper_portfolio_id: Option<String>,
) -> Result<TradingSession, String> {
    let trading_mode = match mode.to_lowercase().as_str() {
        "live" => TradingMode::Live,
        "paper" | "sandbox" => TradingMode::Paper,
        _ => return Err("Invalid mode. Use 'live' or 'paper'".to_string()),
    };

    // For paper mode, ensure portfolio exists or create one
    let portfolio_id = if trading_mode == TradingMode::Paper {
        match paper_portfolio_id {
            Some(id) => {
                // Verify portfolio exists
                paper_trading::get_portfolio(&id).map_err(|e| e.to_string())?;
                Some(id)
            }
            None => {
                // Create default paper portfolio for this broker
                let portfolio = paper_trading::create_portfolio(
                    &format!("{} Paper Trading", broker),
                    1000000.0, // 10 lakh INR default
                    "INR",
                    1.0,
                    "cross",
                    0.0003, // 0.03% fee (typical for Zerodha)
                ).map_err(|e| e.to_string())?;
                Some(portfolio.id)
            }
        }
    } else {
        None
    };

    let session = TradingSession {
        broker: broker.clone(),
        mode: trading_mode,
        paper_portfolio_id: portfolio_id,
        is_connected: false,
    };

    let mut session_guard = TRADING_SESSION.write().await;
    *session_guard = Some(session.clone());

    Ok(session)
}

/// Get current trading session
#[tauri::command]
pub async fn get_trading_session() -> Result<Option<TradingSession>, String> {
    let session_guard = TRADING_SESSION.read().await;
    Ok(session_guard.clone())
}

/// Switch trading mode
#[tauri::command]
pub async fn switch_trading_mode(mode: String) -> Result<TradingSession, String> {
    let trading_mode = match mode.to_lowercase().as_str() {
        "live" => TradingMode::Live,
        "paper" | "sandbox" => TradingMode::Paper,
        _ => return Err("Invalid mode".to_string()),
    };

    let mut session_guard = TRADING_SESSION.write().await;

    if let Some(ref mut session) = *session_guard {
        session.mode = trading_mode;

        // Create paper portfolio if switching to paper and none exists
        if trading_mode == TradingMode::Paper && session.paper_portfolio_id.is_none() {
            let portfolio = paper_trading::create_portfolio(
                &format!("{} Paper Trading", session.broker),
                1000000.0,
                "INR",
                1.0,
                "cross",
                0.0003,
            ).map_err(|e| e.to_string())?;
            session.paper_portfolio_id = Some(portfolio.id);
        }

        Ok(session.clone())
    } else {
        Err("No active trading session".to_string())
    }
}

// ============================================================================
// Order Placement
// ============================================================================

/// Place order - routes to live broker or paper trading based on mode
#[tauri::command]
pub async fn unified_place_order(
    order: UnifiedOrder,
) -> Result<UnifiedOrderResponse, String> {
    let session_guard = TRADING_SESSION.read().await;

    let session = session_guard.as_ref()
        .ok_or("No active trading session. Call init_trading_session first.")?;

    match session.mode {
        TradingMode::Paper => {
            place_paper_order(&session, &order).await
        }
        TradingMode::Live => {
            place_live_order(&session, &order).await
        }
    }
}

/// Place order in paper trading
async fn place_paper_order(
    session: &TradingSession,
    order: &UnifiedOrder,
) -> Result<UnifiedOrderResponse, String> {
    let portfolio_id = session.paper_portfolio_id.as_ref()
        .ok_or("No paper portfolio configured")?;

    let symbol = format!("{}:{}", order.exchange, order.symbol);

    // For market orders without price, use a default price estimate (will be filled at market)
    let order_price = if order.order_type == "market" && order.price.is_none() {
        // Use a default placeholder price - order will be filled at actual market price
        Some(1000.0) // Default estimate for margin calculation
    } else {
        order.price
    };

    let paper_order = paper_trading::place_order(
        portfolio_id,
        &symbol,
        &order.side,
        &order.order_type,
        order.quantity,
        order_price,
        order.stop_price,
        false, // reduce_only
    ).map_err(|e| e.to_string())?;

    // For market orders, auto-fill immediately with estimated price
    if order.order_type == "market" {
        let fill_price = order.price.unwrap_or(order_price.unwrap_or(1000.0));
        let _ = paper_trading::fill_order(&paper_order.id, fill_price, None);
    }

    Ok(UnifiedOrderResponse {
        success: true,
        order_id: Some(paper_order.id),
        message: Some("Paper order placed".to_string()),
        mode: TradingMode::Paper,
    })
}

/// Place order with live broker
async fn place_live_order(
    session: &TradingSession,
    order: &UnifiedOrder,
) -> Result<UnifiedOrderResponse, String> {
    // Route to appropriate broker
    match session.broker.to_lowercase().as_str() {
        "zerodha" => {
            place_zerodha_order(order).await
        }
        "fyers" => {
            place_fyers_order(order).await
        }
        _ => Err(format!("Live trading not implemented for {}", session.broker))
    }
}

/// Place order via Zerodha API
async fn place_zerodha_order(order: &UnifiedOrder) -> Result<UnifiedOrderResponse, String> {
    use crate::commands::brokers;
    use std::collections::HashMap;
    use serde_json::Value;

    // Get stored credentials
    let creds = brokers::get_indian_broker_credentials("zerodha".to_string())
        .await
        .map_err(|e| e.to_string())?;

    if !creds.success || creds.data.is_none() {
        return Err("Zerodha credentials not found. Please authenticate first.".to_string());
    }

    let cred_data = creds.data.ok_or("Zerodha credentials data missing")?;
    let api_key = cred_data.get("apiKey")
        .and_then(|v| v.as_str())
        .ok_or("API key not found")?
        .to_string();
    let access_token = cred_data.get("accessToken")
        .and_then(|v| v.as_str())
        .ok_or("Access token not found. Please re-authenticate.")?
        .to_string();

    // Map order type
    let zerodha_order_type = match order.order_type.as_str() {
        "market" => "MARKET",
        "limit" => "LIMIT",
        "stop" => "SL-M",
        "stop_limit" => "SL",
        _ => "MARKET",
    };

    // Map transaction type
    let transaction_type = order.side.to_uppercase();

    // Map product type
    let product = order.product_type.as_deref().unwrap_or("CNC");

    // Build params HashMap
    let mut params: HashMap<String, Value> = HashMap::new();
    params.insert("tradingsymbol".to_string(), Value::String(order.symbol.clone()));
    params.insert("exchange".to_string(), Value::String(order.exchange.clone()));
    params.insert("transaction_type".to_string(), Value::String(transaction_type));
    params.insert("order_type".to_string(), Value::String(zerodha_order_type.to_string()));
    params.insert("product".to_string(), Value::String(product.to_string()));
    let qty_i64 = if order.quantity < 0.0 || order.quantity > i64::MAX as f64 || !order.quantity.is_finite() {
        return Err("Invalid order quantity".to_string());
    } else {
        order.quantity as i64
    };
    params.insert("quantity".to_string(), Value::Number(serde_json::Number::from(qty_i64)));

    if let Some(price) = order.price {
        params.insert("price".to_string(), Value::Number(serde_json::Number::from_f64(price).unwrap_or(serde_json::Number::from(0))));
    }
    if let Some(trigger_price) = order.stop_price {
        params.insert("trigger_price".to_string(), Value::Number(serde_json::Number::from_f64(trigger_price).unwrap_or(serde_json::Number::from(0))));
    }

    // Determine variety
    let variety = "regular".to_string();

    // Place order via Zerodha API
    let response = brokers::zerodha_place_order(
        api_key,
        access_token,
        params,
        variety,
    ).await.map_err(|e| e.to_string())?;

    Ok(UnifiedOrderResponse {
        success: response.success,
        order_id: response.order_id,
        message: response.error,
        mode: TradingMode::Live,
    })
}

/// Place order via Fyers API
async fn place_fyers_order(order: &UnifiedOrder) -> Result<UnifiedOrderResponse, String> {
    use crate::commands::brokers;

    // Get stored credentials
    let creds = brokers::get_indian_broker_credentials("fyers".to_string())
        .await
        .map_err(|e| e.to_string())?;

    if !creds.success || creds.data.is_none() {
        return Err("Fyers credentials not found. Please authenticate first.".to_string());
    }

    let cred_data = creds.data.ok_or("Fyers credentials data missing")?;
    let api_key = cred_data.get("apiKey")
        .and_then(|v| v.as_str())
        .ok_or("Fyers API key not found")?
        .to_string();
    let access_token = cred_data.get("accessToken")
        .and_then(|v| v.as_str())
        .ok_or("Fyers access token not found. Please re-authenticate.")?
        .to_string();

    // Map order type: UnifiedOrder uses "market"/"limit"/"stop"/"stop_limit"
    // Fyers uses: 1=Limit, 2=Market, 3=Stop Loss Limit, 4=Stop Loss Market
    let fyers_order_type: i32 = match order.order_type.as_str() {
        "market" => 2,
        "limit" => 1,
        "stop" => 4,       // Stop Loss Market
        "stop_limit" => 3, // Stop Loss Limit
        _ => 2,            // Default to market
    };

    // Map side: UnifiedOrder uses "buy"/"sell", Fyers uses 1=Buy, -1=Sell
    let fyers_side: i32 = match order.side.to_lowercase().as_str() {
        "buy" => 1,
        "sell" => -1,
        _ => 1,
    };

    // Map product type: default to INTRADAY for algo trading
    let product_type = order.product_type.as_deref().unwrap_or("INTRADAY").to_string();

    // Build Fyers symbol format: "NSE:SBIN-EQ" or use as-is if already formatted
    let fyers_symbol = if order.symbol.contains(':') {
        order.symbol.clone()
    } else {
        format!("{}:{}-EQ", order.exchange, order.symbol)
    };

    // Place order via Fyers API
    let response = brokers::fyers_place_order(
        api_key,
        access_token,
        fyers_symbol,
        {
            if order.quantity < 0.0 || order.quantity > i32::MAX as f64 || !order.quantity.is_finite() {
                return Err("Invalid order quantity for Fyers".to_string());
            }
            order.quantity as i32
        },
        fyers_order_type,
        fyers_side,
        product_type,
        order.price,       // limit_price
        order.stop_price,  // stop_price
        None,              // disclosed_qty
        Some("DAY".to_string()), // validity
        None,              // offline_order
        None,              // stop_loss
        None,              // take_profit
    ).await.map_err(|e| e.to_string())?;

    Ok(UnifiedOrderResponse {
        success: response.success,
        order_id: response.order_id,
        message: response.error,
        mode: TradingMode::Live,
    })
}

// ============================================================================
// Price Updates & Order Fills
// ============================================================================

/// Process tick for paper trading
/// Called when we receive price updates from WebSocket
#[tauri::command]
pub async fn process_tick_for_paper_trading(
    symbol: String,
    price: f64,
) -> Result<(), String> {
    let session_guard = TRADING_SESSION.read().await;

    let session = match session_guard.as_ref() {
        Some(s) if s.mode == TradingMode::Paper => s,
        _ => return Ok(()), // Not in paper mode, ignore
    };

    let portfolio_id = match &session.paper_portfolio_id {
        Some(id) => id,
        None => return Ok(()),
    };

    // 1. Update position prices
    let _ = paper_trading::update_position_price(portfolio_id, &symbol, price);

    // 2. Check and fill pending orders
    let orders = paper_trading::get_orders(portfolio_id, Some("pending"))
        .map_err(|e| e.to_string())?;

    for order in orders {
        if order.symbol != symbol {
            continue;
        }

        let should_fill = match order.order_type.as_str() {
            "market" => true,
            "limit" => {
                match order.side.as_str() {
                    "buy" => order.price.map(|p| price <= p).unwrap_or(false),
                    "sell" => order.price.map(|p| price >= p).unwrap_or(false),
                    _ => false,
                }
            }
            "stop" => {
                match order.side.as_str() {
                    "buy" => order.stop_price.map(|p| price >= p).unwrap_or(false),
                    "sell" => order.stop_price.map(|p| price <= p).unwrap_or(false),
                    _ => false,
                }
            }
            "stop_limit" => {
                // Stop triggered, now check limit
                let stop_triggered = match order.side.as_str() {
                    "buy" => order.stop_price.map(|p| price >= p).unwrap_or(false),
                    "sell" => order.stop_price.map(|p| price <= p).unwrap_or(false),
                    _ => false,
                };
                if stop_triggered {
                    match order.side.as_str() {
                        "buy" => order.price.map(|p| price <= p).unwrap_or(false),
                        "sell" => order.price.map(|p| price >= p).unwrap_or(false),
                        _ => false,
                    }
                } else {
                    false
                }
            }
            _ => false,
        };

        if should_fill {
            let _ = paper_trading::fill_order(&order.id, price, None);
        }
    }

    Ok(())
}

// ============================================================================
// Position & Order Queries
// ============================================================================

/// Get positions - returns paper or live based on mode
#[tauri::command]
pub async fn unified_get_positions() -> Result<Vec<UnifiedPosition>, String> {
    let session_guard = TRADING_SESSION.read().await;

    let session = session_guard.as_ref()
        .ok_or("No active trading session")?;

    match session.mode {
        TradingMode::Paper => {
            let portfolio_id = session.paper_portfolio_id.as_ref()
                .ok_or("No paper portfolio")?;

            let positions = paper_trading::get_positions(portfolio_id)
                .map_err(|e| e.to_string())?;

            Ok(positions.into_iter().map(|p| {
                let parts: Vec<&str> = p.symbol.splitn(2, ':').collect();
                UnifiedPosition {
                    symbol: parts.get(1).unwrap_or(&p.symbol.as_str()).to_string(),
                    exchange: parts.get(0).unwrap_or(&"NSE").to_string(),
                    side: p.side,
                    quantity: p.quantity,
                    entry_price: p.entry_price,
                    current_price: p.current_price,
                    unrealized_pnl: p.unrealized_pnl,
                    realized_pnl: p.realized_pnl,
                    mode: TradingMode::Paper,
                }
            }).collect())
        }
        TradingMode::Live => {
            // Get from broker
            get_live_positions(&session).await
        }
    }
}

async fn get_live_positions(session: &TradingSession) -> Result<Vec<UnifiedPosition>, String> {
    match session.broker.to_lowercase().as_str() {
        "zerodha" => {
            use crate::commands::brokers;

            let creds = brokers::get_indian_broker_credentials("zerodha".to_string())
                .await
                .map_err(|e| e.to_string())?;

            if !creds.success || creds.data.is_none() {
                return Err("Credentials not found".to_string());
            }

            let cred_data = creds.data.ok_or("Zerodha credentials data missing")?;
            let api_key = cred_data.get("apiKey")
                .and_then(|v| v.as_str())
                .ok_or("API key not found")?
                .to_string();
            let access_token = cred_data.get("accessToken")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            let response = brokers::zerodha_get_positions(
                api_key,
                access_token,
            ).await.map_err(|e| e.to_string())?;

            if !response.success {
                return Err(response.error.unwrap_or("Failed to get positions".to_string()));
            }

            // Map Zerodha positions to unified format
            let positions: Vec<UnifiedPosition> = response.data.map(|data| {
                if let Some(net_positions) = data.get("net").and_then(|v| v.as_array()) {
                    net_positions.iter().filter_map(|p| {
                        let qty = p.get("quantity").and_then(|v| v.as_f64()).unwrap_or(0.0);
                        let side = if qty >= 0.0 { "long" } else { "short" };
                        Some(UnifiedPosition {
                            symbol: p.get("tradingsymbol").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                            exchange: p.get("exchange").and_then(|v| v.as_str()).unwrap_or("NSE").to_string(),
                            side: side.to_string(),
                            quantity: qty.abs(),
                            entry_price: p.get("average_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                            current_price: p.get("last_price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                            unrealized_pnl: p.get("unrealised").and_then(|v| v.as_f64()).unwrap_or(0.0),
                            realized_pnl: p.get("realised").and_then(|v| v.as_f64()).unwrap_or(0.0),
                            mode: TradingMode::Live,
                        })
                    }).collect()
                } else {
                    Vec::new()
                }
            }).unwrap_or_default();

            Ok(positions)
        }
        "fyers" => {
            use crate::commands::brokers;

            let creds = brokers::get_indian_broker_credentials("fyers".to_string())
                .await
                .map_err(|e| e.to_string())?;

            if !creds.success || creds.data.is_none() {
                return Err("Fyers credentials not found".to_string());
            }

            let cred_data = creds.data.ok_or("Fyers credentials data missing")?;
            let api_key = cred_data.get("apiKey")
                .and_then(|v| v.as_str())
                .ok_or("API key not found")?
                .to_string();
            let access_token = cred_data.get("accessToken")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_string();

            let response = brokers::fyers_get_positions(
                api_key,
                access_token,
            ).await.map_err(|e| e.to_string())?;

            if !response.success {
                return Err(response.error.unwrap_or("Failed to get Fyers positions".to_string()));
            }

            // Map Fyers positions to unified format
            // Fyers netPositions array has: symbol, qty, avgPrice, ltp, pl, unrealizedProfit, etc.
            let positions: Vec<UnifiedPosition> = response.data.map(|data| {
                if let Some(positions_arr) = data.as_array() {
                    positions_arr.iter().filter_map(|p| {
                        let qty = p.get("netQty").or_else(|| p.get("qty"))
                            .and_then(|v| v.as_f64()).unwrap_or(0.0);
                        if qty == 0.0 { return None; }
                        let side = if qty > 0.0 { "long" } else { "short" };

                        // Fyers symbol format: "NSE:SBIN-EQ" -> extract parts
                        let full_symbol = p.get("symbol").and_then(|v| v.as_str()).unwrap_or("");
                        let parts: Vec<&str> = full_symbol.splitn(2, ':').collect();
                        let exchange = parts.first().unwrap_or(&"NSE").to_string();
                        let symbol = parts.get(1).unwrap_or(&full_symbol).to_string();

                        Some(UnifiedPosition {
                            symbol,
                            exchange,
                            side: side.to_string(),
                            quantity: qty.abs(),
                            entry_price: p.get("avgPrice").or_else(|| p.get("buyAvg"))
                                .and_then(|v| v.as_f64()).unwrap_or(0.0),
                            current_price: p.get("ltp").and_then(|v| v.as_f64()).unwrap_or(0.0),
                            unrealized_pnl: p.get("unrealizedProfit").or_else(|| p.get("pl"))
                                .and_then(|v| v.as_f64()).unwrap_or(0.0),
                            realized_pnl: p.get("realizedProfit")
                                .and_then(|v| v.as_f64()).unwrap_or(0.0),
                            mode: TradingMode::Live,
                        })
                    }).collect()
                } else {
                    Vec::new()
                }
            }).unwrap_or_default();

            Ok(positions)
        }
        _ => Err(format!("Not implemented for {}", session.broker))
    }
}

/// Get orders - returns paper or live based on mode
#[tauri::command]
pub async fn unified_get_orders(status: Option<String>) -> Result<Vec<paper_trading::Order>, String> {
    let session_guard = TRADING_SESSION.read().await;

    let session = session_guard.as_ref()
        .ok_or("No active trading session")?;

    match session.mode {
        TradingMode::Paper => {
            let portfolio_id = session.paper_portfolio_id.as_ref()
                .ok_or("No paper portfolio")?;

            paper_trading::get_orders(portfolio_id, status.as_deref())
                .map_err(|e| e.to_string())
        }
        TradingMode::Live => {
            // TODO: Get from broker
            Err("Live orders not yet implemented".to_string())
        }
    }
}

/// Cancel order
#[tauri::command]
pub async fn unified_cancel_order(order_id: String) -> Result<(), String> {
    let session_guard = TRADING_SESSION.read().await;

    let session = session_guard.as_ref()
        .ok_or("No active trading session")?;

    match session.mode {
        TradingMode::Paper => {
            paper_trading::cancel_order(&order_id)
                .map_err(|e| e.to_string())
        }
        TradingMode::Live => {
            // TODO: Cancel via broker
            Err("Live cancel not yet implemented".to_string())
        }
    }
}
