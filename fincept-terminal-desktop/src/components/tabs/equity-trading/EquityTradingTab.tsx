/**
 * Equity Trading Tab - Bloomberg Terminal Style
 * Professional multi-broker trading interface for equities
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Activity, TrendingUp, TrendingDown, DollarSign, BarChart3,
  RefreshCw, Settings, Wifi, WifiOff, ChevronDown, ChevronUp,
  Minimize2, Maximize2, Clock, AlertCircle, Target, RotateCcw, X
} from 'lucide-react';
import { useBrokerState } from './hooks/useBrokerState';
import { useOrderExecution } from './hooks/useOrderExecution';
import { BrokerType } from './core/types';
import { registerAllAdapters } from './core/AdapterRegistry';
import BrokerConfigTab from './sub-tabs/BrokerConfigTab';
import ComparisonTab from './sub-tabs/ComparisonTab';
import BacktestingTab from './sub-tabs/BacktestingTab';
import PerformanceTab from './sub-tabs/PerformanceTab';
import MarketDataTab from './sub-tabs/MarketDataTab';
import Nifty100Tab from './sub-tabs/Nifty100Tab';
import LiveStreamingTab from './sub-tabs/LiveStreamingTab';
import TradingInterface from './ui/TradingInterface';
import OrdersPanel from './ui/OrdersPanel';
import PositionsPanel from './ui/PositionsPanel';
import HoldingsPanel from './ui/HoldingsPanel';
import MarketDepthChart from './ui/MarketDepthChart';
import TradingChart from './ui/TradingChart';
import PaperTradingPanel from './ui/PaperTradingPanel';
import { TabFooter } from '@/components/common/TabFooter';
import { integrationManager } from './integrations/IntegrationManager';

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
  INPUT_BG: '#0A0A0A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

type SubTab = 'trading' | 'config' | 'comparison' | 'backtesting' | 'performance' | 'market-data' | 'nifty100' | 'live-streaming';
type TradingMode = 'live' | 'paper';

// Helper function to safely get numeric value from stats
const getStatValue = (stats: any, key: string, defaultValue: number | undefined = 0): number => {
  const value = stats?.stats?.[key] ?? stats?.[key] ?? defaultValue;
  return typeof value === 'number' ? value : (defaultValue ?? 0);
};

const EquityTradingTab: React.FC = () => {
  const [activeSubTab, setActiveSubTab] = useState<SubTab>('trading');
  const [selectedBrokers, setSelectedBrokers] = useState<BrokerType[]>(['fyers', 'kite', 'alpaca']);
  const [tradingMode, setTradingMode] = useState<TradingMode>('paper');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const [showResetModal, setShowResetModal] = useState(false);
  const [resetAmount, setResetAmount] = useState('1000000');

  const brokerState = useBrokerState(selectedBrokers);
  const orderExecution = useOrderExecution();

  // Register adapters on component mount (runs once)
  useEffect(() => {
    try {
      registerAllAdapters();
    } catch (error) {
      console.error('[EquityTradingTab] Failed to register adapters:', error);
    }
  }, []);

  // Initialize integration manager (paper trading, etc.) and enable paper trading automatically
  useEffect(() => {
    const initializePaperTrading = async () => {
      try {
        await integrationManager.initialize();

        // Auto-enable paper trading on mount
        if (!integrationManager.isPaperTradingEnabled()) {
          await integrationManager.enablePaperTrading();
        }
      } catch (error) {
        console.error('[EquityTradingTab] ❌ Failed to initialize integration manager:', error);
      }
    };

    initializePaperTrading();
  }, []);

  // Handle paper trading account reset
  const handleResetPaperTrading = async () => {
    try {
      const amount = parseFloat(resetAmount);
      if (isNaN(amount) || amount <= 0) {
        alert('Please enter a valid amount');
        return;
      }

      await integrationManager.resetPaperAccount(amount);
      setShowResetModal(false);

      // Show success message
      alert(`Paper trading account reset successfully!\nNew balance: ₹${amount.toLocaleString('en-IN')}`);
    } catch (error: any) {
      console.error('[EquityTradingTab] ❌ Failed to reset paper trading:', error);
      alert(`Failed to reset account: ${error.message}`);
    }
  };

  // Update time
  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Initialize brokers and start auto-refresh
  useEffect(() => {
    brokerState.startAutoRefresh(5000);
    return () => brokerState.stopAutoRefresh();
  }, []);

  // Get authenticated brokers
  const activeBrokers = brokerState.getActiveBrokers();

  // Sub-tab configurations
  const subTabs = [
    { id: 'trading' as SubTab, label: 'TRADING', icon: Activity },
    { id: 'live-streaming' as SubTab, label: 'LIVE STREAM', icon: Wifi },
    { id: 'market-data' as SubTab, label: 'MARKET DATA', icon: TrendingUp },
    { id: 'nifty100' as SubTab, label: 'NIFTY 100', icon: BarChart3 },
    { id: 'config' as SubTab, label: 'BROKER CONFIG', icon: Settings },
    { id: 'comparison' as SubTab, label: 'COMPARISON', icon: BarChart3 },
    { id: 'backtesting' as SubTab, label: 'BACKTESTING', icon: Target },
    { id: 'performance' as SubTab, label: 'PERFORMANCE', icon: DollarSign }
  ];

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
      color: BLOOMBERG.WHITE,
      fontFamily: 'monospace'
    }}>
      {/* Bloomberg-Style Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 16px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        height: '48px'
      }}>
        {/* Left: Title */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <Activity size={20} color={BLOOMBERG.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${BLOOMBERG.ORANGE})` }} />
            <span style={{
              fontSize: '14px',
              fontWeight: 700,
              color: BLOOMBERG.ORANGE,
              letterSpacing: '1px'
            }}>
              EQUITY TRADING
            </span>
          </div>

          <div style={{ height: '20px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Sub-tab Navigation */}
          <div style={{ display: 'flex', gap: '4px' }}>
            {subTabs.map(tab => {
              const Icon = tab.icon;
              const isActive = activeSubTab === tab.id;
              return (
                <button
                  key={tab.id}
                  onClick={() => setActiveSubTab(tab.id)}
                  style={{
                    display: 'flex',
                    alignItems: 'center',
                    gap: '4px',
                    padding: '4px 12px',
                    fontSize: '10px',
                    fontWeight: 600,
                    letterSpacing: '0.5px',
                    color: isActive ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    backgroundColor: isActive ? BLOOMBERG.ORANGE : 'transparent',
                    border: `1px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    transition: 'all 0.15s ease',
                    outline: 'none'
                  }}
                  onMouseEnter={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                      e.currentTarget.style.color = BLOOMBERG.WHITE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.color = BLOOMBERG.GRAY;
                    }
                  }}
                >
                  <Icon size={12} />
                  {tab.label}
                </button>
              );
            })}
          </div>
        </div>

        {/* Right: Status & Time */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '11px' }}>
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
                transition: 'all 0.2s',
                outline: 'none'
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
                transition: 'all 0.2s',
                outline: 'none'
              }}
            >
              LIVE
            </button>
          </div>

          {/* WebSocket Status for Paper Trading */}
          {tradingMode === 'paper' && (
            <>
              <div style={{ height: '20px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />
              <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                <div style={{
                  width: '8px',
                  height: '8px',
                  borderRadius: '50%',
                  backgroundColor: wsConnected ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  animation: wsConnected ? 'pulse 2s infinite' : 'none',
                }} />
                <span style={{ color: wsConnected ? BLOOMBERG.GREEN : BLOOMBERG.GRAY, fontWeight: 600, fontSize: '9px' }}>
                  {wsConnected ? 'RT DATA' : 'OFFLINE'}
                </span>
              </div>
              <div style={{ height: '20px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />
              <button
                onClick={() => setShowResetModal(true)}
                style={{
                  padding: '4px 10px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.YELLOW}`,
                  color: BLOOMBERG.YELLOW,
                  cursor: 'pointer',
                  fontSize: '9px',
                  fontWeight: 700,
                  transition: 'all 0.2s',
                  outline: 'none',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  letterSpacing: '0.5px'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.YELLOW;
                  e.currentTarget.style.color = BLOOMBERG.DARK_BG;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG;
                  e.currentTarget.style.color = BLOOMBERG.YELLOW;
                }}
              >
                <RotateCcw size={12} />
                RESET
              </button>
            </>
          )}

          <div style={{ height: '20px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Broker Status Indicators */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
            {selectedBrokers.map(brokerId => {
              const status = brokerState.authStatus.get(brokerId);
              const isAuth = status?.authenticated || false;
              return (
                <div key={brokerId} style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                  {isAuth ? <Wifi size={14} color={BLOOMBERG.GREEN} /> : <WifiOff size={14} color={BLOOMBERG.RED} />}
                  <span style={{ color: isAuth ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontWeight: 600, textTransform: 'uppercase' }}>
                    {brokerId}
                  </span>
                </div>
              );
            })}
          </div>

          <div style={{ height: '20px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />

          {/* Clock */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <Clock size={14} color={BLOOMBERG.CYAN} />
            <span style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
              {currentTime.toLocaleTimeString('en-US', { hour12: false })}
            </span>
          </div>
        </div>
      </div>

      {/* Sub-tab Content */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {activeSubTab === 'trading' && (
          <TradingView
            brokerState={brokerState}
            orderExecution={orderExecution}
            activeBrokers={activeBrokers}
            tradingMode={tradingMode}
            isBottomPanelMinimized={isBottomPanelMinimized}
            setIsBottomPanelMinimized={setIsBottomPanelMinimized}
            wsConnected={wsConnected}
            setWsConnected={setWsConnected}
          />
        )}

        {activeSubTab === 'config' && (
          <BrokerConfigTab
            selectedBrokers={selectedBrokers}
            onBrokersChange={setSelectedBrokers}
            brokerState={brokerState}
          />
        )}

        {activeSubTab === 'comparison' && (
          <ComparisonTab activeBrokers={activeBrokers} />
        )}

        {activeSubTab === 'backtesting' && (
          <BacktestingTab activeBrokers={activeBrokers} />
        )}

        {activeSubTab === 'performance' && (
          <PerformanceTab
            activeBrokers={activeBrokers}
            orders={brokerState.getAllOrders()}
            positions={brokerState.getAllPositions()}
          />
        )}

        {activeSubTab === 'market-data' && (
          <MarketDataTab />
        )}

        {activeSubTab === 'nifty100' && (
          <Nifty100Tab />
        )}

        <div style={{ display: activeSubTab === 'live-streaming' ? 'block' : 'none', height: '100%' }}>
          <LiveStreamingTab />
        </div>
      </div>

      {/* Tab Footer */}
      <TabFooter
        tabName="EQUITY TRADING"
        backgroundColor={BLOOMBERG.HEADER_BG}
        borderColor={BLOOMBERG.ORANGE}
        leftInfo={[
          {
            label: 'Active Brokers',
            value: activeBrokers.length.toString(),
            color: BLOOMBERG.CYAN
          },
          {
            label: 'Total P&L',
            value: `₹${brokerState.getTotalPnL().toFixed(2)}`,
            color: brokerState.getTotalPnL() >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
          }
        ]}
        statusInfo={
          <span style={{ color: BLOOMBERG.GRAY }}>
            Last Updated: {currentTime.toLocaleTimeString('en-US', { hour12: false })}
          </span>
        }
      />

      {/* CSS Animation */}
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }
      `}</style>

      {/* Reset Confirmation Modal */}
      {showResetModal && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0,0,0,0.85)',
          display: 'flex',
          justifyContent: 'center',
          alignItems: 'center',
          zIndex: 10000,
          fontFamily: 'monospace'
        }}>
          <div style={{
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `3px solid ${BLOOMBERG.ORANGE}`,
            borderRadius: '8px',
            padding: '30px',
            maxWidth: '450px',
            width: '90%',
            boxShadow: `0 0 30px ${BLOOMBERG.ORANGE}50`
          }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '20px' }}>
              <h3 style={{ margin: 0, color: BLOOMBERG.ORANGE, fontSize: '16px', letterSpacing: '1px' }}>
                RESET PAPER TRADING ACCOUNT
              </h3>
              <button
                onClick={() => setShowResetModal(false)}
                style={{
                  background: 'none',
                  border: 'none',
                  color: BLOOMBERG.GRAY,
                  cursor: 'pointer',
                  padding: '4px'
                }}
              >
                <X size={20} />
              </button>
            </div>

            <div style={{
              backgroundColor: BLOOMBERG.DARK_BG,
              border: `1px solid ${BLOOMBERG.RED}`,
              padding: '12px',
              borderRadius: '4px',
              marginBottom: '20px',
              display: 'flex',
              alignItems: 'center',
              gap: '10px'
            }}>
              <AlertCircle size={20} color={BLOOMBERG.RED} />
              <div style={{ color: BLOOMBERG.GRAY, fontSize: '11px', lineHeight: '1.5' }}>
                This will <span style={{ color: BLOOMBERG.WHITE, fontWeight: 700 }}>close all positions</span>,{' '}
                <span style={{ color: BLOOMBERG.WHITE, fontWeight: 700 }}>cancel all orders</span>, and{' '}
                <span style={{ color: BLOOMBERG.WHITE, fontWeight: 700 }}>reset your balance</span>.
                <br />
                <span style={{ color: BLOOMBERG.RED, fontWeight: 700 }}>This action cannot be undone.</span>
              </div>
            </div>

            <div style={{ marginBottom: '25px' }}>
              <label style={{
                display: 'block',
                marginBottom: '10px',
                color: BLOOMBERG.CYAN,
                fontSize: '11px',
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                INITIAL BALANCE (₹)
              </label>
              <input
                type="number"
                value={resetAmount}
                onChange={(e) => setResetAmount(e.target.value)}
                placeholder="1000000"
                style={{
                  width: '100%',
                  padding: '12px',
                  backgroundColor: BLOOMBERG.DARK_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  borderRadius: '4px',
                  fontFamily: 'monospace',
                  fontSize: '16px',
                  fontWeight: 600,
                  outline: 'none',
                  transition: 'border-color 0.2s'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = BLOOMBERG.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = BLOOMBERG.BORDER}
              />
              <div style={{ marginTop: '6px', fontSize: '9px', color: BLOOMBERG.GRAY }}>
                Default: ₹10,00,000 (10 Lakhs)
              </div>
            </div>

            <div style={{ display: 'flex', gap: '12px' }}>
              <button
                onClick={() => setShowResetModal(false)}
                style={{
                  flex: 1,
                  padding: '12px',
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  color: BLOOMBERG.WHITE,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontFamily: 'monospace',
                  fontSize: '12px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s',
                  outline: 'none'
                }}
                onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
                onMouseLeave={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG}
              >
                CANCEL
              </button>
              <button
                onClick={handleResetPaperTrading}
                style={{
                  flex: 1,
                  padding: '12px',
                  backgroundColor: BLOOMBERG.ORANGE,
                  border: 'none',
                  color: BLOOMBERG.DARK_BG,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontFamily: 'monospace',
                  fontSize: '12px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s',
                  outline: 'none'
                }}
                onMouseEnter={(e) => e.currentTarget.style.transform = 'scale(1.02)'}
                onMouseLeave={(e) => e.currentTarget.style.transform = 'scale(1)'}
              >
                RESET ACCOUNT
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

// Quote Data Panel Component
const QuoteDataPanel: React.FC<{ symbol: string; exchange: string }> = ({ symbol, exchange }) => {
  const [quote, setQuote] = useState<any>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [searchedSymbol, setSearchedSymbol] = useState('');
  const [autoRefresh, setAutoRefresh] = useState(false);
  const [wsConnected, setWsConnected] = useState(false);
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const subscriptionIdRef = useRef<string | null>(null);
  const [webSocketManager, setWebSocketManager] = useState<any>(null);

  // Import WebSocket manager
  useEffect(() => {
    import('./services/WebSocketManager').then(module => {
      setWebSocketManager(module.webSocketManager);
    });
  }, []);

  const fetchQuote = async (symbolToFetch: string) => {
    if (!symbolToFetch || symbolToFetch.length < 2) {
      setError('Please enter a valid symbol');
      return;
    }

    setLoading(true);
    setError(null);
    try {
      const { authManager } = await import('./services/AuthManager');
      const adapter = authManager.getAdapter('fyers');

      if (!adapter) {
        throw new Error('Fyers adapter not available');
      }

      const fyersSymbol = `${symbolToFetch.toUpperCase()}-EQ`;
      console.log('[QuoteDataPanel] Fetching quote for:', fyersSymbol);

      const quoteData = await adapter.getQuote(fyersSymbol, exchange);
      setQuote(quoteData);
      setSearchedSymbol(symbolToFetch.toUpperCase());
      console.log('[QuoteDataPanel] Quote data:', quoteData);
    } catch (err: any) {
      console.error('[QuoteDataPanel] Error fetching quote:', err);
      setError(err.message || 'Failed to fetch quote');
      setQuote(null);
    } finally {
      setLoading(false);
    }
  };

  const handleSearch = () => {
    fetchQuote(symbol);
  };

  const toggleAutoRefresh = () => {
    setAutoRefresh(!autoRefresh);
  };

  // WebSocket subscription for real-time updates - DISABLED (use polling instead)
  useEffect(() => {
    if (!webSocketManager || !searchedSymbol) return;

    const connectAndSubscribe = async () => {
      try {
        const { authManager } = await import('./services/AuthManager');

        // Skip WebSocket if not authenticated
        if (!authManager.isAuthenticated('fyers')) {
          setWsConnected(false);
          return;
        }

        const isConnected = webSocketManager.isConnectedTo('fyers');
        if (!isConnected) {
          await webSocketManager.connect('fyers');
          setWsConnected(true);
        } else {
          setWsConnected(true);
        }

        if (subscriptionIdRef.current) {
          webSocketManager.unsubscribeQuotes(subscriptionIdRef.current);
          subscriptionIdRef.current = null;
        }

        const symbols = [{
          symbol: `${searchedSymbol}-EQ`,
          exchange: exchange
        }];

        const subId = webSocketManager.subscribeQuotes(
          'fyers',
          symbols,
          (wsQuote: any) => {
            setQuote((prevQuote: any) => ({
              ...prevQuote,
              lastPrice: wsQuote.lastPrice,
              change: wsQuote.change,
              changePercent: wsQuote.changePercent,
              high: wsQuote.high,
              low: wsQuote.low,
              volume: wsQuote.volume,
              bid: wsQuote.bid,
              ask: wsQuote.ask,
              timestamp: new Date(),
            }));
          }
        );

        subscriptionIdRef.current = subId;

      } catch (error) {
        setWsConnected(false);
      }
    };

    connectAndSubscribe();

    return () => {
      if (subscriptionIdRef.current && webSocketManager) {
        webSocketManager.unsubscribeQuotes(subscriptionIdRef.current);
        subscriptionIdRef.current = null;
      }
    };
  }, [webSocketManager, searchedSymbol, exchange]);

  // Polling fallback (only when WebSocket not active)
  useEffect(() => {
    if (autoRefresh && searchedSymbol && !wsConnected) {
      intervalRef.current = setInterval(() => {
        fetchQuote(searchedSymbol);
      }, 5000);
    } else {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
        intervalRef.current = null;
      }
    }

    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, [autoRefresh, searchedSymbol, exchange, wsConnected]);

  return (
    <div style={{
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderRadius: '4px',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column'
    }}>
      <div style={{
        padding: '8px 12px',
        borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
        backgroundColor: BLOOMBERG.HEADER_BG
      }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <TrendingUp size={14} color={BLOOMBERG.CYAN} />
            <span style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>
              QUOTE DATA
            </span>
            {searchedSymbol && (
              <span style={{ fontSize: '9px', color: BLOOMBERG.CYAN, fontWeight: 600 }}>
                ({searchedSymbol})
              </span>
            )}
            {wsConnected && (
              <div style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                backgroundColor: BLOOMBERG.GREEN,
                animation: 'pulse 2s infinite',
              }} />
            )}
          </div>
          <div style={{ display: 'flex', gap: '6px' }}>
            <button
              onClick={handleSearch}
              disabled={loading || !symbol}
              style={{
                padding: '4px 10px',
                backgroundColor: BLOOMBERG.CYAN,
                border: 'none',
                color: BLOOMBERG.DARK_BG,
                fontSize: '9px',
                fontWeight: 700,
                cursor: loading || !symbol ? 'not-allowed' : 'pointer',
                borderRadius: '3px',
                letterSpacing: '0.5px',
                opacity: loading || !symbol ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              {loading ? <RefreshCw size={10} className="spin" /> : <TrendingUp size={10} />}
              SEARCH
            </button>
            <button
              onClick={toggleAutoRefresh}
              disabled={!searchedSymbol || wsConnected}
              style={{
                padding: '4px 10px',
                backgroundColor: autoRefresh ? BLOOMBERG.GREEN : BLOOMBERG.PANEL_BG,
                border: `1px solid ${autoRefresh ? BLOOMBERG.GREEN : BLOOMBERG.BORDER}`,
                color: autoRefresh ? BLOOMBERG.DARK_BG : BLOOMBERG.WHITE,
                fontSize: '9px',
                fontWeight: 700,
                cursor: (!searchedSymbol || wsConnected) ? 'not-allowed' : 'pointer',
                borderRadius: '3px',
                letterSpacing: '0.5px',
                opacity: (!searchedSymbol || wsConnected) ? 0.5 : 1,
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
              title={wsConnected ? 'WebSocket active - real-time updates enabled' : 'Enable polling fallback'}
            >
              <RefreshCw size={10} />
              {wsConnected ? 'LIVE' : 'AUTO'}
            </button>
          </div>
        </div>
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {!symbol || symbol.length < 2 ? (
          <div style={{
            height: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: BLOOMBERG.GRAY
          }}>
            <div style={{ textAlign: 'center' }}>
              <TrendingUp size={48} color={BLOOMBERG.MUTED} style={{ margin: '0 auto 12px' }} />
              <div style={{ fontSize: '12px', fontWeight: 600 }}>ENTER A SYMBOL</div>
              <div style={{ fontSize: '10px', marginTop: '4px' }}>Type a symbol to view quote data</div>
            </div>
          </div>
        ) : error ? (
          <div style={{
            padding: '12px',
            backgroundColor: BLOOMBERG.INPUT_BG,
            border: `1px solid ${BLOOMBERG.RED}`,
            borderRadius: '4px',
            color: BLOOMBERG.RED,
            fontSize: '11px'
          }}>
            <AlertCircle size={16} style={{ marginBottom: '6px' }} />
            <div>{error}</div>
          </div>
        ) : !quote ? (
          <div style={{ textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
            Loading...
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            {/* Symbol Header */}
            <div style={{
              padding: '12px',
              backgroundColor: BLOOMBERG.INPUT_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px'
            }}>
              <div style={{ fontSize: '18px', fontWeight: 700, color: BLOOMBERG.WHITE, marginBottom: '4px' }}>
                {symbol}
              </div>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>
                {exchange} • EQUITY
              </div>
            </div>

            {/* Last Price */}
            <div style={{
              padding: '12px',
              backgroundColor: BLOOMBERG.INPUT_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px'
            }}>
              <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>LAST TRADED PRICE</div>
              <div style={{ fontSize: '24px', fontWeight: 700, color: BLOOMBERG.GREEN }}>
                ₹{quote.lastPrice?.toFixed(2) || 'N/A'}
              </div>
              {quote.change !== undefined && (
                <div style={{
                  fontSize: '12px',
                  fontWeight: 600,
                  color: quote.change >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  marginTop: '4px'
                }}>
                  {quote.change >= 0 ? '+' : ''}{quote.change.toFixed(2)} ({quote.changePercent >= 0 ? '+' : ''}{quote.changePercent?.toFixed(2)}%)
                </div>
              )}
            </div>

            {/* OHLC */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1fr',
              gap: '8px'
            }}>
              <div style={{
                padding: '10px',
                backgroundColor: BLOOMBERG.INPUT_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>OPEN</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: BLOOMBERG.WHITE }}>
                  ₹{quote.open?.toFixed(2) || 'N/A'}
                </div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: BLOOMBERG.INPUT_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>HIGH</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: BLOOMBERG.GREEN }}>
                  ₹{quote.high?.toFixed(2) || 'N/A'}
                </div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: BLOOMBERG.INPUT_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>LOW</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: BLOOMBERG.RED }}>
                  ₹{quote.low?.toFixed(2) || 'N/A'}
                </div>
              </div>
              <div style={{
                padding: '10px',
                backgroundColor: BLOOMBERG.INPUT_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '4px'
              }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>PREV CLOSE</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: BLOOMBERG.WHITE }}>
                  ₹{quote.close?.toFixed(2) || 'N/A'}
                </div>
              </div>
            </div>

            {/* Volume & Bid/Ask */}
            <div style={{
              padding: '12px',
              backgroundColor: BLOOMBERG.INPUT_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              borderRadius: '4px'
            }}>
              <div style={{ marginBottom: '8px' }}>
                <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>VOLUME</div>
                <div style={{ fontSize: '14px', fontWeight: 600, color: BLOOMBERG.CYAN }}>
                  {quote.volume ? quote.volume.toLocaleString('en-IN') : 'N/A'}
                </div>
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginTop: '12px' }}>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>BID</div>
                  <div style={{ fontSize: '12px', fontWeight: 600, color: BLOOMBERG.GREEN }}>
                    ₹{quote.bid?.toFixed(2) || 'N/A'}
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY, marginBottom: '4px' }}>ASK</div>
                  <div style={{ fontSize: '12px', fontWeight: 600, color: BLOOMBERG.RED }}>
                    ₹{quote.ask?.toFixed(2) || 'N/A'}
                  </div>
                </div>
              </div>
            </div>

            {/* Timestamp */}
            <div style={{
              fontSize: '9px',
              color: BLOOMBERG.GRAY,
              textAlign: 'center',
              paddingTop: '8px',
              borderTop: `1px solid ${BLOOMBERG.BORDER}`
            }}>
              Last updated: {quote.timestamp ? new Date(quote.timestamp).toLocaleTimeString() : 'N/A'}
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

// Main Trading View - Bloomberg Style
const TradingView: React.FC<{
  brokerState: ReturnType<typeof useBrokerState>;
  orderExecution: ReturnType<typeof useOrderExecution>;
  activeBrokers: BrokerType[];
  tradingMode: TradingMode;
  isBottomPanelMinimized: boolean;
  setIsBottomPanelMinimized: (value: boolean) => void;
  wsConnected: boolean;
  setWsConnected: (value: boolean) => void;
}> = ({ brokerState, orderExecution, activeBrokers, tradingMode, isBottomPanelMinimized, setIsBottomPanelMinimized, wsConnected, setWsConnected }) => {
  const [selectedPanel, setSelectedPanel] = useState<'orders' | 'positions' | 'holdings' | 'stats'>('orders');
  const [selectedSymbol, setSelectedSymbol] = useState('');
  const [selectedExchange, setSelectedExchange] = useState('NSE');
  const [depthBroker, setDepthBroker] = useState<BrokerType>(activeBrokers[0] || 'fyers');
  const [paperPositions, setPaperPositions] = useState<any[]>([]);
  const [paperOrders, setPaperOrders] = useState<any[]>([]);
  const [paperBalance, setPaperBalance] = useState(0);
  const [paperEquity, setPaperEquity] = useState(0);
  const [paperStats, setPaperStats] = useState<any>(null);
  const [positionPrices, setPositionPrices] = useState<Map<string, number>>(new Map());
  const subscriptionIdRef = useRef<string | null>(null);

  // Import WebSocket manager at the top level
  const [webSocketManager, setWebSocketManager] = useState<any>(null);

  useEffect(() => {
    import('./services/WebSocketManager').then(module => {
      setWebSocketManager(module.webSocketManager);
    });
  }, []);

  // Auto-connect to WebSocket and subscribe to position symbols for real-time price updates
  useEffect(() => {
    if (!webSocketManager || paperPositions.length === 0) {
      return;
    }

    const connectAndSubscribe = async () => {
      try {
        const { authManager } = await import('./services/AuthManager');

        // Check Fyers authentication - skip WebSocket if not authenticated
        if (!authManager.isAuthenticated('fyers')) {
          setWsConnected(false);
          return;
        }

        const isConnected = webSocketManager.isConnectedTo('fyers');
        if (!isConnected) {
          await webSocketManager.connect('fyers');
          setWsConnected(true);
        } else {
          setWsConnected(true);
        }

        if (subscriptionIdRef.current) {
          webSocketManager.unsubscribeQuotes(subscriptionIdRef.current);
          subscriptionIdRef.current = null;
        }

        const symbols = Array.from(
          new Set(
            paperPositions.map(pos => ({
              symbol: `${pos.symbol}-EQ`,
              exchange: pos.exchange || 'NSE'
            }))
          )
        );

        if (symbols.length === 0) return;

        const subId = webSocketManager.subscribeQuotes(
          'fyers',
          symbols,
          (quote: any) => {
            const cleanSymbol = quote.symbol.replace('-EQ', '');
            setPositionPrices(prev => {
              const updated = new Map(prev);
              updated.set(cleanSymbol, quote.lastPrice);
              return updated;
            });

            import('./integrations/PaperTradingIntegration').then(({ paperTradingIntegration }) => {
              if (paperTradingIntegration && paperTradingIntegration.isInitialized()) {
                const bridge = (paperTradingIntegration as any).bridgeAdapter;
                if (bridge) {
                  bridge.updatePrice(cleanSymbol, quote.lastPrice, quote.exchange);
                }
              }
            });
          }
        );

        subscriptionIdRef.current = subId;

      } catch (error) {
        setWsConnected(false);
      }
    };

    connectAndSubscribe();

    return () => {
      if (subscriptionIdRef.current && webSocketManager) {
        webSocketManager.unsubscribeQuotes(subscriptionIdRef.current);
        subscriptionIdRef.current = null;
      }
    };
  }, [webSocketManager, tradingMode, paperPositions]);

  // Polling fallback for paper trading when WebSocket is not available
  useEffect(() => {
    if (tradingMode !== 'paper' || paperPositions.length === 0 || wsConnected) {
      return;
    }

    const pollInterval = setInterval(async () => {
      try {
        const { paperTradingIntegration } = await import('./integrations/PaperTradingIntegration');

        if (!paperTradingIntegration || !paperTradingIntegration.isInitialized()) {
          return;
        }

        const updatedPositions = await paperTradingIntegration.getPositions();

        updatedPositions.forEach((pos: any) => {
          setPositionPrices(prev => {
            const updated = new Map(prev);
            updated.set(pos.symbol, pos.lastPrice);
            return updated;
          });
        });

      } catch (error) {
        // Silent error handling
      }
    }, 3000);

    return () => clearInterval(pollInterval);
  }, [tradingMode, paperPositions, wsConnected]);

  // Handle closing a position by placing a reverse order
  const handleClosePosition = async (brokerId: string, symbol: string, quantity: number) => {
    try {

      // Find the position to determine the side
      const position = paperPositions.find(p => p.symbol === symbol);
      if (!position) {
        return;
      }

      // Determine opposite side (close BUY with SELL, close SELL with BUY)
      const oppositeSide = position.side === 'BUY' ? 'SELL' : 'BUY';


      // Place a market order to close the position
      const orderRequest = {
        symbol: symbol,
        exchange: position.exchange || 'NSE',
        side: oppositeSide as any, // Will be converted to OrderSide in placeOrder
        type: 'MARKET' as any,
        quantity: Math.abs(quantity),
        product: position.product || 'CNC' as any,
        validity: 'DAY' as any
      };


      const result = await orderExecution.placeOrder(orderRequest);

      if (result.success) {
        // Refresh positions after a short delay
        setTimeout(() => {
          brokerState.fetchPositions();
        }, 1000);
      } else {
      }
    } catch (error) {
    }
  };

  // Fetch paper trading data when paper trading is enabled
  useEffect(() => {
    const fetchPaperData = async () => {
      if (tradingMode === 'paper') {
        // Check if paper trading is enabled
        const isPaperEnabled = integrationManager.isPaperTradingEnabled();

        if (!isPaperEnabled) {
          return;
        }

        // Check if paper trading is initialized
        const isInitialized = integrationManager.isPaperTradingInitialized();

        if (!isInitialized) {
          return;
        }

        // Small delay to ensure initialization is complete (handles React StrictMode double-render)
        await new Promise(resolve => setTimeout(resolve, 100));

        try {

          // Fetch data independently to avoid one failure affecting others
          let positions: any[] = [];
          let orders: any[] = [];
          let balanceValue: number = 0;
          let stats: any = null;

          try {
            positions = await integrationManager.getPaperPositions() || [];
          } catch (err) {
          }

          try {
            orders = await integrationManager.getPaperOrders() || [];
          } catch (err) {
          }

          try {
            balanceValue = await integrationManager.getPaperBalance() || 0;
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch balance:', err);
          }

          try {
            stats = await integrationManager.getPaperStatistics();
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch stats:', err);
            // Stats failure is non-critical
          }

          setPaperPositions(positions || []);
          setPaperOrders(orders || []);

          // Ensure balance is a number, fallback to initial balance if invalid
          let numericBalance = typeof balanceValue === 'number' && balanceValue > 0 ? balanceValue : 0;

          // If balance is still 0, try to get initial balance from stats or use default
          if (numericBalance === 0) {
            numericBalance = stats?.initialBalance || 1000000; // 10 lakhs default
          }

          setPaperBalance(numericBalance);

          // Calculate equity: balance + unrealized P&L
          const unrealizedPnL = (positions || []).reduce((sum, p) => {
            // Check both unrealizedPnl and unrealizedPnL (capital L)
            const pnl = p.unrealizedPnl || p.unrealizedPnL || 0;
            return sum + pnl;
          }, 0);

          const totalEquity = numericBalance + unrealizedPnL;
          setPaperEquity(totalEquity);

          setPaperStats(stats || null);
        } catch (error) {
          console.error('[EquityTradingTab] Failed to fetch paper trading data:', error);
          setPaperPositions([]);
          setPaperOrders([]);
          setPaperBalance(0);
          setPaperEquity(0);
          setPaperStats(null);
        }
      } else {
        setPaperPositions([]);
        setPaperOrders([]);
        setPaperBalance(0);
        setPaperEquity(0);
        setPaperStats(null);
      }
    };

    // Initial fetch
    fetchPaperData();

    // Poll every 2 seconds for real-time updates
    const interval = setInterval(fetchPaperData, 2000);

    return () => clearInterval(interval);
  }, [tradingMode]);

  const livePnL = brokerState.getTotalPnL();
  const liveOrders = brokerState.getAllOrders();
  const livePositions = brokerState.getAllPositions();
  const allHoldings = brokerState.getAllHoldings();

  // Calculate P&L based on trading mode
  const paperPnL = paperStats ? getStatValue(paperStats, 'totalPnL', 0) : 0;
  const totalPnL = tradingMode === 'paper' ? paperPnL : livePnL;

  // Use appropriate data based on trading mode
  const allOrders = tradingMode === 'paper' ? paperOrders : liveOrders;
  const allPositions = tradingMode === 'paper' ? paperPositions : livePositions;

  // Calculate return percentage for paper trading
  const initialBalance = paperStats?.initialBalance || 1000000; // Default 10 lakhs
  const paperReturn = initialBalance > 0
    ? ((paperEquity - initialBalance) / initialBalance) * 100
    : 0;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Paper Trading Stats Bar */}
      {tradingMode === 'paper' && (
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
          padding: '10px 16px',
          display: 'flex',
          alignItems: 'center',
          gap: '24px',
          fontSize: '11px',
          flexShrink: 0
        }}>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>BALANCE</div>
            <div style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
              ₹{(paperBalance || 0).toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>TOTAL EQUITY</div>
            <div style={{ color: BLOOMBERG.YELLOW, fontWeight: 600 }}>
              ₹{(paperEquity || 0).toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>TOTAL P&L</div>
            <div style={{ color: (totalPnL || 0) >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontWeight: 600 }}>
              {(totalPnL || 0) >= 0 ? '+' : ''}₹{(totalPnL || 0).toFixed(2)}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>RETURN</div>
            <div style={{ color: (paperReturn || 0) >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED, fontWeight: 600 }}>
              {(paperReturn || 0) >= 0 ? '+' : ''}{(paperReturn || 0).toFixed(2)}%
            </div>
          </div>
          <div style={{ height: '24px', width: '1px', backgroundColor: BLOOMBERG.BORDER }} />
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>WIN RATE</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
              {getStatValue(paperStats, 'winRate', 0).toFixed(2)}%
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>TOTAL TRADES</div>
            <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
              {getStatValue(paperStats, 'totalTrades', 0).toFixed(0)}
            </div>
          </div>
          <div>
            <div style={{ color: BLOOMBERG.GRAY, fontSize: '9px', marginBottom: '2px' }}>SHARPE RATIO</div>
            <div style={{ color: BLOOMBERG.PURPLE, fontWeight: 600 }}>
              {(() => {
                const sharpe = paperStats?.sharpeRatio ?? paperStats?.stats?.sharpeRatio;
                return (sharpe !== null && sharpe !== undefined && typeof sharpe === 'number') ? sharpe.toFixed(2) : 'N/A';
              })()}
            </div>
          </div>
        </div>
      )}

      {/* Main Trading Interface - Bloomberg 3-Column Layout */}
      <div style={{
        flex: 1,
        display: 'grid',
        gridTemplateColumns: '1fr 1.5fr 1fr',
        gap: '8px',
        padding: '8px',
        overflow: 'hidden',
        minHeight: 0
      }}>
        {/* Left Column: Order Entry */}
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '4px',
          overflow: 'auto'
        }}>
          <div style={{
            padding: '8px 12px',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            backgroundColor: BLOOMBERG.HEADER_BG
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <DollarSign size={14} color={BLOOMBERG.ORANGE} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>
                ORDER ENTRY
              </span>
            </div>
          </div>
          <div style={{ padding: '12px' }}>
            <TradingInterface
              orderExecution={orderExecution}
              activeBrokers={activeBrokers}
              onSymbolChange={(symbol, exchange) => {
                setSelectedSymbol(symbol);
                setSelectedExchange(exchange);
              }}
            />
          </div>
        </div>

        {/* Center Column: Trading Chart */}
        <div style={{
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderRadius: '4px',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column'
        }}>
          <div style={{
            padding: '8px 12px',
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            backgroundColor: BLOOMBERG.HEADER_BG,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <BarChart3 size={14} color={BLOOMBERG.CYAN} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>
                CHART
              </span>
              {selectedSymbol && (
                <>
                  <span style={{ color: BLOOMBERG.GRAY }}>•</span>
                  <span style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.YELLOW }}>
                    {selectedSymbol}
                  </span>
                  <span style={{ fontSize: '9px', color: BLOOMBERG.GRAY }}>
                    {selectedExchange}
                  </span>
                </>
              )}
            </div>
          </div>
          <div style={{ flex: 1, overflow: 'hidden' }}>
            {selectedSymbol ? (
              <TradingChart
                brokerId={depthBroker}
                symbol={selectedSymbol}
                exchange={selectedExchange}
              />
            ) : (
              <div style={{
                height: '100%',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                color: BLOOMBERG.GRAY
              }}>
                <div style={{ textAlign: 'center' }}>
                  <BarChart3 size={48} color={BLOOMBERG.MUTED} style={{ margin: '0 auto 12px' }} />
                  <div style={{ fontSize: '12px', fontWeight: 600 }}>SELECT A SYMBOL</div>
                  <div style={{ fontSize: '10px', marginTop: '4px' }}>Enter a symbol to view chart data</div>
                </div>
              </div>
            )}
          </div>
        </div>

        {/* Right Column: Quote Data */}
        <QuoteDataPanel
          symbol={selectedSymbol}
          exchange={selectedExchange}
        />
      </div>

      {/* Bottom Panel: Orders/Positions/Holdings - Bloomberg Style */}
      <div style={{
        height: isBottomPanelMinimized ? '40px' : '280px',
        borderTop: `2px solid ${BLOOMBERG.ORANGE}`,
        backgroundColor: BLOOMBERG.PANEL_BG,
        transition: 'height 0.2s ease'
      }}>
        {/* Panel Header */}
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          padding: '8px 12px',
          backgroundColor: BLOOMBERG.HEADER_BG,
          borderBottom: `1px solid ${BLOOMBERG.BORDER}`
        }}>
          {/* Panel Tabs */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
            {[
              { id: 'orders' as const, label: 'ORDERS', count: allOrders.length },
              { id: 'positions' as const, label: 'POSITIONS', count: allPositions.length },
              { id: 'holdings' as const, label: 'HOLDINGS', count: allHoldings.length },
              { id: 'stats' as const, label: 'STATS', count: tradingMode === 'paper' ? (paperStats?.totalTrades || 0) : 0 }
            ].map(tab => {
              const isActive = selectedPanel === tab.id;
              return (
                <button
                  key={tab.id}
                  onClick={() => setSelectedPanel(tab.id)}
                  style={{
                    padding: '4px 12px',
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    color: isActive ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                    backgroundColor: isActive ? BLOOMBERG.ORANGE : 'transparent',
                    border: `1px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                    borderRadius: '2px',
                    cursor: 'pointer',
                    transition: 'all 0.15s ease',
                    outline: 'none'
                  }}
                  onMouseEnter={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                      e.currentTarget.style.color = BLOOMBERG.WHITE;
                    }
                  }}
                  onMouseLeave={(e) => {
                    if (!isActive) {
                      e.currentTarget.style.backgroundColor = 'transparent';
                      e.currentTarget.style.color = BLOOMBERG.GRAY;
                    }
                  }}
                >
                  {tab.label} ({tab.count})
                </button>
              );
            })}
          </div>

          {/* P&L and Controls */}
          <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
            {/* Total P&L */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
              <span style={{ color: BLOOMBERG.GRAY, fontWeight: 600 }}>TOTAL P&L:</span>
              <span style={{
                color: totalPnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                fontWeight: 700,
                fontSize: '13px'
              }}>
                {totalPnL >= 0 ? '+' : ''}₹{totalPnL.toFixed(2)}
              </span>
              {totalPnL >= 0 ? (
                <TrendingUp size={14} color={BLOOMBERG.GREEN} />
              ) : (
                <TrendingDown size={14} color={BLOOMBERG.RED} />
              )}
            </div>

            {/* Refresh Button */}
            <button
              onClick={() => {
                brokerState.fetchOrders();
                brokerState.fetchPositions();
                brokerState.fetchHoldings();
              }}
              style={{
                padding: '4px 8px',
                backgroundColor: 'transparent',
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                color: BLOOMBERG.CYAN,
                fontSize: '10px',
                outline: 'none'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
              }}
            >
              <RefreshCw size={12} />
              REFRESH
            </button>

            {/* Minimize/Maximize Button */}
            <button
              onClick={() => setIsBottomPanelMinimized(!isBottomPanelMinimized)}
              style={{
                padding: '4px 8px',
                backgroundColor: 'transparent',
                border: `1px solid ${BLOOMBERG.BORDER}`,
                borderRadius: '2px',
                cursor: 'pointer',
                color: BLOOMBERG.GRAY,
                outline: 'none'
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                e.currentTarget.style.color = BLOOMBERG.WHITE;
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.backgroundColor = 'transparent';
                e.currentTarget.style.color = BLOOMBERG.GRAY;
              }}
            >
              {isBottomPanelMinimized ? <Maximize2 size={14} /> : <Minimize2 size={14} />}
            </button>
          </div>
        </div>

        {/* Panel Content */}
        {!isBottomPanelMinimized && (
          <div style={{ height: 'calc(100% - 40px)', overflow: 'auto' }}>
            {selectedPanel === 'orders' && (
              <OrdersPanel
                orders={allOrders}
                loading={brokerState.loading.orders}
                onRefresh={brokerState.fetchOrders}
                onCancel={(brokerId, orderId) => orderExecution.cancelOrder(brokerId as BrokerType, orderId)}
              />
            )}

            {selectedPanel === 'positions' && (
              <PositionsPanel
                positions={allPositions}
                loading={brokerState.loading.positions}
                onRefresh={brokerState.fetchPositions}
                onClose={tradingMode === 'paper' ? handleClosePosition : undefined}
                realtimePrices={tradingMode === 'paper' ? positionPrices : undefined}
              />
            )}

            {selectedPanel === 'holdings' && (
              <HoldingsPanel
                holdings={allHoldings}
                loading={brokerState.loading.holdings}
                onRefresh={brokerState.fetchHoldings}
              />
            )}

            {selectedPanel === 'stats' && (
              <div style={{
                padding: '16px',
                display: 'grid',
                gridTemplateColumns: 'repeat(auto-fit, minmax(250px, 1fr))',
                gap: '16px'
              }}>
                {/* Performance Metrics */}
                <div style={{
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderRadius: '4px',
                  padding: '12px'
                }}>
                  <div style={{
                    fontSize: '10px',
                    color: BLOOMBERG.ORANGE,
                    fontWeight: 700,
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    PERFORMANCE METRICS
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '11px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Win Rate</span>
                      <span style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
                        {getStatValue(paperStats, 'winRate', 0).toFixed(2)}%
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Profit Factor</span>
                      <span style={{ color: BLOOMBERG.CYAN, fontWeight: 600 }}>
                        {getStatValue(paperStats, 'profitFactor', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Sharpe Ratio</span>
                      <span style={{ color: BLOOMBERG.PURPLE, fontWeight: 600 }}>
                        {(() => {
                          const sharpe = paperStats?.sharpeRatio ?? paperStats?.stats?.sharpeRatio;
                          return (sharpe !== null && sharpe !== undefined && typeof sharpe === 'number') ? sharpe.toFixed(2) : 'N/A';
                        })()}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Max Drawdown</span>
                      <span style={{ color: BLOOMBERG.RED, fontWeight: 600 }}>
                        {(() => {
                          const dd = paperStats?.maxDrawdown ?? paperStats?.stats?.maxDrawdown;
                          return (dd !== null && dd !== undefined && typeof dd === 'number') ? `${dd.toFixed(2)}%` : 'N/A';
                        })()}
                      </span>
                    </div>
                  </div>
                </div>

                {/* Trade Statistics */}
                <div style={{
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderRadius: '4px',
                  padding: '12px'
                }}>
                  <div style={{
                    fontSize: '10px',
                    color: BLOOMBERG.ORANGE,
                    fontWeight: 700,
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    TRADE STATISTICS
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '11px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Avg Win</span>
                      <span style={{ color: BLOOMBERG.GREEN, fontWeight: 600 }}>
                        ₹{getStatValue(paperStats, 'averageWin', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Avg Loss</span>
                      <span style={{ color: BLOOMBERG.RED, fontWeight: 600 }}>
                        ₹{getStatValue(paperStats, 'averageLoss', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Largest Win</span>
                      <span style={{ color: BLOOMBERG.GREEN, fontWeight: 600 }}>
                        ₹{getStatValue(paperStats, 'largestWin', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Largest Loss</span>
                      <span style={{ color: BLOOMBERG.RED, fontWeight: 600 }}>
                        ₹{getStatValue(paperStats, 'largestLoss', 0).toFixed(2)}
                      </span>
                    </div>
                  </div>
                </div>

                {/* P&L Summary */}
                <div style={{
                  backgroundColor: BLOOMBERG.HEADER_BG,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderRadius: '4px',
                  padding: '12px'
                }}>
                  <div style={{
                    fontSize: '10px',
                    color: BLOOMBERG.ORANGE,
                    fontWeight: 700,
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    P&L SUMMARY
                  </div>
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px', fontSize: '11px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Realized P&L</span>
                      <span style={{
                        color: getStatValue(paperStats, 'realizedPnL', 0) >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                        fontWeight: 600
                      }}>
                        {getStatValue(paperStats, 'realizedPnL', 0) >= 0 ? '+' : ''}₹{getStatValue(paperStats, 'realizedPnL', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Unrealized P&L</span>
                      <span style={{
                        color: getStatValue(paperStats, 'unrealizedPnL', 0) >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                        fontWeight: 600
                      }}>
                        {getStatValue(paperStats, 'unrealizedPnL', 0) >= 0 ? '+' : ''}₹{getStatValue(paperStats, 'unrealizedPnL', 0).toFixed(2)}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Total Fees</span>
                      <span style={{ color: BLOOMBERG.YELLOW, fontWeight: 600 }}>
                        ₹{getStatValue(paperStats, 'totalFees', 0).toFixed(2)}
                      </span>
                    </div>
                  </div>
                </div>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default EquityTradingTab;
