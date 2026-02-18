//! Algo Trading module
//!
//! Refactored from monolithic algo_trading.rs into modular structure for better maintainability.
//!
//! ## Module Structure
//! - `helpers`: Shared helper functions
//! - `strategy`: Strategy CRUD operations
//! - `deployment`: Deployment lifecycle management
//! - `trades`: Trade history operations
//! - `scanner`: Market scanner
//! - `candles`: Candle cache and aggregation
//! - `backtest`: Backtesting engine
//! - `order_bridge`: Order signal processing for live trading
//! - `monitoring`: Diagnostics and monitoring
//! - `python_strategies`: Python strategy library management

mod helpers;
mod strategy;
mod deployment;
mod trades;
mod scanner;
mod candles;
mod backtest;
mod order_bridge;
mod monitoring;
mod python_strategies;

// Re-export all public commands
pub use strategy::*;
pub use deployment::*;
pub use trades::*;
pub use scanner::*;
pub use candles::*;
pub use backtest::*;
pub use order_bridge::*;
pub use monitoring::*;
pub use python_strategies::*;
