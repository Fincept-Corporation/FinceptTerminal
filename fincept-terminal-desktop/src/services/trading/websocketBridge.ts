// WebSocket Bridge - TypeScript wrapper for Rust WebSocket system
// Provides a clean API for frontend components to use Rust WebSocket backend

import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';

const INVOKE_TIMEOUT_MS = 15000;

function withTimeout<T>(promise: Promise<T>, ms: number = INVOKE_TIMEOUT_MS, label?: string): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error(`[WebSocketBridge] ${label || 'invoke'} timed out after ${ms}ms`));
    }, ms);
    promise.then(
      (val) => { clearTimeout(timer); resolve(val); },
      (err) => { clearTimeout(timer); reject(err); }
    );
  });
}

// ============================================================================
// TYPES (matching Rust types)
// ============================================================================

export interface ProviderConfig {
  name: string;
  url: string;
  api_key?: string;
  api_secret?: string;
  enabled: boolean;
  reconnect_delay_ms: number;
  max_reconnect_attempts: number;
  heartbeat_interval_ms: number;
}

export interface TickerData {
  provider: string;
  symbol: string;
  price: number;
  bid?: number;
  ask?: number;
  bid_size?: number;
  ask_size?: number;
  volume?: number;
  high?: number;
  low?: number;
  open?: number;
  close?: number;
  change?: number;
  change_percent?: number;
  quote_volume?: number;
  timestamp: number;
}

export interface OrderBookLevel {
  price: number;
  quantity: number;
  count?: number;
}

export interface OrderBookData {
  provider: string;
  symbol: string;
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  timestamp: number;
  is_snapshot: boolean;
}

export interface TradeData {
  provider: string;
  symbol: string;
  trade_id?: string;
  price: number;
  quantity: number;
  side: 'buy' | 'sell' | 'unknown';
  timestamp: number;
}

export interface CandleData {
  provider: string;
  symbol: string;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
  timestamp: number;
  interval: string;
}

export interface StatusData {
  provider: string;
  status: 'connected' | 'connecting' | 'disconnected' | 'reconnecting' | 'error';
  message?: string;
  timestamp: number;
}

export interface ConnectionMetrics {
  provider: string;
  status: 'connected' | 'connecting' | 'disconnected' | 'reconnecting' | 'error';
  connected_at?: number;
  last_message_at?: number;
  messages_received: number;
  messages_sent: number;
  active_subscriptions: number;
  reconnect_count: number;
  latency_ms?: number;
}

// ============================================================================
// WEBSOCKET BRIDGE CLASS
// ============================================================================

export class WebSocketBridge {
  // Track all listeners per event type (multiple listeners allowed)
  private eventListeners: Map<string, Set<UnlistenFn>> = new Map();

  // ========================================================================
  // CONFIGURATION
  // ========================================================================

  async setConfig(config: ProviderConfig): Promise<void> {
    await withTimeout(invoke('ws_set_config', { config }), INVOKE_TIMEOUT_MS, 'setConfig');
  }

  // ========================================================================
  // CONNECTION MANAGEMENT
  // ========================================================================

  async connect(provider: string): Promise<void> {
    await withTimeout(invoke('ws_connect', { provider }), INVOKE_TIMEOUT_MS, 'connect');
  }

  async disconnect(provider: string): Promise<void> {
    await withTimeout(invoke('ws_disconnect', { provider }), INVOKE_TIMEOUT_MS, 'disconnect');
  }

  async reconnect(provider: string): Promise<void> {
    await withTimeout(invoke('ws_reconnect', { provider }), INVOKE_TIMEOUT_MS, 'reconnect');
  }

  // ========================================================================
  // SUBSCRIPTION MANAGEMENT
  // ========================================================================

  async subscribe(
    provider: string,
    symbol: string,
    channel: string,
    params?: Record<string, any>
  ): Promise<void> {
    console.log(`[WebSocketBridge] subscribe: provider='${provider}' symbol='${symbol}' channel='${channel}'`);
    await withTimeout(invoke('ws_subscribe', {
      provider,
      symbol,
      channel,
      params: params ? params : null,
    }), INVOKE_TIMEOUT_MS, 'subscribe');
  }

  async unsubscribe(provider: string, symbol: string, channel: string): Promise<void> {
    await withTimeout(invoke('ws_unsubscribe', { provider, symbol, channel }), INVOKE_TIMEOUT_MS, 'unsubscribe');
  }

