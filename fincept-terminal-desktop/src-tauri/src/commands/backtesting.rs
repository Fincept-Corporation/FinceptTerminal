/**
 * Backtesting Tauri Commands
 *
 * Rust commands for executing Python backtesting providers.
 * Bridges TypeScript frontend to Python backend.
 */

use serde::{Deserialize, Serialize};
use std::process::Command;
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

// ============================================================================
// Commands
// ============================================================================

// Removed execute_lean_cli - Lean support discontinued

/**
 * Execute generic command
 *
 * Only allows a restricted set of safe commands for testing connections
 * and running backtesting utilities. Prevents command injection.
 */
#[tauri::command]
pub async fn execute_command(command: String) -> Result<CommandResult, String> {
    // Parse command
    let parts: Vec<&str> = command.split_whitespace().collect();
    if parts.is_empty() {
        return Err("Empty command".to_string());
    }

    let program = parts[0];

    // Allowlist of safe programs
    let allowed_programs = [
        "python", "python3", "python.exe",
        "pip", "pip3", "pip.exe",
    ];

    // Extract just the binary name from potential full path
    let program_name = std::path::Path::new(program)
        .file_name()
        .and_then(|n| n.to_str())
        .unwrap_or(program)
        .to_lowercase();

    if !allowed_programs.iter().any(|&allowed| program_name == allowed.to_lowercase()) {
        return Err(format!(
            "Command '{}' is not allowed. Only Python-related commands are permitted.",
            program
        ));
    }

    let args = &parts[1..];

    // Block dangerous Python flags/arguments
    for arg in args.iter() {
        let lower_arg = arg.to_lowercase();
        if lower_arg == "-c" || lower_arg == "--command" {
            return Err("Inline code execution via -c flag is not allowed".to_string());
        }
    }

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
        let stdout = String::from_utf8_lossy(&output.stdout);
        // Python providers print JSON errors to stdout then exit(1)
        // Include both stdout and stderr for full error context
        if !stdout.is_empty() {
            return Err(format!("Python script failed: {}", stdout.trim()));
        }
        return Err(format!("Python script failed: {}", stderr));
    }

    let stdout = String::from_utf8_lossy(&output.stdout).to_string();
    Ok(stdout)
}

/**
 * Validate that a path is within the allowed backtesting data directory.
 * Prevents path traversal attacks and access to system files.
 */
fn validate_backtesting_path(path: &str) -> Result<std::path::PathBuf, String> {
    use std::path::PathBuf;

    let path = PathBuf::from(path);

    // Canonicalize to resolve any .. or symlinks
    // For new files that don't exist yet, validate the parent directory
    let resolved = if path.exists() {
        path.canonicalize()
            .map_err(|e| format!("Failed to resolve path: {}", e))?
    } else {
        // For non-existent paths, check parent exists and is valid
        let parent = path.parent()
            .ok_or_else(|| "Invalid path: no parent directory".to_string())?;
        if !parent.exists() {
            return Err("Parent directory does not exist".to_string());
        }
        let resolved_parent = parent.canonicalize()
            .map_err(|e| format!("Failed to resolve parent path: {}", e))?;
        resolved_parent.join(path.file_name().unwrap_or_default())
    };

    // Only allow paths within user's app data or home directory for backtesting
    let allowed_roots: Vec<PathBuf> = vec![
        // Windows: %APPDATA%/fincept-terminal/backtesting/
        dirs::data_dir().map(|d| d.join("fincept-terminal")),
        dirs::config_dir().map(|d| d.join("fincept-terminal")),
        // Home directory backtesting folder
        dirs::home_dir().map(|d| d.join(".fincept").join("backtesting")),
        // Temp directory for intermediate results
        Some(std::env::temp_dir().join("fincept-backtesting")),
    ].into_iter().flatten().collect();

    let path_str = resolved.to_string_lossy().to_lowercase();

    // Block access to sensitive system paths regardless
    let blocked_patterns = [
        "system32", "windows\\system", "/etc/", "/usr/",
        ".ssh", ".gnupg", ".aws", "credentials",
        ".env", "secrets",
    ];

    for pattern in &blocked_patterns {
        if path_str.contains(&pattern.to_lowercase()) {
            return Err(format!("Access to path containing '{}' is not allowed", pattern));
        }
    }

    // Check if path is within any allowed root
    let is_allowed = allowed_roots.iter().any(|root| {
        resolved.starts_with(root)
    });

    if !is_allowed {
        return Err(format!(
            "Path '{}' is outside allowed directories. Files must be within the fincept-terminal data directory.",
            path.display()
        ));
    }

    Ok(resolved)
}

/**
 * Check if file exists
 *
 * Utility to verify file paths. Only allows paths within backtesting data directory.
 */
#[tauri::command]
pub async fn check_file_exists(path: String) -> Result<bool, String> {
    let validated_path = validate_backtesting_path(&path)?;
    Ok(validated_path.exists())
}

/**
 * Create directory
 *
 * Creates a directory if it doesn't exist. Only allows within backtesting data directory.
 */
#[tauri::command]
pub async fn create_directory(path: String) -> Result<(), String> {
    use std::fs;
    let validated_path = validate_backtesting_path(&path)?;
    fs::create_dir_all(&validated_path).map_err(|e| format!("Failed to create directory: {}", e))
}

/**
 * Write file
 *
 * Writes content to a file. Only allows within backtesting data directory.
 */
#[tauri::command]
pub async fn write_file(path: String, content: String) -> Result<(), String> {
    use std::fs;
    let validated_path = validate_backtesting_path(&path)?;
    fs::write(&validated_path, content).map_err(|e| format!("Failed to write file: {}", e))
}

/**
 * Read file
 *
 * Reads content from a file. Only allows within backtesting data directory.
 */
#[tauri::command]
pub async fn read_file(path: String) -> Result<String, String> {
    use std::fs;
    let validated_path = validate_backtesting_path(&path)?;
    fs::read_to_string(&validated_path).map_err(|e| format!("Failed to read file: {}", e))
}

// ============================================================================
// Helper Functions
// ============================================================================

// Note: get_python_path() has been removed - now using crate::utils::python::get_python_path()
// which properly handles dev vs production mode with #[cfg(debug_assertions)]
