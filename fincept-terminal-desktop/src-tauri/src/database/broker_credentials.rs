// Broker Credentials Module - Secure encrypted storage for broker API credentials
// Uses AES-256-GCM encryption for sensitive data

use aes_gcm::{
    aead::{Aead, KeyInit, OsRng},
    Aes256Gcm, Nonce,
};
use anyhow::{Context, Result};
use base64::{engine::general_purpose, Engine as _};
use once_cell::sync::Lazy;
use parking_lot::Mutex;
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use std::time::{SystemTime, UNIX_EPOCH};

/// Encryption key storage (in production, use OS keychain)
static ENCRYPTION_KEY: Lazy<Mutex<Option<Vec<u8>>>> = Lazy::new(|| Mutex::new(None));

/// Broker credentials structure
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BrokerCredentials {
    pub id: Option<i64>,
    pub broker_id: String,
    pub api_key: Option<String>,
    pub api_secret: Option<String>,
    pub access_token: Option<String>,
    pub refresh_token: Option<String>,
    pub additional_data: Option<String>, // JSON blob for broker-specific fields
    pub encrypted: bool,
    pub created_at: i64,
    pub updated_at: i64,
}

/// Initialize encryption key (should be called on app startup)
/// Uses machine-specific data to derive a consistent key across restarts
pub fn init_encryption_key() -> Result<()> {
    let mut key_guard = ENCRYPTION_KEY.lock();

    if key_guard.is_none() {
        // Derive encryption key from machine-specific identifiers
        // This ensures the same key is used across app restarts
        let machine_id = get_machine_id()?;
        let key = derive_encryption_key(&machine_id);
        *key_guard = Some(key);

        eprintln!("[BrokerCredentials] Encryption key initialized from machine ID");
    }

    Ok(())
}

/// Get machine-specific identifier
fn get_machine_id() -> Result<String> {
    use std::env;

    // Combine multiple machine-specific values for uniqueness
    let mut identifiers = Vec::new();

    // Computer name
    if let Ok(name) = env::var("COMPUTERNAME") {
        identifiers.push(name);
    } else if let Ok(name) = env::var("HOSTNAME") {
        identifiers.push(name);
    }

    // Username
    if let Ok(user) = env::var("USERNAME") {
        identifiers.push(user);
    } else if let Ok(user) = env::var("USER") {
        identifiers.push(user);
    }

    // OS-specific identifiers
    #[cfg(target_os = "windows")]
    {
        if let Ok(sid) = env::var("USERDOMAIN") {
            identifiers.push(sid);
        }
    }

    // Fallback if no identifiers found
    if identifiers.is_empty() {
        identifiers.push("fincept-terminal-default".to_string());
    }

    Ok(identifiers.join("-"))
}

/// Derive a consistent encryption key from machine ID
fn derive_encryption_key(machine_id: &str) -> Vec<u8> {
    use sha2::{Sha256, Digest};

    // Use PBKDF2-like approach with SHA-256
    let mut hasher = Sha256::new();
    hasher.update(b"fincept-terminal-encryption-v1");
    hasher.update(machine_id.as_bytes());
    hasher.update(b"broker-credentials");

    let result = hasher.finalize();
    result.to_vec() // 32 bytes for AES-256
}

/// Encrypt data using AES-256-GCM
fn encrypt_data(plaintext: &str) -> Result<String> {
    let key_guard = ENCRYPTION_KEY.lock();
    let key_bytes = key_guard
        .as_ref()
        .context("Encryption key not initialized")?;

    let key = aes_gcm::Key::<Aes256Gcm>::from_slice(key_bytes);
    let cipher = Aes256Gcm::new(key);

    // Generate random nonce (12 bytes for GCM)
    use rand::RngCore;
    let mut nonce_bytes = [0u8; 12];
    OsRng.fill_bytes(&mut nonce_bytes);
    let nonce = Nonce::from_slice(&nonce_bytes);

    // Encrypt
    let ciphertext = cipher
        .encrypt(nonce, plaintext.as_bytes())
        .map_err(|e| anyhow::anyhow!("Encryption failed: {}", e))?;

    // Combine nonce + ciphertext and encode as base64
    let mut combined = nonce_bytes.to_vec();
    combined.extend_from_slice(&ciphertext);

    Ok(general_purpose::STANDARD.encode(&combined))
}

