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
export { BinanceAdapter } from './binance/BinanceAdapter';
export { OKXAdapter } from './okx/OKXAdapter';
export { CoinbaseAdapter } from './coinbase/CoinbaseAdapter';
export { BybitAdapter } from './bybit/BybitAdapter';
export { KucoinAdapter } from './kucoin/KucoinAdapter';
export { GateioAdapter } from './gateio/GateioAdapter';

// Note: Paper Trading now in standalone src/paper-trading/ module

// Exchange registry
import { KrakenAdapter } from './kraken/KrakenAdapter';
import { HyperLiquidAdapter } from './hyperliquid/HyperLiquidAdapter';
import { BinanceAdapter } from './binance/BinanceAdapter';
import { OKXAdapter } from './okx/OKXAdapter';
import { CoinbaseAdapter } from './coinbase/CoinbaseAdapter';
import { BybitAdapter } from './bybit/BybitAdapter';
import { KucoinAdapter } from './kucoin/KucoinAdapter';
import { GateioAdapter } from './gateio/GateioAdapter';
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
  binance: {
    id: 'binance',
    name: 'Binance',
    adapter: BinanceAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      options: true,
      staking: true,
      earn: true,
      lending: true,
    },
  },
  okx: {
    id: 'okx',
    name: 'OKX',
    adapter: OKXAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      options: true,
      perpetuals: true,
      staking: true,
      earn: true,
    },
  },
  coinbase: {
    id: 'coinbase',
    name: 'Coinbase',
    adapter: CoinbaseAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'us',
    features: {
      spot: true,
      margin: false,
      futures: false,
      options: false,
      perpetuals: false,
      staking: true,
      earn: true,
    },
  },
  bybit: {
    id: 'bybit',
    name: 'Bybit',
    adapter: BybitAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      options: true,
      perpetuals: true,
      copyTrading: true,
      earn: true,
      unifiedAccount: true,
    },
  },
  kucoin: {
    id: 'kucoin',
    name: 'KuCoin',
    adapter: KucoinAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      options: false,
      perpetuals: true,
      lending: true,
      staking: true,
      subaccounts: true,
    },
  },
  gateio: {
    id: 'gateio',
    name: 'Gate.io',
    adapter: GateioAdapter,
    type: 'centralized',
    assetClass: 'crypto',
    region: 'global',
    features: {
      spot: true,
      margin: true,
      futures: true,
      options: true,
      perpetuals: true,
      copyTrading: true,
      lending: true,
      earn: true,
      subaccounts: true,
    },
  },
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
export function getSupportedExchanges(): Array<{
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
