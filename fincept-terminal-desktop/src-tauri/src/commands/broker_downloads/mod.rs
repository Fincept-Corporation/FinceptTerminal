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

