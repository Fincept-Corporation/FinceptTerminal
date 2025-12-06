/**
 * Brokers Integration Module
 *
 * Professional-grade broker integrations for Fincept Terminal
 * Organized by asset class: Crypto exchanges vs Stock brokers
 */

// ============================================================================
// CRYPTO EXCHANGES (CCXT-based)
// ============================================================================
export * from './crypto';

// ============================================================================
// STOCK BROKERS
// ============================================================================
export * from './stocks';

// ============================================================================
// UNIFIED BROKER REGISTRY
// ============================================================================
import { SUPPORTED_EXCHANGES } from './crypto';

export const SUPPORTED_BROKERS = {
  // Crypto Exchanges
  ...Object.fromEntries(
    Object.entries(SUPPORTED_EXCHANGES).map(([id, exchange]) => [
      id,
      {
        id: exchange.id,
        name: exchange.name,
        type: exchange.assetClass,
        region: exchange.region,
        supported: true,
        category: 'crypto',
      },
    ])
  ),

  // Stock Brokers
  zerodha: {
    id: 'zerodha',
    name: 'Zerodha Kite',
    type: 'stocks',
    region: 'india',
    supported: true,
    category: 'stocks',
  },
} as const;

export type BrokerId = keyof typeof SUPPORTED_BROKERS;

/**
 * Get brokers by category
 */
export function getBrokersByCategory(category: 'crypto' | 'stocks') {
  return Object.values(SUPPORTED_BROKERS).filter(b => b.category === category);
}

/**
 * Get all crypto exchanges
 */
export function getCryptoExchanges() {
  return getBrokersByCategory('crypto');
}

/**
 * Get all stock brokers
 */
export function getStockBrokers() {
  return getBrokersByCategory('stocks');
}
