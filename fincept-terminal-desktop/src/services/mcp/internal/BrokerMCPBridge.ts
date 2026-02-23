// Broker MCP Bridge - Connects BrokerContext to MCP Internal Tools
// Provides unified trading interface for all brokers via MCP

import { terminalMCPProvider } from './TerminalMCPProvider';
import type { IExchangeAdapter } from '@/brokers/crypto/types';
import type { PaperTradingAdapter } from '@/paper-trading';
import { getMarketDataService } from '@/services/trading/UnifiedMarketDataService';

interface BrokerMCPBridgeConfig {
  activeAdapter: IExchangeAdapter | PaperTradingAdapter | null;
  /** Real exchange adapter — used in paper mode for market discovery & price fetching */
  realAdapter?: IExchangeAdapter | null;
  tradingMode: 'live' | 'paper';
  activeBroker: string;
}

// Preferred quote currencies in order of priority
const PREFERRED_QUOTES = ['USD', 'USDT', 'USDC', 'EUR', 'BTC', 'ETH'];

/**
 * Bridge that connects BrokerContext to MCP tools
 * Allows AI to control trading across all brokers (Kraken, Binance, HyperLiquid, etc.)
 *
 * Symbol resolution is FULLY DYNAMIC — it fetches ALL markets from the exchange
 * and builds a lookup index so ANY coin listed on the exchange can be found by:
 *   - exact pair (BTC/USD)
 *   - base ticker (BTC, POL, BONK, any ticker)
 *   - exchange-specific ID / altname (XXBT, XETH, etc.)
 *   - partial / substring match
 */
export class BrokerMCPBridge {
  private config: BrokerMCPBridgeConfig | null = null;
  private marketsCache: any[] | null = null;
  private marketsCacheTime = 0;
  // Index: lowercased key → list of market symbols  (built from ALL markets)
  private marketsIndex: Map<string, string[]> | null = null;
  private static MARKETS_CACHE_TTL = 5 * 60 * 1000; // 5 minutes

  /**
   * Initialize or update the bridge with broker context
   */
  connect(config: BrokerMCPBridgeConfig): void {
    this.config = config;
    this.marketsCache = null;
    this.marketsIndex = null;
    this.updateMCPContexts();

    // Pre-fetch markets in the background so first resolution is fast
    this.getMarkets().catch((err) => {
      console.warn('[BrokerMCPBridge] Background market pre-fetch failed:', err);
    });
  }

  /**
   * Get the adapter best suited for market discovery & price fetching.
   * In paper mode, prefer the real exchange adapter (has full CCXT market list).
   * Falls back to active adapter.
   */
  private getMarketAdapter(): IExchangeAdapter | PaperTradingAdapter | null {
    if (this.config?.tradingMode === 'paper' && this.config.realAdapter && this.config.realAdapter.isConnected()) {
      return this.config.realAdapter;
    }
    return this.config?.activeAdapter || null;
  }

  /**
   * Fetch and cache ALL markets from the exchange, build a lookup index.
   * Uses the real exchange adapter when available (paper mode) for full market coverage.
   */
  private async getMarkets(): Promise<any[]> {
    const adapter = this.getMarketAdapter();
    if (!adapter) return [];

    const now = Date.now();
    if (this.marketsCache && this.marketsIndex && (now - this.marketsCacheTime) < BrokerMCPBridge.MARKETS_CACHE_TTL) {
      return this.marketsCache;
    }

    try {
      if (typeof adapter.fetchMarkets === 'function') {
        // Retry logic for fetchMarkets with exponential backoff
        let lastError: any = null;
        const maxRetries = 2;

        for (let attempt = 0; attempt <= maxRetries; attempt++) {
          try {
            const markets = await adapter.fetchMarkets();
            this.marketsCache = markets;
            this.marketsCacheTime = now;
            this.buildMarketsIndex(markets);
            console.log(`[BrokerMCPBridge] Loaded ${markets.length} markets, index has ${this.marketsIndex?.size || 0} keys`);
            return markets;
          } catch (err: any) {
            lastError = err;
            const isTimeout = err?.message?.includes('timed out') || err?.message?.includes('timeout');

            if (isTimeout && attempt < maxRetries) {
              const delay = 2000 * Math.pow(2, attempt); // 2s, 4s
              console.warn(`[BrokerMCPBridge] Market fetch timeout (attempt ${attempt + 1}/${maxRetries + 1}), retrying in ${delay}ms...`);
              await new Promise(resolve => setTimeout(resolve, delay));
            } else {
              throw err;
            }
          }
        }

        if (lastError) throw lastError;
      }
    } catch (err) {
      console.warn('[BrokerMCPBridge] Failed to fetch markets after retries:', err);
    }
    return this.marketsCache || [];
  }

