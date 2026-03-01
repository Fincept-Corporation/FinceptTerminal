// commands/brokers/groww/mod.rs - Groww broker integration

#![allow(dead_code)]

pub mod auth;
pub mod orders;
pub mod portfolio;
pub mod market_data;

pub use auth::*;
pub use orders::*;
pub use portfolio::*;
pub use market_data::*;

use reqwest::header::{HeaderMap, HeaderValue, AUTHORIZATION, CONTENT_TYPE};

// Groww API Configuration
pub(super) const GROWW_API_BASE: &str = "https://api.groww.in";

// Segment constants
pub(super) const SEGMENT_CASH: &str = "CASH";
pub(super) const SEGMENT_FNO: &str = "FNO";

// Exchange constants
pub(super) const EXCHANGE_NSE: &str = "NSE";
pub(super) const EXCHANGE_BSE: &str = "BSE";

pub(super) fn create_groww_headers(access_token: &str) -> HeaderMap {
    let mut headers = HeaderMap::new();

    let auth_value = format!("Bearer {}", access_token);
    if let Ok(header_value) = HeaderValue::from_str(&auth_value) {
        headers.insert(AUTHORIZATION, header_value);
    }

    headers.insert(CONTENT_TYPE, HeaderValue::from_static("application/json"));
    headers.insert("Accept", HeaderValue::from_static("application/json"));
    headers
}

/// Generate TOTP code from API secret using TOTP algorithm
pub(super) fn generate_totp(api_secret: &str) -> Result<String, String> {
    use std::time::{SystemTime, UNIX_EPOCH};

    // Decode base32 secret
    let secret_bytes = base32_decode(api_secret)?;

    // Get current time step (30 second intervals)
    let time = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map_err(|e| format!("Time error: {}", e))?
        .as_secs() / 30;

    // Generate HOTP with time counter
    let hotp = generate_hotp(&secret_bytes, time)?;

    // Return 6 digit code with leading zeros
    Ok(format!("{:06}", hotp % 1_000_000))
}

/// Decode base32 string to bytes
fn base32_decode(input: &str) -> Result<Vec<u8>, String> {
    let alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    let input = input.to_uppercase().replace("=", "");

    let mut bits = String::new();
    for c in input.chars() {
        let index = alphabet.find(c)
            .ok_or_else(|| format!("Invalid base32 character: {}", c))?;
        bits.push_str(&format!("{:05b}", index));
    }

    let mut bytes = Vec::new();
    for chunk in bits.as_bytes().chunks(8) {
        if chunk.len() == 8 {
            let byte_str = std::str::from_utf8(chunk)
                .map_err(|e| format!("Invalid UTF-8 in base32 decode: {}", e))?;
            let byte = u8::from_str_radix(byte_str, 2)
                .map_err(|e| format!("Invalid binary digit in base32 decode: {}", e))?;
            bytes.push(byte);
        }
    }

    Ok(bytes)
}

/// Generate HOTP code
fn generate_hotp(secret: &[u8], counter: u64) -> Result<u32, String> {
    use hmac::{Hmac, Mac};
    use sha1::Sha1;

    type HmacSha1 = Hmac<Sha1>;

    let counter_bytes = counter.to_be_bytes();

    let mut mac = HmacSha1::new_from_slice(secret)
        .map_err(|e| format!("HMAC error: {}", e))?;
    mac.update(&counter_bytes);
    let result = mac.finalize().into_bytes();

    // Dynamic truncation
    let offset = (result[19] & 0x0f) as usize;
    let code = ((result[offset] & 0x7f) as u32) << 24
        | (result[offset + 1] as u32) << 16
        | (result[offset + 2] as u32) << 8
        | (result[offset + 3] as u32);

    Ok(code)
}
