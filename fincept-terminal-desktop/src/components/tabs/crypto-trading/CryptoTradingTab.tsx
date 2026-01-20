// CryptoTradingTab.tsx - Professional Fincept Terminal-Grade Crypto Trading Interface
import React, { useState, useEffect, useCallback } from 'react';
import { useTranslation } from 'react-i18next';
import { invoke } from '@tauri-apps/api/core';

import { useBrokerContext } from '@/contexts/BrokerContext';
import { useRustTicker, useRustOrderBook, useRustTrades } from '@/hooks/useRustWebSocket';
import { getMarketDataService } from '@/services/trading/UnifiedMarketDataService';

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
  RightPanelViewType,
  LeftSidebarViewType,
  BottomPanelTabType,
} from './types';

import type { OrderRequest } from '@/types/trading';

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

  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSymbol, setSelectedSymbol] = useState(defaultSymbols[0] || 'BTC/USD');
  const [tickerState, setTickerData] = useState<TickerData | null>(null);
  const [orderBook, setOrderBook] = useState<OrderBook>({ bids: [], asks: [], symbol: '' });
  const [tradesState, setTradesData] = useState<TradeData[]>([]);
  const [isPlacingOrder, setIsPlacingOrder] = useState(false);
  const [lastOrderBookUpdate, setLastOrderBookUpdate] = useState<number>(0);

  // UI State
  const [searchQuery, setSearchQuery] = useState('');
  const [availableSymbols, setAvailableSymbols] = useState<string[]>([]);
  const [isLoadingSymbols, setIsLoadingSymbols] = useState(false);
  const [showSymbolDropdown, setShowSymbolDropdown] = useState(false);
  const [showBrokerDropdown, setShowBrokerDropdown] = useState(false);
  const [selectedView, setSelectedView] = useState<CenterViewType>('chart');
  const [rightPanelView, setRightPanelView] = useState<RightPanelViewType>('orderbook');
  const [leftSidebarView, setLeftSidebarView] = useState<LeftSidebarViewType>('watchlist');

  // Paper trading state
  const [positions, setPositions] = useState<Position[]>([]);
  const [orders, setOrders] = useState<Order[]>([]);
  const [trades, setTrades] = useState<any[]>([]);
  const [balance, setBalance] = useState(0);
  const [equity, setEquity] = useState(0);
  const [stats, setStats] = useState<any>(null);
  const [activeBottomTab, setActiveBottomTab] = useState<BottomPanelTabType>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);

  // Trading settings
  const [slippage, setSlippage] = useState(0);
  const [makerFee, setMakerFee] = useState(0.0002);
  const [takerFee, setTakerFee] = useState(0.0005);
  const [showSettings, setShowSettings] = useState(false);

  // Watchlist
  const [watchlist, setWatchlist] = useState<string[]>(DEFAULT_CRYPTO_WATCHLIST);
  const [watchlistPrices, setWatchlistPrices] = useState<Record<string, WatchlistPrice>>({});

  // Initialize WebSocket providers (Kraken & HyperLiquid)
  useEffect(() => {
    const initializeProviders = async () => {
      try {
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

  // Fetch watchlist prices
  useEffect(() => {
    const fetchWatchlistPrices = async () => {
      try {
        if (realAdapter?.isConnected() && typeof realAdapter.fetchTickers === 'function') {
          try {
            const tickers = await realAdapter.fetchTickers(watchlist);
            const prices: Record<string, WatchlistPrice> = {};

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

            if (hasValidData) {
              setWatchlistPrices(prices);
              return;
            }
          } catch (err) {
            console.warn('[CryptoTrading] fetchTickers failed, trying fallback API:', err);
          }
        }

        // Fallback to public Binance API
        const symbols = watchlist.map(s => s.replace('/USD', 'USDT').replace('/', ''));
        const response = await fetch('https://api.binance.com/api/v3/ticker/24hr');

        if (!response.ok) {
          throw new Error(`Binance API returned ${response.status}`);
        }

        const allTickers = await response.json();

        if (!Array.isArray(allTickers)) {
          throw new Error('Invalid response from Binance API');
        }

        const prices: Record<string, WatchlistPrice> = {};
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

        if (hasValidData) {
          setWatchlistPrices(prices);
        }
      } catch (error) {
        console.error('[CryptoTrading] Failed to fetch prices:', error);
      }
    };

    fetchWatchlistPrices();
    const interval = setInterval(fetchWatchlistPrices, 30000);
    return () => clearInterval(interval);
  }, [realAdapter, watchlist]);

  // Clear data when symbol changes
  useEffect(() => {
    setTickerData(null);
    setTradesData([]);
  }, [selectedSymbol]);

  // Reset data when broker changes
  useEffect(() => {
    setTickerData(null);
    setOrderBook({ bids: [], asks: [], symbol: '' });
    setTradesData([]);
    setWatchlistPrices({});
    setLastOrderBookUpdate(0);

    if (defaultSymbols.length > 0) {
      setSelectedSymbol(defaultSymbols[0]);
    }
  }, [activeBroker, defaultSymbols]);

  // Subscribe to Rust WebSocket feeds
  const { data: tickerData, error: tickerError } = useRustTicker(
    activeBroker || '',
    selectedSymbol,
    !!activeBroker
  );

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

  // Update trades from Rust WebSocket
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
        console.error('[CryptoTrading] Failed to load paper trading data:', error);
      }
    };

    loadPaperTradingData();
    const intervalRef = { current: null as NodeJS.Timeout | null };
    intervalRef.current = setInterval(loadPaperTradingData, 1000);
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [tradingMode, paperAdapter, activeBroker]);

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

  // Fetch markets
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
        console.error('[CryptoTrading] Failed to fetch markets:', error);
        setAvailableSymbols(defaultSymbols);
      } finally {
        setIsLoadingSymbols(false);
      }
    };
    fetchMarkets();
  }, [realAdapter, defaultSymbols]);

  // Handle broker change
  const handleBrokerChange = async (brokerId: string) => {
    setShowBrokerDropdown(false);
    await setActiveBroker(brokerId);
  };

  // Handle symbol selection
  const handleSymbolSelect = (symbol: string) => {
    setSelectedSymbol(symbol);
    setShowSymbolDropdown(false);
  };

  const currentPrice = tickerState?.last || tickerState?.price || orderBook.asks[0]?.price || 0;

  // Calculate price change and percentage
  let priceChange = 0;
  let priceChangePercent = 0;

  if (tickerState) {
    priceChange = tickerState.change || 0;
    priceChangePercent = tickerState.changePercent || 0;

    if (priceChange === 0 && priceChangePercent === 0) {
      const last = tickerState.last || tickerState.price || 0;
      const open = tickerState.open || 0;

      if (last > 0 && open > 0) {
        priceChange = last - open;
        priceChangePercent = (priceChange / open) * 100;
      }
    }
  }

  // Fallback: use watchlist data
  if (priceChange === 0 && priceChangePercent === 0 && watchlistPrices[selectedSymbol]) {
    priceChangePercent = watchlistPrices[selectedSymbol].change;
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

      {/* Ticker Bar */}
      <CryptoTickerBar
        selectedSymbol={selectedSymbol}
        tickerData={tickerState}
        availableSymbols={availableSymbols}
        searchQuery={searchQuery}
        showSymbolDropdown={showSymbolDropdown}
        balance={balance}
        equity={equity}
        tradingMode={tradingMode}
        priceChange={priceChange}
        priceChangePercent={priceChangePercent}
        onSymbolSelect={handleSymbolSelect}
        onSearchQueryChange={setSearchQuery}
        onSymbolDropdownToggle={() => setShowSymbolDropdown(!showSymbolDropdown)}
      />

      {/* Main Content */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Sidebar - Watchlist */}
        <CryptoWatchlist
          watchlist={watchlist}
          watchlistPrices={watchlistPrices}
          selectedSymbol={selectedSymbol}
          leftSidebarView={leftSidebarView}
          positions={positions}
          equity={equity}
          onSymbolSelect={handleSymbolSelect}
          onViewChange={setLeftSidebarView}
        />

        {/* Center - Chart & Bottom Panel */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Chart Area */}
          <CryptoChartArea
            selectedSymbol={selectedSymbol}
            selectedView={selectedView}
            tradesData={tradesState}
            isBottomPanelMinimized={isBottomPanelMinimized}
            onViewChange={setSelectedView}
          />

          {/* Bottom Panel */}
          <CryptoBottomPanel
            activeTab={activeBottomTab}
            positions={positions}
            orders={orders}
            trades={trades}
            isMinimized={isBottomPanelMinimized}
            activeBroker={activeBroker}
            selectedSymbol={selectedSymbol}
            tickerData={tickerState}
            realAdapter={realAdapter}
            onTabChange={setActiveBottomTab}
            onMinimizeToggle={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
          />
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
          <CryptoOrderEntry
            selectedSymbol={selectedSymbol}
            currentPrice={currentPrice}
            balance={balance}
            tradingMode={tradingMode}
            activeBroker={activeBroker}
            isLoading={isLoading}
            isConnecting={isConnecting}
            paperAdapter={paperAdapter}
            realAdapter={realAdapter}
            onTradingModeChange={setTradingMode}
          />

          {/* Order Book */}
          <CryptoOrderBook
            orderBook={orderBook}
            currentPrice={currentPrice}
            selectedSymbol={selectedSymbol}
            rightPanelView={rightPanelView}
            lastOrderBookUpdate={lastOrderBookUpdate}
            onViewChange={setRightPanelView}
          />
        </div>
      </div>

      {/* Status Bar */}
      <CryptoStatusBar
        activeBroker={activeBroker}
        tradingMode={tradingMode}
        isConnected={activeAdapter?.isConnected() || false}
      />
    </div>
  );
}

export default CryptoTradingTab;
