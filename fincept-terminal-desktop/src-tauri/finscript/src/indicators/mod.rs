/// Pure Rust implementations of technical indicators.
/// All functions operate on price/volume slices and return computed series.

mod moving_averages;
mod oscillators;
mod trend;
mod volume;
mod statistics;

#[cfg(test)]
mod tests;

// Re-export all indicator functions at the module level
// so callers can use `indicators::sma(...)` etc.

// Moving averages
pub use moving_averages::{sma, ema, wma, rma, hma};

// Oscillators
pub use oscillators::{rsi, stochastic, williams_r, cci, mfi, roc, momentum, percentrank};

// Trend
pub use trend::{macd, atr, adx, bollinger, sar, supertrend};

// Volume
pub use volume::{obv, vwap};

// Statistics & utilities
pub use statistics::{stdev, linreg, highest, lowest, change, cum, true_range, pivothigh, pivotlow};