  /**
   * Build a multi-key lookup index from ALL exchange markets.
   * Keys include: base, baseId, altname, full symbol, id — all lowercased.
   * This allows resolution of ANY coin on the exchange without a hardcoded list.
   */
  private buildMarketsIndex(markets: any[]): void {
    const index = new Map<string, string[]>();

    const addKey = (key: string, symbol: string) => {
      if (!key) return;
      const k = key.toLowerCase();
      const existing = index.get(k);
      if (existing) {
        if (!existing.includes(symbol)) existing.push(symbol);
      } else {
        index.set(k, [symbol]);
      }
    };

    for (const m of markets) {
      if (!m.symbol || !m.active) continue;
      const sym = m.symbol as string; // e.g. "BTC/USD"

      // Index by full symbol: "btc/usd"
      addKey(sym, sym);

      // Index by base: "btc"
      if (m.base) addKey(m.base, sym);

      // Index by baseId (exchange-specific): "xxbt" for Kraken's BTC
      if (m.baseId) addKey(m.baseId, sym);

      // Index by exchange info fields (altname, name, etc.)
      if (m.info) {
        if (m.info.altname) addKey(m.info.altname, sym);
        if (m.info.wsname) addKey(m.info.wsname, sym);
        if (m.info.name) addKey(m.info.name, sym);
        // Some exchanges put a "base_currency" or "base_asset" field
        if (m.info.base_currency) addKey(m.info.base_currency, sym);
        if (m.info.baseAsset) addKey(m.info.baseAsset, sym);
      }

      // Index by exchange id: "btcusd" etc.
      if (m.id) addKey(m.id, sym);
    }

    this.marketsIndex = index;
  }

  /**
   * Sort matched symbols by quote currency preference (USD first, then USDT, USDC, etc.)
   */
  private sortByQuotePreference(symbols: string[]): string[] {
    return [...symbols].sort((a, b) => {
      const quoteA = a.split('/')[1]?.toUpperCase() || '';
      const quoteB = b.split('/')[1]?.toUpperCase() || '';
      const idxA = PREFERRED_QUOTES.indexOf(quoteA);
      const idxB = PREFERRED_QUOTES.indexOf(quoteB);
      return (idxA === -1 ? 999 : idxA) - (idxB === -1 ? 999 : idxB);
    });
  }

