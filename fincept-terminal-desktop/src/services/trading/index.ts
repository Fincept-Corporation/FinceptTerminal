// File: src/services/trading/index.ts
// Trading services exports

// Market data service - unified source for prices
export * from './UnifiedMarketDataService';

// WebSocket bridge - communication with Rust backend
export * from './websocketBridge';

// Alpha Arena broker bridge - for AI competition
export * from './alphaArenaBrokerBridge';

// Trading types
export type * from '../../types/trading';

// Paper trading is now at src/paper-trading/
// Use: import { createPaperTradingAdapter } from '../../paper-trading';
