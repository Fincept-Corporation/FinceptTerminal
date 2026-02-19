// mcp_bridge.rs - Local HTTP bridge between Python finagent_core and TypeScript MCP tools
//
// Architecture:
//   Python subprocess → POST /tool { id, tool, args }
//     → emit Tauri event "mcp-tool-call" to TypeScript
//     → TypeScript calls mcpToolService.executeToolDirect() + invoke('register_mcp_tool_result')
//     → HTTP response { success, data, error } back to Python
//
// The server binds to 127.0.0.1:0 (random port) and runs as a singleton for the app lifetime.

use dashmap::DashMap;
use once_cell::sync::Lazy;
use std::io::{BufRead, BufReader, Write};
use std::net::TcpListener;
use std::sync::atomic::{AtomicU16, Ordering};
use std::sync::Arc;
use tauri::{AppHandle, Emitter};
use tokio::sync::oneshot;

// Global port (0 = not started)
static BRIDGE_PORT: AtomicU16 = AtomicU16::new(0);

// Pending tool calls: id → oneshot sender for the result JSON string
static PENDING_CALLS: Lazy<Arc<DashMap<String, oneshot::Sender<String>>>> =
    Lazy::new(|| Arc::new(DashMap::new()));

/// Returns the bridge port. Starts the bridge if not already running.
pub fn get_or_start_bridge(app: AppHandle) -> u16 {
    let port = BRIDGE_PORT.load(Ordering::Relaxed);
    if port != 0 {
        return port;
    }
    start_bridge(app)
}

/// Starts the HTTP bridge server. Returns the bound port.
/// Safe to call multiple times — only starts once.
fn start_bridge(app: AppHandle) -> u16 {
    // Bind to random available port on loopback only
    let listener = TcpListener::bind("127.0.0.1:0").expect("Failed to bind MCP bridge");
    let port = listener.local_addr().expect("No local addr").port();

    // Store port globally
    BRIDGE_PORT.store(port, Ordering::Relaxed);

    eprintln!("[MCPBridge] Started on http://127.0.0.1:{}", port);

    let pending = Arc::clone(&*PENDING_CALLS);

    // Spawn a background OS thread (not tokio) for blocking accept loop
    std::thread::spawn(move || {
        for stream in listener.incoming() {
            match stream {
                Ok(mut stream) => {
                    let pending = Arc::clone(&pending);
                    let app = app.clone();
                    std::thread::spawn(move || {
                        handle_connection(&mut stream, pending, app);
                    });
                }
                Err(e) => {
                    eprintln!("[MCPBridge] Accept error: {}", e);
                }
            }
        }
    });

    port
}

/// Reads and handles a single HTTP request from a TCP stream.
fn handle_connection(
    stream: &mut std::net::TcpStream,
    pending: Arc<DashMap<String, oneshot::Sender<String>>>,
    app: AppHandle,
) {
    // Read HTTP request line + headers
    let mut reader = BufReader::new(stream.try_clone().expect("clone stream"));
    let mut request_line = String::new();
    if reader.read_line(&mut request_line).is_err() {
        return;
    }
    let request_line = request_line.trim().to_string();

    // Read headers to find Content-Length
    let mut content_length: usize = 0;
    loop {
        let mut header = String::new();
        if reader.read_line(&mut header).is_err() {
            break;
        }
        let header = header.trim();
        if header.is_empty() {
            break; // End of headers
        }
        let lower = header.to_lowercase();
        if lower.starts_with("content-length:") {
            if let Ok(n) = lower["content-length:".len()..].trim().parse::<usize>() {
                content_length = n;
            }
        }
    }

    // Parse method + path from request line: "POST /tool HTTP/1.1"
    let parts: Vec<&str> = request_line.splitn(3, ' ').collect();
    if parts.len() < 2 {
        write_response(stream, 400, "Bad Request", b"Bad request line");
        return;
    }
    let method = parts[0];
    let path = parts[1];

    // Read body
    let mut body = vec![0u8; content_length];
    if content_length > 0 {
        use std::io::Read;
        if reader.read_exact(&mut body).is_err() {
            write_response(stream, 400, "Bad Request", b"Failed to read body");
            return;
        }
    }

    match (method, path) {
        ("POST", "/tool") => handle_tool_call(stream, &body, pending, app),
        ("GET", "/health") => write_response(stream, 200, "OK", b"{\"status\":\"ok\"}"),
        ("OPTIONS", _) => write_response(stream, 200, "OK", b""),
        _ => write_response(stream, 404, "Not Found", b"{\"error\":\"not found\"}"),
    }
}

