/**
 * Backtesting Tab - Professional Backtesting Interface
 *
 * Clean, provider-aware backtesting UI that works with:
 * - Backtesting.py (Python strategies, fast event-driven)
 * - VectorBT (NumPy vectorized, ultra-fast)
 * - Backtrader (Python-based backtesting framework)
 */

import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { Activity, TrendingUp, Settings, Play, StopCircle, ChevronDown, ChevronRight } from 'lucide-react';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { BacktestRequest, BacktestResult } from '@/services/backtesting/interfaces/types';
import { sqliteService } from '@/services/sqliteService';

// ============================================================================
// Types
// ============================================================================

interface BacktestConfig {
  // Universal
  symbol: string;
  startDate: string;
  endDate: string;
  initialCapital: number;

  // Strategy
  strategyType: 'sma_crossover' | 'ema_crossover' | 'rsi' | 'macd' | 'bollinger_bands' | 'custom';
  strategyCode?: string;

  // Parameters (dynamic based on strategy)
  parameters: Record<string, any>;

  // Provider-specific
  commission?: number;
  slippage?: number;
  leverage?: number;
}

const BacktestingTabNew: React.FC = () => {
  const { t } = useTranslation('backtesting');
  const colors = {
    bg: '#0A0A0A',
    panel: '#1A1A1A',
    border: '#2A2A2A',
    accent: '#FF8800',
    text: '#FFFFFF',
    textMuted: '#888888',
    success: '#00FF88',
    alert: '#FF4444',
  };

  // ============================================================================
  // State
  // ============================================================================

  const [activeProvider, setActiveProvider] = useState<string | null>(null);
  const [availableProviders, setAvailableProviders] = useState<string[]>([]);
  const [showProviderDropdown, setShowProviderDropdown] = useState(false);

  const [config, setConfig] = useState<BacktestConfig>({
    symbol: 'SPY',
    startDate: new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    endDate: new Date().toISOString().split('T')[0],
    initialCapital: 100000,
    strategyType: 'sma_crossover',
    parameters: {
      fastPeriod: 10,
      slowPeriod: 20,
    },
    commission: 0.001,
    slippage: 0.0005,
    leverage: 1,
  });

  const [isRunning, setIsRunning] = useState(false);
  const [result, setResult] = useState<BacktestResult | null>(null);
  const [error, setError] = useState<string | null>(null);

  const [expandedSections, setExpandedSections] = useState({
    provider: true,
    symbol: true,
    strategy: true,
    parameters: true,
    advanced: false,
    results: true,
  });

  // ============================================================================
  // Initialize
  // ============================================================================

  useEffect(() => {
    initializeProviders();
  }, []);

  const initializeProviders = async () => {
    try {
      const allProviders = backtestingRegistry.listProviders();
      const providerNames = allProviders.map(p => typeof p === 'string' ? p : p.name);
      setAvailableProviders(providerNames);

      // Get active provider from DB
      const dbProviders = await sqliteService.getBacktestingProviders();
      const activeDbProvider = dbProviders.find(p => p.is_active === true || (p.is_active as any) === 1);

      if (activeDbProvider) {
        setActiveProvider(activeDbProvider.name);
      } else if (providerNames.length > 0) {
        setActiveProvider(providerNames[0]);
      }
    } catch (err) {
      console.error('[Backtesting] Failed to initialize providers:', err);
      setError('Failed to load backtesting providers');
    }
  };

  // ============================================================================
  // Provider Switching
  // ============================================================================

  const handleProviderSwitch = async (providerName: string) => {
    try {
      setShowProviderDropdown(false);

      const provider = backtestingRegistry.getProvider(providerName);
      if (!provider) {
        throw new Error(`Provider ${providerName} not found`);
      }

      await backtestingRegistry.setActiveProvider(providerName);
      await sqliteService.setActiveBacktestingProvider(providerName);

      setActiveProvider(providerName);
      setError(null);

      // Reset config based on provider capabilities
      resetConfigForProvider(providerName);
    } catch (err) {
      setError(`Failed to switch provider: ${err}`);
    }
  };

  const resetConfigForProvider = (providerName: string) => {
    // Provider-specific defaults
    if (providerName === 'Backtesting.py') {
      setConfig(prev => ({
        ...prev,
        strategyType: 'sma_crossover',
        parameters: { fastPeriod: 10, slowPeriod: 20 },
        commission: 0.001,
      }));
    } else if (providerName === 'VectorBT') {
      setConfig(prev => ({
        ...prev,
        strategyType: 'sma_crossover',
        parameters: { fastWindow: 10, slowWindow: 20 },
      }));
    }
  };

  // ============================================================================
  // Run Backtest
  // ============================================================================

  const handleRunBacktest = async () => {
    if (!activeProvider) {
      setError('No active provider selected');
      return;
    }

    setIsRunning(true);
    setError(null);
    setResult(null);

    try {
      const provider = backtestingRegistry.getProvider(activeProvider);
      if (!provider) {
        throw new Error('Provider not found');
      }

      // Build request based on provider type
      const request: BacktestRequest = {
        strategy: {
          name: `${config.strategyType} - ${config.symbol}`,
          type: config.strategyType,
          code: config.strategyCode,
          parameters: config.parameters,
        },
        startDate: config.startDate,
        endDate: config.endDate,
        initialCapital: config.initialCapital,
        assets: [{
          symbol: config.symbol,
          assetClass: 'stocks',
          weight: 1.0,
        }],
        commission: config.commission,
        // Backtesting.py specific
        tradeOnClose: false,
        hedging: false,
        exclusiveOrders: true,
        margin: config.leverage,
      };

      console.log('[Backtesting] Running backtest:', request);

      const backtestResult = await provider.runBacktest(request);

      setResult(backtestResult);
      setExpandedSections(prev => ({ ...prev, results: true }));

    } catch (err) {
      console.error('[Backtesting] Error:', err);
      setError(String(err));
    } finally {
      setIsRunning(false);
    }
  };

  // ============================================================================
  // UI Helpers
  // ============================================================================

  const toggleSection = (section: keyof typeof expandedSections) => {
    setExpandedSections(prev => ({ ...prev, [section]: !prev[section] }));
  };

  const SectionHeader: React.FC<{
    title: string;
    section: keyof typeof expandedSections;
    icon: React.ReactNode;
  }> = ({ title, section, icon }) => (
    <div
      onClick={() => toggleSection(section)}
      style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        padding: '12px 16px',
        background: colors.panel,
        borderBottom: `1px solid ${colors.border}`,
        cursor: 'pointer',
        userSelect: 'none',
      }}
    >
      {expandedSections[section] ? <ChevronDown size={16} /> : <ChevronRight size={16} />}
      {icon}
      <span style={{ fontWeight: 600, fontSize: '0.9rem' }}>{title}</span>
    </div>
  );

  const FormField: React.FC<{
    label: string;
    children: React.ReactNode;
    description?: string;
  }> = ({ label, children, description }) => (
    <div style={{ marginBottom: '16px' }}>
      <label style={{
        display: 'block',
        fontSize: '0.85rem',
        color: colors.textMuted,
        marginBottom: '6px',
        fontWeight: 500,
      }}>
        {label}
      </label>
      {children}
      {description && (
        <div style={{ fontSize: '0.75rem', color: colors.textMuted, marginTop: '4px' }}>
          {description}
        </div>
      )}
    </div>
  );

  // ============================================================================
  // Strategy-Specific Parameters
  // ============================================================================

  const renderStrategyParameters = () => {
    if (config.strategyType === 'sma_crossover') {
      return (
        <>
          <FormField label={t('smaParams.fastPeriod')} description={t('smaParams.fastNote')}>
            <input
              type="number"
              value={config.parameters.fastPeriod || 10}
              onChange={(e) => setConfig(prev => ({
                ...prev,
                parameters: { ...prev.parameters, fastPeriod: parseInt(e.target.value) }
              }))}
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.bg,
                border: `1px solid ${colors.border}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </FormField>

          <FormField label={t('smaParams.slowPeriod')} description={t('smaParams.slowNote')}>
            <input
              type="number"
              value={config.parameters.slowPeriod || 20}
              onChange={(e) => setConfig(prev => ({
                ...prev,
                parameters: { ...prev.parameters, slowPeriod: parseInt(e.target.value) }
              }))}
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.bg,
                border: `1px solid ${colors.border}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </FormField>
        </>
      );
    } else if (config.strategyType === 'rsi') {
      return (
        <>
          <FormField label="RSI Period" description="Lookback period for RSI">
            <input
              type="number"
              value={config.parameters.period || 14}
              onChange={(e) => setConfig(prev => ({
                ...prev,
                parameters: { ...prev.parameters, period: parseInt(e.target.value) }
              }))}
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.bg,
                border: `1px solid ${colors.border}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </FormField>

          <FormField label="Oversold Threshold">
            <input
              type="number"
              value={config.parameters.oversold || 30}
              onChange={(e) => setConfig(prev => ({
                ...prev,
                parameters: { ...prev.parameters, oversold: parseInt(e.target.value) }
              }))}
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.bg,
                border: `1px solid ${colors.border}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </FormField>

          <FormField label="Overbought Threshold">
            <input
              type="number"
              value={config.parameters.overbought || 70}
              onChange={(e) => setConfig(prev => ({
                ...prev,
                parameters: { ...prev.parameters, overbought: parseInt(e.target.value) }
              }))}
              style={{
                width: '100%',
                padding: '8px 12px',
                background: colors.bg,
                border: `1px solid ${colors.border}`,
                borderRadius: '4px',
                color: colors.text,
                fontSize: '0.9rem',
              }}
            />
          </FormField>
        </>
      );
    } else if (config.strategyType === 'custom' && activeProvider === 'Backtesting.py') {
      return (
        <FormField label="Python Strategy Code" description="Custom strategy using Backtesting.py API">
          <textarea
            value={config.strategyCode || ''}
            onChange={(e) => setConfig(prev => ({ ...prev, strategyCode: e.target.value }))}
            placeholder={`class MyStrategy(Strategy):
    def init(self):
        self.sma = self.I(SMA, self.data.Close, 20)

    def next(self):
        if self.data.Close[-1] > self.sma[-1]:
            self.buy()
        elif self.data.Close[-1] < self.sma[-1]:
            self.sell()`}
            style={{
              width: '100%',
              minHeight: '200px',
              padding: '12px',
              background: colors.bg,
              border: `1px solid ${colors.border}`,
              borderRadius: '4px',
              color: colors.text,
              fontSize: '0.85rem',
              fontFamily: 'monospace',
              resize: 'vertical',
            }}
          />
        </FormField>
      );
    }

    return null;
  };

  // ============================================================================
  // Render
  // ============================================================================

  return (
    <div style={{
      width: '100%',
      height: '100%',
      background: colors.bg,
      color: colors.text,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: '#1A1A1A',
        borderBottom: `2px solid ${colors.accent}`,
        padding: '12px 20px',
        boxShadow: `0 2px 8px rgba(255, 136, 0, 0.2)`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <Activity size={20} color={colors.accent} style={{ filter: `drop-shadow(0 0 4px ${colors.accent})` }} />
          <span style={{
            color: colors.accent,
            fontWeight: 700,
            fontSize: '15px',
            letterSpacing: '0.5px',
            textShadow: `0 0 10px rgba(255, 136, 0, 0.4)`,
          }}>
            {t('title')}
          </span>
        </div>

        {/* Provider Selector */}
        <div style={{ position: 'relative' }}>
          <button
            onClick={() => setShowProviderDropdown(!showProviderDropdown)}
            style={{
              padding: '6px 14px',
              background: colors.panel,
              border: `1px solid ${colors.accent}`,
              borderRadius: '4px',
              color: colors.text,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              fontSize: '0.85rem',
              fontWeight: 600,
            }}
          >
            <Settings size={14} />
            {activeProvider || 'Select Provider'}
            <ChevronDown size={14} />
          </button>

          {showProviderDropdown && (
            <div style={{
              position: 'absolute',
              top: '100%',
              right: 0,
              marginTop: '4px',
              background: colors.panel,
              border: `1px solid ${colors.accent}`,
              borderRadius: '4px',
              minWidth: '200px',
              zIndex: 1000,
              boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
            }}>
              {availableProviders.map(provider => (
                <div
                  key={provider}
                  onClick={() => handleProviderSwitch(provider)}
                  style={{
                    padding: '10px 14px',
                    cursor: 'pointer',
                    borderBottom: `1px solid ${colors.border}`,
                    background: provider === activeProvider ? `${colors.accent}22` : 'transparent',
                    color: provider === activeProvider ? colors.accent : colors.text,
                    fontWeight: provider === activeProvider ? 600 : 400,
                  }}
                >
                  {provider}
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      {/* Main Content */}
      <div style={{
        flex: 1,
        display: 'grid',
        gridTemplateColumns: '1fr 1fr',
        gap: '16px',
        padding: '16px',
        overflow: 'auto',
      }}>
        {/* Left Column - Configuration */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>

          {/* Symbol Selection */}
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.border}`,
            borderRadius: '6px',
            overflow: 'hidden',
          }}>
            <SectionHeader title={t('config.symbolTimeframe')} section="symbol" icon={<TrendingUp size={16} />} />

            {expandedSections.symbol && (
              <div style={{ padding: '16px' }}>
                <FormField label={t('config.symbol')} description={t('config.stockTicker')}>
                  <input
                    type="text"
                    value={config.symbol}
                    onChange={(e) => setConfig(prev => ({ ...prev, symbol: e.target.value.toUpperCase() }))}
                    placeholder="SPY"
                    style={{
                      width: '100%',
                      padding: '10px 14px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '1rem',
                      fontWeight: 600,
                      textTransform: 'uppercase',
                    }}
                  />
                </FormField>

                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                  <FormField label={t('config.startDate')}>
                    <input
                      type="date"
                      value={config.startDate}
                      onChange={(e) => setConfig(prev => ({ ...prev, startDate: e.target.value }))}
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        background: colors.bg,
                        border: `1px solid ${colors.border}`,
                        borderRadius: '4px',
                        color: colors.text,
                        fontSize: '0.9rem',
                      }}
                    />
                  </FormField>

                  <FormField label={t('config.endDate')}>
                    <input
                      type="date"
                      value={config.endDate}
                      onChange={(e) => setConfig(prev => ({ ...prev, endDate: e.target.value }))}
                      style={{
                        width: '100%',
                        padding: '8px 12px',
                        background: colors.bg,
                        border: `1px solid ${colors.border}`,
                        borderRadius: '4px',
                        color: colors.text,
                        fontSize: '0.9rem',
                      }}
                    />
                  </FormField>
                </div>

                <FormField label={t('config.initialCapital')}>
                  <input
                    type="number"
                    value={config.initialCapital}
                    onChange={(e) => setConfig(prev => ({ ...prev, initialCapital: parseFloat(e.target.value) }))}
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '0.9rem',
                    }}
                  />
                </FormField>
              </div>
            )}
          </div>

          {/* Strategy Selection */}
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.border}`,
            borderRadius: '6px',
            overflow: 'hidden',
          }}>
            <SectionHeader title={t('sections.strategyConfig')} section="strategy" icon={<Settings size={16} />} />

            {expandedSections.strategy && (
              <div style={{ padding: '16px' }}>
                <FormField label={t('sections.strategyType')}>
                  <select
                    value={config.strategyType}
                    onChange={(e) => setConfig(prev => ({
                      ...prev,
                      strategyType: e.target.value as any,
                      parameters: {},
                    }))}
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '0.9rem',
                    }}
                  >
                    <option value="sma_crossover">SMA Crossover</option>
                    <option value="rsi">RSI Strategy</option>
                    <option value="macd">MACD Strategy</option>
                    {activeProvider === 'Backtesting.py' && (
                      <option value="custom">Custom Python Code</option>
                    )}
                  </select>
                </FormField>
              </div>
            )}
          </div>

          {/* Parameters */}
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.border}`,
            borderRadius: '6px',
            overflow: 'hidden',
          }}>
            <SectionHeader title={t('sections.strategyParams')} section="parameters" icon={<Settings size={16} />} />

            {expandedSections.parameters && (
              <div style={{ padding: '16px' }}>
                {renderStrategyParameters()}
              </div>
            )}
          </div>

          {/* Advanced Settings */}
          <div style={{
            background: colors.panel,
            border: `1px solid ${colors.border}`,
            borderRadius: '6px',
            overflow: 'hidden',
          }}>
            <SectionHeader title={t('sections.advancedSettings')} section="advanced" icon={<Settings size={16} />} />

            {expandedSections.advanced && (
              <div style={{ padding: '16px' }}>
                <FormField label="Commission (%)" description="Trading commission percentage">
                  <input
                    type="number"
                    step="0.0001"
                    value={config.commission}
                    onChange={(e) => setConfig(prev => ({ ...prev, commission: parseFloat(e.target.value) }))}
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '0.9rem',
                    }}
                  />
                </FormField>

                <FormField label="Slippage (%)" description="Expected slippage per trade">
                  <input
                    type="number"
                    step="0.0001"
                    value={config.slippage}
                    onChange={(e) => setConfig(prev => ({ ...prev, slippage: parseFloat(e.target.value) }))}
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '0.9rem',
                    }}
                  />
                </FormField>

                <FormField label="Leverage" description="Margin/leverage multiplier">
                  <input
                    type="number"
                    step="0.1"
                    value={config.leverage}
                    onChange={(e) => setConfig(prev => ({ ...prev, leverage: parseFloat(e.target.value) }))}
                    style={{
                      width: '100%',
                      padding: '8px 12px',
                      background: colors.bg,
                      border: `1px solid ${colors.border}`,
                      borderRadius: '4px',
                      color: colors.text,
                      fontSize: '0.9rem',
                    }}
                  />
                </FormField>
              </div>
            )}
          </div>

          {/* Run Button */}
          <button
            onClick={handleRunBacktest}
            disabled={isRunning || !activeProvider}
            style={{
              padding: '14px',
              background: isRunning ? colors.border : `linear-gradient(135deg, ${colors.accent}, #FF6600)`,
              border: 'none',
              borderRadius: '6px',
              color: colors.text,
              fontSize: '1rem',
              fontWeight: 700,
              cursor: isRunning ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '10px',
              boxShadow: isRunning ? 'none' : `0 4px 12px rgba(255, 136, 0, 0.3)`,
              transition: 'all 0.2s',
            }}
          >
            {isRunning ? (
              <>
                <StopCircle size={18} />
                Running Backtest...
              </>
            ) : (
              <>
                <Play size={18} />
                {t('toolbar.run')}
              </>
            )}
          </button>

          {error && (
            <div style={{
              padding: '12px',
              background: `${colors.alert}22`,
              border: `1px solid ${colors.alert}`,
              borderRadius: '6px',
              color: colors.alert,
              fontSize: '0.85rem',
            }}>
              {error}
            </div>
          )}
        </div>

        {/* Right Column - Results */}
        <div style={{
          background: colors.panel,
          border: `1px solid ${colors.border}`,
          borderRadius: '6px',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}>
          <SectionHeader title={t('sections.backtestResults')} section="results" icon={<TrendingUp size={16} />} />

          {expandedSections.results && (
            <div style={{ flex: 1, padding: '16px', overflow: 'auto' }}>
              {!result && !isRunning && (
                <div style={{
                  height: '100%',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  color: colors.textMuted,
                  fontSize: '0.9rem',
                }}>
                  Configure your backtest and click "Run Backtest" to see results
                </div>
              )}

              {isRunning && (
                <div style={{
                  height: '100%',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  flexDirection: 'column',
                  gap: '16px',
                }}>
                  <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-[#FF8800]"></div>
                  <div style={{ color: colors.textMuted }}>Running backtest on {activeProvider}...</div>
                </div>
              )}

              {result && (
                <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
                  {/* Summary */}
                  <div style={{
                    padding: '16px',
                    background: colors.bg,
                    border: `1px solid ${colors.border}`,
                    borderRadius: '6px',
                  }}>
                    <h4 style={{ marginBottom: '12px', fontSize: '0.9rem', color: colors.textMuted }}>
                      Backtest Summary
                    </h4>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '0.85rem' }}>
                      <div>Symbol: <span style={{ color: colors.accent, fontWeight: 600 }}>{config.symbol}</span></div>
                      <div>Strategy: <span style={{ color: colors.text, fontWeight: 600 }}>{config.strategyType}</span></div>
                      <div>Period: {config.startDate} to {config.endDate}</div>
                      <div>Provider: {activeProvider}</div>
                    </div>
                  </div>

                  {/* Performance Metrics */}
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                    {[
                      { label: 'Total Return', value: ((result.performance?.totalReturn ?? 0) * 100).toFixed(2) + '%', positive: (result.performance?.totalReturn ?? 0) > 0 },
                      { label: 'Sharpe Ratio', value: (result.performance?.sharpeRatio ?? 0).toFixed(2), positive: (result.performance?.sharpeRatio ?? 0) > 0 },
                      { label: 'Max Drawdown', value: ((result.performance?.maxDrawdown ?? 0) * 100).toFixed(2) + '%', positive: false },
                      { label: 'Win Rate', value: ((result.performance?.winRate ?? 0) * 100).toFixed(1) + '%', positive: true },
                      { label: 'Total Trades', value: (result.performance?.totalTrades ?? 0).toString(), positive: true },
                      { label: 'Profit Factor', value: (result.performance?.profitFactor ?? 0).toFixed(2), positive: (result.performance?.profitFactor ?? 0) > 1 },
                    ].map((metric) => (
                      <div key={metric.label} style={{
                        padding: '12px',
                        background: colors.bg,
                        border: `1px solid ${colors.border}`,
                        borderRadius: '6px',
                      }}>
                        <div style={{ fontSize: '0.75rem', color: colors.textMuted, marginBottom: '4px' }}>
                          {metric.label}
                        </div>
                        <div style={{
                          fontSize: '1.3rem',
                          fontWeight: 'bold',
                          color: metric.positive ? colors.success : colors.alert,
                        }}>
                          {metric.value}
                        </div>
                      </div>
                    ))}
                  </div>

                  {/* Trades Summary */}
                  <div style={{
                    padding: '12px',
                    background: colors.bg,
                    border: `1px solid ${colors.border}`,
                    borderRadius: '6px',
                  }}>
                    <h4 style={{ marginBottom: '8px', fontSize: '0.85rem' }}>Recent Trades</h4>
                    <div style={{ fontSize: '0.75rem', color: colors.textMuted }}>
                      {result.trades?.length ?? 0} total trades executed
                    </div>
                  </div>
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default BacktestingTabNew;
