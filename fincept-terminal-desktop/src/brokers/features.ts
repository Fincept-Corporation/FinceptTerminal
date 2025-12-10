/**
 * Broker Feature Detection System
 *
 * Auto-detects which methods and features each broker adapter supports
 * Enables dynamic UI rendering based on broker capabilities
 */

import type { IExchangeAdapter } from './crypto/types';
import type { BrokerMetadata } from './registry';

// ============================================================================
// FEATURE CATEGORIES
// ============================================================================

export interface BrokerFeatures {
  // Market Data
  marketData: {
    fetchMarkets: boolean;
    fetchTicker: boolean;
    fetchTickers: boolean;
    fetchOrderBook: boolean;
    fetchTrades: boolean;
    fetchOHLCV: boolean;
    fetchStatus: boolean;
    fetchTime: boolean;
    fetchCurrencies: boolean;
  };

  // Account & Balance
  account: {
    fetchBalance: boolean;
    fetchPositions: boolean;
    fetchLedger: boolean;
    fetchTradingFee: boolean;
    fetchDeposits: boolean;
    fetchWithdrawals: boolean;
  };

  // Trading - Basic
  trading: {
    createOrder: boolean;
    createOrders: boolean;
    editOrder: boolean;
    editOrders: boolean;
    cancelOrder: boolean;
    cancelOrders: boolean;
    cancelAllOrders: boolean;
    cancelAllOrdersAfter: boolean;
  };

  // Trading - Advanced
  tradingAdvanced: {
    fetchOrder: boolean;
    fetchOrders: boolean;
    fetchOpenOrders: boolean;
    fetchClosedOrders: boolean;
    fetchCanceledOrders: boolean;
    fetchMyTrades: boolean;
    fetchOrderTrades: boolean;
  };

  // Margin & Leverage
  margin: {
    setLeverage: boolean;
    setMarginMode: boolean;
    addMargin: boolean;
    reduceMargin: boolean;
  };

  // Transfers & Withdrawals
  transfers: {
    transfer: boolean;
    transferOut: boolean;
    withdraw: boolean;
    createDepositAddress: boolean;
    fetchDepositAddress: boolean;
    fetchDepositMethods: boolean;
  };

  // Futures & Derivatives
  futures: {
    fetchFundingRate: boolean;
    fetchFundingRates: boolean;
    fetchFundingRateHistory: boolean;
    fetchFundingHistory: boolean;
    fetchOpenInterest: boolean;
    fetchOpenInterests: boolean;
  };

  // HyperLiquid-specific
  hyperliquid: {
    createVault: boolean;
    setVaultAddress: boolean;
    setSubAccountAddress: boolean;
    fetchVaultBalance: boolean;
  };

  // WebSocket
  websocket: {
    watchTicker: boolean;
    watchTickers: boolean;
    watchOrderBook: boolean;
    watchTrades: boolean;
    watchOHLCV: boolean;
    watchOrders: boolean;
    watchMyTrades: boolean;
    watchBalance: boolean;
    watchPositions: boolean;
  };
}

// ============================================================================
// FEATURE DETECTION
// ============================================================================

/**
 * Detect all features supported by a broker adapter
 */
