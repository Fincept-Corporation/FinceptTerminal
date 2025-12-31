/**
 * Paper Trading Panel
 * Complete UI for paper trading functionality
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  TrendingUp, TrendingDown, RefreshCw, RotateCcw, Activity,
  DollarSign, Target, TrendingUpIcon, BarChart3, AlertCircle,
  CheckCircle, XCircle, Info, Settings
} from 'lucide-react';
import { integrationManager } from '../integrations/IntegrationManager';
import { websocketBridge, TickerData } from '@/services/websocketBridge';

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
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

interface PaperTradingStats {
  portfolioId: string;
  portfolioName: string;
  initialBalance: number;
  currentBalance: number;
  totalValue: number;
  totalPnL: number;
  realizedPnL: number;
  unrealizedPnL: number;
  returnPercent: string;
  totalTrades: number;
  winningTrades: number;
  losingTrades: number;
  winRate: string;
  averageWin: string;
  averageLoss: string;
  largestWin: string;
  largestLoss: string;
  profitFactor: string;
  sharpeRatio: string | null;
  maxDrawdown: string | null;
  expectancy: string;
  kellyCriterion: string;
  avgHoldingPeriod: string | null;
  totalFees: string;
  riskRewardRatio: string;
}

// Add CSS animation for live indicator
const styles = `
  @keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.4; }
  }
`;

const PaperTradingPanel: React.FC = () => {
  const [isEnabled, setIsEnabled] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [balance, setBalance] = useState(1000000);
  const [statistics, setStatistics] = useState<PaperTradingStats | null>(null);
  const [positions, setPositions] = useState<any[]>([]);
  const [orders, setOrders] = useState<any[]>([]);
  const [error, setError] = useState<string | null>(null);
  const [showResetConfirm, setShowResetConfirm] = useState(false);
  const [resetAmount, setResetAmount] = useState('1000000');

  // WebSocket state for real-time prices
  const [realtimePrices, setRealtimePrices] = useState<Map<string, number>>(new Map());
  const wsListenerRef = useRef<(() => void) | null>(null);
  const subscribedSymbols = useRef<Set<string>>(new Set());

  // Load initial state
  useEffect(() => {
    loadPaperTradingState();
    const interval = setInterval(loadPaperTradingState, 5000); // Refresh every 5s
    return () => clearInterval(interval);
  }, [isEnabled]);

  // WebSocket: Subscribe to real-time prices for positions
  useEffect(() => {
    if (!isEnabled || positions.length === 0) {
      // Cleanup if disabled or no positions
      if (wsListenerRef.current) {
        wsListenerRef.current();
        wsListenerRef.current = null;
      }
      subscribedSymbols.current.clear();
      return;
    }

    const setupWebSocket = async () => {
      try {
        console.log('[PaperTradingPanel] Setting up WebSocket for', positions.length, 'positions');

        // Clean up old listener
        if (wsListenerRef.current) {
          wsListenerRef.current();
          wsListenerRef.current = null;
        }

        // Subscribe to each unique symbol
        const newSymbols = new Set<string>();
        for (const position of positions) {
          const symbol = position.symbol.replace('/INR', ''); // Clean symbol
          const exchange = position.exchange || 'NSE';
          const fyersSymbol = `${exchange}:${symbol}-EQ`;

          if (!subscribedSymbols.current.has(fyersSymbol)) {
            try {
              await websocketBridge.subscribe('fyers', fyersSymbol, 'ticker');
              newSymbols.add(fyersSymbol);
              console.log('[PaperTradingPanel] ✅ Subscribed to:', fyersSymbol);
            } catch (err) {
              console.error('[PaperTradingPanel] Failed to subscribe to', fyersSymbol, err);
            }
          }
        }

        // Update subscribed symbols
        subscribedSymbols.current = newSymbols;

        // Listen to ticker updates
        const unlisten = await websocketBridge.onTicker((tickerData: TickerData) => {
          if (tickerData.provider !== 'fyers') return;

          console.log('[PaperTradingPanel] 📡 Received ticker:', tickerData.symbol, tickerData.price);

          // Update real-time prices map
          setRealtimePrices(prev => {
            const updated = new Map(prev);
            // Store with clean symbol (without -EQ suffix)
            const cleanSymbol = tickerData.symbol.replace('-EQ', '').split(':')[1];
            updated.set(cleanSymbol, tickerData.price);
            return updated;
          });
        });

        wsListenerRef.current = unlisten;
        console.log('[PaperTradingPanel] ✅ WebSocket listener established');
      } catch (err) {
        console.error('[PaperTradingPanel] WebSocket setup failed:', err);
      }
    };

    setupWebSocket();

    return () => {
      if (wsListenerRef.current) {
        wsListenerRef.current();
        wsListenerRef.current = null;
      }
    };
  }, [isEnabled, positions]);

  const loadPaperTradingState = async () => {
    try {
      const enabled = integrationManager.isPaperTradingEnabled();
      setIsEnabled(enabled);

      if (enabled) {
        const [bal, stats, pos, ords] = await Promise.all([
          integrationManager.getPaperBalance(),
          integrationManager.getPaperStatistics(),
          integrationManager.getPaperPositions(),
          integrationManager.getPaperOrders(),
        ]);

        setBalance(bal);
        setStatistics(stats);
        setPositions(pos);
        setOrders(ords);
      }
    } catch (err: any) {
      console.error('[PaperTradingPanel] Error loading state:', err);
      setError(err.message);
    }
  };

  const handleTogglePaperTrading = async () => {
    setIsLoading(true);
    setError(null);

    try {
      if (isEnabled) {
        await integrationManager.disablePaperTrading();
        setIsEnabled(false);
        console.log('[PaperTradingPanel] Paper trading disabled');
      } else {
        await integrationManager.enablePaperTrading();
        setIsEnabled(true);
        console.log('[PaperTradingPanel] Paper trading enabled');
        await loadPaperTradingState();
      }
    } catch (err: any) {
      console.error('[PaperTradingPanel] Toggle error:', err);
      setError(err.message);
    } finally {
      setIsLoading(false);
    }
  };

  const handleReset = async () => {
    setIsLoading(true);
    setError(null);

    try {
      const amount = parseFloat(resetAmount);
      if (isNaN(amount) || amount <= 0) {
        throw new Error('Invalid reset amount');
      }

      await integrationManager.resetPaperAccount(amount);
      setShowResetConfirm(false);
      await loadPaperTradingState();
      console.log('[PaperTradingPanel] Account reset to:', amount);
    } catch (err: any) {
      console.error('[PaperTradingPanel] Reset error:', err);
      setError(err.message);
    } finally {
      setIsLoading(false);
    }
  };

  const formatCurrency = (value: number) => {
    return new Intl.NumberFormat('en-IN', {
      style: 'currency',
      currency: 'INR',
      minimumFractionDigits: 2,
    }).format(value);
  };

  const formatNumber = (value: string | number) => {
    const num = typeof value === 'string' ? parseFloat(value) : value;
    return isNaN(num) ? '0.00' : num.toFixed(2);
  };

  return (
    <>
      {/* Inject CSS animation */}
      <style>{styles}</style>

      <div style={{
        padding: '20px',
        backgroundColor: BLOOMBERG.DARK_BG,
      minHeight: '100%',
      color: BLOOMBERG.WHITE
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        marginBottom: '20px',
        paddingBottom: '15px',
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Activity size={28} color={BLOOMBERG.ORANGE} />
          <h2 style={{ margin: 0, fontSize: '24px', fontWeight: 'bold' }}>
            PAPER TRADING
          </h2>
          <div style={{
            backgroundColor: isEnabled ? BLOOMBERG.GREEN : BLOOMBERG.GRAY,
            color: BLOOMBERG.DARK_BG,
            padding: '4px 12px',
            borderRadius: '4px',
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            {isEnabled ? 'ACTIVE' : 'INACTIVE'}
          </div>
        </div>

        <div style={{ display: 'flex', gap: '10px' }}>
          <button
            onClick={() => loadPaperTradingState()}
            disabled={isLoading}
            style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.WHITE,
              padding: '8px 16px',
              borderRadius: '4px',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontFamily: 'monospace'
            }}
          >
            <RefreshCw size={16} className={isLoading ? 'spin' : ''} />
            REFRESH
          </button>

          <button
            onClick={() => setShowResetConfirm(true)}
            disabled={!isEnabled || isLoading}
            style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              color: BLOOMBERG.YELLOW,
              padding: '8px 16px',
              borderRadius: '4px',
              cursor: (!isEnabled || isLoading) ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              fontFamily: 'monospace',
              opacity: (!isEnabled || isLoading) ? 0.5 : 1
            }}
          >
            <RotateCcw size={16} />
            RESET
          </button>

          <button
            onClick={handleTogglePaperTrading}
            disabled={isLoading}
            style={{
              backgroundColor: isEnabled ? BLOOMBERG.RED : BLOOMBERG.GREEN,
              border: 'none',
              color: BLOOMBERG.WHITE,
              padding: '8px 20px',
              borderRadius: '4px',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              fontWeight: 'bold',
              fontFamily: 'monospace',
              fontSize: '14px'
            }}
          >
            {isLoading ? 'LOADING...' : isEnabled ? 'DISABLE' : 'ENABLE'}
          </button>
        </div>
      </div>

      {/* Error Message */}
      {error && (
        <div style={{
          backgroundColor: '#2A0A0A',
          border: `1px solid ${BLOOMBERG.RED}`,
          color: BLOOMBERG.RED,
          padding: '12px',
          borderRadius: '4px',
          marginBottom: '20px',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <AlertCircle size={20} />
          {error}
        </div>
      )}

      {/* Reset Confirmation Modal */}
      {showResetConfirm && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.8)',
          display: 'flex',
          justifyContent: 'center',
          alignItems: 'center',
          zIndex: 1000
        }}>
          <div style={{
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `2px solid ${BLOOMBERG.ORANGE}`,
            borderRadius: '8px',
            padding: '30px',
            maxWidth: '400px',
            width: '90%'
          }}>
            <h3 style={{ marginTop: 0, color: BLOOMBERG.ORANGE }}>
              RESET PAPER TRADING ACCOUNT
            </h3>
            <p style={{ color: BLOOMBERG.GRAY, marginBottom: '20px' }}>
              This will close all positions, cancel all orders, and reset your balance.
              This action cannot be undone.
            </p>
            <div style={{ marginBottom: '20px' }}>
              <label style={{ display: 'block', marginBottom: '8px', color: BLOOMBERG.WHITE }}>
                Initial Balance (₹)
              </label>
              <input
                type="number"
                value={resetAmount}
                onChange={(e) => setResetAmount(e.target.value)}
                style={{
                  width: '100%',
                  padding: '10px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  borderRadius: '4px',
                  fontFamily: 'monospace',
                  fontSize: '16px'
                }}
              />
            </div>
            <div style={{ display: 'flex', gap: '10px' }}>
              <button
                onClick={() => setShowResetConfirm(false)}
                style={{
                  flex: 1,
                  padding: '10px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontFamily: 'monospace'
                }}
              >
                CANCEL
              </button>
              <button
                onClick={handleReset}
                disabled={isLoading}
                style={{
                  flex: 1,
                  padding: '10px',
                  backgroundColor: BLOOMBERG.RED,
                  border: 'none',
                  color: BLOOMBERG.WHITE,
                  borderRadius: '4px',
                  cursor: isLoading ? 'not-allowed' : 'pointer',
                  fontWeight: 'bold',
                  fontFamily: 'monospace'
                }}
              >
                {isLoading ? 'RESETTING...' : 'CONFIRM RESET'}
              </button>
            </div>
          </div>
        </div>
      )}

      {!isEnabled ? (
        /* Disabled State */
        <div style={{
          textAlign: 'center',
          padding: '60px 20px',
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderRadius: '8px',
          border: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          <Info size={48} color={BLOOMBERG.GRAY} style={{ marginBottom: '20px' }} />
          <h3 style={{ color: BLOOMBERG.GRAY, marginBottom: '10px' }}>
            Paper Trading is Disabled
          </h3>
          <p style={{ color: BLOOMBERG.GRAY, marginBottom: '20px' }}>
            Enable paper trading to practice trading without risking real money.
            All orders will be simulated with realistic execution.
          </p>
          <button
            onClick={handleTogglePaperTrading}
            disabled={isLoading}
            style={{
              backgroundColor: BLOOMBERG.GREEN,
              border: 'none',
              color: BLOOMBERG.WHITE,
              padding: '12px 24px',
              borderRadius: '4px',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              fontWeight: 'bold',
              fontFamily: 'monospace',
              fontSize: '16px'
            }}
          >
            ENABLE PAPER TRADING
          </button>
        </div>
      ) : (
        /* Enabled State - Show Stats */
        <div>
          {/* Balance Overview */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))',
            gap: '15px',
            marginBottom: '20px'
          }}>
            <StatCard
              label="BALANCE"
              value={formatCurrency(balance)}
              icon={<DollarSign size={20} />}
              color={BLOOMBERG.CYAN}
            />
            <StatCard
              label="TOTAL P&L"
              value={formatCurrency(statistics?.totalPnL || 0)}
              icon={<TrendingUp size={20} />}
              color={(statistics?.totalPnL || 0) >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
            />
            <StatCard
              label="RETURN"
              value={`${statistics?.returnPercent || '0.00'}%`}
              icon={<BarChart3 size={20} />}
              color={(parseFloat(statistics?.returnPercent || '0') >= 0) ? BLOOMBERG.GREEN : BLOOMBERG.RED}
            />
            <StatCard
              label="TOTAL TRADES"
              value={statistics?.totalTrades.toString() || '0'}
              icon={<Activity size={20} />}
              color={BLOOMBERG.ORANGE}
            />
          </div>

          {/* Performance Metrics */}
          {statistics && (
            <div style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '8px',
              padding: '20px',
              marginBottom: '20px'
            }}>
              <h3 style={{
                margin: '0 0 20px 0',
                fontSize: '16px',
                color: BLOOMBERG.ORANGE,
                borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                paddingBottom: '10px'
              }}>
                PERFORMANCE METRICS
              </h3>

              <div style={{
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(180px, 1fr))',
                gap: '15px'
              }}>
                <MetricRow label="Win Rate" value={`${statistics.winRate}%`} />
                <MetricRow label="Profit Factor" value={statistics.profitFactor} />
                <MetricRow label="Sharpe Ratio" value={statistics.sharpeRatio || 'N/A'} />
                <MetricRow label="Max Drawdown" value={statistics.maxDrawdown ? `${statistics.maxDrawdown}%` : 'N/A'} />
                <MetricRow label="Expectancy" value={formatCurrency(parseFloat(statistics.expectancy))} />
                <MetricRow label="Kelly Criterion" value={`${formatNumber(parseFloat(statistics.kellyCriterion) * 100)}%`} />
                <MetricRow label="Avg Win" value={formatCurrency(parseFloat(statistics.averageWin))} />
                <MetricRow label="Avg Loss" value={formatCurrency(parseFloat(statistics.averageLoss))} />
                <MetricRow label="Largest Win" value={formatCurrency(parseFloat(statistics.largestWin))} />
                <MetricRow label="Largest Loss" value={formatCurrency(parseFloat(statistics.largestLoss))} />
                <MetricRow label="Risk/Reward" value={statistics.riskRewardRatio} />
                <MetricRow label="Avg Hold Time" value={statistics.avgHoldingPeriod ? `${statistics.avgHoldingPeriod} min` : 'N/A'} />
              </div>
            </div>
          )}

          {/* Positions & Orders Summary */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '15px' }}>
            <div style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '8px',
              padding: '20px'
            }}>
              <h3 style={{
                margin: '0 0 15px 0',
                fontSize: '14px',
                color: BLOOMBERG.ORANGE
              }}>
                OPEN POSITIONS ({positions.length})
              </h3>
              {positions.length === 0 ? (
                <p style={{ color: BLOOMBERG.GRAY, fontSize: '12px' }}>No open positions</p>
              ) : (
                <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                  {positions.map(pos => {
                    // Get real-time price or fallback to lastPrice
                    const cleanSymbol = pos.symbol.replace('/INR', '');
                    const currentPrice = realtimePrices.get(cleanSymbol) || pos.lastPrice;
                    const hasRealtimePrice = realtimePrices.has(cleanSymbol);

                    // Recalculate P&L with real-time price
                    const priceDiff = currentPrice - pos.averagePrice;
                    const realTimePnL = pos.side === 'BUY'
                      ? priceDiff * pos.quantity
                      : -priceDiff * pos.quantity;

                    return (
                      <div key={pos.id} style={{
                        padding: '8px 0',
                        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                        fontSize: '12px'
                      }}>
                        <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                            <span style={{ fontWeight: 'bold' }}>{pos.symbol}</span>
                            {hasRealtimePrice && (
                              <span style={{
                                fontSize: '10px',
                                color: BLOOMBERG.CYAN,
                                animation: 'pulse 1.5s ease-in-out infinite'
                              }}>
                                ● LIVE
                              </span>
                            )}
                          </div>
                          <span style={{ color: realTimePnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontWeight: 'bold' }}>
                            {formatCurrency(realTimePnL)}
                          </span>
                        </div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
                          <span>{pos.quantity} @ {formatCurrency(pos.averagePrice)} • {pos.side}</span>
                          <span style={{ color: hasRealtimePrice ? BLOOMBERG.CYAN : BLOOMBERG.WHITE }}>
                            LTP: {formatCurrency(currentPrice)}
                          </span>
                        </div>
                      </div>
                    );
                  })}
                </div>
              )}
            </div>

            <div style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '8px',
              padding: '20px'
            }}>
              <h3 style={{
                margin: '0 0 15px 0',
                fontSize: '14px',
                color: BLOOMBERG.ORANGE
              }}>
                PENDING ORDERS ({orders.length})
              </h3>
              {orders.length === 0 ? (
                <p style={{ color: BLOOMBERG.GRAY, fontSize: '12px' }}>No pending orders</p>
              ) : (
                <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                  {orders.map(order => (
                    <div key={order.id} style={{
                      padding: '8px 0',
                      borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                      fontSize: '12px'
                    }}>
                      <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '4px' }}>
                        <span style={{ fontWeight: 'bold' }}>{order.symbol}</span>
                        <span style={{ color: BLOOMBERG.CYAN }}>
                          {order.status}
                        </span>
                      </div>
                      <div style={{ color: BLOOMBERG.GRAY, fontSize: '11px' }}>
                        {order.side} {order.quantity} @ {formatCurrency(order.price || 0)} • {order.type}
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>
      )}
    </div>
    </>
  );
};

