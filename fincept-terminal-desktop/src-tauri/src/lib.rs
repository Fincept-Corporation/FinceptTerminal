// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

use std::collections::HashMap;
use std::process::{Child, Command, Stdio};
use std::sync::Mutex;
use std::io::{BufRead, BufReader, Write};
use serde::Serialize;
use sha2::{Sha256, Digest};

// Data sources and commands modules
mod data_sources;
mod commands;

// Global state to manage MCP server processes
struct MCPState {
    processes: Mutex<HashMap<String, Child>>,
}

#[derive(Debug, Serialize)]
struct SpawnResult {
    pid: u32,
    success: bool,
    error: Option<String>,
}

#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

// Spawn an MCP server process
#[tauri::command]
fn spawn_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
    command: String,
    args: Vec<String>,
    env: HashMap<String, String>,
) -> Result<SpawnResult, String> {
    println!("[Tauri] Spawning MCP server: {} with command: {} {:?}", server_id, command, args);

    // Build command
    let mut cmd = Command::new(&command);
    cmd.args(&args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    // Add environment variables
    for (key, value) in env {
        cmd.env(key, value);
    }

    // Spawn process
    match cmd.spawn() {
        Ok(child) => {
            let pid = child.id();
            println!("[Tauri] MCP server spawned successfully with PID: {}", pid);

            // Store process
            let mut processes = state.processes.lock().unwrap();
            processes.insert(server_id.clone(), child);

            Ok(SpawnResult {
                pid,
                success: true,
                error: None,
            })
        }
        Err(e) => {
            eprintln!("[Tauri] Failed to spawn MCP server: {}", e);
            Ok(SpawnResult {
                pid: 0,
                success: false,
                error: Some(format!("Failed to spawn process: {}", e)),
            })
        }
    }
}

// Send JSON-RPC request to MCP server
#[tauri::command]
fn send_mcp_request(
    state: tauri::State<MCPState>,
    server_id: String,
    request: String,
) -> Result<String, String> {
    println!("[Tauri] Sending request to server {}: {}", server_id, request);

    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        // Get stdin handle
        if let Some(ref mut stdin) = child.stdin {
            // Write request
            if let Err(e) = writeln!(stdin, "{}", request) {
                return Err(format!("Failed to write to stdin: {}", e));
            }

            if let Err(e) = stdin.flush() {
                return Err(format!("Failed to flush stdin: {}", e));
            }

            // Read response from stdout
            if let Some(ref mut stdout) = child.stdout {
                let reader = BufReader::new(stdout);
                if let Some(Ok(line)) = reader.lines().next() {
                    println!("[Tauri] Received response: {}", line);
                    return Ok(line);
                } else {
                    return Err("No response from server".to_string());
                }
            } else {
                return Err("No stdout available".to_string());
            }
        } else {
            return Err("No stdin available".to_string());
        }
    } else {
        return Err(format!("Server {} not found", server_id));
    }
}

// Send notification (fire and forget)
#[tauri::command]
fn send_mcp_notification(
    state: tauri::State<MCPState>,
    server_id: String,
    notification: String,
) -> Result<(), String> {
    println!("[Tauri] Sending notification to server {}: {}", server_id, notification);

    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        if let Some(ref mut stdin) = child.stdin {
            writeln!(stdin, "{}", notification)
                .map_err(|e| format!("Failed to write notification: {}", e))?;
            stdin.flush()
                .map_err(|e| format!("Failed to flush: {}", e))?;
            Ok(())
        } else {
            Err("No stdin available".to_string())
        }
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

// Ping MCP server to check if alive
#[tauri::command]
fn ping_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<bool, String> {
    let mut processes = state.processes.lock().unwrap();

    if let Some(child) = processes.get_mut(&server_id) {
        // Check if process is still running
        match child.try_wait() {
            Ok(Some(_)) => Ok(false), // Process has exited
            Ok(None) => Ok(true),      // Process is still running
            Err(_) => Ok(false),       // Error checking status
        }
    } else {
        Ok(false) // Server not found
    }
}

// Kill MCP server
#[tauri::command]
fn kill_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<(), String> {
    println!("[Tauri] Killing MCP server: {}", server_id);

    let mut processes = state.processes.lock().unwrap();

    if let Some(mut child) = processes.remove(&server_id) {
        match child.kill() {
            Ok(_) => {
                println!("[Tauri] Server {} killed successfully", server_id);
                Ok(())
            }
            Err(e) => Err(format!("Failed to kill server: {}", e)),
        }
    } else {
        Ok(()) // Server not found, consider it killed
    }
}

// SHA256 hash for Fyers authentication
#[tauri::command]
fn sha256_hash(input: String) -> String {
    let mut hasher = Sha256::new();
    hasher.update(input.as_bytes());
    let result = hasher.finalize();
    format!("{:x}", result)
}

// Execute Python script with arguments and environment variables
#[tauri::command]
fn execute_python_script(
    script_name: String,
    args: Vec<String>,
    env: Option<HashMap<String, String>>,
) -> Result<String, String> {
    println!("[Tauri] Executing Python script: {} with args: {:?}", script_name, args);

    let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("resources")
        .join("python")
        .join("python.exe");

    let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("resources")
        .join("scripts")
        .join(&script_name);

    let mut cmd = Command::new(&python_path);
    cmd.arg(&script_path).args(&args);

    // Add environment variables if provided
    if let Some(env_vars) = env {
        for (key, value) in env_vars {
            cmd.env(key, value);
        }
    }

    match cmd.output() {
        Ok(output) => {
            if output.status.success() {
                String::from_utf8(output.stdout)
                    .map_err(|e| format!("Failed to parse output: {}", e))
            } else {
                let error = String::from_utf8_lossy(&output.stderr);
                Err(format!("Python script failed: {}", error))
            }
        }
        Err(e) => Err(format!("Failed to execute Python script: {}", e)),
    }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_cors_fetch::init())
        .plugin(tauri_plugin_sql::Builder::default().build())
        .manage(MCPState {
            processes: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![
            greet,
            spawn_mcp_server,
            send_mcp_request,
            send_mcp_notification,
            ping_mcp_server,
            kill_mcp_server,
            sha256_hash,
            execute_python_script,
            commands::market_data::get_market_quote,
            commands::market_data::get_market_quotes,
            commands::market_data::get_period_returns,
            commands::market_data::check_market_data_health,
            commands::market_data::get_historical_data,
            commands::market_data::get_stock_info,
            commands::market_data::get_financials
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
