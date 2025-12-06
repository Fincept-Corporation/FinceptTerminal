/**
 * Zerodha Kite Connect Integration
 *
 * Indian stock broker integration using Kite Connect API v3
 * Supports equity, F&O, commodity, currency, and mutual funds
 */

export { KiteConnectAdapter, createKiteAdapter } from './kiteAdapter';
export type {
  KiteConfig,
  SessionData,
  OrderParams as KiteOrderParams,
  OrderResponse,
  Order,
  Trade,
  Quote,
  Holding,
  Position,
  HistoricalParams,
  CandleData,
  GTTParams,
  GTTResponse,
  GTT,
  AlertParams,
  Alert,
  MarginOrder,
  MarginResponse,
  MFOrder,
  MFHolding,
  MFSip,
} from './kiteAdapter';

// Re-export utilities
export {
  RateLimiter,
  WebhookHandler,
  BatchOperations,
  SessionManager,
  FileSessionStorage,
  PortfolioTracker,
  KiteError,
  KiteAuthError,
  KiteRateLimitError,
} from './kiteAdapter';

// Broker metadata
export const ZERODHA_BROKER = {
  id: 'zerodha',
  name: 'Zerodha Kite',
  version: '3.0.0',
  region: 'india',
  supported: {
    equity: true,
    fno: true,
    commodity: true,
    currency: true,
    mutualFunds: true,
    websocket: true,
  },
  features: {
    restApi: true,
    websocket: true,
    streaming: true,
    historicalData: true,
    realTimeData: true,
    advancedOrders: true,
    gtt: true,
    alerts: true,
    marginCalculator: true,
  },
  apis: {
    rest: 'https://api.kite.trade',
    websocket: 'wss://ws.kite.trade',
    login: 'https://kite.zerodha.com/connect/login',
  },
  rateLimits: {
    rest: {
      public: 1000, // 1 request per second
      private: 3000, // 1 request per 3 seconds
    },
  },
};

export default ZERODHA_BROKER;
