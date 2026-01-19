/**
 * Unified Broker Registry
 *
 * Central registry for all broker adapters with metadata and capabilities
 * Enables seamless broker switching and future extensibility
 */

import { KrakenAdapter } from './crypto/kraken/KrakenAdapter';
import { HyperLiquidAdapter } from './crypto/hyperliquid/HyperLiquidAdapter';
import { BinanceAdapter } from './crypto/binance/BinanceAdapter';
import { OKXAdapter } from './crypto/okx/OKXAdapter';
import { CoinbaseAdapter } from './crypto/coinbase/CoinbaseAdapter';
import { ZerodhaAdapter } from './india/zerodha/ZerodhaAdapter';
import { FyersAdapter } from './stocks/india/fyers/FyersAdapter';
import type { IExchangeAdapter, ExchangeConfig } from './crypto/types';
import type { IIndianBrokerAdapter } from './india/types';

// ============================================================================
// BROKER METADATA
// ============================================================================

export interface BrokerMetadata {
  id: string;
  name: string;
  displayName: string;
  type: 'crypto' | 'stocks' | 'forex' | 'commodities';
  category: 'centralized' | 'decentralized';
  region: 'global' | 'us' | 'india' | 'asia' | 'europe';

  // Adapter class
  adapterClass: any; // Using any to avoid complex type incompatibility with async generators

  // Feature flags
  features: {
    spot?: boolean;
    margin?: boolean;
    futures?: boolean;
    perpetuals?: boolean;
    options?: boolean;
    staking?: boolean;
    vaults?: boolean;
    subaccounts?: boolean;
  };

  // Trading features
  tradingFeatures: {
    marketOrders: boolean;
    limitOrders: boolean;
    stopOrders: boolean;
    stopLimitOrders: boolean;
    trailingStopOrders: boolean;
    icebergOrders: boolean;
    batchOrders: boolean;
    editOrders: boolean;
  };

  // Advanced features
  advancedFeatures: {
    leverage: boolean;
    maxLeverage?: number;
    marginMode: boolean;
    transfers: boolean;
    withdrawals: boolean;
    deposits: boolean;
  };

  // Default symbols (popular pairs)
  defaultSymbols: string[];

  // WebSocket support
  websocket: {
    enabled: boolean;
    endpoint?: string;
  };

  // Trading fees
  fees?: {
    maker: number;
    taker: number;
  };
}

// ============================================================================
// BROKER REGISTRY
// ============================================================================

