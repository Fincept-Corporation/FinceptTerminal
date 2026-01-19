//! Stock Brokers Module
//!
//! This module contains broker-specific integrations for stock brokers.
//! Each broker has its own file with complete API implementation.
//!
//! ## Available Indian Brokers:
//! - **Fyers**: Full-featured broker with OAuth, orders, positions, WebSocket
//! - **Zerodha**: Kite Connect API with smart orders, bulk operations
//! - **Upstox**: Full-featured broker with OAuth v2/v3 API
//! - **Dhan**: Full-featured broker with OAuth, orders, positions
//! - **Kotak**: Full-featured broker with TOTP+MPIN auth
//! - **Groww**: Groww trading platform integration
//! - **AliceBlue**: AliceBlue ANT API integration
//! - **5Paisa**: 5Paisa trading platform integration
//! - **IIFL**: IIFL Securities trading integration
//! - **Motilal**: Motilal Oswal trading integration
//! - **Shoonya**: Finvasia Shoonya trading integration
//!
//! ## Available US Brokers:
//! - **Alpaca**: Commission-free trading with paper/live modes, WebSocket streaming
//! - **IBKR**: Interactive Brokers Client Portal API with paper/live modes
//! - **Tradier**: US broker with OAuth, paper/live modes, options support, WebSocket streaming
//!
//! ## Available European Brokers:
//! - **Saxo Bank**: Pan-European broker with OAuth 2.0, 30+ exchanges, WebSocket streaming
//!
//! ## Module Structure:
//! ```
//! brokers/
//! ├── common.rs      - Shared types and utilities
//! ├── fyers.rs       - Fyers broker (23 commands)
//! ├── zerodha.rs     - Zerodha broker (24 commands)
//! ├── upstox.rs      - Upstox broker (18 commands)
//! ├── dhan.rs        - Dhan broker (17 commands)
//! ├── kotak.rs       - Kotak broker (18 commands)
//! ├── groww.rs       - Groww broker
//! ├── aliceblue.rs   - AliceBlue broker
//! ├── fivepaisa.rs   - 5Paisa broker
//! ├── iifl.rs        - IIFL broker
//! ├── motilal.rs     - Motilal Oswal broker
//! ├── shoonya.rs     - Shoonya broker
//! ├── alpaca.rs      - Alpaca US broker (43 commands)
//! ├── ibkr.rs        - Interactive Brokers US broker (35+ commands)
//! ├── tradier.rs     - Tradier US broker (20+ commands)
//! └── mod.rs         - This file (re-exports)
//! ```

pub mod common;
pub mod common_commands;

// Indian Brokers
pub mod fyers;
pub mod zerodha;
pub mod upstox;
pub mod dhan;
pub mod kotak;
pub mod groww;
pub mod aliceblue;
pub mod fivepaisa;
pub mod iifl;
pub mod motilal;
pub mod shoonya;

// US Brokers
pub mod alpaca;
pub mod ibkr;
pub mod tradier;

// European Brokers
pub mod saxobank;

// Re-export everything from fyers (includes tauri command wrappers)
pub use fyers::*;

// Re-export everything from zerodha (includes tauri command wrappers)
pub use zerodha::*;

// Re-export everything from upstox (includes tauri command wrappers)
pub use upstox::*;

// Re-export everything from dhan (includes tauri command wrappers)
pub use dhan::*;

// Re-export everything from kotak (includes tauri command wrappers)
pub use kotak::*;

// Re-export everything from groww (includes tauri command wrappers)
pub use groww::*;

// Re-export everything from aliceblue (includes tauri command wrappers)
pub use aliceblue::*;

// Re-export everything from fivepaisa (includes tauri command wrappers)
pub use fivepaisa::*;

// Re-export everything from iifl (includes tauri command wrappers)
pub use iifl::*;

// Re-export everything from motilal (includes tauri command wrappers)
pub use motilal::*;

// Re-export everything from shoonya (includes tauri command wrappers)
pub use shoonya::*;

// Re-export everything from alpaca (includes tauri command wrappers)
pub use alpaca::*;

// Re-export everything from ibkr (includes tauri command wrappers)
pub use ibkr::*;

// Re-export everything from tradier (includes tauri command wrappers)
pub use tradier::*;

// Re-export everything from saxobank (includes tauri command wrappers)
pub use saxobank::*;

// Re-export common utility commands
pub use common_commands::*;
