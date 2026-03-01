/**
 * Equity Trading Tab - Fincept Terminal Style
 *
 * Professional stock/equity trading interface matching the crypto trading tab design.
 * Supports multiple brokers across different regions with real-time market data.
 *
 * This file is the thin orchestration layer. UI is split into:
 * - EquityTopNav       — Top navigation bar
 * - EquityTickerBar    — Price ticker with symbol search
 * - EquityWatchlist    — Left sidebar (watchlist/indices/sectors)
 * - EquityChartArea    — Center chart/depth/trades panel
 * - EquityBottomPanel  — Bottom positions/holdings/orders/stats panel
 * - EquityOrderEntry   — Right sidebar order entry
 * - EquityOrderBook    — Right sidebar market depth / order book
 * - EquityStatusBar    — Bottom status bar
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';

import { StockBrokerProvider, useStockBrokerContext, useStockTradingMode, useStockTradingData } from '@/contexts/StockBrokerContext';
import type { SymbolSearchResult } from '@/services/trading/masterContractService';
import type { StockExchange, Quote, MarketDepth, TimeFrame } from '@/brokers/stocks/types';

// Market hours and caching utilities
import { getMarketStatus, getPollingInterval, getCacheTTL, isMarketOpen } from '@/services/markets/marketHoursService';
import cacheService from '@/services/cache/cacheService';

// Constants
import { DEFAULT_WATCHLIST, getCurrencyFromSymbol, formatCurrency, EQUITY_TERMINAL_STYLES } from './constants';

// Child components
import { EquityTopNav } from './components/EquityTopNav';
import { EquityTickerBar } from './components/EquityTickerBar';
import { EquityWatchlist } from './components/EquityWatchlist';
import { EquityChartArea } from './components/EquityChartArea';
import { EquityBottomPanel } from './components/EquityBottomPanel';
import { EquityOrderEntry } from './components/EquityOrderEntry';
import { EquityOrderBook } from './components/EquityOrderBook';
import { EquityStatusBar } from './components/EquityStatusBar';

// Types
import type { BottomPanelTab, LeftSidebarView, CenterView, RightPanelView } from './types';

// Fincept color palette (for CSS in style tag)
const FINCEPT_DARK_BG = '#000000';
const FINCEPT_ORANGE = '#FF8800';

// Main Tab Content (wrapped in provider)
function EquityTradingContent() {
  const {
    activeBroker,
    activeBrokerMetadata,
    isAuthenticated,
    adapter,
    defaultSymbols,
    isLoading,
    isConnecting,
  } = useStockBrokerContext();
  const { mode: tradingMode, setMode: setTradingMode, isLive, isPaper } = useStockTradingMode();
  const { positions, holdings, orders, trades, funds, refreshAll, isRefreshing } = useStockTradingData();

  // Time state
  const [currentTime, setCurrentTime] = useState(new Date());

  // Symbol state
  const [selectedSymbol, setSelectedSymbol] = useState<string>('');
  const [selectedExchange, setSelectedExchange] = useState<StockExchange>('NSE');

  // Quote data
  const [quote, setQuote] = useState<Quote | null>(null);
  const [marketDepth, setMarketDepth] = useState<MarketDepth | null>(null);
  const [isLoadingQuote, setIsLoadingQuote] = useState(false);

  // Watchlist
  const [watchlist, setWatchlist] = useState<string[]>(DEFAULT_WATCHLIST);
  const [watchlistQuotes, setWatchlistQuotes] = useState<Record<string, Quote>>({});
  const [useWebSocketForWatchlist, setUseWebSocketForWatchlist] = useState<boolean>(false);

  // UI State
  const [leftSidebarView, setLeftSidebarView] = useState<LeftSidebarView>('watchlist');
  const [centerView, setCenterView] = useState<CenterView>('chart');
  const [rightPanelView, setRightPanelView] = useState<RightPanelView>('orderbook');
  const [activeBottomTab, setActiveBottomTab] = useState<BottomPanelTab>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);
  const [showSettings, setShowSettings] = useState(false);

  // Chart timeframe
  const [chartInterval, setChartInterval] = useState<TimeFrame>('5m');

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    selectedSymbol,
    selectedExchange,
    leftSidebarView,
    centerView,
    rightPanelView,
    activeBottomTab,
    chartInterval,
  }), [selectedSymbol, selectedExchange, leftSidebarView, centerView, rightPanelView, activeBottomTab, chartInterval]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.selectedSymbol === 'string') setSelectedSymbol(state.selectedSymbol);
    if (typeof state.selectedExchange === 'string') setSelectedExchange(state.selectedExchange as StockExchange);
    if (typeof state.leftSidebarView === 'string') setLeftSidebarView(state.leftSidebarView as LeftSidebarView);
    if (typeof state.centerView === 'string') setCenterView(state.centerView as CenterView);
    if (typeof state.rightPanelView === 'string') setRightPanelView(state.rightPanelView as RightPanelView);
    if (typeof state.activeBottomTab === 'string') setActiveBottomTab(state.activeBottomTab as BottomPanelTab);
    if (typeof state.chartInterval === 'string') setChartInterval(state.chartInterval as TimeFrame);
  }, []);

  useWorkspaceTabState('equity-trading', getWorkspaceState, setWorkspaceState);

  // Set correct chart interval when broker changes
  useEffect(() => {
    if (activeBroker) {
      const defaultInterval = activeBroker === 'yfinance' ? '1d' : '5m';
      setChartInterval(defaultInterval);
      console.log(`[EquityTrading] Setting chart interval to ${defaultInterval} for broker ${activeBroker}`);

      if (activeBroker === 'yfinance') {
        setRightPanelView('info');
        setCenterView('chart');
      }
    }
  }, [activeBroker]);

  // Filter trades for the selected symbol (memoized)
  const symbolTrades = useMemo(() => {
    if (!selectedSymbol || !trades || trades.length === 0) return [];
    return trades
      .filter(t => t.symbol === selectedSymbol)
      .sort((a, b) => new Date(b.tradedAt).getTime() - new Date(a.tradedAt).getTime())
      .slice(0, 50);
  }, [trades, selectedSymbol]);

  // Get currency based on selected symbol and exchange
  const currentCurrency = useMemo(() => {
    return getCurrencyFromSymbol(selectedSymbol, selectedExchange);
  }, [selectedSymbol, selectedExchange]);

  // Currency formatter for current symbol
  const fmtPrice = useCallback((value: number, compact = false) => {
    return formatCurrency(value, currentCurrency, compact);
  }, [currentCurrency]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Auto-toggle WebSocket based on market hours AND broker support
  useEffect(() => {
    const checkMarketStatus = () => {
      const brokerSupportsWs = activeBrokerMetadata?.features?.webSocket ?? false;
      const marketOpen = isMarketOpen(selectedExchange);
      const shouldUseWs = brokerSupportsWs && marketOpen;

      setUseWebSocketForWatchlist(prev => {
        if (prev !== shouldUseWs) {
          if (!brokerSupportsWs) {
            console.log(`[EquityTrading] Broker doesn't support WebSocket - using REST polling`);
          } else {
            console.log(`[EquityTrading] Market ${marketOpen ? 'OPEN' : 'CLOSED'} - WebSocket ${shouldUseWs ? 'enabled' : 'disabled'}`);
          }
        }
        return shouldUseWs;
      });
    };

    checkMarketStatus();
    const interval = setInterval(checkMarketStatus, 60 * 1000);
    return () => clearInterval(interval);
  }, [selectedExchange, activeBrokerMetadata]);

  // Track previous broker to detect broker changes
  const prevBrokerRef = React.useRef<string | null>(null);

  // Set default symbol and exchange when broker changes
  useEffect(() => {
    const brokerChanged = prevBrokerRef.current !== null && prevBrokerRef.current !== activeBroker;
    prevBrokerRef.current = activeBroker;

    if (defaultSymbols.length > 0) {
      if (brokerChanged || !selectedSymbol) {
        setSelectedSymbol(defaultSymbols[0]);
        if (brokerChanged) {
          setQuote(null);
          setMarketDepth(null);
        }
      }

      if (activeBrokerMetadata && (brokerChanged || selectedExchange === 'NSE')) {
        const brokerRegion = activeBrokerMetadata.region;
        const brokerExchanges = activeBrokerMetadata.exchanges || [];

        if (brokerRegion === 'india') {
          setSelectedExchange('NSE');
        } else if (brokerRegion === 'us') {
          if (brokerExchanges.includes('NASDAQ')) {
            setSelectedExchange('NASDAQ');
          } else if (brokerExchanges.includes('NYSE')) {
            setSelectedExchange('NYSE');
          }
        } else if (brokerExchanges.includes('NASDAQ')) {
          setSelectedExchange('NASDAQ');
        } else if (brokerExchanges.includes('NYSE')) {
          setSelectedExchange('NYSE');
        } else if (brokerExchanges.includes('NSE')) {
          setSelectedExchange('NSE');
        } else if (brokerExchanges.length > 0) {
          setSelectedExchange(brokerExchanges[0] as StockExchange);
        }
      }

      setWatchlist(defaultSymbols.slice(0, 50));
    } else {
      setWatchlist(DEFAULT_WATCHLIST);
    }
  }, [defaultSymbols, activeBroker, activeBrokerMetadata]);

  // Subscribe to WebSocket for live prices and fetch initial data
  useEffect(() => {
    if (!adapter || !isAuthenticated || !selectedSymbol) {
      console.log('[EquityTrading] Skipping subscription - broker not authenticated or no symbol selected');
      return;
    }

    let isCancelled = false;

    const subscribeToSymbol = async () => {
      try {
        console.log(`[EquityTrading] Subscribing to ${selectedSymbol} via WebSocket...`);
        setIsLoadingQuote(true);

        try {
          const initialQuote = await adapter.getQuote(selectedSymbol, selectedExchange);
          if (!isCancelled && initialQuote) {
            setQuote(initialQuote);
            console.log(`[EquityTrading] Initial quote loaded for ${selectedSymbol}:`, {
              lastPrice: initialQuote.lastPrice,
              bid: initialQuote.bid,
              ask: initialQuote.ask,
            });
          }
        } catch (quoteErr) {
          console.warn(`[EquityTrading] Failed to fetch initial quote:`, quoteErr);
        }

        if (activeBroker !== 'yfinance') {
          try {
            const initialDepth = await adapter.getMarketDepth(selectedSymbol, selectedExchange);
            if (!isCancelled && initialDepth) {
              setMarketDepth(initialDepth);
              console.log(`[EquityTrading] Initial depth loaded for ${selectedSymbol}:`, {
                bids: initialDepth.bids?.length || 0,
                asks: initialDepth.asks?.length || 0,
              });
            }
          } catch (depthErr) {
            console.warn(`[EquityTrading] Failed to fetch initial depth:`, depthErr);
          }
        }

        setIsLoadingQuote(false);

        await adapter.subscribe(selectedSymbol, selectedExchange, 'full');

        const handleTick = (tick: any) => {
          if (tick.symbol === selectedSymbol && !isCancelled) {
            setQuote((prevQuote) => ({
              symbol: tick.symbol,
              exchange: tick.exchange,
              lastPrice: tick.lastPrice || tick.price || prevQuote?.lastPrice || 0,
              open: tick.open || prevQuote?.open || tick.lastPrice,
              high: tick.high || prevQuote?.high || tick.lastPrice,
              low: tick.low || prevQuote?.low || tick.lastPrice,
              close: tick.close || prevQuote?.close || tick.lastPrice,
              previousClose: tick.previousClose || prevQuote?.previousClose || tick.close || tick.lastPrice,
              volume: tick.volume ?? prevQuote?.volume ?? 0,
              change: tick.change ?? prevQuote?.change ?? 0,
              changePercent: tick.changePercent ?? prevQuote?.changePercent ?? 0,
              bid: tick.bid || prevQuote?.bid || 0,
              bidQty: tick.bidQty || prevQuote?.bidQty || 0,
              ask: tick.ask || prevQuote?.ask || 0,
              askQty: tick.askQty || prevQuote?.askQty || 0,
              timestamp: Date.now(),
            }));

            if (tick.depth && (tick.depth.bids?.length > 0 || tick.depth.asks?.length > 0)) {
              setMarketDepth({
                bids: tick.depth.bids || [],
                asks: tick.depth.asks || [],
              });
            }
          }
        };

        const handleDepth = (depth: any) => {
          if (depth.symbol === selectedSymbol && !isCancelled) {
            setMarketDepth({
              bids: depth.bids || [],
              asks: depth.asks || [],
            });

            const bestBid = depth.bids?.[0];
            const bestAsk = depth.asks?.[0];
            if (bestBid || bestAsk) {
              setQuote((prevQuote) => {
                if (!prevQuote) return prevQuote;
                return {
                  ...prevQuote,
                  bid: bestBid?.price || prevQuote.bid,
                  bidQty: bestBid?.quantity || prevQuote.bidQty,
                  ask: bestAsk?.price || prevQuote.ask,
                  askQty: bestAsk?.quantity || prevQuote.askQty,
                };
              });
            }
          }
        };

        adapter.onTick(handleTick);
        adapter.onDepth(handleDepth);
      } catch (err: any) {
        console.error('[EquityTrading] WebSocket subscription failed:', err);
        setIsLoadingQuote(false);
      }
    };

    subscribeToSymbol();

    return () => {
      isCancelled = true;
      if (adapter && selectedSymbol) {
        adapter.unsubscribe(selectedSymbol, selectedExchange).catch(console.error);
      }
    };
  }, [adapter, isAuthenticated, selectedSymbol, selectedExchange]);

  // Subscribe to watchlist via WebSocket for real-time updates
  useEffect(() => {
    if (!adapter || !isAuthenticated || !useWebSocketForWatchlist) {
      console.log('[EquityTrading] Skipping watchlist subscription - broker not authenticated or WS disabled');
      return;
    }

    let isCancelled = false;
    const subscribedSymbols: string[] = [];

    const subscribeWatchlist = async () => {
      console.log('[EquityTrading] Subscribing to watchlist via WebSocket...');

      const handleWatchlistTick = (tick: any) => {
        if (watchlist.includes(tick.symbol) && !isCancelled) {
          setWatchlistQuotes(prev => ({
            ...prev,
            [tick.symbol]: {
              symbol: tick.symbol,
              exchange: tick.exchange || selectedExchange,
              lastPrice: tick.lastPrice || tick.price,
              open: tick.open || prev[tick.symbol]?.open || 0,
              high: tick.high || prev[tick.symbol]?.high || tick.lastPrice,
              low: tick.low || prev[tick.symbol]?.low || tick.lastPrice,
              close: tick.close || prev[tick.symbol]?.close || tick.lastPrice,
              previousClose: tick.previousClose || prev[tick.symbol]?.previousClose || tick.close || tick.lastPrice,
              volume: tick.volume || prev[tick.symbol]?.volume || 0,
              change: tick.change || 0,
              changePercent: tick.changePercent || 0,
              bid: tick.bid || prev[tick.symbol]?.bid || 0,
              bidQty: tick.bidQty || prev[tick.symbol]?.bidQty || 0,
              ask: tick.ask || prev[tick.symbol]?.ask || 0,
              askQty: tick.askQty || prev[tick.symbol]?.askQty || 0,
              timestamp: tick.timestamp || Date.now(),
            }
          }));
        }
      };

      adapter.onTick(handleWatchlistTick);

      const BATCH_SIZE = 10;
      const BATCH_DELAY = 500;

      for (let i = 0; i < watchlist.length && !isCancelled; i += BATCH_SIZE) {
        const batch = watchlist.slice(i, i + BATCH_SIZE);

        for (const symbol of batch) {
          if (isCancelled) break;
          try {
            await adapter.subscribe(symbol, selectedExchange, 'quote');
            subscribedSymbols.push(symbol);
          } catch (err) {
            console.warn(`[EquityTrading] Failed to subscribe to ${symbol}:`, err);
          }
        }

        if (i + BATCH_SIZE < watchlist.length && !isCancelled) {
          await new Promise(resolve => setTimeout(resolve, BATCH_DELAY));
        }
      }

      console.log(`[EquityTrading] Subscribed to ${subscribedSymbols.length} watchlist symbols via WebSocket`);

      return handleWatchlistTick;
    };

    let tickHandler: ((tick: any) => void) | undefined;

    subscribeWatchlist().then(handler => {
      tickHandler = handler;
    }).catch(err => {
      console.error('[EquityTrading] WebSocket watchlist subscription failed:', err);
    });

    return () => {
      isCancelled = true;

      if (tickHandler) {
        adapter.offTick(tickHandler);
      }

      for (const symbol of subscribedSymbols) {
        if (symbol !== selectedSymbol) {
          adapter.unsubscribe(symbol, selectedExchange).catch(() => {});
        }
      }
    };
  }, [adapter, isAuthenticated, watchlist, selectedExchange, useWebSocketForWatchlist, selectedSymbol]);

  // Fallback: Fetch watchlist quotes via REST if WebSocket is disabled
  useEffect(() => {
    if (!adapter || !isAuthenticated || useWebSocketForWatchlist) {
      console.log('[EquityTrading] Skipping watchlist polling - broker not authenticated or WS enabled');
      return;
    }

    let isCancelled = false;
    let isPolling = false;
    let pollingIntervalId: ReturnType<typeof setInterval> | null = null;
    let hasFetchedInitial = false;

    const fetchWatchlistQuotes = async (useCache = false) => {
      if (isCancelled || isPolling) return;

      const marketOpen = isMarketOpen(selectedExchange);
      const cacheKey = `watchlist:quotes:${selectedExchange}:${watchlist.slice(0, 5).join('-')}`;

      if (!marketOpen && hasFetchedInitial && useCache) {
        console.log('[EquityTrading] Market closed, using cached watchlist data');
        return;
      }

      isPolling = true;

      try {
        if (!marketOpen) {
          const cached = await cacheService.get<Record<string, Quote>>(cacheKey);
          if (cached && !cached.isStale) {
            setWatchlistQuotes(prev => ({ ...prev, ...cached.data }));
            console.log('[EquityTrading] ✓ Loaded watchlist from cache (market closed)');
            hasFetchedInitial = true;
            return;
          }
        }

        console.log(`[EquityTrading] Fetching watchlist quotes (market ${marketOpen ? 'OPEN' : 'CLOSED'})...`);

        const quotes = await adapter.getQuotes(
          watchlist.map(s => ({ symbol: s, exchange: selectedExchange }))
        );

        if (quotes && quotes.length > 0 && !isCancelled) {
          const newQuotes: Record<string, Quote> = {};
          quotes.forEach(q => {
            newQuotes[q.symbol] = q;
          });
          setWatchlistQuotes(prev => ({ ...prev, ...newQuotes }));

          const ttl = getCacheTTL(selectedExchange);
          await cacheService.set(cacheKey, newQuotes, 'market-quotes', ttl);

          console.log(`[EquityTrading] ✓ Received ${quotes.length} watchlist quotes`);
          hasFetchedInitial = true;
        }
      } catch (err) {
        console.error('[EquityTrading] Failed to fetch watchlist quotes:', err);
      } finally {
        isPolling = false;
      }
    };

    const interval = getPollingInterval(selectedExchange);
    const marketStatus = getMarketStatus(selectedExchange);

    console.log(`[EquityTrading] Watchlist poll interval: ${interval / 1000}s (${marketStatus.status})`);

    const initialTimeout = setTimeout(() => {
      fetchWatchlistQuotes(false);
    }, 3000);

    pollingIntervalId = setInterval(() => {
      fetchWatchlistQuotes(true);
    }, interval);

    return () => {
      isCancelled = true;
      clearTimeout(initialTimeout);
      if (pollingIntervalId) {
        clearInterval(pollingIntervalId);
      }
    };
  }, [adapter, isAuthenticated, watchlist, selectedExchange, useWebSocketForWatchlist]);

  // Smart polling for selected symbol's quote and depth data
  useEffect(() => {
    if (!adapter || !isAuthenticated || !selectedSymbol) {
      console.log('[EquityTrading] Skipping symbol polling - broker not authenticated or no symbol');
      return;
    }

    let isCancelled = false;
    let isPolling = false;
    let pollingIntervalId: ReturnType<typeof setInterval> | null = null;
    let hasFetchedInitial = false;

    const fetchQuoteAndDepth = async (useCache = false) => {
      if (isCancelled || isPolling) return;

      const marketOpen = isMarketOpen(selectedExchange);
      const quoteCacheKey = `quote:${selectedExchange}:${selectedSymbol}`;
      const depthCacheKey = `depth:${selectedExchange}:${selectedSymbol}`;

      if (!marketOpen && hasFetchedInitial && useCache) {
        console.log(`[EquityTrading] Market closed, skipping poll for ${selectedSymbol}`);
        return;
      }

      isPolling = true;

      try {
        if (!marketOpen) {
          const [cachedQuote, cachedDepth] = await Promise.all([
            cacheService.get<Quote>(quoteCacheKey),
            cacheService.get<MarketDepth>(depthCacheKey),
          ]);

          if (cachedQuote && !cachedQuote.isStale) {
            setQuote(cachedQuote.data);
            console.log(`[EquityTrading] ✓ Loaded ${selectedSymbol} quote from cache`);
          }
          if (cachedDepth && !cachedDepth.isStale) {
            setMarketDepth(cachedDepth.data);
            console.log(`[EquityTrading] ✓ Loaded ${selectedSymbol} depth from cache`);
          }

          if (cachedQuote && !cachedQuote.isStale && cachedDepth && !cachedDepth.isStale) {
            hasFetchedInitial = true;
            return;
          }
        }

        console.log(`[EquityTrading] Polling ${selectedSymbol} (market ${marketOpen ? 'OPEN' : 'CLOSED'})...`);

        const skipDepth = activeBroker === 'yfinance';
        const [freshQuote, freshDepth] = await Promise.all([
          adapter.getQuote(selectedSymbol, selectedExchange).catch(() => null),
          skipDepth ? Promise.resolve(null) : adapter.getMarketDepth(selectedSymbol, selectedExchange).catch(() => null),
        ]);

        const ttl = getCacheTTL(selectedExchange);

        if (freshQuote && !isCancelled) {
          setQuote(freshQuote);
          await cacheService.set(quoteCacheKey, freshQuote, 'market-quotes', ttl);
        }

        if (freshDepth && !isCancelled) {
          setMarketDepth(freshDepth);
          await cacheService.set(depthCacheKey, freshDepth, 'market-quotes', ttl);
          console.log(`[EquityTrading] ✓ Depth updated: ${freshDepth.bids?.length || 0} bids, ${freshDepth.asks?.length || 0} asks`);
        }

        hasFetchedInitial = true;
      } catch (err) {
        console.error(`[EquityTrading] Failed to poll ${selectedSymbol}:`, err);
      } finally {
        isPolling = false;
      }
    };

    const interval = getPollingInterval(selectedExchange);
    const marketStatus = getMarketStatus(selectedExchange);

    console.log(`[EquityTrading] ${selectedSymbol} poll interval: ${interval / 1000}s (${marketStatus.status})`);

    const initialTimeout = setTimeout(() => {
      fetchQuoteAndDepth(false);
    }, 5000);

    pollingIntervalId = setInterval(() => {
      fetchQuoteAndDepth(true);
    }, interval);

    return () => {
      isCancelled = true;
      clearTimeout(initialTimeout);
      if (pollingIntervalId) {
        clearInterval(pollingIntervalId);
      }
    };
  }, [adapter, isAuthenticated, selectedSymbol, selectedExchange]);

  // Select symbol from search
  const handleSelectSymbol = (symbol: string, exchange: StockExchange, result?: SymbolSearchResult) => {
    setSelectedSymbol(symbol);
    setSelectedExchange(exchange);

    if (result) {
      console.log(`[EquityTrading] Selected ${symbol} on ${exchange}`, {
        token: result.token,
        name: result.name,
        instrumentType: result.instrument_type,
        lotSize: result.lot_size
      });
    }
  };

  // Calculate derived values
  const currentPrice = quote?.lastPrice || 0;
  const priceChange = quote?.change || 0;
  const priceChangePercent = quote?.changePercent || 0;
  const spread = (quote?.ask && quote?.bid && quote.ask > 0 && quote.bid > 0)
    ? (quote.ask - quote.bid)
    : 0;
  const spreadPercent = (spread > 0 && quote?.bid && quote.bid > 0)
    ? (spread / quote.bid) * 100
    : 0;
  const dayRange = quote ? (quote.high - quote.low) : 0;
  const dayRangePercent = quote && quote.low > 0 ? (dayRange / quote.low) * 100 : 0;

  // Connection status
  const isConnected = isAuthenticated && adapter !== null;

  // Calculate totals for display
  const totalPositionValue = positions.reduce((acc, p) => acc + (p.quantity * p.lastPrice), 0);
  const totalPositionPnl = positions.reduce((acc, p) => acc + p.pnl, 0);
  const totalHoldingsValue = holdings.reduce((acc, h) => acc + h.currentValue, 0);

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT_DARK_BG,
      color: '#FFFFFF',
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT_DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: #2A2A2A; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: #4A4A4A; }

        .terminal-glow {
          text-shadow: 0 0 10px ${FINCEPT_ORANGE}40;
        }

        .price-flash {
          animation: flash 0.3s ease-out;
        }

        @keyframes flash {
          0% { background-color: #FFD70040; }
          100% { background-color: transparent; }
        }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>

      {/* Top Navigation Bar */}
      <EquityTopNav
        activeBroker={activeBroker}
        activeBrokerMetadata={activeBrokerMetadata}
        tradingMode={tradingMode}
        isConnected={isConnected}
        isPaper={isPaper}
        isLive={isLive}
        showSettings={showSettings}
        currentTime={currentTime}
        onTradingModeChange={setTradingMode}
        onSettingsToggle={() => setShowSettings(!showSettings)}
      />

      {/* Ticker Bar */}
      <EquityTickerBar
        selectedSymbol={selectedSymbol}
        selectedExchange={selectedExchange}
        activeBroker={activeBroker}
        quote={quote}
        funds={funds}
        isAuthenticated={isAuthenticated}
        totalPositionPnl={totalPositionPnl}
        priceChange={priceChange}
        priceChangePercent={priceChangePercent}
        currentPrice={currentPrice}
        dayRange={dayRange}
        dayRangePercent={dayRangePercent}
        onSymbolSelect={handleSelectSymbol}
        fmtPrice={fmtPrice}
      />

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Sidebar - Watchlist */}
        <EquityWatchlist
          watchlist={watchlist}
          watchlistQuotes={watchlistQuotes}
          selectedSymbol={selectedSymbol}
          selectedExchange={selectedExchange}
          leftSidebarView={leftSidebarView}
          useWebSocketForWatchlist={useWebSocketForWatchlist}
          onSymbolSelect={(symbol, exchange) => handleSelectSymbol(symbol, exchange)}
          onViewChange={setLeftSidebarView}
          onWebSocketToggle={() => setUseWebSocketForWatchlist(!useWebSocketForWatchlist)}
        />

        {/* Center - Chart & Bottom Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
          {/* Chart Area */}
          <EquityChartArea
            selectedSymbol={selectedSymbol}
            selectedExchange={selectedExchange}
            centerView={centerView}
            chartInterval={chartInterval}
            quote={quote}
            symbolTrades={symbolTrades}
            isBottomPanelMinimized={isBottomPanelMinimized}
            isRefreshing={isRefreshing}
            onViewChange={setCenterView}
            onIntervalChange={setChartInterval}
            onRefresh={refreshAll}
            fmtPrice={fmtPrice}
          />

          {/* Bottom Panel */}
          <EquityBottomPanel
            activeTab={activeBottomTab}
            isMinimized={isBottomPanelMinimized}
            positions={positions}
            holdings={holdings}
            orders={orders}
            funds={funds}
            totalPositionValue={totalPositionValue}
            totalPositionPnl={totalPositionPnl}
            totalHoldingsValue={totalHoldingsValue}
            selectedSymbol={selectedSymbol}
            selectedExchange={selectedExchange}
            currentPrice={currentPrice}
            activeBroker={activeBroker}
            isPaper={isPaper}
            adapter={adapter}
            onTabChange={setActiveBottomTab}
            onMinimizeToggle={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
            fmtPrice={fmtPrice}
          />
        </div>

        {/* Right Sidebar - Order Entry & Market Depth */}
        <div style={{
          width: '280px',
          minWidth: '280px',
          backgroundColor: '#0F0F0F',
          borderLeft: '1px solid #2A2A2A',
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0
        }}>
          <EquityOrderEntry
            selectedSymbol={selectedSymbol}
            selectedExchange={selectedExchange}
            currentPrice={currentPrice}
            isAuthenticated={isAuthenticated}
            isLoading={isLoading}
            isConnecting={isConnecting}
            onConnectBroker={() => setActiveBottomTab('brokers')}
          />

          <EquityOrderBook
            rightPanelView={rightPanelView}
            marketDepth={marketDepth}
            quote={quote}
            selectedSymbol={selectedSymbol}
            selectedExchange={selectedExchange}
            isAuthenticated={isAuthenticated}
            isLoadingQuote={isLoadingQuote}
            spread={spread}
            spreadPercent={spreadPercent}
            onViewChange={setRightPanelView}
            fmtPrice={fmtPrice}
          />
        </div>
      </div>

      {/* Status Bar */}
      <EquityStatusBar
        activeBrokerMetadata={activeBrokerMetadata}
        tradingMode={tradingMode}
        isConnected={isConnected}
        isPaper={isPaper}
      />
    </div>
  );
}

// Main export with Provider wrapper
export function EquityTradingTab() {
  return (
    <StockBrokerProvider>
      <EquityTradingContent />
    </StockBrokerProvider>
  );
}

export default EquityTradingTab;
