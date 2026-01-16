// File: src/components/tabs/TradingTab.tsx
// Professional Bloomberg Terminal-Grade Trading Interface

import React, { useState, useEffect, useCallback, memo, useMemo } from 'react';
import {
  TrendingUp, TrendingDown, Activity, DollarSign, BarChart3,
  Settings as SettingsIcon, Globe, Wifi, WifiOff, Bell,
  Zap, Target, AlertCircle, RefreshCw, Maximize2, ChevronDown,
  Clock, TrendingDown as ArrowDown, TrendingUp as ArrowUp, ChevronUp, Minimize2
} from 'lucide-react';

import { useBrokerContext } from '../../contexts/BrokerContext';
import { useRustTicker, useRustOrderBook, useRustTrades } from '../../hooks/useRustWebSocket';
import { invoke } from '@tauri-apps/api/core';
import { getMarketDataService } from '../../services/trading/UnifiedMarketDataService';
// Import new enhanced order form
import { EnhancedOrderForm } from './trading/core/OrderForm';
import { HyperLiquidVaultManager } from './trading/exchange-specific/hyperliquid';
import { StakingPanel, FuturesPanel } from './trading/exchange-specific/kraken';
import { PositionsTable } from './trading/core/PositionManager';
import { OrdersTable, ClosedOrders } from './trading/core/OrderManager';
import { TradingChart, DepthChart, VolumeProfile } from './trading/charts';
import { AccountStats, FeesDisplay, MarginPanel } from './trading/core/AccountInfo';
import { PortfolioAggregator, ArbitrageDetector } from './trading/cross-exchange';
import { TimezoneSelector } from '../common/TimezoneSelector';
import { AIAgentsPanel } from './trading/ai-agents/AIAgentsPanel';
import { ModelChatPanel } from './trading/ai-agents/ModelChatPanel';
import { LeaderboardPanel } from './trading/ai-agents/LeaderboardPanel';
import type { OrderRequest } from '../../types/trading';
import { useTranslation } from 'react-i18next';

interface OrderBookLevel {
  price: number;
  size: number;
}

interface OrderBook {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  symbol: string;
  checksum?: number;
}

interface Position {
  symbol: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  positionValue: number;
  currentPrice: number;
  unrealizedPnl: number;
  pnlPercent: number;
}

interface Order {
  id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: string;
  quantity: number;
  price?: number;
  status: string;
  createdAt: string;
}

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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

