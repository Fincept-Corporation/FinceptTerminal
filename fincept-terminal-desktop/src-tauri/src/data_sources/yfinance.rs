// YFinance data source wrapper using Python yfinance
// Modular design: if this fails, app continues working with other data sources

use serde::{Deserialize, Serialize};
use anyhow::{Result, Context};
use std::process::Command;

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

pub struct YFinanceProvider {}

impl YFinanceProvider {
    /// Create a new YFinance provider instance
    pub fn new() -> Self {
        Self {}
    }

    /// Fetch real-time quote for a single symbol
    /// Returns None if fetch fails (graceful degradation)
    pub async fn get_quote(&self, symbol: &str) -> Option<QuoteData> {
        match self.fetch_quote(symbol).await {
            Ok(quote) => Some(quote),
            Err(_e) => {
                // Silent fail for production
                None
            }
        }
    }

    /// Fetch real-time quotes for multiple symbols
    /// Returns only successful fetches (partial success)
    pub async fn get_quotes(&self, symbols: Vec<String>) -> Vec<QuoteData> {
        match self.fetch_batch_quotes(symbols).await {
            Ok(quotes) => quotes,
            Err(_) => Vec::new(),
        }
    }

    /// Internal batch fetch method - calls Python yfinance script with multiple symbols
    async fn fetch_batch_quotes(&self, symbols: Vec<String>) -> Result<Vec<QuoteData>> {
        let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("python")
            .join("python.exe");

        let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("scripts")
            .join("yfinance_data.py");

        let mut cmd = Command::new(&python_path);
        cmd.arg(&script_path).arg("batch_quotes");

        for symbol in symbols {
            cmd.arg(symbol);
        }

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
        // Use absolute path from project root
        let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("python")
            .join("python.exe");

        let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("scripts")
            .join("yfinance_data.py");

        let output = Command::new(&python_path)
            .arg(&script_path)
            .arg("quote")
            .arg(symbol)
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
        let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("python")
            .join("python.exe");

        let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("scripts")
            .join("yfinance_data.py");

        let output = Command::new(&python_path)
            .arg(&script_path)
            .arg("historical")
            .arg(symbol)
            .arg(start_date)
            .arg(end_date)
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
        let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("python")
            .join("python.exe");

        let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("scripts")
            .join("yfinance_data.py");

        let output = Command::new(&python_path)
            .arg(&script_path)
            .arg("info")
            .arg(symbol)
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
        let python_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("python")
            .join("python.exe");

        let script_path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
            .join("resources")
            .join("scripts")
            .join("yfinance_data.py");

        let output = Command::new(&python_path)
            .arg(&script_path)
            .arg("financials")
            .arg(symbol)
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

impl Default for YFinanceProvider {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_quote_fetch() {
        let provider = YFinanceProvider::new();
        let quote = provider.get_quote("AAPL").await;
        assert!(quote.is_some());
    }

    #[tokio::test]
    async fn test_health_check() {
        let provider = YFinanceProvider::new();
        assert!(provider.health_check().await);
    }
}
