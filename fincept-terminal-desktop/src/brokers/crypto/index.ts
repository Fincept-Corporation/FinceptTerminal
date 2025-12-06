/**
 * Exchange Adapters
 *
 * Unified CCXT-based exchange integrations for Fincept Terminal
 * Professional-grade architecture for trading, paper trading, and agentic systems
 */

// Core exports
export * from './types';
export { BaseExchangeAdapter } from './BaseExchangeAdapter';
export { KrakenAdapter } from './kraken/KrakenAdapter';
export { HyperLiquidAdapter } from './hyperliquid/HyperLiquidAdapter';

// Note: Paper Trading now in standalone src/paper-trading/ module

// Exchange registry
import { KrakenAdapter } from './kraken/KrakenAdapter';
import { HyperLiquidAdapter } from './hyperliquid/HyperLiquidAdapter';
import type { ExchangeConfig } from './types';

export const SUPPORTED_EXCHANGES = {
  kraken: {
    id: 'kraken',
    name: 'Kraken',
    adapter: KrakenAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      staking: true,
      earn: true,
    },
  },
  hyperliquid: {
    id: 'hyperliquid',
    name: 'HyperLiquid',
    adapter: HyperLiquidAdapter,
    type: 'decentralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: false,
      futures: true, // Perpetuals
      swap: true,
      vault: true,
      subaccounts: true,
    },
  },
  // Future exchanges will be added here
  // binance: { ... },
  // coinbase: { ... },
  // etc.
} as const;

export type SupportedExchangeId = keyof typeof SUPPORTED_EXCHANGES;

/**
 * Factory function to create exchange adapter
 */
export function createExchangeAdapter(
  exchangeId: SupportedExchangeId,
  config?: Partial<ExchangeConfig>
) {
  const exchange = SUPPORTED_EXCHANGES[exchangeId];
  if (!exchange) {
    throw new Error(`Exchange "${exchangeId}" is not supported`);
  }

  return new exchange.adapter({
    exchange: exchangeId,
    ...config,
  });
}

/**
 * Get list of all supported exchanges
 */
export function getupportedExchanges(): Array<{
  id: string;
  name: string;
  type: string;
  assetClass: string;
  region: string;
}> {
  return Object.values(SUPPORTED_EXCHANGES).map(exchange => ({
    id: exchange.id,
    name: exchange.name,
    type: exchange.type,
    assetClass: exchange.assetClass,
    region: exchange.region,
  }));
}

/**
 * Check if an exchange is supported
 */
export function isExchangeSupported(exchangeId: string): exchangeId is SupportedExchangeId {
  return exchangeId in SUPPORTED_EXCHANGES;
}
