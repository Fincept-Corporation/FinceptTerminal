// CryptoTradingTab.tsx - Professional Fincept Terminal-Grade Crypto Trading Interface
import React, { useState, useEffect, useCallback, useReducer, useRef } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { useTranslation } from 'react-i18next';
import { invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { fetch as tauriFetch } from '@tauri-apps/plugin-http';

import { useBrokerContext } from '@/contexts/BrokerContext';
import { useRustTicker, useRustOrderBook, useRustTrades } from '@/hooks/useRustWebSocket';
import { getMarketDataService } from '@/services/trading/UnifiedMarketDataService';
import { withTimeout } from '@/services/core/apiUtils';
import { sanitizeInput } from '@/services/core/validators';
import { ErrorBoundary } from '@/components/common/ErrorBoundary';

// Import sub-components
import {
  CryptoTopNav,
  CryptoTickerBar,
  CryptoWatchlist,
  CryptoOrderBook,
  CryptoOrderEntry,
  CryptoChartArea,
  CryptoBottomPanel,
  CryptoStatusBar,
  CryptoSettingsPanel,
} from './components';

// Import constants and types
import { FINCEPT, DEFAULT_CRYPTO_WATCHLIST, CRYPTO_TERMINAL_STYLES } from './constants';
import type {
  OrderBook,
  Position,
  Order,
  TickerData,
  TradeData,
  WatchlistPrice,
  CenterViewType,
  BottomPanelTabType,
} from './types';

import type { OrderRequest } from '@/types/trading';

// Constants
const API_TIMEOUT_MS = 30000;
const WATCHLIST_REFRESH_MS = 30000;
const PAPER_TRADING_REFRESH_MS = 30000; // 30 second fallback sync (WebSocket handles real-time)

// State machine types
type CryptoStatus = 'idle' | 'loading' | 'ready' | 'error';

interface CryptoState {
  status: CryptoStatus;
  error: string | null;
  tickerData: TickerData | null;
  orderBook: OrderBook;
  tradesData: TradeData[];
  watchlistPrices: Record<string, WatchlistPrice>;
  positions: Position[];
  orders: Order[];
  trades: any[];
  balance: number;
  equity: number;
  stats: any;
  availableSymbols: string[];
  isLoadingSymbols: boolean;
  lastOrderBookUpdate: number;
}

type CryptoAction =
  | { type: 'SET_STATUS'; status: CryptoStatus; error?: string }
  | { type: 'SET_TICKER'; data: TickerData | null }
  | { type: 'SET_ORDERBOOK'; data: OrderBook; timestamp: number }
  | { type: 'SET_TRADES'; data: TradeData[] }
  | { type: 'SET_WATCHLIST_PRICES'; prices: Record<string, WatchlistPrice> }
  | { type: 'SET_PAPER_DATA'; positions: Position[]; orders: Order[]; trades: any[]; balance: number; equity: number; stats: any }
  | { type: 'SET_SYMBOLS'; symbols: string[]; isLoading: boolean }
  | { type: 'RESET_DATA' }
  | { type: 'CLEAR_TICKER_TRADES' }
  | { type: 'UPDATE_POSITION_PRICES'; prices: Map<string, number> };

const initialState: CryptoState = {
  status: 'idle',
  error: null,
  tickerData: null,
  orderBook: { bids: [], asks: [], symbol: '' },
  tradesData: [],
  watchlistPrices: {},
  positions: [],
  orders: [],
  trades: [],
  balance: 0,
  equity: 0,
  stats: null,
  availableSymbols: [],
  isLoadingSymbols: false,
  lastOrderBookUpdate: 0,
};

function cryptoReducer(state: CryptoState, action: CryptoAction): CryptoState {
  switch (action.type) {
    case 'SET_STATUS':
      return { ...state, status: action.status, error: action.error || null };
    case 'SET_TICKER':
      return { ...state, tickerData: action.data, status: action.data ? 'ready' : state.status };
    case 'SET_ORDERBOOK':
      return { ...state, orderBook: action.data, lastOrderBookUpdate: action.timestamp };
    case 'SET_TRADES':
      return { ...state, tradesData: action.data };
    case 'SET_WATCHLIST_PRICES':
      return { ...state, watchlistPrices: action.prices };
    case 'SET_PAPER_DATA':
      return {
        ...state,
        positions: action.positions,
        orders: action.orders,
        trades: action.trades,
        balance: action.balance,
        equity: action.equity,
        stats: action.stats,
      };
    case 'SET_SYMBOLS':
      return { ...state, availableSymbols: action.symbols, isLoadingSymbols: action.isLoading };
    case 'RESET_DATA':
      return {
        ...state,
        tickerData: null,
        orderBook: { bids: [], asks: [], symbol: '' },
        tradesData: [],
        watchlistPrices: {},
        lastOrderBookUpdate: 0,
      };
    case 'CLEAR_TICKER_TRADES':
      return { ...state, tickerData: null, tradesData: [] };
    case 'UPDATE_POSITION_PRICES':
      return {
        ...state,
        positions: state.positions.map(pos => {
          const newPrice = action.prices.get(pos.symbol);
          if (!newPrice) return pos;

          const entryPrice = pos.entryPrice || 0;
          const quantity = pos.quantity || 0;

          // Recalculate P&L with new price
          let unrealizedPnl = 0;
          let pnlPercent = 0;

          if (newPrice > 0 && entryPrice > 0 && quantity > 0) {
            if (pos.side === 'long') {
              unrealizedPnl = (newPrice - entryPrice) * quantity;
            } else {
              unrealizedPnl = (entryPrice - newPrice) * quantity;
            }
            pnlPercent = ((newPrice - entryPrice) / entryPrice) * 100;
            if (pos.side === 'short') pnlPercent = -pnlPercent;
          }

          return {
            ...pos,
            currentPrice: newPrice,
            unrealizedPnl,
            pnlPercent,
            positionValue: newPrice * quantity,
          };
        }),
      };
    default:
      return state;
  }
}

export function CryptoTradingTab() {
  const { t } = useTranslation('trading');
  const {
    activeBroker,
    availableBrokers,
    setActiveBroker,
    tradingMode,
    setTradingMode,
    paperPortfolioMode,
    setPaperPortfolioMode,
    activeAdapter,
    realAdapter,
    paperAdapter,
    defaultSymbols,
    isConnecting,
    isLoading,
  } = useBrokerContext();

  // Refs for cleanup and race condition prevention
  const mountedRef = useRef(true);
  const fetchIdRef = useRef(0);
  const watchlistFetchIdRef = useRef(0);
  const paperDataFetchIdRef = useRef(0);

  // Main state reducer
  const [state, dispatch] = useReducer(cryptoReducer, initialState);

  // UI-only state (doesn't need reducer)
  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSymbol, setSelectedSymbol] = useState(defaultSymbols[0] || 'BTC/USD');
  const [searchQuery, setSearchQuery] = useState('');
  const [showSymbolDropdown, setShowSymbolDropdown] = useState(false);
  const [showBrokerDropdown, setShowBrokerDropdown] = useState(false);
  const [selectedView, setSelectedView] = useState<CenterViewType>('chart');
  const [activeBottomTab, setActiveBottomTab] = useState<BottomPanelTabType>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    selectedSymbol, selectedView, activeBottomTab, isBottomPanelMinimized,
  }), [selectedSymbol, selectedView, activeBottomTab, isBottomPanelMinimized]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.selectedSymbol === "string") setSelectedSymbol(state.selectedSymbol);
    if (typeof state.selectedView === "string") setSelectedView(state.selectedView as any);
    if (typeof state.activeBottomTab === "string") setActiveBottomTab(state.activeBottomTab as any);
    if (typeof state.isBottomPanelMinimized === "boolean") setIsBottomPanelMinimized(state.isBottomPanelMinimized);
  }, []);

  useWorkspaceTabState("crypto-trading", getWorkspaceState, setWorkspaceState);
  const [showSettings, setShowSettings] = useState(false);

  // Trading settings
  const [slippage, setSlippage] = useState(0);
  const [makerFee, setMakerFee] = useState(0.0002);
  const [takerFee, setTakerFee] = useState(0.0005);

  // Watchlist
  const [watchlist, setWatchlist] = useState<string[]>(DEFAULT_CRYPTO_WATCHLIST);

  // WebSocket provider initialization state
  const [wsProvidersReady, setWsProvidersReady] = useState(false);

  // Cleanup on unmount
  useEffect(() => {
    mountedRef.current = true;
    return () => {
      mountedRef.current = false;
    };
  }, []);

  // Initialize WebSocket providers (Kraken & HyperLiquid)
  useEffect(() => {
    const initializeProviders = async () => {
      if (!mountedRef.current) return;

      try {
        await withTimeout(
          invoke('ws_set_config', {
            config: {
              name: 'kraken',
              url: 'wss://ws.kraken.com/v2',
              enabled: true,
              reconnect_delay_ms: 5000,
              max_reconnect_attempts: 10,
              heartbeat_interval_ms: 30000,
            }
          }),
          API_TIMEOUT_MS,
          'Kraken WebSocket config timeout'
        );

        if (!mountedRef.current) return;

        await withTimeout(
          invoke('ws_set_config', {
            config: {
              name: 'hyperliquid',
              url: 'wss://api.hyperliquid.xyz/ws',
              enabled: true,
              reconnect_delay_ms: 5000,
              max_reconnect_attempts: 10,
              heartbeat_interval_ms: 30000,
            }
          }),
          API_TIMEOUT_MS,
          'HyperLiquid WebSocket config timeout'
        );
        // Mark providers as ready after config is set
        if (mountedRef.current) {
          setWsProvidersReady(true);
        }
      } catch (err) {
        // WebSocket config errors are non-fatal, log for debugging
        if (mountedRef.current) {
          console.warn('[CryptoTrading] WebSocket provider init warning:', err);
          // Still mark as ready to allow fallback behavior
          setWsProvidersReady(true);
        }
      }
    };

    initializeProviders();
  }, []);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Fetch watchlist prices via CoinGecko (fast, reliable, US-accessible)
  const fetchWatchlistPrices = useCallback(async () => {
    const currentFetchId = ++watchlistFetchIdRef.current;

    try {
      const symbolToCoinGeckoId: Record<string, string> = {
        'BTC/USD': 'bitcoin', 'ETH/USD': 'ethereum', 'BNB/USD': 'binancecoin',
        'SOL/USD': 'solana', 'XRP/USD': 'ripple', 'ADA/USD': 'cardano',
        'DOGE/USD': 'dogecoin', 'AVAX/USD': 'avalanche-2', 'DOT/USD': 'polkadot',
        'POL/USD': 'matic-network', 'LTC/USD': 'litecoin', 'SHIB/USD': 'shiba-inu',
        'TRX/USD': 'tron', 'LINK/USD': 'chainlink', 'UNI/USD': 'uniswap',
        'ATOM/USD': 'cosmos', 'XLM/USD': 'stellar', 'ETC/USD': 'ethereum-classic',
        'BCH/USD': 'bitcoin-cash', 'NEAR/USD': 'near', 'APT/USD': 'aptos',
        'ARB/USD': 'arbitrum', 'OP/USD': 'optimism', 'LDO/USD': 'lido-dao',
        'FIL/USD': 'filecoin', 'ICP/USD': 'internet-computer', 'INJ/USD': 'injective-protocol',
        'STX/USD': 'blockstack', 'MKR/USD': 'maker', 'AAVE/USD': 'aave',
      };

      const coinIds = watchlist
        .map(s => symbolToCoinGeckoId[s])
        .filter(Boolean)
        .join(',');

      try {
        const response = await tauriFetch(
          `https://api.coingecko.com/api/v3/simple/price?ids=${coinIds}&vs_currencies=usd&include_24hr_change=true`
        );

        if (!mountedRef.current || currentFetchId !== watchlistFetchIdRef.current) return;

        if (!response.ok) {
          throw new Error(`CoinGecko API returned ${response.status}`);
        }

        const data = await response.json();
        const prices: Record<string, WatchlistPrice> = {};
        let hasValidData = false;

        watchlist.forEach(symbol => {
          const coinId = symbolToCoinGeckoId[symbol];
          const coinData = coinId && data[coinId];

          if (coinData && coinData.usd > 0) {
            prices[symbol] = {
              price: coinData.usd,
              change: coinData.usd_24h_change || 0,
            };
            hasValidData = true;
          }
        });

        if (hasValidData && mountedRef.current && currentFetchId === watchlistFetchIdRef.current) {
          dispatch({ type: 'SET_WATCHLIST_PRICES', prices });
        }
      } catch (fetchErr) {
        if (!mountedRef.current || currentFetchId !== watchlistFetchIdRef.current) return;
        console.warn('[CryptoTrading] CoinGecko API fetch failed:', fetchErr);
      }
    } catch (error) {
      if (!mountedRef.current || currentFetchId !== watchlistFetchIdRef.current) return;
      console.error('[CryptoTrading] Failed to fetch prices:', error);
    }
  }, [watchlist]);

  useEffect(() => {
    fetchWatchlistPrices();
    const interval = setInterval(fetchWatchlistPrices, WATCHLIST_REFRESH_MS);
    return () => clearInterval(interval);
  }, [fetchWatchlistPrices]);

  // Clear data when symbol changes
  useEffect(() => {
    dispatch({ type: 'CLEAR_TICKER_TRADES' });
  }, [selectedSymbol]);

  // Reset data when broker changes
  useEffect(() => {
    dispatch({ type: 'RESET_DATA' });

    if (defaultSymbols.length > 0) {
      setSelectedSymbol(defaultSymbols[0]);
    }
  }, [activeBroker, defaultSymbols]);

  // Subscribe to Rust WebSocket feeds (only after providers are configured)
  const shouldConnectWs = !!activeBroker && wsProvidersReady;

  const { data: tickerData, error: tickerError } = useRustTicker(
    activeBroker || '',
    selectedSymbol,
    shouldConnectWs
  );

  const { data: orderbookData, error: orderbookError } = useRustOrderBook(
    activeBroker || '',
    selectedSymbol,
    25,
    shouldConnectWs
  );

  const { trades: tradesData, error: tradesError } = useRustTrades(
    activeBroker || '',
    selectedSymbol,
    50,
    shouldConnectWs
  );

  // Update ticker data from Rust WebSocket
  useEffect(() => {
    if (!tickerData || !mountedRef.current) return;

    dispatch({
      type: 'SET_TICKER',
      data: {
        symbol: tickerData.symbol,
        price: tickerData.price,
        bid: tickerData.bid,
        ask: tickerData.ask,
        last: tickerData.price,
        volume: tickerData.volume,
        high: tickerData.high,
        low: tickerData.low,
        change: tickerData.change,
        changePercent: tickerData.change_percent,
        open: tickerData.open,
      }
    });

    if (activeBroker && tickerData.price > 0) {
      const marketDataService = getMarketDataService();
      marketDataService.updatePrice(activeBroker, selectedSymbol, {
        price: tickerData.price,
        bid: tickerData.bid,
        ask: tickerData.ask,
        volume: tickerData.volume,
        high: tickerData.high,
        low: tickerData.low,
        change: tickerData.change,
        change_percent: tickerData.change_percent,
        timestamp: tickerData.timestamp,
      });
    }
  }, [tickerData, activeBroker, selectedSymbol]);

  // Update orderbook from Rust WebSocket
  useEffect(() => {
    if (!orderbookData || !mountedRef.current) return;

    dispatch({
      type: 'SET_ORDERBOOK',
      data: {
        bids: orderbookData.bids.map(b => ({ price: b.price, size: b.quantity })),
        asks: orderbookData.asks.map(a => ({ price: a.price, size: a.quantity })),
        symbol: orderbookData.symbol,
      },
      timestamp: Date.now()
    });
  }, [orderbookData]);

  // Update trades from Rust WebSocket
  useEffect(() => {
    if (!tradesData || tradesData.length === 0 || !mountedRef.current) return;

    dispatch({
      type: 'SET_TRADES',
      data: tradesData.map(t => ({
        symbol: t.symbol,
        price: t.price,
        quantity: t.quantity,
        side: t.side,
        timestamp: t.timestamp,
      }))
    });
  }, [tradesData]);

  // Load paper trading data with race condition prevention
  const loadPaperTradingData = useCallback(async () => {
    if (tradingMode !== 'paper' || !paperAdapter || !mountedRef.current) return;

    const currentFetchId = ++paperDataFetchIdRef.current;

    try {
      const balanceData = await withTimeout(
        paperAdapter.fetchBalance(),
        API_TIMEOUT_MS,
        'Fetch balance timeout'
      );

      if (!mountedRef.current || currentFetchId !== paperDataFetchIdRef.current) return;

      const usdBalance = (balanceData.free as any)?.USD || 0;
      const totalEquity = (balanceData.total as any)?.USD || 0;

      const positionsData = await withTimeout(
        paperAdapter.fetchPositions(),
        API_TIMEOUT_MS,
        'Fetch positions timeout'
      );

      if (!mountedRef.current || currentFetchId !== paperDataFetchIdRef.current) return;

      const marketDataService = getMarketDataService();
      const mappedPositions: Position[] = positionsData.map((p: any) => {
        const livePrice = marketDataService.getCurrentPrice(p.symbol, activeBroker || undefined);
        const currentPrice = livePrice?.last || p.markPrice || p.current_price || 0;
        const entryPrice = p.entryPrice || p.entry_price || 0;
        const quantity = p.contracts || p.quantity || 0;

        let unrealizedPnl = p.unrealizedPnl || p.unrealized_pnl || 0;
        let pnlPercent = p.percentage || 0;

        if (currentPrice > 0 && entryPrice > 0 && quantity > 0) {
          if (p.side === 'long') {
            unrealizedPnl = (currentPrice - entryPrice) * quantity;
          } else {
            unrealizedPnl = (entryPrice - currentPrice) * quantity;
          }
          pnlPercent = ((currentPrice - entryPrice) / entryPrice) * 100;
          if (p.side === 'short') pnlPercent = -pnlPercent;
        }

        return {
          symbol: p.symbol,
          side: p.side,
          quantity: quantity,
          entryPrice: entryPrice,
          positionValue: p.info?.positionValue || (currentPrice * quantity),
          currentPrice: currentPrice,
          unrealizedPnl: unrealizedPnl,
          pnlPercent: pnlPercent,
        };
      });

      const ordersData = await withTimeout(
        paperAdapter.fetchOpenOrders(),
        API_TIMEOUT_MS,
        'Fetch orders timeout'
      );

      if (!mountedRef.current || currentFetchId !== paperDataFetchIdRef.current) return;

      const mappedOrders: Order[] = ordersData.map((o: any) => ({
        id: o.id,
        symbol: o.symbol,
        side: o.side,
        type: o.type,
        quantity: o.amount,
        price: o.price,
        status: o.status,
        createdAt: o.datetime,
      }));

      const myTradesData = await withTimeout(
        paperAdapter.fetchMyTrades(undefined, undefined, 50),
        API_TIMEOUT_MS,
        'Fetch trades timeout'
      );

      if (!mountedRef.current || currentFetchId !== paperDataFetchIdRef.current) return;

      let statsData = null;
      if (typeof (paperAdapter as any).getStatistics === 'function') {
        try {
          statsData = await withTimeout(
            (paperAdapter as any).getStatistics(),
            API_TIMEOUT_MS,
            'Fetch stats timeout'
          );
        } catch {
          // Stats are optional
        }
      }

      if (mountedRef.current && currentFetchId === paperDataFetchIdRef.current) {
        dispatch({
          type: 'SET_PAPER_DATA',
          positions: mappedPositions,
          orders: mappedOrders,
          trades: myTradesData,
          balance: usdBalance,
          equity: totalEquity,
          stats: statsData,
        });
      }
    } catch (error) {
      if (!mountedRef.current || currentFetchId !== paperDataFetchIdRef.current) return;
      console.error('[CryptoTrading] Failed to load paper trading data:', error);
    }
  }, [tradingMode, paperAdapter, activeBroker]);

  // Normalize symbol for comparison (removes slashes, dashes, converts to uppercase)
  const normalizeSymbol = (symbol: string): string => {
    return symbol.replace(/[/\-_]/g, '').toUpperCase();
  };

  // Real-time WebSocket listeners for positions
  useEffect(() => {
    let mounted = true;
    const unlisteners: (() => void)[] = [];

    const setupWebSocketListeners = async () => {
      // 1. Price updates (ws_ticker)
      const tickerUnlisten = await listen<{ provider: string; symbol: string; price: number }>('ws_ticker', (event) => {
        if (!mounted) return;

        const { symbol, price } = event.payload;
        const normalizedEventSymbol = normalizeSymbol(symbol);

        // Find position that matches this symbol (with normalization)
        const matchingPosition = state.positions.find(p => normalizeSymbol(p.symbol) === normalizedEventSymbol);
        if (!matchingPosition) return;

        // Update position prices using the position's original symbol format
        const priceMap = new Map<string, number>();
        priceMap.set(matchingPosition.symbol, price);

        dispatch({ type: 'UPDATE_POSITION_PRICES', prices: priceMap });
      });
      unlisteners.push(tickerUnlisten);

      // Note: ws_trade events are market trades (all trades on the exchange)
      // NOT the user's paper trading fills. User trades come from loadPaperTradingData.
      // We don't need to listen to ws_trade here as it would incorrectly add
      // market trades to the user's trade history.
    };

    setupWebSocketListeners();

    return () => {
      mounted = false;
      unlisteners.forEach(unlisten => unlisten());
    };
  }, [state.positions]);

  useEffect(() => {
    loadPaperTradingData();
    const interval = setInterval(loadPaperTradingData, PAPER_TRADING_REFRESH_MS);
    return () => clearInterval(interval);
  }, [loadPaperTradingData]);

  // Update slippage & fees
  useEffect(() => {
    if (paperAdapter && 'paperConfig' in paperAdapter) {
      const config = (paperAdapter as any).paperConfig;
      if (config && config.slippage && config.fees) {
        config.slippage.market = slippage;
        config.fees.maker = makerFee;
        config.fees.taker = takerFee;
      }
    }
  }, [slippage, makerFee, takerFee, paperAdapter]);

  // Fetch markets with timeout
  useEffect(() => {
    const fetchMarkets = async () => {
      const currentFetchId = ++fetchIdRef.current;

      if (!realAdapter || !realAdapter.isConnected()) {
        if (mountedRef.current) {
          dispatch({ type: 'SET_SYMBOLS', symbols: defaultSymbols, isLoading: false });
        }
        return;
      }

      dispatch({ type: 'SET_SYMBOLS', symbols: state.availableSymbols, isLoading: true });

      try {
        const markets = await withTimeout(
          realAdapter.fetchMarkets(),
          API_TIMEOUT_MS,
          'Fetch markets timeout'
        );

        if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;

        const symbols = markets
          .filter((m: any) => m.active && m.spot && (m.quote === 'USD' || m.quote === 'USDT' || m.quote === 'USDC'))
          .map((m: any) => m.symbol)
          .sort();

        dispatch({
          type: 'SET_SYMBOLS',
          symbols: symbols.length > 0 ? symbols : defaultSymbols,
          isLoading: false
        });
      } catch (error) {
        if (!mountedRef.current || currentFetchId !== fetchIdRef.current) return;
        console.error('[CryptoTrading] Failed to fetch markets:', error);
        dispatch({ type: 'SET_SYMBOLS', symbols: defaultSymbols, isLoading: false });
      }
    };

    fetchMarkets();
  }, [realAdapter, defaultSymbols]);

  // Handle broker change
  const handleBrokerChange = useCallback(async (brokerId: string) => {
    setShowBrokerDropdown(false);
    await setActiveBroker(brokerId);
  }, [setActiveBroker]);

  // Handle symbol selection with input validation
  const handleSymbolSelect = useCallback((symbol: string) => {
    const sanitized = sanitizeInput(symbol);
    if (sanitized) {
      setSelectedSymbol(sanitized);
    }
    setShowSymbolDropdown(false);
  }, []);

  // Handle search query change with sanitization
  const handleSearchQueryChange = useCallback((query: string) => {
    setSearchQuery(sanitizeInput(query));
  }, []);

  // Handle reset portfolio
  const handleResetPortfolio = useCallback(async () => {
    if (!paperAdapter || !('portfolioId' in paperAdapter)) return;

    try {
      const { resetPortfolio } = await import('@/paper-trading');
      await resetPortfolio((paperAdapter as any).portfolioId);
      // Reload paper trading data after reset
      loadPaperTradingData();
    } catch (error) {
      console.error('[CryptoTrading] Failed to reset portfolio:', error);
    }
  }, [paperAdapter, loadPaperTradingData]);

  const currentPrice = state.tickerData?.last || state.tickerData?.price || state.orderBook.asks[0]?.price || 0;

  // Calculate price change and percentage
  let priceChange = 0;
  let priceChangePercent = 0;

  if (state.tickerData) {
    priceChange = state.tickerData.change || 0;
    priceChangePercent = state.tickerData.changePercent || 0;

    if (priceChange === 0 && priceChangePercent === 0) {
      const last = state.tickerData.last || state.tickerData.price || 0;
      const open = state.tickerData.open || 0;

      if (last > 0 && open > 0) {
        priceChange = last - open;
        priceChangePercent = (priceChange / open) * 100;
      }
    }
  }

  // Fallback: use watchlist data
  if (priceChange === 0 && priceChangePercent === 0 && state.watchlistPrices[selectedSymbol]) {
    priceChangePercent = state.watchlistPrices[selectedSymbol].change;
    if (currentPrice > 0) {
      priceChange = (priceChangePercent / 100) * currentPrice / (1 + priceChangePercent / 100);
    }
  }

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
      <style>{CRYPTO_TERMINAL_STYLES}</style>

      {/* Top Navigation Bar */}
      <ErrorBoundary name="CryptoTopNav" variant="minimal">
        <CryptoTopNav
          activeBroker={activeBroker}
          availableBrokers={availableBrokers}
          tradingMode={tradingMode}
          isConnected={activeAdapter?.isConnected() || false}
          showBrokerDropdown={showBrokerDropdown}
          showSettings={showSettings}
          onBrokerDropdownToggle={() => setShowBrokerDropdown(!showBrokerDropdown)}
          onBrokerChange={handleBrokerChange}
          onTradingModeChange={setTradingMode}
          onSettingsToggle={() => setShowSettings(!showSettings)}
        />
      </ErrorBoundary>

      {/* Ticker Bar */}
      <ErrorBoundary name="CryptoTickerBar" variant="minimal">
        <CryptoTickerBar
          selectedSymbol={selectedSymbol}
          tickerData={state.tickerData}
          availableSymbols={state.availableSymbols}
          searchQuery={searchQuery}
          showSymbolDropdown={showSymbolDropdown}
          balance={state.balance}
          equity={state.equity}
          tradingMode={tradingMode}
          priceChange={priceChange}
          priceChangePercent={priceChangePercent}
          onSymbolSelect={handleSymbolSelect}
          onSearchQueryChange={handleSearchQueryChange}
          onSymbolDropdownToggle={() => setShowSymbolDropdown(!showSymbolDropdown)}
        />
      </ErrorBoundary>

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Sidebar - Watchlist */}
        <ErrorBoundary name="CryptoWatchlist" variant="minimal">
          <CryptoWatchlist
            watchlist={watchlist}
            watchlistPrices={state.watchlistPrices}
            selectedSymbol={selectedSymbol}
            onSymbolSelect={handleSymbolSelect}
          />
        </ErrorBoundary>

        {/* Center - Chart & Bottom Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Chart Area */}
          <ErrorBoundary name="CryptoChartArea" variant="minimal">
            <CryptoChartArea
              selectedSymbol={selectedSymbol}
              selectedView={selectedView}
              tradesData={state.tradesData}
              isBottomPanelMinimized={isBottomPanelMinimized}
              currentPrice={currentPrice}
              activeBroker={activeBroker}
              isPaper={tradingMode === 'paper'}
              onViewChange={setSelectedView}
            />
          </ErrorBoundary>

          {/* Bottom Panel */}
          <ErrorBoundary name="CryptoBottomPanel" variant="minimal">
            <CryptoBottomPanel
              activeTab={activeBottomTab}
              positions={state.positions}
              orders={state.orders}
              trades={state.trades}
              isMinimized={isBottomPanelMinimized}
              activeBroker={activeBroker}
              selectedSymbol={selectedSymbol}
              tickerData={state.tickerData}
              realAdapter={realAdapter}
              onTabChange={setActiveBottomTab}
              onMinimizeToggle={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
            />
          </ErrorBoundary>
        </div>

        {/* Right Sidebar - Order Entry & Order Book */}
        <div style={{
          width: '300px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderLeft: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Order Entry */}
          <ErrorBoundary name="CryptoOrderEntry" variant="minimal">
            <CryptoOrderEntry
              selectedSymbol={selectedSymbol}
              currentPrice={currentPrice}
              balance={state.balance}
              positions={state.positions}
              tradingMode={tradingMode}
              activeBroker={activeBroker}
              isLoading={isLoading}
              isConnecting={isConnecting}
              paperAdapter={paperAdapter}
              realAdapter={realAdapter}
              onTradingModeChange={setTradingMode}
              onOrderPlaced={loadPaperTradingData}
            />
          </ErrorBoundary>

          {/* Order Book - subscribes directly to WebSocket for flicker-free updates */}
          <ErrorBoundary name="CryptoOrderBook" variant="minimal">
            <CryptoOrderBook
              currentPrice={currentPrice}
              selectedSymbol={selectedSymbol}
              activeBroker={activeBroker}
            />
          </ErrorBoundary>
        </div>
      </div>

      {/* Status Bar */}
      <ErrorBoundary name="CryptoStatusBar" variant="minimal">
        <CryptoStatusBar
          activeBroker={activeBroker}
          tradingMode={tradingMode}
          isConnected={activeAdapter?.isConnected() || false}
        />
      </ErrorBoundary>

      {/* Settings Panel */}
      <CryptoSettingsPanel
        isOpen={showSettings}
        onClose={() => setShowSettings(false)}
        slippage={slippage}
        makerFee={makerFee}
        takerFee={takerFee}
        onSlippageChange={setSlippage}
        onMakerFeeChange={setMakerFee}
        onTakerFeeChange={setTakerFee}
        onResetPortfolio={handleResetPortfolio}
        tradingMode={tradingMode}
      />
    </div>
  );
}

export default CryptoTradingTab;
