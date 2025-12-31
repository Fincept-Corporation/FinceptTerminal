// Fyers Data WebSocket Module
// Handles real-time market data streaming for quotes, depth, and ticks

import { fyersDataSocket } from 'fyers-web-sdk-v3';

/**
 * Fyers Data WebSocket Message Format
 * Based on official Fyers API documentation
 *
 * Sample Full Response (SymbolUpdate mode):
 * {
 *   "ltp": 606.4,
 *   "open_price": 609.85,
 *   "high_price": 610.5,
 *   "low_price": 605.85,
 *   "prev_close_price": 620.2,
 *   "ch": -13.8,
 *   "chp": -2.23,
 *   "vol_traded_today": 3045212,
 *   "last_traded_time": 1690953622,
 *   "exch_feed_time": 1690953622,
 *   "bid_size": 2081,
 *   "ask_size": 903,
 *   "bid_price": 606.4,
 *   "ask_price": 606.45,
 *   "last_traded_qty": 5,
 *   "tot_buy_qty": 749960,
 *   "tot_sell_qty": 1092063,
 *   "avg_trade_price": 608.2,
 *   "type": "sf",
 *   "symbol": "NSE:SBIN-EQ"
 * }
 */
export interface DataSocketMessage {
  // Symbol identification
  symbol: string;
  type?: string;  // "sf" for symbol feed

  // Price fields
  ltp?: number;              // Last traded price
  open_price?: number;       // Open price
  high_price?: number;       // High price
  low_price?: number;        // Low price
  prev_close_price?: number; // Previous close price

  // Change
  ch?: number;               // Change (points)
  chp?: number;              // Change percent

  // Volume
  vol_traded_today?: number; // Volume traded today
  last_traded_qty?: number;  // Last traded quantity

  // Bid/Ask
  bid_price?: number;        // Bid price
  ask_price?: number;        // Ask price
  bid_size?: number;         // Bid size
  ask_size?: number;         // Ask size

  // Additional
  avg_trade_price?: number;  // Average trade price
  tot_buy_qty?: number;      // Total buy quantity
  tot_sell_qty?: number;     // Total sell quantity
  last_traded_time?: number; // Last traded time (unix timestamp)
  exch_feed_time?: number;   // Exchange feed time (unix timestamp)

  // Market depth (for depth subscriptions)
  market_depth?: {
    buy?: Array<{ price: number; qty: number; orders: number }>;
    sell?: Array<{ price: number; qty: number; orders: number }>;
  };

  // Catch-all for any additional fields
  [key: string]: any;
}

export type MessageCallback = (message: DataSocketMessage) => void;
export type ErrorCallback = (error: any) => void;
export type ConnectionCallback = () => void;

export class FyersDataWebSocket {
  private socket: any = null;
  private isConnected = false;
  private accessToken = '';
  private logger: any;
  private hasReceivedFirstMessage = false;

  private messageCallbacks: MessageCallback[] = [];
  private errorCallbacks: ErrorCallback[] = [];
  private connectCallbacks: ConnectionCallback[] = [];
  private closeCallbacks: ConnectionCallback[] = [];

  private subscribedSymbols: Set<string> = new Set();
  private currentMode: number = 2; // 2 = SymbolUpdate (full OHLCV), 1 = LiteMode

  constructor(logger?: any) {
    this.logger = logger || {
      info: (...args: any[]) => console.log('[Fyers Data WebSocket]', ...args),
      debug: (...args: any[]) => console.log('[Fyers Data WebSocket]', ...args),
      error: (...args: any[]) => console.error('[Fyers Data WebSocket]', ...args),
    };
  }

  /**
   * Initialize WebSocket connection
   * IMPORTANT: The Fyers JS SDK defaults to LiteMode which only sends ltp and volume
   * We need full SymbolUpdate mode to get OHLCV data
   */
  connect(accessToken: string): void {
    if (this.isConnected) {
      this.logger.info('Already connected');
      return;
    }

    this.accessToken = accessToken;

    this.logger.info('Initializing Fyers Data WebSocket...');
    this.logger.info('CRITICAL: Requesting FULL data mode (not lite mode)');

    // Create new instance with access token
    // The JS SDK getInstance() may return a cached instance that's in lite mode
    // So we need to ensure we get full data
    this.socket = fyersDataSocket.getInstance(accessToken);

    this.socket.on('error', (error: any) => {
      this.logger.error('WebSocket Error:', error);
      this.errorCallbacks.forEach(cb => cb(error));
    });

    this.socket.on('connect', () => {
      this.logger.info('✓ WebSocket Connected');
      this.isConnected = true;

      // Subscribe to pending symbols AFTER connection
      if (this.subscribedSymbols.size > 0) {
        const symbols = Array.from(this.subscribedSymbols);
        this.logger.info(`Subscribing to ${symbols.length} symbols in SYMBOL UPDATE mode...`);

        // CRITICAL: Use SymbolUpdate mode (not LiteMode)
        // Mode 2 = SymbolUpdate (full OHLCV data)
        // Mode 1 = LiteMode (only ltp and volume)
        this.socket.subscribe(symbols);
        this.socket.mode(this.socket.SymbolUpdate);

        this.logger.info('✓ Subscription complete - waiting for full OHLCV data...');
      }

      this.connectCallbacks.forEach(cb => cb());
    });

    this.socket.on('close', () => {
      this.logger.info('WebSocket Connection closed');
      this.isConnected = false;
      this.closeCallbacks.forEach(cb => cb());
    });

    this.socket.on('message', (message: any) => {
      // Log first message to verify we're getting full data
      if (!this.hasReceivedFirstMessage) {
        this.logger.info('First message received:', message);
        this.logger.info('Checking for OHLCV fields:', {
          has_open: 'open_price' in message,
          has_high: 'high_price' in message,
          has_low: 'low_price' in message,
          has_prev_close: 'prev_close_price' in message,
          has_change: 'ch' in message,
          has_change_percent: 'chp' in message
        });
        this.hasReceivedFirstMessage = true;
      }

      // Forward message to callbacks
      if (message && typeof message === 'object') {
        this.messageCallbacks.forEach(cb => cb(message));
      }
    });

    this.socket.autoreconnect();
    this.socket.connect();
  }

