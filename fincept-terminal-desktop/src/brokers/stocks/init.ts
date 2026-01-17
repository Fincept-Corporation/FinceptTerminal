/**
 * Stock Broker Initialization
 *
 * Registers all broker adapters at application startup
 */

import { registerBrokerAdapter } from './registry';
import { ZerodhaAdapter } from './india/zerodha/ZerodhaAdapter';
import { FyersAdapter } from './india/fyers/FyersAdapter';
import { AngelOneAdapter } from './india/angelone/AngelOneAdapter';

/**
 * Initialize all stock broker adapters
 * Must be called during application startup
 */
export function initializeStockBrokers(): void {
  // Register Indian brokers
  registerBrokerAdapter('zerodha', ZerodhaAdapter);
  registerBrokerAdapter('fyers', FyersAdapter);
  registerBrokerAdapter('angelone', AngelOneAdapter);

  // Future brokers
  // registerBrokerAdapter('dhan', DhanAdapter);
  // registerBrokerAdapter('upstox', UpstoxAdapter);
  // registerBrokerAdapter('groww', GrowwAdapter);
}
