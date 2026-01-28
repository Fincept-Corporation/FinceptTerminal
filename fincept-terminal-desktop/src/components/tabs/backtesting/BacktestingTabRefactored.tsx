/**
 * Backtesting Tab - Refactored with UI Design System
 *
 * Implements all features from:
 * - vectorbt_provider.py (9 commands: backtest, optimize, walk_forward, data, indicator, signals, labels, splits, returns)
 * - vbt_strategies.py (20+ trading strategies)
 *
 * Three-panel layout following UI_DESIGN_SYSTEM.md
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  Activity,
  Play,
  Loader,
  TrendingUp,
  Settings,
  BarChart3,
  Database,
  Zap,
  Target,
  Tag,
  Split,
  DollarSign,
  AlertCircle,
  CheckCircle,
  XCircle,
  ChevronRight,
  Save,
  RefreshCw,
} from 'lucide-react';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { VectorBTAdapter } from '@/services/backtesting/adapters/vectorbt/VectorBTAdapter';
import { BacktestRequest } from '@/services/backtesting/interfaces/types';

// ============================================================================
// Color Constants (UI Design System)
// ============================================================================

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

// ============================================================================
// Strategy Definitions (from vbt_strategies.py)
// ============================================================================

const STRATEGY_CATEGORIES = {
  trend: {
    label: 'Trend Following',
    icon: TrendingUp,
    strategies: [
      { id: 'sma_crossover', name: 'SMA Crossover', params: ['fastPeriod', 'slowPeriod'] },
      { id: 'ema_crossover', name: 'EMA Crossover', params: ['fastPeriod', 'slowPeriod'] },
      { id: 'macd', name: 'MACD Crossover', params: ['fastPeriod', 'slowPeriod', 'signalPeriod'] },
      { id: 'adx_trend', name: 'ADX Trend Filter', params: ['adxPeriod', 'adxThreshold'] },
      { id: 'keltner_breakout', name: 'Keltner Channel', params: ['period', 'atrMultiplier'] },
      { id: 'triple_ma', name: 'Triple MA', params: ['fastPeriod', 'mediumPeriod', 'slowPeriod'] },
    ],
  },
  meanReversion: {
    label: 'Mean Reversion',
    icon: Target,
    strategies: [
      { id: 'mean_reversion', name: 'Z-Score Reversion', params: ['window', 'threshold'] },
      { id: 'bollinger_bands', name: 'Bollinger Bands', params: ['period', 'stdDev'] },
      { id: 'rsi', name: 'RSI Mean Reversion', params: ['period', 'oversold', 'overbought'] },
      { id: 'stochastic', name: 'Stochastic', params: ['kPeriod', 'dPeriod', 'oversold', 'overbought'] },
    ],
  },
  momentum: {
    label: 'Momentum',
    icon: Zap,
    strategies: [
      { id: 'momentum', name: 'Momentum (ROC)', params: ['period', 'threshold'] },
      { id: 'dual_momentum', name: 'Dual Momentum', params: ['absolutePeriod', 'relativePeriod'] },
    ],
  },
  breakout: {
    label: 'Breakout',
    icon: BarChart3,
    strategies: [
      { id: 'breakout', name: 'Donchian Breakout', params: ['period'] },
      { id: 'volatility_breakout', name: 'Volatility Breakout', params: ['atrPeriod', 'atrMultiplier'] },
    ],
  },
  multiIndicator: {
    label: 'Multi-Indicator',
    icon: Activity,
    strategies: [
      { id: 'rsi_macd', name: 'RSI + MACD', params: ['rsiPeriod', 'macdFast', 'macdSlow'] },
      { id: 'macd_adx', name: 'MACD + ADX Filter', params: ['macdFast', 'macdSlow', 'adxPeriod'] },
      { id: 'bollinger_rsi', name: 'Bollinger + RSI', params: ['bbPeriod', 'rsiPeriod'] },
    ],
  },
  other: {
    label: 'Other',
    icon: Settings,
    strategies: [
      { id: 'williams_r', name: 'Williams %R', params: ['period', 'oversold', 'overbought'] },
      { id: 'cci', name: 'CCI', params: ['period', 'threshold'] },
      { id: 'obv_trend', name: 'OBV Trend', params: ['maPeriod'] },
    ],
  },
};

// Default parameter values
const DEFAULT_PARAMS: Record<string, any> = {
  fastPeriod: 10,
  slowPeriod: 20,
  mediumPeriod: 30,
  signalPeriod: 9,
  period: 14,
  window: 20,
  threshold: 2.0,
  stdDev: 2.0,
  oversold: 30,
  overbought: 70,
  kPeriod: 14,
  dPeriod: 3,
  atrPeriod: 14,
  atrMultiplier: 2.0,
  adxPeriod: 14,
  adxThreshold: 25,
  rsiPeriod: 14,
  macdFast: 12,
  macdSlow: 26,
  bbPeriod: 20,
  absolutePeriod: 12,
  relativePeriod: 12,
  maPeriod: 20,
};

// ============================================================================
// Command Types (from vectorbt_provider.py)
// ============================================================================

type VBTCommand =
  | 'backtest'
  | 'optimize'
  | 'walk_forward'
  | 'data'
  | 'indicator'
  | 'indicator_signals'
  | 'signals'
  | 'labels'
  | 'splits'
  | 'returns';

interface CommandInfo {
  id: VBTCommand;
  label: string;
  icon: React.ElementType;
  description: string;
}

const COMMANDS: CommandInfo[] = [
  { id: 'backtest', label: 'Backtest', icon: Play, description: 'Run strategy backtest' },
  { id: 'optimize', label: 'Optimize', icon: Target, description: 'Parameter optimization' },
  { id: 'walk_forward', label: 'Walk Forward', icon: ChevronRight, description: 'Walk-forward validation' },
  { id: 'data', label: 'Data', icon: Database, description: 'Download market data' },
  { id: 'indicator', label: 'Indicators', icon: Activity, description: 'Calculate indicators' },
  { id: 'indicator_signals', label: 'Ind. Signals', icon: Zap, description: 'Indicator-based signals' },
  { id: 'signals', label: 'Signals', icon: TrendingUp, description: 'Generate signals' },
  { id: 'labels', label: 'Labels', icon: Tag, description: 'ML label generation' },
  { id: 'splits', label: 'Splits', icon: Split, description: 'Cross-validation splits' },
  { id: 'returns', label: 'Returns', icon: DollarSign, description: 'Returns analysis' },
];

// ============================================================================
// Main Component
// ============================================================================

const BacktestingTabRefactored: React.FC = () => {
  // --- State ---
  const [activeCommand, setActiveCommand] = useState<VBTCommand>('backtest');
  const [selectedCategory, setSelectedCategory] = useState<string>('trend');
  const [selectedStrategy, setSelectedStrategy] = useState<string>('sma_crossover');
  const [strategyParams, setStrategyParams] = useState<Record<string, number>>(DEFAULT_PARAMS);
  const [vbtAdapter] = useState(() => new VectorBTAdapter());
  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<any>(null);
  const [error, setError] = useState<string | null>(null);

  // Backtest configuration
  const [symbols, setSymbols] = useState('SPY');
  const [startDate, setStartDate] = useState(
    new Date(Date.now() - 366 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]
  );
  const [endDate, setEndDate] = useState(
    new Date(Date.now() - 1 * 24 * 60 * 60 * 1000).toISOString().split('T')[0]
  );
  const [initialCapital, setInitialCapital] = useState(100000);
  const [commission, setCommission] = useState(0.001);
  const [slippage, setSlippage] = useState(0.0005);

  // Get current strategy info
  const currentCategory = STRATEGY_CATEGORIES[selectedCategory as keyof typeof STRATEGY_CATEGORIES];
  const currentStrategy = currentCategory?.strategies.find(s => s.id === selectedStrategy);

  // ============================================================================
  // Handlers
  // ============================================================================

  const handleRunBacktest = useCallback(async () => {
    setIsRunning(true);
    setError(null);
    setResult(null);

    try {
      const symbolList = symbols.split(',').map(s => s.trim().toUpperCase()).filter(Boolean);

      const request: BacktestRequest = {
        strategy: {
          name: `${currentStrategy?.name} - ${symbolList.join(',')}`,
          type: selectedStrategy,
          parameters: strategyParams,
        },
        startDate,
        endDate,
        initialCapital,
        assets: symbolList.map(sym => ({
          symbol: sym,
          assetClass: 'stocks' as const,
          weight: 1.0 / symbolList.length,
        })),
        commission,
        slippage,
        tradeOnClose: false,
        hedging: false,
        exclusiveOrders: true,
      };

      const result = await vbtAdapter.runBacktest(request);
      setResult(result);
    } catch (err) {
      setError(String(err));
    } finally {
      setIsRunning(false);
    }
  }, [
    symbols,
    startDate,
    endDate,
    initialCapital,
    commission,
    slippage,
    selectedStrategy,
    strategyParams,
    currentStrategy,
    vbtAdapter,
  ]);

  const handleParamChange = (paramName: string, value: number) => {
    setStrategyParams(prev => ({ ...prev, [paramName]: value }));
  };

  // ============================================================================
  // Render: Top Navigation
  // ============================================================================

  const renderTopNav = () => (
    <div
      style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
        <Activity size={16} color={FINCEPT.ORANGE} />
        <span
          style={{
            fontSize: '12px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            fontFamily: '"IBM Plex Mono", monospace',
            letterSpacing: '0.5px',
          }}
        >
          BACKTESTING - VECTORBT
        </span>
      </div>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
        <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>PROVIDER</span>
        <div
          style={{
            padding: '2px 6px',
            backgroundColor: `${FINCEPT.GREEN}20`,
            color: FINCEPT.GREEN,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
          }}
        >
          ACTIVE
        </div>
      </div>
    </div>
  );

  // ============================================================================
  // Render: Left Panel - Commands
  // ============================================================================

  const renderLeftPanel = () => (
    <div
      style={{
        width: '280px',
        backgroundColor: FINCEPT.DARK_BG,
        borderRight: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Section Header */}
      <div
        style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}
      >
        <span
          style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          COMMANDS
        </span>
      </div>

      {/* Command List */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {COMMANDS.map(cmd => {
          const Icon = cmd.icon;
          const isActive = activeCommand === cmd.id;

          return (
            <div
              key={cmd.id}
              onClick={() => setActiveCommand(cmd.id)}
              style={{
                padding: '10px 12px',
                backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderLeft: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                alignItems: 'center',
                gap: '10px',
              }}
              onMouseEnter={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
              }}
              onMouseLeave={e => {
                if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
              }}
            >
              <Icon size={14} color={isActive ? FINCEPT.ORANGE : FINCEPT.GRAY} />
              <div style={{ flex: 1 }}>
                <div
                  style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                >
                  {cmd.label}
                </div>
                <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
                  {cmd.description}
                </div>
              </div>
            </div>
          );
        })}
      </div>

      {/* Strategy Categories (for backtest/optimize commands) */}
      {(activeCommand === 'backtest' || activeCommand === 'optimize' || activeCommand === 'walk_forward') && (
        <>
          <div
            style={{
              padding: '12px',
              backgroundColor: FINCEPT.HEADER_BG,
              borderTop: `1px solid ${FINCEPT.BORDER}`,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
            }}
          >
            <span
              style={{
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              STRATEGY CATEGORIES
            </span>
          </div>
          <div style={{ overflowY: 'auto', maxHeight: '300px' }}>
            {Object.entries(STRATEGY_CATEGORIES).map(([key, category]) => {
              const Icon = category.icon;
              const isActive = selectedCategory === key;

              return (
                <div
                  key={key}
                  onClick={() => setSelectedCategory(key)}
                  style={{
                    padding: '8px 12px',
                    backgroundColor: isActive ? `${FINCEPT.CYAN}15` : 'transparent',
                    borderLeft: isActive ? `2px solid ${FINCEPT.CYAN}` : '2px solid transparent',
                    cursor: 'pointer',
                    transition: 'all 0.2s',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                  }}
                  onMouseEnter={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                  }}
                  onMouseLeave={e => {
                    if (!isActive) e.currentTarget.style.backgroundColor = 'transparent';
                  }}
                >
                  <Icon size={12} color={isActive ? FINCEPT.CYAN : FINCEPT.GRAY} />
                  <span
                    style={{
                      fontSize: '9px',
                      fontWeight: 700,
                      color: isActive ? FINCEPT.WHITE : FINCEPT.GRAY,
                      fontFamily: '"IBM Plex Mono", monospace',
                    }}
                  >
                    {category.label.toUpperCase()}
                  </span>
                  <span
                    style={{
                      marginLeft: 'auto',
                      fontSize: '8px',
                      color: FINCEPT.MUTED,
                    }}
                  >
                    {category.strategies.length}
                  </span>
                </div>
              );
            })}
          </div>
        </>
      )}
    </div>
  );

  // ============================================================================
  // Render: Center Panel - Configuration
  // ============================================================================

  const renderCenterPanel = () => (
    <div
      style={{
        flex: 1,
        backgroundColor: FINCEPT.DARK_BG,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Section Header */}
      <div
        style={{
          padding: '12px 16px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
        }}
      >
        <span
          style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          {activeCommand.toUpperCase()} CONFIGURATION
        </span>
        <button
          onClick={handleRunBacktest}
          disabled={isRunning}
          style={{
            padding: '6px 12px',
            backgroundColor: isRunning ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG,
            border: 'none',
            borderRadius: '2px',
            fontSize: '9px',
            fontWeight: 700,
            cursor: isRunning ? 'wait' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            opacity: isRunning ? 0.7 : 1,
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          {isRunning ? (
            <>
              <Loader size={10} className="animate-spin" />
              RUNNING...
            </>
          ) : (
            <>
              <Play size={10} />
              RUN {activeCommand.toUpperCase()}
            </>
          )}
        </button>
      </div>

      {/* Content Area */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '16px' }}>
        {activeCommand === 'backtest' && renderBacktestConfig()}
        {activeCommand === 'optimize' && renderOptimizeConfig()}
        {activeCommand === 'walk_forward' && renderWalkForwardConfig()}
        {activeCommand === 'data' && renderDataConfig()}
        {activeCommand === 'indicator' && renderIndicatorConfig()}
        {activeCommand === 'indicator_signals' && renderIndicatorSignalsConfig()}
        {activeCommand === 'signals' && renderSignalsConfig()}
        {activeCommand === 'labels' && renderLabelsConfig()}
        {activeCommand === 'splits' && renderSplitsConfig()}
        {activeCommand === 'returns' && renderReturnsConfig()}
      </div>
    </div>
  );

  // ============================================================================
  // Render: Backtest Configuration
  // ============================================================================

  const renderBacktestConfig = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
      {/* Market Data Section */}
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '12px',
        }}
      >
        <div
          style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            marginBottom: '12px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          MARKET DATA
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              SYMBOLS
            </label>
            <input
              type="text"
              value={symbols}
              onChange={e => setSymbols(e.target.value)}
              placeholder="SPY, AAPL, MSFT"
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              INITIAL CAPITAL
            </label>
            <input
              type="number"
              value={initialCapital}
              onChange={e => setInitialCapital(Number(e.target.value))}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              START DATE
            </label>
            <input
              type="date"
              value={startDate}
              onChange={e => setStartDate(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              END DATE
            </label>
            <input
              type="date"
              value={endDate}
              onChange={e => setEndDate(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
        </div>
      </div>

      {/* Strategy Selection */}
      {currentCategory && (
        <div
          style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            padding: '12px',
          }}
        >
          <div
            style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '12px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}
          >
            {currentCategory.label.toUpperCase()} STRATEGIES
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '8px' }}>
            {currentCategory.strategies.map(strategy => (
              <button
                key={strategy.id}
                onClick={() => setSelectedStrategy(strategy.id)}
                style={{
                  padding: '8px 12px',
                  backgroundColor:
                    selectedStrategy === strategy.id ? FINCEPT.ORANGE : 'transparent',
                  color: selectedStrategy === strategy.id ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  textAlign: 'left',
                  transition: 'all 0.2s',
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
                onMouseEnter={e => {
                  if (selectedStrategy !== strategy.id) {
                    e.currentTarget.style.borderColor = FINCEPT.ORANGE;
                    e.currentTarget.style.color = FINCEPT.WHITE;
                  }
                }}
                onMouseLeave={e => {
                  if (selectedStrategy !== strategy.id) {
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                    e.currentTarget.style.color = FINCEPT.GRAY;
                  }
                }}
              >
                {strategy.name}
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Strategy Parameters */}
      {currentStrategy && (
        <div
          style={{
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            padding: '12px',
          }}
        >
          <div
            style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '12px',
              fontFamily: '"IBM Plex Mono", monospace',
            }}
          >
            {currentStrategy.name.toUpperCase()} PARAMETERS
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px' }}>
            {currentStrategy.params.map(paramName => (
              <div key={paramName}>
                <label
                  style={{
                    display: 'block',
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.GRAY,
                    marginBottom: '4px',
                    letterSpacing: '0.5px',
                    fontFamily: '"IBM Plex Mono", monospace',
                  }}
                >
                  {paramName.replace(/([A-Z])/g, ' $1').toUpperCase()}
                </label>
                <input
                  type="number"
                  value={strategyParams[paramName] ?? DEFAULT_PARAMS[paramName] ?? 10}
                  onChange={e => handleParamChange(paramName, Number(e.target.value))}
                  style={{
                    width: '100%',
                    padding: '8px 10px',
                    backgroundColor: FINCEPT.DARK_BG,
                    color: FINCEPT.CYAN,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    fontSize: '10px',
                    fontFamily: '"IBM Plex Mono", monospace',
                    outline: 'none',
                  }}
                  onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
                  onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
                />
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Execution Settings */}
      <div
        style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
          padding: '12px',
        }}
      >
        <div
          style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            marginBottom: '12px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          EXECUTION SETTINGS
        </div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, 1fr)', gap: '12px' }}>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              COMMISSION (%)
            </label>
            <input
              type="number"
              value={commission * 100}
              onChange={e => setCommission(Number(e.target.value) / 100)}
              step={0.01}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
          <div>
            <label
              style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: FINCEPT.GRAY,
                marginBottom: '4px',
                letterSpacing: '0.5px',
                fontFamily: '"IBM Plex Mono", monospace',
              }}
            >
              SLIPPAGE (%)
            </label>
            <input
              type="number"
              value={slippage * 100}
              onChange={e => setSlippage(Number(e.target.value) / 100)}
              step={0.01}
              style={{
                width: '100%',
                padding: '8px 10px',
                backgroundColor: FINCEPT.DARK_BG,
                color: FINCEPT.WHITE,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                fontSize: '10px',
                fontFamily: '"IBM Plex Mono", monospace',
                outline: 'none',
              }}
              onFocus={e => (e.target.style.borderColor = FINCEPT.ORANGE)}
              onBlur={e => (e.target.style.borderColor = FINCEPT.BORDER)}
            />
          </div>
        </div>
      </div>
    </div>
  );

  // Placeholder configs for other commands
  const renderOptimizeConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Optimize configuration coming soon...
    </div>
  );

  const renderWalkForwardConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Walk-forward configuration coming soon...
    </div>
  );

  const renderDataConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Data download configuration coming soon...
    </div>
  );

  const renderIndicatorConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Indicator configuration coming soon...
    </div>
  );

  const renderIndicatorSignalsConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Indicator signals configuration coming soon...
    </div>
  );

  const renderSignalsConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Signals configuration coming soon...
    </div>
  );

  const renderLabelsConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Labels configuration coming soon...
    </div>
  );

  const renderSplitsConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Splits configuration coming soon...
    </div>
  );

  const renderReturnsConfig = () => (
    <div style={{ color: FINCEPT.GRAY, textAlign: 'center', padding: '40px' }}>
      Returns analysis configuration coming soon...
    </div>
  );

  // ============================================================================
  // Render: Right Panel - Results
  // ============================================================================

  const renderRightPanel = () => (
    <div
      style={{
        width: '350px',
        backgroundColor: FINCEPT.DARK_BG,
        borderLeft: `1px solid ${FINCEPT.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        overflow: 'hidden',
      }}
    >
      {/* Section Header */}
      <div
        style={{
          padding: '12px',
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
        }}
      >
        <span
          style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        >
          RESULTS
        </span>
      </div>

      {/* Results Area */}
      <div style={{ flex: 1, overflowY: 'auto', padding: '12px' }}>
        {error && (
          <div
            style={{
              padding: '12px',
              backgroundColor: `${FINCEPT.RED}20`,
              border: `1px solid ${FINCEPT.RED}`,
              borderRadius: '2px',
              marginBottom: '12px',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'flex-start', gap: '8px' }}>
              <AlertCircle size={14} color={FINCEPT.RED} style={{ marginTop: '2px' }} />
              <div>
                <div
                  style={{
                    fontSize: '9px',
                    fontWeight: 700,
                    color: FINCEPT.RED,
                    marginBottom: '4px',
                  }}
                >
                  ERROR
                </div>
                <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: 1.4 }}>
                  {error}
                </div>
              </div>
            </div>
          </div>
        )}

        {isRunning && (
          <div
            style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              padding: '40px 20px',
              gap: '12px',
            }}
          >
            <Loader size={24} color={FINCEPT.ORANGE} className="animate-spin" />
            <span style={{ fontSize: '10px', color: FINCEPT.GRAY }}>
              Running backtest...
            </span>
          </div>
        )}

        {result && !isRunning && (
          <div
            style={{
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: '2px',
              padding: '12px',
            }}
          >
            <div
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                marginBottom: '12px',
              }}
            >
              <CheckCircle size={14} color={FINCEPT.GREEN} />
              <span
                style={{
                  fontSize: '9px',
                  fontWeight: 700,
                  color: FINCEPT.GREEN,
                  fontFamily: '"IBM Plex Mono", monospace',
                }}
              >
                BACKTEST COMPLETED
              </span>
            </div>
            <pre
              style={{
                fontSize: '9px',
                color: FINCEPT.WHITE,
                fontFamily: '"IBM Plex Mono", monospace',
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-word',
                lineHeight: 1.5,
              }}
            >
              {JSON.stringify(result, null, 2)}
            </pre>
          </div>
        )}

        {!result && !isRunning && !error && (
          <div
            style={{
              display: 'flex',
              flexDirection: 'column',
              alignItems: 'center',
              justifyContent: 'center',
              height: '100%',
              color: FINCEPT.MUTED,
              fontSize: '10px',
              textAlign: 'center',
              gap: '8px',
            }}
          >
            <BarChart3 size={24} style={{ opacity: 0.5 }} />
            <span>No results yet</span>
            <span style={{ fontSize: '9px' }}>Configure and run a backtest</span>
          </div>
        )}
      </div>
    </div>
  );

  // ============================================================================
  // Render: Status Bar
  // ============================================================================

  const renderStatusBar = () => (
    <div
      style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontFamily: '"IBM Plex Mono", monospace',
      }}
    >
      <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
        <span>PROVIDER: VectorBT v0.25.0</span>
        <span>•</span>
        <span>COMMAND: {activeCommand.toUpperCase()}</span>
        {currentStrategy && (
          <>
            <span>•</span>
            <span>STRATEGY: {currentStrategy.name.toUpperCase()}</span>
          </>
        )}
      </div>
      <div>
        {isRunning ? (
          <span style={{ color: FINCEPT.ORANGE }}>● RUNNING</span>
        ) : (
          <span style={{ color: FINCEPT.GREEN }}>● READY</span>
        )}
      </div>
    </div>
  );

  // ============================================================================
  // Main Render
  // ============================================================================

  return (
    <div
      style={{
        height: '100%',
        display: 'flex',
        flexDirection: 'column',
        backgroundColor: FINCEPT.DARK_BG,
        fontFamily: '"IBM Plex Mono", monospace',
      }}
    >
      {renderTopNav()}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden' }}>
        {renderLeftPanel()}
        {renderCenterPanel()}
        {renderRightPanel()}
      </div>
      {renderStatusBar()}
    </div>
  );
};

export default BacktestingTabRefactored;
