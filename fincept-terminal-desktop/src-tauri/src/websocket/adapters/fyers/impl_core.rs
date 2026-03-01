use super::{FyersAdapter, FyersDataType};
use super::{FYERS_SYMBOL_TOKEN_API, SOURCE_IDENTIFIER, MODE_PRODUCTION};
use crate::websocket::types::*;
use base64::{engine::general_purpose::URL_SAFE_NO_PAD, Engine};
use serde_json::Value;
use std::collections::HashMap;
use std::sync::Arc;
use tokio::sync::RwLock;

impl FyersAdapter {
    pub fn new(config: ProviderConfig) -> Self {
        let access_token = config.api_key.clone().unwrap_or_default();
        Self {
            config,
            access_token,
            hsm_key: None,
            ws_stream: Arc::new(RwLock::new(None)),
            message_callback: Arc::new(std::sync::RwLock::new(None)),
            subscriptions: Arc::new(RwLock::new(HashMap::new())),
            is_connected: Arc::new(RwLock::new(false)),
            is_authenticated: Arc::new(RwLock::new(false)),
            topic_mappings: Arc::new(RwLock::new(HashMap::new())),
            scrips_cache: Arc::new(RwLock::new(HashMap::new())),
            symbol_mappings: Arc::new(RwLock::new(HashMap::new())),
            fytoken_cache: Arc::new(RwLock::new(HashMap::new())),
        }
    }

    /// Fetch fytokens from Fyers API for symbol-to-token conversion
    /// This is critical for proper HSM subscription
    pub(super) async fn fetch_fytokens(&self, symbols: &[String]) -> HashMap<String, String> {
        let mut result = HashMap::new();

        if symbols.is_empty() {
            return result;
        }

        let client = reqwest::Client::new();
        let body = serde_json::json!({
            "symbols": symbols
        });

        match client
            .post(FYERS_SYMBOL_TOKEN_API)
            .header("Authorization", &self.access_token)
            .header("Content-Type", "application/json")
            .json(&body)
            .send()
            .await
        {
            Ok(response) => {
                if let Ok(json) = response.json::<Value>().await {
                    if json.get("s").and_then(|s| s.as_str()) == Some("ok") {
                        if let Some(valid_symbols) = json.get("validSymbol").and_then(|v| v.as_object()) {
                            for (symbol, fytoken) in valid_symbols {
                                if let Some(fytoken_str) = fytoken.as_str() {
                                    result.insert(symbol.clone(), fytoken_str.to_string());
                                }
                            }
                        }
                    }
                }
            }
            Err(_e) => {}
        }

        result
    }

    /// Convert fytoken to HSM token format
    /// fytoken format: "10100000002885" (14 digits)
    /// - First 4 digits: exchange segment (1010 = NSE CM)
    /// - Last 10 digits: token suffix for HSM
    pub(super) fn fytoken_to_hsm_token(fytoken: &str, data_type: &FyersDataType, is_index: bool) -> Option<String> {
        if fytoken.len() < 10 {
            return None;
        }

        let ex_sg = &fytoken[..4];
        let segment = match ex_sg {
            "1010" => "nse_cm",    // NSE Cash
            "1011" => "nse_fo",    // NSE F&O
            "1120" => "mcx_fo",    // MCX F&O
            "1210" => "bse_cm",    // BSE Cash
            "1211" => "bse_fo",    // BSE F&O
            "1212" => "bcs_fo",    // BSE Currency
            "1012" => "cde_fo",    // CDE F&O
            "1020" => "nse_com",   // NSE Commodity
            _ => {
                return None;
            }
        };

        // For indices, always use "if" prefix regardless of data type
        if is_index {
            let token_suffix = &fytoken[10..];
            return Some(format!("if|{}|{}", segment, token_suffix));
        }

        let prefix = match data_type {
            FyersDataType::SymbolUpdate => "sf",
            FyersDataType::DepthUpdate => "dp",
            FyersDataType::IndexUpdate => "if",
        };

        // Extract token suffix (last part after first 10 chars, or from position 10)
        let token_suffix = &fytoken[10..];

        Some(format!("{}|{}|{}", prefix, segment, token_suffix))
    }

    /// Extract HSM key from JWT access token
    pub(super) fn extract_hsm_key(access_token: &str) -> Option<String> {
        // Remove app_id prefix if present (format: "appid:token")
        let token = if access_token.contains(':') {
            access_token.split(':').nth(1)?
        } else {
            access_token
        };

        // JWT format: header.payload.signature
        let parts: Vec<&str> = token.split('.').collect();
        if parts.len() != 3 {
            return None;
        }

        // Decode payload (second part)
        let payload_b64 = parts[1];

        // Base64 URL-safe decode
        let decoded = URL_SAFE_NO_PAD.decode(payload_b64).ok()?;
        let payload_str = String::from_utf8(decoded).ok()?;

        // Parse JSON
        let payload: Value = serde_json::from_str(&payload_str).ok()?;

        // Extract HSM key
        let hsm_key = payload.get("hsm_key")?.as_str()?.to_string();

        // Check expiration
        if let Some(exp) = payload.get("exp").and_then(|e| e.as_i64()) {
            let now = chrono::Utc::now().timestamp();
            if exp < now {
                return None;
            }
        }

        Some(hsm_key)
    }

