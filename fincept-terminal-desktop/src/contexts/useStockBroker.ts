/**
 * Stock Broker Context - Consumer Hooks
 *
 * Convenience hooks for accessing specific slices of the StockBrokerContext.
 * Extracted from StockBrokerContext.tsx to keep the provider file focused.
 */

import { useStockBrokerContext } from './StockBrokerContext';

/**
 * Hook for broker selection
 */
export function useStockBrokerSelection() {
  const {
    activeBroker,
    activeBrokerMetadata,
    availableBrokers,
    setActiveBroker,
    getBrokersByRegion,
    isLoading,
  } = useStockBrokerContext();

  return {
    broker: activeBroker,
    metadata: activeBrokerMetadata,
    brokers: availableBrokers,
    setBroker: setActiveBroker,
    getBrokersByRegion,
    isLoading,
  };
}

/**
 * Hook for authentication
 */
export function useStockBrokerAuth() {
  const {
    isAuthenticated,
    authenticate,
    connect,
    logout,
    getAuthUrl,
    isConnecting,
    error,
    clearError,
    adapter,
  } = useStockBrokerContext();

  return {
    isAuthenticated,
    authenticate,
    connect, // Explicit connect using stored credentials
    logout,
    getAuthUrl,
    isConnecting,
    error,
    clearError,
    adapter,
  };
}

/**
 * Hook for trading data
 */
export function useStockTradingData() {
  const {
    adapter,
    positions,
    holdings,
    orders,
    trades,
    funds,
    refreshPositions,
    refreshHoldings,
    refreshOrders,
    refreshTrades,
    refreshFunds,
    refreshAll,
    isRefreshing,
    isAuthenticated,
  } = useStockBrokerContext();

  return {
    adapter,
    positions,
    holdings,
    orders,
    trades,
    funds,
    refreshPositions,
    refreshHoldings,
    refreshOrders,
    refreshTrades,
    refreshFunds,
    refreshAll,
    isRefreshing,
    isReady: isAuthenticated && adapter !== null,
  };
}

/**
 * Hook for broker capabilities
 */
export function useStockBrokerCapabilities() {
  const {
    activeBrokerMetadata,
    supportsFeature,
    supportsTradingFeature,
    defaultSymbols,
  } = useStockBrokerContext();

  return {
    metadata: activeBrokerMetadata,
    supportsFeature,
    supportsTradingFeature,
    defaultSymbols,
    // Quick access to common features
    hasWebSocket: supportsFeature('webSocket'),
    hasAMO: supportsFeature('amo'),
    hasGTT: supportsFeature('gtt'),
    hasBracketOrder: supportsFeature('bracketOrder'),
    hasCoverOrder: supportsFeature('coverOrder'),
    hasOptionsChain: supportsFeature('optionsChain'),
  };
}

/**
 * Hook for trading mode
 */
export function useStockTradingMode() {
  const { tradingMode, setTradingMode } = useStockBrokerContext();

  return {
    mode: tradingMode,
    setMode: setTradingMode,
    isLive: tradingMode === 'live',
    isPaper: tradingMode === 'paper',
  };
}
