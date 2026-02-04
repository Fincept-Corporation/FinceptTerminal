// Broker Credentials Module - Secure encrypted storage for broker API credentials
// Uses AES-256-GCM encryption with HMAC-SHA256 key derivation

use aes_gcm::{
    aead::{Aead, KeyInit, OsRng},
    Aes256Gcm, Nonce,
};
use anyhow::{Context, Result};
use base64::{engine::general_purpose, Engine as _};
use hmac::Hmac;
use once_cell::sync::Lazy;
use parking_lot::Mutex;
use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use sha2::Sha256;
use std::path::PathBuf;
use std::time::{SystemTime, UNIX_EPOCH};

/// Number of HMAC iterations for key derivation
const KDF_ITERATIONS: u32 = 100_000;

/// Encryption key storage
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

/// Get the app data directory for storing persistent files
fn get_app_data_dir() -> Result<PathBuf> {
    #[cfg(target_os = "windows")]
    {
        if let Ok(local_app_data) = std::env::var("LOCALAPPDATA") {
            return Ok(PathBuf::from(local_app_data).join("fincept-dev"));
        }
    }

    #[cfg(target_os = "macos")]
    {
        if let Ok(home) = std::env::var("HOME") {
            return Ok(PathBuf::from(home).join("Library/Application Support/fincept-dev"));
        }
    }

    #[cfg(target_os = "linux")]
    {
        if let Ok(home) = std::env::var("HOME") {
            return Ok(PathBuf::from(home).join(".local/share/fincept-dev"));
        }
    }

    Err(anyhow::anyhow!("Could not determine app data directory"))
}

/// Get or create a stable, persisted key identifier.
/// On first run, generates a random 32-byte ID and saves it.
/// On subsequent runs, reads the persisted ID.
fn get_stable_key_id() -> Result<Vec<u8>> {
    let data_dir = get_app_data_dir()?;
    let key_id_path = data_dir.join(".credential_key_id");

    if key_id_path.exists() {
        let contents = std::fs::read(&key_id_path)
            .context("Failed to read persisted key ID")?;
        if contents.len() == 32 {
            return Ok(contents);
        }
        // Invalid length, regenerate
        eprintln!("[BrokerCredentials] Warning: Key ID file has invalid length, regenerating");
    }

    // Generate new random key ID
    use rand::RngCore;
    let mut key_id = vec![0u8; 32];
    OsRng.fill_bytes(&mut key_id);

    // Ensure directory exists
    std::fs::create_dir_all(&data_dir)
        .context("Failed to create app data directory")?;

    std::fs::write(&key_id_path, &key_id)
        .context("Failed to persist key ID")?;

    eprintln!("[BrokerCredentials] Generated and persisted new key ID");
    Ok(key_id)
}

/// Get or create a persisted salt for key derivation
fn get_or_create_salt() -> Result<Vec<u8>> {
    let data_dir = get_app_data_dir()?;
    let salt_path = data_dir.join(".credential_salt");

    if salt_path.exists() {
        let contents = std::fs::read(&salt_path)
            .context("Failed to read persisted salt")?;
        if contents.len() == 32 {
            return Ok(contents);
        }
        eprintln!("[BrokerCredentials] Warning: Salt file has invalid length, regenerating");
    }

    // Generate new random salt
    use rand::RngCore;
    let mut salt = vec![0u8; 32];
    OsRng.fill_bytes(&mut salt);

    std::fs::create_dir_all(&data_dir)
        .context("Failed to create app data directory")?;

    std::fs::write(&salt_path, &salt)
        .context("Failed to persist salt")?;

    eprintln!("[BrokerCredentials] Generated and persisted new salt");
    Ok(salt)
}

/// Initialize encryption key (should be called on app startup)
/// Uses a persisted stable key ID + PBKDF2 key derivation
pub fn init_encryption_key() -> Result<()> {
    let mut key_guard = ENCRYPTION_KEY.lock();

    if key_guard.is_none() {
        let key_id = get_stable_key_id()?;
        let salt = get_or_create_salt()?;
        let key = derive_encryption_key(&key_id, &salt)?;
        *key_guard = Some(key);

        eprintln!("[BrokerCredentials] Encryption key initialized from persisted key ID");
    }

    Ok(())
}

/// Derive encryption key using PBKDF2-HMAC-SHA256
fn derive_encryption_key(key_id: &[u8], salt: &[u8]) -> Result<Vec<u8>> {
    let mut derived_key = vec![0u8; 32]; // 32 bytes for AES-256
    pbkdf2::pbkdf2::<Hmac<Sha256>>(key_id, salt, KDF_ITERATIONS, &mut derived_key)
        .map_err(|e| anyhow::anyhow!("PBKDF2 key derivation failed: {}", e))?;
    Ok(derived_key)
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

/// Helper to decrypt a single field, returning an error rather than silently dropping
fn decrypt_field(field_name: &str, broker_id: &str, encrypted_value: &Option<String>) -> Result<Option<String>> {
    match encrypted_value {
        Some(val) => {
            let decrypted = decrypt_data(val)
                .with_context(|| format!(
                    "Failed to decrypt {} for broker '{}'. The encryption key may have changed.",
                    field_name, broker_id
                ))?;
            Ok(Some(decrypted))
        }
        None => Ok(None),
    }
}

/// Get current timestamp in seconds since UNIX epoch
fn current_timestamp() -> i64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs() as i64
}

