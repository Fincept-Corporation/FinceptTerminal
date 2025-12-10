/**
 * Backtesting Tauri Commands
 *
 * Rust commands for executing Python backtesting providers.
 * Bridges TypeScript frontend to Python backend.
 */

use serde::{Deserialize, Serialize};
use std::process::{Command, Stdio};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use tauri::State;

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

/**
 * Execute Lean CLI command
 *
 * Runs Lean backtesting engine and returns process information.
 */
#[tauri::command]
pub async fn execute_lean_cli(
    command: String,
    working_dir: String,
    backtest_id: String,
    state: State<'_, BacktestingState>,
) -> Result<ProcessInfo, String> {
    // Parse command into parts
    let parts: Vec<&str> = command.split_whitespace().collect();
    if parts.is_empty() {
        return Err("Empty command".to_string());
    }

    let program = parts[0];
    let args = &parts[1..];

    // Execute command
    let child = Command::new(program)
        .args(args)
        .current_dir(&working_dir)
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .map_err(|e| format!("Failed to spawn Lean CLI: {}", e))?;

    let pid = child.id();

    // Store process info
    {
        let mut processes = state.running_processes.lock().unwrap();
        processes.insert(
            backtest_id.clone(),
            RunningProcess { pid },
        );
    }

    // In production, would spawn a thread to monitor output
    // For now, we'll wait for completion in the background

    Ok(ProcessInfo { process_id: pid })
}

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

/**
 * Kill Lean process
 *
 * Terminates a running backtest by process ID.
 */
#[tauri::command]
pub async fn kill_lean_process(
    process_id: u32,
    state: State<'_, BacktestingState>,
) -> Result<(), String> {
    #[cfg(unix)]
    {
        use nix::sys::signal::{kill, Signal};
        use nix::unistd::Pid;

        let pid = Pid::from_raw(process_id as i32);
        kill(pid, Signal::SIGTERM).map_err(|e| format!("Failed to kill process: {}", e))?;
    }

    #[cfg(windows)]
    {
        use std::process::Command;
        Command::new("taskkill")
            .args(&["/PID", &process_id.to_string(), "/F"])
            .output()
            .map_err(|e| format!("Failed to kill process: {}", e))?;
    }

    // Remove from tracking
    {
        let mut processes = state.running_processes.lock().unwrap();
        processes.retain(|_, p| p.pid != process_id);
    }

    Ok(())
}

/**
 * Execute Python backtesting provider
 *
 * Runs a Python provider script with given command and arguments.
 */
#[tauri::command]
pub async fn execute_python_backtest(
    provider: String,
    command: String,
    args: String,
) -> Result<String, String> {
    // Determine Python path (bundled or system)
    let python_path = get_python_path();

    // Build script path
    let script_path = format!(
        "resources/scripts/Analytics/backtesting/{}/{}",
        provider.to_lowercase(),
        format!("{}_provider.py", provider.to_lowercase())
    );

    // Execute Python script
    let output = Command::new(&python_path)
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

/**
 * Get Python executable path
 *
 * Returns path to bundled Python or system Python.
 */
fn get_python_path() -> String {
    // Check for bundled Python first
    #[cfg(target_os = "windows")]
    {
        let bundled_path = "resources/python/python.exe";
        if std::path::Path::new(bundled_path).exists() {
            return bundled_path.to_string();
        }
        "python".to_string()
    }

    #[cfg(target_os = "macos")]
    {
        let bundled_path = "resources/python/bin/python3";
        if std::path::Path::new(bundled_path).exists() {
            return bundled_path.to_string();
        }
        "python3".to_string()
    }

    #[cfg(target_os = "linux")]
    {
        let bundled_path = "resources/python/bin/python3";
        if std::path::Path::new(bundled_path).exists() {
            return bundled_path.to_string();
        }
        "python3".to_string()
    }
}
