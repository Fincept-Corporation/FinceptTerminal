use crate::barter_integration::types::*;
use crate::barter_integration::strategy::{Strategy, Signal};
use rust_decimal::Decimal;
use chrono::{DateTime, Utc};
use std::collections::HashMap;

/// Backtest engine
pub struct BacktestEngine {
    config: BacktestConfig,
    capital: Decimal,
    positions: HashMap<String, Decimal>,
    trades: Vec<TradeRecord>,
    equity_curve: Vec<(DateTime<Utc>, Decimal)>,
}

impl BacktestEngine {
    pub fn new(config: BacktestConfig) -> Self {
        Self {
            capital: config.initial_capital,
            config,
            positions: HashMap::new(),
            trades: Vec::new(),
            equity_curve: Vec::new(),
        }
    }

    /// Run backtest with provided strategy and candle data
    pub async fn run(
        &mut self,
        strategy: &mut dyn Strategy,
        candle_data: HashMap<String, Vec<Candle>>,
    ) -> BarterResult<BacktestResult> {
        // Initialize strategy
        strategy.initialize(&self.config.strategy)?;

        // Collect all timestamps
        let mut all_timestamps: Vec<DateTime<Utc>> = candle_data
            .values()
            .flatten()
            .map(|c| c.timestamp)
            .collect();
        all_timestamps.sort();
        all_timestamps.dedup();

        // Filter timestamps within backtest range
        let timestamps: Vec<_> = all_timestamps
            .into_iter()
            .filter(|t| *t >= self.config.start_date && *t <= self.config.end_date)
            .collect();

        // Process each timestamp
        for timestamp in timestamps {
            for (symbol, candles) in &candle_data {
                if let Some(candle) = candles.iter().find(|c| c.timestamp == timestamp) {
                    // Feed candle to strategy
                    let signals = strategy.on_market_data(symbol, candle)?;

                    // Execute signals
                    for signal in signals {
                        self.execute_signal(&signal, candle)?;
                    }

                    // Update equity curve
                    let equity = self.calculate_equity(&candle_data, timestamp);
                    self.equity_curve.push((timestamp, equity));
                }
            }
        }

        // Close all open positions at end
        self.close_all_positions(&candle_data)?;

        // Calculate metrics
        self.calculate_results()
    }

    /// Execute trading signal
    fn execute_signal(&mut self, signal: &Signal, current_candle: &Candle) -> BarterResult<()> {
        let price = signal.price.unwrap_or(current_candle.close);
        let cost = price * signal.quantity;
        let commission = cost * self.config.commission_rate;

        match signal.side {
            OrderSide::Buy => {
                let total_cost = cost + commission;
                if self.capital >= total_cost {
                    self.capital -= total_cost;
                    *self.positions.entry(signal.symbol.clone()).or_insert(Decimal::ZERO) +=
                        signal.quantity;

                    tracing::debug!(
                        "BUY {} @ {}: qty={}, cost={}",
                        signal.symbol,
                        price,
                        signal.quantity,
                        total_cost
                    );
                }
            }
            OrderSide::Sell => {
                let position = self.positions.get(&signal.symbol).copied().unwrap_or(Decimal::ZERO);
                if position >= signal.quantity {
                    let proceeds = cost - commission;
                    self.capital += proceeds;
                    *self.positions.get_mut(&signal.symbol).unwrap() -= signal.quantity;

                    tracing::debug!(
                        "SELL {} @ {}: qty={}, proceeds={}",
                        signal.symbol,
                        price,
                        signal.quantity,
                        proceeds
                    );

                    // Record trade (simplified - assumes FIFO)
                    // In production, track entry/exit properly
                }
            }
        }

        Ok(())
    }

    /// Close all open positions
    fn close_all_positions(
        &mut self,
        candle_data: &HashMap<String, Vec<Candle>>,
    ) -> BarterResult<()> {
        for (symbol, quantity) in self.positions.iter() {
            if *quantity > Decimal::ZERO {
                if let Some(candles) = candle_data.get(symbol) {
                    if let Some(last_candle) = candles.last() {
                        let proceeds = last_candle.close * *quantity;
                        let commission = proceeds * self.config.commission_rate;
                        self.capital += proceeds - commission;
                    }
                }
            }
        }
        self.positions.clear();
        Ok(())
    }

    /// Calculate current equity
    fn calculate_equity(
        &self,
        candle_data: &HashMap<String, Vec<Candle>>,
        timestamp: DateTime<Utc>,
    ) -> Decimal {
        let mut equity = self.capital;

        for (symbol, quantity) in &self.positions {
            if let Some(candles) = candle_data.get(symbol) {
                if let Some(candle) = candles.iter().find(|c| c.timestamp == timestamp) {
                    equity += candle.close * *quantity;
                }
            }
        }

        equity
    }

