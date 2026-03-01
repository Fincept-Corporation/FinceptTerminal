/**
 * Fyers WebSocket Operations
 *
 * Module-level WebSocket functions used by FyersAdapter
 */

import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import type {
  TickData,
  MarketDepth,
  StockExchange,
} from '../../types';

export type DepthEmitter = (data: { symbol: string; exchange: StockExchange } & MarketDepth) => void;
export type TickEmitter = (tick: TickData) => void;

/**
 * Check if Indian stock market is currently open
 * NSE/BSE: Monday-Friday, 9:15 AM - 3:30 PM IST
 */
export function fyersIsMarketOpen(): boolean {
  const now = new Date();
  const istFormatter = new Intl.DateTimeFormat('en-US', {
    timeZone: 'Asia/Kolkata',
    hour: 'numeric',
    minute: 'numeric',
    hour12: false,
    weekday: 'short',
  });

  const parts = istFormatter.formatToParts(now);
  const weekday = parts.find(p => p.type === 'weekday')?.value || '';
  const hour = parseInt(parts.find(p => p.type === 'hour')?.value || '0', 10);
  const minute = parseInt(parts.find(p => p.type === 'minute')?.value || '0', 10);

  const timeInMinutes = hour * 60 + minute;

  const marketOpen = 9 * 60 + 15;   // 9:15 AM = 555 minutes
  const marketClose = 15 * 60 + 30; // 3:30 PM = 930 minutes

  const weekdays = ['Mon', 'Tue', 'Wed', 'Thu', 'Fri'];
  const isWeekday = weekdays.includes(weekday);
  const isWithinHours = timeInMinutes >= marketOpen && timeInMinutes <= marketClose;

  console.log(`[Fyers] Market check: ${weekday} ${hour}:${minute.toString().padStart(2, '0')} IST | open=${isWeekday && isWithinHours}`);

  return isWeekday && isWithinHours;
}

/**
 * Connect WebSocket directly via Tauri backend
 */
export async function fyersConnectWebSocket(
  apiKey: string,
  accessToken: string
): Promise<void> {
  const wsAccessToken = `${apiKey}:${accessToken}`;

  console.log('[Fyers WS] Connecting...', {
    hasApiKey: !!apiKey,
    apiKeyLength: apiKey?.length || 0,
    accessTokenLength: accessToken?.length || 0,
    wsAccessTokenLength: wsAccessToken.length,
  });

  try {
    const response = await invoke<{
      success: boolean;
      data?: boolean;
      error?: string;
    }>('fyers_ws_connect', {
      app: undefined,
      apiKey,
      accessToken: wsAccessToken,
    });

    console.log('[Fyers WS] Response:', response);

    if (!response.success) {
      const errorMsg = response.error || 'WebSocket connection failed (no error details)';
      console.error('[Fyers WS] ✗ Connection failed:', errorMsg);
      throw new Error(errorMsg);
    }

    console.log('[Fyers WS] ✓ Connected successfully');
  } catch (invokeError) {
    console.error('[Fyers WS] ✗ Tauri invoke error:', invokeError);
    throw invokeError;
  }
}

/**
 * Disconnect WebSocket
 */
export async function fyersDisconnectWebSocket(): Promise<void> {
  try {
    await invoke('fyers_ws_disconnect');
    console.log('[Fyers] WebSocket disconnected');
  } catch (error) {
    console.error('[Fyers] WebSocket disconnect error:', error);
  }
}

/**
 * Check if WebSocket is connected
 */
export async function fyersIsWebSocketConnected(): Promise<boolean> {
  try {
    return await invoke<boolean>('fyers_ws_is_connected');
  } catch {
    return false;
  }
}

/**
 * Subscribe to a symbol via WebSocket
 */
export async function fyersWsSubscribe(
  fyersSymbol: string,
  mode: string
): Promise<{ success: boolean; error?: string }> {
  return invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe', {
    symbol: fyersSymbol,
    mode,
  });
}

/**
 * Batch subscribe to symbols via WebSocket
 */
