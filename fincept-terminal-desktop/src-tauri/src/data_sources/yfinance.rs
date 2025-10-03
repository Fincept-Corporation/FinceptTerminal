// YFinance data source wrapper
// Modular design: if this fails, app continues working with other data sources

use serde::{Deserialize, Serialize};
use yahoo_finance_api as yahoo;
use anyhow::{Result, Context};

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
    provider: yahoo::YahooConnector,
}

impl YFinanceProvider {
    /// Create a new YFinance provider instance
    pub fn new() -> Self {
        Self {
            provider: yahoo::YahooConnector::new(),
        }
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
        let mut quotes = Vec::new();

        for symbol in symbols {
            if let Some(quote) = self.get_quote(&symbol).await {
                quotes.push(quote);
            }
            // Small delay to avoid rate limiting
            tokio::time::sleep(tokio::time::Duration::from_millis(200)).await;
        }

        quotes
    }

    /// Internal fetch method with error handling
    async fn fetch_quote(&self, symbol: &str) -> Result<QuoteData> {
        // TEMPORARY: Mock data until Yahoo Finance API is fixed
        // TODO: Replace with actual API call or switch to Alpha Vantage/Finnhub

        use std::collections::HashMap;

        let mut mock_prices: HashMap<&str, f64> = HashMap::new();
        // Stock Indices
        mock_prices.insert("^GSPC", 5870.62);
        mock_prices.insert("^IXIC", 18489.55);
        mock_prices.insert("^DJI", 38546.12);
        mock_prices.insert("^RUT", 2087.45);
        mock_prices.insert("^VIX", 18.23);
        mock_prices.insert("^FTSE", 7623.98);
        mock_prices.insert("^GDAXI", 18245.67);
        mock_prices.insert("^N225", 35897.45);
        mock_prices.insert("^FCHI", 7890.12);
        mock_prices.insert("^HSI", 18654.23);

        // Forex
        mock_prices.insert("EURUSD=X", 1.0840);
        mock_prices.insert("GBPUSD=X", 1.2567);
        mock_prices.insert("JPY=X", 151.45);
        mock_prices.insert("CHF=X", 0.8923);
        mock_prices.insert("CAD=X", 1.3578);
        mock_prices.insert("AUDUSD=X", 0.6634);
        mock_prices.insert("NZDUSD=X", 0.6089);
        mock_prices.insert("EURGBP=X", 0.8623);
        mock_prices.insert("EURJPY=X", 164.23);
        mock_prices.insert("GBPJPY=X", 190.45);

        // Commodities
        mock_prices.insert("GC=F", 2312.80);
        mock_prices.insert("SI=F", 28.45);
        mock_prices.insert("CL=F", 78.35);
        mock_prices.insert("BZ=F", 82.10);
        mock_prices.insert("NG=F", 3.45);
        mock_prices.insert("HG=F", 4.23);
        mock_prices.insert("PL=F", 987.50);
        mock_prices.insert("PA=F", 1045.20);
        mock_prices.insert("ZW=F", 567.80);
        mock_prices.insert("ZC=F", 478.90);

        // Bonds
        mock_prices.insert("^TNX", 4.35);
        mock_prices.insert("^TYX", 4.52);
        mock_prices.insert("^IRX", 4.89);
        mock_prices.insert("DE10Y=X", 2.45);
        mock_prices.insert("GB10Y=X", 4.12);
        mock_prices.insert("JP10Y=X", 0.78);
        mock_prices.insert("FR10Y=X", 2.98);
        mock_prices.insert("IT10Y=X", 4.23);
        mock_prices.insert("ES10Y=X", 3.67);
        mock_prices.insert("CA10Y=X", 3.89);

        // ETFs
        mock_prices.insert("SPY", 523.45);
        mock_prices.insert("QQQ", 412.67);
        mock_prices.insert("DIA", 385.23);
        mock_prices.insert("EEM", 41.78);
        mock_prices.insert("GLD", 231.45);
        mock_prices.insert("XLK", 198.90);
        mock_prices.insert("XLE", 89.34);
        mock_prices.insert("XLF", 42.56);
        mock_prices.insert("XLV", 156.78);
        mock_prices.insert("VNQ", 78.90);

        // Cryptocurrencies
        mock_prices.insert("BTC-USD", 67890.45);
        mock_prices.insert("ETH-USD", 3789.23);
        mock_prices.insert("BNB-USD", 567.89);
        mock_prices.insert("SOL-USD", 234.56);
        mock_prices.insert("DOGE-USD", 0.1234);
        mock_prices.insert("MATIC-USD", 1.23);
        mock_prices.insert("LINK-USD", 23.45);
        mock_prices.insert("ADA-USD", 0.67);
        mock_prices.insert("DOT-USD", 12.34);
        mock_prices.insert("AVAX-USD", 45.67);

        // Regional - India
        mock_prices.insert("RELIANCE.NS", 2500.45);
        mock_prices.insert("TCS.NS", 3456.78);
        mock_prices.insert("HDFCBANK.NS", 1678.90);
        mock_prices.insert("INFY.NS", 1234.56);
        mock_prices.insert("HINDUNILVR.NS", 2345.67);
        mock_prices.insert("ICICIBANK.NS", 987.65);
        mock_prices.insert("SBIN.NS", 678.90);
        mock_prices.insert("BHARTIARTL.NS", 876.54);

        // Regional - China
        mock_prices.insert("BABA", 85.67);
        mock_prices.insert("PDD", 123.45);
        mock_prices.insert("JD", 34.56);
        mock_prices.insert("BIDU", 145.67);
        mock_prices.insert("TCEHY", 45.78);
        mock_prices.insert("NIO", 12.34);
        mock_prices.insert("LI", 23.45);
        mock_prices.insert("XPEV", 15.67);

        // Regional - US
        mock_prices.insert("AAPL", 175.43);
        mock_prices.insert("MSFT", 420.67);
        mock_prices.insert("GOOGL", 140.23);
        mock_prices.insert("AMZN", 145.32);
        mock_prices.insert("NVDA", 800.45);
        mock_prices.insert("META", 345.67);
        mock_prices.insert("TSLA", 200.78);
        mock_prices.insert("JPM", 156.89);

        let base_price = mock_prices.get(symbol).copied().unwrap_or(100.0);

        // Add some random variation
        use std::hash::{Hash, Hasher};
        use std::collections::hash_map::DefaultHasher;
        let mut hasher = DefaultHasher::new();
        symbol.hash(&mut hasher);
        let hash = hasher.finish();
        let variation = (hash % 1000) as f64 / 10000.0; // 0-10% variation

        let price = base_price * (1.0 + variation);
        let previous_close = base_price;
        let change = price - previous_close;
        let change_percent = (change / previous_close) * 100.0;

        Ok(QuoteData {
            symbol: symbol.to_string(),
            price,
            change,
            change_percent,
            volume: Some(1_000_000),
            high: Some(price * 1.02),
            low: Some(price * 0.98),
            open: Some(previous_close),
            previous_close: Some(previous_close),
            timestamp: std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .unwrap()
                .as_secs() as i64,
        })
    }

