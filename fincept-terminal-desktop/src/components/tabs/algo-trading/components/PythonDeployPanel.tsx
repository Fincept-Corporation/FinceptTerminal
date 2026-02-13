import React, { useState, useEffect } from 'react';
import {
  Rocket, X, AlertCircle, Settings, DollarSign, Shield, Clock,
  Activity, CheckCircle, ChevronDown, ChevronRight, Loader2
} from 'lucide-react';
import type { PythonStrategy, StrategyParameter } from '../types';
import {
  extractStrategyParameters,
  getPythonStrategyCode,
} from '../services/algoTradingService';
import { invoke } from '@tauri-apps/api/core';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import ParameterEditor from './ParameterEditor';

const F = {
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

interface PythonDeployPanelProps {
  strategy: PythonStrategy;
  onClose: () => void;
  onDeployed?: (deployId: string) => void;
}

type DeployMode = 'paper' | 'live';

interface DeployConfig {
  symbol: string;
  mode: DeployMode;
  broker: string;
  quantity: number;
  timeframe: string;
  initialCash: number;
  stopLoss: number;
  takeProfit: number;
  maxDailyLoss: number;
}

const BROKERS = [
  { id: 'simulator', label: 'Simulator', live: false },
  { id: 'angelone', label: 'Angel One', live: true },
  { id: 'fyers', label: 'Fyers', live: true },
  { id: 'zerodha', label: 'Zerodha', live: true },
  { id: 'upstox', label: 'Upstox', live: true },
  { id: 'dhan', label: 'Dhan', live: true },
  { id: 'kotak', label: 'Kotak Neo', live: true },
  { id: 'groww', label: 'Groww', live: true },
  { id: 'aliceblue', label: 'AliceBlue', live: true },
  { id: '5paisa', label: '5Paisa', live: true },
  { id: 'iifl', label: 'IIFL', live: true },
  { id: 'motilal', label: 'Motilal Oswal', live: true },
  { id: 'shoonya', label: 'Shoonya (Finvasia)', live: true },
  { id: 'kraken', label: 'Kraken (Crypto)', live: true },
];

const TIMEFRAMES = [
  { value: '1s', label: '1 Second' },
  { value: '5s', label: '5 Seconds' },
  { value: '1m', label: '1 Minute' },
  { value: '5m', label: '5 Minutes' },
  { value: '15m', label: '15 Minutes' },
  { value: '1h', label: '1 Hour' },
  { value: '4h', label: '4 Hours' },
  { value: '1d', label: '1 Day' },
];

const PythonDeployPanel: React.FC<PythonDeployPanelProps> = ({
  strategy,
  onClose,
  onDeployed,
}) => {
  // Configuration state
  const [config, setConfig] = useState<DeployConfig>({
    symbol: '',
    mode: 'paper',
    broker: 'simulator',
    quantity: 1,
    timeframe: '1d',
    initialCash: 100000,
    stopLoss: 0,
    takeProfit: 0,
    maxDailyLoss: 0,
  });

  // Strategy parameters
  const [strategyCode, setStrategyCode] = useState<string | null>(null);
  const [strategyParams, setStrategyParams] = useState<Record<string, string>>({});
  const [showParams, setShowParams] = useState(true);
  const [showRisk, setShowRisk] = useState(true);

  // Deployment state
  const [deploying, setDeploying] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [checkingDuplicate, setCheckingDuplicate] = useState(false);
  const [isDuplicate, setIsDuplicate] = useState(false);

  // Load strategy code for parameter extraction
  useEffect(() => {
    const loadCode = async () => {
      try {
        const result = await getPythonStrategyCode(strategy.id);
        if (result.success && result.data) {
          setStrategyCode(result.data.code);
        }
      } catch (err) {
        console.error('Failed to load strategy code:', err);
      }
    };
    loadCode();
  }, [strategy.id]);

  // Check for duplicate deployment when symbol changes
  useEffect(() => {
    if (!config.symbol.trim()) {
      setIsDuplicate(false);
      return;
    }

    const checkDuplicate = async () => {
      setCheckingDuplicate(true);
      try {
        const result = await invoke<string>('check_algo_duplicate', {
          strategyId: strategy.id,
          symbol: config.symbol.toUpperCase(),
        });
        const parsed = JSON.parse(result);
        setIsDuplicate(parsed.is_duplicate || false);
      } catch {
        setIsDuplicate(false);
      } finally {
        setCheckingDuplicate(false);
      }
    };

    const debounce = setTimeout(checkDuplicate, 300);
    return () => clearTimeout(debounce);
  }, [config.symbol, strategy.id]);

  // Handle deployment
  const handleDeploy = async () => {
    setError(null);
    setSuccess(null);

    // Validation
    if (!config.symbol.trim()) {
      setError('Symbol is required');
      return;
    }

    if (config.quantity <= 0) {
      setError('Quantity must be greater than 0');
      return;
    }

    if (config.mode === 'live' && config.broker === 'simulator') {
      setError('Please select a broker for live trading');
      return;
    }

    setDeploying(true);

    try {
      // Build params object
      const deployParams = {
        symbol: config.symbol.toUpperCase(),
        mode: config.mode,
        broker: config.broker,
        quantity: config.quantity,
        timeframe: config.timeframe,
        initial_cash: config.initialCash,
        stop_loss: config.stopLoss,
        take_profit: config.takeProfit,
        max_daily_loss: config.maxDailyLoss,
        asset_type: isLikelyCrypto(config.symbol) ? 'crypto' : 'equity',
        strategy_type: 'python',
        python_strategy_id: strategy.id,
        strategy_name: strategy.name,
        ...strategyParams, // Include strategy-specific parameters
      };

      const result = await invoke<string>('deploy_algo_strategy', {
        strategyId: strategy.id,
        params: JSON.stringify(deployParams),
      });

      const parsed = JSON.parse(result);

      if (parsed.success) {
        setSuccess(`Strategy deployed successfully! Deployment ID: ${parsed.deploy_id}`);
        onDeployed?.(parsed.deploy_id);
        // Ensure WebSocket subscription for stock brokers so ticks reach candle aggregator
        const sym = config.symbol.toUpperCase();
        if (config.broker !== 'kraken' && config.broker !== 'simulator') {
          invoke('angelone_ws_subscribe', {
            symbol: `NSE:${sym}`, mode: 'ltp', symbolName: sym,
          }).catch(() => {});
        }
        // Auto-close after success
        setTimeout(() => {
          onClose();
        }, 2000);
      } else {
        setError(parsed.error || 'Deployment failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Deployment failed');
    } finally {
      setDeploying(false);
    }
  };

  // Check if symbol is likely crypto
  const isLikelyCrypto = (symbol: string): boolean => {
    const s = symbol.toUpperCase();
    return s.includes('/') || s.includes('-') ||
           ['BTC', 'ETH', 'XRP', 'SOL', 'DOGE', 'ADA', 'DOT', 'LINK'].some(c => s.includes(c));
  };

  const updateConfig = <K extends keyof DeployConfig>(key: K, value: DeployConfig[K]) => {
    setConfig((prev) => ({ ...prev, [key]: value }));
    setError(null);
    setSuccess(null);
  };

  return (
    <div
      style={{
        position: 'fixed',
        top: 0,
        left: 0,
        right: 0,
        bottom: 0,
        backgroundColor: 'rgba(0,0,0,0.85)',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        zIndex: 1000,
      }}
      onClick={(e) => {
        if (e.target === e.currentTarget) onClose();
      }}
    >
      <div
        style={{
          width: '600px',
          maxHeight: '90vh',
          backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          overflow: 'hidden',
          display: 'flex',
          flexDirection: 'column',
        }}
      >
        {/* Header */}
        <div
          style={{
            padding: '16px',
            borderBottom: `2px solid ${F.ORANGE}`,
            backgroundColor: F.HEADER_BG,
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <div style={{
              width: '28px', height: '28px', borderRadius: '2px',
              backgroundColor: `${F.ORANGE}15`, display: 'flex',
              alignItems: 'center', justifyContent: 'center',
            }}>
              <Rocket size={14} color={F.ORANGE} />
            </div>
            <div>
              <div style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                DEPLOY PYTHON STRATEGY
              </div>
              <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '2px' }}>
                {strategy.name.toUpperCase()} • {strategy.id}
              </div>
            </div>
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '6px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.GRAY,
              cursor: 'pointer',
            }}
          >
            <X size={14} />
          </button>
        </div>

        {/* Content */}
        <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
          {/* Mode Selection */}
          <div style={{ marginBottom: '16px' }}>
            <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '8px', display: 'block' }}>
              TRADING MODE
            </label>
            <div style={{ display: 'flex', gap: '8px' }}>
              {(['paper', 'live'] as DeployMode[]).map((mode) => (
                <button
                  key={mode}
                  onClick={() => updateConfig('mode', mode)}
                  style={{
                    flex: 1,
                    padding: '12px',
                    backgroundColor: config.mode === mode ? (mode === 'live' ? `${F.RED}20` : `${F.GREEN}20`) : 'transparent',
                    border: `1px solid ${config.mode === mode ? (mode === 'live' ? F.RED : F.GREEN) : F.BORDER}`,
                    borderRadius: '2px',
                    color: config.mode === mode ? (mode === 'live' ? F.RED : F.GREEN) : F.GRAY,
                    fontSize: '11px',
                    fontWeight: 700,
                    cursor: 'pointer',
                    display: 'flex',
                    flexDirection: 'column',
                    alignItems: 'center',
                    gap: '4px',
                  }}
                >
                  {mode === 'paper' ? <Activity size={16} /> : <DollarSign size={16} />}
                  {mode === 'paper' ? 'PAPER TRADING' : 'LIVE TRADING'}
                  <span style={{ fontSize: '9px', fontWeight: 400, color: F.MUTED }}>
                    {mode === 'paper' ? 'Simulated with fake money' : 'Real money with broker'}
                  </span>
                </button>
              ))}
            </div>
          </div>

          {/* Basic Configuration */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
              {/* Symbol */}
              <div>
                <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px', display: 'block' }}>
                  SYMBOL *
                </label>
                <div style={{ position: 'relative' }}>
                  <input
                    type="text"
                    value={config.symbol}
                    onChange={(e) => updateConfig('symbol', e.target.value.toUpperCase())}
                    placeholder="e.g. AAPL, BTC/USD"
                    style={{
                      width: '100%',
                      padding: '10px',
                      backgroundColor: F.DARK_BG,
                      border: `1px solid ${isDuplicate ? F.YELLOW : F.BORDER}`,
                      borderRadius: '2px',
                      color: F.WHITE,
                      fontSize: '11px',
                    }}
                  />
                  {checkingDuplicate && (
                    <Loader2
                      size={12}
                      style={{
                        position: 'absolute',
                        right: '10px',
                        top: '50%',
                        transform: 'translateY(-50%)',
                        color: F.GRAY,
                        animation: 'spin 1s linear infinite',
                      }}
                    />
                  )}
                </div>
                {isDuplicate && (
                  <div style={{ fontSize: '9px', color: F.YELLOW, marginTop: '4px' }}>
                    Warning: This strategy is already deployed for this symbol
                  </div>
                )}
              </div>

              {/* Broker */}
              <div>
                <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px', display: 'block' }}>
                  BROKER
                </label>
                <select
                  value={config.broker}
                  onChange={(e) => updateConfig('broker', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: F.DARK_BG,
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    color: F.WHITE,
                    fontSize: '11px',
                  }}
                >
                  {BROKERS.filter((b) => config.mode === 'paper' || b.live).map((b) => (
                    <option key={b.id} value={b.id}>
                      {b.label}
                    </option>
                  ))}
                </select>
              </div>

              {/* Quantity */}
              <div>
                <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px', display: 'block' }}>
                  QUANTITY
                </label>
                <input
                  type="number"
                  value={config.quantity}
                  onChange={(e) => updateConfig('quantity', parseFloat(e.target.value) || 0)}
                  min={0.0001}
                  step={0.01}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: F.DARK_BG,
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    color: F.WHITE,
                    fontSize: '11px',
                  }}
                />
              </div>

              {/* Timeframe */}
              <div>
                <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px', display: 'block' }}>
                  TIMEFRAME
                </label>
                <select
                  value={config.timeframe}
                  onChange={(e) => updateConfig('timeframe', e.target.value)}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: F.DARK_BG,
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    color: F.WHITE,
                    fontSize: '11px',
                  }}
                >
                  {TIMEFRAMES.map((tf) => (
                    <option key={tf.value} value={tf.value}>
                      {tf.label}
                    </option>
                  ))}
                </select>
              </div>

              {/* Initial Cash */}
              <div>
                <label style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px', marginBottom: '4px', display: 'block' }}>
                  INITIAL CASH
                </label>
                <input
                  type="number"
                  value={config.initialCash}
                  onChange={(e) => updateConfig('initialCash', parseFloat(e.target.value) || 0)}
                  min={100}
                  step={1000}
                  style={{
                    width: '100%',
                    padding: '10px',
                    backgroundColor: F.DARK_BG,
                    border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px',
                    color: F.WHITE,
                    fontSize: '11px',
                  }}
                />
              </div>
            </div>
          </div>

          {/* Risk Management Section */}
          <div
            style={{
              marginBottom: '16px',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              overflow: 'hidden',
            }}
          >
            <div
              onClick={() => setShowRisk(!showRisk)}
              style={{
                padding: '10px 12px',
                backgroundColor: F.HEADER_BG,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                cursor: 'pointer',
              }}
            >
              {showRisk ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
              <Shield size={12} style={{ color: F.CYAN }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                RISK MANAGEMENT
              </span>
              <span style={{ fontSize: '9px', color: F.MUTED }}>(Optional)</span>
            </div>
            {showRisk && (
              <div style={{ padding: '12px' }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                  <div>
                    <label style={{ fontSize: '9px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                      STOP LOSS ($)
                    </label>
                    <input
                      type="number"
                      value={config.stopLoss}
                      onChange={(e) => updateConfig('stopLoss', parseFloat(e.target.value) || 0)}
                      min={0}
                      step={100}
                      placeholder="0 = disabled"
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: F.DARK_BG,
                        border: `1px solid ${F.BORDER}`,
                        borderRadius: '2px',
                        color: F.WHITE,
                        fontSize: '10px',
                      }}
                    />
                  </div>
                  <div>
                    <label style={{ fontSize: '9px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                      TAKE PROFIT ($)
                    </label>
                    <input
                      type="number"
                      value={config.takeProfit}
                      onChange={(e) => updateConfig('takeProfit', parseFloat(e.target.value) || 0)}
                      min={0}
                      step={100}
                      placeholder="0 = disabled"
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: F.DARK_BG,
                        border: `1px solid ${F.BORDER}`,
                        borderRadius: '2px',
                        color: F.WHITE,
                        fontSize: '10px',
                      }}
                    />
                  </div>
                  <div>
                    <label style={{ fontSize: '9px', color: F.GRAY, marginBottom: '4px', display: 'block' }}>
                      MAX DAILY LOSS ($)
                    </label>
                    <input
                      type="number"
                      value={config.maxDailyLoss}
                      onChange={(e) => updateConfig('maxDailyLoss', parseFloat(e.target.value) || 0)}
                      min={0}
                      step={100}
                      placeholder="0 = disabled"
                      style={{
                        width: '100%',
                        padding: '8px',
                        backgroundColor: F.DARK_BG,
                        border: `1px solid ${F.BORDER}`,
                        borderRadius: '2px',
                        color: F.WHITE,
                        fontSize: '10px',
                      }}
                    />
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* Strategy Parameters Section */}
          {strategyCode && (
            <div
              style={{
                marginBottom: '16px',
                border: `1px solid ${F.BORDER}`,
                borderRadius: '2px',
                overflow: 'hidden',
              }}
            >
              <div
                onClick={() => setShowParams(!showParams)}
                style={{
                  padding: '10px 12px',
                  backgroundColor: F.HEADER_BG,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  cursor: 'pointer',
                }}
              >
                {showParams ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                <Settings size={12} style={{ color: F.PURPLE }} />
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                  STRATEGY PARAMETERS
                </span>
              </div>
              {showParams && (
                <div style={{ padding: '12px' }}>
                  <ParameterEditor
                    code={strategyCode}
                    values={strategyParams}
                    onChange={setStrategyParams}
                  />
                </div>
              )}
            </div>
          )}

          {/* Error/Success Messages */}
          {error && (
            <div
              style={{
                padding: '10px 12px',
                backgroundColor: `${F.RED}20`,
                border: `1px solid ${F.RED}`,
                borderRadius: '2px',
                marginBottom: '16px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                color: F.RED,
                fontSize: '11px',
              }}
            >
              <AlertCircle size={14} />
              {error}
            </div>
          )}

          {success && (
            <div
              style={{
                padding: '10px 12px',
                backgroundColor: `${F.GREEN}20`,
                border: `1px solid ${F.GREEN}`,
                borderRadius: '2px',
                marginBottom: '16px',
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                color: F.GREEN,
                fontSize: '11px',
              }}
            >
              <CheckCircle size={14} />
              {success}
            </div>
          )}

          {/* Live Trading Warning */}
          {config.mode === 'live' && (
            <div
              style={{
                padding: '10px 12px',
                backgroundColor: `${F.YELLOW}10`,
                border: `1px solid ${F.YELLOW}`,
                borderRadius: '2px',
                marginBottom: '16px',
                display: 'flex',
                alignItems: 'flex-start',
                gap: '8px',
                color: F.YELLOW,
                fontSize: '10px',
              }}
            >
              <AlertCircle size={14} style={{ flexShrink: 0, marginTop: '2px' }} />
              <div>
                <strong>Live Trading Warning:</strong> This will execute real trades with real
                money. Ensure your broker is connected and you understand the risks. Always test
                strategies in paper trading first.
              </div>
            </div>
          )}
        </div>

        {/* Footer */}
        <div
          style={{
            padding: '16px',
            borderTop: `1px solid ${F.BORDER}`,
            backgroundColor: F.HEADER_BG,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <button
            onClick={onClose}
            style={{
              padding: '10px 20px',
              backgroundColor: 'transparent',
              border: `1px solid ${F.BORDER}`,
              borderRadius: '2px',
              color: F.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              cursor: 'pointer',
              letterSpacing: '0.5px',
              transition: 'all 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            CANCEL
          </button>
          <button
            onClick={handleDeploy}
            disabled={deploying || !config.symbol.trim()}
            style={{
              padding: '10px 24px',
              backgroundColor: config.mode === 'live' ? F.RED : F.GREEN,
              border: 'none',
              borderRadius: '2px',
              color: F.WHITE,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: deploying || !config.symbol.trim() ? 'not-allowed' : 'pointer',
              opacity: deploying || !config.symbol.trim() ? 0.5 : 1,
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
            }}
          >
            {deploying ? (
              <>
                <Loader2 size={12} style={{ animation: 'spin 1s linear infinite' }} />
                DEPLOYING...
              </>
            ) : (
              <>
                <Rocket size={12} />
                {config.mode === 'live' ? 'DEPLOY LIVE' : 'DEPLOY PAPER'}
              </>
            )}
          </button>
        </div>
      </div>

      <style>{`@keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }`}</style>
    </div>
  );
};

export default PythonDeployPanel;
