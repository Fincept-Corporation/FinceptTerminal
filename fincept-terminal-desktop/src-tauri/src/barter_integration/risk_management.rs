/// Risk Management Module
/// Provides position sizing, stop-loss, risk limits, and leverage controls

use crate::barter_integration::types::*;
use rust_decimal::Decimal;
use serde::{Deserialize, Serialize};
use std::collections::HashMap;

/// Position sizing method
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum PositionSizingMethod {
    /// Fixed dollar amount per trade
    FixedAmount(Decimal),
    /// Fixed percentage of portfolio
    FixedPercentage(Decimal),
    /// Kelly Criterion (f = (bp - q) / b where b=odds, p=win prob, q=loss prob)
    Kelly { win_rate: Decimal, avg_win: Decimal, avg_loss: Decimal },
    /// Risk-based (risk % of capital per trade)
    RiskBased { risk_per_trade: Decimal },
    /// Volatility-based (ATR multiplier)
    VolatilityBased { atr_multiplier: Decimal },
}

/// Stop-loss configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum StopLossType {
    /// No stop-loss
    None,
    /// Fixed percentage from entry
    FixedPercentage(Decimal),
    /// Fixed dollar amount
    FixedAmount(Decimal),
    /// Trailing stop (follows price)
    Trailing { percentage: Decimal },
    /// ATR-based stop
    ATRBased { multiplier: Decimal, period: usize },
}

/// Take-profit configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum TakeProfitType {
    /// No take-profit
    None,
    /// Fixed percentage from entry
    FixedPercentage(Decimal),
    /// Fixed dollar amount
    FixedAmount(Decimal),
    /// Risk-reward ratio (e.g., 2:1)
    RiskRewardRatio(Decimal),
}

/// Risk limits configuration
#[derive(Debug, Clone, Serialize, Deserialize, Default)]
pub struct RiskLimits {
    /// Maximum drawdown before stopping (e.g., 0.10 = 10%)
    pub max_drawdown: Option<Decimal>,
    /// Maximum daily loss
    pub max_daily_loss: Option<Decimal>,
    /// Maximum position size as % of portfolio
    pub max_position_size: Option<Decimal>,
    /// Maximum number of concurrent positions
    pub max_positions: Option<usize>,
    /// Maximum leverage allowed
    pub max_leverage: Option<Decimal>,
    /// Maximum risk per trade as % of portfolio
    pub max_risk_per_trade: Option<Decimal>,
}

/// Risk management configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RiskConfig {
    pub position_sizing: PositionSizingMethod,
    pub stop_loss: StopLossType,
    pub take_profit: TakeProfitType,
    pub limits: RiskLimits,
}

impl Default for RiskConfig {
    fn default() -> Self {
        Self {
            position_sizing: PositionSizingMethod::FixedPercentage(Decimal::new(2, 2)), // 2%
            stop_loss: StopLossType::FixedPercentage(Decimal::new(2, 2)), // 2%
            take_profit: TakeProfitType::RiskRewardRatio(Decimal::new(2, 0)), // 2:1
            limits: RiskLimits {
                max_drawdown: Some(Decimal::new(20, 2)), // 20%
                max_daily_loss: Some(Decimal::new(5, 2)), // 5%
                max_position_size: Some(Decimal::new(10, 2)), // 10%
                max_positions: Some(5),
                max_leverage: Some(Decimal::new(3, 0)), // 3x
                max_risk_per_trade: Some(Decimal::new(2, 2)), // 2%
            },
        }
    }
}

/// Risk manager state
pub struct RiskManager {
    config: RiskConfig,
    initial_capital: Decimal,
    peak_equity: Decimal,
    daily_start_equity: Decimal,
    active_positions: HashMap<String, PositionRisk>,
}

/// Position risk tracking
#[derive(Debug, Clone)]
struct PositionRisk {
    symbol: String,
    entry_price: Decimal,
    quantity: Decimal,
    stop_loss_price: Option<Decimal>,
    take_profit_price: Option<Decimal>,
    trailing_stop_price: Option<Decimal>,
    atr_values: Vec<Decimal>, // For ATR-based stops
}

impl RiskManager {
    pub fn new(config: RiskConfig, initial_capital: Decimal) -> Self {
        Self {
            config,
            initial_capital,
            peak_equity: initial_capital,
            daily_start_equity: initial_capital,
            active_positions: HashMap::new(),
        }
    }

