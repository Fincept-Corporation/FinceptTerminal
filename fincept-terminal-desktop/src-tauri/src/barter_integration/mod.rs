/// Barter-rs integration module
/// Provides live trading, paper trading, and backtesting capabilities
///
/// Architecture:
/// - market_data: Stream real-time market data from exchanges
/// - execution: Execute orders and manage positions
/// - strategy: Pluggable trading strategy interface
/// - backtest: Backtesting engine with performance metrics
/// - types: Common data structures and types
/// - commands: Tauri commands for frontend integration

pub mod types;
pub mod market_data;
pub mod execution;
pub mod strategy;
pub mod backtest;
pub mod commands;
pub mod data_mapper;
pub mod risk_management;

#[cfg(test)]
mod tests;

#[cfg(test)]
mod test_databento;

#[cfg(test)]
mod test_csv_mapping;

#[cfg(test)]
mod test_risk_management;

pub use types::*;
pub use commands::*;
