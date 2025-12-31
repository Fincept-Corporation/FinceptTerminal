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
  private connectionString = '';
  private logger: any;

  private orderCallbacks: OrderCallback[] = [];
  private tradeCallbacks: TradeCallback[] = [];
  private positionCallbacks: PositionCallback[] = [];
  private generalCallbacks: GeneralCallback[] = [];
  private errorCallbacks: ErrorCallback[] = [];

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

    this.connectionString = `${appId}:${accessToken}`;
    this.socket = new fyersOrderSocket();

    this.socket.on('error', (error: any) => {
      this.logger.error('Error:', error);
      this.errorCallbacks.forEach(cb => cb(error));
    });

    this.socket.on('connect', () => {
      this.logger.info('Connected successfully');
      this.isConnected = true;

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
    });

    this.socket.on('orders', (data: any) => {
      this.logger.debug('Order update:', data);
      if (data.orders) {
        this.orderCallbacks.forEach(cb => cb(data.orders));
      }
    });

    this.socket.on('trades', (data: any) => {
      this.logger.debug('Trade update:', data);
      if (data.trades) {
        this.tradeCallbacks.forEach(cb => cb(data.trades));
      }
    });

    this.socket.on('positions', (data: any) => {
      this.logger.debug('Position update:', data);
      if (data.positions) {
        this.positionCallbacks.forEach(cb => cb(data.positions));
      }
    });

    this.socket.on('general', (data: any) => {
      this.logger.debug('General update:', data);
      this.generalCallbacks.forEach(cb => cb(data));
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
