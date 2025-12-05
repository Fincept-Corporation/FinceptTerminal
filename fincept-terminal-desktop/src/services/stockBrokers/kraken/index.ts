/**
 * Kraken Broker Integration
 *
 * Complete Kraken API integration for Fincept Terminal
 * Supports spot, margin, futures trading and all WebSocket channels
 */

export { default as KrakenService } from './krakenService';
export { default as KrakenWebSocket } from './krakenWebSocket';
export { default as KrakenAuth } from './krakenAuth';
export { default as KrakenConfig } from './krakenConfig';
export * from '../krakenTypes';

// Export factory function for easy initialization
export function createKrakenService(config?: any) {
  const KrakenServiceClass = require('./krakenService').default;
  return new KrakenServiceClass(config);
}

// Export WebSocket factory
export function createKrakenWebSocket(config?: any) {
  const KrakenWebSocketClass = require('./krakenWebSocket').default;
  return new KrakenWebSocketClass(config);
}

// Broker metadata for registry
export const KRAKEN_BROKER = {
  id: 'kraken',
  name: 'Kraken',
  version: '1.0.0',
  supported: {
    spot: true,
    margin: true,
    futures: true,
    options: false,
    staking: true,
    lending: true,
    websocket: true,
  },
  features: {
    restApi: true,
    websocket: true,
    streaming: true,
    historicalData: true,
    realTimeData: true,
    advancedOrders: true,
    batchOperations: true,
    multiAccount: true,
  },
  apis: {
    rest: {
      spot: 'https://api.kraken.com',
      futures: 'https://futures.kraken.com',
      auth: 'https://api.kraken.com',
    },
    websocket: {
      public: 'wss://ws.kraken.com/v2',
      private: 'wss://ws-auth.kraken.com/v2',
      futures: 'wss://futures.kraken.com/ws/v1',
      testnet: 'wss://demo-futures.kraken.com/ws/v1',
    },
  },
  rateLimits: {
    rest: {
      public: 1000, // 1 request per second
      private: 3000, // 1 request per 3 seconds
    },
    websocket: {
      connections: 10, // Max concurrent connections
      subscriptions: 100, // Max subscriptions per connection
    },
  },
};

export default KRAKEN_BROKER;