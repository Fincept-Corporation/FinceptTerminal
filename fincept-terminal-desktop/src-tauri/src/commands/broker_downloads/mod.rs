//! Broker Master Contract Downloads
//!
//! This module contains master contract download implementations for all supported brokers.
//! Each broker downloads from their public CSV/JSON endpoints and transforms to the unified symbol format.

pub mod fyers;
pub mod zerodha;
pub mod angelone;
pub mod upstox;
pub mod dhan;
pub mod shoonya;

// Re-export commonly used types
pub use fyers::fyers_download_symbols;
pub use zerodha::zerodha_download_symbols;
pub use angelone::angelone_download_symbols;
pub use upstox::upstox_download_symbols;
pub use dhan::dhan_download_symbols;
pub use shoonya::shoonya_download_symbols;
