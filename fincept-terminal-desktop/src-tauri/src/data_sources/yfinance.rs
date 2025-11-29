// YFinance data source wrapper using Python yfinance
// Modular design: if this fails, app continues working with other data sources

use serde::{Deserialize, Serialize};
use anyhow::{Result, Context};
use std::process::Command;
use std::path::PathBuf;

// Windows-specific imports to hide console windows
#[cfg(target_os = "windows")]
use std::os::windows::process::CommandExt;

#[cfg(target_os = "windows")]
const CREATE_NO_WINDOW: u32 = 0x08000000;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct QuoteData {
    pub symbol: String,
    pub price: f64,
    pub change: f64,
    pub change_percent: f64,
    pub volume: Option<u64>,
    pub high: Option<f64>,
    pub low: Option<f64>,
    pub open: Option<f64>,
    pub previous_close: Option<f64>,
    pub timestamp: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HistoricalData {
    pub symbol: String,
    pub timestamp: i64,
    pub open: f64,
    pub high: f64,
    pub low: f64,
    pub close: f64,
    pub volume: u64,
    pub adj_close: f64,
}

pub struct YFinanceProvider {
    python_path: PathBuf,
    script_path: PathBuf,
}

impl YFinanceProvider {
    /// Create a new YFinance provider instance with runtime paths
    pub fn new(app: &tauri::AppHandle) -> Result<Self> {
        let python_path = crate::utils::python::get_python_path(app)
            .map_err(|e| anyhow::anyhow!(e))?;
        let script_path = crate::utils::python::get_script_path(app, "yfinance_data.py")
            .map_err(|e| anyhow::anyhow!(e))?;

        Ok(Self {
            python_path,
            script_path,
        })
    }

    /// Fetch real-time quote for a single symbol
    /// Returns None if fetch fails (graceful degradation)
    pub async fn get_quote(&self, symbol: &str) -> Option<QuoteData> {
        match self.fetch_quote(symbol).await {
            Ok(quote) => Some(quote),
            Err(e) => {
                eprintln!("[YFinance] Failed to fetch quote for {}: {}", symbol, e);
                None
            }
        }
    }

    /// Fetch real-time quotes for multiple symbols
    /// Returns only successful fetches (partial success)
    pub async fn get_quotes(&self, symbols: Vec<String>) -> Vec<QuoteData> {
        match self.fetch_batch_quotes(symbols).await {
            Ok(quotes) => quotes,
            Err(e) => {
                eprintln!("[YFinance] Batch fetch failed: {}", e);
                Vec::new()
            },
        }
    }

    /// Internal batch fetch method - calls Python yfinance script with multiple symbols
    async fn fetch_batch_quotes(&self, symbols: Vec<String>) -> Result<Vec<QuoteData>> {
        let mut cmd = Command::new(&self.python_path);
        cmd.arg(&self.script_path).arg("batch_quotes");

        for symbol in symbols {
            cmd.arg(symbol);
        }

        // Hide console window on Windows
        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd
            .output()
            .context("Failed to execute Python script")?;

        if !output.status.success() {
            let error = String::from_utf8_lossy(&output.stderr);
            anyhow::bail!("Python script failed: {}", error);
        }

        let json_output = String::from_utf8_lossy(&output.stdout);
        let quotes: Vec<QuoteData> = serde_json::from_str(&json_output)
            .context("Failed to parse JSON from Python script")?;

        Ok(quotes)
    }

    /// Internal fetch method - calls Python yfinance script
    async fn fetch_quote(&self, symbol: &str) -> Result<QuoteData> {
        let mut cmd = Command::new(&self.python_path);
        cmd.arg(&self.script_path)
            .arg("quote")
            .arg(symbol);

        // Hide console window on Windows
        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd
            .output()
            .context("Failed to execute Python script")?;

        if !output.status.success() {
            let error = String::from_utf8_lossy(&output.stderr);
            anyhow::bail!("Python script failed: {}", error);
        }

        let json_output = String::from_utf8_lossy(&output.stdout);
        let quote: QuoteData = serde_json::from_str(&json_output)
            .context("Failed to parse JSON from Python script")?;

        Ok(quote)
    }

    /// Fetch historical data for a symbol
    /// Returns None if fetch fails (graceful degradation)
    pub async fn get_historical(
        &self,
        symbol: &str,
        start_date: &str,
        end_date: &str,
    ) -> Option<Vec<HistoricalData>> {
        match self.fetch_historical(symbol, start_date, end_date).await {
            Ok(data) => Some(data),
            Err(_e) => {
                // Silent fail for production
                None
            }
        }
    }

    /// Internal historical fetch method - calls Python yfinance script
    async fn fetch_historical(
        &self,
        symbol: &str,
        start_date: &str,
        end_date: &str,
    ) -> Result<Vec<HistoricalData>> {
        let mut cmd = Command::new(&self.python_path);
        cmd.arg(&self.script_path)
            .arg("historical")
            .arg(symbol)
            .arg(start_date)
            .arg(end_date);

        // Hide console window on Windows
        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd
            .output()
            .context("Failed to execute Python script")?;

        if !output.status.success() {
            let error = String::from_utf8_lossy(&output.stderr);
            anyhow::bail!("Python script failed: {}", error);
        }

        let json_output = String::from_utf8_lossy(&output.stdout);
        let historical: Vec<HistoricalData> = serde_json::from_str(&json_output)
            .context("Failed to parse JSON from Python script")?;

        Ok(historical)
    }

    /// Calculate period returns (7D, 30D) for a symbol
    /// Uses historical data to calculate returns
    pub async fn get_period_returns(&self, symbol: &str) -> Option<(f64, f64)> {
        // Get current date and past dates
        let today = chrono::Utc::now().format("%Y-%m-%d").to_string();
        let _seven_days_ago = (chrono::Utc::now() - chrono::Duration::days(10)).format("%Y-%m-%d").to_string();
        let thirty_days_ago = (chrono::Utc::now() - chrono::Duration::days(35)).format("%Y-%m-%d").to_string();

        // Fetch historical data
        let hist_data = self.get_historical(symbol, &thirty_days_ago, &today).await?;

        if hist_data.len() < 2 {
            return None;
        }

        let current_price = hist_data.last()?.close;

        // Calculate 7-day return
        let seven_day_return = if hist_data.len() >= 7 {
            let idx = hist_data.len() - 7;
            let old_price = hist_data.get(idx)?.close;
            ((current_price - old_price) / old_price) * 100.0
        } else {
            0.0
        };

        // Calculate 30-day return
        let thirty_day_return = if hist_data.len() >= 20 {
            let old_price = hist_data.first()?.close;
            ((current_price - old_price) / old_price) * 100.0
        } else {
            0.0
        };

        Some((seven_day_return, thirty_day_return))
    }

    /// Health check - tests if the provider is working
    pub async fn health_check(&self) -> bool {
        self.get_quote("AAPL").await.is_some()
    }

    /// Fetch stock info (company data, metrics)
    pub async fn get_info(&self, symbol: &str) -> Option<serde_json::Value> {
        match self.fetch_info(symbol).await {
            Ok(info) => Some(info),
            Err(_) => None,
        }
    }

    /// Internal info fetch method - calls Python yfinance script
    async fn fetch_info(&self, symbol: &str) -> Result<serde_json::Value> {
        let mut cmd = Command::new(&self.python_path);
        cmd.arg(&self.script_path)
            .arg("info")
            .arg(symbol);

        // Hide console window on Windows
        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd
            .output()
            .context("Failed to execute Python script")?;

        if !output.status.success() {
            let error = String::from_utf8_lossy(&output.stderr);
            anyhow::bail!("Python script failed: {}", error);
        }

        let json_output = String::from_utf8_lossy(&output.stdout);
        let info: serde_json::Value = serde_json::from_str(&json_output)
            .context("Failed to parse JSON from Python script")?;

        Ok(info)
    }

    /// Fetch financial statements
    pub async fn get_financials(&self, symbol: &str) -> Option<serde_json::Value> {
        match self.fetch_financials(symbol).await {
            Ok(financials) => Some(financials),
            Err(_) => None,
        }
    }

    /// Internal financials fetch method - calls Python yfinance script
    async fn fetch_financials(&self, symbol: &str) -> Result<serde_json::Value> {
        let mut cmd = Command::new(&self.python_path);
        cmd.arg(&self.script_path)
            .arg("financials")
            .arg(symbol);

        // Hide console window on Windows
        #[cfg(target_os = "windows")]
        cmd.creation_flags(CREATE_NO_WINDOW);

        let output = cmd
            .output()
            .context("Failed to execute Python script")?;

        if !output.status.success() {
            let error = String::from_utf8_lossy(&output.stderr);
            anyhow::bail!("Python script failed: {}", error);
        }

        let json_output = String::from_utf8_lossy(&output.stdout);
        let financials: serde_json::Value = serde_json::from_str(&json_output)
            .context("Failed to parse JSON from Python script")?;

        Ok(financials)
    }
}

// Removed Default implementation - requires AppHandle

// Tests removed - require AppHandle which isn't available in unit tests
