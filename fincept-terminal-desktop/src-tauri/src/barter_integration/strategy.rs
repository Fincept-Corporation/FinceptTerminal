use crate::barter_integration::types::*;
use rust_decimal::Decimal;
use std::collections::HashMap;
use serde::{Serialize, Deserialize};

/// Strategy trait that all trading strategies must implement
pub trait Strategy: Send + Sync {
    /// Strategy name
    fn name(&self) -> &str;

    /// Initialize strategy with configuration
    fn initialize(&mut self, config: &StrategyConfig) -> BarterResult<()>;

    /// Process new market data and generate signals
    fn on_market_data(
        &mut self,
        symbol: &str,
        candle: &Candle,
    ) -> BarterResult<Vec<Signal>>;

    /// Process trade execution
    fn on_order_filled(&mut self, order: &OrderResponse) -> BarterResult<()>;

    /// Risk management check before order submission
    fn validate_order(&self, request: &OrderRequest) -> BarterResult<bool>;

    /// Get strategy parameters
    fn get_parameters(&self) -> HashMap<String, serde_json::Value>;

    /// Update strategy parameters
    fn update_parameters(&mut self, params: HashMap<String, serde_json::Value>) -> BarterResult<()>;
}

/// Trading signal
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Signal {
    pub symbol: String,
    pub side: OrderSide,
    pub order_type: OrderType,
    pub quantity: Decimal,
    pub price: Option<Decimal>,
    pub confidence: f64,
    pub reason: String,
}

/// Simple moving average crossover strategy (example)
pub struct SmaStrategy {
    name: String,
    fast_period: usize,
    slow_period: usize,
    position_size: Decimal,
    candles: HashMap<String, Vec<Candle>>,
}

impl SmaStrategy {
    pub fn new() -> Self {
        Self {
            name: "SMA Crossover".to_string(),
            fast_period: 10,
            slow_period: 20,
            position_size: Decimal::new(100, 0),
            candles: HashMap::new(),
        }
    }

    fn calculate_sma(&self, candles: &[Candle], period: usize) -> Option<Decimal> {
        if candles.len() < period {
            return None;
        }

        let recent: Vec<_> = candles.iter().rev().take(period).collect();
        let sum: Decimal = recent.iter().map(|c| c.close).sum();
        Some(sum / Decimal::new(period as i64, 0))
    }
}

impl Default for SmaStrategy {
    fn default() -> Self {
        Self::new()
    }
}

impl Strategy for SmaStrategy {
    fn name(&self) -> &str {
        &self.name
    }

    fn initialize(&mut self, config: &StrategyConfig) -> BarterResult<()> {
        if let Some(fast) = config.parameters.get("fast_period") {
            if let Some(fast_val) = fast.as_u64() {
                self.fast_period = fast_val as usize;
            }
        }

        if let Some(slow) = config.parameters.get("slow_period") {
            if let Some(slow_val) = slow.as_u64() {
                self.slow_period = slow_val as usize;
            }
        }

        if let Some(size) = config.parameters.get("position_size") {
            if let Some(size_str) = size.as_str() {
                self.position_size = size_str.parse().map_err(|_| {
                    BarterError::Strategy("Invalid position size".to_string())
                })?;
            }
        }

        Ok(())
    }