  /**
   * Resolve a user query (ANY name, ticker, partial, or pair) to valid trading pair(s).
   * Fully dynamic — works with ALL symbols on the connected exchange.
   *
   * Resolution order:
   * 1. Exact pair match via fetchTicker (e.g. "BTC/USD")
   * 2. Exact key lookup in markets index (covers base, baseId, altname, id)
   * 3. Try appending common quote currencies to raw input
   * 4. Substring / fuzzy search across all market keys
   * 5. Direct fetchTicker attempts with quote currency variants
   */
  private async resolveSymbol(query: string): Promise<{ symbol: string; price?: number; matches: string[] }> {
    const adapter = this.config?.activeAdapter;
    const broker = this.config?.activeBroker || '';
    if (!adapter) throw new Error('No adapter connected');

    // Validate adapter belongs to current broker to prevent stale config issues
    if (adapter && 'id' in adapter && adapter.id !== broker) {
      console.warn(`[BrokerMCPBridge] Adapter ID (${adapter.id}) mismatch with broker (${broker}) - config may be stale`);
      throw new Error(`Broker adapter mismatch - please retry`);
    }

    const input = query.trim();
    const inputLower = input.toLowerCase();
    const marketDataService = getMarketDataService();
    const marketAdapter = this.getMarketAdapter(); // real exchange adapter in paper mode

    // Helper: fetch price and cache it.
    // Tries the active adapter first, then the real exchange adapter (for paper mode).
    const fetchAndCachePrice = async (symbol: string): Promise<number | undefined> => {
      const adaptersToTry = [adapter];
      if (marketAdapter && marketAdapter !== adapter) {
        // Validate market adapter also belongs to current broker
        if ('id' in marketAdapter && marketAdapter.id !== broker) {
          console.warn(`[BrokerMCPBridge] Market adapter ID (${marketAdapter.id}) mismatch with broker (${broker})`);
        } else {
          adaptersToTry.push(marketAdapter);
        }
      }

      for (const a of adaptersToTry) {
        try {
          const ticker = await a.fetchTicker(symbol);
          const price = ticker.last || ticker.close || ticker.bid || ticker.ask;
          if (price && price > 0) {
            marketDataService.updatePrice(broker, symbol, {
              price, bid: ticker.bid || price, ask: ticker.ask || price,
              volume: ticker.volume || 0, high: ticker.high || price, low: ticker.low || price,
              change: ticker.change || 0, change_percent: ticker.percentage || 0, timestamp: Date.now(),
            });
            return price;
          }
        } catch { /* this adapter doesn't have it, try next */ }
      }
      return undefined;
    };

    // --- Step 1: Exact pair (contains "/") ---
    if (input.includes('/')) {
      const price = await fetchAndCachePrice(input);
      if (price) return { symbol: input, price, matches: [input] };

      // Try upper-cased version
      const upper = input.toUpperCase();
      if (upper !== input) {
        const price2 = await fetchAndCachePrice(upper);
        if (price2) return { symbol: upper, price: price2, matches: [upper] };
      }
    }

    // --- Step 2: Ensure markets index is loaded ---
    const markets = await this.getMarkets();
    const index = this.marketsIndex || new Map();

    // --- Step 3: Exact key lookup in index ---
    // Strip trailing quote currency if user typed "BTCUSD" or "btcusd"
    const strippedInput = inputLower.replace(/(\/?(usd|usdt|usdc|eur))$/i, '');

    for (const lookupKey of [inputLower, strippedInput]) {
      const exactHits = index.get(lookupKey);
      if (exactHits && exactHits.length > 0) {
        const sorted = this.sortByQuotePreference(exactHits);
        const price = await fetchAndCachePrice(sorted[0]);
        return { symbol: sorted[0], price, matches: sorted.slice(0, 10) };
      }
    }

    // --- Step 4: Try appending quote currencies directly ---
    const inputUpper = input.toUpperCase().replace(/(\/?(USD|USDT|USDC|EUR))$/i, '');
    for (const quote of PREFERRED_QUOTES.slice(0, 3)) { // USD, USDT, USDC
      const pair = `${inputUpper}/${quote}`;
      const price = await fetchAndCachePrice(pair);
      if (price) {
        // Collect other quote variants from index
        const baseHits = index.get(inputUpper.toLowerCase()) || [];
        const allMatches = baseHits.length > 0 ? this.sortByQuotePreference(baseHits) : [pair];
        return { symbol: pair, price, matches: allMatches.slice(0, 10) };
      }
    }

    // --- Step 5: Substring / fuzzy search across index keys ---
    if (index.size > 0 && strippedInput.length >= 2) {
      const fuzzyResults: string[] = [];
      for (const [key, symbols] of index) {
        if (key.includes(strippedInput) || strippedInput.includes(key)) {
          for (const s of symbols) {
            if (!fuzzyResults.includes(s)) fuzzyResults.push(s);
          }
        }
      }

      // Prefer USD-quoted pairs
      const sorted = this.sortByQuotePreference(fuzzyResults).slice(0, 15);
      if (sorted.length > 0) {
        const price = await fetchAndCachePrice(sorted[0]);
        return { symbol: sorted[0], price, matches: sorted };
      }
    }

    // --- Step 6: Brute-force search across raw market list ---
    if (markets.length > 0 && strippedInput.length >= 2) {
      const bruteMatches = markets
        .filter((m: any) => {
          if (!m.symbol || !m.active) return false;
          const sym = (m.symbol as string).toLowerCase();
          const base = (m.base || '').toLowerCase();
          const baseId = (m.baseId || '').toLowerCase();
          const altname = (m.info?.altname || '').toLowerCase();
          const name = (m.info?.name || '').toLowerCase();
          return (
            base === strippedInput ||
            baseId === strippedInput ||
            altname === strippedInput ||
            sym.startsWith(strippedInput + '/') ||
            base.includes(strippedInput) ||
            name.includes(strippedInput)
          );
        })
        .map((m: any) => m.symbol as string);

      const sorted = this.sortByQuotePreference(bruteMatches).slice(0, 15);
      if (sorted.length > 0) {
        const price = await fetchAndCachePrice(sorted[0]);
        return { symbol: sorted[0], price, matches: sorted };
      }
    }

    throw new Error(
      `Could not find any trading pair for "${query}" on ${this.config?.activeBroker || 'the exchange'}. ` +
      `The symbol may not be listed on this exchange. Try searching with crypto_search_symbol first.`
    );
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    this.config = null;
    this.marketsCache = null;
    terminalMCPProvider.setContexts({
      placeTrade: undefined,
      cancelOrder: undefined,
      modifyOrder: undefined,
      getPositions: undefined,
      getBalance: undefined,
      getOrders: undefined,
      getHoldings: undefined,
      closePosition: undefined,
      getQuote: undefined,
      resolveSymbol: undefined,
      searchSymbol: undefined,
    });
  }