// Helper Components
const StatCard: React.FC<{
  label: string;
  value: string;
  icon: React.ReactNode;
  color: string;
}> = ({ label, value, icon, color }) => (
  <div style={{
    backgroundColor: BLOOMBERG.PANEL_BG,
    border: `1px solid ${BLOOMBERG.BORDER}`,
    borderRadius: '8px',
    padding: '15px',
    display: 'flex',
    justifyContent: 'space-between',
    alignItems: 'center'
  }}>
    <div>
      <div style={{ fontSize: '11px', color: BLOOMBERG.GRAY, marginBottom: '5px' }}>
        {label}
      </div>
      <div style={{ fontSize: '20px', fontWeight: 'bold', color }}>
        {value}
      </div>
    </div>
    <div style={{ color }}>
      {icon}
    </div>
  </div>
);

const MetricRow: React.FC<{ label: string; value: string }> = ({ label, value }) => (
  <div style={{
    display: 'flex',
    justifyContent: 'space-between',
    padding: '8px',
    backgroundColor: BLOOMBERG.DARK_BG,
    borderRadius: '4px'
  }}>
    <span style={{ fontSize: '12px', color: BLOOMBERG.GRAY }}>{label}</span>
    <span style={{ fontSize: '12px', fontWeight: 'bold', color: BLOOMBERG.WHITE, fontFamily: 'monospace' }}>
      {value}
    </span>
  </div>
);

export default PaperTradingPanel;