/// Save broker credentials (insert or update)
pub fn save_credentials(conn: &Connection, creds: &BrokerCredentials) -> Result<()> {
    eprintln!("[BrokerCredentials] save_credentials called for broker: {}", creds.broker_id);

    eprintln!("[BrokerCredentials] Input data:");
    eprintln!("  - api_key: {:?} (len: {})",
        creds.api_key.as_ref().map(|s| if !s.is_empty() { "***PRESENT***" } else { "EMPTY" }),
        creds.api_key.as_ref().map(|s| s.len()).unwrap_or(0));
    eprintln!("  - api_secret: {:?} (len: {})",
        creds.api_secret.as_ref().map(|s| if !s.is_empty() { "***PRESENT***" } else { "EMPTY" }),
        creds.api_secret.as_ref().map(|s| s.len()).unwrap_or(0));

    // Ensure encryption key is initialized
    init_encryption_key()?;

    let now = current_timestamp();

    // Encrypt sensitive fields (including additional_data)
    let encrypted_api_key = creds.api_key.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_api_secret = creds.api_secret.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_access_token = creds.access_token.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_refresh_token = creds.refresh_token.as_ref().map(|v| encrypt_data(v)).transpose()?;
    let encrypted_additional_data = creds.additional_data.as_ref().map(|v| encrypt_data(v)).transpose()?;

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
            encrypted_additional_data,
            now,
        ],
    )?;

    eprintln!("[BrokerCredentials] ✓ Credentials saved successfully for broker: {}", creds.broker_id);
    Ok(())
}

/// Get broker credentials by broker_id
pub fn get_credentials(conn: &Connection, broker_id: &str) -> Result<Option<BrokerCredentials>> {
    eprintln!("[BrokerCredentials] get_credentials called for broker: {}", broker_id);
    init_encryption_key()?;

    eprintln!("[BrokerCredentials] Decrypting credentials for: {}", broker_id);

    let mut stmt = conn.prepare(
        "SELECT id, broker_id, api_key, api_secret, access_token, refresh_token, additional_data, encrypted, created_at, updated_at
         FROM broker_credentials
         WHERE broker_id = ?1",
    )?;

    let result = stmt.query_row(params![broker_id], |row| {
        let encrypted: bool = row.get::<_, i32>(7)? == 1;

        Ok(BrokerCredentials {
            id: Some(row.get(0)?),
            broker_id: row.get(1)?,
            api_key: row.get(2)?,
            api_secret: row.get(3)?,
            access_token: row.get(4)?,
            refresh_token: row.get(5)?,
            additional_data: row.get(6)?,
            encrypted,
            created_at: row.get(8)?,
            updated_at: row.get(9)?,
        })
    });

    match result {
        Ok(mut creds) => {
            eprintln!("[BrokerCredentials] Raw data from DB:");
            eprintln!("  - api_key encrypted: {:?}", creds.api_key.as_ref().map(|s| format!("{}...", &s[..20.min(s.len())])));
            eprintln!("  - api_secret encrypted: {:?}", creds.api_secret.as_ref().map(|s| format!("{}...", &s[..20.min(s.len())])));
            eprintln!("  - encrypted flag: {}", creds.encrypted);

            if creds.encrypted {
                // Try to decrypt — if the encryption key has changed, auto-delete stale credentials
                let decrypt_result = (|| -> Result<()> {
                    creds.api_key = decrypt_field("api_key", broker_id, &creds.api_key)?;
                    creds.api_secret = decrypt_field("api_secret", broker_id, &creds.api_secret)?;
                    creds.access_token = decrypt_field("access_token", broker_id, &creds.access_token)?;
                    creds.refresh_token = decrypt_field("refresh_token", broker_id, &creds.refresh_token)?;
                    creds.additional_data = decrypt_field("additional_data", broker_id, &creds.additional_data)?;
                    Ok(())
                })();

                if let Err(e) = decrypt_result {
                    eprintln!(
                        "[BrokerCredentials] WARNING: Decryption failed for '{}': {}. \
                         Deleting stale credentials -- please re-enter them.",
                        broker_id, e
                    );
                    // Remove the undecryptable row so the user can start fresh
                    let _ = delete_credentials(conn, broker_id);
                    return Err(anyhow::anyhow!(
                        "Credentials for '{}' could not be decrypted (encryption key changed). \
                         They have been removed — please re-enter your API credentials.",
                        broker_id
                    ));
                }

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
