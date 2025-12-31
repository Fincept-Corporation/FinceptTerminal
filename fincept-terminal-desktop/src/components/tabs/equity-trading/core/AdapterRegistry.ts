/**
 * Adapter Registry
 * Central registration point for all broker adapters
 * Ensures adapters are properly initialized before use
 */

import { authManager } from '../services/AuthManager';
import { webSocketManager } from '../services/WebSocketManager';
import { FyersStandardAdapter } from '../adapters/FyersStandardAdapter';
import { KiteStandardAdapter } from '../adapters/KiteStandardAdapter';

/**
 * Register all broker adapters with AuthManager and WebSocketManager
 * This should be called once when the equity trading module initializes
 */
export function registerAllAdapters(): void {
  console.log('[AdapterRegistry] Registering all broker adapters...');

  try {
    // Register Fyers adapter
    const fyersAdapter = new FyersStandardAdapter();
    authManager.registerAdapter('fyers', fyersAdapter);
    webSocketManager.registerAdapter('fyers', fyersAdapter);
    console.log('[AdapterRegistry] ✓ Fyers adapter registered with AuthManager and WebSocketManager');

    // Register Kite adapter
    const kiteAdapter = new KiteStandardAdapter();
    authManager.registerAdapter('kite', kiteAdapter);
    webSocketManager.registerAdapter('kite', kiteAdapter);
    console.log('[AdapterRegistry] ✓ Kite adapter registered with AuthManager and WebSocketManager');

    console.log('[AdapterRegistry] All adapters registered successfully');
  } catch (error) {
    console.error('[AdapterRegistry] Failed to register adapters:', error);
    throw error;
  }
}

/**
 * Check if all required adapters are registered
 */
export function areAdaptersRegistered(): boolean {
  const fyersAdapter = authManager.getAdapter('fyers');
  const kiteAdapter = authManager.getAdapter('kite');

  return !!(fyersAdapter && kiteAdapter);
}
