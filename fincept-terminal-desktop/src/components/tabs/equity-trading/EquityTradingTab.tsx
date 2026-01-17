/**
 * Equity Trading Tab - Bloomberg Terminal Style
 *
 * Professional stock/equity trading interface matching the crypto trading tab design.
 * Supports multiple brokers across different regions with real-time market data.
 */

import React, { useState, useEffect, useCallback, useMemo } from 'react';
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
  BrokerSetupPanel,
  StockOrderForm,
  PositionsPanel,
  HoldingsPanel,
  OrdersPanel,
  FundsPanel,
  StockTradingChart,
  SymbolSearch,
} from './components';
import type { SymbolSearchResult, SupportedBroker } from '@/services/trading/masterContractService';

import type { StockExchange, Quote } from '@/brokers/stocks/types';
import { GridTradingPanel } from '../trading/grid-trading';

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

// Default Indian market watchlist
const DEFAULT_WATCHLIST = [
  'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK',
  'HINDUNILVR', 'SBIN', 'BHARTIARTL', 'ITC', 'KOTAKBANK',
  'LT', 'AXISBANK', 'BAJFINANCE', 'MARUTI', 'ASIANPAINT',
  'HCLTECH', 'SUNPHARMA', 'TITAN', 'WIPRO', 'ULTRACEMCO',
  'NESTLEIND', 'M&M', 'TATASTEEL', 'POWERGRID', 'NTPC',
  'JSWSTEEL', 'TECHM', 'ADANIPORTS', 'DIVISLAB', 'DRREDDY'
];

// Market indices
const MARKET_INDICES = [
  { symbol: 'NIFTY 50', exchange: 'NSE' as StockExchange },
  { symbol: 'SENSEX', exchange: 'BSE' as StockExchange },
  { symbol: 'BANKNIFTY', exchange: 'NSE' as StockExchange },
  { symbol: 'NIFTYIT', exchange: 'NSE' as StockExchange },
];

