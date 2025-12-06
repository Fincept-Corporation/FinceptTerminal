/**
 * Universal Paper Trading Module
 *
 * Professional-grade paper trading system for ANY broker/exchange
 * Works with crypto, stocks, forex, commodities, options, futures, etc.
 * Drop-in replacement for real trading adapters
 */

export * from './types';
export { PaperTradingAdapter } from './PaperTradingAdapter';
export { PaperTradingDatabase, paperTradingDatabase } from './PaperTradingDatabase';
export { PaperTradingBalance } from './PaperTradingBalance';
export { OrderMatchingEngine } from './OrderMatchingEngine';

// Helper function to create paper trading adapter
import type { ExchangeConfig } from '../brokers/crypto/types';
import type { PaperTradingConfig } from './types';
import { PaperTradingAdapter } from './PaperTradingAdapter';
import type { IExchangeAdapter } from '../brokers/crypto/types';

/**
 * Create a universal paper trading adapter for ANY broker
 *
 * This function creates a paper trading adapter that wraps a real broker adapter,
 * allowing you to simulate trading with virtual money while using real market data.
 *
 * @param config - Paper trading configuration
 * @param realAdapter - The actual broker adapter to use for market data
 *
 * @example Crypto (Kraken)
 * ```typescript
 * import { createPaperTradingAdapter } from '@/paper-trading';
 * import { createExchangeAdapter } from '@/brokers/crypto';
 *
 * const krakenAdapter = createExchangeAdapter('kraken');
 * const paperAdapter = createPaperTradingAdapter({
 *   portfolioId: 'my-crypto-portfolio',
 *   portfolioName: 'BTC Strategy',
 *   provider: 'kraken',
 *   assetClass: 'crypto',
 *   initialBalance: 100000,
 *   fees: { maker: 0.0002, taker: 0.0005 },
 *   slippage: { market: 0.001, limit: 0 },
 * }, krakenAdapter);
 *
 * await paperAdapter.connect();
 * const order = await paperAdapter.createOrder('BTC/USD', 'market', 'buy', 0.1);
 * ```
 *
 * @example Stocks (Zerodha)
 * ```typescript
 * import { createPaperTradingAdapter } from '@/paper-trading';
 * import { createKiteAdapter } from '@/brokers/stocks';
 *
 * const kiteAdapter = createKiteAdapter({ apiKey: 'xxx', apiSecret: 'yyy' });
 * const paperAdapter = createPaperTradingAdapter({
 *   portfolioId: 'my-stock-portfolio',
 *   portfolioName: 'NIFTY Strategy',
 *   provider: 'zerodha',
 *   assetClass: 'stocks',
 *   initialBalance: 1000000, // ₹10L
 *   currency: 'INR',
 *   fees: { maker: 0.0003, taker: 0.0003 },
 *   slippage: { market: 0.0005, limit: 0 },
 * }, kiteAdapter);
 *
 * await paperAdapter.connect();
 * const order = await paperAdapter.createOrder('RELIANCE', 'limit', 'buy', 10, 2500);
 * ```
 */
export function createPaperTradingAdapter(
  config: PaperTradingConfig,
  realAdapter: IExchangeAdapter
): PaperTradingAdapter {
  // Create paper trading adapter with real broker adapter
  const exchangeConfig: ExchangeConfig & {
    paperTradingConfig: PaperTradingConfig;
    realAdapter: IExchangeAdapter;
  } = {
    exchange: `${config.provider}_paper`,
    enableRateLimit: true,
    paperTradingConfig: config,
    realAdapter,
  };

  return new PaperTradingAdapter(exchangeConfig);
}