  /**
   * Update MCP contexts with current adapter methods
   */
  private updateMCPContexts(): void {
    if (!this.config?.activeAdapter) {
      console.warn('[BrokerMCPBridge] No active adapter available');
      return;
    }

    const { activeAdapter, tradingMode, activeBroker } = this.config;

    terminalMCPProvider.setContexts({
      // Place trade (market, limit, stop, etc.)
      placeTrade: async (params: any) => {
        try {
          let resolvedSymbol = params.symbol;
          let orderPrice = params.price;

          // For market orders, get price from cache or broker
          if (!orderPrice && (params.order_type === 'market' || !params.order_type)) {
            const marketDataService = getMarketDataService();

            // Try cached price first
            const cachedPrice = marketDataService.getCurrentPrice(resolvedSymbol, activeBroker);
            if (cachedPrice && cachedPrice.last > 0) {
              orderPrice = cachedPrice.last;
              console.log(`[BrokerMCPBridge] Using cached market price for ${resolvedSymbol}: ${orderPrice}`);
            } else {
              // Try fetching price: active adapter first, then real adapter, then full symbol resolution
              let priceFetched = false;

              // Try active adapter
              try {
                const ticker = await activeAdapter.fetchTicker(resolvedSymbol);
                orderPrice = ticker.last || ticker.close || ticker.bid || ticker.ask;
                if (orderPrice && orderPrice > 0) {
                  marketDataService.updatePrice(activeBroker, resolvedSymbol, {
                    price: orderPrice, bid: ticker.bid || orderPrice, ask: ticker.ask || orderPrice,
                    volume: ticker.volume || 0, high: ticker.high || orderPrice, low: ticker.low || orderPrice,
                    change: ticker.change || 0, change_percent: ticker.percentage || 0, timestamp: Date.now(),
                  });
                  priceFetched = true;
                  console.log(`[BrokerMCPBridge] Fetched live price for ${resolvedSymbol}: ${orderPrice}`);
                }
              } catch { /* active adapter doesn't have it */ }

              // In paper mode, try the real exchange adapter for price discovery
              if (!priceFetched) {
                const realAdapter = this.getMarketAdapter();
                if (realAdapter && realAdapter !== activeAdapter) {
                  try {
                    const ticker = await realAdapter.fetchTicker(resolvedSymbol);
                    orderPrice = ticker.last || ticker.close || ticker.bid || ticker.ask;
                    if (orderPrice && orderPrice > 0) {
                      marketDataService.updatePrice(activeBroker, resolvedSymbol, {
                        price: orderPrice, bid: ticker.bid || orderPrice, ask: ticker.ask || orderPrice,
                        volume: ticker.volume || 0, high: ticker.high || orderPrice, low: ticker.low || orderPrice,
                        change: ticker.change || 0, change_percent: ticker.percentage || 0, timestamp: Date.now(),
                      });
                      priceFetched = true;
                      console.log(`[BrokerMCPBridge] Fetched price from real adapter for ${resolvedSymbol}: ${orderPrice}`);
                    }
                  } catch { /* real adapter doesn't have it either */ }
                }
              }

              // Full symbol resolution as last resort
              if (!priceFetched) {
                console.log(`[BrokerMCPBridge] Symbol "${resolvedSymbol}" not found directly, attempting resolution...`);
                try {
                  const resolved = await this.resolveSymbol(resolvedSymbol);
                  resolvedSymbol = resolved.symbol;
                  orderPrice = resolved.price;
                  console.log(`[BrokerMCPBridge] Resolved "${params.symbol}" → "${resolvedSymbol}" (price: ${orderPrice})`);
                } catch (resolveError: any) {
                  throw new Error(resolveError.message);
                }
              }

              if (!orderPrice || orderPrice <= 0) {
                throw new Error(`No valid market price available for ${resolvedSymbol}. Ensure the symbol is correct and the broker supports it.`);
              }
            }
          } else if (orderPrice) {
            // For limit orders with price, validate symbol exists via active or real adapter
            let symbolValid = false;
            try { await activeAdapter.fetchTicker(resolvedSymbol); symbolValid = true; } catch { /* */ }
            if (!symbolValid) {
              const realAdapter = this.getMarketAdapter();
              if (realAdapter && realAdapter !== activeAdapter) {
                try { await realAdapter.fetchTicker(resolvedSymbol); symbolValid = true; } catch { /* */ }
              }
            }
            if (!symbolValid) {
              console.log(`[BrokerMCPBridge] Resolving symbol "${resolvedSymbol}" for limit order...`);
              try {
                const resolved = await this.resolveSymbol(resolvedSymbol);
                resolvedSymbol = resolved.symbol;
                console.log(`[BrokerMCPBridge] Resolved "${params.symbol}" → "${resolvedSymbol}"`);
              } catch {
                // Keep original symbol, let the broker reject if invalid
              }
            }
          }

          // Validate balance before placing order
          const orderAmount = parseFloat(params.amount || params.quantity);
          if (!orderAmount || isNaN(orderAmount) || orderAmount <= 0) {
            throw new Error('Invalid order amount. Must be a positive number.');
          }

          const effectivePrice = orderPrice || params.price || 0;
          if (params.side === 'buy' && effectivePrice > 0) {
            try {
              const balance = await activeAdapter.fetchBalance();
              const freeUSD = balance.free?.USD || balance.free?.USDT || 0;
              const orderCost = orderAmount * effectivePrice;

              if (orderCost > freeUSD) {
                throw new Error(
                  `Insufficient balance. Order cost: $${orderCost.toFixed(2)}, Available: $${freeUSD.toFixed(2)}`
                );
              }
              console.log(`[BrokerMCPBridge] Balance check passed: cost=$${orderCost.toFixed(2)}, available=$${freeUSD.toFixed(2)}`);
            } catch (balanceError: any) {
              if (balanceError.message.includes('Insufficient balance')) {
                throw balanceError;
              }
              // Non-fatal: if balance fetch fails, let the broker reject it
              console.warn(`[BrokerMCPBridge] Balance check skipped:`, balanceError.message);
            }
          }

          const order = await activeAdapter.createOrder(
            resolvedSymbol,
            params.order_type || 'market',
            params.side,
            orderAmount,
            orderPrice,
            {
              stopPrice: params.stop_price,
              ...params
            }
          );

          console.log(`[BrokerMCPBridge] Order executed:`, {
            orderId: order.id,
            symbol: order.symbol,
            side: order.side,
            type: order.type,
            amount: order.amount,
            price: order.price,
            status: order.status
          });

          return {
            success: true,
            orderId: order.id,
            symbol: order.symbol || resolvedSymbol,
            resolvedFrom: params.symbol !== resolvedSymbol ? params.symbol : undefined,
            side: order.side,
            type: order.type,
            amount: order.amount,
            price: order.price,
            status: order.status,
            timestamp: order.timestamp,
          };
        } catch (error: any) {
          throw new Error(`Failed to place order: ${error.message}`);
        }
      },

      // Cancel order
      cancelOrder: async (orderId: string) => {
        try {
          // Try to extract symbol from order ID or fetch open orders to find it
          const openOrders = await activeAdapter.fetchOpenOrders();
          const order = openOrders.find(o => o.id === orderId);

          if (!order) {
            throw new Error(`Order ${orderId} not found`);
          }

          const cancelled = await activeAdapter.cancelOrder(orderId, order.symbol);
          return {
            success: true,
            orderId: cancelled.id,
            status: cancelled.status,
          };
        } catch (error: any) {
          throw new Error(`Failed to cancel order: ${error.message}`);
        }
      },

      // Modify order (cancel + replace)
      modifyOrder: async (orderId: string, updates: any) => {
        try {
          const openOrders = await activeAdapter.fetchOpenOrders();
          const order = openOrders.find(o => o.id === orderId);

          if (!order) {
            throw new Error(`Order ${orderId} not found`);
          }

          // Cancel existing order
          await activeAdapter.cancelOrder(orderId, order.symbol);

          // Place new order with updates
          const newOrder = await activeAdapter.createOrder(
            order.symbol,
            order.type as any,
            order.side as any,
            updates.quantity || order.amount,
            updates.price || order.price
          );

          return {
            success: true,
            oldOrderId: orderId,
            newOrderId: newOrder.id,
            newOrder,
          };
        } catch (error: any) {
          throw new Error(`Failed to modify order: ${error.message}`);
        }
      },

      // Get positions
      getPositions: async () => {
        try {
          const positions = await activeAdapter.fetchPositions();
          return positions.map(p => ({
            symbol: p.symbol,
            side: p.side,
            amount: p.contracts || p.contractSize || 0,
            entryPrice: p.entryPrice,
            markPrice: p.markPrice,
            unrealizedPnl: p.unrealizedPnl,
            leverage: p.leverage,
            marginType: p.marginMode,
            liquidationPrice: p.liquidationPrice,
          }));
        } catch (error: any) {
          console.error('[BrokerMCPBridge] Failed to fetch positions:', error);
          return [];
        }
      },

      // Get balance
      getBalance: async () => {
        try {
          const balance = await activeAdapter.fetchBalance();
          return balance;
        } catch (error: any) {
          throw new Error(`Failed to fetch balance: ${error.message}`);
        }
      },

      // Get orders
      getOrders: async (filters?: any) => {
        try {
          let orders: any[] = [];

          if (!filters?.status || filters.status === 'open' || filters.status === 'all') {
            const openOrders = await activeAdapter.fetchOpenOrders(filters?.symbol);
            orders.push(...openOrders);
          }

          // Try to fetch closed orders if adapter supports it
          if (!filters?.status || filters.status === 'closed' || filters.status === 'all') {
            try {
              if ('fetchClosedOrders' in activeAdapter && typeof activeAdapter.fetchClosedOrders === 'function') {
                const closedOrders = await activeAdapter.fetchClosedOrders(filters?.symbol, filters?.limit || 100);
                orders.push(...closedOrders);
              }
            } catch (err) {
              console.warn('[BrokerMCPBridge] Closed orders not supported by adapter');
            }
          }

          return orders.map(o => ({
            id: o.id,
            symbol: o.symbol,
            type: o.type,
            side: o.side,
            amount: o.amount,
            price: o.price,
            status: o.status,
            timestamp: o.timestamp,
            filled: o.filled,
            remaining: o.remaining,
            cost: o.cost,
            fee: o.fee,
          }));
        } catch (error: any) {
          throw new Error(`Failed to fetch orders: ${error.message}`);
        }
      },

      // Get holdings (balance formatted as holdings)
      getHoldings: async () => {
        try {
          const balance = await activeAdapter.fetchBalance();
          const holdings = Object.entries(balance.total || {})
            .filter(([_, amount]) => (amount as number) > 0)
            .map(([currency, amount]) => ({
              currency,
              amount: amount as number,
              free: balance.free?.[currency] || 0,
              used: balance.used?.[currency] || 0,
            }));
          return holdings;
        } catch (error: any) {
          throw new Error(`Failed to fetch holdings: ${error.message}`);
        }
      },

      // Get quote/ticker for a symbol
      getQuote: async (symbol: string) => {
        try {
          // Try direct fetch first
          try {
            const ticker = await activeAdapter.fetchTicker(symbol);
            return ticker;
          } catch {
            // Try resolving the symbol
            const resolved = await this.resolveSymbol(symbol);
            const ticker = await activeAdapter.fetchTicker(resolved.symbol);
            return { ...ticker, resolvedFrom: symbol, resolvedTo: resolved.symbol };
          }
        } catch (error: any) {
          throw new Error(`Failed to get ticker for ${symbol}: ${error.message}`);
        }
      },

      // Resolve symbol - fuzzy match crypto names/tickers to valid trading pairs
      resolveSymbol: async (query: string) => {
        return this.resolveSymbol(query);
      },

      // Search symbol - search ALL available markets dynamically
      searchSymbol: async (query: string) => {
        const markets = await this.getMarkets();
        if (markets.length === 0) {
          // Fallback: try resolve
          try {
            const resolved = await this.resolveSymbol(query);
            return resolved.matches.map((s: string) => ({ symbol: s }));
          } catch {
            return [];
          }
        }

        const searchLower = query.trim().toLowerCase();
        const index = this.marketsIndex || new Map();

        // 1. Exact index lookup first
        const exactHits = index.get(searchLower);
        if (exactHits && exactHits.length > 0) {
          const sorted = this.sortByQuotePreference(exactHits);
          return sorted.slice(0, 20).map(sym => {
            const m = markets.find((mk: any) => mk.symbol === sym);
            return { symbol: sym, base: m?.base, quote: m?.quote, active: m?.active ?? true };
          });
        }

        // 2. Substring search across all keys in the index
        const matchedSymbols: string[] = [];
        for (const [key, symbols] of index) {
          if (key.includes(searchLower) || searchLower.includes(key)) {
            for (const s of symbols) {
              if (!matchedSymbols.includes(s)) matchedSymbols.push(s);
            }
          }
        }

        // 3. Also search raw market fields (catches any fields not indexed)
        for (const m of markets) {
          if (!m.symbol || !m.active) continue;
          if (matchedSymbols.includes(m.symbol)) continue;
          const sym = (m.symbol as string).toLowerCase();
          const base = (m.base || '').toLowerCase();
          const baseId = (m.baseId || '').toLowerCase();
          const altname = (m.info?.altname || '').toLowerCase();
          const name = (m.info?.name || '').toLowerCase();
          if (
            sym.includes(searchLower) || base.includes(searchLower) ||
            baseId.includes(searchLower) || altname.includes(searchLower) ||
            name.includes(searchLower)
          ) {
            matchedSymbols.push(m.symbol);
          }
        }

        const sorted = this.sortByQuotePreference(matchedSymbols);
        return sorted.slice(0, 20).map(sym => {
          const m = markets.find((mk: any) => mk.symbol === sym);
          return { symbol: sym, base: m?.base, quote: m?.quote, active: m?.active ?? true };
        });
      },

      // Close position
      closePosition: async (symbol: string) => {
        try {
          const positions = await activeAdapter.fetchPositions([symbol]);
          const position = positions.find(p => p.symbol === symbol);

          if (!position) {
            throw new Error(`No open position for ${symbol}`);
          }

          // Determine close side (opposite of position side)
          const closeSide = position.side === 'long' ? 'sell' : 'buy';
          const amount = Math.abs(position.contracts || position.contractSize || 0);

          const closeOrder = await activeAdapter.createOrder(
            symbol,
            'market',
            closeSide as any,
            amount
          );

          return {
            success: true,
            symbol,
            closedPosition: position,
            closeOrder,
          };
        } catch (error: any) {
          throw new Error(`Failed to close position: ${error.message}`);
        }
      },
    });

    console.log(`[BrokerMCPBridge] Connected to ${activeBroker} (${tradingMode} mode)`);
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return this.config !== null && this.config.activeAdapter !== null;
  }

  /**
   * Get current configuration
   */
  getConfig(): BrokerMCPBridgeConfig | null {
    return this.config;
  }
}

// Singleton instance
export const brokerMCPBridge = new BrokerMCPBridge();
