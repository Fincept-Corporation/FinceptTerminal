// WebSocket Module - High-performance multi-provider WebSocket manager
//
// Architecture:
// - Single connection per provider (connection pooling)
// - Multiplexed subscriptions (1000s of symbols per connection)
// - Event-driven message routing (broadcast channels)
// - Auto-reconnection with subscription restoration
// - Zero-copy message parsing and routing

pub mod types;
pub mod manager;
pub mod router;
pub mod adapters;
pub mod services;

pub use manager::WebSocketManager;
pub use router::MessageRouter;
