/**
 * Unified Broker Registry
 *
 * Central registry for all broker adapters with metadata and capabilities
 * Enables seamless broker switching and future extensibility
 */

import { KrakenAdapter } from './crypto/kraken/KrakenAdapter';
import { HyperLiquidAdapter } from './crypto/hyperliquid/HyperLiquidAdapter';
import type { IExchangeAdapter, ExchangeConfig } from './crypto/types';

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

    defaultSymbols: ['BTC/USDC:USDC', 'ETH/USDC:USDC', 'SOL/USDC:USDC', 'AVAX/USDC:USDC', 'MATIC/USDC:USDC'],

    websocket: {
      enabled: true, // HyperLiquidAdapter now available
      endpoint: 'wss://api.hyperliquid.xyz/ws',
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
