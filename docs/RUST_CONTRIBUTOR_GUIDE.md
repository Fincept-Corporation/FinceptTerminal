# Rust Contributor Guide

This guide covers Rust/Tauri development for Fincept Terminal - 99 commands, WebSocket handlers, database operations, and Python integration.

> **Prerequisites**: Read the [Contributing Guide](./CONTRIBUTING.md) first for setup and workflow.

---

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Command Categories](#command-categories)
- [Tauri Commands](#tauri-commands)
- [Python Integration](#python-integration)
- [Database Operations](#database-operations)
- [WebSocket Handlers](#websocket-handlers)
- [Code Standards](#code-standards)
- [Resources](#resources)

---

## Overview

The Rust backend handles:
- **99 Tauri commands** - Bridge between frontend and backend
- **Python execution** - Running 119 analytics scripts
- **Database** - SQLite operations via Tauri plugin
- **WebSocket** - Real-time data streaming
- **Native features** - File I/O, system APIs, performance

**Related Guides:**
- [TypeScript Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) - Frontend that calls commands
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) - Scripts executed by Rust

---

## Project Structure

```
src-tauri/
├── src/
│   ├── main.rs                    # Entry point
│   ├── lib.rs                     # Main library (130K+ lines)
│   ├── python.rs                  # Python runtime management
│   ├── setup.rs                   # App initialization
│   ├── paper_trading.rs           # Paper trading system
│   │
│   ├── commands/                  # 99 Tauri commands
│   │   ├── mod.rs                 # Module exports
│   │   │
│   │   ├── # Analytics & Data
│   │   ├── analytics.rs           # Financial analytics
│   │   ├── market_data.rs         # Market data
│   │   ├── technical_analysis.rs  # Technical indicators
│   │   ├── portfolio.rs           # Portfolio management
│   │   ├── quantstats.rs          # Performance metrics
│   │   │
│   │   ├── # Trading
│   │   ├── algo_trading.rs        # Algorithmic trading (81K)
│   │   ├── unified_trading.rs     # Unified trading API
│   │   ├── strategies.rs          # Trading strategies
│   │   ├── backtesting.rs         # Strategy backtesting
│   │   │
│   │   ├── # AI & Agents
│   │   ├── agents.rs              # AI agents
│   │   ├── ai_agents.rs           # AI agent management
│   │   ├── agent_streaming.rs     # Streaming responses
│   │   ├── ai_quant_lab.rs        # AI/ML analytics
│   │   ├── alpha_arena.rs         # Alpha Arena
│   │   │
│   │   ├── # Data Sources
│   │   ├── yfinance.rs            # Yahoo Finance
│   │   ├── databento.rs           # Databento
│   │   ├── akshare.rs             # Chinese markets
│   │   ├── fred.rs                # Federal Reserve
│   │   ├── imf.rs                 # IMF data
│   │   ├── nasdaq.rs              # NASDAQ
│   │   ├── sec.rs                 # SEC filings
│   │   ├── ecb.rs                 # European Central Bank
│   │   ├── bis.rs                 # Bank for Intl Settlements
│   │   ├── oecd.rs                # OECD data
│   │   └── ...                    # 50+ more data sources
│   │   │
│   │   ├── # Library Wrappers
│   │   ├── pyportfolioopt.rs      # Portfolio optimization
│   │   ├── skfolio.rs             # Scikit-portfolio
│   │   ├── gluonts.rs             # Time series
│   │   ├── gs_quant.rs            # GS Quant
│   │   ├── talipp.rs              # Technical indicators
│   │   │
│   │   ├── # Brokers
│   │   ├── brokers/               # Broker integrations
│   │   ├── broker_credentials.rs  # Credential management
│   │   │
│   │   ├── # System
│   │   ├── database.rs            # Database operations
│   │   ├── storage.rs             # File storage
│   │   ├── news.rs                # News aggregation
│   │   └── screener_commands.rs   # Stock screener
│   │
│   ├── database/                  # SQLite operations
│   ├── services/                  # Backend services
│   ├── websocket/                 # WebSocket handlers
│   ├── data_sources/              # Data processing
│   └── market_sim/                # Market simulation
│
├── resources/
│   └── scripts/                   # 119 Python scripts
│
├── Cargo.toml                     # Dependencies
└── tauri.conf.json                # Tauri configuration
```

---

## Command Categories

### By Function

| Category | Commands | Description |
|----------|----------|-------------|
| Analytics | 15+ | Financial calculations, metrics |
| Trading | 10+ | Order management, strategies |
| Data Sources | 50+ | External API integrations |
| AI/Agents | 8+ | LLM and agent commands |
| Portfolio | 10+ | Portfolio optimization |
| Database | 5+ | CRUD operations |
| System | 10+ | File, storage, news |

### By Size (Lines of Code)

| Command File | Lines | Purpose |
|--------------|-------|---------|
| `algo_trading.rs` | 81K | Algorithmic trading |
| `bis.rs` | 32K | BIS data |
| `databento.rs` | 29K | Market data |
| `agents.rs` | 25K | AI agents |
| `analytics.rs` | 22K | Financial analytics |
| `unified_trading.rs` | 25K | Trading API |

---

## Tauri Commands

### Basic Command

```rust
use tauri::command;

#[command]
pub fn greet(name: String) -> String {
    format!("Hello, {}!", name)
}
```

### Command with Structured Types

```rust
use serde::{Deserialize, Serialize};

#[derive(Deserialize)]
pub struct PortfolioRequest {
    pub symbols: Vec<String>,
    pub weights: Option<Vec<f64>>,
    pub optimize: bool,
}

#[derive(Serialize)]
pub struct PortfolioResponse {
    pub weights: Vec<f64>,
    pub expected_return: f64,
    pub volatility: f64,
    pub sharpe_ratio: f64,
}

#[command]
pub async fn optimize_portfolio(
    request: PortfolioRequest
) -> Result<PortfolioResponse, String> {
    // Validation
    if request.symbols.is_empty() {
        return Err("No symbols provided".to_string());
    }

    // Call Python script
    let result = run_python_script(
        "Analytics/portfolioManagement/optimize.py",
        &serde_json::to_string(&request).unwrap()
    ).await?;

    Ok(serde_json::from_str(&result).map_err(|e| e.to_string())?)
}
```

### Async Command

```rust
#[command]
pub async fn fetch_market_data(
    symbols: Vec<String>
) -> Result<Vec<Quote>, String> {
    let mut results = Vec::new();

    for symbol in symbols {
        let quote = fetch_quote(&symbol).await?;
        results.push(quote);
    }

    Ok(results)
}
```

### Registering Commands

In `lib.rs`:

```rust
tauri::Builder::default()
    .invoke_handler(tauri::generate_handler![
        commands::greet,
        commands::optimize_portfolio,
        commands::fetch_market_data,
        // ... 96 more commands
    ])
```

---

## Python Integration

### Execute Python Script

```rust
use std::process::Command;

pub async fn run_python_script(
    script: &str,
    args: &str
) -> Result<String, String> {
    let script_path = format!("resources/scripts/{}", script);

    let output = Command::new("python")
        .arg(&script_path)
        .arg(args)
        .output()
        .map_err(|e| format!("Failed to execute: {}", e))?;

    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Script error: {}", stderr));
    }

    Ok(String::from_utf8_lossy(&output.stdout).to_string())
}
```

### Parse JSON Response

```rust
#[derive(Deserialize)]
struct PythonResponse<T> {
    success: bool,
    data: Option<T>,
    error: Option<String>,
}

pub async fn call_analytics<T: DeserializeOwned>(
    script: &str,
    args: &str
) -> Result<T, String> {
    let output = run_python_script(script, args).await?;

    let response: PythonResponse<T> = serde_json::from_str(&output)
        .map_err(|e| format!("JSON parse error: {}", e))?;

    if response.success {
        response.data.ok_or("No data returned".to_string())
    } else {
        Err(response.error.unwrap_or("Unknown error".to_string()))
    }
}
```

---

## Database Operations

### SQLite via Tauri Plugin

```rust
use tauri_plugin_sql::{Migration, MigrationKind};

// Define migrations
let migrations = vec![
    Migration {
        version: 1,
        description: "create watchlist",
        sql: "CREATE TABLE watchlist (
            id INTEGER PRIMARY KEY,
            symbol TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )",
        kind: MigrationKind::Up,
    }
];
```

### Database Commands

```rust
#[command]
pub async fn save_watchlist(
    app: tauri::AppHandle,
    symbols: Vec<String>
) -> Result<(), String> {
    let db = app.state::<Database>();

    for symbol in symbols {
        sqlx::query("INSERT INTO watchlist (symbol) VALUES (?)")
            .bind(&symbol)
            .execute(&db.pool)
            .await
            .map_err(|e| e.to_string())?;
    }

    Ok(())
}
```

---

## WebSocket Handlers

### WebSocket Server

```rust
// src/websocket/mod.rs
use tokio_tungstenite::tungstenite::Message;

pub async fn handle_websocket(
    ws_stream: WebSocketStream<TcpStream>
) {
    let (mut write, mut read) = ws_stream.split();

    while let Some(msg) = read.next().await {
        match msg {
            Ok(Message::Text(text)) => {
                // Handle message
                let response = process_message(&text).await;
                write.send(Message::Text(response)).await.ok();
            }
            Ok(Message::Close(_)) => break,
            _ => {}
        }
    }
}
```

---

## Code Standards

### Error Handling

```rust
// ✅ Good - Use Result
#[command]
pub fn safe_operation(input: String) -> Result<String, String> {
    if input.is_empty() {
        return Err("Input cannot be empty".to_string());
    }
    Ok(input.to_uppercase())
}

// ❌ Bad - Never panic in commands
#[command]
pub fn bad_operation(input: String) -> String {
    if input.is_empty() {
        panic!("Empty!"); // Never do this
    }
    input
}
```

### Type Safety

```rust
// ✅ Good - Strong typing
#[derive(Deserialize)]
pub struct Request {
    pub symbol: String,
    pub period: String,
}

// ❌ Bad - Weak typing
pub fn bad_fn(data: String) -> String { ... }
```

### Pre-Submission Checklist

```bash
cargo build      # Compiles
cargo test       # Tests pass
cargo fmt        # Formatted
cargo clippy     # No warnings
```

---

## Resources

### Rust Learning

- [The Rust Book](https://doc.rust-lang.org/book/)
- [Rust by Example](https://doc.rust-lang.org/rust-by-example/)
- [Async Rust](https://rust-lang.github.io/async-book/)

### Tauri Documentation

- [Tauri v2 Docs](https://tauri.app/v2/)
- [Command System](https://tauri.app/v2/guides/features/command/)
- [Plugin System](https://tauri.app/v2/guides/plugins/)

### Key Crates

| Crate | Purpose |
|-------|---------|
| `tauri` | Desktop framework |
| `serde` | Serialization |
| `tokio` | Async runtime |
| `reqwest` | HTTP client |
| `sqlx` | Database |
| `anyhow` | Error handling |

### Related Guides

- [Contributing Guide](./CONTRIBUTING.md) - General workflow
- [TypeScript Guide](./TYPESCRIPT_CONTRIBUTOR_GUIDE.md) - Frontend
- [Python Guide](./PYTHON_CONTRIBUTOR_GUIDE.md) - Analytics scripts

---

**Questions?** Open an issue on [GitHub](https://github.com/Fincept-Corporation/FinceptTerminal).
