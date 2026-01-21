/**
 * Equity Trading Tab - Fincept Terminal Style
 *
 * Professional stock/equity trading interface matching the crypto trading tab design.
 * Supports multiple brokers across different regions with real-time market data.
 */

import React, { useState, useEffect, useCallback, useMemo, useRef } from 'react';
import {
  TrendingUp, TrendingDown, Activity, DollarSign, BarChart3,
  Settings as SettingsIcon, Globe, Wifi, WifiOff, Bell,
  Zap, Target, AlertCircle, RefreshCw, Maximize2, ChevronDown,
  Clock, ChevronUp, Minimize2, Search, Building2, Briefcase,
  FileText, History, PieChart, LineChart, BarChart2, Layers
} from 'lucide-react';
import { useTranslation } from 'react-i18next';

import { StockBrokerProvider, useStockBrokerContext, useStockTradingMode, useStockTradingData } from '@/contexts/StockBrokerContext';
import {
  BrokerSelector,
  BrokersManagementPanel,
  StockOrderForm,
  PositionsPanel,
  HoldingsPanel,
  OrdersPanel,
  FundsPanel,
  StockTradingChart,
  SymbolSearch,
} from './components';
import type { SymbolSearchResult, SupportedBroker } from '@/services/trading/masterContractService';

import type { StockExchange, Quote, MarketDepth } from '@/brokers/stocks/types';
import { GridTradingPanel } from '../trading/grid-trading';

// Market hours and caching utilities
import { getMarketStatus, getPollingInterval, getCacheTTL, isMarketOpen, getMarketStatusMessage } from '@/services/markets/marketHoursService';
import cacheService from '@/services/cache/cacheService';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

// NIFTY 50 Components - Full watchlist (current constituents as of Jan 2025)
const DEFAULT_WATCHLIST = [
  // Financials (Weight: ~35%)
  'HDFCBANK', 'ICICIBANK', 'SBIN', 'KOTAKBANK', 'AXISBANK',
  'BAJFINANCE', 'BAJAJFINSV', 'HDFCLIFE', 'SBILIFE', 'INDUSINDBK',

  // IT (Weight: ~13%)
  'TCS', 'INFY', 'WIPRO', 'HCLTECH', 'TECHM', 'LTIM',

  // Oil & Gas / Energy (Weight: ~12%)
  'RELIANCE', 'ONGC', 'NTPC', 'POWERGRID', 'ADANIPORTS', 'ADANIENT',

  // Auto (Weight: ~7%)
  'MARUTI', 'TATAMOTORS', 'M&M', 'BAJAJ-AUTO', 'HEROMOTOCO', 'EICHERMOT',

  // FMCG (Weight: ~8%)
  'HINDUNILVR', 'ITC', 'NESTLEIND', 'BRITANNIA', 'TATACONSUM',

  // Metals & Mining (Weight: ~4%)
  'TATASTEEL', 'JSWSTEEL', 'HINDALCO', 'COALINDIA',

  // Pharma & Healthcare (Weight: ~4%)
  'SUNPHARMA', 'DRREDDY', 'CIPLA', 'APOLLOHOSP', 'DIVISLAB',

  // Infrastructure & Others (Weight: ~17%)
  'LT', 'BHARTIARTL', 'ULTRACEMCO', 'GRASIM', 'TITAN',
  'ASIANPAINT', 'BPCL', 'SHRIRAMFIN', 'TRENT',
];

// Market indices
const MARKET_INDICES = [
  { symbol: 'NIFTY 50', exchange: 'NSE' as StockExchange },
  { symbol: 'SENSEX', exchange: 'BSE' as StockExchange },
  { symbol: 'BANKNIFTY', exchange: 'NSE' as StockExchange },
  { symbol: 'NIFTYIT', exchange: 'NSE' as StockExchange },
];

type BottomPanelTab = 'positions' | 'holdings' | 'orders' | 'history' | 'stats' | 'market' | 'grid-trading' | 'brokers';
type LeftSidebarView = 'watchlist' | 'indices' | 'sectors';
type CenterView = 'chart' | 'depth' | 'trades';
type RightPanelView = 'orderbook' | 'marketdepth' | 'info';