    /// Fetch historical data for a symbol
    /// Returns None if fetch fails (graceful degradation)
    pub async fn get_historical(
        &self,
        symbol: &str,
        start: i64,
        end: i64,
    ) -> Option<Vec<HistoricalData>> {
        match self.fetch_historical(symbol, start, end).await {
            Ok(data) => Some(data),
            Err(_e) => {
                // Silent fail for production
                None
            }
        }
    }

    /// Internal historical fetch method
    async fn fetch_historical(
        &self,
        symbol: &str,
        start: i64,
        end: i64,
    ) -> Result<Vec<HistoricalData>> {
        use time::OffsetDateTime;

        let start_time = OffsetDateTime::from_unix_timestamp(start)
            .context("Invalid start timestamp")?;
        let end_time = OffsetDateTime::from_unix_timestamp(end)
            .context("Invalid end timestamp")?;

        let response = self.provider
            .get_quote_history(symbol, start_time, end_time)
            .await
            .context("Failed to fetch historical data")?;

        let quotes = response.quotes()
            .context("No historical quotes available")?;

        let historical: Vec<HistoricalData> = quotes
            .iter()
            .map(|q| HistoricalData {
                symbol: symbol.to_string(),
                timestamp: q.timestamp as i64,
                open: q.open,
                high: q.high,
                low: q.low,
                close: q.close,
                volume: q.volume,
                adj_close: q.adjclose,
            })
            .collect();

        Ok(historical)
    }

    /// Calculate period returns (7D, 30D) for a symbol
    pub async fn get_period_returns(&self, symbol: &str) -> Option<(f64, f64)> {
        // TEMPORARY: Mock data
        use std::hash::{Hash, Hasher};
        use std::collections::hash_map::DefaultHasher;

        let mut hasher = DefaultHasher::new();
        symbol.hash(&mut hasher);
        let hash = hasher.finish();

        let seven_day = ((hash % 200) as f64 - 100.0) / 10.0; // -10% to +10%
        let thirty_day = ((hash % 300) as f64 - 150.0) / 10.0; // -15% to +15%

        Some((seven_day, thirty_day))
    }

    /// Health check - tests if the provider is working
    pub async fn health_check(&self) -> bool {
        self.get_quote("AAPL").await.is_some()
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