/// Decrypt data using AES-256-GCM
fn decrypt_data(encrypted: &str) -> Result<String> {
    let key_guard = ENCRYPTION_KEY.lock();
    let key_bytes = key_guard
        .as_ref()
        .context("Encryption key not initialized")?;

    let key = aes_gcm::Key::<Aes256Gcm>::from_slice(key_bytes);
    let cipher = Aes256Gcm::new(key);

    // Decode base64
    let combined = general_purpose::STANDARD
        .decode(encrypted)
        .context("Invalid base64 encoding")?;

    // Split nonce (first 12 bytes) and ciphertext
    if combined.len() < 12 {
        return Err(anyhow::anyhow!("Invalid encrypted data length"));
    }

    let (nonce_bytes, ciphertext) = combined.split_at(12);
    let nonce = Nonce::from_slice(nonce_bytes);

    // Decrypt
    let plaintext = cipher
        .decrypt(nonce, ciphertext)
        .map_err(|e| anyhow::anyhow!("Decryption failed: {}", e))?;

    String::from_utf8(plaintext).context("Invalid UTF-8 in decrypted data")
}

/// Get current timestamp in seconds since UNIX epoch
fn current_timestamp() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap()
        .as_secs() as i64
}

/// Save broker credentials (insert or update)
pub fn save_credentials(conn: &Connection, creds: &BrokerCredentials) -> Result<()> {
    eprintln!("[BrokerCredentials] save_credentials called for broker: {}", creds.broker_id);

    // Debug: Print what we received
    eprintln!("[BrokerCredentials] Input data:");
    eprintln!("  - api_key: {:?} (len: {})",
        creds.api_key.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
        creds.api_key.as_ref().map(|s| s.len()).unwrap_or(0));
    eprintln!("  - api_secret: {:?} (len: {})",
        creds.api_secret.as_ref().map(|s| if s.len() > 0 { "***PRESENT***" } else { "EMPTY" }),
        creds.api_secret.as_ref().map(|s| s.len()).unwrap_or(0));

    // Ensure encryption key is initialized
    init_encryption_key()?;

    let now = current_timestamp();

    // Encrypt sensitive fields
    let encrypted_api_key = creds.api_key.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_api_secret = creds.api_secret.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_access_token = creds.access_token.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_refresh_token = creds.refresh_token.as_ref().map(|v| encrypt_data(v)).transpose()?;

    conn.execute(
        "INSERT INTO broker_credentials
         (broker_id, api_key, api_secret, access_token, refresh_token, additional_data, encrypted, created_at, updated_at)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, 1, ?7, ?7)
         ON CONFLICT(broker_id) DO UPDATE SET
            api_key = excluded.api_key,
            api_secret = excluded.api_secret,
            access_token = excluded.access_token,
            refresh_token = excluded.refresh_token,
            additional_data = excluded.additional_data,
            updated_at = ?7",
        params![
            &creds.broker_id,
            encrypted_api_key,
            encrypted_api_secret,
            encrypted_access_token,
            encrypted_refresh_token,
            &creds.additional_data,
            now,
        ],
    )?;

    eprintln!("[BrokerCredentials] ✓ Credentials saved successfully for broker: {}", creds.broker_id);
    Ok(())
}

