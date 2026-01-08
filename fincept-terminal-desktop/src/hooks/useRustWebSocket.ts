// React hooks for Rust WebSocket system

import { useEffect, useState, useCallback, useRef } from 'react';
import {
  websocketBridge,
  TickerData,
  OrderBookData,
  TradeData,
  CandleData,
  StatusData,
  ConnectionMetrics,
} from '@/services/websocketBridge';

/**
 * Normalize a symbol for comparison (removes slashes, dashes, underscores, uppercase)
 */
function normalizeSymbol(symbol: string): string {
  return symbol.replace(/[/\-_]/g, '').toUpperCase();
}

// ============================================================================
// TICKER HOOK
// ============================================================================

export function useRustTicker(provider: string, symbol: string, autoConnect = true) {
  const [data, setData] = useState<TickerData | null>(null);
  const [error, setError] = useState<string | null>(null);
  // Track subscription state with the actual provider/symbol we subscribed to
  const subscriptionRef = useRef<{ provider: string; symbol: string } | null>(null);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;
    const normalizedInputSymbol = normalizeSymbol(symbol);

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Unsubscribe from old subscription if provider/symbol changed
        if (subscriptionRef.current &&
            (subscriptionRef.current.provider !== provider ||
             subscriptionRef.current.symbol !== symbol)) {
          await websocketBridge.unsubscribe(
            subscriptionRef.current.provider,
            subscriptionRef.current.symbol,
            'ticker'
          ).catch(() => {});
          subscriptionRef.current = null;
        }

        // Subscribe to ticker channel
        if (!subscriptionRef.current) {
          await websocketBridge.subscribe(provider, symbol, 'ticker');
          subscriptionRef.current = { provider, symbol };
        }

        // Listen to ticker events
        const unlisten = await websocketBridge.onTicker((tickerData) => {
          // Only process if mounted and matches our symbol/provider
          if (!mounted) return;
          if (tickerData.provider !== provider) return;
          if (normalizeSymbol(tickerData.symbol) !== normalizedInputSymbol) return;

          setData(tickerData);
        });

        listenerRef.current = unlisten;
      } catch (err) {
        if (mounted) {
          setError(err instanceof Error ? err.message : String(err));
        }
      }
    };

    if (autoConnect && provider && symbol) {
      subscribe();
    }

    return () => {
      mounted = false;
      if (listenerRef.current) {
        listenerRef.current();
        listenerRef.current = null;
      }
      if (subscriptionRef.current) {
        websocketBridge.unsubscribe(
          subscriptionRef.current.provider,
          subscriptionRef.current.symbol,
          'ticker'
        ).catch(() => {});
        subscriptionRef.current = null;
      }
    };
  }, [provider, symbol, autoConnect]);

  return { data, error };
}

// ============================================================================
// ORDER BOOK HOOK
// ============================================================================

export function useRustOrderBook(
  provider: string,
  symbol: string,
  depth: number = 25,
  autoConnect = true
) {
  const [data, setData] = useState<OrderBookData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const subscriptionRef = useRef<{ provider: string; symbol: string } | null>(null);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;
    const normalizedInputSymbol = normalizeSymbol(symbol);

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Unsubscribe from old subscription if provider/symbol changed
        if (subscriptionRef.current &&
            (subscriptionRef.current.provider !== provider ||
             subscriptionRef.current.symbol !== symbol)) {
          await websocketBridge.unsubscribe(
            subscriptionRef.current.provider,
            subscriptionRef.current.symbol,
            'book'
          ).catch(() => {});
          subscriptionRef.current = null;
        }

        // Subscribe to book channel with depth parameter
        if (!subscriptionRef.current) {
          await websocketBridge.subscribe(provider, symbol, 'book', { depth });
          subscriptionRef.current = { provider, symbol };
        }

        // Listen to orderbook events
        const unlisten = await websocketBridge.onOrderBook((bookData) => {
          if (!mounted) return;
          if (bookData.provider !== provider) return;
          if (normalizeSymbol(bookData.symbol) !== normalizedInputSymbol) return;

          setData(bookData);
        });

        listenerRef.current = unlisten;
      } catch (err) {
        if (mounted) {
          setError(err instanceof Error ? err.message : String(err));
        }
      }
    };

    if (autoConnect && provider && symbol) {
      subscribe();
    }

    return () => {
      mounted = false;
      if (listenerRef.current) {
        listenerRef.current();
        listenerRef.current = null;
      }
      if (subscriptionRef.current) {
        websocketBridge.unsubscribe(
          subscriptionRef.current.provider,
          subscriptionRef.current.symbol,
          'book'
        ).catch(() => {});
        subscriptionRef.current = null;
      }
    };
  }, [provider, symbol, depth, autoConnect]);

  return { data, error };
}

