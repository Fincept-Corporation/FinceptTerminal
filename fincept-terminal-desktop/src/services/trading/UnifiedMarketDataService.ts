// UnifiedMarketDataService - Multi-provider market data service
// Single source of truth for current prices across all providers
// Works independently of broker adapters

import { EventEmitter } from 'eventemitter3';
import { websocketBridge, TickerData as WsTickerData } from './websocketBridge';
import type { UnlistenFn } from '@tauri-apps/api/event';

// Price data interface - normalized across providers
export interface PriceData {
  bid: number;
  ask: number;
  last: number;
  timestamp: number;
  provider: string;
  volume?: number;
  high?: number;
  low?: number;
  change?: number;
  changePercent?: number;
}

// Event types emitted by the service
export interface MarketDataEvents {
  priceUpdate: (data: PriceData & { symbol: string }) => void;
  providerConnected: (provider: string) => void;
  providerDisconnected: (provider: string) => void;
  error: (error: { provider: string; message: string }) => void;
}

// Callback type for price subscriptions
type PriceCallback = (price: PriceData) => void;

/**
 * UnifiedMarketDataService - Singleton service for market data
 *
 * Features:
 * - Multi-provider support (Kraken, Binance, HyperLiquid, etc.)
 * - Price caching with automatic updates
 * - Event-based notifications for price changes
 * - Provider priority for symbols
 * - Works independently of trading adapters
 */
export class UnifiedMarketDataService extends EventEmitter<MarketDataEvents> {
  // Cache: Map<"provider:symbol", PriceData>
  private priceCache: Map<string, PriceData> = new Map();

  // Symbol -> providers that support it
  private symbolProviders: Map<string, Set<string>> = new Map();

  // Symbol -> preferred provider
  private symbolPreferredProvider: Map<string, string> = new Map();

  // Symbol -> array of callbacks
  private subscriptions: Map<string, Set<PriceCallback>> = new Map();

  // Active WebSocket subscriptions
  private activeWsSubscriptions: Map<string, boolean> = new Map();

  // WebSocket listener cleanup functions
  private wsUnlisteners: UnlistenFn[] = [];

  // Initialization state
  private initialized = false;

  constructor() {
    super();
  }

  /**
   * Initialize the service - connects to WebSocket events
   */
  async initialize(): Promise<void> {
    if (this.initialized) return;

    try {
      // Listen to ticker updates from all providers
      const tickerUnlisten = await websocketBridge.onTicker((ticker) => {
        this.handleTickerUpdate(ticker);
      });
      this.wsUnlisteners.push(tickerUnlisten);

      // Listen to connection status
      const statusUnlisten = await websocketBridge.onStatus((status) => {
        if (status.status === 'connected') {
          this.emit('providerConnected', status.provider);
        } else if (status.status === 'disconnected' || status.status === 'error') {
          this.emit('providerDisconnected', status.provider);
          if (status.message) {
            this.emit('error', { provider: status.provider, message: status.message });
          }
        }
      });
      this.wsUnlisteners.push(statusUnlisten);

      this.initialized = true;
      console.log('[UnifiedMarketDataService] Initialized');
    } catch (error) {
      console.error('[UnifiedMarketDataService] Failed to initialize:', error);
      throw error;
    }
  }

  /**
   * Handle incoming ticker update from WebSocket
   */
  private handleTickerUpdate(ticker: WsTickerData): void {
    const price: PriceData = {
      bid: ticker.bid || ticker.price,
      ask: ticker.ask || ticker.price,
      last: ticker.price,
      timestamp: ticker.timestamp,
      provider: ticker.provider,
      volume: ticker.volume,
      high: ticker.high,
      low: ticker.low,
      change: ticker.change,
      changePercent: ticker.change_percent,
    };

    // Update cache
    const cacheKey = `${ticker.provider}:${ticker.symbol}`;
    this.priceCache.set(cacheKey, price);

    // Track which providers support this symbol
    if (!this.symbolProviders.has(ticker.symbol)) {
      this.symbolProviders.set(ticker.symbol, new Set());
    }
    this.symbolProviders.get(ticker.symbol)!.add(ticker.provider);

    // Emit to EventEmitter listeners
    this.emit('priceUpdate', { ...price, symbol: ticker.symbol });

    // Notify direct subscribers for this symbol
    const callbacks = this.subscriptions.get(ticker.symbol);
    if (callbacks) {
      for (const callback of callbacks) {
        try {
          callback(price);
        } catch (error) {
          console.error('[UnifiedMarketDataService] Callback error:', error);
        }
      }
    }
  }

  /**
   * Get current price for a symbol
   * @param symbol Trading symbol (e.g., "BTC/USD")
   * @param preferredProvider Optional provider to prefer
   * @returns Price data or undefined if not available
   */
  getCurrentPrice(symbol: string, preferredProvider?: string): PriceData | undefined {
    // Try preferred provider first
    const provider = preferredProvider || this.symbolPreferredProvider.get(symbol);
    if (provider) {
      const price = this.priceCache.get(`${provider}:${symbol}`);
      if (price) return price;
    }

    // Try any available provider
    const providers = this.symbolProviders.get(symbol);
    if (providers) {
      for (const p of providers) {
        const price = this.priceCache.get(`${p}:${symbol}`);
        if (price) return price;
      }
    }

    return undefined;
  }