/// Get broker credentials by broker_id
pub fn get_credentials(conn: &Connection, broker_id: &str) -> Result<Option<BrokerCredentials>> {
    eprintln!("[BrokerCredentials] get_credentials called for broker: {}", broker_id);
    // Ensure encryption key is initialized
    init_encryption_key()?;

    eprintln!("[BrokerCredentials] Decrypting credentials for: {}", broker_id);

    let mut stmt = conn.prepare(
        "SELECT id, broker_id, api_key, api_secret, access_token, refresh_token, additional_data, encrypted, created_at, updated_at
         FROM broker_credentials
         WHERE broker_id = ?1",
    )?;

    let result = stmt.query_row(params![broker_id], |row| {
        let encrypted: bool = row.get::<_, i32>(7)? == 1;

        // Get encrypted fields
        let api_key_enc: Option<String> = row.get(2)?;
        let api_secret_enc: Option<String> = row.get(3)?;
        let access_token_enc: Option<String> = row.get(4)?;
        let refresh_token_enc: Option<String> = row.get(5)?;

        Ok(BrokerCredentials {
            id: Some(row.get(0)?),
            broker_id: row.get(1)?,
            api_key: api_key_enc,
            api_secret: api_secret_enc,
            access_token: access_token_enc,
            refresh_token: refresh_token_enc,
            additional_data: row.get(6)?,
            encrypted,
            created_at: row.get(8)?,
            updated_at: row.get(9)?,
        })
    });

    match result {
        Ok(mut creds) => {
            // Debug: Print what was retrieved from database BEFORE decryption
            eprintln!("[BrokerCredentials] Raw data from DB:");
            eprintln!("  - api_key encrypted: {:?}", creds.api_key.as_ref().map(|s| format!("{}...", &s[..20.min(s.len())])));
            eprintln!("  - api_secret encrypted: {:?}", creds.api_secret.as_ref().map(|s| format!("{}...", &s[..20.min(s.len())])));
            eprintln!("  - encrypted flag: {}", creds.encrypted);

            // Decrypt sensitive fields if encrypted
            if creds.encrypted {
                eprintln!("[BrokerCredentials] Decrypting api_key for: {}", broker_id);
                creds.api_key = match creds.api_key.as_ref().map(|v| decrypt_data(v)).transpose() {
                    Ok(val) => val,
                    Err(e) => {
                        eprintln!("[BrokerCredentials] ❌ Failed to decrypt api_key: {}", e);
                        None
                    }
                };

                eprintln!("[BrokerCredentials] Decrypting api_secret for: {}", broker_id);
                creds.api_secret = match creds.api_secret.as_ref().map(|v| decrypt_data(v)).transpose() {
                    Ok(val) => val,
                    Err(e) => {
                        eprintln!("[BrokerCredentials] ❌ Failed to decrypt api_secret: {}", e);
                        None
                    }
                };

                eprintln!("[BrokerCredentials] Decrypting access_token for: {}", broker_id);
                creds.access_token = match creds.access_token.as_ref().map(|v| decrypt_data(v)).transpose() {
                    Ok(val) => val,
                    Err(e) => {
                        eprintln!("[BrokerCredentials] ❌ Failed to decrypt access_token: {}", e);
                        None
                    }
                };

                eprintln!("[BrokerCredentials] Decrypting refresh_token for: {}", broker_id);
                creds.refresh_token = match creds.refresh_token.as_ref().map(|v| decrypt_data(v)).transpose() {
                    Ok(val) => val,
                    Err(e) => {
                        eprintln!("[BrokerCredentials] ❌ Failed to decrypt refresh_token: {}", e);
                        None
                    }
                };

                eprintln!("[BrokerCredentials] ✓ Decryption complete for: {}", broker_id);
            }

            Ok(Some(creds))
        }
        Err(rusqlite::Error::QueryReturnedNoRows) => Ok(None),
        Err(e) => Err(e.into()),
    }
}

/// Delete broker credentials
pub fn delete_credentials(conn: &Connection, broker_id: &str) -> Result<()> {
    conn.execute(
        "DELETE FROM broker_credentials WHERE broker_id = ?1",
        params![broker_id],
    )?;

    Ok(())
}

/// List all saved broker IDs
pub fn list_all_credentials(conn: &Connection) -> Result<Vec<String>> {
    let mut stmt = conn.prepare("SELECT broker_id FROM broker_credentials ORDER BY broker_id")?;

    let broker_ids = stmt
        .query_map([], |row| row.get(0))?
        .collect::<Result<Vec<String>, _>>()?;

    Ok(broker_ids)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_encryption_decryption() {
        init_encryption_key().unwrap();

        let plaintext = "my_secret_api_key_12345";
        let encrypted = encrypt_data(plaintext).unwrap();
        let decrypted = decrypt_data(&encrypted).unwrap();

        assert_eq!(plaintext, decrypted);
        assert_ne!(plaintext, encrypted); // Ensure it's actually encrypted
    }
}