type BottomPanelTab = 'positions' | 'holdings' | 'orders' | 'history' | 'stats' | 'market' | 'grid-trading';
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
  const { positions, holdings, orders, funds, refreshAll, isRefreshing } = useStockTradingData();

  // Time state
  const [currentTime, setCurrentTime] = useState(new Date());

  // Symbol state
  const [selectedSymbol, setSelectedSymbol] = useState<string>('');
  const [selectedExchange, setSelectedExchange] = useState<StockExchange>('NSE');
  // Note: searchQuery, showSymbolDropdown, searchResults, isSearching moved to SymbolSearch component

  // Quote data
  const [quote, setQuote] = useState<Quote | null>(null);
  const [wsError, setWsError] = useState<string | null>(null);

  // Watchlist
  const [watchlist, setWatchlist] = useState<string[]>(DEFAULT_WATCHLIST);
  const [watchlistQuotes, setWatchlistQuotes] = useState<Record<string, Quote>>({});

  // UI State
  const [leftSidebarView, setLeftSidebarView] = useState<LeftSidebarView>('watchlist');
  const [centerView, setCenterView] = useState<CenterView>('chart');
  const [rightPanelView, setRightPanelView] = useState<RightPanelView>('orderbook');
  const [activeBottomTab, setActiveBottomTab] = useState<BottomPanelTab>('positions');
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);
  const [showSettings, setShowSettings] = useState(false);

  // Recent trades for trades view
  const [recentTrades, setRecentTrades] = useState<any[]>([]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // Set default symbol when broker changes
  useEffect(() => {
    if (defaultSymbols.length > 0 && !selectedSymbol) {
      setSelectedSymbol(defaultSymbols[0]);
    }
    // Update watchlist from broker defaults
    if (defaultSymbols.length > 0) {
      setWatchlist(defaultSymbols.slice(0, 30));
    }
  }, [defaultSymbols, selectedSymbol]);

  // Subscribe to WebSocket for live prices
  useEffect(() => {
    if (!adapter || !isAuthenticated || !selectedSymbol) return;

    const subscribeToSymbol = async () => {
      try {
        console.log(`[EquityTrading] Subscribing to ${selectedSymbol} via WebSocket...`);

        await adapter.subscribe(selectedSymbol, selectedExchange, 'quote');

        const handleTick = (tick: any) => {
          if (tick.symbol === selectedSymbol) {
            setQuote({
              symbol: tick.symbol,
              exchange: tick.exchange,
              lastPrice: tick.lastPrice || tick.price,
              open: tick.open || tick.lastPrice,
              high: tick.high || tick.lastPrice,
              low: tick.low || tick.lastPrice,
              close: tick.close || tick.lastPrice,
              previousClose: tick.close || tick.lastPrice,
              volume: tick.volume || 0,
              change: tick.change || 0,
              changePercent: tick.changePercent || 0,
              bid: tick.bid || 0,
              bidQty: tick.bidQty || 0,
              ask: tick.ask || 0,
              askQty: tick.askQty || 0,
              timestamp: Date.now(),
            });
          }
        };

        adapter.onTick(handleTick);
        setWsError(null);
      } catch (err: any) {
        console.error('[EquityTrading] WebSocket subscription failed:', err);
        setWsError(err.message);
      }
    };

    subscribeToSymbol();

    return () => {
      if (adapter && selectedSymbol) {
        adapter.unsubscribe(selectedSymbol, selectedExchange).catch(console.error);
      }
    };
  }, [adapter, isAuthenticated, selectedSymbol, selectedExchange]);

  // Fetch watchlist quotes periodically
  useEffect(() => {
    if (!adapter || !isAuthenticated) return;

    const fetchWatchlistQuotes = async () => {
      try {
        const quotes: Record<string, Quote> = {};
        // Batch fetch quotes for watchlist
        for (const symbol of watchlist.slice(0, 15)) {
          try {
            const q = await adapter.getQuote(symbol, selectedExchange);
            if (q) {
              quotes[symbol] = q;
            }
          } catch (err) {
            // Skip individual quote errors
          }
        }
        if (Object.keys(quotes).length > 0) {
          setWatchlistQuotes(quotes);
        }
      } catch (err) {
        console.error('[EquityTrading] Failed to fetch watchlist quotes:', err);
      }
    };

    fetchWatchlistQuotes();
    const interval = setInterval(fetchWatchlistQuotes, 30000);
    return () => clearInterval(interval);
  }, [adapter, isAuthenticated, watchlist, selectedExchange]);

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
  const spread = quote ? (quote.ask - quote.bid) : 0;
  const spreadPercent = quote && quote.bid > 0 ? (spread / quote.bid) * 100 : 0;
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
          {/* Terminal Branding */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Building2 size={18} color={BLOOMBERG.ORANGE} style={{ filter: 'drop-shadow(0 0 4px ' + BLOOMBERG.ORANGE + ')' }} />
            <span style={{
              color: BLOOMBERG.ORANGE,
              fontWeight: 700,
              fontSize: '14px',
              letterSpacing: '0.5px',
              textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
            }}>
              EQUITY TERMINAL
            </span>
          </div>

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Broker Selector */}
          <BrokerSelector />

          <div style={{ height: '16px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Trading Mode Toggle */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <button
              onClick={() => setTradingMode('paper')}
              style={{
                padding: '4px 10px',
                backgroundColor: isPaper ? BLOOMBERG.GREEN : BLOOMBERG.PANEL_BG,
                border: `1px solid ${isPaper ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
                color: isPaper ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
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
                backgroundColor: isLive ? BLOOMBERG.RED : BLOOMBERG.PANEL_BG,
                border: `1px solid ${isLive ? BLOOMBERG.RED : BLOOMBERG.BORDER}`,
                color: isLive ? BLOOMBERG.WHITE : BLOOMBERG.GRAY,
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
          {/* Connection Status */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px', fontSize: '10px' }}>
            {isConnected ? (
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

          {/* Clock */}
          <div style={{
            fontSize: '11px',
            fontWeight: 600,
            color: BLOOMBERG.CYAN,
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
            color: BLOOMBERG.YELLOW,
            willChange: 'contents',
            transition: 'none'
          }}>
            {currentPrice > 0 ? `₹${currentPrice.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}` : '--'}
          </span>
          {quote && (priceChange !== 0 || priceChangePercent !== 0) && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              {priceChange >= 0 ? (
                <TrendingUp size={16} color={BLOOMBERG.GREEN} />
              ) : (
                <TrendingDown size={16} color={BLOOMBERG.RED} />
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
        <div style={{ display: 'flex', alignItems: 'center', gap: '24px', fontSize: '11px', willChange: 'contents' }}>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>BID</div>
            <div style={{ color: BLOOMBERG.GREEN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.bid ? `₹${quote.bid.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '60px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>ASK</div>
            <div style={{ color: BLOOMBERG.RED, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.ask ? `₹${quote.ask.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '100px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>DAY RANGE</div>
            <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {dayRange > 0 ? `₹${dayRange.toFixed(2)} (${dayRangePercent.toFixed(2)}%)` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>HIGH</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.high ? `₹${quote.high.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '80px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>LOW</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.low ? `₹${quote.low.toFixed(2)}` : '--'}
            </div>
          </div>
          <div style={{ minWidth: '100px' }}>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>VOLUME</div>
            <div style={{ color: BLOOMBERG.PURPLE, fontWeight: 600, willChange: 'contents', transition: 'none' }}>
              {quote?.volume ? quote.volume.toLocaleString('en-IN') : '--'}
            </div>
          </div>
        </div>

        {/* Account Summary */}
        {isAuthenticated && (
          <>
            <div style={{ height: '24px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />
            <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>MARGIN</div>
                <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
                  ₹{(funds?.availableMargin || 0).toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
                </div>
              </div>
              <div>
                <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>P&L</div>
                <div style={{ color: totalPositionPnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontWeight: 600 }}>
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
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderRight: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0
        }}>
          {/* Toggle Header */}
          <div style={{
            padding: '6px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
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
                  backgroundColor: leftSidebarView === view ? BLOOMBERG.ORANGE : 'transparent',
                  border: 'none',
                  color: leftSidebarView === view ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
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
                        backgroundColor: isSelected ? `${BLOOMBERG.ORANGE}15` : 'transparent',
                        borderLeft: isSelected ? `2px solid ${BLOOMBERG.ORANGE}` : '2px solid transparent',
                        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                        borderRight: idx % 2 === 0 ? `1px solid ${BLOOMBERG.BORDER}` : 'none',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => {
                        if (!isSelected) {
                          e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
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
                          color: isSelected ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE,
                          whiteSpace: 'nowrap',
                          overflow: 'hidden',
                          textOverflow: 'ellipsis',
                          maxWidth: '60px'
                        }}>
                          {symbol}
                        </div>

                        {q ? (
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
                              ₹{q.lastPrice >= 1000
                                ? q.lastPrice.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })
                                : q.lastPrice.toFixed(2)
                              }
                            </div>
                            <div style={{
                              fontSize: '9px',
                              color: q.changePercent >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                              fontFamily: 'monospace',
                              fontWeight: 700
                            }}>
                              {q.changePercent >= 0 ? '▲' : '▼'} {Math.abs(q.changePercent).toFixed(2)}%
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
                  );
                })}
              </div>
            )}

            {leftSidebarView === 'indices' && (
              <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
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
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      marginBottom: '8px',
                      cursor: 'pointer'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div>
                        <div style={{ fontSize: '12px', fontWeight: 700, color: BLOOMBERG.WHITE }}>{index.symbol}</div>
                        <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>{index.exchange}</div>
                      </div>
                      <div style={{ textAlign: 'right' }}>
                        <div style={{ fontSize: '12px', fontWeight: 600, color: BLOOMBERG.YELLOW }}>--</div>
                        <div style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>--</div>
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
                  color: BLOOMBERG.GRAY,
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
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
                      marginBottom: '6px',
                      display: 'flex',
                      justifyContent: 'space-between',
                      alignItems: 'center',
                      cursor: 'pointer'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.CYAN;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                    }}
                  >
                    <span style={{ fontSize: '11px', color: BLOOMBERG.WHITE }}>{sector}</span>
                    <span style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>--</span>
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
            backgroundColor: BLOOMBERG.PANEL_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            minHeight: '300px',
            transition: 'flex 0.3s ease'
          }}>
            {/* Chart Header - Compact */}
            <div style={{
              padding: '4px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
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
                    backgroundColor: centerView === view.toLowerCase() ? BLOOMBERG.ORANGE : 'transparent',
                    border: 'none',
                    color: centerView === view.toLowerCase() ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
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
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.GRAY,
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
              color: BLOOMBERG.GRAY,
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
                  <BarChart3 size={48} color={BLOOMBERG.GRAY} />
                  <span style={{ fontSize: '14px', color: BLOOMBERG.GRAY }}>Market Depth Chart</span>
                  <span style={{ fontSize: '11px', color: BLOOMBERG.MUTED }}>Connect to broker for real-time depth data</span>
                </div>
              )}
              {centerView === 'trades' && (
                <div style={{ width: '100%', height: '100%', overflow: 'auto', padding: '8px' }}>
                  {recentTrades.length === 0 ? (
                    <div style={{
                      padding: '40px',
                      textAlign: 'center',
                      color: BLOOMBERG.GRAY,
                      fontSize: '11px'
                    }}>
                      No recent trades
                    </div>
                  ) : (
                    <table style={{ width: '100%', fontSize: '10px', borderCollapse: 'collapse' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                          <th style={{ padding: '6px', textAlign: 'left', color: BLOOMBERG.GRAY }}>TIME</th>
                          <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG.GRAY }}>PRICE</th>
                          <th style={{ padding: '6px', textAlign: 'right', color: BLOOMBERG.GRAY }}>QTY</th>
                        </tr>
                      </thead>
                      <tbody>
                        {recentTrades.slice(0, 20).map((trade, idx) => (
                          <tr key={idx} style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}40` }}>
                            <td style={{ padding: '4px 6px', color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                              {new Date(trade.timestamp).toLocaleTimeString()}
                            </td>
                            <td style={{
                              padding: '4px 6px',
                              textAlign: 'right',
                              color: trade.side === 'buy' ? BLOOMBERG.GREEN : BLOOMBERG.RED
                            }}>
                              ₹{trade.price?.toFixed(2)}
                            </td>
                            <td style={{ padding: '4px 6px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
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

          {/* Bottom Panel - Positions/Holdings/Orders - Compact by default */}
          <div style={{
            flex: isBottomPanelMinimized ? '0 0 32px' : '0 0 200px',
            minHeight: isBottomPanelMinimized ? '32px' : '150px',
            maxHeight: isBottomPanelMinimized ? '32px' : '220px',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            transition: 'all 0.3s ease'
          }}>
            {/* Bottom Panel Header */}
            <div style={{
              padding: '6px 12px',
              backgroundColor: BLOOMBERG.HEADER_BG,
              borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
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
                ] as const).map((tab) => (
                  <button
                    key={tab.id}
                    onClick={() => setActiveBottomTab(tab.id)}
                    style={{
                      padding: '6px 16px',
                      backgroundColor: activeBottomTab === tab.id ? BLOOMBERG.ORANGE : 'transparent',
                      border: 'none',
                      color: activeBottomTab === tab.id ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
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
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>TOTAL POSITIONS VALUE</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.CYAN }}>
                        ₹{totalPositionValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>DAY P&L</div>
                      <div style={{
                        fontSize: '18px',
                        fontWeight: 700,
                        color: totalPositionPnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                      }}>
                        {totalPositionPnl >= 0 ? '+' : ''}₹{totalPositionPnl.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>PORTFOLIO VALUE</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.YELLOW }}>
                        ₹{totalHoldingsValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
                      </div>
                    </div>
                    <div style={{
                      padding: '16px',
                      backgroundColor: BLOOMBERG.HEADER_BG,
                      border: `1px solid ${BLOOMBERG.BORDER}`
                    }}>
                      <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>AVAILABLE MARGIN</div>
                      <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.PURPLE }}>
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
              </div>
            )}
          </div>
        </div>

        {/* RIGHT SIDEBAR - Order Entry & Market Depth */}
        <div style={{
          width: '280px',
          minWidth: '280px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderLeft: `1px solid ${BLOOMBERG.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0
        }}>
          {/* Order Entry Section */}
          <div style={{
            height: '50%',
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
              {!isAuthenticated ? (
                <BrokerSetupPanel />
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
                {(['orderbook', 'marketdepth', 'info'] as RightPanelView[]).map((view) => (
                  <button
                    key={view}
                    onClick={() => setRightPanelView(view)}
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
                    <div style={{ color: BLOOMBERG.GREEN, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      BIDS (BUY ORDERS)
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                          <th style={{ padding: '4px', textAlign: 'left', color: BLOOMBERG.GRAY }}>QTY</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>PRICE</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>ORDERS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {[1, 2, 3, 4, 5].map((i) => (
                          <tr key={i} style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}40` }}>
                            <td style={{ padding: '4px', color: BLOOMBERG.WHITE }}>--</td>
                            <td style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GREEN }}>--</td>
                            <td style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>--</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>

                  {/* Spread Divider */}
                  <div style={{
                    padding: '8px',
                    textAlign: 'center',
                    backgroundColor: BLOOMBERG.HEADER_BG,
                    border: `1px solid ${BLOOMBERG.BORDER}`,
                    fontSize: '10px'
                  }}>
                    <span style={{ color: BLOOMBERG.GRAY }}>Spread: </span>
                    <span style={{ color: BLOOMBERG.YELLOW, fontWeight: 600 }}>
                      {spread > 0 ? `₹${spread.toFixed(2)} (${spreadPercent.toFixed(3)}%)` : '--'}
                    </span>
                  </div>

                  {/* Sell Orders (Asks) */}
                  <div>
                    <div style={{ color: BLOOMBERG.RED, fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>
                      ASKS (SELL ORDERS)
                    </div>
                    <table style={{ width: '100%', fontSize: '10px' }}>
                      <thead>
                        <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                          <th style={{ padding: '4px', textAlign: 'left', color: BLOOMBERG.GRAY }}>PRICE</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>QTY</th>
                          <th style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>ORDERS</th>
                        </tr>
                      </thead>
                      <tbody>
                        {[1, 2, 3, 4, 5].map((i) => (
                          <tr key={i} style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}40` }}>
                            <td style={{ padding: '4px', color: BLOOMBERG.RED }}>--</td>
                            <td style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.WHITE }}>--</td>
                            <td style={{ padding: '4px', textAlign: 'right', color: BLOOMBERG.GRAY }}>--</td>
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
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexDirection: 'column',
                  gap: '12px'
                }}>
                  <Layers size={48} color={BLOOMBERG.GRAY} />
                  <span style={{ fontSize: '12px', color: BLOOMBERG.GRAY }}>Market Depth</span>
                  <span style={{ fontSize: '10px', color: BLOOMBERG.MUTED, textAlign: 'center' }}>
                    Connect to broker for<br />real-time depth visualization
                  </span>
                </div>
              )}

              {rightPanelView === 'info' && (
                <div style={{ padding: '8px' }}>
                  <div style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: BLOOMBERG.GRAY,
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
                          backgroundColor: BLOOMBERG.HEADER_BG,
                          border: `1px solid ${BLOOMBERG.BORDER}`
                        }}
                      >
                        <span style={{ fontSize: '10px', color: BLOOMBERG.GRAY }}>{item.label}</span>
                        <span style={{ fontSize: '10px', color: BLOOMBERG.WHITE, fontWeight: 600 }}>{item.value}</span>
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
                backgroundColor: BLOOMBERG.HEADER_BG,
                borderTop: `1px solid ${BLOOMBERG.BORDER}`,
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
        <span>Fincept Terminal v3.1.4 | Equity Trading Platform</span>
        <span>
          Broker: <span style={{ color: BLOOMBERG.ORANGE }}>{activeBrokerMetadata?.displayName?.toUpperCase() || 'NONE'}</span> |
          Mode: <span style={{ color: isPaper ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
            {tradingMode.toUpperCase()}
          </span> |
          Status: <span style={{ color: isConnected ? BLOOMBERG.GREEN : BLOOMBERG.RED }}>
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
