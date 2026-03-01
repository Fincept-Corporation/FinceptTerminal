use super::FyersAdapter;
use super::{DATA_FIELDS, INDEX_FIELDS, DEPTH_FIELDS};
use crate::websocket::types::*;
use futures_util::SinkExt;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::net::TcpStream;
use tokio::sync::RwLock;
use tokio_tungstenite::{MaybeTlsStream, WebSocketStream};
use futures_util::StreamExt;
use tokio_tungstenite::tungstenite::Message;

impl FyersAdapter {
    /// Parse binary message from HSM WebSocket
    pub(super) fn parse_binary_message(
        data: &[u8],
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        if data.len() < 3 {
            return;
        }

        let msg_type = data[2];

        match msg_type {
            1 => {
                // Authentication response
            }
            4 => {
                // Subscription acknowledgment
            }
            6 => {
                // Data feed message
                Self::parse_data_feed(data, topic_mappings, scrips_cache, symbol_mappings, callback);
            }
            13 => {
                // Master data (usually large message on connect)
            }
            _ => {}
        }
    }

    /// Parse data feed message (message type 6)
    pub(super) fn parse_data_feed(
        data: &[u8],
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        if data.len() < 9 {
            return;
        }

        // Get scrip count at bytes 7-8
        let scrip_count = u16::from_be_bytes([data[7], data[8]]) as usize;
        let mut offset = 9;

        for _ in 0..scrip_count {
            if offset >= data.len() {
                break;
            }

            // Get data type
            let data_type = data[offset];
            offset += 1;

            match data_type {
                83 => {
                    // Snapshot data (0x53 = 'S')
                    offset = Self::parse_snapshot_data(data, offset, topic_mappings, scrips_cache, symbol_mappings, callback);
                }
                85 => {
                    // Update data (0x55 = 'U')
                    offset = Self::parse_update_data(data, offset, topic_mappings, scrips_cache, symbol_mappings, callback);
                }
                _ => {
                    break;
                }
            }
        }
    }

    /// Parse snapshot data (data_type = 83)
    pub(super) fn parse_snapshot_data(
        data: &[u8],
        mut offset: usize,
        topic_mappings: &mut HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 3 > data.len() {
            return offset;
        }

        // Get topic ID (2 bytes, little-endian in Fyers)
        let topic_id = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;

        // Get topic name length
        let topic_name_len = data[offset] as usize;
        offset += 1;

        if offset + topic_name_len > data.len() {
            return offset;
        }

        // Get topic name (HSM token like "sf|nse_cm|2885")
        let topic_name = String::from_utf8_lossy(&data[offset..offset + topic_name_len]).to_string();
        offset += topic_name_len;

        // Store topic mapping
        topic_mappings.insert(topic_id, topic_name.clone());

        // Parse based on topic type
        if topic_name.starts_with("sf|") {
            offset = Self::parse_scrip_snapshot(data, offset, topic_id, &topic_name, scrips_cache, symbol_mappings, callback);
        } else if topic_name.starts_with("if|") {
            offset = Self::parse_index_snapshot(data, offset, topic_id, &topic_name, symbol_mappings, callback);
        } else if topic_name.starts_with("dp|") {
            offset = Self::parse_depth_snapshot(data, offset, topic_id, &topic_name, scrips_cache, symbol_mappings, callback);
        }

        offset
    }

    /// Parse scrip snapshot data
    pub(super) fn parse_scrip_snapshot(
        data: &[u8],
        mut offset: usize,
        topic_id: u16,
        topic_name: &str,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut scrip_data: HashMap<String, i32> = HashMap::new();

        // Parse field values (4 bytes each, big-endian)
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            // -2147483648 means null/no value
            if value != i32::MIN && index < DATA_FIELDS.len() {
                scrip_data.insert(DATA_FIELDS[index].to_string(), value);
            }
        }

        // Skip 2 bytes
        offset += 2;

        if offset + 3 > data.len() {
            return offset;
        }

        // Get multiplier and precision
        let multiplier = u16::from_be_bytes([data[offset], data[offset + 1]]);
        offset += 2;
        let _precision = data[offset];
        offset += 1;

        // Parse string fields: exchange, exchange_token, symbol
        let mut exchange = String::new();
        let mut _exchange_token = String::new();
        let mut symbol = String::new();

        for field_name in &["exchange", "exchange_token", "symbol"] {
            if offset + 1 > data.len() {
                break;
            }

            let string_len = data[offset] as usize;
            offset += 1;

            if offset + string_len > data.len() {
                break;
            }

            let string_value = String::from_utf8_lossy(&data[offset..offset + string_len]).to_string();
            offset += string_len;

            match *field_name {
                "exchange" => exchange = string_value,
                "exchange_token" => _exchange_token = string_value,
                "symbol" => symbol = string_value,
                _ => {}
            }
        }

