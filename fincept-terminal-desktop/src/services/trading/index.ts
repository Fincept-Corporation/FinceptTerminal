// File: src/services/trading/index.ts
// Trading services exports

// DEPRECATED: Old paper trading service replaced by new adapter-based system
// The old service has been moved to paperTradingService.ts.deprecated
//
// Use the new institutional-grade paper trading adapter instead:
// import { createPaperTradingAdapter } from '../../brokers/crypto/paper-trading';
//
// Example:
// const adapter = createPaperTradingAdapter({
//   portfolioId: 'my-portfolio',
//   portfolioName: 'Test Strategy',
//   realExchange: 'kraken',
//   initialBalance: 100000,
//   fees: { maker: 0.0002, taker: 0.0005 },
//   slippage: { market: 0.001, limit: 0 },
// });

export type * from '../../types/trading';