export const BROKER_REGISTRY: Record<string, BrokerMetadata> = {
  kraken: {
    id: 'kraken',
    name: 'kraken',
    displayName: 'Kraken',
    type: 'crypto',
    category: 'centralized',
    region: 'global',
    adapterClass: KrakenAdapter,

    features: {
      spot: true,
      margin: true,
      futures: true,
      perpetuals: false,
      options: false,
      staking: true,
      vaults: false,
      subaccounts: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true,
      icebergOrders: true,
      batchOrders: true,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 5,
      marginMode: true,
      transfers: true,
      withdrawals: true,
      deposits: true,
    },

    defaultSymbols: [
      'BTC/USD',
      'ETH/USD',
      'SOL/USD',
      'XRP/USD',
      'ADA/USD',
      'DOGE/USD',
      'MATIC/USD',
      'DOT/USD',
      'AVAX/USD',
      'LINK/USD',
      'UNI/USD',
      'ATOM/USD',
      'LTC/USD',
      'BCH/USD',
      'ALGO/USD',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://ws.kraken.com/v2',
    },

    fees: {
      maker: 0.0016,
      taker: 0.0026,
    },
  },

  hyperliquid: {
    id: 'hyperliquid',
    name: 'hyperliquid',
    displayName: 'HyperLiquid',
    type: 'crypto',
    category: 'decentralized',
    region: 'global',
    adapterClass: HyperLiquidAdapter,

    features: {
      spot: true,
      margin: false,
      futures: true,
      perpetuals: true,
      options: false,
      staking: false,
      vaults: true,
      subaccounts: true,
    },

    tradingFeatures: {
      marketOrders: true, // Simulated with limit orders
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
      icebergOrders: false,
      batchOrders: true,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 50,
      marginMode: true,
      transfers: true,
      withdrawals: true,
      deposits: false,
    },

    defaultSymbols: ['BTC/USD', 'ETH/USD', 'SOL/USD', 'AVAX/USD', 'MATIC/USD', 'ARB/USD', 'OP/USD', 'SUI/USD'],

    websocket: {
      enabled: true, // HyperLiquidAdapter now available
      endpoint: 'wss://api.hyperliquid.xyz/ws',
    },

    fees: {
      maker: 0.00015,
      taker: 0.00035,
    },
  },

  binance: {
    id: 'binance',
    name: 'binance',
    displayName: 'Binance',
    type: 'crypto',
    category: 'centralized',
    region: 'global',
    adapterClass: BinanceAdapter,

    features: {
      spot: true,
      margin: true,
      futures: true,
      perpetuals: true,
      options: true,
      staking: true,
      vaults: false,
      subaccounts: true,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true,
      icebergOrders: false,
      batchOrders: true,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 125,
      marginMode: true,
      transfers: true,
      withdrawals: true,
      deposits: true,
    },

    defaultSymbols: [
      'BTC/USDT',
      'ETH/USDT',
      'BNB/USDT',
      'SOL/USDT',
      'XRP/USDT',
      'ADA/USDT',
      'DOGE/USDT',
      'MATIC/USDT',
      'DOT/USDT',
      'AVAX/USDT',
      'LINK/USDT',
      'UNI/USDT',
      'ATOM/USDT',
      'LTC/USDT',
      'ETC/USDT',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://stream.binance.com:9443/ws',
    },

    fees: {
      maker: 0.001,
      taker: 0.001,
    },
  },

  okx: {
    id: 'okx',
    name: 'okx',
    displayName: 'OKX',
    type: 'crypto',
    category: 'centralized',
    region: 'global',
    adapterClass: OKXAdapter,

    features: {
      spot: true,
      margin: true,
      futures: true,
      perpetuals: true,
      options: true,
      staking: true,
      vaults: false,
      subaccounts: true,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: true,
      icebergOrders: true,
      batchOrders: true,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 125,
      marginMode: true,
      transfers: true,
      withdrawals: true,
      deposits: true,
    },

    defaultSymbols: [
      'BTC/USDT',
      'ETH/USDT',
      'SOL/USDT',
      'XRP/USDT',
      'DOGE/USDT',
      'ADA/USDT',
      'AVAX/USDT',
      'DOT/USDT',
      'MATIC/USDT',
      'LINK/USDT',
      'UNI/USDT',
      'ATOM/USDT',
      'LTC/USDT',
      'APT/USDT',
      'ARB/USDT',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://ws.okx.com:8443/ws/v5/public',
    },

    fees: {
      maker: 0.0008,
      taker: 0.001,
    },
  },

  coinbase: {
    id: 'coinbase',
    name: 'coinbase',
    displayName: 'Coinbase',
    type: 'crypto',
    category: 'centralized',
    region: 'us',
    adapterClass: CoinbaseAdapter,

    features: {
      spot: true,
      margin: false,
      futures: false,
      perpetuals: false,
      options: false,
      staking: true,
      vaults: false,
      subaccounts: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
      icebergOrders: false,
      batchOrders: false,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: false,
      maxLeverage: 1,
      marginMode: false,
      transfers: true,
      withdrawals: true,
      deposits: true,
    },

    defaultSymbols: [
      'BTC/USD',
      'ETH/USD',
      'SOL/USD',
      'XRP/USD',
      'DOGE/USD',
      'ADA/USD',
      'AVAX/USD',
      'DOT/USD',
      'MATIC/USD',
      'LINK/USD',
      'UNI/USD',
      'ATOM/USD',
      'LTC/USD',
      'BCH/USD',
      'SHIB/USD',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://ws-feed.exchange.coinbase.com',
    },

    fees: {
      maker: 0.004,
      taker: 0.006,
    },
  },

  // ============================================================================
  // INDIAN STOCK BROKERS
  // ============================================================================

  zerodha: {
    id: 'zerodha',
    name: 'zerodha',
    displayName: 'Zerodha (Kite)',
    type: 'stocks',
    category: 'centralized',
    region: 'india',
    adapterClass: ZerodhaAdapter,

    features: {
      spot: true,       // Equity delivery
      margin: true,     // MIS (Intraday)
      futures: true,    // F&O
      perpetuals: false,
      options: true,    // Options
      staking: false,
      vaults: false,
      subaccounts: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,      // SL
      stopLimitOrders: true, // SL-M
      trailingStopOrders: false,
      icebergOrders: false,
      batchOrders: false,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 5,  // MIS leverage up to 5x
      marginMode: true,
      transfers: false,
      withdrawals: false,
      deposits: false,
    },

    defaultSymbols: [
      'RELIANCE',
      'TCS',
      'HDFCBANK',
      'INFY',
      'ICICIBANK',
      'SBIN',
      'BHARTIARTL',
      'HINDUNILVR',
      'ITC',
      'KOTAKBANK',
      'LT',
      'AXISBANK',
      'MARUTI',
      'TATAMOTORS',
      'WIPRO',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://ws.kite.trade',
    },

    fees: {
      maker: 0,       // Zerodha has zero brokerage for equity delivery
      taker: 0.0003,  // 0.03% or Rs 20 whichever is lower for intraday
    },
  },

  fyers: {
    id: 'fyers',
    name: 'fyers',
    displayName: 'Fyers',
    type: 'stocks',
    category: 'centralized',
    region: 'india',
    adapterClass: FyersAdapter,

    features: {
      spot: true,       // Equity delivery
      margin: true,     // INTRADAY
      futures: true,    // F&O
      perpetuals: false,
      options: true,    // Options
      staking: false,
      vaults: false,
      subaccounts: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,      // Stop Loss
      stopLimitOrders: true, // Stop Loss Limit
      trailingStopOrders: false,
      icebergOrders: false,
      batchOrders: false,
      editOrders: true,
    },

    advancedFeatures: {
      leverage: true,
      maxLeverage: 5,  // Intraday leverage
      marginMode: true,
      transfers: false,
      withdrawals: false,
      deposits: false,
    },

    defaultSymbols: [
      'RELIANCE',
      'TCS',
      'HDFCBANK',
      'INFY',
      'ICICIBANK',
      'SBIN',
      'BHARTIARTL',
      'HINDUNILVR',
      'ITC',
      'KOTAKBANK',
      'LT',
      'AXISBANK',
      'MARUTI',
      'TATAMOTORS',
      'WIPRO',
    ],

    websocket: {
      enabled: true,
      endpoint: 'wss://socket.fyers.in/hsm/v1-5/prod',
    },

    fees: {
      maker: 0,       // Fyers has zero brokerage for equity delivery
      taker: 0.0003,  // 0.03% or Rs 20 whichever is lower for intraday
    },
  },
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Get all registered brokers
 */
export function getAllBrokers(): BrokerMetadata[] {
  return Object.values(BROKER_REGISTRY);
}

/**
 * Get brokers by type
 */
export function getBrokersByType(type: 'crypto' | 'stocks' | 'forex' | 'commodities'): BrokerMetadata[] {
  return getAllBrokers().filter(broker => broker.type === type);
}

/**
 * Get brokers by region
 */
export function getBrokersByRegion(region: 'global' | 'us' | 'india' | 'asia' | 'europe'): BrokerMetadata[] {
  return getAllBrokers().filter(broker => broker.region === region);
}

/**
 * Get Indian stock brokers
 */
export function getIndianBrokers(): BrokerMetadata[] {
  return getAllBrokers().filter(broker => broker.type === 'stocks' && broker.region === 'india');
}

/**
 * Get broker metadata by ID
 */
export function getBrokerMetadata(brokerId: string): BrokerMetadata | undefined {
  return BROKER_REGISTRY[brokerId];
}

/**
 * Check if broker is registered
 */
export function isBrokerRegistered(brokerId: string): boolean {
  return brokerId in BROKER_REGISTRY;
}

/**
 * Get broker display name
 */
export function getBrokerDisplayName(brokerId: string): string {
  return BROKER_REGISTRY[brokerId]?.displayName || brokerId.toUpperCase();
}

/**
 * Get default symbols for broker
 */
export function getDefaultSymbols(brokerId: string): string[] {
  return BROKER_REGISTRY[brokerId]?.defaultSymbols || [];
}

/**
 * Check if broker supports feature
 */
export function supportsBrokerFeature(
  brokerId: string,
  feature: keyof BrokerMetadata['features']
): boolean {
  const metadata = BROKER_REGISTRY[brokerId];
  return metadata?.features[feature] === true;
}

/**
 * Check if broker supports trading feature
 */
export function supportsTradingFeature(
  brokerId: string,
  feature: keyof BrokerMetadata['tradingFeatures']
): boolean {
  const metadata = BROKER_REGISTRY[brokerId];
  return metadata?.tradingFeatures[feature] === true;
}

/**
 * Check if broker supports advanced feature
 */
export function supportsAdvancedFeature(
  brokerId: string,
  feature: keyof BrokerMetadata['advancedFeatures']
): boolean {
  const metadata = BROKER_REGISTRY[brokerId];
  return metadata?.advancedFeatures[feature] === true;
}

/**
 * Get max leverage for broker
 */
export function getMaxLeverage(brokerId: string): number {
  return BROKER_REGISTRY[brokerId]?.advancedFeatures.maxLeverage || 1;
}

// ============================================================================
// ADAPTER FACTORY
// ============================================================================

/**
 * Create broker adapter instance
 */
export function createBrokerAdapter(
  brokerId: string,
  config?: Partial<ExchangeConfig>
): IExchangeAdapter {
  const metadata = BROKER_REGISTRY[brokerId];

  if (!metadata) {
    throw new Error(`Broker "${brokerId}" is not registered`);
  }

  return new metadata.adapterClass(config);
}

/**
 * Create multiple broker adapters
 */
export function createBrokerAdapters(
  brokerIds: string[],
  config?: Partial<ExchangeConfig>
): Map<string, IExchangeAdapter> {
  const adapters = new Map<string, IExchangeAdapter>();

  for (const brokerId of brokerIds) {
    adapters.set(brokerId, createBrokerAdapter(brokerId, config));
  }

  return adapters;
}