/// Handles POST /tool: { "id": "uuid", "tool": "get_quote", "args": {...} }
fn handle_tool_call(
    stream: &mut std::net::TcpStream,
    body: &[u8],
    pending: Arc<DashMap<String, oneshot::Sender<String>>>,
    app: AppHandle,
) {
    // Parse body as JSON
    let body_json: serde_json::Value = match serde_json::from_slice(body) {
        Ok(v) => v,
        Err(e) => {
            let msg = format!("{{\"error\":\"invalid JSON: {}\"}}", e);
            write_response(stream, 400, "Bad Request", msg.as_bytes());
            return;
        }
    };

    let id = match body_json.get("id").and_then(|v| v.as_str()) {
        Some(s) => s.to_string(),
        None => {
            write_response(stream, 400, "Bad Request", b"{\"error\":\"missing id\"}");
            return;
        }
    };

    let tool_name = match body_json.get("tool").and_then(|v| v.as_str()) {
        Some(s) => s.to_string(),
        None => {
            write_response(stream, 400, "Bad Request", b"{\"error\":\"missing tool\"}");
            return;
        }
    };

    let args = body_json.get("args").cloned().unwrap_or(serde_json::json!({}));

    eprintln!("[MCPBridge] Tool call: id={} tool={}", id, tool_name);

    // Create oneshot channel — we'll block this thread until TypeScript responds
    let (tx, rx) = oneshot::channel::<String>();
    pending.insert(id.clone(), tx);

    // Emit Tauri event to TypeScript
    let event_payload = serde_json::json!({
        "id": id,
        "tool_name": tool_name,
        "args": args,
    });
    if let Err(e) = app.emit("mcp-tool-call", event_payload) {
        eprintln!("[MCPBridge] Failed to emit mcp-tool-call: {}", e);
        pending.remove(&id);
        write_response(stream, 500, "Internal Server Error", b"{\"error\":\"emit failed\"}");
        return;
    }

    // Wait for TypeScript to call register_mcp_tool_result (max 90 seconds).
    // Economics tools (CEIC, central bank rates) can take 15-30s each.
    // We're on a std thread (not a tokio task), so we create a minimal runtime.
    let result = {
        let rt = tokio::runtime::Builder::new_current_thread()
            .enable_all()
            .build()
            .expect("mini runtime for MCP bridge");
        rt.block_on(async {
            tokio::time::timeout(std::time::Duration::from_secs(90), rx).await
        })
    };

    pending.remove(&id);

    match result {
        Ok(Ok(response_str)) => {
            write_response(stream, 200, "OK", response_str.as_bytes());
        }
        Ok(Err(_)) => {
            // Sender dropped
            write_response(stream, 500, "Internal Server Error", b"{\"error\":\"result channel closed\"}");
        }
        Err(_) => {
            // Timeout
            eprintln!("[MCPBridge] Tool call timeout: id={} tool={}", id, tool_name);
            write_response(
                stream,
                504,
                "Gateway Timeout",
                b"{\"error\":\"tool execution timed out\"}",
            );
        }
    }
}

/// Called by the `register_mcp_tool_result` Tauri command when TypeScript has executed a tool.
pub fn deliver_tool_result(id: &str, result: String) -> bool {
    if let Some((_, tx)) = PENDING_CALLS.remove(id) {
        let _ = tx.send(result);
        true
    } else {
        eprintln!("[MCPBridge] No pending call for id={}", id);
        false
    }
}

/// Writes a minimal HTTP response to the stream.
fn write_response(stream: &mut std::net::TcpStream, status: u16, reason: &str, body: &[u8]) {
    let response = format!(
        "HTTP/1.1 {} {}\r\nContent-Type: application/json\r\nContent-Length: {}\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n",
        status,
        reason,
        body.len()
    );
    let _ = stream.write_all(response.as_bytes());
    let _ = stream.write_all(body);
}