    fn on_market_data(
        &mut self,
        symbol: &str,
        candle: &Candle,
    ) -> BarterResult<Vec<Signal>> {
        let candles = self.candles.entry(symbol.to_string()).or_insert_with(Vec::new);
        candles.push(candle.clone());

        // Keep only necessary candles
        let max_period = self.slow_period.max(self.fast_period);
        if candles.len() > max_period + 10 {
            candles.drain(0..candles.len() - max_period - 10);
        }

        let mut signals = Vec::new();

        // Get candle reference after modification
        let candles_ref = self.candles.get(symbol).unwrap();

        // Calculate SMAs
        if let (Some(fast_sma), Some(slow_sma)) = (
            self.calculate_sma(candles_ref, self.fast_period),
            self.calculate_sma(candles_ref, self.slow_period),
        ) {
            // Previous SMAs
            if candles_ref.len() > self.slow_period {
                let prev_candles: Vec<_> = candles_ref.iter()
                    .take(candles_ref.len() - 1)
                    .cloned()
                    .collect();

                if let (Some(prev_fast), Some(prev_slow)) = (
                    self.calculate_sma(&prev_candles, self.fast_period),
                    self.calculate_sma(&prev_candles, self.slow_period),
                ) {
                    // Bullish crossover
                    if fast_sma > slow_sma && prev_fast <= prev_slow {
                        signals.push(Signal {
                            symbol: symbol.to_string(),
                            side: OrderSide::Buy,
                            order_type: OrderType::Market,
                            quantity: self.position_size,
                            price: None,
                            confidence: 0.7,
                            reason: format!(
                                "SMA crossover: fast({}) crossed above slow({})",
                                fast_sma, slow_sma
                            ),
                        });
                    }
                    // Bearish crossover
                    else if fast_sma < slow_sma && prev_fast >= prev_slow {
                        signals.push(Signal {
                            symbol: symbol.to_string(),
                            side: OrderSide::Sell,
                            order_type: OrderType::Market,
                            quantity: self.position_size,
                            price: None,
                            confidence: 0.7,
                            reason: format!(
                                "SMA crossover: fast({}) crossed below slow({})",
                                fast_sma, slow_sma
                            ),
                        });
                    }
                }
            }
        }

        Ok(signals)
    }

    fn on_order_filled(&mut self, _order: &OrderResponse) -> BarterResult<()> {
        // Handle order fills if needed
        Ok(())
    }

    fn validate_order(&self, _request: &OrderRequest) -> BarterResult<bool> {
        // Basic validation - can be extended
        Ok(true)
    }

    fn get_parameters(&self) -> HashMap<String, serde_json::Value> {
        let mut params = HashMap::new();
        params.insert(
            "fast_period".to_string(),
            serde_json::json!(self.fast_period),
        );
        params.insert(
            "slow_period".to_string(),
            serde_json::json!(self.slow_period),
        );
        params.insert(
            "position_size".to_string(),
            serde_json::json!(self.position_size.to_string()),
        );
        params
    }

    fn update_parameters(&mut self, params: HashMap<String, serde_json::Value>) -> BarterResult<()> {
        if let Some(fast) = params.get("fast_period") {
            if let Some(fast_val) = fast.as_u64() {
                self.fast_period = fast_val as usize;
            }
        }

        if let Some(slow) = params.get("slow_period") {
            if let Some(slow_val) = slow.as_u64() {
                self.slow_period = slow_val as usize;
            }
        }

        if let Some(size) = params.get("position_size") {
            if let Some(size_str) = size.as_str() {
                self.position_size = size_str.parse().map_err(|_| {
                    BarterError::Strategy("Invalid position size".to_string())
                })?;
            }
        }

        Ok(())
    }
}

/// Strategy registry
pub struct StrategyRegistry {
    pub strategies: HashMap<String, Box<dyn Strategy>>,
}

impl StrategyRegistry {
    pub fn new() -> Self {
        Self {
            strategies: HashMap::new(),
        }
    }

    pub fn register(&mut self, strategy: Box<dyn Strategy>) {
        let name = strategy.name().to_string();
        self.strategies.insert(name, strategy);
    }

    pub fn get(&self, name: &str) -> Option<&dyn Strategy> {
        self.strategies.get(name).map(|s| s.as_ref())
    }

    pub fn get_mut(&mut self, name: &str) -> Option<&mut Box<dyn Strategy>> {
        self.strategies.get_mut(name)
    }

    pub fn list_strategies(&self) -> Vec<String> {
        self.strategies.keys().cloned().collect()
    }
}

impl Default for StrategyRegistry {
    fn default() -> Self {
        let mut registry = Self::new();
        registry.register(Box::new(SmaStrategy::new()));
        registry
    }
}
