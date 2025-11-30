// File: src/components/tabs/TradingTab.tsx
// Unified trading tab with multi-provider support and paper trading

import React, { useState, useEffect } from 'react';
import { Activity, TrendingUp } from 'lucide-react';
import { ProviderSwitcher } from './trading/ProviderSwitcher';
import { PaperTradingPanel } from './trading/PaperTradingPanel';
import { TickerDisplay } from './trading/TickerDisplay';
import { OrderBook } from './trading/OrderBook';
import { TradesFeed } from './trading/TradesFeed';
import { OrderForm } from './trading/OrderForm';
import { PositionsTable } from './trading/PositionsTable';
import { useProviderContext } from '../../contexts/ProviderContext';
import { usePaperTrading } from '../../hooks/usePaperTrading';
import { useWebSocket } from '../../hooks/useWebSocket';
import { getWebSocketManager } from '../../services/websocket';
import type { OrderRequest } from '../../types/trading';

// Symbol configurations per provider
const KRAKEN_PAIRS = ['BTC/USD', 'ETH/USD', 'SOL/USD', 'AVAX/USD', 'MATIC/USD'];
const HYPERLIQUID_SYMBOLS = ['BTC', 'ETH', 'SOL', 'AVAX', 'MATIC'];

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

  // Get symbols based on active provider
  const popularSymbols = activeProvider === 'kraken' ? KRAKEN_PAIRS : HYPERLIQUID_SYMBOLS;
  const [selectedSymbol, setSelectedSymbol] = useState(popularSymbols[0]);

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

  // Paper trading hook
  const {
    portfolio,
    positions,
    orders,
    isLoading: isPaperTradingLoading,
    placeOrder,
    closePosition,
    resetPortfolio
  } = usePaperTrading({ portfolioId: portfolioId || undefined });

  // Get current price from ticker
  const { message: tickerMessage } = useWebSocket(
    isConnected ? `${activeProvider}.ticker.${selectedSymbol}` : null,
    null,
    { autoSubscribe: true }
  );

  const currentPrice = tickerMessage?.data?.last || tickerMessage?.data?.close || tickerMessage?.data?.mid;

  // Handle order placement
  const handlePlaceOrder = async (order: OrderRequest) => {
    if (!portfolioId) {
      throw new Error('Paper trading not enabled');
    }

    const result = await placeOrder(order);

    if (!result.success) {
      throw new Error(result.error || 'Failed to place order');
    }
  };

  // Handle position close
  const handleClosePosition = async (positionId: string) => {
    await closePosition(positionId);
  };

  // Handle portfolio reset
  const handleReset = async () => {
    if (!portfolioId) return;

    setIsResetting(true);
    try {
      await resetPortfolio();
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
        balance={portfolio?.balance}
        totalPnL={portfolio?.totalPnL}
        onPortfolioChange={setPortfolioId}
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
          placeholder={activeProvider === 'kraken' ? 'BTC/USD' : 'BTC'}
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

          {/* Positions Table (if paper trading enabled) */}
          {portfolioId && (
            <PositionsTable
              positions={positions}
              onClosePosition={handleClosePosition}
              isLoading={isPaperTradingLoading}
            />
          )}
        </div>

        {/* Right Panel - Order Book & Order Form */}
        <div style={{
          width: '320px',
          display: 'flex',
          flexDirection: 'column',
          borderLeft: `1px solid ${GRAY}`,
          backgroundColor: PANEL_BG
        }}>
          <OrderBook symbol={selectedSymbol} provider={activeProvider} depth={25} />

          {/* Order Form (only if paper trading enabled) */}
          {portfolioId && (
            <OrderForm
              symbol={selectedSymbol}
              currentPrice={currentPrice}
              onPlaceOrder={handlePlaceOrder}
              isLoading={isPaperTradingLoading}
            />
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
                  {positions.filter(p => p.status === 'open').length} Open Position{positions.filter(p => p.status === 'open').length !== 1 ? 's' : ''}
                </span>
                <span style={{ marginLeft: '8px' }}>|</span>
                <span style={{ marginLeft: '8px' }}>
                  {orders.filter(o => o.status === 'pending').length} Pending Order{orders.filter(o => o.status === 'pending').length !== 1 ? 's' : ''}
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
