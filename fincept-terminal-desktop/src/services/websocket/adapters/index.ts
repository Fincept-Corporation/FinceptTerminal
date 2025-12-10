// File: src/services/websocket/adapters/index.ts
// Export all adapters and adapter registry

import { AdapterRegistry, IWebSocketAdapter } from '../types';
import { BaseAdapter } from './BaseAdapter';
import { KrakenAdapter } from './KrakenAdapter';
import { HyperLiquidAdapter } from './HyperLiquidAdapter';

/**
 * Global registry of available WebSocket adapters
 * Add new adapters here as they are implemented
 */
export const adapters: AdapterRegistry = {
  kraken: KrakenAdapter,
  hyperliquid: HyperLiquidAdapter,
  // Add more adapters here:
  // fyers: FyersAdapter,
  // polygon: PolygonAdapter,
  // binance: BinanceAdapter,
  // etc.
};

/**
 * Get list of available provider names
 */
export function getAvailableProviders(): string[] {
  return Object.keys(adapters);
}

/**
 * Check if provider is available
 */
export function isProviderAvailable(provider: string): boolean {
  return provider in adapters;
}

/**
 * Create adapter instance for a provider
 */
export function createAdapter(provider: string): IWebSocketAdapter {
  const AdapterClass = adapters[provider];

  if (!AdapterClass) {
    throw new Error(`Adapter not found for provider: ${provider}`);
  }

  return new AdapterClass();
}

// Export adapter classes
export { BaseAdapter } from './BaseAdapter';
export { KrakenAdapter } from './KrakenAdapter';
export { HyperLiquidAdapter } from './HyperLiquidAdapter';
