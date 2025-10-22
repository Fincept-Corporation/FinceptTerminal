# Rust Developer Intern Guide
## Fincept Terminal - Performance & Native Integration

Welcome to the Fincept Terminal Rust development team! This guide will help you contribute to building performance-critical features and native integrations, even if you're new to the project or Rust.

---

## 📋 Table of Contents

1. [What You'll Be Building](#what-youll-be-building)
2. [Getting Started](#getting-started)
3. [Understanding the Architecture](#understanding-the-architecture)
4. [Your First Contribution](#your-first-contribution)
5. [Tauri Command Development](#tauri-command-development)
6. [Performance Optimization](#performance-optimization)
7. [Data Processing Modules](#data-processing-modules)
8. [Native Integration](#native-integration)
9. [Working with Python Scripts](#working-with-python-scripts)
10. [Code Standards & Best Practices](#code-standards--best-practices)
11. [Testing & Debugging](#testing--debugging)
12. [Submitting Your Work](#submitting-your-work)
13. [Resources & Learning](#resources--learning)

---

## 🎯 What You'll Be Building

As a Rust developer intern, your role focuses on **performance** and **native capabilities**:

- **Tauri Commands** - Bridge between frontend (React) and backend (Rust)
- **Data Processing** - High-performance calculations, parsing, transformations
- **Native Integrations** - File I/O, system APIs, database operations
- **Python Orchestration** - Execute Python scripts, manage processes
- **Performance Optimization** - Optimize slow operations, reduce memory usage
- **Concurrent Operations** - Async/await, parallel processing

### Why Rust?

Rust provides:
- **Speed** - Near C/C++ performance
- **Safety** - Memory safety without garbage collection
- **Concurrency** - Fearless parallel processing
- **Small Binaries** - Desktop app stays lightweight (~15MB vs 500MB+ Electron)

---

## 🚀 Getting Started

### Prerequisites

You need:
- **Rust** 1.70+ - https://www.rust-lang.org/tools/install
- **Node.js** 18+ - For frontend development
- **Git** - Version control
- A **code editor** with Rust support (VS Code + rust-analyzer)
- **Basic Rust knowledge** (ownership, borrowing, error handling)

### Setup Instructions

1. **Fork and Clone Repository**
```bash
# Fork on GitHub first, then:
git clone https://github.com/YOUR_USERNAME/FinceptTerminal.git
cd FinceptTerminal
```

2. **Navigate to Tauri Directory**
```bash
cd fincept-terminal-desktop/src-tauri
```

3. **Install Rust Dependencies**
```bash
# Check Rust installation
rustc --version
cargo --version

# Build project
cargo build
```

4. **Install Node Dependencies** (from parent directory)
```bash
cd ..
npm install
```

5. **Run Development Mode**
```bash
npm run tauri dev
```

---

## 🏗️ Understanding the Architecture

### How Tauri Works

```
┌─────────────────────────────────────────┐
│         Frontend (React/TypeScript)      │
│                                         │
│  User clicks button → invoke('command') │
└──────────────────┬──────────────────────┘
                   │
                   │ IPC (Inter-Process Communication)
                   │
┌──────────────────▼──────────────────────┐
│         Backend (Rust/Tauri)            │
│                                         │
│  #[tauri::command]                      │
│  async fn command() -> Result<...> {}   │
│                                         │
│  - File I/O                             │
│  - Database operations                  │
│  - Python script execution              │
│  - Heavy computations                   │
└─────────────────────────────────────────┘
```

### Project Structure

```
src-tauri/
├── src/
│   ├── main.rs              # Entry point
│   ├── lib.rs               # Library setup, command registration
│   │
│   ├── commands/            # Tauri commands (YOUR WORK HERE)
│   │   ├── mod.rs           # Module exports
│   │   └── market_data.rs   # Market data commands
│   │
│   └── data_sources/        # Data processing modules
│       ├── mod.rs
│       └── yfinance.rs      # yfinance integration
│
├── resources/               # Bundled resources
│   └── scripts/             # Python scripts
│       ├── yfinance_data.py
│       └── polygon_data.py
│
├── Cargo.toml               # Rust dependencies
└── tauri.conf.json          # Tauri configuration
```

### Key Dependencies (Cargo.toml)

```toml
[dependencies]
tauri = { version = "2", features = [] }
tauri-plugin-sql = { version = "2", features = ["sqlite"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
tokio = { version = "1", features = ["full"] }
anyhow = "1.0"
chrono = "0.4"
```

---

## 🎓 Your First Contribution

Let's build your first Tauri command - a simple calculator that demonstrates Rust performance!

### Step 1: Create Command Module

Create `src/commands/calculator.rs`:

```rust
use tauri::command;

/// Calculate moving average of a price series
/// This demonstrates Rust's performance for numerical computations
#[command]
pub fn calculate_moving_average(prices: Vec<f64>, window: usize) -> Result<Vec<f64>, String> {
    // Validate inputs
    if prices.is_empty() {
        return Err("Price array is empty".to_string());
    }

    if window == 0 || window > prices.len() {
        return Err(format!(
            "Invalid window size: {}. Must be between 1 and {}",
            window,
            prices.len()
        ));
    }

    // Calculate moving averages
    let mut result = Vec::new();

    for i in 0..=(prices.len() - window) {
        let window_slice = &prices[i..i + window];
        let sum: f64 = window_slice.iter().sum();
        let avg = sum / window as f64;
        result.push(avg);
    }

    Ok(result)
}

/// Calculate Relative Strength Index (RSI)
#[command]
pub fn calculate_rsi(prices: Vec<f64>, period: usize) -> Result<Vec<f64>, String> {
    if prices.len() < period + 1 {
        return Err("Not enough data points for RSI calculation".to_string());
    }

    let mut gains = Vec::new();
    let mut losses = Vec::new();

    // Calculate price changes
    for i in 1..prices.len() {
        let change = prices[i] - prices[i - 1];
        if change > 0.0 {
            gains.push(change);
            losses.push(0.0);
        } else {
            gains.push(0.0);
            losses.push(change.abs());
        }
    }

    // Calculate RSI values
    let mut rsi_values = Vec::new();

    for i in period..gains.len() {
        let avg_gain: f64 = gains[i - period..i].iter().sum::<f64>() / period as f64;
        let avg_loss: f64 = losses[i - period..i].iter().sum::<f64>() / period as f64;

        let rs = if avg_loss == 0.0 {
            100.0
        } else {
            avg_gain / avg_loss
        };

        let rsi = 100.0 - (100.0 / (1.0 + rs));
        rsi_values.push(rsi);
    }

    Ok(rsi_values)
}

/// Batch process multiple symbols' data
#[command]
pub async fn batch_process_data(
    symbols: Vec<String>,
    operation: String,
) -> Result<Vec<String>, String> {
    // Use Tokio for async operations
    let mut results = Vec::new();

    for symbol in symbols {
        let result = format!("Processed {} with operation: {}", symbol, operation);
        results.push(result);

        // Simulate async work
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
    }

    Ok(results)
}
```

### Step 2: Register Commands

Update `src/commands/mod.rs`:

```rust
pub mod calculator;
pub mod market_data;  // Existing module

// Re-export
pub use calculator::*;
pub use market_data::*;
```

Update `src/lib.rs`:

```rust
mod commands;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .invoke_handler(tauri::generate_handler![
            // Existing commands...
            commands::calculate_moving_average,
            commands::calculate_rsi,
            commands::batch_process_data,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
```

### Step 3: Test Your Command

Build and run:

```bash
cargo build
npm run tauri dev
```

Call from frontend (React):

```typescript
import { invoke } from '@tauri-apps/api/core';

// Test moving average
const prices = [100, 102, 101, 105, 107, 103, 108];
const result = await invoke<number[]>('calculate_moving_average', {
  prices,
  window: 3
});

console.log('Moving Average:', result);
// Output: [101.0, 102.67, 104.33, 105.0, 106.0]

// Test RSI
const rsi = await invoke<number[]>('calculate_rsi', {
  prices,
  period: 5
});

console.log('RSI:', rsi);
```

---

## 📚 Tauri Command Development

### Command Anatomy

```rust
use tauri::command;
use serde::{Serialize, Deserialize};

// Define request/response types
#[derive(Deserialize)]
pub struct CalculateRequest {
    pub data: Vec<f64>,
    pub options: CalculationOptions,
}

#[derive(Deserialize)]
pub struct CalculationOptions {
    pub method: String,
    pub window: usize,
}

#[derive(Serialize)]
pub struct CalculateResponse {
    pub success: bool,
    pub result: Vec<f64>,
    pub metadata: Metadata,
}

#[derive(Serialize)]
pub struct Metadata {
    pub calculation_time_ms: u64,
    pub data_points: usize,
}

/// Advanced calculation command with structured types
#[command]
pub async fn advanced_calculate(
    request: CalculateRequest
) -> Result<CalculateResponse, String> {
    let start = std::time::Instant::now();

    // Perform calculation
    let result = match request.options.method.as_str() {
        "sma" => calculate_sma(&request.data, request.options.window),
        "ema" => calculate_ema(&request.data, request.options.window),
        _ => return Err("Unknown calculation method".to_string()),
    }?;

    let elapsed = start.elapsed().as_millis() as u64;

    Ok(CalculateResponse {
        success: true,
        result,
        metadata: Metadata {
            calculation_time_ms: elapsed,
            data_points: request.data.len(),
        },
    })
}

// Helper functions
fn calculate_sma(data: &[f64], window: usize) -> Result<Vec<f64>, String> {
    // Implementation
    Ok(vec![])
}

fn calculate_ema(data: &[f64], window: usize) -> Result<Vec<f64>, String> {
    // Implementation
    Ok(vec![])
}
```

### Async Commands

```rust
use tokio::fs;

#[command]
pub async fn read_large_file(path: String) -> Result<String, String> {
    // Async file reading (non-blocking)
    let content = fs::read_to_string(&path)
        .await
        .map_err(|e| e.to_string())?;

    Ok(content)
}

#[command]
pub async fn parallel_fetch(urls: Vec<String>) -> Result<Vec<String>, String> {
    let mut handles = vec![];

    for url in urls {
        let handle = tokio::spawn(async move {
            // Fetch from URL
            format!("Data from {}", url)
        });
        handles.push(handle);
    }

    // Wait for all to complete
    let mut results = vec![];
    for handle in handles {
        let result = handle.await.map_err(|e| e.to_string())?;
        results.push(result);
    }

    Ok(results)
}
```

### Error Handling

```rust
use anyhow::{Result, Context};

#[command]
pub fn robust_command(input: String) -> Result<String, String> {
    // Use anyhow for better error handling
    let result = process_input(&input)
        .context("Failed to process input")
        .map_err(|e| e.to_string())?;

    Ok(result)
}

fn process_input(input: &str) -> Result<String> {
    if input.is_empty() {
        anyhow::bail!("Input cannot be empty");
    }

    // Processing logic
    Ok(input.to_uppercase())
}
```

---

## ⚡ Performance Optimization

### Parallel Processing

```rust
use rayon::prelude::*;

#[command]
pub fn process_large_dataset(data: Vec<f64>) -> Result<Vec<f64>, String> {
    // Use Rayon for parallel iteration
    let results: Vec<f64> = data
        .par_iter()  // Parallel iterator
        .map(|&x| {
            // Heavy computation per item
            expensive_calculation(x)
        })
        .collect();

    Ok(results)
}

fn expensive_calculation(value: f64) -> f64 {
    // Simulate expensive operation
    (0..1000).fold(value, |acc, _| acc * 1.001)
}
```

### Memory Optimization

```rust
#[command]
pub fn process_streaming_data(chunk_size: usize) -> Result<usize, String> {
    let mut count = 0;

    // Process in chunks instead of loading all at once
    for chunk in (0..1_000_000).collect::<Vec<_>>().chunks(chunk_size) {
        count += chunk.len();
        // Process chunk
    }

    Ok(count)
}
```

### Caching with Lazy Static

```rust
use std::sync::Mutex;
use lazy_static::lazy_static;
use std::collections::HashMap;

lazy_static! {
    static ref CACHE: Mutex<HashMap<String, String>> = Mutex::new(HashMap::new());
}

#[command]
pub fn cached_fetch(key: String) -> Result<String, String> {
    let mut cache = CACHE.lock().unwrap();

    // Check cache first
    if let Some(value) = cache.get(&key) {
        return Ok(value.clone());
    }

    // Expensive fetch
    let value = expensive_fetch(&key)?;

    // Cache result
    cache.insert(key, value.clone());

    Ok(value)
}

fn expensive_fetch(key: &str) -> Result<String, String> {
    // Simulate expensive operation
    std::thread::sleep(std::time::Duration::from_secs(1));
    Ok(format!("Data for {}", key))
}
```

---

## 🔧 Data Processing Modules

### Time Series Analysis

```rust
use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};

#[derive(Debug, Serialize, Deserialize)]
pub struct Candle {
    pub timestamp: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub volume: u64,
}

pub struct TimeSeriesProcessor;

impl TimeSeriesProcessor {
    /// Resample candles to different timeframe
    pub fn resample(candles: Vec<Candle>, target_period: u64) -> Vec<Candle> {
        // Group by target period and aggregate
        let mut result = Vec::new();

        // Implementation...

        result
    }

    /// Detect outliers using Z-score
    pub fn detect_outliers(data: &[f64], threshold: f64) -> Vec<usize> {
        let mean = data.iter().sum::<f64>() / data.len() as f64;
        let variance = data.iter()
            .map(|&x| (x - mean).powi(2))
            .sum::<f64>() / data.len() as f64;
        let std_dev = variance.sqrt();

        data.iter()
            .enumerate()
            .filter(|(_, &value)| {
                ((value - mean) / std_dev).abs() > threshold
            })
            .map(|(i, _)| i)
            .collect()
    }

    /// Calculate Bollinger Bands
    pub fn bollinger_bands(
        prices: &[f64],
        period: usize,
        std_multiplier: f64,
    ) -> Result<(Vec<f64>, Vec<f64>, Vec<f64>), String> {
        if prices.len() < period {
            return Err("Not enough data".to_string());
        }

        let mut upper = Vec::new();
        let mut middle = Vec::new();
        let mut lower = Vec::new();

        for i in period - 1..prices.len() {
            let window = &prices[i - period + 1..=i];
            let mean = window.iter().sum::<f64>() / period as f64;

            let variance = window.iter()
                .map(|&x| (x - mean).powi(2))
                .sum::<f64>() / period as f64;
            let std_dev = variance.sqrt();

            middle.push(mean);
            upper.push(mean + std_multiplier * std_dev);
            lower.push(mean - std_multiplier * std_dev);
        }

        Ok((upper, middle, lower))
    }
}
```

### CSV/JSON Parsing

```rust
use serde_json::Value;
use std::fs::File;
use std::io::BufReader;

#[command]
pub fn parse_financial_json(path: String) -> Result<Vec<FinancialRecord>, String> {
    let file = File::open(&path).map_err(|e| e.to_string())?;
    let reader = BufReader::new(file);

    let data: Value = serde_json::from_reader(reader)
        .map_err(|e| e.to_string())?;

    // Parse into structured format
    let records = data["data"]
        .as_array()
        .ok_or("Invalid JSON structure")?
        .iter()
        .filter_map(|item| {
            Some(FinancialRecord {
                symbol: item["symbol"].as_str()?.to_string(),
                price: item["price"].as_f64()?,
                volume: item["volume"].as_u64()?,
            })
        })
        .collect();

    Ok(records)
}

#[derive(Serialize)]
struct FinancialRecord {
    symbol: String,
    price: f64,
    volume: u64,
}
```

---

## 🐍 Working with Python Scripts

### Execute Python Script

```rust
use std::process::Command;

#[command]
pub async fn fetch_yfinance_quote(symbol: String) -> Result<String, String> {
    // Path to Python script
    let script_path = "resources/scripts/yfinance_data.py";

    // Execute Python script
    let output = Command::new("python")
        .arg(script_path)
        .arg("quote")
        .arg(&symbol)
        .output()
        .map_err(|e| format!("Failed to execute Python: {}", e))?;

    // Check if command succeeded
    if !output.status.success() {
        let stderr = String::from_utf8_lossy(&output.stderr);
        return Err(format!("Python script error: {}", stderr));
    }

    // Parse stdout as JSON
    let stdout = String::from_utf8_lossy(&output.stdout);
    Ok(stdout.to_string())
}
```

### Async Python Execution

```rust
use tokio::process::Command as TokioCommand;

#[command]
pub async fn async_fetch_data(symbols: Vec<String>) -> Result<Vec<String>, String> {
    let mut handles = vec![];

    for symbol in symbols {
        let handle = tokio::spawn(async move {
            let output = TokioCommand::new("python")
                .arg("resources/scripts/yfinance_data.py")
                .arg("quote")
                .arg(&symbol)
                .output()
                .await
                .map_err(|e| e.to_string())?;

            let stdout = String::from_utf8_lossy(&output.stdout);
            Ok::<String, String>(stdout.to_string())
        });

        handles.push(handle);
    }

    // Wait for all
    let mut results = vec![];
    for handle in handles {
        let result = handle.await.map_err(|e| e.to_string())??;
        results.push(result);
    }

    Ok(results)
}
```

---

## ✅ Code Standards & Best Practices

### Error Handling

```rust
// ✅ GOOD - Use Result types
#[command]
pub fn safe_operation(input: String) -> Result<String, String> {
    if input.is_empty() {
        return Err("Input cannot be empty".to_string());
    }

    Ok(input.to_uppercase())
}

// ❌ BAD - Don't panic in commands
#[command]
pub fn unsafe_operation(input: String) -> String {
    if input.is_empty() {
        panic!("Input is empty!");  // NEVER DO THIS
    }
    input.to_uppercase()
}
```

### Ownership & Borrowing

```rust
// ✅ GOOD - Borrow when possible
fn process_data(data: &[f64]) -> f64 {
    data.iter().sum()
}

// ❌ BAD - Unnecessary ownership transfer
fn process_data_bad(data: Vec<f64>) -> f64 {
    data.iter().sum()  // Consumes data
}
```

### Type Safety

```rust
// ✅ GOOD - Strong typing
#[derive(Deserialize)]
pub struct StockRequest {
    pub symbol: String,
    pub start_date: String,
    pub end_date: String,
}

#[command]
pub fn fetch_stock(request: StockRequest) -> Result<String, String> {
    // Type-safe!
    Ok(format!("Fetching {}", request.symbol))
}

// ❌ BAD - Weak typing
#[command]
pub fn fetch_stock_bad(symbol: String, dates: String) -> Result<String, String> {
    // Have to parse dates from string
    Ok(symbol)
}
```

### Documentation

```rust
/// Calculate the Simple Moving Average (SMA) for a price series
///
/// # Arguments
///
/// * `prices` - Vector of price data points
/// * `window` - Size of the moving window
///
/// # Returns
///
/// Vector of SMA values, or error if window is invalid
///
/// # Example
///
/// ```
/// let prices = vec![100.0, 102.0, 101.0, 105.0];
/// let sma = calculate_sma(prices, 2)?;
/// ```
#[command]
pub fn calculate_sma(prices: Vec<f64>, window: usize) -> Result<Vec<f64>, String> {
    // Implementation
    Ok(vec![])
}
```

---

## 🧪 Testing & Debugging

### Unit Tests

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_moving_average() {
        let prices = vec![100.0, 102.0, 104.0, 106.0];
        let result = calculate_moving_average(prices, 2).unwrap();

        assert_eq!(result.len(), 3);
        assert_eq!(result[0], 101.0);
        assert_eq!(result[1], 103.0);
        assert_eq!(result[2], 105.0);
    }

    #[test]
    fn test_empty_input() {
        let prices = vec![];
        let result = calculate_moving_average(prices, 2);

        assert!(result.is_err());
    }

    #[test]
    fn test_invalid_window() {
        let prices = vec![100.0, 102.0];
        let result = calculate_moving_average(prices, 5);

        assert!(result.is_err());
    }
}
```

### Running Tests

```bash
# Run all tests
cargo test

# Run specific test
cargo test test_moving_average

# Run tests with output
cargo test -- --nocapture
```

### Debugging

```rust
// Add debug prints
#[command]
pub fn debug_command(input: String) -> Result<String, String> {
    eprintln!("DEBUG: Input received: {}", input);

    let result = process(&input)?;

    eprintln!("DEBUG: Result: {}", result);

    Ok(result)
}
```

### Benchmarking

```rust
use std::time::Instant;

#[command]
pub fn benchmark_operation() -> Result<String, String> {
    let data = vec![1.0; 1_000_000];

    let start = Instant::now();
    let result = heavy_computation(&data);
    let elapsed = start.elapsed();

    Ok(format!(
        "Processed {} items in {:?}",
        result.len(),
        elapsed
    ))
}
```

---

## 🚀 Submitting Your Work

### Pre-Submission Checklist

- [ ] Code compiles without warnings (`cargo build`)
- [ ] All tests pass (`cargo test`)
- [ ] Code is formatted (`cargo fmt`)
- [ ] No clippy warnings (`cargo clippy`)
- [ ] Documentation added
- [ ] Error handling implemented
- [ ] Tested in Tauri app (`npm run tauri dev`)

### Formatting & Linting

```bash
# Format code
cargo fmt

# Check for issues
cargo clippy

# Fix auto-fixable issues
cargo clippy --fix
```

### Creating Pull Request

```bash
# Create branch
git checkout -b feature/rsi-calculator

# Commit
git add src/commands/calculator.rs
git commit -m "Add RSI calculator command

- Implement RSI calculation
- Add unit tests
- Optimize for large datasets
- Async processing support"

# Push
git push origin feature/rsi-calculator
```

---

## 📖 Resources & Learning

### Rust Learning

**Basics:**
- The Rust Book: https://doc.rust-lang.org/book/
- Rust by Example: https://doc.rust-lang.org/rust-by-example/
- Rustlings Exercises: https://github.com/rust-lang/rustlings

**Advanced:**
- Async Rust: https://rust-lang.github.io/async-book/
- Performance Book: https://nnethercote.github.io/perf-book/

### Tauri Specific

- Tauri Docs: https://tauri.app/
- Command System: https://tauri.app/v1/guides/features/command/
- Async Commands: https://tauri.app/v1/guides/features/command/#async-commands

### Performance

- Rayon (parallelism): https://docs.rs/rayon/
- Tokio (async): https://tokio.rs/
- Serde (serialization): https://serde.rs/

### Project Code

Study existing files:
- `src/lib.rs` - Command registration
- `src/commands/market_data.rs` - Example commands
- `Cargo.toml` - Dependencies

---

## 💡 Project Ideas

### Beginner (Week 1-2)
- Add simple calculation commands
- Implement data validation
- Create utility functions
- Write unit tests

### Intermediate (Week 3-6)
- Build data processing modules
- Optimize existing code
- Add SQLite database integration
- Create file I/O commands

### Advanced (Week 7+)
- Multi-threaded data processing
- WebSocket server for real-time data
- Custom technical indicators
- Performance profiling & optimization

---

## 🎉 Final Words

Welcome to Rust development at Fincept Terminal! Your work on performance and native capabilities is crucial for making this the fastest financial terminal available.

**Remember:**
- Safety first - use Result types
- Performance matters - profile before optimizing
- Test thoroughly
- Document well
- Ask questions when stuck

Your contributions make Fincept Terminal blazingly fast! 🚀

---

**Repository**: https://github.com/Fincept-Corporation/FinceptTerminal/
**Contact**: dev@fincept.in