// ============================================================================
// TRADES HOOK
// ============================================================================

export function useRustTrades(provider: string, symbol: string, maxTrades = 50, autoConnect = true) {
  const [trades, setTrades] = useState<TradeData[]>([]);
  const [error, setError] = useState<string | null>(null);
  const subscriptionRef = useRef<{ provider: string; symbol: string } | null>(null);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;
    const normalizedInputSymbol = normalizeSymbol(symbol);

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Unsubscribe from old subscription if provider/symbol changed
        if (subscriptionRef.current &&
            (subscriptionRef.current.provider !== provider ||
             subscriptionRef.current.symbol !== symbol)) {
          await websocketBridge.unsubscribe(
            subscriptionRef.current.provider,
            subscriptionRef.current.symbol,
            'trade'
          ).catch(() => {});
          subscriptionRef.current = null;
          // Clear old trades when symbol changes
          setTrades([]);
        }

        // Subscribe to trade channel
        if (!subscriptionRef.current) {
          await websocketBridge.subscribe(provider, symbol, 'trade');
          subscriptionRef.current = { provider, symbol };
        }

        // Listen to trade events
        const unlisten = await websocketBridge.onTrade((tradeData) => {
          if (!mounted) return;
          if (tradeData.provider !== provider) return;
          if (normalizeSymbol(tradeData.symbol) !== normalizedInputSymbol) return;

          setTrades((prev) => {
            const updated = [tradeData, ...prev];
            return updated.slice(0, maxTrades);
          });
        });

        listenerRef.current = unlisten;
      } catch (err) {
        if (mounted) {
          setError(err instanceof Error ? err.message : String(err));
        }
      }
    };

    if (autoConnect && provider && symbol) {
      subscribe();
    }

    return () => {
      mounted = false;
      if (listenerRef.current) {
        listenerRef.current();
        listenerRef.current = null;
      }
      if (subscriptionRef.current) {
        websocketBridge.unsubscribe(
          subscriptionRef.current.provider,
          subscriptionRef.current.symbol,
          'trade'
        ).catch(() => {});
        subscriptionRef.current = null;
      }
    };
  }, [provider, symbol, maxTrades, autoConnect]);

  return { trades, error };
}

// ============================================================================
// CANDLE/OHLC HOOK
// ============================================================================

export function useRustCandles(
  provider: string,
  symbol: string,
  interval: string = '1m',
  autoConnect = true
) {
  const [candles, setCandles] = useState<CandleData[]>([]);
  const [latestCandle, setLatestCandle] = useState<CandleData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const subscriptionRef = useRef<{ provider: string; symbol: string; interval: string } | null>(null);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;
    const normalizedInputSymbol = normalizeSymbol(symbol);

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Unsubscribe from old subscription if provider/symbol/interval changed
        if (subscriptionRef.current &&
            (subscriptionRef.current.provider !== provider ||
             subscriptionRef.current.symbol !== symbol ||
             subscriptionRef.current.interval !== interval)) {
          await websocketBridge.unsubscribe(
            subscriptionRef.current.provider,
            subscriptionRef.current.symbol,
            'ohlc'
          ).catch(() => {});
          subscriptionRef.current = null;
          // Clear old candles when symbol/interval changes
          setCandles([]);
          setLatestCandle(null);
        }

        // Subscribe to candle channel
        if (!subscriptionRef.current) {
          await websocketBridge.subscribe(provider, symbol, 'ohlc', { interval });
          subscriptionRef.current = { provider, symbol, interval };
        }

        // Listen to candle events
        const unlisten = await websocketBridge.onCandle((candleData) => {
          if (!mounted) return;
          if (candleData.provider !== provider) return;
          // Use normalized symbol comparison (fixes the inconsistency)
          if (normalizeSymbol(candleData.symbol) !== normalizedInputSymbol) return;
          if (candleData.interval !== interval) return;

          setLatestCandle(candleData);
          setCandles((prev) => {
            // Update existing candle or add new one
            const existingIndex = prev.findIndex((c) => c.timestamp === candleData.timestamp);
            if (existingIndex >= 0) {
              const updated = [...prev];
              updated[existingIndex] = candleData;
              return updated;
            } else {
              return [...prev, candleData].sort((a, b) => a.timestamp - b.timestamp);
            }
          });
        });

        listenerRef.current = unlisten;
      } catch (err) {
        if (mounted) {
          setError(err instanceof Error ? err.message : String(err));
        }
      }
    };

    if (autoConnect && provider && symbol) {
      subscribe();
    }

    return () => {
      mounted = false;
      if (listenerRef.current) {
        listenerRef.current();
        listenerRef.current = null;
      }
      if (subscriptionRef.current) {
        websocketBridge.unsubscribe(
          subscriptionRef.current.provider,
          subscriptionRef.current.symbol,
          'ohlc'
        ).catch(() => {});
        subscriptionRef.current = null;
      }
    };
  }, [provider, symbol, interval, autoConnect]);

  return { candles, latestCandle, error };
}

