/**
 * Stock Broker Registry
 *
 * Central registry for all stock brokers globally
 * Enables broker discovery, switching, and capability detection
 */

import type { StockBrokerMetadata, Region, IStockBrokerAdapter } from './types';

// ============================================================================
// BROKER REGISTRY
// ============================================================================

export const STOCK_BROKER_REGISTRY: Record<string, StockBrokerMetadata> = {
  // ============================================================================
  // INDIAN BROKERS
  // ============================================================================

  zerodha: {
    id: 'zerodha',
    name: 'zerodha',
    displayName: 'Zerodha (Kite)',
    logo: '/brokers/zerodha.png',
    website: 'https://zerodha.com',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 0.0003 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: [
      'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
      'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
      'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
    ],
  },

  fyers: {
    id: 'fyers',
    name: 'fyers',
    displayName: 'Fyers',
    logo: '/brokers/fyers.png',
    website: 'https://fyers.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: false,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: false,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'oauth',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 5,
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 0.0003 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: [
      'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
      'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
      'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
    ],
  },

  angelone: {
    id: 'angelone',
    name: 'angelone',
    displayName: 'Angel One',
    logo: '/brokers/angelone.png',
    website: 'https://www.angelone.in',

    region: 'india',
    country: 'IN',
    currency: 'INR',
    exchanges: ['NSE', 'BSE', 'NFO', 'BFO', 'CDS', 'MCX'],
    marketHours: {
      open: '09:15',
      close: '15:30',
      timezone: 'Asia/Kolkata',
      preMarketOpen: '09:00',
      preMarketClose: '09:08',
    },

    features: {
      webSocket: true,
      amo: true,
      gtt: true,
      bracketOrder: true,
      coverOrder: true,
      marginCalculator: true,
      optionsChain: true,
      paperTrading: false,
    },

    tradingFeatures: {
      marketOrders: true,
      limitOrders: true,
      stopOrders: true,
      stopLimitOrders: true,
      trailingStopOrders: false,
    },

    productTypes: ['CASH', 'INTRADAY', 'MARGIN'],

    authType: 'totp',

    rateLimit: {
      ordersPerSecond: 10,
      quotesPerSecond: 1, // Angel One has 1 request per second for quotes
    },

    fees: {
      equity: { brokerage: 0, stt: 0.001 },
      intraday: { brokerage: 20 },
      fno: { brokerage: 20 },
    },

    defaultSymbols: [
      'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
      'SBIN', 'BHARTIARTL', 'HINDUNILVR', 'ITC', 'KOTAKBANK',
      'LT', 'AXISBANK', 'MARUTI', 'TATAMOTORS', 'WIPRO',
    ],
  },

  // Placeholder for future brokers
  // dhan: { ... },
  // upstox: { ... },
  // groww: { ... },

  // ============================================================================
  // US BROKERS (Placeholder)
  // ============================================================================

  // alpaca: { ... },
  // tradier: { ... },
  // ibkr: { ... },

  // ============================================================================
  // EUROPE BROKERS (Placeholder)
  // ============================================================================

  // degiro: { ... },
};

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
