// Backend Services - consume WebSocket data in Rust

pub mod paper_trading;
pub mod arbitrage;
pub mod portfolio;
pub mod monitoring;
pub mod candle_aggregator;

pub use paper_trading::PaperTradingService;
pub use arbitrage::ArbitrageService;
pub use portfolio::PortfolioService;
pub use monitoring::MonitoringService;
pub use candle_aggregator::CandleAggregatorService;
