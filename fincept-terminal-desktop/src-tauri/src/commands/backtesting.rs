/**
 * Backtesting Tauri Commands
 *
 * Rust commands for executing Python backtesting providers.
 * Bridges TypeScript frontend to Python backend.
 */

use serde::{Deserialize, Serialize};
use std::process::Command;
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use crate::utils::python;

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// ============================================================================
// Types
// ============================================================================

#[derive(Debug, Serialize, Deserialize)]
pub struct CommandResult {
    success: bool,
    stdout: String,
    stderr: String,
    exit_code: i32,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ProcessInfo {
    process_id: u32,
}

#[derive(Debug, Clone)]
struct RunningProcess {
    pid: u32,
    // In production, would store child process handle for cancellation
}

// State for tracking running processes
pub struct BacktestingState {
    running_processes: Arc<Mutex<HashMap<String, RunningProcess>>>,
}

impl Default for BacktestingState {
    fn default() -> Self {
        Self {
            running_processes: Arc::new(Mutex::new(HashMap::new())),
        }
    }
}

// ============================================================================
// Commands
// ============================================================================

// Removed execute_lean_cli - Lean support discontinued

/**
 * Execute generic command
 *
 * Used for testing connections, running utilities, etc.
 */
#[tauri::command]
pub async fn execute_command(command: String) -> Result<CommandResult, String> {
    // Parse command
    let parts: Vec<&str> = command.split_whitespace().collect();
    if parts.is_empty() {
        return Err("Empty command".to_string());
    }

    let program = parts[0];
    let args = &parts[1..];

    // Execute
    let output = Command::new(program)
        .args(args)
        .output()
        .map_err(|e| format!("Failed to execute command: {}", e))?;

    Ok(CommandResult {
        success: output.status.success(),
        stdout: String::from_utf8_lossy(&output.stdout).to_string(),
        stderr: String::from_utf8_lossy(&output.stderr).to_string(),
        exit_code: output.status.code().unwrap_or(-1),
    })
}

// Removed kill_lean_process - Lean support discontinued

/**
 * Execute Python backtesting provider
 *
 * Runs a Python provider script with given command and arguments.
 * Automatically selects the correct Python venv based on provider.
 */
#[tauri::command]
pub async fn execute_python_backtest(
    provider: String,
    command: String,
    args: String,
    app: tauri::AppHandle,
) -> Result<String, String> {
    // Use library-specific Python path (switches between numpy1/numpy2 venvs)
    let python_path = python::get_python_path_for_library(&app, Some(&provider))?;

    // Build script path - handle both dev and production
    let script_relative_path = format!(
        "Analytics/backtesting/{}/{}",
        provider.to_lowercase(),
        format!("{}_provider.py", provider.to_lowercase())
    );

    // Use the central get_script_path utility which handles path resolution
    let script_path = python::get_script_path(&app, &script_relative_path)?;

    // Execute Python script
    let mut cmd = Command::new(&python_path);

    #[cfg(target_os = "windows")]
    cmd.creation_flags(0x08000000); // CREATE_NO_WINDOW

    let output = cmd
        .arg(&script_path)
        .arg(&command)
        .arg(&args)
        .output()
        .map_err(|e| format!("Failed to execute Python script: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script failed: {}", stderr));
    }

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(stdout)
}

/**
 * Check if file exists
 *
 * Utility to verify file paths.
 */
#[tauri::command]
pub async fn check_file_exists(path: String) -> Result<bool, String> {
    use std::path::Path;
    Ok(Path::new(&path).exists())
}

/**
 * Create directory
 *
 * Creates a directory if it doesn't exist.
 */
#[tauri::command]
pub async fn create_directory(path: String) -> Result<(), String> {
    use std::fs;
    fs::create_dir_all(&path).map_err(|e| format!("Failed to create directory: {}", e))
}

/**
 * Write file
 *
 * Writes content to a file.
 */
#[tauri::command]
pub async fn write_file(path: String, content: String) -> Result<(), String> {
    use std::fs;
    fs::write(&path, content).map_err(|e| format!("Failed to write file: {}", e))
}

/**
 * Read file
 *
 * Reads content from a file.
 */
#[tauri::command]
pub async fn read_file(path: String) -> Result<String, String> {
    use std::fs;
    fs::read_to_string(&path).map_err(|e| format!("Failed to read file: {}", e))
}

// ============================================================================
// Helper Functions
// ============================================================================

// Note: get_python_path() has been removed - now using crate::utils::python::get_python_path()
// which properly handles dev vs production mode with #[cfg(debug_assertions)]