// Main Tab Content (wrapped in provider)
function EquityTradingContent() {
  const { t } = useTranslation();
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
  const { positions, holdings, orders, trades, funds, refreshAll, refreshTrades, isRefreshing } = useStockTradingData();

  // Time state
  const [currentTime, setCurrentTime] = useState(new Date());

  // Symbol state
  const [selectedSymbol, setSelectedSymbol] = useState<string>('');
  const [selectedExchange, setSelectedExchange] = useState<StockExchange>('NSE');
  // Note: searchQuery, showSymbolDropdown, searchResults, isSearching moved to SymbolSearch component

  // Quote data
  const [quote, setQuote] = useState<Quote | null>(null);
  const [marketDepth, setMarketDepth] = useState<MarketDepth | null>(null);
  const [wsError, setWsError] = useState<string | null>(null);
  const [isLoadingQuote, setIsLoadingQuote] = useState(false);

  // Watchlist
  const [watchlist, setWatchlist] = useState<string[]>(DEFAULT_WATCHLIST);
  const [watchlistQuotes, setWatchlistQuotes] = useState<Record<string, Quote>>({});
  // Auto-set WebSocket based on market hours - ON during market hours, OFF when closed
  const [useWebSocketForWatchlist, setUseWebSocketForWatchlist] = useState<boolean>(() => {
    return isMarketOpen(selectedExchange || 'NSE');
  });

  // UI State
  const [leftSidebarView, setLeftSidebarView] = useState<LeftSidebarView>('watchlist');
  const [centerView, setCenterView] = useState<CenterView>('chart');
  const [rightPanelView, setRightPanelView] = useState<RightPanelView>('orderbook');
  const [activeBottomTab, setActiveBottomTab] = useState<BottomPanelTab>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);
  const [showSettings, setShowSettings] = useState(false);

  // Filter trades for the selected symbol (memoized)
  const symbolTrades = useMemo(() => {
    if (!selectedSymbol || !trades || trades.length === 0) return [];
    // Filter trades for current symbol and sort by time (most recent first)
    return trades
      .filter(t => t.symbol === selectedSymbol)
      .sort((a, b) => new Date(b.tradedAt).getTime() - new Date(a.tradedAt).getTime())
      .slice(0, 50); // Keep last 50 trades
  }, [trades, selectedSymbol]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Auto-toggle WebSocket based on market hours
  // Check every minute and switch WS on when market opens, off when it closes
  useEffect(() => {
    const checkMarketStatus = () => {
      const marketOpen = isMarketOpen(selectedExchange);
      setUseWebSocketForWatchlist(prev => {
        if (prev !== marketOpen) {
          console.log(`[EquityTrading] Market ${marketOpen ? 'OPEN' : 'CLOSED'} - WebSocket ${marketOpen ? 'enabled' : 'disabled'}`);
        }
        return marketOpen;
      });
    };

    // Check immediately
    checkMarketStatus();

    // Check every minute
    const interval = setInterval(checkMarketStatus, 60 * 1000);
    return () => clearInterval(interval);
  }, [selectedExchange]);

  // Set default symbol when broker changes
  useEffect(() => {
    if (defaultSymbols.length > 0 && !selectedSymbol) {
      setSelectedSymbol(defaultSymbols[0]);
    }
    // Update watchlist - use broker defaults if available, otherwise use full Nifty 50
    if (defaultSymbols.length > 0) {
      // Use broker's default symbols (but not sliced - show all 50)
      setWatchlist(defaultSymbols.slice(0, 50));
    } else {
      // Fallback to our DEFAULT_WATCHLIST (full Nifty 50)
      setWatchlist(DEFAULT_WATCHLIST);
    }
  }, [defaultSymbols, selectedSymbol]);

  // Subscribe to WebSocket for live prices and fetch initial data
  useEffect(() => {
    // CRITICAL: Don't make any API calls if broker is not authenticated
    if (!adapter || !isAuthenticated || !selectedSymbol) {
      console.log('[EquityTrading] Skipping subscription - broker not authenticated or no symbol selected');
      return;
    }

    let isCancelled = false;

    const subscribeToSymbol = async () => {
      try {
        console.log(`[EquityTrading] Subscribing to ${selectedSymbol} via WebSocket...`);
        setIsLoadingQuote(true);

        // Fetch initial quote immediately
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

        // Fetch initial market depth
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

        setIsLoadingQuote(false);

        // Subscribe to WebSocket for live updates
        await adapter.subscribe(selectedSymbol, selectedExchange, 'full');

        const handleTick = (tick: any) => {
          if (tick.symbol === selectedSymbol && !isCancelled) {
            // Update quote with tick data
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
              // Update bid/ask from tick or preserve previous values
              bid: tick.bid || prevQuote?.bid || 0,
              bidQty: tick.bidQty || prevQuote?.bidQty || 0,
              ask: tick.ask || prevQuote?.ask || 0,
              askQty: tick.askQty || prevQuote?.askQty || 0,
              timestamp: Date.now(),
            }));

            // Also update market depth if tick includes depth data
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

            // Also update quote's bid/ask from depth data (best bid/ask)
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
        setWsError(null);
      } catch (err: any) {
        console.error('[EquityTrading] WebSocket subscription failed:', err);
        setWsError(err.message);
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
    // CRITICAL: Don't make any API calls if broker is not authenticated
    if (!adapter || !isAuthenticated || !useWebSocketForWatchlist) {
      console.log('[EquityTrading] Skipping watchlist subscription - broker not authenticated or WS disabled');
      return;
    }

    let isCancelled = false;
    const subscribedSymbols: string[] = [];

    const subscribeWatchlist = async () => {
      console.log('[EquityTrading] Subscribing to watchlist via WebSocket...');

      // Handle incoming ticks for watchlist symbols
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

      // Register tick handler
      adapter.onTick(handleWatchlistTick);

      // Subscribe to all watchlist symbols in batches
      const BATCH_SIZE = 10;
      const BATCH_DELAY = 500; // 500ms between subscription batches

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

        // Small delay between batches
        if (i + BATCH_SIZE < watchlist.length && !isCancelled) {
          await new Promise(resolve => setTimeout(resolve, BATCH_DELAY));
        }
      }

      console.log(`[EquityTrading] Subscribed to ${subscribedSymbols.length} watchlist symbols via WebSocket`);

      // Store the tick handler for cleanup
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

      // Unsubscribe from all watchlist symbols
      if (tickHandler) {
        adapter.offTick(tickHandler);
      }

      // Unsubscribe symbols (but not the selected symbol)
      for (const symbol of subscribedSymbols) {
        if (symbol !== selectedSymbol) {
          adapter.unsubscribe(symbol, selectedExchange).catch(() => {});
        }
      }
    };
  }, [adapter, isAuthenticated, watchlist, selectedExchange, useWebSocketForWatchlist, selectedSymbol]);

  // Fallback: Fetch watchlist quotes via REST if WebSocket is disabled
  // Uses smart polling based on market hours - polls frequently when open, uses cache when closed
  useEffect(() => {
    // CRITICAL: Don't make any API calls if broker is not authenticated
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

      // If market is closed and we already fetched, skip polling (use cached data)
      if (!marketOpen && hasFetchedInitial && useCache) {
        console.log('[EquityTrading] Market closed, using cached watchlist data');
        return;
      }

      isPolling = true;

      try {
        // Try to get from cache first when market is closed
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

          // Cache the quotes with TTL based on market status
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

    // Get polling interval based on market hours
    const interval = getPollingInterval(selectedExchange);
    const marketStatus = getMarketStatus(selectedExchange);

    console.log(`[EquityTrading] Watchlist poll interval: ${interval / 1000}s (${marketStatus.status})`);

    // Initial fetch after 3 seconds
    const initialTimeout = setTimeout(() => {
      fetchWatchlistQuotes(false);
    }, 3000);

    // Setup polling interval
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
  // Polls based on market hours - more frequently when open, uses cache when closed
  useEffect(() => {
    // CRITICAL: Don't make any API calls if broker is not authenticated
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

      // If market is closed and we already fetched, skip polling
      if (!marketOpen && hasFetchedInitial && useCache) {
        console.log(`[EquityTrading] Market closed, skipping poll for ${selectedSymbol}`);
        return;
      }

      isPolling = true;

      try {
        // Try cache first when market is closed
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

          // If both are cached and fresh, skip fetch
          if (cachedQuote && !cachedQuote.isStale && cachedDepth && !cachedDepth.isStale) {
            hasFetchedInitial = true;
            return;
          }
        }

        console.log(`[EquityTrading] Polling ${selectedSymbol} (market ${marketOpen ? 'OPEN' : 'CLOSED'})...`);

        // Fetch quote and depth in parallel
        const [freshQuote, freshDepth] = await Promise.all([
          adapter.getQuote(selectedSymbol, selectedExchange).catch(() => null),
          adapter.getMarketDepth(selectedSymbol, selectedExchange).catch(() => null),
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

    // Get polling interval based on market hours
    const interval = getPollingInterval(selectedExchange);
    const marketStatus = getMarketStatus(selectedExchange);

    console.log(`[EquityTrading] ${selectedSymbol} poll interval: ${interval / 1000}s (${marketStatus.status})`);

    // Initial fetch after a short delay (let WebSocket subscription settle first)
    const initialTimeout = setTimeout(() => {
      fetchQuoteAndDepth(false);
    }, 5000);

    // Setup polling interval
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

  // Select symbol from search (supports both legacy search and master contract search)
  const handleSelectSymbol = (symbol: string, exchange: StockExchange, result?: SymbolSearchResult) => {
    setSelectedSymbol(symbol);
    setSelectedExchange(exchange);

    // Log additional info from master contract if available
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
  // Spread = Ask - Bid (only calculate if both values are valid and positive)
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
      backgroundColor: FINCEPT.DARK_BG,
      color: FINCEPT.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }

        .terminal-glow {
          text-shadow: 0 0 10px ${FINCEPT.ORANGE}40;
        }

        .price-flash {
          animation: flash 0.3s ease-out;
        }

        @keyframes flash {
          0% { background-color: ${FINCEPT.YELLOW}40; }
          100% { background-color: transparent; }
        }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>

      {/* ========== TOP NAVIGATION BAR ========== */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          {/* Terminal Branding */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Building2 size={18} color={FINCEPT.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + FINCEPT.ORANGE + ')' }} />
            <span style={{
              color: FINCEPT.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${FINCEPT.ORANGE}40`
            }}>
              EQUITY TERMINAL
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

          {/* Broker Selector */}
          <BrokerSelector />

          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

          {/* Trading Mode Toggle */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '2px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            padding: '2px',
          }}>
            <button
              onClick={() => setTradingMode('paper')}
              style={{
                padding: '6px 14px',
                backgroundColor: isPaper ? FINCEPT.GREEN : 'transparent',
                border: 'none',
                color: isPaper ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {isPaper && (
                <div style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: FINCEPT.DARK_BG,
                }} />
              )}
              PAPER
            </button>
            <button
              onClick={() => {
                if (!isLive) {
                  const confirmed = window.confirm(
                    'WARNING: SWITCHING TO LIVE TRADING\n\n' +
                    'You are about to enable LIVE trading mode.\n\n' +
                    '- All orders will be placed with REAL funds\n' +
                    '- Trades will be executed on the actual exchange\n' +
                    '- You may lose money\n\n' +
                    'Are you sure you want to continue?'
                  );
                  if (confirmed) {
                    setTradingMode('live');
                  }
                }
              }}
              style={{
                padding: '6px 14px',
                backgroundColor: isLive ? FINCEPT.RED : 'transparent',
                border: 'none',
                color: isLive ? FINCEPT.WHITE : FINCEPT.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
              }}
            >
              {isLive && (
                <div style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: FINCEPT.WHITE,
                  boxShadow: `0 0 6px ${FINCEPT.WHITE}`,
                  animation: 'pulse 1s infinite',
                }} />
              )}
              LIVE
            </button>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Connection Status */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px' }}>
            {isConnected ? (
              <>
                <Wifi size={12} color={FINCEPT.GREEN} />
                <span style={{ color: FINCEPT.GREEN }}>CONNECTED</span>
              </>
            ) : (
              <>
                <WifiOff size={12} color={FINCEPT.RED} />
                <span style={{ color: FINCEPT.RED }}>DISCONNECTED</span>
              </>
            )}
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

          {/* Clock */}
          <div style={{
            fontSize: '11px',
            fontWeight: 600,
            color: FINCEPT.CYAN,
            fontFamily: 'monospace'
          }}>
            {currentTime.toLocaleTimeString('en-IN', { hour12: false })}
          </div>

          {/* Settings */}
          <button
            onClick={() => setShowSettings(!showSettings)}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${FINCEPT.BORDER}`,
              color: FINCEPT.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '10px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.ORANGE;
              e.currentTarget.style.color = FINCEPT.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = FINCEPT.BORDER;
              e.currentTarget.style.color = FINCEPT.GRAY;
            }}
          >
            <SettingsIcon size={12} />
          </button>
        </div>
      </div>

      {/* ========== TICKER BAR ========== */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        padding: '10px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '20px',
        flexShrink: 0
      }}>
        {/* Symbol Selector - Using Master Contract Database */}
        <SymbolSearch
          selectedSymbol={selectedSymbol}
          selectedExchange={selectedExchange}
          onSymbolSelect={handleSelectSymbol}
          brokerId={(activeBroker || 'angelone') as SupportedBroker}
          placeholder="Search stocks, F&O, commodities..."
          showDownloadStatus={true}
        />

        {/* Price Display */}
        <div style={{ display: 'flex', alignItems: 'baseline', gap: '8px' }}>
          <span style={{
            fontSize: '24px',
            fontWeight: 700,
            color: FINCEPT.YELLOW,
            willChange: 'contents',
            transition: 'none'
          }}>
            {currentPrice > 0 ? `₹${currentPrice.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}` : '--'}
          </span>
          {quote && (priceChange !== 0 || priceChangePercent !== 0) && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              {priceChange >= 0 ? (
                <TrendingUp size={16} color={FINCEPT.GREEN} />
              ) : (
                <TrendingDown size={16} color={FINCEPT.RED} />
              )}
              <span style={{
                fontSize: '13px',
                fontWeight: 600,
                color: priceChange >= 0 ? FINCEPT.GREEN : FINCEPT.RED
              }}>
                {priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)} ({priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%)
              </span>
            </div>
          )}
        </div>

        <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />

        {/* Market Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px', willChange: 'contents' }}>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
            <div style={{ color: FINCEPT.GREEN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.bid ? `₹${quote.bid.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
            <div style={{ color: FINCEPT.RED, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.ask ? `₹${quote.ask.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '100px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>DAY RANGE</div>
            <div style={{ color: FINCEPT.CYAN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {dayRange > 0 ? `₹${dayRange.toFixed(2)} (${dayRangePercent.toFixed(2)}%)` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>HIGH</div>
            <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.high ? `₹${quote.high.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>LOW</div>
            <div style={{ color: FINCEPT.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.low ? `₹${quote.low.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '100px' }}>
            <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>VOLUME</div>
            <div style={{ color: FINCEPT.PURPLE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.volume ? quote.volume.toLocaleString('en-IN') : '--'}
            </div>
          </div>
        </div>

        {/* Account Summary */}
        {isAuthenticated && (
          <>
            <div style={{ height: '24px', width: '1px', backgroundColor: FINCEPT.BORDER }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
              <div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>MARGIN</div>
                <div style={{ color: FINCEPT.CYAN, fontWeight: 600 }}>
                  ₹{(funds?.availableMargin || 0).toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </div>
              </div>
              <div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginBottom: '2px' }}>P&L</div>
                <div style={{ color: totalPositionPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED, fontWeight: 600 }}>
                  {totalPositionPnl >= 0 ? '+' : ''}₹{totalPositionPnl.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </div>
              </div>
            </div>
          </>
        )}
      </div>

      {/* ========== MAIN CONTENT ========== */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT SIDEBAR - Watchlist / Indices / Sectors */}
        <div style={{
          width: '280px',
          minWidth: '280px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0
        }}>
          {/* Toggle Header */}
          <div style={{
            padding: '6px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            gap: '4px'
          }}>
            {(['watchlist', 'indices', 'sectors'] as LeftSidebarView[]).map((view) => (
              <button
                key={view}
                onClick={() => setLeftSidebarView(view)}
                style={{
                  flex: 1,
                  padding: '6px 8px',
                  backgroundColor: leftSidebarView === view ? FINCEPT.ORANGE : 'transparent',
                  border: 'none',
                  color: leftSidebarView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s',
                  borderRadius: '2px'
                }}
              >
                {view.toUpperCase()}
              </button>
            ))}
          </div>

          {/* WebSocket Toggle for Watchlist */}
          {leftSidebarView === 'watchlist' && (
            <div style={{
              padding: '4px 8px',
              backgroundColor: FINCEPT.PANEL_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
            }}>
              <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>
                {useWebSocketForWatchlist ? '⚡ REALTIME' : '🔄 POLLING'}
              </span>
              <button
                onClick={() => setUseWebSocketForWatchlist(!useWebSocketForWatchlist)}
                style={{
                  padding: '3px 8px',
                  backgroundColor: useWebSocketForWatchlist ? FINCEPT.GREEN : FINCEPT.MUTED,
                  border: 'none',
                  color: useWebSocketForWatchlist ? FINCEPT.DARK_BG : FINCEPT.WHITE,
                  cursor: 'pointer',
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                  transition: 'all 0.2s',
                }}
                title={useWebSocketForWatchlist ? 'Switch to REST polling' : 'Switch to WebSocket real-time'}
              >
                {useWebSocketForWatchlist ? 'WS ON' : 'WS OFF'}
              </button>
            </div>
          )}

          {/* Content */}
          <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
            {leftSidebarView === 'watchlist' && (
              <div style={{
                flex: 1,
                overflow: 'auto',
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: '0',
                alignContent: 'start'
              }}>
                {watchlist.map((symbol, idx) => {
                  const q = watchlistQuotes[symbol];
                  const isSelected = symbol === selectedSymbol;

                  return (
                    <div
                      key={symbol}
                      onClick={() => handleSelectSymbol(symbol, selectedExchange)}
                      style={{
                        padding: '8px 10px',
                        cursor: 'pointer',
                        backgroundColor: isSelected ? `${FINCEPT.ORANGE}15` : 'transparent',
                        borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                        borderBottom: `1px solid ${FINCEPT.BORDER}`,
                        borderRight: idx % 2 === 0 ? `1px solid ${FINCEPT.BORDER}` : 'none',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => {
                        if (!isSelected) {
                          e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (!isSelected) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }
                      }}
                    >
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        gap: '8px'
                      }}>
                        <div style={{
                          fontSize: '11px',
                          fontWeight: 700,
                          color: isSelected ? FINCEPT.ORANGE : FINCEPT.WHITE,
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                          maxWidth: '60px'
                        }}>
                          {symbol}
                        </div>

                        {q && q.lastPrice != null ? (
                          <div style={{
                            display: 'flex',
                            flexDirection: 'column',
                            alignItems: 'flex-end',
                            gap: '2px'
                          }}>
                            <div style={{
                              fontSize: '10px',
                              color: FINCEPT.WHITE,
                              fontFamily: 'monospace',
                              fontWeight: 600
                            }}>
                              ₹{q.lastPrice >= 1000
                                ? q.lastPrice.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })
                                : q.lastPrice.toFixed(2)
                              }
                            </div>
                            <div style={{
                              fontSize: '9px',
                              color: (q.changePercent ?? 0) >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                              fontFamily: 'monospace',
                              fontWeight: 700
                            }}>
                              {(q.changePercent ?? 0) >= 0 ? '▲' : '▼'} {Math.abs(q.changePercent ?? 0).toFixed(2)}%
                            </div>
                          </div>
                        ) : (
                          <div style={{
                            fontSize: '9px',
                            color: FINCEPT.GRAY,
                            fontFamily: 'monospace'
                          }}>
                            ...
                          </div>
                        )}
                      </div>
                    </div>
                  );
                })}
              </div>
            )}

            {leftSidebarView === 'indices' && (
              <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '12px',
                  letterSpacing: '0.5px'
                }}>
                  MARKET INDICES
                </div>
                {MARKET_INDICES.map((index) => (
                  <div
                    key={index.symbol}
                    style={{
                      padding: '12px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      marginBottom: '8px',
                      cursor: 'pointer'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div>
                        <div style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{index.symbol}</div>
                        <div style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{index.exchange}</div>
                      </div>
                      <div style={{ textAlign: 'right' }}>
                        <div style={{ fontSize: '12px', fontWeight: 600, color: FINCEPT.YELLOW }}>--</div>
                        <div style={{ fontSize: '10px', color: FINCEPT.GRAY }}>--</div>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            )}

            {leftSidebarView === 'sectors' && (
              <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: FINCEPT.GRAY,
                  marginBottom: '12px',
                  letterSpacing: '0.5px'
                }}>
                  SECTOR PERFORMANCE
                </div>
                {['IT', 'Banking', 'Pharma', 'Auto', 'FMCG', 'Metal', 'Realty', 'Energy'].map((sector) => (
                  <div
                    key={sector}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      marginBottom: '6px',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      cursor: 'pointer'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.CYAN;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    }}
                  >
                    <span style={{ fontSize: '11px', color: FINCEPT.WHITE }}>{sector}</span>
                    <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>--</span>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>

        {/* CENTER - Chart & Bottom Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>
          {/* Chart Area - Takes maximum available space */}
          <div style={{
            flex: isBottomPanelMinimized ? 1 : '1 1 auto',
            backgroundColor: FINCEPT.PANEL_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            minHeight: '300px',
            transition: 'flex 0.3s ease'
          }}>
            {/* Chart Header - Compact */}
            <div style={{
              padding: '4px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              flexShrink: 0
            }}>
              {(['CHART', 'DEPTH', 'TRADES'] as const).map((view) => (
                <button
                  key={view}
                  onClick={() => setCenterView(view.toLowerCase() as CenterView)}
                  style={{
                    padding: '3px 10px',
                    backgroundColor: centerView === view.toLowerCase() ? FINCEPT.ORANGE : 'transparent',
                    border: 'none',
                    color: centerView === view.toLowerCase() ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                    cursor: 'pointer',
                    fontSize: '9px',
                    fontWeight: 700,
                    transition: 'all 0.2s'
                  }}
                >
                  {view}
                </button>
              ))}

              <div style={{ flex: 1 }} />

              {/* Refresh Button */}
              <button
                onClick={() => refreshAll()}
                disabled={isRefreshing}
                style={{
                  padding: '3px 6px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '9px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}
              >
                <RefreshCw size={10} className={isRefreshing ? 'animate-spin' : ''} />
              </button>
            </div>

            {/* Chart Content - Full Height */}
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'stretch',
              justifyContent: 'stretch',
              color: FINCEPT.GRAY,
              fontSize: '12px',
              overflow: 'hidden',
              minHeight: 0,
              position: 'relative'
            }}>
              {centerView === 'chart' && (
                <div style={{
                  width: '100%',
                  height: '100%',
                  display: 'flex',
                  flexDirection: 'column',
                  position: 'absolute',
                  top: 0,
                  left: 0,
                  right: 0,
                  bottom: 0,
                  paddingBottom: '4px' // Give space for time axis
                }}>
                  <StockTradingChart
                    symbol={selectedSymbol}
                    exchange={selectedExchange}
                    interval="5m"
                  />
                </div>
              )}
              {centerView === 'depth' && (
                <div style={{
                  width: '100%',
                  height: '100%',
                  padding: '20px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexDirection: 'column',
                  gap: '12px'
                }}>
                  <BarChart3 size={48} color={FINCEPT.GRAY} />
                  <span style={{ fontSize: '14px', color: FINCEPT.GRAY }}>Market Depth Chart</span>
                  <span style={{ fontSize: '11px', color: FINCEPT.MUTED }}>Connect to broker for real-time depth data</span>
                </div>
              )}
              {centerView === 'trades' && (
                <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '8px' }}>
                  {symbolTrades.length === 0 ? (
                    <div style={{
                      padding: '40px',
                      textAlign: 'center',
                      color: FINCEPT.GRAY,
                      fontSize: '11px'
                    }}>
                      No recent trades for {selectedSymbol || 'selected symbol'}
                    </div>
                  ) : (
                    <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>TIME</th>
                          <th style={{ padding: '6px', textAlign: 'left', color: FINCEPT.GRAY }}>SIDE</th>
                          <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                          <th style={{ padding: '6px', textAlign: 'right', color: FINCEPT.GRAY }}>QTY</th>
                        </tr>
                      </thead>
                      <tbody>
                        {symbolTrades.slice(0, 20).map((trade, idx) => (
                          <tr key={trade.tradeId || idx} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                            <td style={{ padding: '4px 6px', color: FINCEPT.GRAY, fontSize: '9px' }}>
                              {new Date(trade.tradedAt).toLocaleTimeString()}
                            </td>
                            <td style={{
                              padding: '4px 6px',
                              color: trade.side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED,
                              fontWeight: 600,
                              fontSize: '9px'
                            }}>
                              {trade.side}
                            </td>
                            <td style={{
                              padding: '4px 6px',
                              textAlign: 'right',
                              color: trade.side === 'BUY' ? FINCEPT.GREEN : FINCEPT.RED
                            }}>
                              ₹{trade.price?.toFixed(2)}
                            </td>
                            <td style={{ padding: '4px 6px', textAlign: 'right', color: FINCEPT.WHITE }}>
                              {trade.quantity}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  )}
                </div>
              )}
            </div>
          </div>

          {/* Bottom Panel - Positions/Holdings/Orders - Compact by default, larger for brokers */}
          <div style={{
            flex: isBottomPanelMinimized ? '0 0 32px' : activeBottomTab === 'brokers' ? '0 0 350px' : '0 0 200px',
            minHeight: isBottomPanelMinimized ? '32px' : activeBottomTab === 'brokers' ? '300px' : '150px',
            maxHeight: isBottomPanelMinimized ? '32px' : activeBottomTab === 'brokers' ? '450px' : '220px',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            transition: 'all 0.3s ease'
          }}>
            {/* Bottom Panel Header */}
            <div style={{
              padding: '6px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div style={{ display: 'flex', gap: '4px' }}>
                {([
                  { id: 'positions', label: 'POSITIONS', count: positions.length },
                  { id: 'holdings', label: 'HOLDINGS', count: holdings.length },
                  { id: 'orders', label: 'ORDERS', count: orders.filter(o => ['PENDING', 'OPEN'].includes(o.status)).length },
                  { id: 'history', label: 'HISTORY', count: null },
                  { id: 'stats', label: 'STATS', count: null },
                  { id: 'grid-trading', label: 'GRID BOT', count: null },
                  { id: 'brokers', label: 'BROKERS', count: null },
                ] as const).map((tab) => (
                  <button
                    key={tab.id}
                    onClick={() => setActiveBottomTab(tab.id)}
                    style={{
                      padding: '6px 16px',
                      backgroundColor: activeBottomTab === tab.id ? FINCEPT.ORANGE : 'transparent',
                      border: 'none',
                      color: activeBottomTab === tab.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                      cursor: 'pointer',
                      fontSize: '10px',
                      fontWeight: 700,
                      textTransform: 'uppercase',
                      transition: 'all 0.2s'
                    }}
                  >
                    {tab.label} {tab.count !== null && `(${tab.count})`}
                  </button>
                ))}
              </div>

              {/* Minimize/Maximize Button */}
              <button
                onClick={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${FINCEPT.BORDER}`,
                  color: FINCEPT.GRAY,
                  cursor: 'pointer',
                  fontSize: '10px',
                  fontWeight: 600,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  borderRadius: '2px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  e.currentTarget.style.borderColor = FINCEPT.GRAY;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                }}
              >
                {isBottomPanelMinimized ? (
                  <>
                    <ChevronUp size={14} />
                    <span>MAXIMIZE</span>
                  </>
                ) : (
                  <>
                    <ChevronDown size={14} />
                    <span>MINIMIZE</span>
                  </>
                )}
              </button>
            </div>

            {/* Bottom Panel Content */}
            {!isBottomPanelMinimized && (
              <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                {activeBottomTab === 'positions' && <PositionsPanel />}
                {activeBottomTab === 'holdings' && <HoldingsPanel />}
                {activeBottomTab === 'orders' && <OrdersPanel />}
                {activeBottomTab === 'history' && <OrdersPanel showHistory />}
                {activeBottomTab === 'stats' && (
                  <div style={{
                    padding: '20px',
                    display: 'grid',
                    gridTemplateColumns: 'repeat(4, 1fr)',
                    gap: '16px'
                  }}>
                    <div style={{
                      padding: '16px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>TOTAL POSITIONS VALUE</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.CYAN }}>
                        ₹{totalPositionValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>DAY P&L</div>
                      <div style={{
                        fontSize: '18px',
                        fontWeight: 700,
                        color: totalPositionPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED
                      }}>
                        {totalPositionPnl >= 0 ? '+' : ''}₹{totalPositionPnl.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>PORTFOLIO VALUE</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.YELLOW }}>
                        ₹{totalHoldingsValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: FINCEPT.HEADER_BG,
                      border: `1px solid ${FINCEPT.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>AVAILABLE MARGIN</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: FINCEPT.PURPLE }}>
                        ₹{(funds?.availableMargin || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                  </div>
                )}

                {/* Grid Trading Tab */}
                {activeBottomTab === 'grid-trading' && (
                  <div style={{ padding: '16px', overflow: 'auto', height: '100%' }}>
                    <GridTradingPanel
                      symbol={selectedSymbol}
                      exchange={selectedExchange}
                      currentPrice={currentPrice}
                      brokerType="stock"
                      brokerId={activeBroker || ''}
                      stockAdapter={adapter || undefined}
                      variant="full"
                    />
                  </div>
                )}

                {/* Brokers Management Tab */}
                {activeBottomTab === 'brokers' && (
                  <div style={{ height: '100%', overflow: 'hidden' }}>
                    <BrokersManagementPanel />
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        {/* RIGHT SIDEBAR - Order Entry & Market Depth */}
        <div style={{
          width: '280px',
          minWidth: '280px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0
        }}>
          {/* Order Entry Section */}
          <div style={{
            height: '50%',
            borderBottom: `1px solid ${FINCEPT.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            <div style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              letterSpacing: '0.5px'
            }}>
              ORDER ENTRY
            </div>
            <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
              {!isAuthenticated ? (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  height: '100%',
                  gap: '12px',
                  textAlign: 'center',
                }}>
                  <AlertCircle size={32} color={FINCEPT.ORANGE} />
                  <div>
                    <p style={{ color: FINCEPT.WHITE, fontSize: '12px', fontWeight: 600, margin: '0 0 4px 0' }}>
                      No Broker Connected
                    </p>
                    <p style={{ color: FINCEPT.GRAY, fontSize: '10px', margin: 0 }}>
                      Configure your broker in the BROKERS tab below
                    </p>
                  </div>
                  <button
                    onClick={() => setActiveBottomTab('brokers')}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: FINCEPT.ORANGE,
                      border: 'none',
                      color: FINCEPT.DARK_BG,
                      fontSize: '10px',
                      fontWeight: 700,
                      cursor: 'pointer',
                    }}
                  >
                    CONNECT BROKER
                  </button>
                </div>
              ) : (
                <StockOrderForm
                  symbol={selectedSymbol}
                  exchange={selectedExchange}
                  lastPrice={currentPrice}
                />
              )}
            </div>
          </div>

          {/* Market Depth / Info Section */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            <div style={{
              padding: '8px 12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              letterSpacing: '0.5px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div style={{ display: 'flex', gap: '8px' }}>
                {(['orderbook', 'marketdepth', 'info'] as RightPanelView[]).map((view) => (
                  <button
                    key={view}
                    onClick={() => setRightPanelView(view)}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: rightPanelView === view ? FINCEPT.ORANGE : 'transparent',
                      border: 'none',
                      color: rightPanelView === view ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                      cursor: 'pointer',
                      fontSize: '9px',
                      fontWeight: 700,
                      transition: 'all 0.2s'
                    }}
                  >
                    {view === 'orderbook' ? 'BOOK' : view === 'marketdepth' ? 'DEPTH' : 'INFO'}
                  </button>
                ))}
              </div>
            </div>

            <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
              {rightPanelView === 'orderbook' && (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '8px',
                  height: '100%'
                }}>
                  {/* Buy Orders (Bids) */}
                  <div>
                    <div style={{ color: FINCEPT.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      BIDS (BUY ORDERS)
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          <th style={{ padding: '4px', textAlign: 'left', color: FINCEPT.GRAY }}>QTY</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>PRICE</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>ORDERS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {(marketDepth?.bids && marketDepth.bids.length > 0
                          ? marketDepth.bids.slice(0, 5)
                          : [null, null, null, null, null]
                        ).map((bid, i) => (
                          <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                            <td style={{ padding: '4px', color: FINCEPT.WHITE }}>
                              {bid?.quantity ? bid.quantity.toLocaleString('en-IN') : '--'}
                            </td>
                            <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GREEN }}>
                              {bid?.price ? `₹${bid.price.toFixed(2)}` : '--'}
                            </td>
                            <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>
                              {bid?.orders || '--'}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>

                  {/* Spread Divider */}
                  <div style={{
                    padding: '8px',
                    textAlign: 'center',
                    backgroundColor: FINCEPT.HEADER_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    fontSize: '10px'
                  }}>
                    <span style={{ color: FINCEPT.GRAY }}>Spread: </span>
                    <span style={{ color: FINCEPT.YELLOW, fontWeight: 600 }}>
                      {spread > 0 ? `₹${spread.toFixed(2)} (${spreadPercent.toFixed(3)}%)` : '--'}
                    </span>
                  </div>

                  {/* Sell Orders (Asks) */}
                  <div>
                    <div style={{ color: FINCEPT.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      ASKS (SELL ORDERS)
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                          <th style={{ padding: '4px', textAlign: 'left', color: FINCEPT.GRAY }}>PRICE</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>QTY</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>ORDERS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {(marketDepth?.asks && marketDepth.asks.length > 0
                          ? marketDepth.asks.slice(0, 5)
                          : [null, null, null, null, null]
                        ).map((ask, i) => (
                          <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}40` }}>
                            <td style={{ padding: '4px', color: FINCEPT.RED }}>
                              {ask?.price ? `₹${ask.price.toFixed(2)}` : '--'}
                            </td>
                            <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.WHITE }}>
                              {ask?.quantity ? ask.quantity.toLocaleString('en-IN') : '--'}
                            </td>
                            <td style={{ padding: '4px', textAlign: 'right', color: FINCEPT.GRAY }}>
                              {ask?.orders || '--'}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </div>
              )}

              {rightPanelView === 'marketdepth' && (
                <div style={{
                  height: '100%',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: '8px'
                }}>
                  {marketDepth && (marketDepth.bids?.length > 0 || marketDepth.asks?.length > 0) ? (
                    <>
                      {/* Depth Chart Visualization */}
                      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '4px' }}>
                        {/* Bid depth bars */}
                        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', justifyContent: 'flex-end', gap: '2px' }}>
                          {marketDepth.bids?.slice(0, 5).reverse().map((bid, i) => {
                            const maxQty = Math.max(
                              ...marketDepth.bids.slice(0, 5).map(b => b.quantity),
                              ...marketDepth.asks.slice(0, 5).map(a => a.quantity)
                            );
                            const widthPercent = (bid.quantity / maxQty) * 100;
                            return (
                              <div key={i} style={{ display: 'flex', alignItems: 'center', gap: '4px', height: '20px' }}>
                                <span style={{ fontSize: '9px', color: FINCEPT.GREEN, width: '60px', textAlign: 'right' }}>
                                  ₹{bid.price.toFixed(2)}
                                </span>
                                <div style={{ flex: 1, height: '16px', backgroundColor: FINCEPT.PANEL_BG, position: 'relative' }}>
                                  <div style={{
                                    position: 'absolute',
                                    right: 0,
                                    height: '100%',
                                    width: `${widthPercent}%`,
                                    backgroundColor: `${FINCEPT.GREEN}40`,
                                    borderRight: `2px solid ${FINCEPT.GREEN}`,
                                  }} />
                                </div>
                                <span style={{ fontSize: '9px', color: FINCEPT.WHITE, width: '50px' }}>
                                  {bid.quantity.toLocaleString('en-IN')}
                                </span>
                              </div>
                            );
                          })}
                        </div>

                        {/* Spread line */}
                        <div style={{
                          padding: '4px 8px',
                          backgroundColor: FINCEPT.HEADER_BG,
                          textAlign: 'center',
                          fontSize: '9px',
                          color: FINCEPT.YELLOW,
                          borderTop: `1px solid ${FINCEPT.BORDER}`,
                          borderBottom: `1px solid ${FINCEPT.BORDER}`,
                        }}>
                          Spread: {spread > 0 ? `₹${spread.toFixed(2)}` : '--'}
                        </div>

                        {/* Ask depth bars */}
                        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', gap: '2px' }}>
                          {marketDepth.asks?.slice(0, 5).map((ask, i) => {
                            const maxQty = Math.max(
                              ...marketDepth.bids.slice(0, 5).map(b => b.quantity),
                              ...marketDepth.asks.slice(0, 5).map(a => a.quantity)
                            );
                            const widthPercent = (ask.quantity / maxQty) * 100;
                            return (
                              <div key={i} style={{ display: 'flex', alignItems: 'center', gap: '4px', height: '20px' }}>
                                <span style={{ fontSize: '9px', color: FINCEPT.RED, width: '60px', textAlign: 'right' }}>
                                  ₹{ask.price.toFixed(2)}
                                </span>
                                <div style={{ flex: 1, height: '16px', backgroundColor: FINCEPT.PANEL_BG, position: 'relative' }}>
                                  <div style={{
                                    position: 'absolute',
                                    left: 0,
                                    height: '100%',
                                    width: `${widthPercent}%`,
                                    backgroundColor: `${FINCEPT.RED}40`,
                                    borderLeft: `2px solid ${FINCEPT.RED}`,
                                  }} />
                                </div>
                                <span style={{ fontSize: '9px', color: FINCEPT.WHITE, width: '50px' }}>
                                  {ask.quantity.toLocaleString('en-IN')}
                                </span>
                              </div>
                            );
                          })}
                        </div>
                      </div>
                    </>
                  ) : (
                    <div style={{
                      height: '100%',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      flexDirection: 'column',
                      gap: '12px'
                    }}>
                      <Layers size={48} color={FINCEPT.GRAY} />
                      <span style={{ fontSize: '12px', color: FINCEPT.GRAY }}>Market Depth</span>
                      <span style={{ fontSize: '10px', color: FINCEPT.MUTED, textAlign: 'center' }}>
                        {isLoadingQuote ? 'Loading depth data...' : 'Select a symbol to view depth'}
                      </span>
                    </div>
                  )}
                </div>
              )}

              {rightPanelView === 'info' && (
                <div style={{ padding: '8px' }}>
                  <div style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    SYMBOL INFO
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    {[
                      { label: 'Symbol', value: selectedSymbol || '--' },
                      { label: 'Exchange', value: selectedExchange },
                      { label: 'Open', value: quote?.open ? `₹${quote.open.toFixed(2)}` : '--' },
                      { label: 'Prev Close', value: quote?.previousClose ? `₹${quote.previousClose.toFixed(2)}` : '--' },
                      { label: 'Day High', value: quote?.high ? `₹${quote.high.toFixed(2)}` : '--' },
                      { label: 'Day Low', value: quote?.low ? `₹${quote.low.toFixed(2)}` : '--' },
                      { label: 'Volume', value: quote?.volume ? quote.volume.toLocaleString('en-IN') : '--' },
                    ].map((item) => (
                      <div
                        key={item.label}
                        style={{
                          display: 'flex',
                          justifyContent: 'space-between',
                          padding: '6px 8px',
                          backgroundColor: FINCEPT.HEADER_BG,
                          border: `1px solid ${FINCEPT.BORDER}`
                        }}
                      >
                        <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>{item.label}</span>
                        <span style={{ fontSize: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>{item.value}</span>
                      </div>
                    ))}
                  </div>
                </div>
              )}
            </div>

            {/* Funds Panel (Compact) */}
            {isAuthenticated && (
              <div style={{
                padding: '6px 8px',
                backgroundColor: FINCEPT.HEADER_BG,
                borderTop: `1px solid ${FINCEPT.BORDER}`,
                fontSize: '10px',
                flexShrink: 0,
                overflow: 'hidden'
              }}>
                <FundsPanel compact />
              </div>
            )}
          </div>
        </div>
      </div>

      {/* ========== STATUS BAR ========== */}
      <div style={{
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
        padding: '4px 12px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <span>Fincept Terminal v3.1.4 | Equity Trading Platform</span>
        <span>
          Broker: <span style={{ color: FINCEPT.ORANGE }}>{activeBrokerMetadata?.displayName?.toUpperCase() || 'NONE'}</span> |
          Mode: <span style={{ color: isPaper ? FINCEPT.GREEN : FINCEPT.RED }}>
            {tradingMode.toUpperCase()}
          </span> |
          Status: <span style={{ color: isConnected ? FINCEPT.GREEN : FINCEPT.RED }}>
            {isConnected ? 'CONNECTED' : 'DISCONNECTED'}
          </span>
        </span>
      </div>
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
