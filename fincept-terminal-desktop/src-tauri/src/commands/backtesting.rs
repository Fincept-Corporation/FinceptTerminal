/**
 * Backtesting Tauri Commands
 *
 * Rust commands for executing Python backtesting providers.
 * Bridges TypeScript frontend to Python backend.
 */

use serde::{Deserialize, Serialize};
use std::process::Command;
use crate::python;

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

    // Execute in blocking thread to avoid blocking the async runtime
    let program = program.to_string();
    let args: Vec<String> = args.iter().map(|s| s.to_string()).collect();

    tokio::task::spawn_blocking(move || {
        let output = Command::new(&program)
            .args(&args)
            .output()
            .map_err(|e| format!("Failed to execute command: {}", e))?;

        Ok(CommandResult {
            success: output.status.success(),
            stdout: String::from_utf8_lossy(&output.stdout).to_string(),
            stderr: String::from_utf8_lossy(&output.stderr).to_string(),
            exit_code: output.status.code().unwrap_or(-1),
        })
    }).await.map_err(|e| format!("Task join error: {}", e))?
}

// Removed kill_lean_process - Lean support discontinued

/**
 * Execute Python backtesting provider
 *
 * Runs a Python provider script with given command and arguments.
 * For commands with large marketData, uses stdin to pass the JSON payload.
 * Automatically selects the correct Python venv based on provider.
 */
#[tauri::command]
pub async fn execute_python_backtest(
    provider: String,
    command: String,
    args: String,
    app: tauri::AppHandle,
) -> Result<String, String> {
    eprintln!("[RUST] execute_python_backtest: provider={}, command={}", provider, command);
    eprintln!("[RUST] args length: {} bytes", args.len());

    // Build script name - handle both dev and production
    let script_name = format!(
        "Analytics/backtesting/{}/{}",
        provider.to_lowercase(),
        format!("{}_provider.py", provider.to_lowercase())
    );

    // Check if args contains large JSON (>4KB suggests marketData present)
    let output = if args.len() > 4096 {
        eprintln!("[RUST] Large payload detected, using stdin for data transfer");

        // Execute in blocking thread since execute_with_stdin is sync
        let app_clone = app.clone();
        let script_name_clone = script_name.clone();
        let command_clone = command.clone();
        let args_clone = args.clone();

        tokio::task::spawn_blocking(move || {
            // Build arguments for Python script (without the large JSON)
            let script_args = vec![command_clone];

            // Pass the full args JSON via stdin
            python::execute_with_stdin(&app_clone, &script_name_clone, script_args, &args_clone)
        })
        .await
        .map_err(|e| format!("Task join error: {}", e))??
    } else {
        eprintln!("[RUST] Small payload, using standard execution");

        // Build arguments for Python script
        let script_args = vec![command, args];

        // Execute Python script using standard API
        python::execute(&app, &script_name, script_args).await?
    };

    eprintln!("[RUST] vectorbt output (first 500 chars): {}", &output.chars().take(500).collect::<String>());

    Ok(output)
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
 * Get home directory path
 */
#[tauri::command]
pub async fn get_home_dir() -> Result<String, String> {
    dirs::home_dir()
        .and_then(|p| p.to_str().map(String::from))
        .ok_or_else(|| "Failed to get home directory".to_string())
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

// Note: get_python_path() has been removed - now using crate::python::get_python_path()
// which properly handles dev vs production mode with #[cfg(debug_assertions)]
