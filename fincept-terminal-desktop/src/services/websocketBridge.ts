// WebSocket Bridge - TypeScript wrapper for Rust WebSocket system
// Provides a clean API for frontend components to use Rust WebSocket backend

import { invoke } from '@tauri-apps/api/core';
import { listen, UnlistenFn } from '@tauri-apps/api/event';

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
  private eventListeners: Map<string, UnlistenFn> = new Map();

  // ========================================================================
  // CONFIGURATION
  // ========================================================================

  async setConfig(config: ProviderConfig): Promise<void> {
    await invoke('ws_set_config', { config });
  }

  // ========================================================================
  // CONNECTION MANAGEMENT
  // ========================================================================

  async connect(provider: string): Promise<void> {
    await invoke('ws_connect', { provider });
  }

  async disconnect(provider: string): Promise<void> {
    await invoke('ws_disconnect', { provider });
  }

  async reconnect(provider: string): Promise<void> {
    await invoke('ws_reconnect', { provider });
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
    await invoke('ws_subscribe', {
      provider,
      symbol,
      channel,
      params: params ? params : null,
    });
  }

  async unsubscribe(provider: string, symbol: string, channel: string): Promise<void> {
    await invoke('ws_unsubscribe', { provider, symbol, channel });
  }

  // ========================================================================
  // EVENT LISTENERS
  // ========================================================================

  async onTicker(callback: (data: TickerData) => void): Promise<UnlistenFn> {
    const unlisten = await listen<TickerData>('ws_ticker', (event) => {
      callback(event.payload);
    });
    this.eventListeners.set('ws_ticker', unlisten);
    return unlisten;
  }

  async onOrderBook(callback: (data: OrderBookData) => void): Promise<UnlistenFn> {
    const unlisten = await listen<OrderBookData>('ws_orderbook', (event) => {
      callback(event.payload);
    });
    this.eventListeners.set('ws_orderbook', unlisten);
    return unlisten;
  }

  async onTrade(callback: (data: TradeData) => void): Promise<UnlistenFn> {
    const unlisten = await listen<TradeData>('ws_trade', (event) => {
      callback(event.payload);
    });
    this.eventListeners.set('ws_trade', unlisten);
    return unlisten;
  }

  async onCandle(callback: (data: CandleData) => void): Promise<UnlistenFn> {
    const unlisten = await listen<CandleData>('ws_candle', (event) => {
      callback(event.payload);
    });
    this.eventListeners.set('ws_candle', unlisten);
    return unlisten;
  }

  async onStatus(callback: (data: StatusData) => void): Promise<UnlistenFn> {
    const unlisten = await listen<StatusData>('ws_status', (event) => {
      callback(event.payload);
    });
    this.eventListeners.set('ws_status', unlisten);
    return unlisten;
  }

  // ========================================================================
  // METRICS
  // ========================================================================

  async getMetrics(provider: string): Promise<ConnectionMetrics | null> {
    return await invoke('ws_get_metrics', { provider });
  }

  async getAllMetrics(): Promise<ConnectionMetrics[]> {
    return await invoke('ws_get_all_metrics');
  }

  // ========================================================================
  // CLEANUP
  // ========================================================================

  async cleanup(): Promise<void> {
    // Unlisten from all events
    for (const unlisten of this.eventListeners.values()) {
      unlisten();
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
  return ['kraken', 'binance', 'coinbase']; // Add more as needed
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