export async function fyersWsSubscribeBatch(
  symbols: string[],
  mode: string
): Promise<{ success: boolean; error?: string }> {
  return invoke<{ success: boolean; error?: string }>('fyers_ws_subscribe_batch', {
    symbols,
    mode,
  });
}

/**
 * Unsubscribe from a symbol via WebSocket
 */
export async function fyersWsUnsubscribe(
  fyersSymbol: string
): Promise<{ success: boolean; error?: string }> {
  return invoke<{ success: boolean; data?: boolean; error?: string }>('fyers_ws_unsubscribe', {
    symbol: fyersSymbol,
  });
}

/**
 * Set up Tauri event listeners for WebSocket messages
 * Returns unlisten functions to call on cleanup
 */
export async function fyersSetupWebSocketListeners(
  emitTick: TickEmitter,
  emitDepth: DepthEmitter,
  onDisconnect: () => void
): Promise<Array<() => void>> {
  const unlisteners: Array<() => void> = [];

  // Listen for ticker data
  const tickerUnlisten = await listen<{
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
  }>('fyers_ticker', (event) => {
    const data = event.payload;

    const [exchange, symbolPart] = data.symbol.split(':');
    const symbol = symbolPart?.replace('-EQ', '').replace('-INDEX', '') || data.symbol;

    console.log('[Fyers] Ticker update:', symbol, 'LTP:', data.price, 'change:', data.change_percent?.toFixed(2) + '%');

    const tick: TickData = {
      symbol,
      exchange: (exchange as StockExchange) || 'NSE',
      mode: 'quote',
      lastPrice: data.price,
      open: data.open,
      high: data.high,
      low: data.low,
      close: data.close,
      volume: data.volume,
      change: data.change,
      changePercent: data.change_percent,
      bid: data.bid,
      ask: data.ask,
      bidQty: data.bid_size,
      askQty: data.ask_size,
      timestamp: data.timestamp,
    };

    emitTick(tick);
  });

  unlisteners.push(tickerUnlisten);

  // Listen for orderbook/depth data
  const orderbookUnlisten = await listen<{
    provider: string;
    symbol: string;
    bids: Array<{ price: number; quantity: number; count?: number }>;
    asks: Array<{ price: number; quantity: number; count?: number }>;
    timestamp: number;
    is_snapshot?: boolean;
  }>('fyers_orderbook', (event) => {
    const data = event.payload;

    const [exchange, symbolPart] = data.symbol.split(':');
    const symbol = symbolPart?.replace('-EQ', '').replace('-INDEX', '') || data.symbol;

    console.log('[Fyers] Depth update:', symbol, 'bids:', data.bids?.length, 'asks:', data.asks?.length);

    const bestBid = data.bids?.[0];
    const bestAsk = data.asks?.[0];

    const tick: TickData = {
      symbol,
      exchange: (exchange as StockExchange) || 'NSE',
      mode: 'full',
      lastPrice: bestBid?.price || 0,
      bid: bestBid?.price,
      ask: bestAsk?.price,
      bidQty: bestBid?.quantity,
      askQty: bestAsk?.quantity,
      timestamp: data.timestamp,
      depth: {
        bids: data.bids?.map((b) => ({
          price: b.price,
          quantity: b.quantity,
          orders: b.count || 0,
        })) || [],
        asks: data.asks?.map((a) => ({
          price: a.price,
          quantity: a.quantity,
          orders: a.count || 0,
        })) || [],
      },
    };

    emitTick(tick);

    if (tick.depth) {
      emitDepth({
        symbol,
        exchange: (exchange as StockExchange) || 'NSE',
        bids: tick.depth.bids,
        asks: tick.depth.asks,
      });
    }
  });

  unlisteners.push(orderbookUnlisten);

  // Listen for status updates
  const statusUnlisten = await listen<{
    provider: string;
    status: string;
    message: string;
    timestamp: number;
  }>('fyers_status', (event) => {
    const data = event.payload;
    console.log(`[Fyers] Status: ${data.status} - ${data.message}`);

    if (data.status === 'disconnected' || data.status === 'error') {
      onDisconnect();
    }
  });

  unlisteners.push(statusUnlisten);

  console.log('[Fyers] WebSocket event listeners set up');
  return unlisteners;
}
