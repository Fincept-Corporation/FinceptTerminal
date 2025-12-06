// File: src/components/tabs/TradingTab.tsx
// Unified trading tab with multi-provider support and paper trading
// Uses new broker adapter architecture

import React, { useState, useEffect, useRef, useCallback } from 'react';
import { Activity, TrendingUp } from 'lucide-react';
import { ProviderSwitcher } from './trading/ProviderSwitcher';
import { PaperTradingPanel } from './trading/PaperTradingPanel';
import { TickerDisplay } from './trading/TickerDisplay';
import { OrderBook } from './trading/OrderBook';
import { TradesFeed } from './trading/TradesFeed';
import { OrderForm } from './trading/OrderForm';
import { PositionsTable } from './trading/PositionsTable';
import { OrdersTable } from './trading/OrdersTable';
import { TradeHistory } from './trading/TradeHistory';
import { PaperTradingStats } from './trading/PaperTradingStats';
import { useProviderContext } from '../../contexts/ProviderContext';
import { paperTradingDatabase } from '../../paper-trading';
import { useWebSocket } from '../../hooks/useWebSocket';
import { getWebSocketManager } from '../../services/websocket';
import { createExchangeAdapter } from '../../brokers/crypto';
import { createPaperTradingAdapter } from '../../paper-trading';
import type { IExchangeAdapter } from '../../brokers/crypto/types';
import type { Position as CCXTPosition } from 'ccxt';

// Symbol configurations per provider
const KRAKEN_PAIRS = ['BTC/USD', 'ETH/USD', 'SOL/USD', 'AVAX/USD', 'MATIC/USD'];
const HYPERLIQUID_SYMBOLS = ['BTC/USDC:USDC', 'ETH/USDC:USDC', 'SOL/USDC:USDC', 'AVAX/USDC:USDC', 'MATIC/USDC:USDC'];

// Bloomberg color scheme
const BLOOMBERG_COLORS = {
  ORANGE: '#ea580c',
  WHITE: '#ffffff',
  RED: '#dc2626',
  GREEN: '#10b981',
  GRAY: '#525252',
  DARK_BG: '#0d0d0d',
  PANEL_BG: '#1a1a1a',
  CYAN: '#22d3ee',
  YELLOW: '#fbbf24'
};

