// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/

use std::collections::HashMap;
use std::process::{Child, Command, Stdio, ChildStdin};
use std::sync::{Arc, Mutex};
use std::io::{BufRead, BufReader, Write};
use std::thread;
use std::time::Duration;
use std::sync::mpsc::{channel, Sender, Receiver};
use serde::Serialize;
use sha2::{Sha256, Digest};
use tauri::{Manager, Listener};

// Data sources and commands modules
mod data_sources;
mod commands;
mod utils;
mod setup;
mod database;
mod python;
mod websocket;
mod services;
mod paper_trading;
mod market_sim;

// MCP Server Process with communication channels
struct MCPProcess {
    child: Child,
    stdin: Arc<Mutex<ChildStdin>>,
    response_rx: Receiver<String>,
}

// Global state to manage MCP server processes
struct MCPState {
    processes: Mutex<HashMap<String, MCPProcess>>,
}

// Global state for WebSocket manager
pub struct WebSocketState {
    pub manager: Arc<tokio::sync::RwLock<websocket::WebSocketManager>>,
    pub router: Arc<tokio::sync::RwLock<websocket::MessageRouter>>,
    pub services: Arc<tokio::sync::RwLock<WebSocketServices>>,
}

#[allow(dead_code)]
struct WebSocketServices {
    paper_trading: websocket::services::PaperTradingService,
    arbitrage: websocket::services::ArbitrageService,
    portfolio: websocket::services::PortfolioService,
    monitoring: websocket::services::MonitoringService,
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

// Cleanup workflow on app close (called from frontend)
#[tauri::command]
async fn cleanup_running_workflows() -> Result<(), String> {
    // This is just a marker command - the actual cleanup happens in the frontend
    // via workflowService.cleanupRunningWorkflows()
    Ok(())
}

// Spawn an MCP server process with background stdout reader
#[tauri::command]
fn spawn_mcp_server(
    app: tauri::AppHandle,
    state: tauri::State<MCPState>,
    server_id: String,
    command: String,
    args: Vec<String>,
    env: HashMap<String, String>,
) -> Result<SpawnResult, String> {
    // Determine if we should use bundled Bun (for npx/bunx commands)
    let (fixed_command, fixed_args) = if command == "npx" || command == "bunx" {
        // Try to get bundled Bun path
        match python::get_bun_path(&app) {
            Ok(bun_path) => {
                // Use 'bun x' which is equivalent to 'bunx' or 'npx'
                let mut new_args = vec!["x".to_string()];
                new_args.extend(args.clone());
                (bun_path.to_string_lossy().to_string(), new_args)
            }
            Err(_) => {
                // Fall back to system npx
                #[cfg(target_os = "windows")]
                let cmd = "npx.cmd".to_string();
                #[cfg(not(target_os = "windows"))]
                let cmd = "npx".to_string();
                (cmd, args.clone())
            }
        }
    } else {
        // Fix command for Windows - node/python need .exe extension
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

    // Build command
    let mut cmd = Command::new(&fixed_command);
    cmd.args(&fixed_args)
        .stdin(Stdio::piped())
        .stdout(Stdio::piped())
        .stderr(Stdio::piped());

    // Add environment variables
    for (key, value) in env {
        cmd.env(key, value);
    }

    // Hide console window on Windows
    #[cfg(target_os = "windows")]
    cmd.creation_flags(CREATE_NO_WINDOW);

    // Spawn process
    match cmd.spawn() {
        Ok(mut child) => {
            let pid = child.id();

            // Extract stdin and stdout
            let stdin = child.stdin.take().ok_or("Failed to get stdin")?;
            let stdout = child.stdout.take().ok_or("Failed to get stdout")?;
            let stderr = child.stderr.take();

            // Create channel for responses
            let (response_tx, response_rx): (Sender<String>, Receiver<String>) = channel();

            // Spawn background thread to read stdout
            thread::spawn(move || {
                let reader = BufReader::new(stdout);

                for line in reader.lines() {
                    match line {
                        Ok(content) => {
                            if !content.trim().is_empty() {
                                if response_tx.send(content).is_err() {
                                    break;
                                }
                            }
                        }
                        Err(_) => {
                            break;
                        }
                    }
                }
            });

            // Spawn background thread to read stderr (for debugging)
            if let Some(stderr) = stderr {
                let _server_id_clone = server_id.clone();
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

            // Store process with communication channels
            let mcp_process = MCPProcess {
                child,
                stdin: Arc::new(Mutex::new(stdin)),
                response_rx,
            };

            let mut processes = state.processes.lock().unwrap();
            processes.insert(server_id.clone(), mcp_process);

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

// Send JSON-RPC request to MCP server with timeout
#[tauri::command]
fn send_mcp_request(
    state: tauri::State<MCPState>,
    server_id: String,
    request: String,
) -> Result<String, String> {
    println!("[Tauri] Sending request to server {}: {}", server_id, request);

    let mut processes = state.processes.lock().unwrap();

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        // Write request to stdin
        {
            let mut stdin = mcp_process.stdin.lock().unwrap();
            writeln!(stdin, "{}", request)
                .map_err(|e| format!("Failed to write to stdin: {}", e))?;
            stdin.flush()
                .map_err(|e| format!("Failed to flush stdin: {}", e))?;
        }

        // Wait for response with timeout (30 seconds for initial package download)
        match mcp_process.response_rx.recv_timeout(Duration::from_secs(30)) {
            Ok(response) => {
                Ok(response)
            }
            Err(std::sync::mpsc::RecvTimeoutError::Timeout) => {
                Err("Timeout: No response from server within 30 seconds".to_string())
            }
            Err(std::sync::mpsc::RecvTimeoutError::Disconnected) => {
                Err("Server process has terminated unexpectedly".to_string())
            }
        }
    } else {
        Err(format!("Server {} not found", server_id))
    }
}

// Send notification (fire and forget)
#[tauri::command]
fn send_mcp_notification(
    state: tauri::State<MCPState>,
    server_id: String,
    notification: String,
) -> Result<(), String> {
    let mut processes = state.processes.lock().unwrap();

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        let mut stdin = mcp_process.stdin.lock().unwrap();
        writeln!(stdin, "{}", notification)
            .map_err(|e| format!("Failed to write notification: {}", e))?;
        stdin.flush()
            .map_err(|e| format!("Failed to flush: {}", e))?;
        Ok(())
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

    if let Some(mcp_process) = processes.get_mut(&server_id) {
        // Check if process is still running
        match mcp_process.child.try_wait() {
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
    let mut processes = state.processes.lock().unwrap();

    if let Some(mut mcp_process) = processes.remove(&server_id) {
        match mcp_process.child.kill() {
            Ok(_) => {
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

// ============================================================================
// WEBSOCKET COMMANDS
// ============================================================================

/// Set WebSocket provider configuration
#[tauri::command]
async fn ws_set_config(
    state: tauri::State<'_, WebSocketState>,
    config: websocket::types::ProviderConfig,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.set_config(config.clone());
    Ok(())
}

/// Connect to WebSocket provider
#[tauri::command]
async fn ws_connect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    eprintln!("[ws_connect] Called for provider: {}", provider);

    let manager = state.manager.read().await;
    let result = manager.connect(&provider).await
        .map_err(|e| e.to_string());

    match &result {
        Ok(_) => eprintln!("[ws_connect] ✓ Successfully connected to {}", provider),
        Err(e) => eprintln!("[ws_connect] ✗ Failed to connect to {}: {}", provider, e),
    }

    result
}

/// Disconnect from WebSocket provider
#[tauri::command]
async fn ws_disconnect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.disconnect(&provider).await
        .map_err(|e| e.to_string())
}

/// Subscribe to WebSocket channel
#[tauri::command]
async fn ws_subscribe(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
    symbol: String,
    channel: String,
    params: Option<serde_json::Value>,
) -> Result<(), String> {
    eprintln!("[ws_subscribe] Called: provider={}, symbol={}, channel={}", provider, symbol, channel);

    // Register frontend subscriber
    let topic = format!("{}.{}.{}", provider, channel, symbol);
    eprintln!("[ws_subscribe] Registering frontend subscriber for topic: {}", topic);
    state.router.write().await.subscribe_frontend(&topic);

    // Subscribe via manager
    eprintln!("[ws_subscribe] Calling manager.subscribe...");
    let manager = state.manager.read().await;
    let result = manager.subscribe(&provider, &symbol, &channel, params).await
        .map_err(|e| e.to_string());

    match &result {
        Ok(_) => eprintln!("[ws_subscribe] ✓ Successfully subscribed to {} {} {}", provider, symbol, channel),
        Err(e) => eprintln!("[ws_subscribe] ✗ Failed to subscribe: {}", e),
    }

    result
}

/// Unsubscribe from WebSocket channel
#[tauri::command]
async fn ws_unsubscribe(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
    symbol: String,
    channel: String,
) -> Result<(), String> {
    // Unregister frontend subscriber
    state.router.write().await.unsubscribe_frontend(&format!("{}.{}.{}", provider, channel, symbol));

    // Unsubscribe via manager
    let manager = state.manager.read().await;
    manager.unsubscribe(&provider, &symbol, &channel).await
        .map_err(|e| e.to_string())
}

/// Get connection metrics for a provider
#[tauri::command]
async fn ws_get_metrics(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<Option<websocket::types::ConnectionMetrics>, String> {
    let manager = state.manager.read().await;
    Ok(manager.get_metrics(&provider))
}

/// Get all connection metrics
#[tauri::command]
async fn ws_get_all_metrics(
    state: tauri::State<'_, WebSocketState>,
) -> Result<Vec<websocket::types::ConnectionMetrics>, String> {
    let manager = state.manager.read().await;
    Ok(manager.get_all_metrics())
}

/// Reconnect to provider
#[tauri::command]
async fn ws_reconnect(
    state: tauri::State<'_, WebSocketState>,
    provider: String,
) -> Result<(), String> {
    let manager = state.manager.read().await;
    manager.reconnect(&provider).await
        .map_err(|e| e.to_string())
}

// ============================================================================
// MONITORING COMMANDS
// ============================================================================

/// Add monitoring condition
#[tauri::command]
async fn monitor_add_condition(
    _app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    condition: websocket::services::monitoring::MonitorCondition,
) -> Result<i64, String> {
    use rusqlite::params;

    let pool = database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    conn.execute(
        "INSERT INTO monitor_conditions (provider, symbol, field, operator, value, value2, enabled)
         VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)",
        params![
            &condition.provider,
            &condition.symbol,
            match condition.field {
                websocket::services::monitoring::MonitorField::Price => "price",
                websocket::services::monitoring::MonitorField::Volume => "volume",
                websocket::services::monitoring::MonitorField::ChangePercent => "change_percent",
                websocket::services::monitoring::MonitorField::Spread => "spread",
            },
            match condition.operator {
                websocket::services::monitoring::MonitorOperator::GreaterThan => ">",
                websocket::services::monitoring::MonitorOperator::LessThan => "<",
                websocket::services::monitoring::MonitorOperator::GreaterThanOrEqual => ">=",
                websocket::services::monitoring::MonitorOperator::LessThanOrEqual => "<=",
                websocket::services::monitoring::MonitorOperator::Equal => "==",
                websocket::services::monitoring::MonitorOperator::Between => "between",
            },
            condition.value,
            condition.value2,
            if condition.enabled { 1 } else { 0 },
        ],
    ).map_err(|e| e.to_string())?;

    let id = conn.last_insert_rowid();

    // Reload conditions
    let services = state.services.read().await;
    let _ = services.monitoring.load_conditions().await;

    Ok(id)
}

/// Get all monitoring conditions
#[tauri::command]
async fn monitor_get_conditions(
    _app: tauri::AppHandle,
) -> Result<Vec<websocket::services::monitoring::MonitorCondition>, String> {
    let pool = database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let mut stmt = conn.prepare(
        "SELECT id, provider, symbol, field, operator, value, value2, enabled
         FROM monitor_conditions
         ORDER BY created_at DESC"
    ).map_err(|e| e.to_string())?;

    let conditions = stmt
        .query_map([], |row| {
            Ok(websocket::services::monitoring::MonitorCondition {
                id: Some(row.get(0)?),
                provider: row.get(1)?,
                symbol: row.get(2)?,
                field: websocket::services::monitoring::MonitorField::from_str(&row.get::<_, String>(3)?).unwrap(),
                operator: websocket::services::monitoring::MonitorOperator::from_str(&row.get::<_, String>(4)?).unwrap(),
                value: row.get(5)?,
                value2: row.get(6)?,
                enabled: row.get::<_, i32>(7)? == 1,
            })
        })
        .map_err(|e| e.to_string())?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|e| e.to_string())?;

    Ok(conditions)
}

/// Delete monitoring condition
#[tauri::command]
async fn monitor_delete_condition(
    _app: tauri::AppHandle,
    state: tauri::State<'_, WebSocketState>,
    id: i64,
) -> Result<(), String> {
    use rusqlite::params;

    let pool = database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    conn.execute("DELETE FROM monitor_conditions WHERE id = ?1", params![id])
        .map_err(|e| e.to_string())?;

    // Reload conditions
    let services = state.services.read().await;
    services.monitoring.load_conditions().await.map_err(|e| e.to_string())?;

    Ok(())
}

/// Get recent alerts
#[tauri::command]
async fn monitor_get_alerts(
    _app: tauri::AppHandle,
    limit: i64,
) -> Result<Vec<websocket::services::monitoring::MonitorAlert>, String> {
    use rusqlite::params;

    let pool = database::pool::get_pool().map_err(|e| e.to_string())?;
    let conn = pool.get().map_err(|e| e.to_string())?;

    let mut stmt = conn.prepare(
        "SELECT id, condition_id, provider, symbol, field, triggered_value, triggered_at
         FROM monitor_alerts
         ORDER BY triggered_at DESC
         LIMIT ?1"
    ).map_err(|e| e.to_string())?;

    let alerts = stmt
        .query_map(params![limit], |row| {
            Ok(websocket::services::monitoring::MonitorAlert {
                id: Some(row.get(0)?),
                condition_id: row.get(1)?,
                provider: row.get(2)?,
                symbol: row.get(3)?,
                field: websocket::services::monitoring::MonitorField::from_str(&row.get::<_, String>(4)?).unwrap(),
                triggered_value: row.get(5)?,
                triggered_at: row.get::<_, i64>(6)? as u64,
            })
        })
        .map_err(|e| e.to_string())?
        .collect::<Result<Vec<_>, _>>()
        .map_err(|e| e.to_string())?;

    Ok(alerts)
}

/// Load monitoring conditions on startup
#[tauri::command]
async fn monitor_load_conditions(
    state: tauri::State<'_, WebSocketState>,
) -> Result<(), String> {
    let services = state.services.read().await;
    services.monitoring.load_conditions().await.map_err(|e| e.to_string())
}

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

// Windows creation flags to hide console window
#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

// Execute Python script via worker pool (persistent processes, no subprocess spawn per call)
// Falls back to subprocess if worker pool is not initialized
#[tauri::command]
async fn execute_python_script(
    app: tauri::AppHandle,
    script_name: String,
    args: Vec<String>,
    _env: std::collections::HashMap<String, String>,
) -> Result<String, String> {
    python::execute(&app, &script_name, args).await
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    // Initialize high-performance Rust SQLite database
    // CRITICAL: Database is required for paper trading and other core features
    if let Err(e) = tokio::runtime::Runtime::new().unwrap().block_on(database::initialize()) {
        eprintln!("========================================");
        eprintln!("CRITICAL ERROR: Failed to initialize database!");
        eprintln!("Error: {}", e);
        eprintln!("The application cannot function without the database.");
        eprintln!("Please ensure you have write permissions to:");
        eprintln!("  Windows: %APPDATA%\\fincept-terminal");
        eprintln!("  macOS: ~/Library/Application Support/fincept-terminal");
        eprintln!("  Linux: ~/.local/share/fincept-terminal");
        eprintln!("========================================");
        // Note: We don't panic here to allow the app to show an error UI
        // The frontend will detect database failures via health checks
    }

    // Initialize WebSocket system
    let router = Arc::new(tokio::sync::RwLock::new(websocket::MessageRouter::new()));
    let manager = Arc::new(tokio::sync::RwLock::new(websocket::WebSocketManager::new(router.clone())));

    // Initialize services with default monitoring (will be configured in setup)
    let services = Arc::new(tokio::sync::RwLock::new(WebSocketServices {
        paper_trading: websocket::services::PaperTradingService::new(),
        arbitrage: websocket::services::ArbitrageService::new(),
        portfolio: websocket::services::PortfolioService::new(),
        monitoring: websocket::services::MonitoringService::default(),
    }));

    let ws_state = WebSocketState {
        manager: manager.clone(),
        router: router.clone(),
        services: services.clone(),
    };

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_shell::init())
        .plugin(tauri_plugin_http::init())
        .plugin(tauri_plugin_cors_fetch::init())
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .plugin(tauri_plugin_updater::Builder::new().build())
        .plugin(tauri_plugin_process::init())
        .manage(MCPState {
            processes: Mutex::new(HashMap::new()),
        })
        .manage(ws_state)
        .setup(move |app| {
            // CRITICAL: Set app handle for router to emit WebSocket events to frontend
            let app_handle = app.handle().clone();
            let router_clone = router.clone();
            let services_clone = services.clone();

            // Initialize edgar cache database
            if let Ok(app_data_dir) = app.path().app_data_dir() {
                let edgar_cache_path = app_data_dir.join("edgar_company_cache.db");
                commands::edgar_cache::set_cache_db_path(edgar_cache_path);
            }

            // CRITICAL: Set router app handle SYNCHRONOUSLY before any commands can be called
            // This ensures WebSocket events can be emitted to frontend immediately
            {
                let router_for_handle = router_clone.clone();
                let handle_for_setup = app_handle.clone();
                tauri::async_runtime::block_on(async move {
                    router_for_handle.write().await.set_app_handle(handle_for_setup);
                });
            }

            // Use tauri::async_runtime to spawn task in Tauri's runtime for other async setup
            tauri::async_runtime::spawn(async move {
                // Router app handle already set above synchronously

                // Initialize monitoring service
                let mut services_guard = services_clone.write().await;
                services_guard.monitoring = websocket::services::MonitoringService::new();
                services_guard.monitoring.set_app_handle(app_handle.clone());

                // Subscribe to ticker stream and start monitoring
                let ticker_rx = router_clone.read().await.subscribe_ticker();
                services_guard.monitoring.start_monitoring(ticker_rx);

                // Load existing conditions from database
                let _ = services_guard.monitoring.load_conditions().await;

                drop(services_guard); // Release the lock before listening

                // Listen for Fyers ticker events from frontend
                let router_clone_for_event = router_clone.clone();
                let _ = app_handle.listen("fyers_ticker", move |event: tauri::Event| {
                    if let Ok(payload_str) = serde_json::from_str::<serde_json::Value>(event.payload()) {
                        if let Some(payload) = payload_str.as_object() {
                            // Parse ticker data from frontend event
                            let ticker = websocket::types::TickerData {
                                provider: payload.get("provider").and_then(|v| v.as_str()).unwrap_or("fyers").to_string(),
                                symbol: payload.get("symbol").and_then(|v| v.as_str()).unwrap_or("").to_string(),
                                price: payload.get("price").and_then(|v| v.as_f64()).unwrap_or(0.0),
                                volume: payload.get("volume").and_then(|v| v.as_f64()),
                                bid: payload.get("bid").and_then(|v| v.as_f64()),
                                ask: payload.get("ask").and_then(|v| v.as_f64()),
                                bid_size: payload.get("bid_size").and_then(|v| v.as_f64()),
                                ask_size: payload.get("ask_size").and_then(|v| v.as_f64()),
                                high: payload.get("high").and_then(|v| v.as_f64()),
                                low: payload.get("low").and_then(|v| v.as_f64()),
                                open: payload.get("open").and_then(|v| v.as_f64()),
                                close: payload.get("close").and_then(|v| v.as_f64()),
                                change: payload.get("change").and_then(|v| v.as_f64()),
                                change_percent: payload.get("change_percent").and_then(|v| v.as_f64()),
                                timestamp: payload.get("timestamp").and_then(|v| v.as_u64()).unwrap_or(0),
                            };

                            // Route to monitoring service
                            let router = router_clone_for_event.clone();
                            tauri::async_runtime::spawn(async move {
                                router.read().await.route(websocket::types::MarketMessage::Ticker(ticker)).await;
                            });
                        }
                    }
                });
            });

            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            greet,
            cleanup_running_workflows,
            setup::check_setup_status,
            setup::run_setup,
            spawn_mcp_server,
            send_mcp_request,
            send_mcp_notification,
            ping_mcp_server,
            kill_mcp_server,
            sha256_hash,
            ws_set_config,
            ws_connect,
            ws_disconnect,
            ws_subscribe,
            ws_unsubscribe,
            ws_get_metrics,
            ws_get_all_metrics,
            ws_reconnect,
            monitor_add_condition,
            monitor_get_conditions,
            monitor_delete_condition,
            monitor_get_alerts,
            monitor_load_conditions,
            execute_python_script,
            commands::news::fetch_all_rss_news,
            commands::news::get_rss_feed_count,
            commands::news::get_active_sources,
            commands::news::get_all_rss_feeds,
            commands::news::get_default_feeds,
            commands::news::get_user_rss_feeds,
            commands::news::add_user_rss_feed,
            commands::news::update_user_rss_feed,
            commands::news::delete_user_rss_feed,
            commands::news::toggle_user_rss_feed,
            commands::news::test_rss_feed_url,
            commands::news::toggle_default_rss_feed,
            commands::news::get_default_feed_prefs,
            commands::news::delete_default_rss_feed,
            commands::news::restore_default_rss_feed,
            commands::news::get_deleted_default_feeds,
            commands::news::restore_all_default_feeds,
            commands::market_data::get_market_quote,
            commands::market_data::get_market_quotes,
            commands::market_data::get_period_returns,
            commands::market_data::check_market_data_health,
            commands::market_data::get_historical_data,
            commands::market_data::get_stock_info,
            commands::market_data::get_financials,
            commands::yfinance::execute_yfinance_command,
            commands::edgar::execute_edgar_command,
            commands::edgar_cache::edgar_cache_store_tickers,
            commands::edgar_cache::edgar_cache_search_companies,
            commands::edgar_cache::edgar_cache_get_all_companies,
            commands::edgar_cache::edgar_cache_get_count,
            commands::edgar_cache::edgar_cache_is_populated,
            commands::alphavantage::execute_alphavantage_command,
            commands::alphavantage::get_alphavantage_quote,
            commands::alphavantage::get_alphavantage_daily,
            commands::alphavantage::get_alphavantage_intraday,
            commands::alphavantage::get_alphavantage_overview,
            commands::alphavantage::search_alphavantage_symbols,
            commands::alphavantage::get_alphavantage_comprehensive,
            commands::alphavantage::get_alphavantage_market_movers,
            commands::pmdarima::pmdarima_fit_auto_arima,
            commands::pmdarima::pmdarima_forecast_auto_arima,
            commands::pmdarima::pmdarima_forecast_arima,
            commands::pmdarima::pmdarima_boxcox_transform,
            commands::pmdarima::pmdarima_inverse_boxcox,
            commands::pmdarima::pmdarima_calculate_acf,
            commands::pmdarima::pmdarima_calculate_pacf,
            commands::pmdarima::pmdarima_decompose_timeseries,
            commands::pmdarima::pmdarima_cross_validate,
            commands::government_us::execute_government_us_command,
            commands::government_us::get_treasury_prices,
            commands::government_us::get_treasury_auctions,
            commands::government_us::get_comprehensive_treasury_data,
            commands::government_us::get_treasury_summary,
            commands::congress_gov::execute_congress_gov_command,
            commands::congress_gov::get_congress_bills,
            commands::congress_gov::get_bill_info,
            commands::congress_gov::get_bill_text,
            commands::congress_gov::download_bill_text,
            commands::congress_gov::get_comprehensive_bill_data,
            commands::congress_gov::get_bill_summary_by_congress,
            commands::oecd::execute_oecd_command,
            commands::oecd::get_oecd_gdp_real,
            commands::oecd::get_oecd_consumer_price_index,
            commands::oecd::get_oecd_gdp_forecast,
            commands::oecd::get_oecd_unemployment,
            commands::oecd::get_oecd_economic_summary,
            commands::oecd::get_oecd_country_list,
            commands::imf::execute_imf_command,
            commands::imf::get_imf_economic_indicators,
            commands::imf::get_imf_direction_of_trade,
            commands::imf::get_imf_available_indicators,
            commands::imf::get_imf_comprehensive_economic_data,
            commands::imf::get_imf_reserves_data,
            commands::imf::get_imf_trade_summary,
            commands::fmp::execute_fmp_command,
            commands::fmp::get_fmp_equity_quote,
            commands::fmp::get_fmp_company_profile,
            commands::fmp::get_fmp_historical_prices,
            commands::fmp::get_fmp_income_statement,
            commands::fmp::get_fmp_balance_sheet,
            commands::fmp::get_fmp_cash_flow_statement,
            commands::fmp::get_fmp_financial_ratios,
            commands::fmp::get_fmp_key_metrics,
            commands::fmp::get_fmp_market_snapshots,
            commands::fmp::get_fmp_treasury_rates,
            commands::fmp::get_fmp_etf_info,
            commands::fmp::get_fmp_etf_holdings,
            commands::fmp::get_fmp_crypto_list,
            commands::fmp::get_fmp_crypto_historical,
            commands::fmp::get_fmp_company_news,
            commands::fmp::get_fmp_general_news,
            commands::fmp::get_fmp_economic_calendar,
            commands::fmp::get_fmp_insider_trading,
            commands::fmp::get_fmp_institutional_ownership,
            commands::fmp::get_fmp_company_overview,
            commands::fmp::get_fmp_financial_statements,
            commands::fmp::get_fmp_market_quotes,
            commands::fmp::get_fmp_price_performance,
            commands::fmp::get_fmp_stock_screener,
            commands::fmp::get_fmp_price_targets,
            commands::federal_reserve::execute_federal_reserve_command,
            commands::federal_reserve::get_federal_funds_rate,
            commands::federal_reserve::get_sofr_rate,
            commands::federal_reserve::get_treasury_rates,
            commands::federal_reserve::get_yield_curve,
            commands::federal_reserve::get_money_measures,
            commands::federal_reserve::get_central_bank_holdings,
            commands::federal_reserve::get_overnight_bank_funding_rate,
            commands::federal_reserve::get_comprehensive_monetary_data,
            commands::federal_reserve::get_fed_market_overview,
            commands::sec::execute_sec_command,
            commands::sec::get_sec_company_filings,
            commands::sec::get_sec_cik_map,
            commands::sec::get_sec_symbol_map,
            commands::sec::get_sec_filing_content,
            commands::sec::parse_sec_filing_html,
            commands::sec::get_sec_insider_trading,
            commands::sec::get_sec_institutional_ownership,
            commands::sec::search_sec_companies,
            commands::sec::search_sec_etfs_mutual_funds,
            commands::sec::get_sec_available_form_types,
            commands::sec::get_sec_company_facts,
            commands::sec::get_sec_financial_statements,
            commands::sec::get_sec_company_overview,
            commands::sec::get_sec_filings_by_form_type,
            commands::sec::search_sec_equities,
            commands::sec::search_sec_mutual_funds_etfs,
            commands::sec::get_sec_filing_metadata,
            commands::sec::get_sec_recent_current_reports,
            commands::sec::get_sec_annual_reports,
            commands::sec::get_sec_quarterly_reports,
            commands::sec::get_sec_registration_statements,
            commands::sec::get_sec_beneficial_ownership,
            commands::cftc::execute_cftc_command,
            commands::cftc::get_cftc_cot_data,
            commands::cftc::search_cftc_cot_markets,
            commands::cftc::get_cftc_available_report_types,
            commands::cftc::analyze_cftc_market_sentiment,
            commands::cftc::get_cftc_position_summary,
            commands::cftc::get_cftc_comprehensive_cot_overview,
            commands::cftc::get_cftc_cot_historical_trend,
            commands::cftc::get_cftc_legacy_cot,
            commands::cftc::get_cftc_disaggregated_cot,
            commands::cftc::get_cftc_tff_reports,
            commands::cftc::get_cftc_supplemental_cot,
            commands::cftc::get_cftc_precious_metals_cot,
            commands::cftc::get_cftc_energy_cot,
            commands::cftc::get_cftc_agricultural_cot,
            commands::cftc::get_cftc_financial_cot,
            commands::cftc::get_cftc_crypto_cot,
            commands::cftc::get_cftc_position_changes,
            commands::cftc::get_cftc_extreme_positions,
            commands::cboe::execute_cboe_command,
            commands::cboe::get_cboe_equity_quote,
            commands::cboe::get_cboe_equity_historical,
            commands::cboe::search_cboe_equities,
            commands::cboe::get_cboe_index_constituents,
            commands::cboe::get_cboe_index_historical,
            commands::cboe::search_cboe_indices,
            commands::cboe::get_cboe_index_snapshots,
            commands::cboe::get_cboe_available_indices,
            commands::cboe::get_cboe_futures_curve,
            commands::cboe::get_cboe_options_chains,
            commands::cboe::get_cboe_us_market_overview,
            commands::cboe::get_cboe_european_market_overview,
            commands::cboe::get_cboe_vix_analysis,
            commands::cboe::get_cboe_equity_analysis,
            commands::cboe::get_cboe_index_analysis,
            commands::cboe::get_cboe_market_sentiment,
            commands::cboe::search_cboe_symbols,
            commands::cboe::get_cboe_directory_info,
            commands::bls::execute_bls_command,
            commands::bls::search_bls_series,
            commands::bls::get_bls_series_data,
            commands::bls::get_bls_popular_series,
            commands::bls::get_bls_labor_market_overview,
            commands::bls::get_bls_inflation_overview,
            commands::bls::get_bls_employment_cost_index,
            commands::bls::get_bls_productivity_costs,
            commands::bls::get_bls_survey_categories,
            commands::bls::get_bls_cpi_data,
            commands::bls::get_bls_unemployment_data,
            commands::bls::get_bls_payrolls_data,
            commands::bls::get_bls_participation_data,
            commands::bls::get_bls_ppi_data,
            commands::bls::get_bls_jolts_data,
            commands::bls::get_bls_economic_dashboard,
            commands::bls::get_bls_inflation_analysis,
            commands::bls::get_bls_labor_analysis,
            commands::bls::get_bls_series_comparison,
            commands::bls::get_bls_data_directory,
            commands::nasdaq::execute_nasdaq_command,
            commands::nasdaq::search_nasdaq_equities,
            commands::nasdaq::get_nasdaq_equity_screener,
            commands::nasdaq::get_nasdaq_top_performers,
            commands::nasdaq::get_nasdaq_stocks_by_market_cap,
            commands::nasdaq::get_nasdaq_stocks_by_sector,
            commands::nasdaq::get_nasdaq_stocks_by_exchange,
            commands::nasdaq::search_nasdaq_etfs,
            commands::nasdaq::get_nasdaq_dividend_calendar,
            commands::nasdaq::get_nasdaq_upcoming_dividends,
            commands::nasdaq::get_nasdaq_earnings_calendar,
            commands::nasdaq::get_nasdaq_upcoming_earnings,
            commands::nasdaq::get_nasdaq_ipo_calendar,
            commands::nasdaq::get_nasdaq_upcoming_ipos,
            commands::nasdaq::get_nasdaq_recent_ipos,
            commands::nasdaq::get_nasdaq_filed_ipos,
            commands::nasdaq::get_nasdaq_withdrawn_ipos,
            commands::nasdaq::get_nasdaq_spo_calendar,
            commands::nasdaq::get_nasdaq_economic_calendar,
            commands::nasdaq::get_nasdaq_today_economic_events,
            commands::nasdaq::get_nasdaq_week_economic_events,
            commands::nasdaq::get_nasdaq_top_retail_activity,
            commands::nasdaq::get_nasdaq_retail_sentiment,
            commands::nasdaq::get_nasdaq_market_overview,
            commands::nasdaq::get_nasdaq_dividend_overview,
            commands::nasdaq::get_nasdaq_ipo_overview,
            commands::nasdaq::get_nasdaq_earnings_overview,
            commands::nasdaq::get_nasdaq_sector_analysis,
            commands::nasdaq::get_nasdaq_market_cap_analysis,
            commands::nasdaq::get_nasdaq_exchange_comparison,
            commands::nasdaq::get_nasdaq_date_range_analysis,
            commands::nasdaq::get_nasdaq_directory_info,
            commands::bis::execute_bis_command,
            commands::bis::get_bis_data,
            commands::bis::get_bis_available_constraints,
            commands::bis::get_bis_data_structures,
            commands::bis::get_bis_dataflows,
            commands::bis::get_bis_categorisations,
            commands::bis::get_bis_content_constraints,
            commands::bis::get_bis_actual_constraints,
            commands::bis::get_bis_allowed_constraints,
            commands::bis::get_bis_structures,
            commands::bis::get_bis_concept_schemes,
            commands::bis::get_bis_codelists,
            commands::bis::get_bis_category_schemes,
            commands::bis::get_bis_hierarchical_codelists,
            commands::bis::get_bis_agency_schemes,
            commands::bis::get_bis_concepts,
            commands::bis::get_bis_codes,
            commands::bis::get_bis_categories,
            commands::bis::get_bis_hierarchies,
            commands::bis::get_bis_effective_exchange_rates,
            commands::bis::get_bis_central_bank_policy_rates,
            commands::bis::get_bis_long_term_interest_rates,
            commands::bis::get_bis_short_term_interest_rates,
            commands::bis::get_bis_exchange_rates,
            commands::bis::get_bis_credit_to_non_financial_sector,
            commands::bis::get_bis_house_prices,
            commands::bis::get_bis_available_datasets,
            commands::bis::search_bis_datasets,
            commands::bis::get_bis_economic_overview,
            commands::bis::get_bis_dataset_metadata,
            commands::bis::get_bis_monetary_policy_overview,
            commands::bis::get_bis_exchange_rate_analysis,
            commands::bis::get_bis_credit_conditions_overview,
            commands::bis::get_bis_housing_market_overview,
            commands::bis::get_bis_data_directory,
            commands::bis::get_bis_comprehensive_summary,
            commands::multpl::execute_multpl_command,
            commands::multpl::get_multpl_series,
            commands::multpl::get_multpl_multiple_series,
            commands::multpl::get_multpl_available_series,
            commands::multpl::get_multpl_shiller_pe,
            commands::multpl::get_multpl_pe_ratio,
            commands::multpl::get_multpl_dividend_yield,
            commands::multpl::get_multpl_earnings_yield,
            commands::multpl::get_multpl_price_to_sales,
            commands::multpl::get_multpl_shiller_pe_monthly,
            commands::multpl::get_multpl_shiller_pe_yearly,
            commands::multpl::get_multpl_pe_yearly,
            commands::multpl::get_multpl_dividend_yearly,
            commands::multpl::get_multpl_dividend_monthly,
            commands::multpl::get_multpl_dividend_growth_yearly,
            commands::multpl::get_multpl_dividend_yield_yearly,
            commands::multpl::get_multpl_earnings_yearly,
            commands::multpl::get_multpl_earnings_monthly,
            commands::multpl::get_multpl_earnings_growth_yearly,
            commands::multpl::get_multpl_earnings_yield_yearly,
            commands::multpl::get_multpl_real_price_yearly,
            commands::multpl::get_multpl_inflation_adjusted_price_yearly,
            commands::multpl::get_multpl_sales_yearly,
            commands::multpl::get_multpl_sales_growth_yearly,
            commands::multpl::get_multpl_real_sales_yearly,
            commands::multpl::get_multpl_price_to_sales_yearly,
            commands::multpl::get_multpl_price_to_book_yearly,
            commands::multpl::get_multpl_book_value_yearly,
            commands::multpl::get_multpl_valuation_overview,
            commands::multpl::get_multpl_dividend_overview,
            commands::multpl::get_multpl_earnings_overview,
            commands::multpl::get_multpl_comprehensive_overview,
            commands::multpl::get_multpl_valuation_analysis,
            commands::multpl::get_multpl_dividend_analysis,
            commands::multpl::get_multpl_earnings_analysis,
            commands::multpl::get_multpl_sales_analysis,
            commands::multpl::get_multpl_price_analysis,
            commands::multpl::get_multpl_custom_analysis,
            commands::multpl::get_multpl_year_over_year_comparison,
            commands::multpl::get_multpl_current_valuation,
            commands::eia::execute_eia_command,
            commands::eia::get_eia_petroleum_status_report,
            commands::eia::get_eia_petroleum_categories,
            commands::eia::get_eia_crude_stocks,
            commands::eia::get_eia_gasoline_stocks,
            commands::eia::get_eia_petroleum_imports,
            commands::eia::get_eia_spot_prices,
            commands::eia::get_eia_short_term_energy_outlook,
            commands::eia::get_eia_steo_tables,
            commands::eia::get_eia_energy_markets_summary,
            commands::eia::get_eia_natural_gas_summary,
            commands::eia::get_eia_balance_sheet,
            commands::eia::get_eia_inputs_production,
            commands::eia::get_eia_refiner_production,
            commands::eia::get_eia_distillate_stocks,
            commands::eia::get_eia_imports_by_country,
            commands::eia::get_eia_weekly_estimates,
            commands::eia::get_eia_retail_prices,
            commands::eia::get_eia_nominal_prices,
            commands::eia::get_eia_petroleum_supply,
            commands::eia::get_eia_electricity_overview,
            commands::eia::get_eia_macroeconomic_indicators,
            commands::eia::get_eia_energy_overview,
            commands::eia::get_eia_petroleum_markets_overview,
            commands::eia::get_eia_natural_gas_markets_overview,
            commands::eia::get_eia_energy_price_trends,
            commands::eia::get_eia_energy_supply_analysis,
            commands::eia::get_eia_energy_consumption_analysis,
            commands::eia::get_eia_custom_analysis,
            commands::eia::get_eia_current_market_snapshot,
            // ECB (European Central Bank) Commands
            commands::ecb::execute_ecb_command,
            commands::ecb::get_ecb_available_categories,
            commands::ecb::get_ecb_currency_reference_rates,
            commands::ecb::get_ecb_overview,
            commands::ecb::get_ecb_yield_curve,
            commands::ecb::get_ecb_yield_curve_aaa_spot,
            commands::ecb::get_ecb_yield_curve_aaa_forward,
            commands::ecb::get_ecb_yield_curve_aaa_par_yield,
            commands::ecb::get_ecb_yield_curve_all_spot,
            commands::ecb::get_ecb_yield_curve_all_forward,
            commands::ecb::get_ecb_yield_curve_all_par_yield,
            commands::ecb::get_ecb_balance_of_payments,
            commands::ecb::get_ecb_bop_main,
            commands::ecb::get_ecb_bop_summary,
            commands::ecb::get_ecb_bop_services,
            commands::ecb::get_ecb_bop_investment_income,
            commands::ecb::get_ecb_bop_direct_investment,
            commands::ecb::get_ecb_bop_portfolio_investment,
            commands::ecb::get_ecb_bop_other_investment,
            commands::ecb::get_ecb_bop_united_states,
            commands::ecb::get_ecb_bop_japan,
            commands::ecb::get_ecb_bop_united_kingdom,
            commands::ecb::get_ecb_bop_switzerland,
            commands::ecb::get_ecb_bop_canada,
            commands::ecb::get_ecb_bop_australia,
            commands::ecb::get_ecb_financial_markets_analysis,
            commands::ecb::get_ecb_yield_curve_analysis,
            commands::ecb::get_ecb_major_economies_bop_analysis,
            commands::ecb::get_ecb_custom_analysis,
            commands::ecb::get_ecb_current_market_snapshot,
            // BEA (Bureau of Economic Analysis) Commands
            commands::bea::bea_get_dataset_list,
            commands::bea::bea_get_parameter_list,
            commands::bea::bea_get_parameter_values,
            commands::bea::bea_get_parameter_values_filtered,
            commands::bea::bea_get_nipa_data,
            commands::bea::bea_get_ni_underlying_detail,
            commands::bea::bea_get_fixed_assets,
            commands::bea::bea_get_mne_data,
            commands::bea::bea_get_gdp_by_industry,
            commands::bea::bea_get_international_transactions,
            commands::bea::bea_get_international_investment_position,
            commands::bea::bea_get_input_output,
            commands::bea::bea_get_underlying_gdp_by_industry,
            commands::bea::bea_get_international_services_trade,
            commands::bea::bea_get_regional_data,
            commands::bea::bea_get_economic_overview,
            commands::bea::bea_get_regional_snapshot,
            // WTO (World Trade Organization) Commands
            commands::wto::execute_wto_command,
            // WITS (World Integrated Trade Solution) Commands
            commands::wits::execute_wits_command,
            commands::wto::get_wto_available_apis,
            commands::wto::get_wto_overview,
            commands::wto::get_wto_qr_members,
            commands::wto::get_wto_qr_products,
            commands::wto::get_wto_qr_notifications,
            commands::wto::get_wto_qr_details,
            commands::wto::get_wto_qr_list,
            commands::wto::get_wto_qr_hs_versions,
            commands::wto::get_wto_eping_members,
            commands::wto::search_wto_eping_notifications,
            commands::wto::get_wto_timeseries_topics,
            commands::wto::get_wto_timeseries_indicators,
            commands::wto::get_wto_timeseries_data,
            commands::wto::get_wto_timeseries_reporters,
            commands::wto::get_wto_tfad_data,
            commands::wto::get_wto_trade_restrictions_analysis,
            commands::wto::get_wto_notifications_analysis,
            commands::wto::get_wto_trade_statistics_analysis,
            commands::wto::get_wto_comprehensive_analysis,
            commands::wto::get_wto_us_trade_analysis,
            commands::wto::get_wto_china_trade_analysis,
            commands::wto::get_wto_germany_trade_analysis,
            commands::wto::get_wto_japan_trade_analysis,
            commands::wto::get_wto_uk_trade_analysis,
            commands::wto::get_wto_recent_tbt_notifications,
            commands::wto::get_wto_recent_sps_notifications,
            commands::wto::get_wto_member_analysis,
            commands::wto::get_wto_global_trade_overview,
            commands::wto::get_wto_sector_trade_analysis,
            commands::wto::get_wto_comparative_trade_analysis,
            // UNESCO UIS (UNESCO Institute for Statistics) Commands
            commands::unesco::unesco_get_indicator_data,
            commands::unesco::unesco_export_indicator_data,
            commands::unesco::unesco_list_geo_units,
            commands::unesco::unesco_export_geo_units,
            commands::unesco::unesco_list_indicators,
            commands::unesco::unesco_export_indicators,
            commands::unesco::unesco_get_default_version,
            commands::unesco::unesco_list_versions,
            commands::unesco::unesco_get_education_overview,
            commands::unesco::unesco_get_global_education_trends,
            commands::unesco::unesco_get_country_comparison,
            commands::unesco::unesco_search_indicators_by_theme,
            commands::unesco::unesco_get_regional_education_data,
            commands::unesco::unesco_get_science_technology_data,
            commands::unesco::unesco_get_culture_data,
            commands::unesco::unesco_export_country_dataset,
            // OSCAR (Observing Systems Capability Analysis and Review Tool) Commands
            commands::oscar::oscar_get_instruments,
            commands::oscar::oscar_get_instrument_by_slug,
            commands::oscar::oscar_search_instruments_by_type,
            commands::oscar::oscar_search_instruments_by_agency,
            commands::oscar::oscar_search_instruments_by_year_range,
            commands::oscar::oscar_get_instrument_classifications,
            commands::oscar::oscar_get_satellites,
            commands::oscar::oscar_get_satellite_by_slug,
            commands::oscar::oscar_search_satellites_by_orbit,
            commands::oscar::oscar_search_satellites_by_agency,
            commands::oscar::oscar_get_variables,
            commands::oscar::oscar_get_variable_by_slug,
            commands::oscar::oscar_search_variables_by_instrument,
            commands::oscar::oscar_get_ecv_variables,
            commands::oscar::oscar_get_space_agencies,
            commands::oscar::oscar_get_instrument_types,
            commands::oscar::oscar_search_weather_instruments,
            commands::oscar::oscar_get_climate_monitoring_instruments,
            commands::oscar::oscar_get_overview_statistics,
            // FRED (Federal Reserve Economic Data) Commands
            commands::fred::execute_fred_command,
            commands::fred::get_fred_series,
            commands::fred::search_fred_series,
            commands::fred::get_fred_series_info,
            commands::fred::get_fred_category,
            commands::fred::get_fred_multiple_series,
            // CoinGecko Commands
            commands::coingecko::execute_coingecko_command,
            commands::coingecko::get_coingecko_price,
            commands::coingecko::get_coingecko_market_data,
            commands::coingecko::get_coingecko_historical,
            commands::coingecko::search_coingecko_coins,
            commands::coingecko::get_coingecko_trending,
            commands::coingecko::get_coingecko_global_data,
            commands::coingecko::get_coingecko_top_coins,
            // World Bank Commands
            commands::worldbank::execute_worldbank_command,
            commands::worldbank::get_worldbank_indicator,
            commands::worldbank::search_worldbank_indicators,
            commands::worldbank::get_worldbank_country_info,
            commands::worldbank::get_worldbank_countries,
            commands::worldbank::get_worldbank_gdp,
            commands::worldbank::get_worldbank_economic_overview,
            commands::worldbank::get_worldbank_development_indicators,
            // Trading Economics Commands
            commands::trading_economics::execute_trading_economics_command,
            commands::trading_economics::get_trading_economics_indicators,
            commands::trading_economics::get_trading_economics_historical,
            commands::trading_economics::get_trading_economics_calendar,
            commands::trading_economics::get_trading_economics_markets,
            commands::trading_economics::get_trading_economics_ratings,
            commands::trading_economics::get_trading_economics_forecasts,
            // EconDB Commands
            commands::econdb::execute_econdb_command,
            commands::econdb::get_econdb_series,
            commands::econdb::search_econdb_indicators,
            commands::econdb::get_econdb_datasets,
            commands::econdb::get_econdb_country_data,
            commands::econdb::get_econdb_multiple_series,
            // Canada Government Commands
            commands::canada_gov::execute_canada_gov_command,
            commands::canada_gov::search_canada_gov_datasets,
            commands::canada_gov::get_canada_gov_dataset,
            commands::canada_gov::get_canada_gov_economic_data,
            // UK Government Commands
            commands::datagovuk::execute_datagovuk_command,
            commands::datagovuk::search_datagovuk_datasets,
            commands::datagovuk::get_datagovuk_dataset,
            // Australian Government Commands
            commands::datagov_au::execute_datagov_au_command,
            commands::datagov_au::search_datagov_au_datasets,
            commands::datagov_au::get_datagov_au_dataset,
            // Japan Statistics Commands
            commands::estat_japan::execute_estat_japan_command,
            commands::estat_japan::get_estat_japan_data,
            commands::estat_japan::search_estat_japan_stats,
            // German Government Commands
            commands::govdata_de::execute_govdata_de_command,
            commands::govdata_de::search_govdata_de_datasets,
            commands::govdata_de::get_govdata_de_dataset,
            // Singapore Government Commands
            commands::datagovsg::execute_datagovsg_command,
            commands::datagovsg::search_datagovsg_datasets,
            commands::datagovsg::get_datagovsg_dataset,
            // Hong Kong Government Commands
            commands::data_gov_hk::execute_data_gov_hk_command,
            commands::data_gov_hk::search_data_gov_hk_datasets,
            commands::data_gov_hk::get_data_gov_hk_dataset,
            // French Government Commands
            commands::french_gov::execute_french_gov_command,
            commands::french_gov::search_french_gov_datasets,
            commands::french_gov::get_french_gov_dataset,
            // Swiss Government Commands
            commands::swiss_gov::execute_swiss_gov_command,
            commands::swiss_gov::search_swiss_gov_datasets,
            commands::swiss_gov::get_swiss_gov_dataset,
            // Spain Data Commands
            commands::spain_data::execute_spain_data_command,
            commands::spain_data::get_spain_economic_data,
            commands::spain_data::search_spain_datasets,
            // Asian Development Bank Commands
            commands::adb::execute_adb_command,
            commands::adb::get_adb_indicators,
            commands::adb::search_adb_datasets,
            commands::adb::get_adb_development_data,
            // OpenAFRICA Commands
            commands::openafrica::execute_openafrica_command,
            commands::openafrica::search_openafrica_datasets,
            commands::openafrica::get_openafrica_dataset,
            commands::openafrica::get_openafrica_country_datasets,
            // HDX Commands
            commands::hdx::hdx_search_datasets,
            commands::hdx::hdx_get_dataset,
            commands::hdx::hdx_search_conflict,
            commands::hdx::hdx_search_humanitarian,
            commands::hdx::hdx_search_by_country,
            commands::hdx::hdx_search_by_organization,
            commands::hdx::hdx_search_by_topic,
            commands::hdx::hdx_search_by_dataseries,
            commands::hdx::hdx_list_countries,
            commands::hdx::hdx_list_organizations,
            commands::hdx::hdx_list_topics,
            commands::hdx::hdx_get_resource_url,
            commands::hdx::hdx_advanced_search,
            commands::hdx::hdx_test_connection,
            // Economic Calendar Commands
            commands::economic_calendar::execute_economic_calendar_command,
            commands::economic_calendar::get_economic_calendar_today,
            commands::economic_calendar::get_economic_calendar_upcoming,
            commands::economic_calendar::get_economic_calendar_by_country,
            commands::economic_calendar::get_economic_calendar_high_impact,
            // Databento Commands
            commands::databento::databento_test_connection,
            commands::databento::databento_get_options_chain,
            commands::databento::databento_get_options_definitions,
            commands::databento::databento_get_multi_asset_data,
            commands::databento::databento_get_historical_ohlcv,
            commands::databento::execute_databento_command,
            commands::databento::get_databento_market_data,
            commands::databento::get_databento_datasets,
            commands::databento::get_databento_historical,
            commands::databento::databento_resolve_symbols,
            commands::databento::databento_list_datasets,
            commands::databento::databento_get_dataset_info,
            commands::databento::databento_get_cost_estimate,
            commands::databento::databento_list_schemas,
            // Databento Reference Data Commands
            commands::databento::databento_get_security_master,
            commands::databento::databento_get_corporate_actions,
            commands::databento::databento_get_adjustment_factors,
            // Databento Order Book Commands
            commands::databento::databento_get_order_book,
            commands::databento::databento_get_order_book_snapshot,
            // Databento Live Streaming Commands
            commands::databento::databento_live_info,
            commands::databento::databento_live_start,
            commands::databento::databento_live_stop,
            commands::databento::databento_live_list,
            commands::databento::databento_live_stop_all,
            // Databento Futures Commands
            commands::databento::databento_list_futures,
            commands::databento::databento_get_futures_data,
            commands::databento::databento_get_futures_definitions,
            commands::databento::databento_get_futures_term_structure,
            // Databento Batch Download Commands
            commands::databento::databento_submit_batch_job,
            commands::databento::databento_list_batch_jobs,
            commands::databento::databento_list_batch_files,
            commands::databento::databento_download_batch_job,
            // Databento Additional Schemas Commands
            commands::databento::databento_get_trades,
            commands::databento::databento_get_imbalance,
            commands::databento::databento_get_statistics,
            commands::databento::databento_get_status,
            commands::databento::databento_get_schema_data,
            // Sentinel Hub Commands
            commands::sentinelhub::execute_sentinelhub_command,
            commands::sentinelhub::get_sentinelhub_imagery,
            commands::sentinelhub::get_sentinelhub_collections,
            commands::sentinelhub::search_sentinelhub_data,
            // Jupyter Notebook Commands
            commands::jupyter::execute_python_code,
            commands::jupyter::open_notebook,
            commands::jupyter::save_notebook,
            commands::jupyter::create_new_notebook,
            commands::jupyter::get_python_version,
            commands::jupyter::install_python_package,
            commands::jupyter::list_python_packages,
            commands::finscript_cmd::execute_finscript,
            // Python Agent Commands
            commands::agents::list_available_agents,
            commands::agents::execute_python_agent,
            commands::agents::get_agent_metadata,
            commands::agents::execute_core_agent,
            commands::agents::read_agent_config,
            // SuperAgent & Planner Commands
            commands::agents::route_query,
            commands::agents::execute_routed_query,
            commands::agents::discover_agents,
            commands::agents::create_stock_analysis_plan,
            commands::agents::execute_plan,
            commands::agents::save_trade_decision,
            commands::agents::get_trade_decisions,
            // Agent Streaming & Performance Commands
            commands::agent_streaming::execute_agent_streaming,
            commands::agent_streaming::cancel_agent_stream,
            commands::agent_streaming::execute_agents_parallel,
            commands::agent_streaming::execute_agent_priority,
            commands::agent_streaming::get_active_agent_streams,
            // Portfolio Analytics Commands
            // Portfolio Management (CRUD operations)
            commands::portfolio_management::portfolio_create,
            commands::portfolio_management::portfolio_get_all,
            commands::portfolio_management::portfolio_get_by_id,
            commands::portfolio_management::portfolio_delete,
            commands::portfolio_management::portfolio_add_asset,
            commands::portfolio_management::portfolio_sell_asset,
            commands::portfolio_management::portfolio_get_assets,
            commands::portfolio_management::portfolio_update_asset_symbol,
            commands::portfolio_management::portfolio_get_transactions,
            // Custom Index Commands (Aggregate Index Feature)
            commands::custom_index::custom_index_create,
            commands::custom_index::custom_index_get_all,
            commands::custom_index::custom_index_get_by_id,
            commands::custom_index::custom_index_update,
            commands::custom_index::custom_index_delete,
            commands::custom_index::custom_index_hard_delete,
            commands::custom_index::index_constituent_add,
            commands::custom_index::index_constituent_get_all,
            commands::custom_index::index_constituent_update,
            commands::custom_index::index_constituent_remove,
            commands::custom_index::index_snapshot_save,
            commands::custom_index::index_snapshot_get_all,
            commands::custom_index::index_snapshot_get_latest,
            commands::custom_index::index_snapshot_cleanup,
            // Portfolio Analytics
            commands::portfolio::calculate_portfolio_metrics,
            commands::portfolio::optimize_portfolio,
            commands::portfolio::generate_efficient_frontier,
            commands::portfolio::get_portfolio_overview,
            commands::portfolio::calculate_risk_metrics,
            commands::portfolio::generate_asset_allocation,
            commands::portfolio::calculate_retirement_plan,
            commands::portfolio::analyze_behavioral_biases,
            commands::portfolio::analyze_etf_costs,
            // skfolio - Advanced Portfolio Optimization
            commands::skfolio::skfolio_optimize_portfolio,
            commands::skfolio::skfolio_hyperparameter_tuning,
            commands::skfolio::skfolio_backtest_strategy,
            commands::skfolio::skfolio_stress_test,
            commands::skfolio::skfolio_efficient_frontier,
            commands::skfolio::skfolio_risk_attribution,
            commands::skfolio::skfolio_compare_strategies,
            commands::skfolio::skfolio_generate_report,
            commands::skfolio::skfolio_export_weights,
            commands::skfolio::skfolio_scenario_analysis,
            commands::pyportfolioopt::pyportfolioopt_optimize,
            commands::pyportfolioopt::pyportfolioopt_efficient_frontier,
            commands::pyportfolioopt::pyportfolioopt_discrete_allocation,
            commands::pyportfolioopt::pyportfolioopt_backtest,
            commands::pyportfolioopt::pyportfolioopt_risk_decomposition,
            commands::pyportfolioopt::pyportfolioopt_black_litterman,
            commands::pyportfolioopt::pyportfolioopt_hrp,
            commands::pyportfolioopt::pyportfolioopt_generate_report,
            // Active Management Commands
            commands::portfolio::calculate_active_value_added,
            commands::portfolio::calculate_active_information_ratio,
            commands::portfolio::calculate_active_tracking_risk,
            commands::portfolio::comprehensive_active_analysis,
            commands::portfolio::analyze_manager_selection,
            // Risk Management Commands
            commands::portfolio::comprehensive_risk_analysis,
            // Portfolio Planning Commands
            commands::portfolio::strategic_asset_allocation,
            commands::portfolio::calculate_retirement_planning,
            // Economics & Markets Commands
            commands::portfolio::comprehensive_economics_analysis,
            commands::portfolio::analyze_business_cycle,
            commands::portfolio::analyze_equity_risk_premium,
            // Technical Analysis Commands
            commands::technical_analysis::execute_technical_analysis_command,
            commands::technical_analysis::get_technical_analysis_help,
            commands::technical_analysis::calculate_indicators_yfinance,
            commands::technical_analysis::calculate_indicators_csv,
            commands::technical_analysis::calculate_indicators_json,
            commands::technical_analysis::compute_all_technicals,
            // FiscalData Commands
            commands::fiscaldata::execute_fiscaldata_command,
            commands::fiscaldata::get_fiscaldata_debt_to_penny,
            commands::fiscaldata::get_fiscaldata_avg_interest_rates,
            commands::fiscaldata::get_fiscaldata_interest_expense,
            commands::fiscaldata::get_fiscaldata_datasets,
            // NASA GIBS Commands
            commands::nasa_gibs::execute_nasa_gibs_command,
            commands::nasa_gibs::get_nasa_gibs_layers,
            commands::nasa_gibs::get_nasa_gibs_layer_details,
            commands::nasa_gibs::search_nasa_gibs_layers,
            commands::nasa_gibs::get_nasa_gibs_time_periods,
            commands::nasa_gibs::get_nasa_gibs_tile,
            commands::nasa_gibs::get_nasa_gibs_tile_batch,
            commands::nasa_gibs::get_nasa_gibs_popular_layers,
            commands::nasa_gibs::get_nasa_gibs_supported_projections,
            commands::nasa_gibs::test_nasa_gibs_all_endpoints,
            // Universal CKAN Commands
            commands::ckan::execute_ckan_command,
            commands::ckan::get_ckan_supported_countries,
            commands::ckan::test_ckan_portal_connection,
            commands::ckan::list_ckan_organizations,
            commands::ckan::get_ckan_organization_details,
            commands::ckan::list_ckan_datasets,
            commands::ckan::search_ckan_datasets,
            commands::ckan::get_ckan_dataset_details,
            commands::ckan::get_ckan_datasets_by_organization,
            commands::ckan::get_ckan_dataset_resources,
            commands::ckan::get_ckan_resource_details,
            // Analytics Commands
            commands::analytics::execute_python_command,
            commands::analytics::execute_technical_indicators,
            commands::analytics::execute_pyportfolioopt,
            commands::analytics::execute_riskfolio,
            commands::analytics::execute_skfolio,
            commands::analytics::analyze_digital_assets,
            commands::analytics::analyze_hedge_funds,
            commands::analytics::analyze_real_estate,
            commands::analytics::analyze_private_capital,
            commands::analytics::analyze_natural_resources,
            commands::analytics::analyze_performance_metrics,
            commands::analytics::analyze_investment_risk,
            commands::analytics::price_options,
            commands::analytics::analyze_arbitrage,
            commands::analytics::analyze_forward_commitments,
            commands::analytics::analyze_currency,
            commands::analytics::analyze_growth,
            commands::analytics::analyze_policy,
            commands::analytics::analyze_market_cycles,
            commands::analytics::analyze_trade_geopolitics,
            commands::analytics::analyze_capital_flows,
            commands::analytics::calculate_dcf_valuation,
            commands::analytics::calculate_dividend_valuation,
            commands::analytics::calculate_multiples_valuation,
            commands::analytics::analyze_fundamental,
            commands::analytics::analyze_industry,
            commands::analytics::forecast_financials,
            commands::analytics::analyze_market_efficiency,
            commands::analytics::analyze_index,
            commands::analytics::analyze_portfolio_risk,
            commands::analytics::analyze_portfolio_performance,
            commands::analytics::optimize_portfolio_management,
            commands::analytics::plan_portfolio,
            commands::analytics::analyze_active_management,
            commands::analytics::analyze_behavioral_finance,
            commands::analytics::analyze_etf,
            commands::analytics::calculate_quant_metrics,
            commands::analytics::calculate_rates,
            commands::analytics::execute_economics_analytics,
            commands::analytics::execute_statsmodels_analytics,
            commands::analytics::execute_quant_analytics,
            // FinancePy - Derivatives Pricing Commands
            commands::financepy::financepy_create_date,
            commands::financepy::financepy_date_range,
            commands::financepy::financepy_bond_price,
            commands::financepy::financepy_bond_ytm,
            commands::financepy::financepy_equity_option_price,
            commands::financepy::financepy_equity_option_implied_vol,
            commands::financepy::financepy_fx_option_price,
            commands::financepy::financepy_ibor_swap_price,
            commands::financepy::financepy_cds_spread,
            // AI Agent Commands
            commands::ai_agents::execute_economic_agent,
            commands::ai_agents::run_capitalism_agent,
            commands::ai_agents::run_keynesian_agent,
            commands::ai_agents::run_neoliberal_agent,
            commands::ai_agents::run_socialism_agent,
            commands::ai_agents::run_mercantilist_agent,
            commands::ai_agents::run_mixed_economy_agent,
            commands::ai_agents::execute_geopolitics_agent,
            commands::ai_agents::run_eurasian_chessboard_agent,
            commands::ai_agents::run_geostrategic_players_agent,
            commands::ai_agents::run_eurasian_balkans_agent,
            commands::ai_agents::run_geography_agent,
            commands::ai_agents::run_world_order_agent,
            commands::ai_agents::run_westphalian_europe_agent,
            commands::ai_agents::run_balance_power_agent,
            commands::ai_agents::execute_hedge_fund_agent,
            commands::ai_agents::run_bridgewater_agent,
            commands::ai_agents::run_citadel_agent,
            commands::ai_agents::run_renaissance_agent,
            commands::ai_agents::run_de_shaw_agent,
            commands::ai_agents::run_two_sigma_agent,
            commands::ai_agents::run_elliott_management_agent,
            commands::ai_agents::run_fincept_hedge_fund_orchestrator,
            commands::ai_agents::run_warren_buffett_agent,
            commands::ai_agents::run_benjamin_graham_agent,
            commands::ai_agents::run_charlie_munger_agent,
            commands::ai_agents::run_seth_klarman_agent,
            commands::ai_agents::run_howard_marks_agent,
            commands::ai_agents::run_joel_greenblatt_agent,
            commands::ai_agents::run_david_einhorn_agent,
            commands::ai_agents::run_bill_miller_agent,
            // Report Generator Commands
            commands::report_generator::generate_report_html,
            commands::report_generator::generate_report_pdf,
            commands::report_generator::create_default_report_template,
            commands::report_generator::open_folder,
            commands::backtesting::execute_command,
            commands::backtesting::execute_python_backtest,
            commands::backtesting::get_home_dir,
            commands::backtesting::check_file_exists,
            commands::backtesting::create_directory,
            commands::backtesting::write_file,
            commands::backtesting::read_file,
            // Company News Commands
            commands::company_news::fetch_company_news,
            commands::company_news::fetch_news_by_topic,
            commands::company_news::get_full_article,
            commands::company_news::get_company_news_help,
            // Agno Trading Agents
            commands::agno_trading::agno_list_models,
            commands::agno_trading::agno_recommend_model,
            commands::agno_trading::agno_validate_config,
            commands::agno_trading::agno_create_agent,
            commands::agno_trading::agno_run_agent,
            commands::agno_trading::agno_analyze_market,
            commands::agno_trading::agno_generate_trade_signal,
            commands::agno_trading::agno_manage_risk,
            commands::agno_trading::agno_get_config_template,
            commands::agno_trading::agno_create_competition,
            commands::agno_trading::agno_run_competition,
            commands::agno_trading::agno_get_leaderboard,
            commands::agno_trading::agno_get_recent_decisions,
            // New commands: Auto-Trading, Debate, Evolution
            commands::agno_trading::agno_execute_trade,
            commands::agno_trading::agno_close_position,
            commands::agno_trading::agno_get_agent_trades,
            commands::agno_trading::agno_get_agent_performance,
            commands::agno_trading::agno_get_db_leaderboard,
            commands::agno_trading::agno_get_db_decisions,
            commands::agno_trading::agno_run_debate,
            commands::agno_trading::agno_get_recent_debates,
            commands::agno_trading::agno_evolve_agent,
            commands::agno_trading::agno_check_evolution,
            commands::agno_trading::agno_get_evolution_summary,
            // Alpha Arena Competition (v2 refactored)
            commands::alpha_arena::create_alpha_competition,
            commands::alpha_arena::start_alpha_competition,
            commands::alpha_arena::get_alpha_leaderboard,
            commands::alpha_arena::get_alpha_model_decisions,
            commands::alpha_arena::stop_alpha_competition,
            commands::alpha_arena::run_alpha_cycle,
            commands::alpha_arena::get_alpha_snapshots,
            commands::alpha_arena::list_alpha_agents,
            commands::alpha_arena::get_alpha_evaluation,
            commands::alpha_arena::get_alpha_api_keys,
            commands::alpha_arena::save_alpha_api_keys,
            commands::alpha_arena::list_alpha_competitions,
            commands::alpha_arena::get_alpha_competition,
            commands::alpha_arena::delete_alpha_competition,
            // Alpha Arena - Trading Styles (v2.2)
            commands::alpha_arena::list_alpha_trading_styles,
            commands::alpha_arena::create_alpha_styled_agents,
            commands::alpha_arena::run_alpha_action,
            // AI Quant Lab - Qlib
            commands::ai_quant_lab::qlib_list_models,
            commands::ai_quant_lab::qlib_get_factor_library,
            commands::ai_quant_lab::qlib_get_data,
            commands::ai_quant_lab::qlib_train_model,
            commands::ai_quant_lab::qlib_run_backtest,
            commands::ai_quant_lab::qlib_optimize_portfolio,
            // AI Quant Lab - RD-Agent
            commands::ai_quant_lab::rdagent_get_capabilities,
            commands::ai_quant_lab::rdagent_start_factor_mining,
            commands::ai_quant_lab::rdagent_get_factor_mining_status,
            commands::ai_quant_lab::rdagent_get_discovered_factors,
            commands::ai_quant_lab::rdagent_optimize_model,
            commands::ai_quant_lab::rdagent_analyze_document,
            commands::ai_quant_lab::rdagent_run_autonomous_research,
            commands::ai_quant_lab::deepagent_check_status,
            commands::ai_quant_lab::deepagent_get_capabilities,
            commands::ai_quant_lab::deepagent_execute_task,
            commands::ai_quant_lab::deepagent_create_plan,
            commands::ai_quant_lab::deepagent_execute_step,
            commands::ai_quant_lab::deepagent_synthesize_results,
            commands::ai_quant_lab::deepagent_resume_task,
            commands::ai_quant_lab::deepagent_get_state,
            // AI Quant Lab - Reinforcement Learning
            commands::ai_quant_lab::qlib_rl_get_algorithms,
            commands::ai_quant_lab::qlib_rl_create_environment,
            commands::ai_quant_lab::qlib_rl_train_agent,
            commands::ai_quant_lab::qlib_rl_evaluate_agent,
            // AI Quant Lab - Online Learning
            commands::ai_quant_lab::qlib_online_initialize,
            commands::ai_quant_lab::qlib_online_incremental_train,
            commands::ai_quant_lab::qlib_online_predict,
            commands::ai_quant_lab::qlib_online_detect_drift,
            // AI Quant Lab - High Frequency Trading
            commands::ai_quant_lab::qlib_hft_initialize,
            commands::ai_quant_lab::qlib_hft_create_orderbook,
            commands::ai_quant_lab::qlib_hft_update_orderbook,
            commands::ai_quant_lab::qlib_hft_market_making_quotes,
            commands::ai_quant_lab::qlib_hft_detect_toxic,
            commands::ai_quant_lab::qlib_hft_snapshot,
            commands::ai_quant_lab::qlib_hft_latency_stats,
            // AI Quant Lab - Meta Learning
            commands::ai_quant_lab::qlib_meta_model_selection,
            commands::ai_quant_lab::qlib_meta_create_ensemble,
            commands::ai_quant_lab::qlib_meta_auto_tune,
            // AI Quant Lab - Rolling Retraining
            commands::ai_quant_lab::qlib_rolling_create_schedule,
            commands::ai_quant_lab::qlib_rolling_retrain,
            commands::ai_quant_lab::qlib_rolling_list_schedules,
            // AI Quant Lab - Advanced Models
            commands::ai_quant_lab::qlib_advanced_list_models,
            commands::ai_quant_lab::qlib_advanced_create_model,
            // FFN Analytics - Portfolio Performance Analysis Commands
            commands::ffn_analytics::ffn_check_status,
            commands::ffn_analytics::ffn_calculate_performance,
            commands::ffn_analytics::ffn_calculate_drawdowns,
            commands::ffn_analytics::ffn_calculate_rolling_metrics,
            commands::ffn_analytics::ffn_monthly_returns,
            commands::ffn_analytics::ffn_rebase_prices,
            commands::ffn_analytics::ffn_compare_assets,
            commands::ffn_analytics::ffn_risk_metrics,
            commands::ffn_analytics::ffn_full_analysis,
            commands::ffn_analytics::ffn_portfolio_optimization,
            commands::ffn_analytics::ffn_benchmark_comparison,
            // QuantStats - Portfolio Performance & Risk Analytics
            commands::quantstats::run_quantstats_analysis,
            // Fortitudo.tech Analytics - Portfolio Risk Analytics Commands
            commands::fortitudo_analytics::fortitudo_check_status,
            commands::fortitudo_analytics::fortitudo_portfolio_metrics,
            commands::fortitudo_analytics::fortitudo_covariance_analysis,
            commands::fortitudo_analytics::fortitudo_exp_decay_weighting,
            commands::fortitudo_analytics::fortitudo_option_pricing,
            commands::fortitudo_analytics::fortitudo_option_chain,
            commands::fortitudo_analytics::fortitudo_option_greeks,
            commands::fortitudo_analytics::fortitudo_entropy_pooling,
            commands::fortitudo_analytics::fortitudo_exposure_stacking,
            commands::fortitudo_analytics::fortitudo_full_analysis,
            commands::fortitudo_analytics::fortitudo_optimize_mean_variance,
            commands::fortitudo_analytics::fortitudo_optimize_mean_cvar,
            commands::fortitudo_analytics::fortitudo_efficient_frontier_mv,
            commands::fortitudo_analytics::fortitudo_efficient_frontier_cvar,
            // Functime Analytics - Time Series Forecasting Commands
            commands::functime_analytics::functime_check_status,
            commands::functime_analytics::functime_forecast,
            commands::functime_analytics::functime_preprocess,
            commands::functime_analytics::functime_cross_validate,
            commands::functime_analytics::functime_metrics,
            commands::functime_analytics::functime_add_features,
            commands::functime_analytics::functime_seasonal_period,
            commands::functime_analytics::functime_full_pipeline,
            // Advanced Functime Commands
            commands::functime_analytics::functime_advanced_forecast,
            commands::functime_analytics::functime_ensemble,
            commands::functime_analytics::functime_auto_ensemble,
            commands::functime_analytics::functime_anomaly_detection,
            commands::functime_analytics::functime_seasonality,
            commands::functime_analytics::functime_confidence_intervals,
            commands::functime_analytics::functime_feature_importance,
            commands::functime_analytics::functime_advanced_cv,
            commands::functime_analytics::functime_backtest,
            // High-Performance OrderBook Processing Commands
            commands::orderbook::merge_orderbook,
            commands::orderbook::update_orderbook_snapshot,
            commands::orderbook::calculate_orderbook_metrics,
            commands::orderbook::calculate_cumulative_liquidity,
            commands::orderbook::batch_merge_orderbook,
            // High-Performance Rust SQLite Database Commands
            commands::database::db_check_health,
            commands::database::db_save_setting,
            commands::database::db_get_setting,
            commands::database::db_get_all_settings,
            commands::database::db_save_credential,
            commands::database::db_get_credentials,
            commands::database::db_get_credential_by_service,
            commands::database::db_delete_credential,
            commands::database::db_get_llm_configs,
            commands::database::db_save_llm_config,
            commands::database::db_get_llm_global_settings,
            commands::database::db_save_llm_global_settings,
            commands::database::db_set_active_llm_provider,
            commands::database::db_get_llm_model_configs,
            commands::database::db_save_llm_model_config,
            commands::database::db_delete_llm_model_config,
            commands::database::db_toggle_llm_model_config_enabled,
            commands::database::db_update_llm_model_id,
            commands::database::db_fix_google_model_ids,
            commands::database::db_create_chat_session,
            commands::database::db_get_chat_sessions,
            commands::database::db_add_chat_message,
            commands::database::db_get_chat_messages,
            commands::database::db_delete_chat_session,
            commands::database::db_save_data_source,
            commands::database::db_get_all_data_sources,
            commands::database::db_delete_data_source,
            commands::database::db_add_mcp_server,
            commands::database::db_get_mcp_servers,
            commands::database::db_delete_mcp_server,
            commands::database::db_get_internal_tool_settings,
            commands::database::db_set_internal_tool_enabled,
            commands::database::db_is_internal_tool_enabled,
            commands::database::db_save_backtesting_provider,
            commands::database::db_get_backtesting_providers,
            commands::database::db_save_backtesting_strategy,
            commands::database::db_get_backtesting_strategies,
            commands::database::db_save_backtest_run,
            commands::database::db_get_backtest_runs,
            commands::database::db_save_recorded_context,
            commands::database::db_get_recorded_contexts,
            commands::database::db_delete_recorded_context,
            commands::database::db_create_watchlist,
            commands::database::db_get_watchlists,
            commands::database::db_add_watchlist_stock,
            commands::database::db_get_watchlist_stocks,
            commands::database::db_remove_watchlist_stock,
            commands::database::db_delete_watchlist,
            // Legacy cache commands (backward compatibility)
            commands::database::db_save_market_data_cache,
            commands::database::db_get_cached_market_data,
            commands::database::db_clear_market_data_cache,
            // Unified Cache Commands
            commands::database::cache_get,
            commands::database::cache_get_with_stale,
            commands::database::cache_set,
            commands::database::cache_delete,
            commands::database::cache_get_many,
            commands::database::cache_invalidate_category,
            commands::database::cache_invalidate_pattern,
            commands::database::cache_cleanup,
            commands::database::cache_stats,
            commands::database::cache_clear_all,
            // Tab Session Commands
            commands::database::tab_session_get,
            commands::database::tab_session_set,
            commands::database::tab_session_delete,
            commands::database::tab_session_get_all,
            commands::database::tab_session_cleanup,
            commands::database::db_create_note,
            commands::database::db_get_all_notes,
            commands::database::db_update_note,
            commands::database::db_delete_note,
            commands::database::db_search_notes,
            commands::database::db_get_note_templates,
            commands::database::db_add_or_update_excel_file,
            commands::database::db_get_recent_excel_files,
            commands::database::db_create_excel_snapshot,
            commands::database::db_get_excel_snapshots,
            commands::database::db_delete_excel_snapshot,
            // Agent Configuration Commands
            commands::database::db_save_agent_config,
            commands::database::db_get_agent_configs,
            commands::database::db_get_agent_config,
            commands::database::db_get_agent_configs_by_category,
            commands::database::db_delete_agent_config,
            commands::database::db_set_active_agent_config,
            commands::database::db_get_active_agent_config,
            // Alpha Arena Bridge Commands
            commands::alpha_arena_bridge::paper_trading_create_portfolio,
            commands::alpha_arena_bridge::paper_trading_get_portfolio,
            commands::alpha_arena_bridge::paper_trading_get_positions,
            commands::alpha_arena_bridge::paper_trading_place_order,
            commands::alpha_arena_bridge::paper_trading_fill_order,
            commands::alpha_arena_bridge::paper_trading_get_orders,
            commands::alpha_arena_bridge::paper_trading_get_trades,
            commands::alpha_arena_bridge::alpha_arena_record_decision,
            commands::financial_analysis::analyze_income_statement,
            commands::financial_analysis::analyze_balance_sheet,
            commands::financial_analysis::analyze_cash_flow,
            commands::financial_analysis::analyze_financial_statements,
            commands::financial_analysis::get_financial_key_metrics,
            commands::financial_analysis::analyze_financial_json,
            // Peer Comparison & Benchmarking Commands
            commands::peer_commands::find_peers,
            commands::peer_commands::compare_peers,
            commands::peer_commands::get_sector_benchmarks,
            commands::peer_commands::calculate_peer_percentiles,
            // Stock Screener Commands
            commands::screener_commands::execute_stock_screen,
            commands::screener_commands::get_value_screen,
            commands::screener_commands::get_growth_screen,
            commands::screener_commands::get_dividend_screen,
            commands::screener_commands::get_momentum_screen,
            commands::screener_commands::save_custom_screen,
            commands::screener_commands::load_custom_screens,
            commands::screener_commands::delete_custom_screen,
            commands::screener_commands::get_available_metrics,
            commands::geocoding::execute_geocoding_command,
            commands::geocoding::search_geocode,
            commands::geocoding::reverse_geocode,
            // Paper Trading Commands (Universal)
            paper_trading::pt_create_portfolio,
            paper_trading::pt_get_portfolio,
            paper_trading::pt_list_portfolios,
            paper_trading::pt_delete_portfolio,
            paper_trading::pt_reset_portfolio,
            paper_trading::pt_place_order,
            paper_trading::pt_cancel_order,
            paper_trading::pt_get_orders,
            paper_trading::pt_fill_order,
            paper_trading::pt_get_positions,
            paper_trading::pt_update_price,
            paper_trading::pt_get_trades,
            paper_trading::pt_get_stats,
            // Broker Credentials Commands
            commands::broker_credentials::save_broker_credentials,
            commands::broker_credentials::get_broker_credentials,
            commands::broker_credentials::delete_broker_credentials,
            commands::broker_credentials::list_broker_credentials,
            // Master Contract Commands
            commands::master_contract::save_master_contract_cache,
            commands::master_contract::get_master_contract_cache,
            commands::master_contract::delete_master_contract_cache,
            commands::master_contract::is_master_contract_cache_valid,
            commands::master_contract::get_master_contract_cache_info,
            commands::master_contract::download_master_contract,
            commands::master_contract::download_angelone_master_contract,
            commands::master_contract::download_fyers_master_contract,
            commands::master_contract::search_symbols,
            commands::master_contract::get_symbol,
            commands::master_contract::get_symbol_by_token,
            commands::master_contract::get_symbol_count,
            commands::master_contract::get_exchanges,
            commands::master_contract::get_expiries,
            // Indian Broker Commands (Zerodha, Angel, Dhan, etc.)
            commands::brokers::zerodha_exchange_token,
            commands::brokers::zerodha_validate_token,
            commands::brokers::zerodha_place_order,
            commands::brokers::zerodha_modify_order,
            commands::brokers::zerodha_cancel_order,
            commands::brokers::zerodha_get_orders,
            commands::brokers::zerodha_get_positions,
            commands::brokers::zerodha_get_holdings,
            commands::brokers::zerodha_get_margins,
            commands::brokers::zerodha_get_quote,
            commands::brokers::zerodha_get_historical,
            commands::brokers::zerodha_download_master_contract,
            commands::brokers::zerodha_ws_connect,
            commands::brokers::zerodha_ws_disconnect,
            commands::brokers::zerodha_ws_subscribe,
            commands::brokers::zerodha_ws_unsubscribe,
            commands::brokers::zerodha_ws_set_token_map,
            commands::brokers::zerodha_ws_is_connected,
            commands::brokers::zerodha_get_trade_book,
            commands::brokers::zerodha_get_open_position,
            commands::brokers::zerodha_place_smart_order,
            commands::brokers::zerodha_close_all_positions,
            commands::brokers::zerodha_cancel_all_orders,
            commands::brokers::zerodha_calculate_margin,
            commands::brokers::zerodha_search_symbols,
            commands::brokers::zerodha_get_instrument,
            commands::brokers::zerodha_get_master_contract_metadata,
            commands::brokers::store_indian_broker_credentials,
            commands::brokers::get_indian_broker_credentials,
            commands::brokers::delete_indian_broker_credentials,
            commands::brokers::search_indian_symbols,
            commands::brokers::get_indian_symbol_token,
            commands::brokers::get_master_contract_timestamp,
            // Fyers Broker Commands
            commands::brokers::fyers_exchange_token,
            commands::brokers::fyers_place_order,
            commands::brokers::fyers_modify_order,
            commands::brokers::fyers_cancel_order,
            commands::brokers::fyers_get_orders,
            commands::brokers::fyers_get_trade_book,
            commands::brokers::fyers_get_positions,
            commands::brokers::fyers_get_holdings,
            commands::brokers::fyers_get_funds,
            commands::brokers::fyers_get_quotes,
            commands::brokers::fyers_get_history,
            commands::brokers::fyers_get_depth,
            commands::brokers::fyers_calculate_margin,
            commands::brokers::fyers_download_master_contract,
            commands::brokers::fyers_search_symbol,
            commands::brokers::fyers_get_token_for_symbol,
            commands::brokers::fyers_get_symbol_by_token,
            commands::brokers::fyers_get_master_contract_metadata,
            commands::brokers::fyers_ws_connect,
            commands::brokers::fyers_ws_disconnect,
            commands::brokers::fyers_ws_subscribe,
            commands::brokers::fyers_ws_subscribe_batch,
            commands::brokers::fyers_ws_unsubscribe,
            commands::brokers::fyers_ws_is_connected,
            // Upstox Broker Commands
            commands::brokers::upstox_exchange_token,
            commands::brokers::upstox_validate_token,
            commands::brokers::upstox_place_order,
            commands::brokers::upstox_modify_order,
            commands::brokers::upstox_cancel_order,
            commands::brokers::upstox_get_orders,
            commands::brokers::upstox_get_trade_book,
            commands::brokers::upstox_get_positions,
            commands::brokers::upstox_get_holdings,
            commands::brokers::upstox_get_funds,
            commands::brokers::upstox_get_quotes,
            commands::brokers::upstox_get_history,
            commands::brokers::upstox_get_depth,
            commands::brokers::upstox_download_master_contract,
            commands::brokers::upstox_search_symbol,
            commands::brokers::upstox_get_instrument_key,
            commands::brokers::upstox_get_master_contract_metadata,
            // Dhan Broker Commands
            commands::brokers::dhan_generate_consent,
            commands::brokers::dhan_exchange_token,
            commands::brokers::dhan_validate_token,
            commands::brokers::dhan_place_order,
            commands::brokers::dhan_modify_order,
            commands::brokers::dhan_cancel_order,
            commands::brokers::dhan_get_orders,
            commands::brokers::dhan_get_trade_book,
            commands::brokers::dhan_get_positions,
            commands::brokers::dhan_get_holdings,
            commands::brokers::dhan_get_funds,
            commands::brokers::dhan_get_quotes,
            commands::brokers::dhan_get_history,
            commands::brokers::dhan_get_depth,
            commands::brokers::dhan_download_master_contract,
            commands::brokers::dhan_search_symbol,
            commands::brokers::dhan_get_security_id,
            commands::brokers::dhan_get_master_contract_metadata,
            // Kotak Neo Broker Commands
            commands::brokers::kotak_totp_login,
            commands::brokers::kotak_mpin_validate,
            commands::brokers::kotak_validate_token,
            commands::brokers::kotak_place_order,
            commands::brokers::kotak_modify_order,
            commands::brokers::kotak_cancel_order,
            commands::brokers::kotak_get_orders,
            commands::brokers::kotak_get_trade_book,
            commands::brokers::kotak_get_positions,
            commands::brokers::kotak_get_holdings,
            commands::brokers::kotak_get_funds,
            commands::brokers::kotak_get_quotes,
            commands::brokers::kotak_get_depth,
            commands::brokers::kotak_get_history,
            commands::brokers::kotak_download_master_contract,
            commands::brokers::kotak_search_symbol,
            commands::brokers::kotak_get_token_for_symbol,
            commands::brokers::kotak_get_symbol_by_token,
            commands::brokers::kotak_get_master_contract_metadata,
            // Groww Broker Commands
            commands::brokers::groww_get_access_token,
            commands::brokers::groww_validate_token,
            commands::brokers::groww_place_order,
            commands::brokers::groww_modify_order,
            commands::brokers::groww_cancel_order,
            commands::brokers::groww_get_orders,
            commands::brokers::groww_get_positions,
            commands::brokers::groww_get_holdings,
            commands::brokers::groww_get_margins,
            commands::brokers::groww_get_quote,
            commands::brokers::groww_get_historical,
            commands::brokers::groww_get_depth,
            // Alice Blue Broker Commands
            commands::brokers::aliceblue_get_session,
            commands::brokers::aliceblue_validate_session,
            commands::brokers::aliceblue_place_order,
            commands::brokers::aliceblue_modify_order,
            commands::brokers::aliceblue_cancel_order,
            commands::brokers::aliceblue_get_orders,
            commands::brokers::aliceblue_get_trades,
            commands::brokers::aliceblue_get_positions,
            commands::brokers::aliceblue_get_holdings,
            commands::brokers::aliceblue_get_margins,
            commands::brokers::aliceblue_get_quote,
            commands::brokers::aliceblue_get_historical,
            // 5Paisa Broker Commands
            commands::brokers::fivepaisa_totp_login,
            commands::brokers::fivepaisa_get_access_token,
            commands::brokers::fivepaisa_validate_token,
            commands::brokers::fivepaisa_place_order,
            commands::brokers::fivepaisa_modify_order,
            commands::brokers::fivepaisa_cancel_order,
            commands::brokers::fivepaisa_get_orders,
            commands::brokers::fivepaisa_get_trades,
            commands::brokers::fivepaisa_get_positions,
            commands::brokers::fivepaisa_get_holdings,
            commands::brokers::fivepaisa_get_margins,
            commands::brokers::fivepaisa_get_quote,
            commands::brokers::fivepaisa_get_historical,
            // IIFL Broker Commands
            commands::brokers::iifl_exchange_token,
            commands::brokers::iifl_get_feed_token,
            commands::brokers::iifl_validate_token,
            commands::brokers::iifl_place_order,
            commands::brokers::iifl_modify_order,
            commands::brokers::iifl_cancel_order,
            commands::brokers::iifl_get_orders,
            commands::brokers::iifl_get_trade_book,
            commands::brokers::iifl_get_positions,
            commands::brokers::iifl_get_holdings,
            commands::brokers::iifl_get_funds,
            commands::brokers::iifl_get_quotes,
            commands::brokers::iifl_get_history,
            commands::brokers::iifl_get_depth,
            commands::brokers::iifl_download_master_contract,
            commands::brokers::iifl_search_symbol,
            commands::brokers::iifl_get_token_for_symbol,
            commands::brokers::iifl_get_master_contract_metadata,
            commands::brokers::iifl_close_all_positions,
            commands::brokers::iifl_cancel_all_orders,
            // Motilal Oswal Commands
            commands::brokers::motilal_authenticate,
            commands::brokers::motilal_validate_token,
            commands::brokers::motilal_place_order,
            commands::brokers::motilal_modify_order,
            commands::brokers::motilal_cancel_order,
            commands::brokers::motilal_get_orders,
            commands::brokers::motilal_get_trade_book,
            commands::brokers::motilal_get_positions,
            commands::brokers::motilal_get_holdings,
            commands::brokers::motilal_get_funds,
            commands::brokers::motilal_get_quotes,
            commands::brokers::motilal_get_history,
            commands::brokers::motilal_get_depth,
            commands::brokers::motilal_close_all_positions,
            commands::brokers::motilal_cancel_all_orders,
            commands::brokers::motilal_download_master_contract,
            commands::brokers::motilal_search_symbols,
            // Shoonya (Finvasia) Broker Commands
            commands::brokers::shoonya_authenticate,
            commands::brokers::shoonya_place_order,
            commands::brokers::shoonya_modify_order,
            commands::brokers::shoonya_cancel_order,
            commands::brokers::shoonya_get_orders,
            commands::brokers::shoonya_get_trade_book,
            commands::brokers::shoonya_get_positions,
            commands::brokers::shoonya_get_holdings,
            commands::brokers::shoonya_get_funds,
            commands::brokers::shoonya_get_quotes,
            commands::brokers::shoonya_get_depth,
            commands::brokers::shoonya_get_history,
            commands::brokers::shoonya_download_master_contract,
            commands::brokers::shoonya_search_symbol,
            commands::brokers::shoonya_get_token_for_symbol,
            commands::brokers::shoonya_get_symbol_by_token,
            commands::brokers::shoonya_get_master_contract_metadata,
            // Indian Broker WebSocket Commands
            commands::brokers::upstox_ws_connect,
            commands::brokers::upstox_ws_disconnect,
            commands::brokers::upstox_ws_subscribe,
            commands::brokers::upstox_ws_unsubscribe,
            commands::brokers::dhan_ws_connect,
            commands::brokers::dhan_ws_disconnect,
            commands::brokers::dhan_ws_subscribe,
            commands::brokers::dhan_ws_unsubscribe,
            commands::brokers::angelone_login,
            commands::brokers::angelone_validate_token,
            commands::brokers::angelone_refresh_token,
            commands::brokers::angelone_search_symbols,
            commands::brokers::angelone_get_instrument,
            commands::brokers::angelone_download_master_contract,
            commands::brokers::angelone_master_contract_info,
            commands::brokers::angelone_get_quote,
            commands::brokers::angelone_get_historical,
            commands::brokers::angelone_place_order,
            commands::brokers::angelone_modify_order,
            commands::brokers::angelone_cancel_order,
            commands::brokers::angelone_get_order_book,
            commands::brokers::angelone_get_trade_book,
            commands::brokers::angelone_get_positions,
            commands::brokers::angelone_get_holdings,
            commands::brokers::angelone_get_rms,
            commands::brokers::angelone_calculate_margin,
            commands::brokers::angelone_ws_connect,
            commands::brokers::angelone_ws_disconnect,
            commands::brokers::angelone_ws_subscribe,
            commands::brokers::angelone_ws_unsubscribe,
            commands::brokers::kotak_ws_connect,
            commands::brokers::kotak_ws_disconnect,
            commands::brokers::kotak_ws_subscribe,
            commands::brokers::kotak_ws_unsubscribe,
            commands::brokers::groww_ws_connect,
            commands::brokers::groww_ws_disconnect,
            commands::brokers::groww_ws_subscribe,
            commands::brokers::groww_ws_unsubscribe,
            commands::brokers::aliceblue_ws_connect,
            commands::brokers::aliceblue_ws_disconnect,
            commands::brokers::aliceblue_ws_subscribe,
            commands::brokers::aliceblue_ws_unsubscribe,
            commands::brokers::fivepaisa_ws_connect,
            commands::brokers::fivepaisa_ws_disconnect,
            commands::brokers::fivepaisa_ws_subscribe,
            commands::brokers::fivepaisa_ws_unsubscribe,
            commands::brokers::iifl_ws_connect,
            commands::brokers::iifl_ws_disconnect,
            commands::brokers::iifl_ws_subscribe,
            commands::brokers::iifl_ws_unsubscribe,
            commands::brokers::motilal_ws_connect,
            commands::brokers::motilal_ws_disconnect,
            commands::brokers::motilal_ws_subscribe,
            commands::brokers::motilal_ws_unsubscribe,
            commands::brokers::shoonya_ws_connect,
            commands::brokers::shoonya_ws_disconnect,
            commands::brokers::shoonya_ws_subscribe,
            commands::brokers::shoonya_ws_unsubscribe,
            // Alpaca US Broker Commands (Paper/Live Trading)
            commands::brokers::alpaca_get_account,
            commands::brokers::alpaca_get_clock,
            commands::brokers::alpaca_get_calendar,
            commands::brokers::alpaca_get_account_configurations,
            commands::brokers::alpaca_update_account_configurations,
            commands::brokers::alpaca_get_account_activities,
            commands::brokers::alpaca_get_account_activities_by_type,
            commands::brokers::alpaca_get_portfolio_history,
            commands::brokers::alpaca_place_order,
            commands::brokers::alpaca_modify_order,
            commands::brokers::alpaca_cancel_order,
            commands::brokers::alpaca_get_orders,
            commands::brokers::alpaca_get_order,
            commands::brokers::alpaca_cancel_all_orders,
            commands::brokers::alpaca_place_smart_order,
            commands::brokers::alpaca_get_positions,
            commands::brokers::alpaca_get_position,
            commands::brokers::alpaca_close_position,
            commands::brokers::alpaca_close_all_positions,
            commands::brokers::alpaca_get_watchlists,
            commands::brokers::alpaca_create_watchlist,
            commands::brokers::alpaca_get_watchlist,
            commands::brokers::alpaca_update_watchlist,
            commands::brokers::alpaca_delete_watchlist,
            commands::brokers::alpaca_add_to_watchlist,
            commands::brokers::alpaca_remove_from_watchlist,
            commands::brokers::alpaca_get_snapshot,
            commands::brokers::alpaca_get_snapshots,
            commands::brokers::alpaca_get_bars,
            commands::brokers::alpaca_get_trades,
            commands::brokers::alpaca_get_quotes,
            commands::brokers::alpaca_get_latest_trade,
            commands::brokers::alpaca_get_latest_quote,
            commands::brokers::alpaca_get_latest_trades,
            commands::brokers::alpaca_get_latest_quotes,
            commands::brokers::alpaca_get_multi_bars,
            commands::brokers::alpaca_search_assets,
            commands::brokers::alpaca_get_asset,
            commands::brokers::alpaca_ws_connect,
            commands::brokers::alpaca_ws_disconnect,
            // Interactive Brokers (IBKR) US Broker Commands
            commands::brokers::ibkr_get_auth_status,
            commands::brokers::ibkr_tickle,
            commands::brokers::ibkr_logout,
            commands::brokers::ibkr_validate_sso,
            commands::brokers::ibkr_reauthenticate,
            commands::brokers::ibkr_get_accounts,
            commands::brokers::ibkr_switch_account,
            commands::brokers::ibkr_get_account_summary,
            commands::brokers::ibkr_get_account_ledger,
            commands::brokers::ibkr_get_account_allocation,
            commands::brokers::ibkr_get_positions,
            commands::brokers::ibkr_get_position_by_conid,
            commands::brokers::ibkr_invalidate_positions,
            commands::brokers::ibkr_get_orders,
            commands::brokers::ibkr_get_order,
            commands::brokers::ibkr_place_order,
            commands::brokers::ibkr_reply_to_order,
            commands::brokers::ibkr_modify_order,
            commands::brokers::ibkr_cancel_order,
            commands::brokers::ibkr_preview_order,
            commands::brokers::ibkr_search_contracts,
            commands::brokers::ibkr_get_contract,
            commands::brokers::ibkr_get_contract_rules,
            commands::brokers::ibkr_get_secdef_info,
            commands::brokers::ibkr_get_snapshot,
            commands::brokers::ibkr_unsubscribe_market_data,
            commands::brokers::ibkr_unsubscribe_all_market_data,
            commands::brokers::ibkr_get_historical_data,
            commands::brokers::ibkr_get_scanner_params,
            commands::brokers::ibkr_run_scanner,
            commands::brokers::ibkr_get_alerts,
            commands::brokers::ibkr_create_alert,
            commands::brokers::ibkr_delete_alert,
            commands::brokers::ibkr_get_trades,
            commands::brokers::ibkr_get_performance,
            commands::brokers::ibkr_get_transactions,
            commands::brokers::store_ibkr_credentials,
            // Saxo Bank European Broker Commands
            commands::brokers::saxo_exchange_token,
            commands::brokers::saxo_refresh_token,
            commands::brokers::saxo_api_request,
            commands::brokers::saxo_get_user,
            commands::brokers::saxo_get_sessions,
            commands::brokers::saxo_get_features,
            commands::brokers::saxo_get_client,
            commands::brokers::saxo_get_accounts,
            commands::brokers::saxo_get_account,
            commands::brokers::saxo_get_balance,
            commands::brokers::saxo_get_orders,
            commands::brokers::saxo_get_order,
            commands::brokers::saxo_place_order,
            commands::brokers::saxo_modify_order,
            commands::brokers::saxo_cancel_order,
            commands::brokers::saxo_preview_order,
            commands::brokers::saxo_get_positions,
            commands::brokers::saxo_get_net_positions,
            commands::brokers::saxo_get_position,
            commands::brokers::saxo_close_position,
            commands::brokers::saxo_get_closed_positions,
            commands::brokers::saxo_search_instruments,
            commands::brokers::saxo_get_instrument,
            commands::brokers::saxo_get_quote,
            commands::brokers::saxo_get_quotes_batch,
            commands::brokers::saxo_get_chart_data,
            commands::brokers::saxo_get_chart_data_range,
            commands::brokers::saxo_get_exchanges,
            commands::brokers::saxo_get_exchange,
            commands::brokers::saxo_get_currencies,
            commands::brokers::saxo_get_countries,
            commands::brokers::saxo_get_timezones,
            commands::brokers::saxo_get_performance,
            commands::brokers::saxo_get_performance_timeseries,
            commands::brokers::saxo_get_order_audit,
            commands::brokers::saxo_create_price_subscription,
            commands::brokers::saxo_delete_price_subscription,
            commands::brokers::saxo_create_order_subscription,
            commands::brokers::saxo_create_position_subscription,
            commands::brokers::saxo_create_balance_subscription,
            commands::brokers::saxo_get_exposure,
            commands::brokers::saxo_get_option_chain,
            commands::brokers::saxo_get_option_expiries,
            // Tradier (US) broker commands
            commands::brokers::tradier_get_profile,
            commands::brokers::tradier_get_balances,
            commands::brokers::tradier_get_positions,
            commands::brokers::tradier_get_history,
            commands::brokers::tradier_get_gainloss,
            commands::brokers::tradier_get_orders,
            commands::brokers::tradier_get_order,
            commands::brokers::tradier_place_order,
            commands::brokers::tradier_modify_order,
            commands::brokers::tradier_cancel_order,
            commands::brokers::tradier_get_quotes,
            commands::brokers::tradier_get_historical,
            commands::brokers::tradier_get_timesales,
            commands::brokers::tradier_get_option_chains,
            commands::brokers::tradier_get_option_expirations,
            commands::brokers::tradier_get_option_strikes,
            commands::brokers::tradier_get_clock,
            commands::brokers::tradier_get_calendar,
            commands::brokers::tradier_search_symbols,
            commands::brokers::tradier_lookup_symbol,
            commands::brokers::tradier_get_stream_session,
            commands::brokers::tradier_ws_connect,
            commands::brokers::tradier_ws_subscribe,
            commands::brokers::tradier_ws_unsubscribe,
            commands::brokers::tradier_ws_disconnect,
            // Generic Storage Commands (SQLite key-value storage)
            commands::storage::storage_set,
            commands::storage::storage_get,
            commands::storage::storage_get_optional,
            commands::storage::storage_remove,
            commands::storage::storage_clear,
            commands::storage::storage_has,
            commands::storage::storage_keys,
            commands::storage::storage_size,
            commands::storage::storage_get_many,
            commands::storage::storage_set_many,
            // Unified Trading Commands (Live/Paper mode)
            commands::unified_trading::init_trading_session,
            commands::unified_trading::get_trading_session,
            commands::unified_trading::switch_trading_mode,
            commands::unified_trading::unified_place_order,
            commands::unified_trading::unified_get_positions,
            commands::unified_trading::unified_get_orders,
            commands::unified_trading::unified_cancel_order,
            commands::unified_trading::process_tick_for_paper_trading,
            // AKShare Commands (Free Chinese & Global market data)
            commands::akshare::akshare_query,
            commands::akshare::akshare_get_endpoints,
            commands::akshare::get_stock_cn_spot,
            commands::akshare::get_stock_us_spot,
            commands::akshare::get_stock_hk_spot,
            commands::akshare::get_stock_hot_rank,
            commands::akshare::get_stock_industry_list,
            commands::akshare::get_stock_concept_list,
            commands::akshare::get_stock_hsgt_data,
            commands::akshare::get_stock_industry_pe,
            commands::akshare::get_macro_china_gdp,
            commands::akshare::get_macro_china_cpi,
            commands::akshare::get_macro_china_pmi,
            commands::akshare::get_bond_china_yield,
            commands::akshare::get_fund_etf_spot,
            commands::akshare::get_fund_ranking,
            commands::akshare::get_stock_cn_info,
            commands::akshare::get_stock_cn_profile,
            commands::akshare::get_stock_hk_profile,
            commands::akshare::get_stock_us_info,
            commands::akshare::check_akshare_db_status,
            commands::akshare::build_akshare_database,
            // LLM Models Commands (LiteLLM provider and model listing)
            commands::llm_models::get_llm_providers,
            commands::llm_models::get_llm_models_by_provider,
            commands::llm_models::get_all_llm_models,
            commands::llm_models::get_llm_stats,
            commands::llm_models::search_llm_models,
            // M&A Analytics Commands (Complete M&A and Financial Advisory System)
            commands::ma_analytics::scan_ma_filings,
            commands::ma_analytics::parse_ma_filing,
            commands::ma_analytics::create_ma_deal,
            commands::ma_analytics::get_all_ma_deals,
            commands::ma_analytics::search_ma_deals,
            commands::ma_analytics::update_ma_deal,
            commands::ma_analytics::calculate_precedent_transactions,
            commands::ma_analytics::calculate_trading_comps,
            commands::ma_analytics::calculate_ma_dcf,
            commands::ma_analytics::calculate_dcf_sensitivity,
            commands::ma_analytics::generate_football_field,
            commands::ma_analytics::build_merger_model,
            commands::ma_analytics::calculate_accretion_dilution,
            commands::ma_analytics::build_pro_forma,
            commands::ma_analytics::calculate_sources_uses,
            commands::ma_analytics::analyze_contribution,
            commands::ma_analytics::build_lbo_model,
            commands::ma_analytics::calculate_lbo_returns,
            commands::ma_analytics::analyze_lbo_debt_schedule,
            commands::ma_analytics::calculate_lbo_sensitivity,
            commands::ma_analytics::calculate_berkus_valuation,
            commands::ma_analytics::calculate_scorecard_valuation,
            commands::ma_analytics::calculate_vc_method_valuation,
            commands::ma_analytics::calculate_first_chicago_valuation,
            commands::ma_analytics::calculate_risk_factor_valuation,
            commands::ma_analytics::comprehensive_startup_valuation,
            commands::ma_analytics::quick_pre_revenue_valuation,
            commands::ma_analytics::analyze_payment_structure,
            commands::ma_analytics::value_earnout,
            commands::ma_analytics::calculate_exchange_ratio,
            commands::ma_analytics::analyze_collar_mechanism,
            commands::ma_analytics::value_cvr,
            commands::ma_analytics::calculate_revenue_synergies,
            commands::ma_analytics::calculate_cost_synergies,
            commands::ma_analytics::estimate_integration_costs,
            commands::ma_analytics::value_synergies_dcf,
            commands::ma_analytics::generate_fairness_opinion,
            commands::ma_analytics::analyze_premium_fairness,
            commands::ma_analytics::assess_process_quality,
            commands::ma_analytics::calculate_tech_metrics,
            commands::ma_analytics::calculate_healthcare_metrics,
            commands::ma_analytics::calculate_financial_services_metrics,
            commands::ma_analytics::run_monte_carlo_valuation,
            commands::ma_analytics::run_regression_valuation,
            commands::ma_analytics::compare_deals,
            commands::ma_analytics::rank_deals,
            commands::ma_analytics::benchmark_deal_premium,
            commands::ma_analytics::analyze_payment_structures,
            commands::ma_analytics::analyze_industry_deals,
            // Market Simulation Engine Commands
            market_sim::commands::market_sim_start,
            market_sim::commands::market_sim_step,
            market_sim::commands::market_sim_run,
            market_sim::commands::market_sim_snapshot,
            market_sim::commands::market_sim_analytics,
            market_sim::commands::market_sim_orderbook,
            market_sim::commands::market_sim_inject_news,
            market_sim::commands::market_sim_stop,
            market_sim::commands::market_sim_events,
            // Strategy Engine
            commands::strategies::list_strategies,
            commands::strategies::read_strategy_source,
            commands::strategies::execute_fincept_strategy,
            commands::strategies::deploy_strategy,
            commands::strategies::stop_strategy,
            commands::strategies::stop_all_strategies,
            commands::strategies::list_deployed_strategies,
            commands::strategies::update_strategy
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
