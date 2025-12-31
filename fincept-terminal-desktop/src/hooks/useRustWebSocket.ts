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

// ============================================================================
// TICKER HOOK
// ============================================================================

export function useRustTicker(provider: string, symbol: string, autoConnect = true) {
  const [data, setData] = useState<TickerData | null>(null);
  const [error, setError] = useState<string | null>(null);
  const subscribed = useRef(false);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Subscribe to ticker channel
        if (!subscribed.current) {
          await websocketBridge.subscribe(provider, symbol, 'ticker');
          subscribed.current = true;
        }

        // Listen to ticker events
        const unlisten = await websocketBridge.onTicker((tickerData) => {
          // Normalize symbols for comparison (both with and without slashes)
          const normalizedInputSymbol = symbol.replace('/', '').toUpperCase();
          const normalizedReceivedSymbol = tickerData.symbol.replace('/', '').toUpperCase();

          console.log('[useRustTicker] Received ticker:', {
            provider: tickerData.provider,
            symbol: tickerData.symbol,
            expectedProvider: provider,
            expectedSymbol: symbol,
            normalizedInput: normalizedInputSymbol,
            normalizedReceived: normalizedReceivedSymbol,
            matches: normalizedReceivedSymbol === normalizedInputSymbol && tickerData.provider === provider
          });

          // Only process if mounted and matches our symbol/provider
          if (!mounted) return;
          if (tickerData.provider !== provider) return;
          if (normalizedReceivedSymbol !== normalizedInputSymbol) return;

          console.log('[useRustTicker] ✅ Setting ticker data:', tickerData);
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
      if (subscribed.current) {
        websocketBridge.unsubscribe(provider, symbol, 'ticker').catch(() => {});
        subscribed.current = false;
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
  const subscribed = useRef(false);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Subscribe to book channel with depth parameter
        if (!subscribed.current) {
          await websocketBridge.subscribe(provider, symbol, 'book', { depth });
          subscribed.current = true;
        }

        // Listen to orderbook events
        const unlisten = await websocketBridge.onOrderBook((bookData) => {
          // Normalize symbols for comparison (both with and without slashes)
          const normalizedInputSymbol = symbol.replace('/', '').toUpperCase();
          const normalizedReceivedSymbol = bookData.symbol.replace('/', '').toUpperCase();

          console.log('[useRustOrderBook] Received orderbook:', {
            provider: bookData.provider,
            symbol: bookData.symbol,
            expectedProvider: provider,
            expectedSymbol: symbol,
            normalizedInput: normalizedInputSymbol,
            normalizedReceived: normalizedReceivedSymbol,
            bidsCount: bookData.bids.length,
            asksCount: bookData.asks.length,
            matches: normalizedReceivedSymbol === normalizedInputSymbol && bookData.provider === provider
          });

          if (!mounted) return;
          if (bookData.provider !== provider) return;
          if (normalizedReceivedSymbol !== normalizedInputSymbol) return;

          console.log('[useRustOrderBook] ✅ Setting orderbook data:', bookData);
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
      if (subscribed.current) {
        websocketBridge.unsubscribe(provider, symbol, 'book').catch(() => {});
        subscribed.current = false;
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
  const subscribed = useRef(false);
  const listenerRef = useRef<(() => void) | null>(null);

  useEffect(() => {
    let mounted = true;

    const subscribe = async () => {
      try {
        // Clean up old listener if it exists
        if (listenerRef.current) {
          listenerRef.current();
          listenerRef.current = null;
        }

        // Subscribe to trade channel
        if (!subscribed.current) {
          await websocketBridge.subscribe(provider, symbol, 'trade');
          subscribed.current = true;
        }

        // Listen to trade events
        const unlisten = await websocketBridge.onTrade((tradeData) => {
          // Normalize symbols for comparison (both with and without slashes)
          const normalizedInputSymbol = symbol.replace('/', '').toUpperCase();
          const normalizedReceivedSymbol = tradeData.symbol.replace('/', '').toUpperCase();

          if (!mounted) return;
          if (tradeData.provider !== provider) return;
          if (normalizedReceivedSymbol !== normalizedInputSymbol) return;

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
      if (subscribed.current) {
        websocketBridge.unsubscribe(provider, symbol, 'trade').catch(() => {});
        subscribed.current = false;
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
  const subscribed = useRef(false);

  useEffect(() => {
    let mounted = true;
    let unlisten: (() => void) | null = null;

    const subscribe = async () => {
      try {
        // Subscribe to candle channel
        if (!subscribed.current) {
          await websocketBridge.subscribe(provider, symbol, 'ohlc', { interval });
          subscribed.current = true;
        }

        // Listen to candle events
        unlisten = await websocketBridge.onCandle((candleData) => {
          if (
            mounted &&
            candleData.provider === provider &&
            candleData.symbol === symbol &&
            candleData.interval === interval
          ) {
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
          }
        });
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
      if (unlisten) {
        unlisten();
      }
      if (subscribed.current) {
        websocketBridge.unsubscribe(provider, symbol, 'ohlc').catch(() => {});
        subscribed.current = false;
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
  const subscribed = useRef<Set<string>>(new Set());

  useEffect(() => {
    let mounted = true;
    let unlisten: (() => void) | null = null;

    const subscribe = async () => {
      try {
        // Subscribe to all providers
        for (const provider of providers) {
          if (!subscribed.current.has(provider)) {
            await websocketBridge.subscribe(provider, symbol, 'ticker');
            subscribed.current.add(provider);
          }
        }

        // Listen to ticker events from all providers
        unlisten = await websocketBridge.onTicker((tickerData) => {
          if (mounted && providers.includes(tickerData.provider) && tickerData.symbol === symbol) {
            setPrices((prev) => {
              const updated = new Map(prev);
              updated.set(tickerData.provider, tickerData);
              return updated;
            });
          }
        });
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
      if (unlisten) {
        unlisten();
      }
      for (const provider of subscribed.current) {
        websocketBridge.unsubscribe(provider, symbol, 'ticker').catch(() => {});
      }
      subscribed.current.clear();
    };
  }, [providers.join(','), symbol]);

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