  /**
   * Get mode constant from socket instance
   */
  private getModeConstant(mode: number): any {
    if (!this.socket) return null;

    const modeMap: Record<number, string> = {
      1: 'LiteMode',
      2: 'QuoteMode',
      3: 'FullMode',
    };

    const modeName = modeMap[mode];
    return modeName ? this.socket[modeName] : null;
  }

  /**
   * Disconnect WebSocket
   */
  disconnect(): void {
    if (this.socket && this.isConnected) {
      this.socket.close();
      this.socket = null;
      this.isConnected = false;
      this.subscribedSymbols.clear();
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
   * Subscribe to symbols in SymbolUpdate mode (full OHLCV data)
   * @param symbols Array of symbols in format 'NSE:SBIN-EQ'
   */
  subscribe(symbols: string[]): void {
    if (!this.socket) {
      this.logger.error('Cannot subscribe: socket not initialized');
      return;
    }

    // Store symbols for subscription (will be subscribed on connect)
    symbols.forEach(symbol => this.subscribedSymbols.add(symbol));

    // If already connected, subscribe immediately
    if (this.isConnected) {
      this.logger.info(`Subscribing to ${symbols.length} symbols in SymbolUpdate mode (full OHLCV)...`);

      // Subscribe to symbols - just pass symbol array
      this.socket.subscribe(symbols);

      // Set mode to SymbolUpdate for full data
      this.socket.mode(this.socket.SymbolUpdate);

      this.logger.info('✓ Subscription sent - waiting for OHLCV data');
    } else {
      this.logger.info(`${symbols.length} symbols queued - will subscribe when connected`);
    }
  }

  /**
   * Unsubscribe from symbols
   */
  unsubscribe(symbols: string[]): void {
    if (!this.socket || !this.isConnected) {
      return;
    }

    this.socket.unsubscribe(symbols);
    symbols.forEach(symbol => this.subscribedSymbols.delete(symbol));
    this.logger.info(`Unsubscribed from ${symbols.length} symbols`);
  }

  /**
   * Subscribe to message updates
   */
  onMessage(callback: MessageCallback): () => void {
    this.messageCallbacks.push(callback);
    return () => {
      const index = this.messageCallbacks.indexOf(callback);
      if (index > -1) this.messageCallbacks.splice(index, 1);
    };
  }

  /**
   * Subscribe to connection events
   */
  onConnect(callback: ConnectionCallback): () => void {
    this.connectCallbacks.push(callback);
    return () => {
      const index = this.connectCallbacks.indexOf(callback);
      if (index > -1) this.connectCallbacks.splice(index, 1);
    };
  }

  /**
   * Subscribe to close events
   */
  onClose(callback: ConnectionCallback): () => void {
    this.closeCallbacks.push(callback);
    return () => {
      const index = this.closeCallbacks.indexOf(callback);
      if (index > -1) this.closeCallbacks.splice(index, 1);
    };
  }

  /**
   * Subscribe to errors
   */
  onError(callback: ErrorCallback): () => void {
    this.errorCallbacks.push(callback);
    return () => {
      const index = this.errorCallbacks.indexOf(callback);
      if (index > -1) this.errorCallbacks.splice(index, 1);
    };
  }

  /**
   * Get list of subscribed symbols
   */
  getSubscribedSymbols(): string[] {
    return Array.from(this.subscribedSymbols);
  }

  /**
   * Get current mode
   */
  getCurrentMode(): number {
    return this.currentMode;
  }

  /**
   * Remove all callbacks
   */
  clearCallbacks(): void {
    this.messageCallbacks = [];
    this.errorCallbacks = [];
    this.connectCallbacks = [];
    this.closeCallbacks = [];
  }
}
