/**
 * Stock Brokers Module
 *
 * Unified exports for all stock broker functionality
 */

// Types
export * from './types';

// Constants
export * from './constants';

// Base Adapter
export { BaseStockBrokerAdapter } from './BaseStockBrokerAdapter';

// Registry data
export { STOCK_BROKER_REGISTRY } from './registry';

// Registry helpers & adapter registry
export {
  registerBrokerAdapter,
  createStockBrokerAdapter,
  getAllStockBrokers,
  getStockBrokersByRegion,
  getStockBrokerMetadata,
  isStockBrokerRegistered,
  getStockBrokerDisplayName,
  getStockBrokerDefaultSymbols,
  stockBrokerSupportsFeature,
  stockBrokerSupportsTradingFeature,
  getAvailableRegions,
  getStockBrokersWithFeature,
} from './registryHelpers';

// Initialization
export { initializeStockBrokers } from './init';

// Indian Brokers
export * from './india';

// YFinance Paper Trading
export * from './yfinance';
