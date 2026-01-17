//! Indian Stock Brokers Module
//!
//! This module contains broker-specific integrations for Indian stock brokers.
//! Each broker has its own file with complete API implementation.
//!
//! ## Available Brokers:
//! - **Fyers**: Full-featured broker with OAuth, orders, positions, WebSocket
//! - **Zerodha**: Kite Connect API with smart orders, bulk operations
//!
//! ## Module Structure:
//! ```
//! brokers/
//! ├── common.rs      - Shared types and utilities
//! ├── fyers.rs       - Fyers broker (23 commands)
//! ├── zerodha.rs     - Zerodha broker (24 commands)
//! └── mod.rs         - This file (re-exports)
//! ```

pub mod common;
pub mod common_commands;
pub mod fyers;
pub mod zerodha;

// Re-export everything from fyers (includes tauri command wrappers)
pub use fyers::*;

// Re-export everything from zerodha (includes tauri command wrappers)
pub use zerodha::*;

// Re-export common utility commands
pub use common_commands::*;