    /// Calculate position size based on configured method
    pub fn calculate_position_size(
        &self,
        current_capital: Decimal,
        entry_price: Decimal,
        stop_loss_price: Option<Decimal>,
    ) -> BarterResult<Decimal> {
        let size = match &self.config.position_sizing {
            PositionSizingMethod::FixedAmount(amount) => {
                amount / entry_price
            }
            PositionSizingMethod::FixedPercentage(pct) => {
                (current_capital * pct) / entry_price
            }
            PositionSizingMethod::Kelly { win_rate, avg_win, avg_loss } => {
                if *avg_loss == Decimal::ZERO {
                    return Err(BarterError::Config("Average loss cannot be zero for Kelly".into()));
                }
                let win_loss_ratio = avg_win / avg_loss;
                let kelly_fraction = (win_rate * win_loss_ratio - (Decimal::ONE - win_rate)) / win_loss_ratio;
                let kelly_fraction = kelly_fraction.max(Decimal::ZERO).min(Decimal::new(25, 2)); // Cap at 25%
                (current_capital * kelly_fraction) / entry_price
            }
            PositionSizingMethod::RiskBased { risk_per_trade } => {
                if let Some(stop_price) = stop_loss_price {
                    let risk_amount = current_capital * risk_per_trade;
                    let price_risk = (entry_price - stop_price).abs();
                    if price_risk == Decimal::ZERO {
                        return Err(BarterError::Config("Stop loss too close to entry".into()));
                    }
                    risk_amount / price_risk
                } else {
                    return Err(BarterError::Config("Stop loss required for risk-based sizing".into()));
                }
            }
            PositionSizingMethod::VolatilityBased { atr_multiplier } => {
                // Simplified - would need actual ATR calculation
                (current_capital * Decimal::new(2, 2)) / entry_price / atr_multiplier
            }
        };

        // Apply max position size limit
        if let Some(max_pct) = self.config.limits.max_position_size {
            let max_size = (current_capital * max_pct) / entry_price;
            Ok(size.min(max_size))
        } else {
            Ok(size)
        }
    }

    /// Calculate stop-loss price
    pub fn calculate_stop_loss(
        &self,
        side: OrderSide,
        entry_price: Decimal,
        atr: Option<Decimal>,
    ) -> Option<Decimal> {
        match &self.config.stop_loss {
            StopLossType::None => None,
            StopLossType::FixedPercentage(pct) => {
                match side {
                    OrderSide::Buy => Some(entry_price * (Decimal::ONE - pct)),
                    OrderSide::Sell => Some(entry_price * (Decimal::ONE + pct)),
                }
            }
            StopLossType::FixedAmount(amount) => {
                match side {
                    OrderSide::Buy => Some(entry_price - amount),
                    OrderSide::Sell => Some(entry_price + amount),
                }
            }
            StopLossType::Trailing { percentage } => {
                // Initial stop is same as fixed percentage
                match side {
                    OrderSide::Buy => Some(entry_price * (Decimal::ONE - percentage)),
                    OrderSide::Sell => Some(entry_price * (Decimal::ONE + percentage)),
                }
            }
            StopLossType::ATRBased { multiplier, .. } => {
                if let Some(atr_value) = atr {
                    match side {
                        OrderSide::Buy => Some(entry_price - (atr_value * multiplier)),
                        OrderSide::Sell => Some(entry_price + (atr_value * multiplier)),
                    }
                } else {
                    None
                }
            }
        }
    }

    /// Calculate take-profit price
    pub fn calculate_take_profit(
        &self,
        side: OrderSide,
        entry_price: Decimal,
        stop_loss_price: Option<Decimal>,
    ) -> Option<Decimal> {
        match &self.config.take_profit {
            TakeProfitType::None => None,
            TakeProfitType::FixedPercentage(pct) => {
                match side {
                    OrderSide::Buy => Some(entry_price * (Decimal::ONE + pct)),
                    OrderSide::Sell => Some(entry_price * (Decimal::ONE - pct)),
                }
            }
            TakeProfitType::FixedAmount(amount) => {
                match side {
                    OrderSide::Buy => Some(entry_price + amount),
                    OrderSide::Sell => Some(entry_price - amount),
                }
            }
            TakeProfitType::RiskRewardRatio(ratio) => {
                if let Some(stop_price) = stop_loss_price {
                    let risk = (entry_price - stop_price).abs();
                    match side {
                        OrderSide::Buy => Some(entry_price + (risk * ratio)),
                        OrderSide::Sell => Some(entry_price - (risk * ratio)),
                    }
                } else {
                    None
                }
            }
        }
    }

