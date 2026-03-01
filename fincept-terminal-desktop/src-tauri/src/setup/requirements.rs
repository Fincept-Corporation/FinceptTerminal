// setup/requirements.rs - Requirements hash tracking for update detection

use sha2::{Sha256, Digest};
use tauri::{AppHandle, Manager};

/// Compute SHA-256 hash of a bundled requirements file
pub(super) fn compute_requirements_hash(app: &AppHandle, requirements_file: &str) -> Result<String, String> {
    // tauri.conf.json maps resources/requirements-*.txt directly to requirements-*.txt
    let requirements_path = app.path()
        .resolve(
            requirements_file,
            tauri::path::BaseDirectory::Resource,
        )
        .map_err(|e| format!("Failed to resolve {}: {}", requirements_file, e))?;

    let content = std::fs::read_to_string(&requirements_path)
        .map_err(|e| format!("Failed to read {}: {}", requirements_file, e))?;

    let mut hasher = Sha256::new();
    hasher.update(content.as_bytes());
    let result = hasher.finalize();
    Ok(format!("{:x}", result))
}

/// Check if bundled requirements files have changed since last install.
/// Returns (needs_sync, description of what changed)
pub(super) fn check_requirements_sync(app: &AppHandle) -> (bool, Option<String>) {
    let files = [
        ("requirements-numpy1.txt", "requirements_numpy1_hash"),
        ("requirements-numpy2.txt", "requirements_numpy2_hash"),
    ];

    let mut changed: Vec<&str> = Vec::new();

    for (file, setting_key) in &files {
        match compute_requirements_hash(app, file) {
            Ok(current_hash) => {
                let stored_hash = crate::database::operations::get_setting(setting_key)
                    .ok()
                    .flatten();

                match stored_hash {
                    Some(stored) if stored == current_hash => {
                        eprintln!("[SETUP] {} hash matches stored value", file);
                    }
                    _ => {
                        eprintln!(
                            "[SETUP] {} hash mismatch or missing (stored: {:?}, current: {}...)",
                            file,
                            stored_hash.as_deref().map(|s| &s[..s.len().min(16)]),
                            &current_hash[..current_hash.len().min(16)]
                        );
                        changed.push(file);
                    }
                }
            }
            Err(e) => {
                eprintln!("[SETUP] Could not compute hash for {}: {}", file, e);
            }
        }
    }

    if changed.is_empty() {
        (false, None)
    } else {
        let msg = format!("Package updates available: {}", changed.join(", "));
        (true, Some(msg))
    }
}

/// Save the current hash of a requirements file to the database
pub(super) fn save_requirements_hash(app: &AppHandle, requirements_file: &str, setting_key: &str) -> Result<(), String> {
    let hash = compute_requirements_hash(app, requirements_file)?;
    crate::database::operations::save_setting(setting_key, &hash, Some("setup"))
        .map_err(|e| format!("Failed to save hash for {}: {}", requirements_file, e))
}
