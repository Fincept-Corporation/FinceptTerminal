/**
 * Stock Broker Registry - Helper Functions and Adapter Registry
 */

import type { StockBrokerMetadata, Region, IStockBrokerAdapter } from './types';
import { STOCK_BROKER_REGISTRY } from './registry';

// ============================================================================
// ADAPTER REGISTRY (Runtime instances)
// ============================================================================

// Map of broker ID to adapter class constructor
const adapterClasses: Record<string, new () => IStockBrokerAdapter> = {};

/**
 * Register a broker adapter class
 */
export function registerBrokerAdapter(
  brokerId: string,
  adapterClass: new () => IStockBrokerAdapter
): void {
  adapterClasses[brokerId] = adapterClass;
}

/**
 * Create a broker adapter instance
 */
export function createStockBrokerAdapter(brokerId: string): IStockBrokerAdapter {
  const AdapterClass = adapterClasses[brokerId];

  if (!AdapterClass) {
    throw new Error(`Broker adapter not registered: ${brokerId}`);
  }

  return new AdapterClass();
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Get all registered stock brokers
 */
export function getAllStockBrokers(): StockBrokerMetadata[] {
  return Object.values(STOCK_BROKER_REGISTRY);
}

/**
 * Get brokers by region
 */
export function getStockBrokersByRegion(region: Region): StockBrokerMetadata[] {
  return getAllStockBrokers().filter(broker => broker.region === region);
}

/**
 * Get broker metadata by ID
 */
export function getStockBrokerMetadata(brokerId: string): StockBrokerMetadata | undefined {
  return STOCK_BROKER_REGISTRY[brokerId];
}

/**
 * Check if broker is registered
 */
export function isStockBrokerRegistered(brokerId: string): boolean {
  return brokerId in STOCK_BROKER_REGISTRY;
}

/**
 * Get broker display name
 */
export function getStockBrokerDisplayName(brokerId: string): string {
  return STOCK_BROKER_REGISTRY[brokerId]?.displayName || brokerId.toUpperCase();
}

/**
 * Get default symbols for broker
 */
export function getStockBrokerDefaultSymbols(brokerId: string): string[] {
  return STOCK_BROKER_REGISTRY[brokerId]?.defaultSymbols || [];
}

/**
 * Check if broker supports feature
 */
export function stockBrokerSupportsFeature(
  brokerId: string,
  feature: keyof StockBrokerMetadata['features']
): boolean {
  const metadata = STOCK_BROKER_REGISTRY[brokerId];
  return metadata?.features[feature] === true;
}

/**
 * Check if broker supports trading feature
 */
export function stockBrokerSupportsTradingFeature(
  brokerId: string,
  feature: keyof StockBrokerMetadata['tradingFeatures']
): boolean {
  const metadata = STOCK_BROKER_REGISTRY[brokerId];
  return metadata?.tradingFeatures[feature] === true;
}

/**
 * Get available regions
 */
export function getAvailableRegions(): Region[] {
  const regions = new Set<Region>();
  for (const broker of getAllStockBrokers()) {
    regions.add(broker.region);
  }
  return Array.from(regions);
}

/**
 * Get brokers with specific feature
 */
export function getStockBrokersWithFeature(
  feature: keyof StockBrokerMetadata['features']
): StockBrokerMetadata[] {
  return getAllStockBrokers().filter(broker => broker.features[feature] === true);
}
