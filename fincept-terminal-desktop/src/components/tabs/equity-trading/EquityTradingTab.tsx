/**
 * Equity Trading Tab - Bloomberg Terminal Style
 * Professional multi-broker trading interface for equities
 */

import React, { useState, useEffect } from 'react';
import {
  Activity, TrendingUp, TrendingDown, DollarSign, BarChart3,
  RefreshCw, Settings, Wifi, WifiOff, ChevronDown, ChevronUp,
  Minimize2, Maximize2, Clock, AlertCircle, Target
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
const getStatValue = (stats: any, key: string, defaultValue: number = 0): number => {
  const value = stats?.stats?.[key] ?? stats?.[key] ?? defaultValue;
  return typeof value === 'number' ? value : defaultValue;
};

const EquityTradingTab: React.FC = () => {
  const [activeSubTab, setActiveSubTab] = useState<SubTab>('trading');
  const [selectedBrokers, setSelectedBrokers] = useState<BrokerType[]>(['fyers', 'kite']);
  const [tradingMode, setTradingMode] = useState<TradingMode>('paper');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [isBottomPanelMinimized, setIsBottomPanelMinimized] = useState(false);

  const brokerState = useBrokerState(selectedBrokers);
  const orderExecution = useOrderExecution();

  // Register adapters on component mount (runs once)
  useEffect(() => {
    console.log('[EquityTradingTab] Registering broker adapters...');
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
        console.log('[EquityTradingTab] Initializing integration manager...');
        await integrationManager.initialize();
        console.log('[EquityTradingTab] ✅ Integration manager initialized');

        // Auto-enable paper trading on mount
        if (!integrationManager.isPaperTradingEnabled()) {
          console.log('[EquityTradingTab] Auto-enabling paper trading...');
          await integrationManager.enablePaperTrading();
          console.log('[EquityTradingTab] ✅ Paper trading enabled automatically');
        }
      } catch (error) {
        console.error('[EquityTradingTab] ❌ Failed to initialize integration manager:', error);
      }
    };

    initializePaperTrading();
  }, []);

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
}> = ({ brokerState, orderExecution, activeBrokers, tradingMode, isBottomPanelMinimized, setIsBottomPanelMinimized }) => {
  const [selectedPanel, setSelectedPanel] = useState<'orders' | 'positions' | 'holdings' | 'stats'>('orders');
  const [selectedSymbol, setSelectedSymbol] = useState('');
  const [selectedExchange, setSelectedExchange] = useState('NSE');
  const [depthBroker, setDepthBroker] = useState<BrokerType>(activeBrokers[0] || 'fyers');
  const [paperPositions, setPaperPositions] = useState<any[]>([]);
  const [paperOrders, setPaperOrders] = useState<any[]>([]);
  const [paperBalance, setPaperBalance] = useState(0);
  const [paperEquity, setPaperEquity] = useState(0);
  const [paperStats, setPaperStats] = useState<any>(null);

  // Fetch paper trading data when paper trading is enabled
  useEffect(() => {
    const fetchPaperData = async () => {
      if (tradingMode === 'paper') {
        // Check if paper trading is enabled
        const isPaperEnabled = integrationManager.isPaperTradingEnabled();
        console.log('[EquityTradingTab] Paper trading enabled:', isPaperEnabled);

        if (!isPaperEnabled) {
          console.log('[EquityTradingTab] Paper trading not enabled, skipping fetch');
          return;
        }

        // Check if paper trading is initialized
        const isInitialized = integrationManager.isPaperTradingInitialized();
        console.log('[EquityTradingTab] Paper trading initialized:', isInitialized);

        if (!isInitialized) {
          console.log('[EquityTradingTab] Paper trading not initialized yet, will retry...');
          return;
        }

        try {
          console.log('[EquityTradingTab] Fetching paper trading data...');

          // Fetch data independently to avoid one failure affecting others
          let positions: any[] = [];
          let orders: any[] = [];
          let balanceValue: number = 0;
          let stats: any = null;

          try {
            positions = await integrationManager.getPaperPositions() || [];
            console.log('[EquityTradingTab] ✅ Positions fetched:', positions.length);
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch positions:', err);
          }

          try {
            orders = await integrationManager.getPaperOrders() || [];
            console.log('[EquityTradingTab] ✅ Orders fetched:', orders.length);
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch orders:', err);
          }

          try {
            balanceValue = await integrationManager.getPaperBalance() || 0;
            console.log('[EquityTradingTab] ✅ Balance fetched:', balanceValue);
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch balance:', err);
          }

          try {
            stats = await integrationManager.getPaperStatistics();
            console.log('[EquityTradingTab] ✅ Stats fetched:', stats);
          } catch (err) {
            console.error('[EquityTradingTab] ❌ Failed to fetch stats:', err);
            // Stats failure is non-critical
          }

          console.log('[EquityTradingTab] Paper data fetched:', {
            positionsCount: positions?.length || 0,
            ordersCount: orders?.length || 0,
            balance: balanceValue,
            balanceType: typeof balanceValue,
            stats: stats
          });

          setPaperPositions(positions || []);
          setPaperOrders(orders || []);

          // Ensure balance is a number, fallback to initial balance if invalid
          let numericBalance = typeof balanceValue === 'number' && balanceValue > 0 ? balanceValue : 0;

          // If balance is still 0, try to get initial balance from stats or use default
          if (numericBalance === 0) {
            numericBalance = stats?.initialBalance || 1000000; // 10 lakhs default
            console.log('[EquityTradingTab] Balance was 0, using initial balance:', numericBalance);
          }

          console.log('[EquityTradingTab] Setting balance to:', numericBalance);
          setPaperBalance(numericBalance);

          // Calculate equity: balance + unrealized P&L
          const unrealizedPnL = (positions || []).reduce((sum, p) => {
            // Check both unrealizedPnl and unrealizedPnL (capital L)
            const pnl = p.unrealizedPnl || p.unrealizedPnL || 0;
            return sum + pnl;
          }, 0);

          const totalEquity = numericBalance + unrealizedPnL;
          console.log('[EquityTradingTab] Setting equity to:', totalEquity, '(balance:', numericBalance, '+ unrealizedPnL:', unrealizedPnL, ')');
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
  const paperPnL = paperStats ? (paperStats.totalPnL || 0) : 0;
  const totalPnL = tradingMode === 'paper' ? paperPnL : livePnL;

  // Use appropriate data based on trading mode
  const allOrders = tradingMode === 'paper' ? paperOrders : liveOrders;
  const allPositions = tradingMode === 'paper' ? paperPositions : livePositions;

  // Calculate return percentage for paper trading
  const initialBalance = paperStats?.initialBalance || 1000000; // Default 10 lakhs
  const paperReturn = initialBalance > 0
    ? ((paperEquity - initialBalance) / initialBalance) * 100
    : 0;

  console.log('[EquityTradingTab] Calculated values:', {
    paperEquity,
    initialBalance,
    paperReturn,
    totalPnL,
    paperStats
  });

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
                const sharpe = getStatValue(paperStats, 'sharpeRatio', null);
                return sharpe !== null ? sharpe.toFixed(2) : 'N/A';
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

        {/* Right Column: Market Depth */}
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
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Activity size={14} color={BLOOMBERG.PURPLE} />
              <span style={{ fontSize: '11px', fontWeight: 700, color: BLOOMBERG.WHITE, letterSpacing: '0.5px' }}>
                MARKET DEPTH
              </span>
            </div>
          </div>
          <div style={{ flex: 1, overflow: 'hidden' }}>
            {selectedSymbol ? (
              <MarketDepthChart
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
                  <Activity size={48} color={BLOOMBERG.MUTED} style={{ margin: '0 auto 12px' }} />
                  <div style={{ fontSize: '12px', fontWeight: 600 }}>SELECT A SYMBOL</div>
                  <div style={{ fontSize: '10px', marginTop: '4px' }}>Enter a symbol to view market depth</div>
                </div>
              </div>
            )}
          </div>
        </div>
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
              ...(tradingMode === 'paper' ? [{ id: 'stats' as const, label: 'STATS', count: paperStats?.totalTrades || 0 }] : [])
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
              />
            )}

            {selectedPanel === 'holdings' && (
              <HoldingsPanel
                holdings={allHoldings}
                loading={brokerState.loading.holdings}
                onRefresh={brokerState.fetchHoldings}
              />
            )}

            {selectedPanel === 'stats' && tradingMode === 'paper' && (
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
                          const sharpe = getStatValue(paperStats, 'sharpeRatio', null);
                          return sharpe !== null ? sharpe.toFixed(2) : 'N/A';
                        })()}
                      </span>
                    </div>
                    <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                      <span style={{ color: BLOOMBERG.GRAY }}>Max Drawdown</span>
                      <span style={{ color: BLOOMBERG.RED, fontWeight: 600 }}>
                        {(() => {
                          const dd = getStatValue(paperStats, 'maxDrawdown', null);
                          return dd !== null ? `${dd.toFixed(2)}%` : 'N/A';
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