    /// Create binary authentication message
    pub(super) fn create_auth_message(hsm_key: &str) -> Vec<u8> {
        let source = SOURCE_IDENTIFIER;
        let mode = MODE_PRODUCTION;

        // Calculate buffer size: 18 + hsm_key length + source length
        let buffer_size = 18 + hsm_key.len() + source.len();

        let mut buffer = Vec::with_capacity(buffer_size);

        // Data length (buffer_size - 2) as big-endian u16
        buffer.extend_from_slice(&((buffer_size - 2) as u16).to_be_bytes());

        // Request type = 1 (authentication)
        buffer.push(1);

        // Field count = 4
        buffer.push(4);

        // Field-1: AuthToken (HSM key)
        buffer.push(1); // Field ID
        buffer.extend_from_slice(&(hsm_key.len() as u16).to_be_bytes());
        buffer.extend_from_slice(hsm_key.as_bytes());

        // Field-2: Mode
        buffer.push(2); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.extend_from_slice(mode.as_bytes());

        // Field-3: Unknown flag (always 1)
        buffer.push(3); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.push(1);

        // Field-4: Source
        buffer.push(4); // Field ID
        buffer.extend_from_slice(&(source.len() as u16).to_be_bytes());
        buffer.extend_from_slice(source.as_bytes());

        buffer
    }

    /// Create binary subscription message
    pub(super) fn create_subscription_message(hsm_symbols: &[String], channel: u8) -> Vec<u8> {
        // Create scrips data
        let mut scrips_data = Vec::new();

        // Symbol count as big-endian u16
        let count = hsm_symbols.len() as u16;
        scrips_data.push((count >> 8) as u8);
        scrips_data.push((count & 0xFF) as u8);

        // Each symbol: length byte + symbol bytes
        for symbol in hsm_symbols {
            let symbol_bytes = symbol.as_bytes();
            scrips_data.push(symbol_bytes.len() as u8);
            scrips_data.extend_from_slice(symbol_bytes);
        }

        // Build complete message
        let data_len = 6 + scrips_data.len();

        let mut buffer = Vec::with_capacity(data_len + 2);

        // Data length as big-endian u16
        buffer.extend_from_slice(&(data_len as u16).to_be_bytes());

        // Request type = 4 (subscription)
        buffer.push(4);

        // Field count = 2
        buffer.push(2);

        // Field-1: Symbols
        buffer.push(1); // Field ID
        buffer.extend_from_slice(&(scrips_data.len() as u16).to_be_bytes());
        buffer.extend_from_slice(&scrips_data);

        // Field-2: Channel
        buffer.push(2); // Field ID
        buffer.extend_from_slice(&(1u16).to_be_bytes());
        buffer.push(channel);

        buffer
    }

    /// Convert Fyers symbol to HSM token format
    /// Input: "NSE:RELIANCE-EQ" or "NSE:NIFTY50-INDEX"
    /// Output: "sf|nse_cm|{token}" or "if|nse_cm|Nifty 50"
    pub fn symbol_to_hsm_token(symbol: &str, data_type: &FyersDataType) -> String {
        let prefix = match data_type {
            FyersDataType::SymbolUpdate => "sf",
            FyersDataType::DepthUpdate => "dp",
            FyersDataType::IndexUpdate => "if",
        };

        // Parse symbol format "EXCHANGE:SYMBOL-TYPE"
        let parts: Vec<&str> = symbol.split(':').collect();
        if parts.len() != 2 {
            // Return as-is if already in HSM format
            if symbol.contains('|') {
                return symbol.to_string();
            }
            return format!("{}|nse_cm|{}", prefix, symbol);
        }

        let exchange = parts[0].to_lowercase();
        let symbol_part = parts[1];

        // Determine segment
        let segment = if symbol_part.ends_with("-INDEX") {
            match exchange.as_str() {
                "nse" => "nse_cm",
                "bse" => "bse_cm",
                _ => "nse_cm",
            }
        } else if symbol_part.contains("-FUT") || symbol_part.contains("-OPT") {
            match exchange.as_str() {
                "nse" | "nfo" => "nse_fo",
                "bse" | "bfo" => "bse_fo",
                "mcx" => "mcx_fo",
                _ => "nse_fo",
            }
        } else {
            // Cash market
            match exchange.as_str() {
                "nse" => "nse_cm",
                "bse" => "bse_cm",
                _ => "nse_cm",
            }
        };

        // For indices, use "if" prefix
        let final_prefix = if symbol_part.ends_with("-INDEX") { "if" } else { prefix };

        // Extract token from symbol (remove -EQ, -INDEX suffix)
        let token = symbol_part
            .replace("-EQ", "")
            .replace("-INDEX", "")
            .replace("-FUT", "")
            .replace("-OPT", "");

        format!("{}|{}|{}", final_prefix, segment, token)
    }
}