export function TradingTab() {
  const { activeProvider } = useProviderContext();
  const [portfolioId, setPortfolioId] = useState<string | null>(null);
  const [isResetting, setIsResetting] = useState(false);
  const [currentTime, setCurrentTime] = useState(new Date());
  const [isConnected, setIsConnected] = useState(false);
  const [positions, setPositions] = useState<CCXTPosition[]>([]);
  const [balance, setBalance] = useState<number>(0);
  const [totalPnL, setTotalPnL] = useState<number>(0);
  const [isLoading, setIsLoading] = useState(false);
  const [orders, setOrders] = useState<any[]>([]);
  const [trades, setTrades] = useState<any[]>([]);
  const [stats, setStats] = useState<any>(null);

  // Get symbols based on active provider
  const popularSymbols = activeProvider === 'kraken' ? KRAKEN_PAIRS : HYPERLIQUID_SYMBOLS;
  const [selectedSymbol, setSelectedSymbol] = useState(popularSymbols[0]);

  // Adapter refs
  const realAdapterRef = useRef<any>(null);
  const paperAdapterRef = useRef<any>(null);

  // Initialize WebSocket connection for active provider
  useEffect(() => {
    const manager = getWebSocketManager();

    const initProvider = async () => {
      try {
        console.log(`[TradingTab] Initializing ${activeProvider} provider...`);

        if (activeProvider === 'kraken') {
          manager.setProviderConfig('kraken', {
            provider_name: 'kraken',
            enabled: true,
            endpoint: 'wss://ws.kraken.com/v2'
          });
          await manager.connect('kraken');
          console.log('[TradingTab] Kraken connected successfully');
        } else if (activeProvider === 'hyperliquid') {
          manager.setProviderConfig('hyperliquid', {
            provider_name: 'hyperliquid',
            enabled: true,
            endpoint: 'wss://api.hyperliquid.xyz/ws'
          });
          await manager.connect('hyperliquid');
          console.log('[TradingTab] HyperLiquid connected successfully');
        }
        setIsConnected(true);
      } catch (error) {
        console.error(`[TradingTab] Failed to connect to ${activeProvider}:`, error);
        setIsConnected(false);
      }
    };

    initProvider();
  }, [activeProvider]);

  // Update selected symbol when provider changes
  useEffect(() => {
    const symbols = activeProvider === 'kraken' ? KRAKEN_PAIRS : HYPERLIQUID_SYMBOLS;
    setSelectedSymbol(symbols[0]);
  }, [activeProvider]);

  // Update clock
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Initialize real adapter when provider changes
  useEffect(() => {
    const initAdapter = async () => {
      try {
        // Cleanup old adapter
        if (realAdapterRef.current) {
          await realAdapterRef.current.disconnect();
        }

        // Create new adapter
        realAdapterRef.current = createExchangeAdapter(activeProvider as 'kraken' | 'hyperliquid');
        await realAdapterRef.current?.connect();
        console.log(`[TradingTab] ${activeProvider} adapter connected`);
      } catch (error) {
        console.error(`[TradingTab] Failed to initialize ${activeProvider} adapter:`, error);
      }
    };

    initAdapter();

    return () => {
      if (realAdapterRef.current) {
        realAdapterRef.current.disconnect();
      }
    };
  }, [activeProvider]);

  // Initialize paper trading adapter when portfolio is created
  useEffect(() => {
    const initPaperTrading = async () => {
      if (!portfolioId || !realAdapterRef.current) {
        if (paperAdapterRef.current) {
          // Remove event listeners
          paperAdapterRef.current.off('order', () => {});
          paperAdapterRef.current.off('position', () => {});
          paperAdapterRef.current.off('balance', () => {});
          await paperAdapterRef.current.disconnect();
          paperAdapterRef.current = null;
        }
        setPositions([]);
        setBalance(0);
        setTotalPnL(0);
        setOrders([]);
        setTrades([]);
        setStats(null);
        return;
      }

      try {
        setIsLoading(true);

        // Create paper trading adapter
        paperAdapterRef.current = createPaperTradingAdapter(
          {
            portfolioId,
            portfolioName: `${activeProvider.toUpperCase()} Paper Trading`,
            provider: activeProvider,
            assetClass: 'crypto',
            initialBalance: 100000,
            currency: 'USD',
            fees: {
              maker: activeProvider === 'kraken' ? 0.0002 : 0.0002,
              taker: activeProvider === 'kraken' ? 0.0005 : 0.0005,
            },
            slippage: {
              market: 0.001,
              limit: 0,
            },
            enableMarginTrading: true,
            defaultLeverage: 1,
            maxLeverage: activeProvider === 'hyperliquid' ? 50 : 5,
          },
          realAdapterRef.current
        );

        await paperAdapterRef.current.connect();

        // Set up event listeners for real-time updates
        const orderHandler = (event: any) => {
          console.log('[TradingTab] Order event:', event.data);
          refreshPaperTradingData();
        };

        const positionHandler = (event: any) => {
          console.log('[TradingTab] Position event:', event.data);
          refreshPaperTradingData();
        };

        const balanceHandler = (event: any) => {
          console.log('[TradingTab] Balance event:', event.data);
          refreshPaperTradingData();
        };

        paperAdapterRef.current.on('order', orderHandler);
        paperAdapterRef.current.on('position', positionHandler);
        paperAdapterRef.current.on('balance', balanceHandler);
        console.log('[TradingTab] Paper trading adapter connected');

        // Load initial data
        await refreshPaperTradingData();
      } catch (error) {
        console.error('[TradingTab] Failed to initialize paper trading:', error);
      } finally {
        setIsLoading(false);
      }
    };

    initPaperTrading();

    return () => {
      if (paperAdapterRef.current) {
        paperAdapterRef.current.off('order', () => {});
        paperAdapterRef.current.off('position', () => {});
        paperAdapterRef.current.off('balance', () => {});
        paperAdapterRef.current.disconnect();
      }
    };
  }, [portfolioId, activeProvider]);

  // Refresh paper trading data
  const refreshPaperTradingData = useCallback(async () => {
    if (!paperAdapterRef.current || !portfolioId) return;

    try {
      const [balanceData, positionsData, ordersData, tradesData] = await Promise.all([
        paperAdapterRef.current.fetchBalance(),
        paperAdapterRef.current.fetchPositions(),
        paperTradingDatabase.getPortfolioOrders(portfolioId),
        paperTradingDatabase.getPortfolioTrades(portfolioId, 100),
      ]);

      // Calculate balance and P&L
      const usdBalance = (balanceData.free as any)?.['USD'] || (balanceData.free as any)?.['USDC'] || 100000;
      const unrealizedPnL = positionsData.reduce((sum: number, p: any) => sum + (p.unrealizedPnl || 0), 0);

      // Calculate stats
      const closedTrades = tradesData.filter((t: any) => t.side === 'sell');
      const totalPnL = unrealizedPnL + closedTrades.reduce((sum: number, t: any) => sum + (t.price * t.quantity), 0);
      const winningTrades = closedTrades.filter((t: any) => t.price * t.quantity > 0).length;
      const losingTrades = closedTrades.filter((t: any) => t.price * t.quantity < 0).length;
      const winRate = closedTrades.length > 0 ? (winningTrades / closedTrades.length) * 100 : 0;
      const totalFees = tradesData.reduce((sum: number, t: any) => sum + t.fee, 0);
      const wins = closedTrades.filter((t: any) => t.price * t.quantity > 0).map((t: any) => t.price * t.quantity);
      const losses = closedTrades.filter((t: any) => t.price * t.quantity < 0).map((t: any) => Math.abs(t.price * t.quantity));
      const averageWin = wins.length > 0 ? wins.reduce((a: number, b: number) => a + b, 0) / wins.length : 0;
      const averageLoss = losses.length > 0 ? losses.reduce((a: number, b: number) => a + b, 0) / losses.length : 0;
      const largestWin = wins.length > 0 ? Math.max(...wins) : 0;
      const largestLoss = losses.length > 0 ? Math.max(...losses) : 0;
      const totalWins = wins.reduce((a: number, b: number) => a + b, 0);
      const totalLosses = losses.reduce((a: number, b: number) => a + b, 0);
      const profitFactor = totalLosses > 0 ? totalWins / totalLosses : 0;

      setBalance(usdBalance);
      setPositions(positionsData);
      setTotalPnL(unrealizedPnL);
      setOrders(ordersData);
      setTrades(tradesData);
      setStats({
        totalTrades: closedTrades.length,
        winningTrades,
        losingTrades,
        winRate,
        totalPnL,
        realizedPnL: totalPnL - unrealizedPnL,
        unrealizedPnL,
        totalFees,
        averageWin,
        averageLoss,
        largestWin,
        largestLoss,
        profitFactor,
      });
    } catch (error) {
      console.error('[TradingTab] Failed to refresh paper trading data:', error);
    }
  }, [portfolioId]);

  // Auto-refresh positions every 5 seconds
  useEffect(() => {
    if (!portfolioId) return;

    const interval = setInterval(() => {
      refreshPaperTradingData();
    }, 5000);

    return () => clearInterval(interval);
  }, [portfolioId, refreshPaperTradingData]);

  // Get current price from ticker
  const { message: tickerMessage } = useWebSocket(
    isConnected ? `${activeProvider}.ticker.${selectedSymbol}` : null,
    null,
    { autoSubscribe: true }
  );

  const currentPrice = tickerMessage?.data?.last || tickerMessage?.data?.close || tickerMessage?.data?.mid;

  // Handle portfolio creation
  const handlePortfolioChange = useCallback((newPortfolioId: string | null) => {
    setPortfolioId(newPortfolioId);
  }, []);

  // Handle order placement
  const handlePlaceOrder = async (order: {
    symbol: string;
    side: 'buy' | 'sell';
    type: string;
    quantity: number;
    price?: number;
    stopPrice?: number;
  }) => {
    if (!portfolioId || !paperAdapterRef.current) {
      throw new Error('Paper trading not enabled');
    }

    try {
      setIsLoading(true);

      await paperAdapterRef.current.createOrder(
        order.symbol,
        order.type as any,
        order.side,
        order.quantity,
        order.price,
        order.stopPrice ? { stopPrice: order.stopPrice } : undefined
      );

      // Refresh data
      await refreshPaperTradingData();
    } catch (error) {
      console.error('[TradingTab] Failed to place order:', error);
      throw error;
    } finally {
      setIsLoading(false);
    }
  };

  // Handle position close
  const handleClosePosition = async (positionId: string) => {
    if (!portfolioId || !paperAdapterRef.current) return;

    try {
      setIsLoading(true);

      // Find position to close
      const position = positions.find((p) => p.id === positionId);
      if (!position) {
        throw new Error('Position not found');
      }

      // Close position by placing opposite order
      const closeSide = position.side === 'long' ? 'sell' : 'buy';
      await paperAdapterRef.current.createOrder(
        position.symbol,
        'market',
        closeSide,
        Math.abs(position.contracts || 0),
        undefined,
        { reduceOnly: true }
      );

      // Refresh data
      await refreshPaperTradingData();
    } catch (error) {
      console.error('[TradingTab] Failed to close position:', error);
    } finally {
      setIsLoading(false);
    }
  };

  // Handle order cancellation
  const handleCancelOrder = async (orderId: string) => {
    if (!portfolioId || !paperAdapterRef.current) return;

    try {
      setIsLoading(true);
      // Find the order's symbol first
      const order = orders.find((o) => o.id === orderId);
      if (!order) {
        throw new Error('Order not found');
      }
      await paperAdapterRef.current.cancelOrder(orderId, order.symbol);
      await refreshPaperTradingData();
    } catch (error) {
      console.error('[TradingTab] Failed to cancel order:', error);
    } finally {
      setIsLoading(false);
    }
  };

  // Handle portfolio reset
  const handleReset = async () => {
    if (!portfolioId) return;

    setIsResetting(true);
    try {
      // For now, just recreate the adapter
      // TODO: Implement proper reset in PaperTradingDatabase
      if (paperAdapterRef.current) {
        await paperAdapterRef.current.disconnect();
      }

      // Trigger re-initialization
      const currentId = portfolioId;
      setPortfolioId(null);
      setTimeout(() => setPortfolioId(currentId), 100);
    } finally {
      setIsResetting(false);
    }
  };

  const { ORANGE, WHITE, RED, GREEN, GRAY, DARK_BG, PANEL_BG, CYAN } = BLOOMBERG_COLORS;

  return (
    <div style={{
      height: '100%',
      backgroundColor: DARK_BG,
      color: WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: #1a1a1a; }
        *::-webkit-scrollbar-thumb { background: #404040; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #525252; }
      `}</style>

      {/* Header Bar */}
      <div style={{
        backgroundColor: PANEL_BG,
        borderBottom: `1px solid ${GRAY}`,
        padding: '8px 12px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            <TrendingUp size={16} style={{ color: ORANGE }} />
            <span style={{ color: ORANGE, fontWeight: 'bold', fontSize: '14px' }}>
              FINCEPT TRADING TERMINAL
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: portfolioId ? GREEN : GRAY, fontSize: '10px' }}>
              ● {portfolioId ? 'PAPER TRADING' : 'VIEW ONLY'}
            </span>
            <span style={{ color: WHITE }}>|</span>
            <span style={{ color: CYAN, fontSize: '11px' }}>
              {currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC
            </span>
          </div>

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Activity size={14} style={{ color: GREEN }} />
            <span style={{ color: GREEN, fontSize: '10px', fontWeight: 'bold' }}>
              LIVE MARKET DATA
            </span>
          </div>
        </div>
      </div>

      {/* Provider Switcher */}
      <ProviderSwitcher />

      {/* Paper Trading Panel */}
      <PaperTradingPanel
        portfolioId={portfolioId}
        balance={balance}
        totalPnL={totalPnL}
        onPortfolioChange={handlePortfolioChange}
        onReset={handleReset}
        isResetting={isResetting}
      />

      {/* Symbol Selector */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        backgroundColor: PANEL_BG,
        padding: '8px 12px',
        borderBottom: `1px solid ${GRAY}`,
        flexShrink: 0
      }}>
        <span style={{ fontSize: '10px', color: GRAY, fontWeight: 'bold', textTransform: 'uppercase' }}>
          {activeProvider === 'kraken' ? 'Pair:' : 'Symbol:'}
        </span>
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
          {popularSymbols.map(symbol => (
            <button
              key={symbol}
              onClick={() => setSelectedSymbol(symbol)}
              style={{
                padding: '4px 12px',
                fontSize: '11px',
                fontWeight: 'bold',
                textTransform: 'uppercase',
                backgroundColor: selectedSymbol === symbol ? ORANGE : '#2d2d2d',
                color: selectedSymbol === symbol ? WHITE : GRAY,
                border: selectedSymbol === symbol ? `1px solid ${ORANGE}` : '1px solid #404040',
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => {
                if (selectedSymbol !== symbol) {
                  e.currentTarget.style.backgroundColor = '#404040';
                  e.currentTarget.style.borderColor = GRAY;
                }
              }}
              onMouseLeave={(e) => {
                if (selectedSymbol !== symbol) {
                  e.currentTarget.style.backgroundColor = '#2d2d2d';
                  e.currentTarget.style.borderColor = '#404040';
                }
              }}
            >
              {symbol}
            </button>
          ))}
        </div>
        <input
          type="text"
          placeholder={activeProvider === 'kraken' ? 'BTC/USD' : 'BTC/USDC:USDC'}
          value={selectedSymbol}
          onChange={(e) => setSelectedSymbol(e.target.value.toUpperCase())}
          style={{
            marginLeft: '12px',
            padding: '4px 8px',
            backgroundColor: '#2d2d2d',
            color: WHITE,
            border: '1px solid #404040',
            fontSize: '11px',
            outline: 'none',
            width: '150px'
          }}
          onFocus={(e) => e.currentTarget.style.borderColor = ORANGE}
          onBlur={(e) => e.currentTarget.style.borderColor = '#404040'}
        />
        <span style={{ fontSize: '10px', color: isConnected ? GREEN : GRAY, marginLeft: '12px' }}>
          {isConnected ? '● CONNECTED' : '● CONNECTING...'}
        </span>
      </div>

      {/* Main Trading Layout */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {/* Left Panel - Ticker & Trades */}
        <div style={{
          width: '280px',
          display: 'flex',
          flexDirection: 'column',
          borderRight: `1px solid ${GRAY}`,
          backgroundColor: PANEL_BG
        }}>
          <TickerDisplay symbol={selectedSymbol} provider={activeProvider} />
          <TradesFeed symbol={selectedSymbol} provider={activeProvider} />
        </div>

        {/* Center Panel - Chart Placeholder */}
        <div style={{
          flex: 1,
          display: 'flex',
          flexDirection: 'column',
          backgroundColor: DARK_BG
        }}>
          <div style={{
            flex: 1,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            borderBottom: `1px solid ${GRAY}`
          }}>
            <div style={{ textAlign: 'center', color: GRAY }}>
              <div style={{ fontSize: '48px', marginBottom: '12px' }}>📊</div>
              <div style={{ fontSize: '14px', fontWeight: 'bold', color: WHITE, marginBottom: '8px' }}>
                ADVANCED CHARTING
              </div>
              <div style={{ fontSize: '11px', color: GRAY }}>
                TradingView Integration Coming Soon
              </div>
              <div style={{ fontSize: '10px', marginTop: '8px', color: GRAY }}>
                Showing {selectedSymbol} on {activeProvider.toUpperCase()}
              </div>
            </div>
          </div>

          {/* Bottom Panel - Positions/Orders/Trades */}
          {portfolioId && (
            <div style={{ display: 'flex', flexDirection: 'column', maxHeight: '400px' }}>
              <PositionsTable
                positions={positions}
                onClosePosition={handleClosePosition}
                isLoading={isLoading}
              />
              <OrdersTable
                orders={orders}
                onCancelOrder={handleCancelOrder}
                isLoading={isLoading}
              />
              <TradeHistory trades={trades} />
            </div>
          )}
        </div>

        {/* Right Panel - Order Book & Order Form & Stats */}
        <div style={{
          width: '320px',
          display: 'flex',
          flexDirection: 'column',
          borderLeft: `1px solid ${GRAY}`,
          backgroundColor: PANEL_BG,
          overflow: 'hidden'
        }}>
          <OrderBook symbol={selectedSymbol} provider={activeProvider} depth={25} />

          {/* Order Form (only if paper trading enabled) */}
          {portfolioId && (
            <OrderForm
              symbol={selectedSymbol}
              currentPrice={currentPrice}
              onPlaceOrder={handlePlaceOrder}
              isLoading={isLoading}
            />
          )}

          {/* Paper Trading Stats (only if paper trading enabled) */}
          {portfolioId && stats && (
            <div style={{ overflowY: 'auto' }}>
              <PaperTradingStats
                stats={stats}
                balance={balance}
                initialBalance={100000}
              />
            </div>
          )}

          {/* Paper Trading Disabled Message */}
          {!portfolioId && (
            <div style={{
              backgroundColor: DARK_BG,
              borderTop: `1px solid ${GRAY}`,
              padding: '16px',
              textAlign: 'center'
            }}>
              <div style={{ fontSize: '12px', color: GRAY, marginBottom: '8px', fontWeight: 'bold' }}>
                PAPER TRADING DISABLED
              </div>
              <div style={{ fontSize: '10px', color: GRAY, lineHeight: '1.4' }}>
                Enable paper trading to place orders and manage positions with $100,000 virtual balance
              </div>
            </div>
          )}
        </div>
      </div>

      {/* Status Bar / Footer */}
      <div style={{
        borderTop: `1px solid ${GRAY}`,
        backgroundColor: PANEL_BG,
        padding: '6px 12px',
        fontSize: '9px',
        color: GRAY,
        flexShrink: 0
      }}>
        <div style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          flexWrap: 'wrap',
          gap: '8px'
        }}>
          <span>
            Fincept Trading Terminal v1.0.0 | WebSocket real-time data via {activeProvider.toUpperCase()}
          </span>
          <span style={{ whiteSpace: 'nowrap' }}>
            {portfolioId ? (
              <>
                <span style={{ color: GREEN }}>● PAPER TRADING ACTIVE</span>
                <span style={{ marginLeft: '8px' }}>|</span>
                <span style={{ marginLeft: '8px' }}>
                  {positions.length} Open Position{positions.length !== 1 ? 's' : ''}
                </span>
              </>
            ) : (
              <>
                <span style={{ color: GRAY }}>● VIEW ONLY MODE</span>
                <span style={{ marginLeft: '8px' }}>|</span>
                <span style={{ marginLeft: '8px' }}>Enable paper trading to trade</span>
              </>
            )}
          </span>
        </div>
      </div>
    </div>
  );
}
