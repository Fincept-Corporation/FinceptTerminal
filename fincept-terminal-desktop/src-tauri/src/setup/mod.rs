// setup/mod.rs - Simplified Python and Bun setup

#![allow(dead_code)]

pub mod checks;
pub mod install;
pub mod requirements;
pub mod commands;

pub use commands::*;

use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::sync::Mutex;
use tauri::{AppHandle, Emitter, Manager};

#[cfg(target_os = "windows")]
pub(super) use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
pub(super) const CREATE_NO_WINDOW: u32 = 0x08000000;

pub(super) static SETUP_RUNNING: Mutex<bool> = Mutex::new(false);

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SetupProgress {
    pub step: String,
    pub progress: u8,
    pub message: String,
    pub is_error: bool,
}

#[derive(Debug, Clone, Serialize)]
pub struct SetupStatus {
    pub python_installed: bool,
    pub bun_installed: bool,
    pub packages_installed: bool,
    pub needs_setup: bool,
    pub needs_sync: bool,
    pub sync_message: Option<String>,
}

pub(super) const PYTHON_VERSION: &str = "3.12.7";
pub(super) const BUN_VERSION: &str = "1.1.0";

pub(super) fn emit_progress(app: &AppHandle, step: &str, progress: u8, message: &str, is_error: bool) {
    let _ = app.emit("setup-progress", SetupProgress {
        step: step.to_string(),
        progress,
        message: message.to_string(),
        is_error,
    });
    eprintln!("[SETUP] [{}] {}% - {}", step, progress, message);
}

pub(super) fn is_dev_mode() -> bool {
    cfg!(debug_assertions)
}

pub(super) fn get_install_dir(app: &AppHandle) -> Result<PathBuf, String> {
    if is_dev_mode() {
        // Dev mode: use OS-specific user directory
        let base_dir = if cfg!(target_os = "windows") {
            std::env::var("LOCALAPPDATA")
                .map(PathBuf::from)
                .unwrap_or_else(|_| PathBuf::from("C:\\Users\\Default\\AppData\\Local"))
        } else if cfg!(target_os = "macos") {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join("Library/Application Support"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        } else {
            std::env::var("HOME")
                .map(|h| PathBuf::from(h).join(".local/share"))
                .unwrap_or_else(|_| PathBuf::from("/tmp"))
        };
        Ok(base_dir.join("fincept-dev"))
    } else {
        // Production: use app data directory (writable, persists across updates)
        // Windows: C:\Users\<Username>\AppData\Roaming\com.fincept.terminal\
        // macOS: ~/Library/Application Support/com.fincept.terminal/
        // Linux: ~/.local/share/com.fincept.terminal/
        let app_data_dir = app.path().app_data_dir()
            .map_err(|e| format!("Failed to get app data dir: {}", e))?;

        eprintln!("[SETUP] Production install directory: {:?}", app_data_dir);
        Ok(app_data_dir)
    }
}
