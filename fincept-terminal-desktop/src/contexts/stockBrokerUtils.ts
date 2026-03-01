/**
 * Stock Broker Context - Utilities
 *
 * Constants and helper functions for StockBrokerContext.
 * Extracted to keep the main context file focused on state management.
 */

import { saveSetting, getSetting } from '@/services/core/sqliteService';

// ============================================================================
// CONSTANTS
// ============================================================================

export const CONNECTION_TIMEOUT = 15000; // 15 seconds for stock brokers (may need OAuth)
export const CONNECTION_RETRY_ATTEMPTS = 2;
export const CONNECTION_RETRY_DELAY = 3000;

// Storage keys
export const STORAGE_KEYS = {
  ACTIVE_STOCK_BROKER: 'active_stock_broker',
  STOCK_TRADING_MODE: 'stock_trading_mode',
  LAST_CONNECTED_BROKERS: 'last_connected_stock_brokers',
} as const;

// ============================================================================
// HELPERS
// ============================================================================

export function validateTradingMode(value: string | null): 'live' | 'paper' | null {
  if (value === 'live' || value === 'paper') return value;
  return null;
}

export async function safeStorageGet(key: string): Promise<string | null> {
  try {
    return await getSetting(key);
  } catch (error) {
    console.error(`[StockBrokerContext] Failed to get storage key "${key}":`, error);
    return null;
  }
}

export async function safeStorageSet(key: string, value: string): Promise<void> {
  try {
    await saveSetting(key, value, 'stock_broker');
  } catch (error) {
    console.error(`[StockBrokerContext] Failed to set storage key "${key}":`, error);
  }
}

export function createTimeout(ms: number): Promise<never> {
  return new Promise((_, reject) =>
    setTimeout(() => reject(new Error(`Connection timeout after ${ms}ms`)), ms)
  );
}

export async function retryConnect(
  connectFn: () => Promise<void>,
  attempts: number = CONNECTION_RETRY_ATTEMPTS,
  delay: number = CONNECTION_RETRY_DELAY
): Promise<void> {
  for (let i = 0; i < attempts; i++) {
    try {
      await Promise.race([connectFn(), createTimeout(CONNECTION_TIMEOUT)]);
      return;
    } catch (error) {
      if (i === attempts - 1) throw error;
      await new Promise((resolve) => setTimeout(resolve, delay * (i + 1)));
    }
  }
}