        // Store in cache
        scrips_cache.insert(topic_id, scrip_data.clone());

        // Get original symbol from mapping or use parsed symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| format!("{}:{}", exchange, symbol));

        // Convert to TickerData and emit
        let divisor = if multiplier > 0 { multiplier as f64 * 100.0 } else { 100.0 };

        let tick = TickerData {
            provider: "fyers".to_string(),
            symbol: original_symbol.clone(),
            price: scrip_data.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
            bid: scrip_data.get("bid_price").map(|v| *v as f64 / divisor),
            ask: scrip_data.get("ask_price").map(|v| *v as f64 / divisor),
            bid_size: scrip_data.get("bid_size").map(|v| *v as f64),
            ask_size: scrip_data.get("ask_size").map(|v| *v as f64),
            volume: scrip_data.get("vol_traded_today").map(|v| *v as f64),
            high: scrip_data.get("high_price").map(|v| *v as f64 / divisor),
            low: scrip_data.get("low_price").map(|v| *v as f64 / divisor),
            open: scrip_data.get("open_price").map(|v| *v as f64 / divisor),
            close: scrip_data.get("prev_close_price").map(|v| *v as f64 / divisor),
            change: None,
            change_percent: None,
            quote_volume: None,
            timestamp: scrip_data.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
        };

        // Calculate change
        let tick = {
            let mut t = tick;
            if let (Some(_ltp), Some(close)) = (t.price.into(), t.close) {
                let change = t.price - close;
                let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                t.change = Some(change);
                t.change_percent = Some(change_pct);
            }
            t
        };

