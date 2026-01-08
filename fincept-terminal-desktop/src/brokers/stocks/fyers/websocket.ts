// Fyers WebSocket Module
// Handles real-time streaming for orders, trades, positions

import { fyersOrderSocket } from 'fyers-web-sdk-v3';
import {
  FyersOrderUpdate,
  FyersTradeUpdate,
  FyersPositionUpdate,
  OrderCallback,
  TradeCallback,
  PositionCallback,
  GeneralCallback,
  ErrorCallback
} from './types';

export class FyersWebSocket {
  private socket: any = null;
  private isConnected = false;
  private isConnecting = false;
  private connectionString = '';
  private logger: any;
  private appId = '';
  private accessToken = '';

  // Use Sets for O(1) add/remove operations and prevent duplicates
  private orderCallbacks: Set<OrderCallback> = new Set();
  private tradeCallbacks: Set<TradeCallback> = new Set();
  private positionCallbacks: Set<PositionCallback> = new Set();
  private generalCallbacks: Set<GeneralCallback> = new Set();
  private errorCallbacks: Set<ErrorCallback> = new Set();

  constructor(logger?: any) {
    this.logger = logger || {
      info: (...args: any[]) => console.log('[Fyers WebSocket]', ...args),
      debug: (...args: any[]) => console.log('[Fyers WebSocket]', ...args),
      error: (...args: any[]) => console.error('[Fyers WebSocket]', ...args),
    };
  }

  /**
   * Initialize WebSocket connection
   */
  connect(appId: string, accessToken: string): void {
    if (this.isConnected) {
      this.logger.debug('Already connected');
      return;
    }

    if (this.isConnecting) {
      this.logger.debug('Connection already in progress');
      return;
    }

    this.isConnecting = true;
    this.appId = appId;
    this.accessToken = accessToken;
    this.connectionString = `${appId}:${accessToken}`;
    this.socket = new fyersOrderSocket();

    this.socket.on('error', (error: any) => {
      this.logger.error('Error:', error);
      this.isConnecting = false;
      this.errorCallbacks.forEach(cb => {
        try {
          cb(error);
        } catch (err) {
          this.logger.error('Error callback threw:', err);
        }
      });
    });

    this.socket.on('connect', () => {
      this.logger.info('Connected successfully');
      this.isConnected = true;
      this.isConnecting = false;

      this.socket.subscribe([
        this.socket.orderUpdates,
        this.socket.tradeUpdates,
        this.socket.positionUpdates,
        this.socket.edis,
        this.socket.pricealerts
      ]);
    });

    this.socket.on('close', () => {
      this.logger.info('Connection closed');
      this.isConnected = false;
      this.isConnecting = false;
    });

    this.socket.on('orders', (data: any) => {
      this.logger.debug('Order update:', data);
      if (data.orders) {
        this.orderCallbacks.forEach(cb => {
          try {
            cb(data.orders);
          } catch (err) {
            this.logger.error('Order callback threw:', err);
          }
        });
      }
    });

    this.socket.on('trades', (data: any) => {
      this.logger.debug('Trade update:', data);
      if (data.trades) {
        this.tradeCallbacks.forEach(cb => {
          try {
            cb(data.trades);
          } catch (err) {
            this.logger.error('Trade callback threw:', err);
          }
        });
      }
    });

    this.socket.on('positions', (data: any) => {
      this.logger.debug('Position update:', data);
      if (data.positions) {
        this.positionCallbacks.forEach(cb => {
          try {
            cb(data.positions);
          } catch (err) {
            this.logger.error('Position callback threw:', err);
          }
        });
      }
    });

    this.socket.on('general', (data: any) => {
      this.logger.debug('General update:', data);
      this.generalCallbacks.forEach(cb => {
        try {
          cb(data);
        } catch (err) {
          this.logger.error('General callback threw:', err);
        }
      });
    });

    this.socket.autoreconnect();
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
      this.logger.info('Disconnected');
    }
  }

  /**
   * Check connection status
   */
  getConnectionStatus(): boolean {
    return this.isConnected;
  }

  /**
   * Subscribe to order updates
   * Returns unsubscribe function
   */
  onOrderUpdate(callback: OrderCallback): () => void {
    this.orderCallbacks.add(callback);
    return () => this.orderCallbacks.delete(callback);
  }

  /**
   * Subscribe to trade updates
   * Returns unsubscribe function
   */
  onTradeUpdate(callback: TradeCallback): () => void {
    this.tradeCallbacks.add(callback);
    return () => this.tradeCallbacks.delete(callback);
  }

  /**
   * Subscribe to position updates
   * Returns unsubscribe function
   */
  onPositionUpdate(callback: PositionCallback): () => void {
    this.positionCallbacks.add(callback);
    return () => this.positionCallbacks.delete(callback);
  }

  /**
   * Subscribe to general updates
   * Returns unsubscribe function
   */
  onGeneralUpdate(callback: GeneralCallback): () => void {
    this.generalCallbacks.add(callback);
    return () => this.generalCallbacks.delete(callback);
  }

  /**
   * Subscribe to errors
   * Returns unsubscribe function
   */
  onError(callback: ErrorCallback): () => void {
    this.errorCallbacks.add(callback);
    return () => this.errorCallbacks.delete(callback);
  }

  /**
   * Remove specific callbacks
   */
  removeOrderCallback(callback: OrderCallback): void {
    this.orderCallbacks.delete(callback);
  }

  removeTradeCallback(callback: TradeCallback): void {
    this.tradeCallbacks.delete(callback);
  }

  removePositionCallback(callback: PositionCallback): void {
    this.positionCallbacks.delete(callback);
  }

  removeGeneralCallback(callback: GeneralCallback): void {
    this.generalCallbacks.delete(callback);
  }

  removeErrorCallback(callback: ErrorCallback): void {
    this.errorCallbacks.delete(callback);
  }

  /**
   * Remove all callbacks
   */
  clearCallbacks(): void {
    this.orderCallbacks.clear();
    this.tradeCallbacks.clear();
    this.positionCallbacks.clear();
    this.generalCallbacks.clear();
    this.errorCallbacks.clear();
  }

  /**
   * Get callback counts (for debugging)
   */
  getCallbackCounts(): Record<string, number> {
    return {
      orders: this.orderCallbacks.size,
      trades: this.tradeCallbacks.size,
      positions: this.positionCallbacks.size,
      general: this.generalCallbacks.size,
      errors: this.errorCallbacks.size,
    };
  }
}