// ============================================================================
// CONNECTION STATUS HOOK
// ============================================================================

export function useConnectionStatus(provider: string) {
  const [status, setStatus] = useState<StatusData | null>(null);
  const [metrics, setMetrics] = useState<ConnectionMetrics | null>(null);

  useEffect(() => {
    let mounted = true;
    let unlisten: (() => void) | null = null;
    let intervalId: NodeJS.Timeout | null = null;

    const setup = async () => {
      // Listen to status events
      unlisten = await websocketBridge.onStatus((statusData) => {
        if (mounted && statusData.provider === provider) {
          setStatus(statusData);
        }
      });

      // Poll metrics every 5 seconds
      const fetchMetrics = async () => {
        if (mounted) {
          try {
            const m = await websocketBridge.getMetrics(provider);
            if (m) {
              setMetrics(m);
            }
          } catch (err) {
            // Ignore metrics fetch errors
          }
        }
      };

      await fetchMetrics();
      intervalId = setInterval(fetchMetrics, 5000);
    };

    if (provider) {
      setup();
    }

    return () => {
      mounted = false;
      if (unlisten) {
        unlisten();
      }
      if (intervalId) {
        clearInterval(intervalId);
      }
    };
  }, [provider]);

  return { status, metrics };
}

// ============================================================================
// MULTI-PROVIDER HOOK (for arbitrage, etc.)
// ============================================================================

export function useMultiProviderTicker(providers: string[], symbol: string) {
  const [prices, setPrices] = useState<Map<string, TickerData>>(new Map());
  const [error, setError] = useState<string | null>(null);
  // Track subscriptions with their actual provider/symbol
  const subscriptionsRef = useRef<Map<string, { provider: string; symbol: string }>>(new Map());
  const listenerRef = useRef<(() => void) | null>(null);

  // Memoize providers string to prevent unnecessary re-renders
  const providersKey = providers.join(',');

  useEffect(() => {
    let mounted = true;
    const normalizedInputSymbol = normalizeSymbol(symbol);

    const subscribe = async () => {
      try {
        // Clean up old listener
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Unsubscribe from providers no longer in the list
        for (const [key, sub] of subscriptionsRef.current.entries()) {
          if (!providers.includes(sub.provider) || sub.symbol !== symbol) {
            await websocketBridge.unsubscribe(sub.provider, sub.symbol, 'ticker').catch(() => {});
            subscriptionsRef.current.delete(key);
          }
        }

        // Subscribe to new providers
        for (const provider of providers) {
          const key = `${provider}:${symbol}`;
          if (!subscriptionsRef.current.has(key)) {
            await websocketBridge.subscribe(provider, symbol, 'ticker');
            subscriptionsRef.current.set(key, { provider, symbol });
          }
        }

        // Listen to ticker events from all providers
        const unlisten = await websocketBridge.onTicker((tickerData) => {
          if (!mounted) return;
          if (!providers.includes(tickerData.provider)) return;
          if (normalizeSymbol(tickerData.symbol) !== normalizedInputSymbol) return;

          setPrices((prev) => {
            const updated = new Map(prev);
            updated.set(tickerData.provider, tickerData);
            return updated;
          });
        });

        listenerRef.current = unlisten;
      } catch (err) {
        if (mounted) {
          setError(err instanceof Error ? err.message : String(err));
        }
      }
    };

    if (providers.length > 0 && symbol) {
      subscribe();
    }

    return () => {
      mounted = false;
      if (listenerRef.current) {
        listenerRef.current();
        listenerRef.current = null;
      }
      // Unsubscribe from all on cleanup
      for (const sub of subscriptionsRef.current.values()) {
        websocketBridge.unsubscribe(sub.provider, sub.symbol, 'ticker').catch(() => {});
      }
      subscriptionsRef.current.clear();
      setPrices(new Map());
    };
  }, [providersKey, symbol]);

  // Calculate arbitrage spread
  const spread = useCallback(() => {
    if (prices.size < 2) return null;

    const priceValues = Array.from(prices.values()).map((t) => t.price);
    const min = Math.min(...priceValues);
    const max = Math.max(...priceValues);
    const spreadPercent = ((max - min) / min) * 100;

    return { min, max, spreadPercent };
  }, [prices]);

  return { prices, spread: spread(), error };
}