  // ========================================================================
  // EVENT LISTENERS
  // ========================================================================

  /**
   * Helper to register a listener and track it properly (allows multiple listeners per event)
   */
  private async registerListener<T>(
    eventName: string,
    callback: (data: T) => void
  ): Promise<UnlistenFn> {
    const unlisten = await listen<T>(eventName, (event) => {
      callback(event.payload);
    });

    // Track listener in set (allows multiple listeners)
    if (!this.eventListeners.has(eventName)) {
      this.eventListeners.set(eventName, new Set());
    }
    this.eventListeners.get(eventName)!.add(unlisten);

    // Return a wrapped unlisten that also removes from tracking
    return () => {
      unlisten();
      const listeners = this.eventListeners.get(eventName);
      if (listeners) {
        listeners.delete(unlisten);
      }
    };
  }

  async onTicker(callback: (data: TickerData) => void): Promise<UnlistenFn> {
    return this.registerListener<TickerData>('ws_ticker', callback);
  }

  async onOrderBook(callback: (data: OrderBookData) => void): Promise<UnlistenFn> {
    return this.registerListener<OrderBookData>('ws_orderbook', callback);
  }

  async onTrade(callback: (data: TradeData) => void): Promise<UnlistenFn> {
    return this.registerListener<TradeData>('ws_trade', callback);
  }

  async onCandle(callback: (data: CandleData) => void): Promise<UnlistenFn> {
    return this.registerListener<CandleData>('ws_candle', callback);
  }

  async onStatus(callback: (data: StatusData) => void): Promise<UnlistenFn> {
    return this.registerListener<StatusData>('ws_status', callback);
  }

  // ========================================================================
  // METRICS
  // ========================================================================

  async getMetrics(provider: string): Promise<ConnectionMetrics | null> {
    return await withTimeout(invoke('ws_get_metrics', { provider }), INVOKE_TIMEOUT_MS, 'getMetrics');
  }

  async getAllMetrics(): Promise<ConnectionMetrics[]> {
    return await withTimeout(invoke('ws_get_all_metrics'), INVOKE_TIMEOUT_MS, 'getAllMetrics');
  }

  // ========================================================================
  // CLEANUP
  // ========================================================================

  async cleanup(): Promise<void> {
    // Unlisten from all events (now properly handles multiple listeners per event)
    for (const listeners of this.eventListeners.values()) {
      for (const unlisten of listeners) {
        unlisten();
      }
    }
    this.eventListeners.clear();
  }
}

// ============================================================================
// SINGLETON INSTANCE
// ============================================================================

export const websocketBridge = new WebSocketBridge();

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * Configure provider with default settings
 */
export async function configureProvider(
  name: string,
  url: string,
  options?: Partial<ProviderConfig>
): Promise<void> {
  const config: ProviderConfig = {
    name,
    url,
    enabled: true,
    reconnect_delay_ms: 5000,
    max_reconnect_attempts: 10,
    heartbeat_interval_ms: 30000,
    ...options,
  };

  await websocketBridge.setConfig(config);
}

/**
 * Quick subscribe helper
 */
export async function subscribeToSymbol(
  provider: string,
  symbol: string,
  channels: string[] = ['ticker', 'book', 'trade']
): Promise<void> {
  for (const channel of channels) {
    await websocketBridge.subscribe(provider, symbol, channel);
  }
}

/**
 * Quick unsubscribe helper
 */
export async function unsubscribeFromSymbol(
  provider: string,
  symbol: string,
  channels: string[] = ['ticker', 'book', 'trade']
): Promise<void> {
  for (const channel of channels) {
    await websocketBridge.unsubscribe(provider, symbol, channel);
  }
}

/**
 * Get WebSocket manager instance (for backwards compatibility)
 */
export function getWebSocketManager() {
  return websocketBridge;
}

/**
 * Get available providers
 */
export function getAvailableProviders(): string[] {
  return ['kraken', 'binance', 'hyperliquid', 'okx', 'coinbase', 'bybit', 'kucoin', 'gateio'];
}

/**
 * Connection status enum (for backwards compatibility)
 */
export enum ConnectionStatus {
  CONNECTED = 'connected',
  DISCONNECTED = 'disconnected',
  CONNECTING = 'connecting',
  ERROR = 'error'
}