export function detectBrokerFeatures(adapter: IExchangeAdapter | any): BrokerFeatures {
  return {
    marketData: {
      fetchMarkets: hasMethod(adapter, 'fetchMarkets'),
      fetchTicker: hasMethod(adapter, 'fetchTicker'),
      fetchTickers: hasMethod(adapter, 'fetchTickers'),
      fetchOrderBook: hasMethod(adapter, 'fetchOrderBook'),
      fetchTrades: hasMethod(adapter, 'fetchTrades'),
      fetchOHLCV: hasMethod(adapter, 'fetchOHLCV'),
      fetchStatus: hasMethod(adapter, 'fetchStatus'),
      fetchTime: hasMethod(adapter, 'fetchTime'),
      fetchCurrencies: hasMethod(adapter, 'fetchCurrencies'),
    },

    account: {
      fetchBalance: hasMethod(adapter, 'fetchBalance'),
      fetchPositions: hasMethod(adapter, 'fetchPositions'),
      fetchLedger: hasMethod(adapter, 'fetchLedger'),
      fetchTradingFee: hasMethod(adapter, 'fetchTradingFee'),
      fetchDeposits: hasMethod(adapter, 'fetchDeposits'),
      fetchWithdrawals: hasMethod(adapter, 'fetchWithdrawals'),
    },

    trading: {
      createOrder: hasMethod(adapter, 'createOrder'),
      createOrders: hasMethod(adapter, 'createOrders'),
      editOrder: hasMethod(adapter, 'editOrder'),
      editOrders: hasMethod(adapter, 'editOrders'),
      cancelOrder: hasMethod(adapter, 'cancelOrder'),
      cancelOrders: hasMethod(adapter, 'cancelOrders'),
      cancelAllOrders: hasMethod(adapter, 'cancelAllOrders'),
      cancelAllOrdersAfter: hasMethod(adapter, 'cancelAllOrdersAfter'),
    },

    tradingAdvanced: {
      fetchOrder: hasMethod(adapter, 'fetchOrder'),
      fetchOrders: hasMethod(adapter, 'fetchOrders'),
      fetchOpenOrders: hasMethod(adapter, 'fetchOpenOrders'),
      fetchClosedOrders: hasMethod(adapter, 'fetchClosedOrders'),
      fetchCanceledOrders: hasMethod(adapter, 'fetchCanceledOrders'),
      fetchMyTrades: hasMethod(adapter, 'fetchMyTrades'),
      fetchOrderTrades: hasMethod(adapter, 'fetchOrderTrades'),
    },

    margin: {
      setLeverage: hasMethod(adapter, 'setLeverage'),
      setMarginMode: hasMethod(adapter, 'setMarginMode'),
      addMargin: hasMethod(adapter, 'addMargin'),
      reduceMargin: hasMethod(adapter, 'reduceMargin'),
    },

    transfers: {
      transfer: hasMethod(adapter, 'transfer'),
      transferOut: hasMethod(adapter, 'transferOut'),
      withdraw: hasMethod(adapter, 'withdraw'),
      createDepositAddress: hasMethod(adapter, 'createDepositAddress'),
      fetchDepositAddress: hasMethod(adapter, 'fetchDepositAddress'),
      fetchDepositMethods: hasMethod(adapter, 'fetchDepositMethods'),
    },

    futures: {
      fetchFundingRate: hasMethod(adapter, 'fetchFundingRate'),
      fetchFundingRates: hasMethod(adapter, 'fetchFundingRates'),
      fetchFundingRateHistory: hasMethod(adapter, 'fetchFundingRateHistory'),
      fetchFundingHistory: hasMethod(adapter, 'fetchFundingHistory'),
      fetchOpenInterest: hasMethod(adapter, 'fetchOpenInterest'),
      fetchOpenInterests: hasMethod(adapter, 'fetchOpenInterests'),
    },

    hyperliquid: {
      createVault: hasMethod(adapter, 'createVault'),
      setVaultAddress: hasMethod(adapter, 'setVaultAddress'),
      setSubAccountAddress: hasMethod(adapter, 'setSubAccountAddress'),
      fetchVaultBalance: hasMethod(adapter, 'fetchVaultBalance'),
    },

    websocket: {
      watchTicker: hasMethod(adapter, 'watchTicker'),
      watchTickers: hasMethod(adapter, 'watchTickers'),
      watchOrderBook: hasMethod(adapter, 'watchOrderBook'),
      watchTrades: hasMethod(adapter, 'watchTrades'),
      watchOHLCV: hasMethod(adapter, 'watchOHLCV'),
      watchOrders: hasMethod(adapter, 'watchOrders'),
      watchMyTrades: hasMethod(adapter, 'watchMyTrades'),
      watchBalance: hasMethod(adapter, 'watchBalance'),
      watchPositions: hasMethod(adapter, 'watchPositions'),
    },
  };
}

/**
 * Check if adapter has a specific method
 */
function hasMethod(adapter: any, methodName: string): boolean {
  return typeof adapter?.[methodName] === 'function';
}

/**
 * Check if broker supports a specific feature
 */
export function supportsFeature(
  features: BrokerFeatures,
  category: keyof BrokerFeatures,
  feature: string
): boolean {
  const categoryFeatures = features[category] as any;
  return categoryFeatures?.[feature] === true;
}

/**
 * Get all supported features in a category
 */
export function getSupportedFeatures(
  features: BrokerFeatures,
  category: keyof BrokerFeatures
): string[] {
  const categoryFeatures = features[category] as any;
  return Object.keys(categoryFeatures).filter(key => categoryFeatures[key] === true);
}

/**
 * Get feature count for a category
 */
export function getFeatureCount(
  features: BrokerFeatures,
  category: keyof BrokerFeatures
): number {
  return getSupportedFeatures(features, category).length;
}

/**
 * Check if broker supports category (has at least one feature)
 */
export function supportsCategory(
  features: BrokerFeatures,
  category: keyof BrokerFeatures
): boolean {
  return getFeatureCount(features, category) > 0;
}

/**
 * Combine metadata and detected features
 */
export interface CompleteBrokerInfo {
  metadata: BrokerMetadata;
  features: BrokerFeatures;
  capabilities: ReturnType<IExchangeAdapter['getCapabilities']>;
}

/**
 * Get complete broker information
 */
export function getCompleteBrokerInfo(
  metadata: BrokerMetadata,
  adapter: IExchangeAdapter
): CompleteBrokerInfo {
  return {
    metadata,
    features: detectBrokerFeatures(adapter),
    capabilities: adapter.getCapabilities(),
  };
}

/**
 * Create feature check helper
 */
export function createFeatureChecker(features: BrokerFeatures) {
  return {
    // Market data checks
    canFetchTicker: () => features.marketData.fetchTicker,
    canFetchOrderBook: () => features.marketData.fetchOrderBook,
    canFetchOHLCV: () => features.marketData.fetchOHLCV,

    // Trading checks
    canCreateOrder: () => features.trading.createOrder,
    canEditOrder: () => features.trading.editOrder,
    canBatchOrder: () => features.trading.createOrders,
    canCancelAll: () => features.trading.cancelAllOrders,

    // Advanced checks
    canSetLeverage: () => features.margin.setLeverage,
    canTransfer: () => features.transfers.transfer,
    canWithdraw: () => features.transfers.withdraw,

    // Futures checks
    hasFundingRates: () => features.futures.fetchFundingRates,
    hasOpenInterest: () => features.futures.fetchOpenInterest,

    // HyperLiquid checks
    hasVaults: () => features.hyperliquid.createVault,
    hasSubaccounts: () => features.hyperliquid.setSubAccountAddress,

    // WebSocket checks
    hasWebSocket: () => features.websocket.watchTicker,
    canStreamOrders: () => features.websocket.watchOrders,
  };
}
