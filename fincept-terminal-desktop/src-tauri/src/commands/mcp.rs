// MCP (Model Context Protocol) server process management commands.
// Handles spawning, communicating with, and killing MCP server subprocesses.

use std::collections::HashMap;
use std::io::{BufRead, BufReader, Write};
use std::process::{Child, ChildStdin, Command, Stdio};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;

use serde::Serialize;

// Windows-specific: hide console windows spawned for MCP servers
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

// ============================================================================
// TYPES
// ============================================================================

pub(crate) struct MCPProcess {
    pub child: Child,
    pub stdin: Arc<Mutex<ChildStdin>>,
    pub response_rx: Receiver<String>,
}

pub struct MCPState {
    pub processes: Mutex<HashMap<String, MCPProcess>>,
}

#[derive(Debug, Serialize)]
pub struct SpawnResult {
    pub pid: u32,
    pub success: bool,
    pub error: Option<String>,
}

// ============================================================================
// COMMANDS
// ============================================================================

/// Spawn an MCP server process with background stdout/stderr readers.
#[tauri::command]
pub fn spawn_mcp_server(
    app: tauri::AppHandle,
    state: tauri::State<MCPState>,
    server_id: String,
    command: String,
    args: Vec<String>,
    env: HashMap<String, String>,
) -> Result<SpawnResult, String> {
    // Resolve the actual command (handle npx/bunx → bundled Bun, Windows .exe, etc.)
    let (fixed_command, fixed_args) = if command == "npx" || command == "bunx" {
        match crate::python::get_bun_path(&app) {
            Ok(bun_path) => {
                let mut new_args = vec!["x".to_string()];
                new_args.extend(args.clone());
                (bun_path.to_string_lossy().to_string(), new_args)
            }
            Err(_) => {
                #[cfg(target_os = "windows")]
                let cmd = "npx.cmd".to_string();
                #[cfg(not(target_os = "windows"))]
                let cmd = "npx".to_string();
                (cmd, args.clone())
            }
        }
    } else if command == "uvx" {
        #[cfg(target_os = "windows")]
        let cmd = "uvx.exe".to_string();
        #[cfg(not(target_os = "windows"))]
        let cmd = "uvx".to_string();
        (cmd, args.clone())
    } else {
        #[cfg(target_os = "windows")]
        let cmd = if command == "node" {
            "node.exe".to_string()
        } else if command == "python" {
            "python.exe".to_string()
        } else {
            command.clone()
        };
        #[cfg(not(target_os = "windows"))]
        let cmd = command.clone();
        (cmd, args.clone())
    };

    let mut cmd = Command::new(&fixed_command);
    cmd.args(&fixed_args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    for (key, value) in env {
        cmd.env(key, value);
    }

    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    match cmd.spawn() {
        Ok(mut child) => {
            let pid = child.id();
            let stdin = child.stdin.take().ok_or("Failed to get stdin")?;
            let stdout = child.stdout.take().ok_or("Failed to get stdout")?;
            let stderr = child.stderr.take();

            let (response_tx, response_rx): (Sender<String>, Receiver<String>) = channel();

            thread::spawn(move || {
                let reader = BufReader::new(stdout);
                for line in reader.lines() {
                    match line {
                        Ok(content) if !content.trim().is_empty() => {
                            if response_tx.send(content).is_err() {
                                break;
                            }
                        }
                        Err(_) => break,
                        _ => {}
                    }
                }
            });

            if let Some(stderr) = stderr {
                thread::spawn(move || {
                    let reader = BufReader::new(stderr);
                    for line in reader.lines() {
                        if let Ok(content) = line {
                            if !content.trim().is_empty() {
                                eprintln!("[MCP] {}", content);
                            }
                        }
                    }
                });
            }

            let mcp_process = MCPProcess {
                child,
                stdin: Arc::new(Mutex::new(stdin)),
                response_rx,
            };

            state
                .processes
                .lock()
                .map_err(|e| format!("Failed to lock MCP state: {}", e))?
                .insert(server_id, mcp_process);

            Ok(SpawnResult { pid, success: true, error: None })
        }
        Err(e) => {
            eprintln!("[MCP] Failed to spawn server: {}", e);
            Ok(SpawnResult {
                pid: 0,
                success: false,
                error: Some(format!("Failed to spawn process: {}", e)),
            })
        }
    }
}

/// Send a JSON-RPC request to an MCP server and wait for a response.
#[tauri::command]
pub fn send_mcp_request(
    state: tauri::State<MCPState>,
    server_id: String,
    request: String,
    timeout_secs: Option<u64>,
) -> Result<String, String> {
    let timeout = timeout_secs.unwrap_or(30);

    let mut processes = state
        .processes
        .lock()
        .map_err(|e| format!("Failed to lock MCP state: {}", e))?;

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        {
            let mut stdin = mcp_process
                .stdin
                .lock()
                .map_err(|e| format!("Failed to lock stdin: {}", e))?;
            writeln!(stdin, "{}", request)
                .map_err(|e| format!("Failed to write to stdin: {}", e))?;
            stdin
                .flush()
                .map_err(|e| format!("Failed to flush stdin: {}", e))?;
        }

        match mcp_process
            .response_rx
            .recv_timeout(Duration::from_secs(timeout))
        {
            Ok(response) => Ok(response),
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                Err(format!("Timeout: No response from server within {}s", timeout))
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                Err("Server process has terminated unexpectedly".to_string())
            }
        }
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

/// Send a fire-and-forget JSON-RPC notification to an MCP server.
#[tauri::command]
pub fn send_mcp_notification(
    state: tauri::State<MCPState>,
    server_id: String,
    notification: String,
) -> Result<(), String> {
    let mut processes = state
        .processes
        .lock()
        .map_err(|e| format!("Failed to lock MCP state: {}", e))?;

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        let mut stdin = mcp_process
            .stdin
            .lock()
            .map_err(|e| format!("Failed to lock stdin: {}", e))?;
        writeln!(stdin, "{}", notification)
            .map_err(|e| format!("Failed to write notification: {}", e))?;
        stdin
            .flush()
            .map_err(|e| format!("Failed to flush: {}", e))?;
        Ok(())
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

/// Check whether an MCP server process is still alive.
#[tauri::command]
pub fn ping_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<bool, String> {
    let mut processes = state
        .processes
        .lock()
        .map_err(|e| format!("Failed to lock MCP state: {}", e))?;

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        match mcp_process.child.try_wait() {
            Ok(Some(_)) => Ok(false), // exited
            Ok(None) => Ok(true),     // still running
            Err(_) => Ok(false),
        }
    } else {
        Ok(false)
    }
}

/// Kill an MCP server process and remove it from the registry.
#[tauri::command]
pub fn kill_mcp_server(
    state: tauri::State<MCPState>,
    server_id: String,
) -> Result<(), String> {
    let mut processes = state
        .processes
        .lock()
        .map_err(|e| format!("Failed to lock MCP state: {}", e))?;

    if let Some(mut mcp_process) = processes.remove(&server_id) {
        match mcp_process.child.kill() {
            Ok(_) => {
                let _ = mcp_process.child.wait();
                Ok(())
            }
            Err(e) => Err(format!("Failed to kill server: {}", e)),
        }
    } else {
        Ok(()) // not found → treat as already killed
    }
}