  /**
   * Get prices from all available providers for a symbol
   */
  getAllPrices(symbol: string): Map<string, PriceData> {
    const result = new Map<string, PriceData>();
    const providers = this.symbolProviders.get(symbol);

    if (providers) {
      for (const provider of providers) {
        const price = this.priceCache.get(`${provider}:${symbol}`);
        if (price) {
          result.set(provider, price);
        }
      }
    }

    return result;
  }

  /**
   * Set preferred provider for a symbol
   */
  setPreferredProvider(symbol: string, provider: string): void {
    this.symbolPreferredProvider.set(symbol, provider);
  }

  /**
   * Subscribe to price updates for a symbol
   * Returns an unsubscribe function
   */
  subscribeToPrice(symbol: string, callback: PriceCallback): () => void {
    if (!this.subscriptions.has(symbol)) {
      this.subscriptions.set(symbol, new Set());
    }
    this.subscriptions.get(symbol)!.add(callback);

    // Return unsubscribe function
    return () => {
      const callbacks = this.subscriptions.get(symbol);
      if (callbacks) {
        callbacks.delete(callback);
        if (callbacks.size === 0) {
          this.subscriptions.delete(symbol);
        }
      }
    };
  }

  /**
   * Update price from external source (e.g., REST API, manual update)
   * Useful for components that fetch prices independently
   */
  updatePrice(provider: string, symbol: string, ticker: Partial<WsTickerData>): void {
    const fullTicker: WsTickerData = {
      provider,
      symbol,
      price: ticker.price || 0,
      bid: ticker.bid,
      ask: ticker.ask,
      volume: ticker.volume,
      high: ticker.high,
      low: ticker.low,
      change: ticker.change,
      change_percent: ticker.change_percent,
      timestamp: ticker.timestamp || Date.now(),
    };

    this.handleTickerUpdate(fullTicker);
  }

  /**
   * Subscribe to WebSocket feed for a symbol via a provider
   * This actually connects to the provider's WebSocket
   */
  async subscribeViaWebSocket(provider: string, symbol: string): Promise<void> {
    const key = `${provider}:${symbol}`;
    if (this.activeWsSubscriptions.has(key)) return;

    try {
      await websocketBridge.subscribe(provider, symbol, 'ticker');
      this.activeWsSubscriptions.set(key, true);
      console.log(`[UnifiedMarketDataService] Subscribed to ${provider}:${symbol}`);
    } catch (error) {
      console.error(`[UnifiedMarketDataService] Failed to subscribe to ${provider}:${symbol}:`, error);
      throw error;
    }
  }

  /**
   * Unsubscribe from WebSocket feed for a symbol
   */
  async unsubscribeViaWebSocket(provider: string, symbol: string): Promise<void> {
    const key = `${provider}:${symbol}`;
    if (!this.activeWsSubscriptions.has(key)) return;

    try {
      await websocketBridge.unsubscribe(provider, symbol, 'ticker');
      this.activeWsSubscriptions.delete(key);
      console.log(`[UnifiedMarketDataService] Unsubscribed from ${provider}:${symbol}`);
    } catch (error) {
      console.error(`[UnifiedMarketDataService] Failed to unsubscribe from ${provider}:${symbol}:`, error);
    }
  }

  /**
   * Check if a symbol has recent price data
   * @param maxAgeMs Maximum age of price data in milliseconds (default: 60 seconds)
   */
  hasFreshPrice(symbol: string, maxAgeMs = 60000): boolean {
    const price = this.getCurrentPrice(symbol);
    if (!price) return false;

    return Date.now() - price.timestamp < maxAgeMs;
  }

  /**
   * Get all symbols with price data
   */
  getAvailableSymbols(): string[] {
    return Array.from(this.symbolProviders.keys());
  }

  /**
   * Get all providers for a symbol
   */
  getProvidersForSymbol(symbol: string): string[] {
    const providers = this.symbolProviders.get(symbol);
    return providers ? Array.from(providers) : [];
  }

  /**
   * Clear all cached prices
   */
  clearCache(): void {
    this.priceCache.clear();
  }

  /**
   * Cleanup - unsubscribe from all WebSocket events
   */
  async destroy(): Promise<void> {
    // Unsubscribe from all WebSocket feeds
    for (const key of this.activeWsSubscriptions.keys()) {
      const [provider, symbol] = key.split(':');
      await this.unsubscribeViaWebSocket(provider, symbol);
    }

    // Remove WebSocket listeners
    for (const unlisten of this.wsUnlisteners) {
      unlisten();
    }
    this.wsUnlisteners = [];

    // Clear all data
    this.priceCache.clear();
    this.subscriptions.clear();
    this.symbolProviders.clear();
    this.symbolPreferredProvider.clear();
    this.initialized = false;

    this.removeAllListeners();
    console.log('[UnifiedMarketDataService] Destroyed');
  }
}

// Singleton instance
let instance: UnifiedMarketDataService | null = null;

/**
 * Get the singleton instance of UnifiedMarketDataService
 */
export function getMarketDataService(): UnifiedMarketDataService {
  if (!instance) {
    instance = new UnifiedMarketDataService();
  }
  return instance;
}

/**
 * Initialize the market data service
 */
export async function initializeMarketDataService(): Promise<UnifiedMarketDataService> {
  const service = getMarketDataService();
  await service.initialize();
  return service;
}
