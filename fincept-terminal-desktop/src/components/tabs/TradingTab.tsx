// File: src/components/tabs/TradingTab.tsx
// Professional Bloomberg Terminal-Grade Trading Interface

import React, { useState, useEffect, useCallback } from 'react';
import {
  TrendingUp, TrendingDown, Activity, DollarSign, BarChart3,
  Settings as SettingsIcon, Globe, Wifi, WifiOff, Bell,
  Zap, Target, AlertCircle, RefreshCw, Maximize2, ChevronDown,
  Clock, TrendingDown as ArrowDown, TrendingUp as ArrowUp, ChevronUp, Minimize2
} from 'lucide-react';
import { useBrokerContext } from '../../contexts/BrokerContext';
import { useWebSocket } from '../../hooks/useWebSocket';
// Import new enhanced order form
import { EnhancedOrderForm } from './trading/core/OrderForm';
import { HyperLiquidVaultManager } from './trading/exchange-specific/hyperliquid';
import { StakingPanel, FuturesPanel } from './trading/exchange-specific/kraken';
import { PositionsTable } from './trading/core/PositionManager';
import { OrdersTable, ClosedOrders } from './trading/core/OrderManager';
import { TradingChart, DepthChart, VolumeProfile } from './trading/charts';
import { AccountStats, FeesDisplay, MarginPanel } from './trading/core/AccountInfo';
import { PortfolioAggregator, ArbitrageDetector } from './trading/cross-exchange';
import type { OrderRequest } from '../../types/trading';

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
  const {
    activeBroker,
    availableBrokers,
    setActiveBroker,
    tradingMode,
    setTradingMode,
    paperPortfolioMode,
    setPaperPortfolioMode,
    activeAdapter,
    paperAdapter,
    defaultSymbols,
    isConnecting,
  } = useBrokerContext();

  const [currentTime, setCurrentTime] = useState(new Date());
  const [selectedSymbol, setSelectedSymbol] = useState(defaultSymbols[0] || 'BTC/USD');
  const [tickerData, setTickerData] = useState<any>(null);
  const [orderBook, setOrderBook] = useState<OrderBook>({ bids: [], asks: [], symbol: '' });
  const [tradesData, setTradesData] = useState<any[]>([]);
  const [isPlacingOrder, setIsPlacingOrder] = useState(false);
  const [lastOrderBookUpdate, setLastOrderBookUpdate] = useState<number>(0);

  // UI State
  const [searchQuery, setSearchQuery] = useState('');
  const [availableSymbols, setAvailableSymbols] = useState<string[]>([]);
  const [isLoadingSymbols, setIsLoadingSymbols] = useState(false);
  const [showSymbolDropdown, setShowSymbolDropdown] = useState(false);
  const [showBrokerDropdown, setShowBrokerDropdown] = useState(false);
  const [selectedView, setSelectedView] = useState<'chart' | 'depth' | 'trades'>('chart');
  const [rightPanelView, setRightPanelView] = useState<'orderbook' | 'volume'>('orderbook');

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
  const [watchlist, setWatchlist] = useState<string[]>(['BTC/USD', 'ETH/USD', 'SOL/USD']);

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Clear data when symbol changes
  useEffect(() => {
    setTickerData(null);
    setOrderBook({ bids: [], asks: [], symbol: selectedSymbol });
    setTradesData([]);
    setLastOrderBookUpdate(0);
  }, [selectedSymbol]);

  // Subscribe to WebSocket feeds
  const { message: tickerMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.ticker.${selectedSymbol}` : null
  );

  const { message: orderbookMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.book.${selectedSymbol}` : null
  );

  const { message: tradesMessage } = useWebSocket(
    activeBroker ? `${activeBroker}.trade.${selectedSymbol}` : null
  );

  // Update ticker data
  useEffect(() => {
    if (tickerMessage && tickerMessage.data) {
      const data = tickerMessage.data;
      if (data.symbol === selectedSymbol) {
        setTickerData(data);
        if (tradingMode === 'paper' && paperAdapter) {
          const matchingEngine = (paperAdapter as any).matchingEngine;
          if (matchingEngine && typeof matchingEngine.updatePriceFromWebSocket === 'function') {
            matchingEngine.updatePriceFromWebSocket(selectedSymbol, {
              bid: data.bid,
              ask: data.ask,
              last: data.last,
            });
          }
        }
      }
    }
  }, [tickerMessage, tradingMode, paperAdapter, selectedSymbol]);

  // Update orderbook
  useEffect(() => {
    if (!orderbookMessage || !orderbookMessage.data) return;
    const data = orderbookMessage.data;
    const messageType = data.messageType;
    const dataSymbol = data.symbol || selectedSymbol;

    if (dataSymbol !== selectedSymbol) return;

    if (messageType === 'snapshot') {
      setOrderBook({
        bids: data.bids || [],
        asks: data.asks || [],
        symbol: dataSymbol,
        checksum: data.checksum
      });
      setLastOrderBookUpdate(Date.now());
    } else if (messageType === 'update') {
      setOrderBook(prevBook => {
        const newBook = { ...prevBook };
        if (data.bids && data.bids.length > 0) {
          const bidsMap = new Map(prevBook.bids.map(b => [b.price, b.size]));
          data.bids.forEach((bid: OrderBookLevel) => {
            if (bid.size === 0) bidsMap.delete(bid.price);
            else bidsMap.set(bid.price, bid.size);
          });
          newBook.bids = Array.from(bidsMap.entries())
            .map(([price, size]) => ({ price, size }))
            .sort((a, b) => b.price - a.price)
            .slice(0, 25);
        }
        if (data.asks && data.asks.length > 0) {
          const asksMap = new Map(prevBook.asks.map(a => [a.price, a.size]));
          data.asks.forEach((ask: OrderBookLevel) => {
            if (ask.size === 0) asksMap.delete(ask.price);
            else asksMap.set(ask.price, ask.size);
          });
          newBook.asks = Array.from(asksMap.entries())
            .map(([price, size]) => ({ price, size }))
            .sort((a, b) => a.price - b.price)
            .slice(0, 25);
        }
        newBook.checksum = data.checksum;
        setLastOrderBookUpdate(Date.now());
        return newBook;
      });
    }
  }, [orderbookMessage, selectedSymbol]);

  // Update trades
  useEffect(() => {
    if (tradesMessage && tradesMessage.data) {
      const data = tradesMessage.data;
      const dataSymbol = data.symbol || selectedSymbol;
      if (dataSymbol === selectedSymbol) {
        setTradesData(prev => [data, ...prev].slice(0, 50));
      }
    }
  }, [tradesMessage, selectedSymbol]);

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
        const mappedPositions: Position[] = positionsData.map((p: any) => ({
          symbol: p.symbol,
          side: p.side,
          quantity: p.contracts,
          entryPrice: p.entryPrice,
          positionValue: p.info?.positionValue || (p.entryPrice * p.contracts),
          currentPrice: p.markPrice,
          unrealizedPnl: p.unrealizedPnl,
          pnlPercent: p.percentage || 0,
        }));
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
    const interval = setInterval(loadPaperTradingData, 2000);
    return () => clearInterval(interval);
  }, [tradingMode, paperAdapter]);

  // Update slippage & fees
  useEffect(() => {
    if (paperAdapter) {
      (paperAdapter as any).paperConfig.slippage.market = slippage;
      (paperAdapter as any).paperConfig.fees.maker = makerFee;
      (paperAdapter as any).paperConfig.fees.taker = takerFee;
    }
  }, [slippage, makerFee, takerFee, paperAdapter]);

  // Fetch markets
  useEffect(() => {
    const fetchMarkets = async () => {
      if (!activeAdapter) return;
      setIsLoadingSymbols(true);
      try {
        const markets = await activeAdapter.fetchMarkets();
        const symbols = markets
          .filter((m: any) => m.active && m.spot && (m.quote === 'USD' || m.quote === 'USDT' || m.quote === 'USDC'))
          .map((m: any) => m.symbol)
          .sort();
        setAvailableSymbols(symbols);
      } catch (error) {
        setAvailableSymbols(defaultSymbols);
      } finally {
        setIsLoadingSymbols(false);
      }
    };
    fetchMarkets();
  }, [activeAdapter, defaultSymbols]);

  // Handle order placement
  const handlePlaceOrder = useCallback(async (orderRequest: OrderRequest) => {
    if (!paperAdapter || !paperAdapter.isConnected()) {
      alert('Paper trading adapter not available');
      throw new Error('Adapter not available');
    }

    setIsPlacingOrder(true);
    try {
      const order = await paperAdapter.createOrder(
        orderRequest.symbol,
        orderRequest.type,
        orderRequest.side,
        orderRequest.quantity,
        orderRequest.price,
        { stopPrice: orderRequest.stopPrice }
      );

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

      const balanceData = await paperAdapter.fetchBalance();
      const usdBalance = (balanceData.free as any)?.USD || 0;
      setBalance(usdBalance);

      alert(`✓ Order placed: ${order.id}`);
    } catch (error) {
      alert(`✗ Order failed: ${(error as Error).message}`);
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

  const currentPrice = tickerData?.last || orderBook.asks[0]?.price || 0;

  // Calculate price change - try multiple field names
  const priceChange = tickerData?.change || tickerData?.price_change || 0;

  // Calculate percentage change - try from data or calculate manually
  let priceChangePercent = tickerData?.change_percent || tickerData?.percentage || tickerData?.percent_change || 0;

  // If no percentage provided, calculate it from price change and current price
  if (priceChangePercent === 0 && tickerData?.last && tickerData?.open && tickerData.open > 0) {
    priceChangePercent = ((tickerData.last - tickerData.open) / tickerData.open) * 100;
  } else if (priceChangePercent === 0 && priceChange !== 0 && currentPrice > 0) {
    // Calculate from absolute change if we have it
    const previousPrice = currentPrice - priceChange;
    if (previousPrice > 0) {
      priceChangePercent = (priceChange / previousPrice) * 100;
    }
  }

  // Calculate 24h price range (High - Low)
  const spread24h = (tickerData?.high && tickerData?.low) ? (tickerData.high - tickerData.low) : 0;
  const spread24hPercent = (spread24h > 0 && tickerData?.low && tickerData.low > 0)
    ? ((spread24h / tickerData.low) * 100)
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
              FINCEPT TERMINAL
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
              {activeBroker?.toUpperCase() || 'SELECT BROKER'}
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
              PAPER
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
              LIVE
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

          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '10px', color: BLOOMBERG.CYAN }}>
            <Clock size={12} />
            {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </div>

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
          <span style={{ fontSize: '24px', fontWeight: 700, color: BLOOMBERG.YELLOW }}>
            {tickerData ? `$${tickerData.last?.toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 }) || '0.00'}` : '--'}
          </span>
          {tickerData && (
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
        </div>

        <div style={{ height: '24px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

        {/* Market Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px' }}>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
            <div style={{ color: BLOOMBERG.GREEN, fontWeight: 600 }}>
              {tickerData ? `$${tickerData.bid?.toFixed(2) || '0.00'}` : '--'}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
            <div style={{ color: BLOOMBERG.RED, fontWeight: 600 }}>
              {tickerData ? `$${tickerData.ask?.toFixed(2) || '0.00'}` : '--'}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H RANGE</div>
            <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
              {spread24h > 0 ? (
                <>
                  ${spread24h.toFixed(2)} ({spread24hPercent.toFixed(2)}%)
                </>
              ) : (
                '--'
              )}
            </div>
          </div>
          {tickerData && (
            <>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H HIGH</div>
                <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>${tickerData.high?.toFixed(2) || '--'}</div>
              </div>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H LOW</div>
                <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>${tickerData.low?.toFixed(2) || '--'}</div>
              </div>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>24H VOLUME</div>
                <div style={{ color: BLOOMBERG.PURPLE, fontWeight: 600 }}>
                  {tickerData.volume ? (
                    <>
                      {/* Show volume in USD if available */}
                      {tickerData.quoteVolume ? (
                        `$${tickerData.quoteVolume.toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                      ) : tickerData.volumeUsd ? (
                        `$${tickerData.volumeUsd.toLocaleString(undefined, { maximumFractionDigits: 0 })}`
                      ) : (
                        // Calculate USD value from volume * current price
                        currentPrice > 0 ? (
                          <>
                            ${(tickerData.volume * currentPrice).toLocaleString(undefined, { maximumFractionDigits: 0 })}
                            <span style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginLeft: '4px' }}>
                              ({tickerData.volume.toLocaleString(undefined, { maximumFractionDigits: 2 })} {selectedSymbol.split('/')[0]})
                            </span>
                          </>
                        ) : (
                          <>
                            {tickerData.volume.toLocaleString(undefined, { maximumFractionDigits: 2 })}
                            <span style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginLeft: '2px' }}>
                              {selectedSymbol.split('/')[0]}
                            </span>
                          </>
                        )
                      )}
                    </>
                  ) : '--'}
                </div>
              </div>
            </>
          )}
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
        {/* LEFT SIDEBAR - Watchlist */}
        <div style={{
          width: '220px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG.BORDER}`,
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
            WATCHLIST
          </div>
          <div style={{ flex: 1, overflow: 'auto' }}>
            {watchlist.map((symbol, idx) => (
              <div
                key={symbol}
                onClick={() => setSelectedSymbol(symbol)}
                style={{
                  padding: '10px 12px',
                  cursor: 'pointer',
                  backgroundColor: selectedSymbol === symbol ? `${BLOOMBERG.ORANGE}15` : 'transparent',
                  borderLeft: selectedSymbol === symbol ? `3px solid ${BLOOMBERG.ORANGE}` : '3px solid transparent',
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
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
                <div style={{ fontSize: '11px', fontWeight: 600, color: selectedSymbol === symbol ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE, marginBottom: '4px' }}>
                  {symbol}
                </div>
                <div style={{ fontSize: '10px', color: BLOOMBERG.GREEN }}>+2.45%</div>
              </div>
            ))}
          </div>
        </div>

        {/* CENTER - Chart & Order Book */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Chart Area */}
          <div style={{
            height: '55%',
            backgroundColor: BLOOMBERG.PANEL_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden'
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
              alignItems: 'center',
              justifyContent: 'center',
              color: BLOOMBERG.GRAY,
              fontSize: '12px',
              overflow: 'hidden'
            }}>
              {selectedView === 'chart' && (
                <div style={{ width: '100%', height: '100%' }}>
                  <TradingChart symbol={selectedSymbol} height={isBottomPanelMinimized ? 650 : 450} showVolume={true} />
                </div>
              )}
              {selectedView === 'depth' && (
                <div style={{ width: '100%', height: '100%', padding: '8px' }}>
                  <DepthChart symbol={selectedSymbol} height={420} />
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
                            {trade.amount?.toFixed(4)}
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
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 300px', gap: '12px' }}>
                  {/* Left: Account Statistics */}
                  <div>
                    <AccountStats />
                  </div>

                  {/* Right: Fees & Margin */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
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
          width: '360px',
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
                !paperAdapter || !paperAdapter.isConnected() ? (
                  <div style={{
                    padding: '20px',
                    textAlign: 'center',
                    border: `1px solid ${BLOOMBERG.ORANGE}`,
                    backgroundColor: `${BLOOMBERG.ORANGE}10`
                  }}>
                    <div style={{ color: BLOOMBERG.ORANGE, fontSize: '11px', fontWeight: 700, marginBottom: '8px' }}>
                      ⚠️ INITIALIZING...
                    </div>
                    <div style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
                      {isConnecting ? 'Connecting to broker...' : 'Waiting for adapter...'}
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
                {['orderbook', 'volume'].map((view) => (
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
        <span>Fincept Terminal v3.0.12 | Professional Trading Platform</span>
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