        // Emit to callback
        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::Ticker(tick));
            }
        }

        offset
    }

    /// Parse index snapshot data
    pub(super) fn parse_index_snapshot(
        data: &[u8],
        mut offset: usize,
        _topic_id: u16,
        topic_name: &str,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut index_data: HashMap<String, i32> = HashMap::new();

        // Parse field values
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < INDEX_FIELDS.len() {
                index_data.insert(INDEX_FIELDS[index].to_string(), value);
            }
        }

        // Get original symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| topic_name.to_string());

        // Index values are already in proper format (no division needed)
        let divisor = 100.0;

        let tick = TickerData {
            provider: "fyers".to_string(),
            symbol: original_symbol,
            price: index_data.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
            bid: None,
            ask: None,
            bid_size: None,
            ask_size: None,
            volume: None,
            high: index_data.get("high_price").map(|v| *v as f64 / divisor),
            low: index_data.get("low_price").map(|v| *v as f64 / divisor),
            open: index_data.get("open_price").map(|v| *v as f64 / divisor),
            close: index_data.get("prev_close_price").map(|v| *v as f64 / divisor),
            change: None,
            change_percent: None,
            quote_volume: None,
            timestamp: index_data.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
        };

        // Calculate change
        let tick = {
            let mut t = tick;
            if let Some(close) = t.close {
                let change = t.price - close;
                let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                t.change = Some(change);
                t.change_percent = Some(change_pct);
            }
            t
        };

        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::Ticker(tick));
            }
        }

        offset
    }

    /// Parse depth snapshot data
    pub(super) fn parse_depth_snapshot(
        data: &[u8],
        mut offset: usize,
        topic_id: u16,
        topic_name: &str,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 1 > data.len() {
            return offset;
        }

        let field_count = data[offset] as usize;
        offset += 1;

        let mut depth_data: HashMap<String, i32> = HashMap::new();

        // Parse field values
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < DEPTH_FIELDS.len() {
                depth_data.insert(DEPTH_FIELDS[index].to_string(), value);
            }
        }

        // Skip 2 bytes
        offset += 2;

        if offset + 3 > data.len() {
            return offset;
        }

        // Get multiplier and precision
        let multiplier = u16::from_be_bytes([data[offset], data[offset + 1]]);
        offset += 2;
        let _precision = data[offset];
        offset += 1;

        // Parse string fields
        let mut exchange = String::new();
        let mut symbol = String::new();

        for field_name in &["exchange", "exchange_token", "symbol"] {
            if offset + 1 > data.len() {
                break;
            }

            let string_len = data[offset] as usize;
            offset += 1;

            if offset + string_len > data.len() {
                break;
            }

            let string_value = String::from_utf8_lossy(&data[offset..offset + string_len]).to_string();
            offset += string_len;

            match *field_name {
                "exchange" => exchange = string_value,
                "symbol" => symbol = string_value,
                _ => {}
            }
        }

        // Store in cache
        scrips_cache.insert(topic_id, depth_data.clone());

        // Get original symbol
        let original_symbol = symbol_mappings.get(topic_name)
            .cloned()
            .unwrap_or_else(|| format!("{}:{}", exchange, symbol));

        let divisor = if multiplier > 0 { multiplier as f64 * 100.0 } else { 100.0 };

        // Build order book levels
        let mut bids = Vec::new();
        let mut asks = Vec::new();

        for i in 1..=5 {
            let bid_price_key = format!("bid_price{}", i);
            let bid_size_key = format!("bid_size{}", i);
            let bid_order_key = format!("bid_order{}", i);
            let ask_price_key = format!("ask_price{}", i);
            let ask_size_key = format!("ask_size{}", i);
            let ask_order_key = format!("ask_order{}", i);

            if let Some(&price) = depth_data.get(&bid_price_key) {
                let size = depth_data.get(&bid_size_key).copied().unwrap_or(0);
                let count = depth_data.get(&bid_order_key).map(|&c| c as u32);
                bids.push(OrderBookLevel {
                    price: price as f64 / divisor,
                    quantity: size as f64,
                    count,
                });
            }

            if let Some(&price) = depth_data.get(&ask_price_key) {
                let size = depth_data.get(&ask_size_key).copied().unwrap_or(0);
                let count = depth_data.get(&ask_order_key).map(|&c| c as u32);
                asks.push(OrderBookLevel {
                    price: price as f64 / divisor,
                    quantity: size as f64,
                    count,
                });
            }
        }

        let orderbook = OrderBookData {
            provider: "fyers".to_string(),
            symbol: original_symbol,
            bids,
            asks,
            timestamp: chrono::Utc::now().timestamp() as u64,
            is_snapshot: true,
        };

        if let Ok(cb_lock) = callback.read() {
            if let Some(cb) = cb_lock.as_ref() {
                cb(MarketMessage::OrderBook(orderbook));
            }
        }

        offset
    }

    /// Parse update data (data_type = 85) - incremental updates
    pub(super) fn parse_update_data(
        data: &[u8],
        mut offset: usize,
        topic_mappings: &HashMap<u16, String>,
        scrips_cache: &mut HashMap<u16, HashMap<String, i32>>,
        symbol_mappings: &HashMap<String, String>,
        callback: &Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) -> usize {
        if offset + 3 > data.len() {
            return offset;
        }

        // Get topic ID
        let topic_id = u16::from_le_bytes([data[offset], data[offset + 1]]);
        offset += 2;

        // Get field count
        let field_count = data[offset] as usize;
        offset += 1;

        // Get topic name from mapping
        let topic_name = match topic_mappings.get(&topic_id) {
            Some(name) => name.clone(),
            None => {
                // Skip unknown topic
                offset += field_count * 4;
                return offset;
            }
        };

        // Determine field mapping based on topic type
        let fields: &[&str] = if topic_name.starts_with("sf|") {
            DATA_FIELDS
        } else if topic_name.starts_with("if|") {
            INDEX_FIELDS
        } else if topic_name.starts_with("dp|") {
            DEPTH_FIELDS
        } else {
            offset += field_count * 4;
            return offset;
        };

        // Get or create cache entry
        let cache_entry = scrips_cache.entry(topic_id).or_insert_with(HashMap::new);

        // Update fields
        let mut updated = false;
        for index in 0..field_count {
            if offset + 4 > data.len() {
                break;
            }

            let value = i32::from_be_bytes([data[offset], data[offset + 1], data[offset + 2], data[offset + 3]]);
            offset += 4;

            if value != i32::MIN && index < fields.len() {
                let old_value = cache_entry.get(fields[index]).copied();
                if old_value != Some(value) {
                    cache_entry.insert(fields[index].to_string(), value);
                    updated = true;
                }
            }
        }

        // Emit update if changed
        if updated {
            let original_symbol = symbol_mappings.get(&topic_name)
                .cloned()
                .unwrap_or_else(|| topic_name.clone());

            if topic_name.starts_with("sf|") || topic_name.starts_with("if|") {
                let divisor = 100.0; // Default divisor

                let tick = TickerData {
                    provider: "fyers".to_string(),
                    symbol: original_symbol,
                    price: cache_entry.get("ltp").map(|v| *v as f64 / divisor).unwrap_or(0.0),
                    bid: cache_entry.get("bid_price").map(|v| *v as f64 / divisor),
                    ask: cache_entry.get("ask_price").map(|v| *v as f64 / divisor),
                    bid_size: cache_entry.get("bid_size").map(|v| *v as f64),
                    ask_size: cache_entry.get("ask_size").map(|v| *v as f64),
                    volume: cache_entry.get("vol_traded_today").map(|v| *v as f64),
                    high: cache_entry.get("high_price").map(|v| *v as f64 / divisor),
                    low: cache_entry.get("low_price").map(|v| *v as f64 / divisor),
                    open: cache_entry.get("open_price").map(|v| *v as f64 / divisor),
                    close: cache_entry.get("prev_close_price").map(|v| *v as f64 / divisor),
                    change: None,
                    change_percent: None,
                    quote_volume: None,
                    timestamp: cache_entry.get("exch_feed_time").map(|v| *v as u64).unwrap_or_else(|| chrono::Utc::now().timestamp() as u64),
                };

                // Calculate change
                let tick = {
                    let mut t = tick;
                    if let Some(close) = t.close {
                        let change = t.price - close;
                        let change_pct = if close > 0.0 { (change / close) * 100.0 } else { 0.0 };
                        t.change = Some(change);
                        t.change_percent = Some(change_pct);
                    }
                    t
                };

                if let Ok(cb_lock) = callback.read() {
                    if let Some(cb) = cb_lock.as_ref() {
                        cb(MarketMessage::Ticker(tick));
                    }
                }
            } else if topic_name.starts_with("dp|") {
                // Emit depth update
                let divisor = 100.0;
                let mut bids = Vec::new();
                let mut asks = Vec::new();

                for i in 1..=5 {
                    if let Some(&price) = cache_entry.get(&format!("bid_price{}", i)) {
                        let size = cache_entry.get(&format!("bid_size{}", i)).copied().unwrap_or(0);
                        let count = cache_entry.get(&format!("bid_order{}", i)).map(|&c| c as u32);
                        bids.push(OrderBookLevel {
                            price: price as f64 / divisor,
                            quantity: size as f64,
                            count,
                        });
                    }
                    if let Some(&price) = cache_entry.get(&format!("ask_price{}", i)) {
                        let size = cache_entry.get(&format!("ask_size{}", i)).copied().unwrap_or(0);
                        let count = cache_entry.get(&format!("ask_order{}", i)).map(|&c| c as u32);
                        asks.push(OrderBookLevel {
                            price: price as f64 / divisor,
                            quantity: size as f64,
                            count,
                        });
                    }
                }

                let orderbook = OrderBookData {
                    provider: "fyers".to_string(),
                    symbol: original_symbol,
                    bids,
                    asks,
                    timestamp: chrono::Utc::now().timestamp() as u64,
                    is_snapshot: false,
                };

                if let Ok(cb_lock) = callback.read() {
                    if let Some(cb) = cb_lock.as_ref() {
                        cb(MarketMessage::OrderBook(orderbook));
                    }
                }
            }
        }

        offset
    }

    /// Start message processing loop
    pub(super) async fn start_message_loop(
        ws_stream: Arc<RwLock<Option<WebSocketStream<MaybeTlsStream<TcpStream>>>>>,
        is_connected: Arc<RwLock<bool>>,
        is_authenticated: Arc<RwLock<bool>>,
        topic_mappings: Arc<RwLock<HashMap<u16, String>>>,
        scrips_cache: Arc<RwLock<HashMap<u16, HashMap<String, i32>>>>,
        symbol_mappings: Arc<RwLock<HashMap<String, String>>>,
        callback: Arc<std::sync::RwLock<Option<Box<dyn Fn(MarketMessage) + Send + Sync>>>>,
    ) {
        let mut msg_count: u64 = 0;

        loop {
            let msg = {
                let mut stream_lock = ws_stream.write().await;
                if let Some(stream) = stream_lock.as_mut() {
                    stream.next().await
                } else {
                    break;
                }
            };

            msg_count += 1;

            match msg {
                Some(Ok(Message::Binary(data))) => {
                    // HSM protocol uses binary messages
                    let mut topics = topic_mappings.write().await;
                    let mut cache = scrips_cache.write().await;
                    let symbols = symbol_mappings.read().await;

                    Self::parse_binary_message(
                        &data,
                        &mut topics,
                        &mut cache,
                        &symbols,
                        &callback,
                    );

                    // Check if authenticated after first message
                    if msg_count == 1 && data.len() >= 3 && data[2] == 1 {
                        let mut auth = is_authenticated.write().await;
                        *auth = true;
                    }
                }
                Some(Ok(Message::Text(_text))) => {
                    // Unexpected text message
                }
                Some(Ok(Message::Ping(data))) => {
                    let mut stream_lock = ws_stream.write().await;
                    if let Some(stream) = stream_lock.as_mut() {
                        let _ = stream.send(Message::Pong(data)).await;
                    }
                }
                Some(Ok(Message::Pong(_))) => {
                    // Pong received, connection is alive
                }
                Some(Ok(Message::Close(_frame))) => {
                    break;
                }
                Some(Ok(Message::Frame(_))) => {
                    // Raw frame, ignore
                }
                Some(Err(_e)) => {
                    break;
                }
                None => {
                    break;
                }
            }
        }

        // Mark as disconnected
        let mut connected = is_connected.write().await;
        *connected = false;
        let mut auth = is_authenticated.write().await;
        *auth = false;
    }
}