    /// Update trailing stop based on current price
    pub fn update_trailing_stop(
        &mut self,
        symbol: &str,
        current_price: Decimal,
    ) -> Option<Decimal> {
        if let Some(position) = self.active_positions.get_mut(symbol) {
            if let StopLossType::Trailing { percentage } = &self.config.stop_loss {
                let is_long = position.quantity > Decimal::ZERO;

                if is_long {
                    let new_stop = current_price * (Decimal::ONE - percentage);
                    if let Some(current_stop) = position.trailing_stop_price {
                        if new_stop > current_stop {
                            position.trailing_stop_price = Some(new_stop);
                            return Some(new_stop);
                        }
                    } else {
                        position.trailing_stop_price = Some(new_stop);
                        return Some(new_stop);
                    }
                } else {
                    let new_stop = current_price * (Decimal::ONE + percentage);
                    if let Some(current_stop) = position.trailing_stop_price {
                        if new_stop < current_stop {
                            position.trailing_stop_price = Some(new_stop);
                            return Some(new_stop);
                        }
                    } else {
                        position.trailing_stop_price = Some(new_stop);
                        return Some(new_stop);
                    }
                }
            }
        }
        None
    }

    /// Check if stop-loss or take-profit is hit
    pub fn check_exit_signals(
        &self,
        symbol: &str,
        current_price: Decimal,
    ) -> Option<ExitSignal> {
        if let Some(position) = self.active_positions.get(symbol) {
            let is_long = position.quantity > Decimal::ZERO;

            // Check stop-loss
            let stop_price = position.trailing_stop_price.or(position.stop_loss_price);
            if let Some(stop) = stop_price {
                if (is_long && current_price <= stop) || (!is_long && current_price >= stop) {
                    return Some(ExitSignal::StopLoss);
                }
            }

            // Check take-profit
            if let Some(tp) = position.take_profit_price {
                if (is_long && current_price >= tp) || (!is_long && current_price <= tp) {
                    return Some(ExitSignal::TakeProfit);
                }
            }
        }
        None
    }

    /// Check if risk limits are breached
    pub fn check_risk_limits(&mut self, current_equity: Decimal) -> Vec<RiskLimitBreach> {
        let mut breaches = Vec::new();

        // Update peak equity
        if current_equity > self.peak_equity {
            self.peak_equity = current_equity;
        }

        // Check max drawdown
        if let Some(max_dd) = self.config.limits.max_drawdown {
            let current_dd = (self.peak_equity - current_equity) / self.peak_equity;
            if current_dd > max_dd {
                breaches.push(RiskLimitBreach::MaxDrawdown {
                    current: current_dd,
                    limit: max_dd
                });
            }
        }

        // Check max daily loss
        if let Some(max_daily) = self.config.limits.max_daily_loss {
            let daily_loss = (self.daily_start_equity - current_equity) / self.daily_start_equity;
            if daily_loss > max_daily {
                breaches.push(RiskLimitBreach::MaxDailyLoss {
                    current: daily_loss,
                    limit: max_daily
                });
            }
        }

        // Check max positions
        if let Some(max_pos) = self.config.limits.max_positions {
            if self.active_positions.len() >= max_pos {
                breaches.push(RiskLimitBreach::MaxPositions {
                    current: self.active_positions.len(),
                    limit: max_pos
                });
            }
        }

        breaches
    }

    /// Register new position for tracking
    pub fn register_position(
        &mut self,
        symbol: String,
        side: OrderSide,
        entry_price: Decimal,
        quantity: Decimal,
        atr: Option<Decimal>,
    ) {
        let stop_loss = self.calculate_stop_loss(side, entry_price, atr);
        let take_profit = self.calculate_take_profit(side, entry_price, stop_loss);

        self.active_positions.insert(
            symbol.clone(),
            PositionRisk {
                symbol,
                entry_price,
                quantity: if side == OrderSide::Buy { quantity } else { -quantity },
                stop_loss_price: stop_loss,
                take_profit_price: take_profit,
                trailing_stop_price: None,
                atr_values: Vec::new(),
            },
        );
    }

    /// Remove position from tracking
    pub fn remove_position(&mut self, symbol: &str) {
        self.active_positions.remove(symbol);
    }

    /// Reset daily tracking (call at start of new day)
    pub fn reset_daily(&mut self, current_equity: Decimal) {
        self.daily_start_equity = current_equity;
    }

    /// Get active position count
    pub fn active_position_count(&self) -> usize {
        self.active_positions.len()
    }
}

/// Exit signal types
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
pub enum ExitSignal {
    StopLoss,
    TakeProfit,
}

/// Risk limit breach types
#[derive(Debug, Clone, Serialize, Deserialize)]
pub enum RiskLimitBreach {
    MaxDrawdown { current: Decimal, limit: Decimal },
    MaxDailyLoss { current: Decimal, limit: Decimal },
    MaxPositions { current: usize, limit: usize },
}