    /// Calculate backtest results
    fn calculate_results(&self) -> BarterResult<BacktestResult> {
        let final_equity = self.equity_curve.last().map(|(_, e)| *e).unwrap_or(self.config.initial_capital);

        let total_return = if self.config.initial_capital > Decimal::ZERO {
            ((final_equity - self.config.initial_capital) / self.config.initial_capital) * Decimal::new(100, 0)
        } else {
            Decimal::ZERO
        };

        // Calculate returns for each period
        let returns: Vec<Decimal> = self
            .equity_curve
            .windows(2)
            .map(|w| {
                let prev = w[0].1;
                let curr = w[1].1;
                if prev > Decimal::ZERO {
                    (curr - prev) / prev
                } else {
                    Decimal::ZERO
                }
            })
            .collect();

        // Sharpe ratio (simplified)
        let avg_return = if !returns.is_empty() {
            returns.iter().sum::<Decimal>() / Decimal::new(returns.len() as i64, 0)
        } else {
            Decimal::ZERO
        };

        let variance = if returns.len() > 1 {
            returns
                .iter()
                .map(|r| {
                    let diff = *r - avg_return;
                    diff * diff // Use multiplication instead of powi
                })
                .sum::<Decimal>()
                / Decimal::new((returns.len() - 1) as i64, 0)
        } else {
            Decimal::ZERO
        };

        // Simple sqrt approximation for Decimal
        let std_dev = if variance > Decimal::ZERO {
            // Convert to f64, sqrt, then back to Decimal
            let var_f64: f64 = variance.to_string().parse().unwrap_or(0.0);
            Decimal::try_from(var_f64.sqrt()).unwrap_or(Decimal::ZERO)
        } else {
            Decimal::ZERO
        };

        let sharpe_ratio = if std_dev > Decimal::ZERO {
            let annualization_f64 = 252.0_f64.sqrt();
            let annualization = Decimal::try_from(annualization_f64).unwrap_or(Decimal::ONE);
            avg_return / std_dev * annualization
        } else {
            Decimal::ZERO
        };

        // Max drawdown
        let mut max_equity = Decimal::ZERO;
        let mut max_drawdown = Decimal::ZERO;

        for (_, equity) in &self.equity_curve {
            if *equity > max_equity {
                max_equity = *equity;
            }
            let drawdown = if max_equity > Decimal::ZERO {
                (max_equity - *equity) / max_equity
            } else {
                Decimal::ZERO
            };
            if drawdown > max_drawdown {
                max_drawdown = drawdown;
            }
        }

        // Trade statistics
        let total_trades = self.trades.len() as u64;
        let winning_trades = self.trades.iter().filter(|t| t.pnl > Decimal::ZERO).count() as u64;
        let losing_trades = total_trades - winning_trades;

        let win_rate = if total_trades > 0 {
            Decimal::new(winning_trades as i64, 0) / Decimal::new(total_trades as i64, 0) * Decimal::new(100, 0)
        } else {
            Decimal::ZERO
        };

        let total_wins: Decimal = self
            .trades
            .iter()
            .filter(|t| t.pnl > Decimal::ZERO)
            .map(|t| t.pnl)
            .sum();

        let total_losses: Decimal = self
            .trades
            .iter()
            .filter(|t| t.pnl < Decimal::ZERO)
            .map(|t| t.pnl.abs())
            .sum();

        let average_win = if winning_trades > 0 {
            total_wins / Decimal::new(winning_trades as i64, 0)
        } else {
            Decimal::ZERO
        };

        let average_loss = if losing_trades > 0 {
            total_losses / Decimal::new(losing_trades as i64, 0)
        } else {
            Decimal::ZERO
        };

        let profit_factor = if total_losses > Decimal::ZERO {
            total_wins / total_losses
        } else if total_wins > Decimal::ZERO {
            Decimal::new(999, 0) // Very high if no losses
        } else {
            Decimal::ZERO
        };

        // Sortino ratio (simplified - using same std for now)
        let sortino_ratio = sharpe_ratio; // TODO: Calculate properly with downside deviation

        Ok(BacktestResult {
            total_return,
            sharpe_ratio,
            sortino_ratio,
            max_drawdown,
            win_rate,
            total_trades,
            winning_trades,
            losing_trades,
            average_win,
            average_loss,
            profit_factor,
            final_capital: final_equity,
            equity_curve: self.equity_curve.clone(),
            trades: self.trades.clone(),
        })
    }
}

/// Backtest runner utility
pub struct BacktestRunner;

impl BacktestRunner {
    /// Run single backtest
    pub async fn run_backtest(
        config: BacktestConfig,
        strategy: &mut dyn Strategy,
        candle_data: HashMap<String, Vec<Candle>>,
    ) -> BarterResult<BacktestResult> {
        let mut engine = BacktestEngine::new(config);
        engine.run(strategy, candle_data).await
    }

    /// Run parameter optimization (grid search)
    pub async fn optimize_parameters(
        base_config: BacktestConfig,
        _strategy_name: &str,
        parameter_grid: HashMap<String, Vec<serde_json::Value>>,
        candle_data: HashMap<String, Vec<Candle>>,
    ) -> BarterResult<Vec<(HashMap<String, serde_json::Value>, BacktestResult)>> {
        let mut results = Vec::new();

        // Generate all parameter combinations
        let combinations = Self::generate_combinations(&parameter_grid);

        for params in combinations {
            let mut config = base_config.clone();
            config.strategy.parameters = params.clone();

            // Create strategy instance (simplified - needs proper factory)
            let mut strategy = crate::barter_integration::strategy::SmaStrategy::new();

            let result = Self::run_backtest(
                config,
                &mut strategy,
                candle_data.clone(),
            )
            .await?;

            results.push((params, result));
        }

        // Sort by Sharpe ratio
        results.sort_by(|a, b| {
            b.1.sharpe_ratio
                .partial_cmp(&a.1.sharpe_ratio)
                .unwrap_or(std::cmp::Ordering::Equal)
        });

        Ok(results)
    }

    fn generate_combinations(
        grid: &HashMap<String, Vec<serde_json::Value>>,
    ) -> Vec<HashMap<String, serde_json::Value>> {
        if grid.is_empty() {
            return vec![HashMap::new()];
        }

        let mut results = vec![HashMap::new()];

        for (key, values) in grid {
            let mut new_results = Vec::new();
            for result in results {
                for value in values {
                    let mut new_map = result.clone();
                    new_map.insert(key.clone(), value.clone());
                    new_results.push(new_map);
                }
            }
            results = new_results;
        }

        results
    }
}