export function TradingTab() {
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

  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSymbol, setSelectedSymbol] = useState(defaultSymbols[0] || 'BTC/USD');
  const [tickerState, setTickerData] = useState<any>(null);
  const [orderBook, setOrderBook] = useState<OrderBook>({ bids: [], asks: [], symbol: '' });
  const [tradesState, setTradesData] = useState<any[]>([]);
  const [isPlacingOrder, setIsPlacingOrder] = useState(false);
  const [lastOrderBookUpdate, setLastOrderBookUpdate] = useState<number>(0);

  // UI State
  const [searchQuery, setSearchQuery] = useState('');
  const [availableSymbols, setAvailableSymbols] = useState<string[]>([]);
  const [isLoadingSymbols, setIsLoadingSymbols] = useState(false);
  const [showSymbolDropdown, setShowSymbolDropdown] = useState(false);
  const [showBrokerDropdown, setShowBrokerDropdown] = useState(false);
  const [selectedView, setSelectedView] = useState<'chart' | 'depth' | 'trades'>('chart');
  const [rightPanelView, setRightPanelView] = useState<'orderbook' | 'volume' | 'modelchat'>('orderbook');
  const [leftSidebarView, setLeftSidebarView] = useState<'watchlist' | 'ai-agents' | 'leaderboard'>('watchlist');

  // Paper trading state
  const [positions, setPositions] = useState<Position[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [trades, setTrades] = useState<any[]>([]);
  const [balance, setBalance] = useState(0);
  const [equity, setEquity] = useState(0);
  const [stats, setStats] = useState<any>(null);
  const [activeBottomTab, setActiveBottomTab] = useState<'positions' | 'orders' | 'history' | 'trades' | 'stats' | 'features' | 'cross-exchange'>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);

  // Trading settings
  const [slippage, setSlippage] = useState(0);
  const [makerFee, setMakerFee] = useState(0.0002);
  const [takerFee, setTakerFee] = useState(0.0005);
  const [showSettings, setShowSettings] = useState(false);

  // Watchlist
  // Top 30 most traded crypto pairs
  const [watchlist, setWatchlist] = useState<string[]>([
    'BTC/USD', 'ETH/USD', 'BNB/USD', 'SOL/USD', 'XRP/USD',
    'ADA/USD', 'DOGE/USD', 'AVAX/USD', 'DOT/USD', 'MATIC/USD',
    'LTC/USD', 'SHIB/USD', 'TRX/USD', 'LINK/USD', 'UNI/USD',
    'ATOM/USD', 'XLM/USD', 'ETC/USD', 'BCH/USD', 'NEAR/USD',
    'APT/USD', 'ARB/USD', 'OP/USD', 'LDO/USD', 'FIL/USD',
    'ICP/USD', 'INJ/USD', 'STX/USD', 'MKR/USD', 'AAVE/USD'
  ]);

  // Watchlist prices
  const [watchlistPrices, setWatchlistPrices] = useState<Record<string, { price: number; change: number }>>({});

  // Initialize WebSocket providers (Kraken & HyperLiquid)
  useEffect(() => {
    const initializeProviders = async () => {
      try {
        // Initialize Kraken
        await invoke('ws_set_config', {
          config: {
            name: 'kraken',
            url: 'wss://ws.kraken.com/v2',
            enabled: true,
            reconnect_delay_ms: 5000,
            max_reconnect_attempts: 10,
            heartbeat_interval_ms: 30000,
          }
        });

        // Initialize HyperLiquid
        await invoke('ws_set_config', {
          config: {
            name: 'hyperliquid',
            url: 'wss://api.hyperliquid.xyz/ws',
            enabled: true,
            reconnect_delay_ms: 5000,
            max_reconnect_attempts: 10,
            heartbeat_interval_ms: 30000,
          }
        });
      } catch (err) {
        // Ignore config errors
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

  // Fetch watchlist prices - Use realAdapter for exchange data (works in both paper and live modes)
  // NOTE: This batches all requests into one API call for optimal performance
  useEffect(() => {
    const fetchWatchlistPrices = async () => {
      try {
        // Approach 1: Try using realAdapter's bulk fetchTickers (works in paper mode too)
        if (realAdapter?.isConnected() && typeof realAdapter.fetchTickers === 'function') {
          try {
            const tickers = await realAdapter.fetchTickers(watchlist);
            const prices: Record<string, { price: number; change: number }> = {};

            let hasValidData = false;
            watchlist.forEach(symbol => {
              const ticker = tickers[symbol];
              if (ticker && ticker.last && ticker.last > 0) {
                prices[symbol] = {
                  price: ticker.last,
                  change: ticker.percentage || 0
                };
                hasValidData = true;
              }
            });

            // Only update if we got valid data
            if (hasValidData) {
              setWatchlistPrices(prices);
              console.log('[Watchlist] Successfully fetched prices from realAdapter');
              return;
            }
          } catch (err) {
            console.warn('[Watchlist] fetchTickers failed, trying fallback API:', err);
          }
        }

        // Approach 2: Fallback to public Binance API for crypto prices
        const symbols = watchlist.map(s => s.replace('/USD', 'USDT').replace('/', ''));
        const response = await fetch('https://api.binance.com/api/v3/ticker/24hr');

        if (!response.ok) {
          throw new Error(`Binance API returned ${response.status}`);
        }

        const allTickers = await response.json();

        if (!Array.isArray(allTickers)) {
          throw new Error('Invalid response from Binance API');
        }

        const prices: Record<string, { price: number; change: number }> = {};
        let hasValidData = false;

        watchlist.forEach((symbol, idx) => {
          const binanceSymbol = symbols[idx];
          const ticker = allTickers.find((t: any) => t.symbol === binanceSymbol);

          if (ticker && ticker.lastPrice) {
            const price = parseFloat(ticker.lastPrice);
            const change = parseFloat(ticker.priceChangePercent);

            if (price > 0) {
              prices[symbol] = { price, change };
              hasValidData = true;
            }
          }
        });

        // Only update if we got valid data
        if (hasValidData) {
          setWatchlistPrices(prices);
          console.log('[Watchlist] Successfully fetched prices from Binance');
        } else {
          console.warn('[Watchlist] No valid data received, keeping existing prices');
        }
      } catch (error) {
        console.error('[Watchlist] Failed to fetch prices:', error);
        // DON'T clear existing data on error - keep what we have
        console.warn('[Watchlist] Keeping existing prices due to fetch error');
      }
    };

    // Initial fetch
    fetchWatchlistPrices();

    // Update every 30 seconds for more responsive watchlist
    const interval = setInterval(fetchWatchlistPrices, 30000);
    return () => clearInterval(interval);
  }, [realAdapter, watchlist]);

  // Clear data when symbol changes - but don't clear orderbook immediately
  useEffect(() => {
    setTickerData(null);
    setTradesData([]);
    // Don't clear orderbook - let WebSocket update it
  }, [selectedSymbol]);

  // Reset data when broker changes
  useEffect(() => {
    // Clear all market data when broker switches
    setTickerData(null);
    setOrderBook({ bids: [], asks: [], symbol: '' });
    setTradesData([]);
    setWatchlistPrices({});
    setLastOrderBookUpdate(0);

    // Reset symbol to default for new broker
    if (defaultSymbols.length > 0) {
      setSelectedSymbol(defaultSymbols[0]);
    }
  }, [activeBroker, defaultSymbols]);

  // Subscribe to Rust WebSocket feeds (10,000+ connections, minimal memory)
  const { data: tickerData, error: tickerError } = useRustTicker(
    activeBroker || '',
    selectedSymbol,
    !!activeBroker
  );

  // Use Rust WebSocket for orderbook (primary source of real-time data)
  const { data: orderbookData, error: orderbookError } = useRustOrderBook(
    activeBroker || '',
    selectedSymbol,
    25,
    !!activeBroker
  );

  const { trades: tradesData, error: tradesError } = useRustTrades(
    activeBroker || '',
    selectedSymbol,
    50,
    !!activeBroker
  );

  // Update ticker data from Rust WebSocket
  // Also feed into UnifiedMarketDataService for paper trading
  useEffect(() => {
    if (!tickerData) return;

    setTickerData({
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
    });

    // Feed price into UnifiedMarketDataService for paper trading
    // This keeps positions updated and allows order matching
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
    if (!orderbookData) return;

    setOrderBook({
      bids: orderbookData.bids.map(b => ({ price: b.price, size: b.quantity })),
      asks: orderbookData.asks.map(a => ({ price: a.price, size: a.quantity })),
      symbol: orderbookData.symbol,
    });
    setLastOrderBookUpdate(Date.now());
  }, [orderbookData]);

  // Update trades from Rust WebSocket (auto-maintains 50 trades)
  useEffect(() => {
    if (tradesData && tradesData.length > 0) {
      setTradesData(tradesData.map(t => ({
        symbol: t.symbol,
        price: t.price,
        quantity: t.quantity,
        side: t.side,
        timestamp: t.timestamp,
      })));
    }
  }, [tradesData]);

  // Load paper trading data
  useEffect(() => {
    const loadPaperTradingData = async () => {
      if (tradingMode !== 'paper' || !paperAdapter) return;

      try {
        const balanceData = await paperAdapter.fetchBalance();
        const usdBalance = (balanceData.free as any)?.USD || 0;
        const totalEquity = (balanceData.total as any)?.USD || 0;
        setBalance(usdBalance);
        setEquity(totalEquity);

        const positionsData = await paperAdapter.fetchPositions();
        const marketDataService = getMarketDataService();

        const mappedPositions: Position[] = positionsData.map((p: any) => {
          // Get live price from market data service
          const livePrice = marketDataService.getCurrentPrice(p.symbol, activeBroker || undefined);
          const currentPrice = livePrice?.last || p.markPrice || p.current_price || 0;
          const entryPrice = p.entryPrice || p.entry_price || 0;
          const quantity = p.contracts || p.quantity || 0;

          // Recalculate P&L with live price
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
        setPositions(mappedPositions);

        const ordersData = await paperAdapter.fetchOpenOrders();
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
        setOrders(mappedOrders);

        const tradesData = await paperAdapter.fetchMyTrades(undefined, undefined, 50);
        setTrades(tradesData);

        if (typeof (paperAdapter as any).getStatistics === 'function') {
          const statsData = await (paperAdapter as any).getStatistics();
          setStats(statsData);
        }
      } catch (error) {
        console.error('[TradingTab] Failed to load paper trading data:', error);
      }
    };

    loadPaperTradingData();
    // Refresh every 1 second for live position updates
    const intervalRef = { current: null as NodeJS.Timeout | null };
    intervalRef.current = setInterval(loadPaperTradingData, 1000);
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [tradingMode, paperAdapter, activeBroker]);

  // Update slippage & fees - use proper adapter methods instead of direct mutation
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

  // Fetch markets - use realAdapter to get exchange symbols (works in both paper and live modes)
  useEffect(() => {
    const fetchMarkets = async () => {
      if (!realAdapter || !realAdapter.isConnected()) {
        setAvailableSymbols(defaultSymbols);
        return;
      }
      setIsLoadingSymbols(true);
      try {
        const markets = await realAdapter.fetchMarkets();
        const symbols = markets
          .filter((m: any) => m.active && m.spot && (m.quote === 'USD' || m.quote === 'USDT' || m.quote === 'USDC'))
          .map((m: any) => m.symbol)
          .sort();
        setAvailableSymbols(symbols.length > 0 ? symbols : defaultSymbols);
      } catch (error) {
        console.error('[TradingTab] Failed to fetch markets:', error);
        setAvailableSymbols(defaultSymbols);
      } finally {
        setIsLoadingSymbols(false);
      }
    };
    fetchMarkets();
  }, [realAdapter, defaultSymbols]);

  // Handle order placement - re-validate adapter before each async operation
  const handlePlaceOrder = useCallback(async (orderRequest: OrderRequest) => {
    // Initial validation
    if (!paperAdapter || !paperAdapter.isConnected()) {
      alert('Paper trading adapter not available');
      throw new Error('Adapter not available');
    }

    setIsPlacingOrder(true);
    try {
      // Create order
      const order = await paperAdapter.createOrder(
        orderRequest.symbol,
        orderRequest.type,
        orderRequest.side,
        orderRequest.quantity,
        orderRequest.price,
        { stopPrice: orderRequest.stopPrice }
      );

      // Re-validate adapter is still connected after async operation
      if (!paperAdapter || !paperAdapter.isConnected()) {
        throw new Error('Adapter disconnected during order placement');
      }

      // Fetch updated orders
      const ordersData = await paperAdapter.fetchOpenOrders();
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
      setOrders(mappedOrders);

      // Re-validate again before fetching balance
      if (!paperAdapter || !paperAdapter.isConnected()) {
        throw new Error('Adapter disconnected during order refresh');
      }

      // Fetch updated balance
      const balanceData = await paperAdapter.fetchBalance();
      const usdBalance = (balanceData.free as any)?.USD || 0;
      setBalance(usdBalance);

      alert(`[OK] Order placed: ${order.id}`);
    } catch (error) {
      alert(`[FAIL] Order failed: ${(error as Error).message}`);
      throw error;
    } finally {
      setIsPlacingOrder(false);
    }
  }, [paperAdapter]);

  // Handle order cancellation
  const handleCancelOrder = useCallback(async (orderId: string, symbol: string) => {
    if (!paperAdapter) return;
    try {
      await paperAdapter.cancelOrder(orderId, symbol);
      const ordersData = await paperAdapter.fetchOpenOrders();
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
      setOrders(mappedOrders);
    } catch (error) {
      console.error('Cancel failed:', error);
    }
  }, [paperAdapter]);

  // Handle broker change
  const handleBrokerChange = async (brokerId: string) => {
    setShowBrokerDropdown(false);
    await setActiveBroker(brokerId);
  };

  const currentPrice = tickerState?.last || tickerState?.price || orderBook.asks[0]?.price || 0;

  // Calculate price change and percentage - use 24h open price from watchlist as fallback
  let priceChange = 0;
  let priceChangePercent = 0;

  if (tickerState) {
    // Try direct fields first
    priceChange = tickerState.change || 0;
    priceChangePercent = tickerState.changePercent || tickerState.change_percent || 0;

    // If not available, calculate from open/last
    if (priceChange === 0 && priceChangePercent === 0) {
      const last = tickerState.last || tickerState.price || 0;
      const open = tickerState.open || 0;

      if (last > 0 && open > 0) {
        priceChange = last - open;
        priceChangePercent = (priceChange / open) * 100;
      }
    }
  }

  // Fallback: use watchlist data if ticker doesn't have change info
  if (priceChange === 0 && priceChangePercent === 0 && watchlistPrices[selectedSymbol]) {
    priceChangePercent = watchlistPrices[selectedSymbol].change;
    if (currentPrice > 0) {
      priceChange = (priceChangePercent / 100) * currentPrice / (1 + priceChangePercent / 100);
    }
  }

  // Calculate 24h price range (High - Low)
  const spread24h = (tickerState?.high && tickerState?.low) ? (tickerState.high - tickerState.low) : 0;
  const spread24hPercent = (spread24h > 0 && tickerState?.low && tickerState.low > 0)
    ? ((spread24h / tickerState.low) * 100)
    : 0;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        @import url('https://fonts.googleapis.com/css2?family=IBM+Plex+Mono:wght@400;500;600;700&display=swap');

        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${BLOOMBERG.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${BLOOMBERG.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${BLOOMBERG.MUTED}; }

        .terminal-glow {
          text-shadow: 0 0 10px ${BLOOMBERG.ORANGE}40;
        }

        .price-flash {
          animation: flash 0.3s ease-out;
        }

        @keyframes flash {
          0% { background-color: ${BLOOMBERG.YELLOW}40; }
          100% { background-color: transparent; }
        }
      `}</style>

      {/* ========== TOP NAVIGATION BAR ========== */}
      <div style={{
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        padding: '6px 12px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        flexShrink: 0,
        boxShadow: `0 2px 8px ${BLOOMBERG.ORANGE}20`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Activity size={18} color={BLOOMBERG.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + BLOOMBERG.ORANGE + ')' }} />
            <span style={{
              color: BLOOMBERG.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
            }}>
              {t('header.terminal')}
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Broker Selector */}
          <div style={{ position: 'relative' }}>
            <button
              onClick={() => setShowBrokerDropdown(!showBrokerDropdown)}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                padding: '4px 10px',
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                color: BLOOMBERG.CYAN,
                cursor: 'pointer',
                fontSize: '11px',
                fontWeight: 600,
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.borderColor = BLOOMBERG.CYAN}
              onMouseLeave={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
            >
              <Globe size={12} />
              {activeBroker?.toUpperCase() || t('header.selectBroker')}
              <ChevronDown size={10} />
            </button>

            {showBrokerDropdown && (
              <div style={{
                position: 'absolute',
                top: '100%',
                left: 0,
                marginTop: '4px',
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                minWidth: '200px',
                zIndex: 1000,
                boxShadow: `0 4px 16px ${BLOOMBERG.DARK_BG}80`
              }}>
                {availableBrokers.map((broker) => (
                  <div
                    key={broker.id}
                    onClick={() => handleBrokerChange(broker.id)}
                    style={{
                      padding: '10px 12px',
                      cursor: 'pointer',
                      fontSize: '11px',
                      backgroundColor: activeBroker === broker.id ? `${BLOOMBERG.ORANGE}20` : 'transparent',
                      borderLeft: activeBroker === broker.id ? `3px solid ${BLOOMBERG.ORANGE}` : '3px solid transparent',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'space-between',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      if (activeBroker !== broker.id) {
                        e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (activeBroker !== broker.id) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }
                    }}
                  >
                    <div>
                      <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>{broker.name}</div>
                      <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px' }}>{broker.type.toUpperCase()}</div>
                    </div>
                    {activeBroker === broker.id && (
                      <div style={{
                        width: '6px',
                        height: '6px',
                        borderRadius: '50%',
                        backgroundColor: BLOOMBERG.GREEN,
                        boxShadow: `0 0 6px ${BLOOMBERG.GREEN}`
                      }} />
                    )}
                  </div>
                ))}
              </div>
            )}
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Trading Mode Toggle */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <button
              onClick={() => setTradingMode('paper')}
              style={{
                padding: '4px 10px',
                backgroundColor: tradingMode === 'paper' ? BLOOMBERG.GREEN : BLOOMBERG.PANEL_BG,
                border: `1px solid ${tradingMode === 'paper' ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
                color: tradingMode === 'paper' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                transition: 'all 0.2s'
              }}
            >
              {t('header.paper')}
            </button>
            <button
              onClick={() => setTradingMode('live')}
              style={{
                padding: '4px 10px',
                backgroundColor: tradingMode === 'live' ? BLOOMBERG.RED : BLOOMBERG.PANEL_BG,
                border: `1px solid ${tradingMode === 'live' ? BLOOMBERG.RED : BLOOMBERG.BORDER}`,
                color: tradingMode === 'live' ? BLOOMBERG.WHITE : BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '10px',
                fontWeight: 700,
                transition: 'all 0.2s'
              }}
            >
              {t('header.live')}
            </button>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px' }}>
            {activeAdapter?.isConnected() ? (
              <>
                <Wifi size={12} color={BLOOMBERG.GREEN} />
                <span style={{ color: BLOOMBERG.GREEN }}>CONNECTED</span>
              </>
            ) : (
              <>
                <WifiOff size={12} color={BLOOMBERG.RED} />
                <span style={{ color: BLOOMBERG.RED }}>DISCONNECTED</span>
              </>
            )}
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          <TimezoneSelector compact />

          <button
            onClick={() => setShowSettings(!showSettings)}
            style={{
              padding: '4px 8px',
              backgroundColor: 'transparent',
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              fontSize: '10px',
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
              e.currentTarget.style.color = BLOOMBERG.ORANGE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
              e.currentTarget.style.color = BLOOMBERG.GRAY;
            }}
          >
            <SettingsIcon size={12} />
          </button>
        </div>
      </div>

      {/* ========== TICKER BAR ========== */}
      <div style={{
        backgroundColor: BLOOMBERG.PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '10px 12px',
        display: 'flex',
        alignItems: 'center',
        gap: '20px',
        flexShrink: 0
      }}>
        {/* Symbol Selector */}
        <div style={{ position: 'relative', minWidth: '180px' }} data-symbol-selector>
          <div
            onClick={() => setShowSymbolDropdown(!showSymbolDropdown)}
            style={{
              padding: '6px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              cursor: 'pointer',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}
          >
            <span style={{ fontSize: '13px', fontWeight: 700, color: BLOOMBERG.ORANGE }}>{selectedSymbol}</span>
            <ChevronDown size={12} color={BLOOMBERG.GRAY} />
          </div>

          {showSymbolDropdown && (
            <div style={{
              position: 'absolute',
              top: '100%',
              left: 0,
              right: 0,
              marginTop: '4px',
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              maxHeight: '400px',
              overflow: 'hidden',
              zIndex: 1000,
              boxShadow: `0 4px 16px ${BLOOMBERG.DARK_BG}80`
            }}>
              <div style={{ padding: '8px', borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                <input
                  type="text"
                  placeholder="Search symbols..."
                  value={searchQuery}
                  onChange={(e) => setSearchQuery(e.target.value)}
                  autoFocus
                  style={{
                    width: '100%',
                    padding: '6px 8px',
                    backgroundColor: BLOOMBERG.HEADER_BG,
                    color: BLOOMBERG.WHITE,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '11px',
                    outline: 'none'
                  }}
                />
              </div>
              <div style={{ maxHeight: '340px', overflowY: 'auto' }}>
                {availableSymbols
                  .filter(symbol => symbol.toLowerCase().includes(searchQuery.toLowerCase()))
                  .map(symbol => (
                    <div
                      key={symbol}
                      onClick={() => {
                        setSelectedSymbol(symbol);
                        setShowSymbolDropdown(false);
                        setSearchQuery('');
                      }}
                      style={{
                        padding: '8px 12px',
                        cursor: 'pointer',
                        fontSize: '11px',
                        backgroundColor: symbol === selectedSymbol ? `${BLOOMBERG.ORANGE}20` : 'transparent',
                        color: symbol === selectedSymbol ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE,
                        borderLeft: symbol === selectedSymbol ? `3px solid ${BLOOMBERG.ORANGE}` : 'none'
                      }}
                      onMouseEnter={(e) => {
                        if (symbol !== selectedSymbol) {
                          e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                        }
                      }}
                      onMouseLeave={(e) => {
                        if (symbol !== selectedSymbol) {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }
                      }}
                    >
                      {symbol}
                    </div>
                  ))}
              </div>
            </div>
          )}
        </div>

        {/* Price Display */}
        <div style={{ display: 'flex', alignItems: 'baseline', gap: '8px' }}>
          <span style={{ fontSize: '24px', fontWeight: 700, color: BLOOMBERG.YELLOW, willChange: 'contents', transition: 'none' }}>
            {tickerState ? `$${(tickerState.last || tickerState.price)?.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }) || '0.00'}` : '--'}
          </span>
          {tickerState && (priceChange !== 0 || priceChangePercent !== 0) && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              {priceChange >= 0 ? (
                <ArrowUp size={16} color={BLOOMBERG.GREEN} />
              ) : (
                <ArrowDown size={16} color={BLOOMBERG.RED} />
              )}
              <span style={{
                fontSize: '13px',
                fontWeight: 600,
                color: priceChange >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
              }}>
                {priceChange >= 0 ? '+' : ''}{priceChange.toFixed(2)} ({priceChangePercent >= 0 ? '+' : ''}{priceChangePercent.toFixed(2)}%)
              </span>
            </div>
          )}
          {tickerState && priceChange === 0 && priceChangePercent === 0 && (
            <div style={{ fontSize: '11px', color: BLOOMBERG.GRAY, fontStyle: 'italic' }}>
              No change data
            </div>
          )}
        </div>

        <div style={{ height: '24px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

        {/* Market Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px', willChange: 'contents' }}>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
            <div style={{ color: BLOOMBERG.GREEN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {tickerState?.bid ? `$${tickerState.bid.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
            <div style={{ color: BLOOMBERG.RED, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {tickerState?.ask ? `$${tickerState.ask.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '120px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H RANGE</div>
            <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {spread24h > 0 ? `$${spread24h.toFixed(2)} (${spread24hPercent.toFixed(2)}%)` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H HIGH</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {tickerState?.high ? `$${tickerState.high.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H LOW</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {tickerState?.low ? `$${tickerState.low.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '100px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H VOLUME</div>
            <div style={{ color: BLOOMBERG.PURPLE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {tickerState?.volume ? (
                tickerState.quoteVolume
                  ? `$${tickerState.quoteVolume.toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                  : currentPrice > 0
                    ? `$${(tickerState.volume * currentPrice).toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                    : `${tickerState.volume.toLocaleString(undefined, { maximumFractionDigits: 2 })} ${selectedSymbol.split('/')[0]}`
              ) : '--'}
            </div>
          </div>
        </div>

        {tradingMode === 'paper' && (
          <>
            <div style={{ height: '24px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>BALANCE</div>
                <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>${balance.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</div>
              </div>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>EQUITY</div>
                <div style={{ color: BLOOMBERG.YELLOW, fontWeight: 600 }}>${equity.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</div>
              </div>
            </div>
          </>
        )}
      </div>

      {/* ========== MAIN CONTENT ========== */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* LEFT SIDEBAR - Watchlist / AI Agents Toggle */}
        <div style={{
          width: '320px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Toggle Header */}
          <div style={{
            padding: '6px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            gap: '4px'
          }}>
            <button
              onClick={() => setLeftSidebarView('watchlist')}
              style={{
                flex: 1,
                padding: '6px 8px',
                backgroundColor: leftSidebarView === 'watchlist' ? BLOOMBERG.ORANGE : 'transparent',
                border: 'none',
                color: leftSidebarView === 'watchlist' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                borderRadius: '2px'
              }}
            >
              WATCH
            </button>
            <button
              onClick={() => setLeftSidebarView('ai-agents')}
              style={{
                flex: 1,
                padding: '6px 8px',
                backgroundColor: leftSidebarView === 'ai-agents' ? BLOOMBERG.ORANGE : 'transparent',
                border: 'none',
                color: leftSidebarView === 'ai-agents' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                borderRadius: '2px'
              }}
            >
              AGENTS
            </button>
            <button
              onClick={() => setLeftSidebarView('leaderboard')}
              style={{
                flex: 1,
                padding: '6px 8px',
                backgroundColor: leftSidebarView === 'leaderboard' ? BLOOMBERG.ORANGE : 'transparent',
                border: 'none',
                color: leftSidebarView === 'leaderboard' ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                cursor: 'pointer',
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                transition: 'all 0.2s',
                borderRadius: '2px'
              }}
            >
              LEADER
            </button>
          </div>

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
                {watchlist.map((symbol, idx) => (
                  <div
                    key={symbol}
                    onClick={() => setSelectedSymbol(symbol)}
                    style={{
                      padding: '8px 10px',
                      cursor: 'pointer',
                      backgroundColor: selectedSymbol === symbol ? `${BLOOMBERG.ORANGE}15` : 'transparent',
                      borderLeft: selectedSymbol === symbol ? `2px solid ${BLOOMBERG.ORANGE}` : '2px solid transparent',
                      borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                      borderRight: idx % 2 === 0 ? `1px solid ${BLOOMBERG.BORDER}` : 'none',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      if (selectedSymbol !== symbol) {
                        e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                      }
                    }}
                    onMouseLeave={(e) => {
                      if (selectedSymbol !== symbol) {
                        e.currentTarget.style.backgroundColor = 'transparent';
                      }
                    }}
                  >
                    {/* Flex container: Ticker on left, Price/Change on right */}
                    <div style={{
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      gap: '8px'
                    }}>
                      {/* Left side: Ticker symbol */}
                      <div style={{
                        fontSize: '11px',
                        fontWeight: 700,
                        color: selectedSymbol === symbol ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE,
                        whiteSpace: 'nowrap'
                      }}>
                        {symbol.replace('/USD', '')}
                      </div>

                      {/* Right side: Price and Change */}
                      {watchlistPrices[symbol] ? (
                        <div style={{
                          display: 'flex',
                          flexDirection: 'column',
                          alignItems: 'flex-end',
                          gap: '2px'
                        }}>
                          <div style={{
                            fontSize: '10px',
                            color: BLOOMBERG.WHITE,
                            fontFamily: 'monospace',
                            fontWeight: 600
                          }}>
                            ${watchlistPrices[symbol].price >= 1000
                              ? watchlistPrices[symbol].price.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })
                              : watchlistPrices[symbol].price >= 1
                                ? watchlistPrices[symbol].price.toFixed(2)
                                : watchlistPrices[symbol].price.toFixed(4)
                            }
                          </div>
                          <div style={{
                            fontSize: '9px',
                            color: watchlistPrices[symbol].change >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                            fontFamily: 'monospace',
                            fontWeight: 700
                          }}>
                            {watchlistPrices[symbol].change >= 0 ? '▲' : '▼'} {Math.abs(watchlistPrices[symbol].change).toFixed(2)}%
                          </div>
                        </div>
                      ) : (
                        <div style={{
                          fontSize: '9px',
                          color: BLOOMBERG.GRAY,
                          fontFamily: 'monospace'
                        }}>
                          ...
                        </div>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            )}

            {leftSidebarView === 'ai-agents' && (
              <div style={{ flex: 1, overflow: 'auto' }}>
                <AIAgentsPanel
                  selectedSymbol={selectedSymbol}
                  portfolioData={{
                    positions: positions.map(p => ({
                      symbol: p.symbol,
                      quantity: p.quantity,
                      entry_price: p.entryPrice,
                      current_price: p.currentPrice,
                      value: p.positionValue
                    })),
                    total_value: equity
                  }}
                />
              </div>
            )}

            {leftSidebarView === 'leaderboard' && (
              <div style={{ flex: 1, overflow: 'hidden' }}>
                <LeaderboardPanel refreshInterval={10000} />
              </div>
            )}
          </div>
        </div>

        {/* CENTER - Chart & Order Book */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Chart Area */}
          <div style={{
            flex: isBottomPanelMinimized ? 1 : '0 0 55%',
            backgroundColor: BLOOMBERG.PANEL_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            transition: 'flex 0.3s ease'
          }}>
            <div style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              display: 'flex',
              alignItems: 'center',
              gap: '8px'
            }}>
              {['CHART', 'DEPTH', 'TRADES'].map((view) => (
                <button
                  key={view}
                  onClick={() => setSelectedView(view.toLowerCase() as any)}
                  style={{
                    padding: '4px 12px',
                    backgroundColor: selectedView === view.toLowerCase() ? BLOOMBERG.ORANGE : 'transparent',
                    border: 'none',
                    color: selectedView === view.toLowerCase() ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    cursor: 'pointer',
                    fontSize: '10px',
                    fontWeight: 700,
                    transition: 'all 0.2s'
                  }}
                >
                  {view}
                </button>
              ))}
            </div>
            <div style={{
              flex: 1,
              display: 'flex',
              alignItems: 'stretch',
              justifyContent: 'center',
              color: BLOOMBERG.GRAY,
              fontSize: '12px',
              overflow: 'visible',
              minHeight: 0
            }}>
              {selectedView === 'chart' && (
                <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column' }}>
                  <TradingChart
                    symbol={selectedSymbol}
                    showVolume={true}
                  />
                </div>
              )}
              {selectedView === 'depth' && (
                <div style={{ width: '100%', height: '100%', padding: '8px' }}>
                  <DepthChart
                    symbol={selectedSymbol}
                  />
                </div>
              )}
              {selectedView === 'trades' && (
                <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '8px' }}>
                  <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                    <thead>
                      <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                        <th style={{ padding: '6px', textAlign: 'left', color: BLOOMBERG.GRAY }}>TIME</th>
                        <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG.GRAY }}>PRICE</th>
                        <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG.GRAY }}>SIZE</th>
                      </tr>
                    </thead>
                    <tbody>
                      {tradesData.slice(0, 20).map((trade, idx) => (
                        <tr key={idx} style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}40` }}>
                          <td style={{ padding: '4px 6px', color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                            {new Date(trade.timestamp).toLocaleTimeString()}
                          </td>
                          <td style={{
                            padding: '4px 6px',
                            textAlign: 'right',
                            color: trade.side === 'buy' ? BLOOMBERG.GREEN : BLOOMBERG.RED
                          }}>
                            ${trade.price?.toFixed(2)}
                          </td>
                          <td style={{ padding: '4px 6px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                            {trade.quantity?.toFixed(4)}
                          </td>
                        </tr>
                      ))}
                    </tbody>
                  </table>
                </div>
              )}
            </div>
          </div>

          {/* Bottom Panel - Positions/Orders/Trades */}
          <div style={{
            flex: isBottomPanelMinimized ? '0 0 40px' : 1,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            transition: 'flex 0.3s ease'
          }}>
            <div style={{
              padding: '6px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div style={{ display: 'flex', gap: '4px' }}>
                {(['positions', 'orders', 'history', 'trades', 'stats', 'features', 'cross-exchange'] as const).map((tab) => (
                  <button
                    key={tab}
                    onClick={() => setActiveBottomTab(tab)}
                    style={{
                      padding: '6px 16px',
                      backgroundColor: activeBottomTab === tab ? BLOOMBERG.ORANGE : 'transparent',
                      border: 'none',
                      color: activeBottomTab === tab ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                      cursor: 'pointer',
                      fontSize: '10px',
                      fontWeight: 700,
                      textTransform: 'uppercase',
                      transition: 'all 0.2s'
                    }}
                  >
                    {tab} {tab === 'positions' && `(${positions.length})`} {tab === 'orders' && `(${orders.length})`}
                  </button>
                ))}
              </div>

              {/* Minimize/Maximize Button */}
              <button
                onClick={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
                style={{
                  padding: '4px 8px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.GRAY,
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
                  e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                  e.currentTarget.style.borderColor = BLOOMBERG.GRAY;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                }}
                title={isBottomPanelMinimized ? 'Maximize Panel' : 'Minimize Panel'}
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

            {!isBottomPanelMinimized && (
              <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                {/* Positions Tab */}
                {activeBottomTab === 'positions' && <PositionsTable />}

                {/* Orders Tab */}
                {activeBottomTab === 'orders' && <OrdersTable />}

                {/* History Tab (Closed Orders) */}
                {activeBottomTab === 'history' && <ClosedOrders />}

                {/* Trades Tab */}
                {activeBottomTab === 'trades' && (
                  trades.length === 0 ? (
                    <div style={{ padding: '40px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
                      No trade history
                    </div>
                  ) : (
                    <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                          <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 600 }}>TIME</th>
                          <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 600 }}>SYMBOL</th>
                          <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG.GRAY, fontWeight: 600 }}>SIDE</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 600 }}>QTY</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 600 }}>PRICE</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 600 }}>VALUE</th>
                          <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 600 }}>FEE</th>
                        </tr>
                      </thead>
                      <tbody>
                        {trades.map((trade, idx) => (
                          <tr key={idx} style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}40` }}>
                            <td style={{ padding: '8px', color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                              {new Date(trade.timestamp).toLocaleTimeString()}
                            </td>
                            <td style={{ padding: '8px', color: BLOOMBERG.WHITE }}>{trade.symbol}</td>
                            <td style={{ padding: '8px', textAlign: 'center' }}>
                              <span style={{
                                padding: '2px 8px',
                                backgroundColor: trade.side === 'buy' ? `${BLOOMBERG.GREEN}20` : `${BLOOMBERG.RED}20`,
                                color: trade.side === 'buy' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                                fontSize: '9px',
                                fontWeight: 700
                              }}>
                                {trade.side.toUpperCase()}
                              </span>
                            </td>
                            <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                              {trade.amount?.toFixed(4) || '0.0000'}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.YELLOW }}>
                              ${trade.price?.toFixed(2) || '0.00'}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.CYAN }}>
                              ${trade.cost?.toFixed(2) || '0.00'}
                            </td>
                            <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG.RED }}>
                              ${trade.fee?.cost?.toFixed(2) || '0.00'}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  )
                )}

                {/* Stats Tab */}
                {activeBottomTab === 'stats' && (
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 300px',
                    gap: '12px',
                    padding: '12px',
                    height: '100%',
                    overflow: 'hidden'
                  }}>
                    {/* Left: Account Statistics */}
                    <div style={{ height: '100%', overflow: 'auto' }}>
                      <AccountStats />
                    </div>

                    {/* Right: Fees & Margin */}
                    <div style={{
                      display: 'flex',
                      flexDirection: 'column',
                      gap: '12px',
                      height: '100%',
                      overflow: 'auto'
                    }}>
                      <FeesDisplay />
                      <MarginPanel />
                    </div>
                  </div>
                )}

                {/* Features Tab - Exchange-Specific Features */}
                {activeBottomTab === 'features' && (
                  <div style={{ padding: '16px', overflow: 'auto' }}>
                    {/* Kraken-Specific Features */}
                    {activeBroker === 'kraken' && (
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                        <StakingPanel />
                        <FuturesPanel />
                      </div>
                    )}

                    {/* HyperLiquid - Already shown in order form area */}
                    {activeBroker === 'hyperliquid' && (
                      <div style={{
                        padding: '40px',
                        textAlign: 'center',
                        color: BLOOMBERG.GRAY,
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        borderRadius: '4px'
                      }}>
                        <div style={{ fontSize: '14px', fontWeight: 600, marginBottom: '8px' }}>
                          HyperLiquid Features
                        </div>
                        <div style={{ fontSize: '11px' }}>
                          HyperLiquid-specific features (Vault Manager, Leverage Control) are available in the order form panel on the right.
                        </div>
                      </div>
                    )}

                    {/* No exchange-specific features */}
                    {activeBroker !== 'kraken' && activeBroker !== 'hyperliquid' && (
                      <div style={{
                        padding: '40px',
                        textAlign: 'center',
                        color: BLOOMBERG.GRAY,
                        backgroundColor: BLOOMBERG.PANEL_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`,
                        borderRadius: '4px'
                      }}>
                        <div style={{ fontSize: '14px', fontWeight: 600, marginBottom: '8px' }}>
                          No Exchange-Specific Features
                        </div>
                        <div style={{ fontSize: '11px' }}>
                          The selected exchange does not have additional features available in this panel.
                        </div>
                      </div>
                    )}
                  </div>
                )}

                {/* Cross-Exchange Tab - Multi-Exchange Portfolio & Arbitrage */}
                {activeBottomTab === 'cross-exchange' && (
                  <div style={{ padding: '16px', overflow: 'auto' }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
                      <PortfolioAggregator />
                      <ArbitrageDetector />
                    </div>
                  </div>
                )}
              </div>
            )}
          </div>
        </div>

        {/* RIGHT SIDEBAR - Order Entry & Order Book */}
        <div style={{
          width: '300px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderLeft: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden'
        }}>
          {/* Order Entry */}
          <div style={{
            height: '45%',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
          }}>
            <div style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: BLOOMBERG.ORANGE,
              letterSpacing: '0.5px'
            }}>
              ORDER ENTRY
            </div>
            <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
              {tradingMode === 'paper' ? (
                isLoading ? (
                  <div style={{
                    padding: '20px',
                    textAlign: 'center',
                    border: `1px solid ${BLOOMBERG.CYAN}`,
                    backgroundColor: `${BLOOMBERG.CYAN}10`
                  }}>
                    <div style={{ color: BLOOMBERG.CYAN, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                      ⏳ LOADING...
                    </div>
                    <div style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
                      Loading broker configurations...
                    </div>
                  </div>
                ) : !paperAdapter ? (
                  <div style={{
                    padding: '20px',
                    textAlign: 'center',
                    border: `1px solid ${BLOOMBERG.RED}`,
                    backgroundColor: `${BLOOMBERG.RED}10`
                  }}>
                    <div style={{ color: BLOOMBERG.RED, fontSize: '12px', fontWeight: 700, marginBottom: '12px' }}>
                      {isConnecting ? '⏳ CONNECTING...' : '❌ ADAPTER NOT READY'}
                    </div>
                    {!isConnecting && (
                      <>
                        <div style={{ color: BLOOMBERG.WHITE, fontSize: '10px', marginBottom: '12px', lineHeight: '1.5' }}>
                          Paper trading adapter failed to initialize.
                          <br />
                          This is usually caused by:
                        </div>
                        <div style={{
                          color: BLOOMBERG.GRAY,
                          fontSize: '9px',
                          textAlign: 'left',
                          backgroundColor: BLOOMBERG.DARK_BG,
                          padding: '8px',
                          borderRadius: '2px',
                          marginBottom: '12px',
                          lineHeight: '1.6'
                        }}>
                          1. Database initialization failure<br />
                          2. Missing write permissions<br />
                          3. Broker adapter connection error
                        </div>
                        <div style={{ color: BLOOMBERG.YELLOW, fontSize: '9px', marginBottom: '8px' }}>
                          Check the browser console (F12) and terminal for detailed error messages
                        </div>
                        <button
                          onClick={() => window.location.reload()}
                          style={{
                            padding: '6px 12px',
                            backgroundColor: BLOOMBERG.ORANGE,
                            border: 'none',
                            color: BLOOMBERG.DARK_BG,
                            cursor: 'pointer',
                            fontSize: '10px',
                            fontWeight: 700,
                            borderRadius: '2px',
                            marginTop: '4px'
                          }}
                        >
                          RELOAD APPLICATION
                        </button>
                      </>
                    )}
                    {isConnecting && (
                      <div style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
                        Setting up paper trading adapter...
                      </div>
                    )}
                  </div>
                ) : !paperAdapter.isConnected() ? (
                  <div style={{
                    padding: '20px',
                    textAlign: 'center',
                    border: `1px solid ${BLOOMBERG.YELLOW}`,
                    backgroundColor: `${BLOOMBERG.YELLOW}10`
                  }}>
                    <div style={{ color: BLOOMBERG.YELLOW, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                      ⏳ CONNECTING...
                    </div>
                    <div style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
                      Establishing connection to {activeBroker}...
                    </div>
                  </div>
                ) : (
                  <>
                    <EnhancedOrderForm
                      symbol={selectedSymbol}
                      currentPrice={currentPrice}
                      balance={balance}
                      onOrderPlaced={() => {
                        // Reload positions and orders after successful order
                        console.log('Order placed successfully');
                      }}
                    />

                    {/* HyperLiquid Vault Manager */}
                    <HyperLiquidVaultManager />
                  </>
                )
              ) : (
                <div style={{
                  padding: '20px',
                  textAlign: 'center',
                  border: `1px solid ${BLOOMBERG.RED}`,
                  backgroundColor: `${BLOOMBERG.RED}10`,
                  color: BLOOMBERG.RED,
                  fontSize: '11px'
                }}>
                  Live trading not yet enabled
                </div>
              )}
            </div>
          </div>

          {/* Order Book / Volume Profile */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
            <div style={{
              padding: '8px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
              fontSize: '10px',
              fontWeight: 700,
              color: BLOOMBERG.ORANGE,
              letterSpacing: '0.5px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center'
            }}>
              <div style={{ display: 'flex', gap: '8px' }}>
                {['orderbook', 'volume', 'modelchat'].map((view) => (
                  <button
                    key={view}
                    onClick={() => setRightPanelView(view as any)}
                    style={{
                      padding: '4px 8px',
                      backgroundColor: rightPanelView === view ? BLOOMBERG.ORANGE : 'transparent',
                      border: 'none',
                      color: rightPanelView === view ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                      cursor: 'pointer',
                      fontSize: '9px',
                      fontWeight: 700,
                      transition: 'all 0.2s'
                    }}
                  >
                    {view.toUpperCase()}
                  </button>
                ))}
              </div>
              {rightPanelView === 'orderbook' && (
                <span style={{ fontSize: '8px', color: BLOOMBERG.GRAY }}>
                  {orderBook.asks.length + orderBook.bids.length} LEVELS
                </span>
              )}
            </div>
            <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>
              {rightPanelView === 'orderbook' && (
                <>
                  <div style={{ marginBottom: '12px' }}>
                    <div style={{ color: BLOOMBERG.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      ASKS ({orderBook.asks.length})
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <tbody>
                        {orderBook.asks.slice(0, 12).reverse().map((ask, idx) => {
                          const depth = (ask.size / Math.max(...orderBook.asks.map(a => a.size))) * 100;
                          return (
                            <tr key={idx} style={{ position: 'relative' }}>
                              <td style={{
                                padding: '2px 4px',
                                color: BLOOMBERG.RED,
                                position: 'relative',
                                zIndex: 1
                              }}>
                                <div style={{
                                  position: 'absolute',
                                  right: 0,
                                  top: 0,
                                  bottom: 0,
                                  width: `${depth}%`,
                                  backgroundColor: `${BLOOMBERG.RED}15`,
                                  zIndex: -1
                                }} />
                                ${ask.price.toFixed(2)}
                              </td>
                              <td style={{ padding: '2px 4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>
                                {ask.size.toFixed(4)}
                              </td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </table>
                  </div>

                  <div style={{
                    height: '1px',
                    backgroundColor: BLOOMBERG.BORDER,
                    margin: '8px 0',
                    position: 'relative'
                  }}>
                    <div style={{
                      position: 'absolute',
                      left: '50%',
                      top: '50%',
                      transform: 'translate(-50%, -50%)',
                      backgroundColor: BLOOMBERG.PANEL_BG,
                      padding: '0 8px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.YELLOW
                    }}>
                      ${currentPrice.toFixed(2)}
                    </div>
                  </div>

                  <div>
                    <div style={{ color: BLOOMBERG.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      BIDS ({orderBook.bids.length})
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <tbody>
                        {orderBook.bids.slice(0, 12).map((bid, idx) => {
                          const depth = (bid.size / Math.max(...orderBook.bids.map(b => b.size))) * 100;
                          return (
                            <tr key={idx} style={{ position: 'relative' }}>
                              <td style={{
                                padding: '2px 4px',
                                color: BLOOMBERG.GREEN,
                                position: 'relative',
                                zIndex: 1
                              }}>
                                <div style={{
                                  position: 'absolute',
                                  right: 0,
                                  top: 0,
                                  bottom: 0,
                                  width: `${depth}%`,
                                  backgroundColor: `${BLOOMBERG.GREEN}15`,
                                  zIndex: -1
                                }} />
                                ${bid.price.toFixed(2)}
                              </td>
                              <td style={{ padding: '2px 4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>
                                {bid.size.toFixed(4)}
                              </td>
                            </tr>
                          );
                        })}
                      </tbody>
                    </table>
                  </div>
                </>
              )}

              {rightPanelView === 'volume' && (
                <VolumeProfile symbol={selectedSymbol} height={550} />
              )}

              {rightPanelView === 'modelchat' && (
                <ModelChatPanel refreshInterval={5000} />
              )}
            </div>
            <div style={{
              padding: '4px 8px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderTop: `1px solid ${BLOOMBERG.BORDER}`,
              fontSize: '8px',
              color: BLOOMBERG.GRAY
            }}>
              Updated: {lastOrderBookUpdate > 0 ? new Date(lastOrderBookUpdate).toLocaleTimeString() : 'Never'}
            </div>
          </div>
        </div>
      </div>

      {/* ========== STATUS BAR ========== */}
      <div style={{
        borderTop: `1px solid ${BLOOMBERG.BORDER}`,
        backgroundColor: BLOOMBERG.HEADER_BG,
        padding: '4px 12px',
        fontSize: '9px',
        color: BLOOMBERG.GRAY,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <span>Fincept Terminal v3.1.4 | Professional Trading Platform</span>
        <span>
          Broker: <span style={{ color: BLOOMBERG.ORANGE }}>{activeBroker?.toUpperCase() || 'NONE'}</span> |
          Mode: <span style={{ color: tradingMode === 'paper' ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
            {tradingMode.toUpperCase()}
          </span> |
          Status: <span style={{ color: activeAdapter?.isConnected() ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
            {activeAdapter?.isConnected() ? 'CONNECTED' : 'DISCONNECTED'}
          </span>
        </span>
      </div>
    </div>
  );
}
