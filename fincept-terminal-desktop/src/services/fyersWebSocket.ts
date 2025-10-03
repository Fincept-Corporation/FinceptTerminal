// Fyers WebSocket Service
// Handles real-time streaming for orders, trades, positions

import { fyersOrderSocket } from 'fyers-web-sdk-v3';

export interface FyersOrderUpdate {
  clientId: string;
  id: string;
  exchOrdId: string;
  qty: number;
  filledQty: number;
  limitPrice: number;
  type: number;
  fyToken: string;
  exchange: number;
  segment: number;
  symbol: string;
  orderDateTime: string;
  orderValidity: string;
  productType: string;
  side: number;
  status: number;
  ex_sym: string;
  description: string;
}

export interface FyersTradeUpdate {
  id: string;
  orderId: string;
  symbol: string;
  side: number;
  qty: number;
  price: number;
  tradeDateTime: string;
}

export interface FyersPositionUpdate {
  symbol: string;
  netQty: number;
  avgPrice: number;
  ltp: number;
  pl: number;
  side: number;
}

type OrderCallback = (order: FyersOrderUpdate) => void;
type TradeCallback = (trade: FyersTradeUpdate) => void;
type PositionCallback = (position: FyersPositionUpdate) => void;
type GeneralCallback = (data: any) => void;
type ErrorCallback = (error: any) => void;

class FyersWebSocketService {
  private socket: any = null;
  private isConnected = false;
  private connectionString = '';

  private orderCallbacks: OrderCallback[] = [];
  private tradeCallbacks: TradeCallback[] = [];
  private positionCallbacks: PositionCallback[] = [];
  private generalCallbacks: GeneralCallback[] = [];
  private errorCallbacks: ErrorCallback[] = [];

  /**
   * Initialize WebSocket connection
   * @param appId - Fyers App ID
   * @param accessToken - Access token
   */
  connect(appId: string, accessToken: string): void {
    if (this.isConnected) {
      console.log('[FyersWS] Already connected');
      return;
    }

    this.connectionString = `${appId}:${accessToken}`;
    this.socket = new fyersOrderSocket(this.connectionString);

    // Error handler
    this.socket.on('error', (error: any) => {
      console.error('[FyersWS] Error:', error);
      this.errorCallbacks.forEach(cb => cb(error));
    });

    // Connection established
    this.socket.on('connect', () => {
      console.log('[FyersWS] Connected successfully');
      this.isConnected = true;

      // Subscribe to all channels
      this.socket.subscribe([
        this.socket.orderUpdates,
        this.socket.tradeUpdates,
        this.socket.positionUpdates,
        this.socket.edis,
        this.socket.pricealerts
      ]);
    });

    // Connection closed
    this.socket.on('close', () => {
      console.log('[FyersWS] Connection closed');
      this.isConnected = false;
    });

    // Order updates
    this.socket.on('orders', (data: any) => {
      console.log('[FyersWS] Order update:', data);
      if (data.orders) {
        this.orderCallbacks.forEach(cb => cb(data.orders));
      }
    });

    // Trade updates
    this.socket.on('trades', (data: any) => {
      console.log('[FyersWS] Trade update:', data);
      if (data.trades) {
        this.tradeCallbacks.forEach(cb => cb(data.trades));
      }
    });

    // Position updates
    this.socket.on('positions', (data: any) => {
      console.log('[FyersWS] Position update:', data);
      if (data.positions) {
        this.positionCallbacks.forEach(cb => cb(data.positions));
      }
    });

    // General updates (EDIS, price alerts)
    this.socket.on('general', (data: any) => {
      console.log('[FyersWS] General update:', data);
      this.generalCallbacks.forEach(cb => cb(data));
    });

    // Enable auto-reconnect
    this.socket.autoreconnect();

    // Connect
    this.socket.connect();
  }

  /**
   * Disconnect WebSocket
   */
  disconnect(): void {
    if (this.socket && this.isConnected) {
      this.socket.close();
      this.socket = null;
      this.isConnected = false;
      console.log('[FyersWS] Disconnected');
    }
  }

  /**
   * Check connection status
   */
  getConnectionStatus(): boolean {
    return this.isConnected;
  }

  // ==================== EVENT LISTENERS ====================

  /**
   * Subscribe to order updates
   */
  onOrderUpdate(callback: OrderCallback): void {
    this.orderCallbacks.push(callback);
  }

  /**
   * Subscribe to trade updates
   */
  onTradeUpdate(callback: TradeCallback): void {
    this.tradeCallbacks.push(callback);
  }

  /**
   * Subscribe to position updates
   */
  onPositionUpdate(callback: PositionCallback): void {
    this.positionCallbacks.push(callback);
  }

  /**
   * Subscribe to general updates
   */
  onGeneralUpdate(callback: GeneralCallback): void {
    this.generalCallbacks.push(callback);
  }

  /**
   * Subscribe to errors
   */
  onError(callback: ErrorCallback): void {
    this.errorCallbacks.push(callback);
  }

  /**
   * Remove all callbacks
   */
  clearCallbacks(): void {
    this.orderCallbacks = [];
    this.tradeCallbacks = [];
    this.positionCallbacks = [];
    this.generalCallbacks = [];
    this.errorCallbacks = [];
  }
}

// Singleton instance
export const fyersWebSocket = new FyersWebSocketService();
