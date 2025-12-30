// Backend Services - consume WebSocket data in Rust

pub mod paper_trading;
pub mod arbitrage;
pub mod portfolio;

pub use paper_trading::PaperTradingService;
pub use arbitrage::ArbitrageService;
pub use portfolio::PortfolioService;
