import React, { useState, useEffect } from 'react';
import {
  Rocket, X, AlertCircle, Settings, DollarSign, Shield, Clock,
  Activity, CheckCircle, ChevronDown, ChevronRight, Loader2
} from 'lucide-react';
import type { PythonStrategy } from '../types';
import {
  getPythonStrategyCode,
} from '../services/algoTradingService';
import { invoke } from '@tauri-apps/api/core';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import ParameterEditor from './ParameterEditor';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

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

  const [strategyCode, setStrategyCode] = useState<string | null>(null);
  const [strategyParams, setStrategyParams] = useState<Record<string, string>>({});
  const [showParams, setShowParams] = useState(true);
  const [showRisk, setShowRisk] = useState(true);

  const [deploying, setDeploying] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);
  const [checkingDuplicate, setCheckingDuplicate] = useState(false);
  const [isDuplicate, setIsDuplicate] = useState(false);

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

  const handleDeploy = async () => {
    setError(null);
    setSuccess(null);

    if (!config.symbol.trim()) { setError('Symbol is required'); return; }
    if (config.quantity <= 0) { setError('Quantity must be greater than 0'); return; }
    if (config.mode === 'live' && config.broker === 'simulator') {
      setError('Please select a broker for live trading');
      return;
    }

    setDeploying(true);

    try {
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
        ...strategyParams,
      };

      const result = await invoke<string>('deploy_algo_strategy', {
        strategyId: strategy.id,
        params: JSON.stringify(deployParams),
      });

      const parsed = JSON.parse(result);

      if (parsed.success) {
        setSuccess(`Strategy deployed successfully! Deployment ID: ${parsed.deploy_id}`);
        onDeployed?.(parsed.deploy_id);
        const sym = config.symbol.toUpperCase();
        if (config.broker !== 'kraken' && config.broker !== 'simulator') {
          invoke('angelone_ws_subscribe', {
            symbol: `NSE:${sym}`, mode: 'ltp', symbolName: sym,
          }).catch(() => {});
        }
        setTimeout(() => { onClose(); }, 2000);
      } else {
        setError(parsed.error || 'Deployment failed');
      }
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Deployment failed');
    } finally {
      setDeploying(false);
    }
  };

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
      className={S.modalOverlay}
      style={{ backgroundColor: 'rgba(0,0,0,0.85)' }}
      onClick={(e) => { if (e.target === e.currentTarget) onClose(); }}
    >
      <div
        className="flex flex-col rounded overflow-hidden"
        style={{
          width: '640px',
          maxHeight: '90vh',
          backgroundColor: F.PANEL_BG,
          border: `1px solid ${F.BORDER}`,
        }}
      >
        {/* Header */}
        <div
          className="flex items-center justify-between px-5 py-4 shrink-0"
          style={{ borderBottom: `2px solid ${F.ORANGE}`, backgroundColor: F.HEADER_BG }}
        >
          <div className="flex items-center gap-3">
            <div
              className="w-8 h-8 rounded flex items-center justify-center"
              style={{ backgroundColor: `${F.ORANGE}15` }}
            >
              <Rocket size={16} style={{ color: F.ORANGE }} />
            </div>
            <div>
              <div className="text-[13px] font-bold tracking-wide" style={{ color: F.WHITE }}>
                DEPLOY PYTHON STRATEGY
              </div>
              <div className="text-[10px] mt-0.5" style={{ color: F.MUTED }}>
                {strategy.name.toUpperCase()} &middot; {strategy.id}
              </div>
            </div>
          </div>
          <button onClick={onClose} className={S.btnGhost} style={{ color: F.GRAY }}>
            <X size={16} />
          </button>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-auto p-5">
          {/* Mode Selection */}
          <div className="mb-5">
            <label className={S.label}>TRADING MODE</label>
            <div className="flex gap-3">
              {(['paper', 'live'] as DeployMode[]).map((m) => (
                <button
                  key={m}
                  onClick={() => updateConfig('mode', m)}
                  className="flex-1 flex flex-col items-center gap-1.5 rounded py-3 cursor-pointer transition-all duration-200 border"
                  style={{
                    backgroundColor: config.mode === m ? (m === 'live' ? `${F.RED}20` : `${F.GREEN}20`) : 'transparent',
                    borderColor: config.mode === m ? (m === 'live' ? F.RED : F.GREEN) : F.BORDER,
                    color: config.mode === m ? (m === 'live' ? F.RED : F.GREEN) : F.GRAY,
                  }}
                >
                  {m === 'paper' ? <Activity size={18} /> : <DollarSign size={18} />}
                  <span className="text-[11px] font-bold tracking-wide">
                    {m === 'paper' ? 'PAPER TRADING' : 'LIVE TRADING'}
                  </span>
                  <span className="text-[10px]" style={{ color: F.MUTED }}>
                    {m === 'paper' ? 'Simulated with fake money' : 'Real money with broker'}
                  </span>
                </button>
              ))}
            </div>
          </div>

          {/* Basic Configuration */}
          <div className="grid grid-cols-2 gap-4 mb-5">
            {/* Symbol */}
            <div>
              <label className={S.label}>SYMBOL *</label>
              <div className="relative">
                <input
                  type="text"
                  value={config.symbol}
                  onChange={(e) => updateConfig('symbol', e.target.value.toUpperCase())}
                  placeholder="e.g. AAPL, BTC/USD"
                  className={S.input}
                  style={{ borderColor: isDuplicate ? F.YELLOW : undefined }}
                />
                {checkingDuplicate && (
                  <Loader2
                    size={14}
                    className="animate-spin"
                    style={{
                      position: 'absolute', right: '10px', top: '50%',
                      transform: 'translateY(-50%)', color: F.GRAY,
                    }}
                  />
                )}
              </div>
              {isDuplicate && (
                <div className="text-[10px] mt-1" style={{ color: F.YELLOW }}>
                  Warning: This strategy is already deployed for this symbol
                </div>
              )}
            </div>

            {/* Broker */}
            <div>
              <label className={S.label}>BROKER</label>
              <select
                value={config.broker}
                onChange={(e) => updateConfig('broker', e.target.value)}
                className={S.select}
              >
                {BROKERS.filter((b) => config.mode === 'paper' || b.live).map((b) => (
                  <option key={b.id} value={b.id}>{b.label}</option>
                ))}
              </select>
            </div>

            {/* Quantity */}
            <div>
              <label className={S.label}>QUANTITY</label>
              <input
                type="number"
                value={config.quantity}
                onChange={(e) => updateConfig('quantity', parseFloat(e.target.value) || 0)}
                min={0.0001} step={0.01}
                className={S.input}
              />
            </div>

            {/* Timeframe */}
            <div>
              <label className={S.label}>TIMEFRAME</label>
              <select
                value={config.timeframe}
                onChange={(e) => updateConfig('timeframe', e.target.value)}
                className={S.select}
              >
                {TIMEFRAMES.map((tf) => (
                  <option key={tf.value} value={tf.value}>{tf.label}</option>
                ))}
              </select>
            </div>

            {/* Initial Cash */}
            <div>
              <label className={S.label}>INITIAL CASH</label>
              <input
                type="number"
                value={config.initialCash}
                onChange={(e) => updateConfig('initialCash', parseFloat(e.target.value) || 0)}
                min={100} step={1000}
                className={S.input}
              />
            </div>
          </div>

          {/* Risk Management Section */}
          <div className={`${S.section} mb-5`}>
            <button
              onClick={() => setShowRisk(!showRisk)}
              className="w-full flex items-center gap-2 px-4 py-3 border-none cursor-pointer"
              style={{ backgroundColor: F.HEADER_BG, color: F.WHITE }}
            >
              {showRisk ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
              <Shield size={14} style={{ color: F.CYAN }} />
              <span className="text-[11px] font-bold tracking-wide">RISK MANAGEMENT</span>
              <span className="text-[10px]" style={{ color: F.MUTED }}>(Optional)</span>
            </button>
            {showRisk && (
              <div className="p-4">
                <div className="grid grid-cols-3 gap-4">
                  <div>
                    <label className={S.label}>STOP LOSS ($)</label>
                    <input
                      type="number"
                      value={config.stopLoss}
                      onChange={(e) => updateConfig('stopLoss', parseFloat(e.target.value) || 0)}
                      min={0} step={100} placeholder="0 = disabled"
                      className={S.input}
                    />
                  </div>
                  <div>
                    <label className={S.label}>TAKE PROFIT ($)</label>
                    <input
                      type="number"
                      value={config.takeProfit}
                      onChange={(e) => updateConfig('takeProfit', parseFloat(e.target.value) || 0)}
                      min={0} step={100} placeholder="0 = disabled"
                      className={S.input}
                    />
                  </div>
                  <div>
                    <label className={S.label}>MAX DAILY LOSS ($)</label>
                    <input
                      type="number"
                      value={config.maxDailyLoss}
                      onChange={(e) => updateConfig('maxDailyLoss', parseFloat(e.target.value) || 0)}
                      min={0} step={100} placeholder="0 = disabled"
                      className={S.input}
                    />
                  </div>
                </div>
              </div>
            )}
          </div>

          {/* Strategy Parameters Section */}
          {strategyCode && (
            <div className={`${S.section} mb-5`}>
              <button
                onClick={() => setShowParams(!showParams)}
                className="w-full flex items-center gap-2 px-4 py-3 border-none cursor-pointer"
                style={{ backgroundColor: F.HEADER_BG, color: F.WHITE }}
              >
                {showParams ? <ChevronDown size={14} /> : <ChevronRight size={14} />}
                <Settings size={14} style={{ color: F.PURPLE }} />
                <span className="text-[11px] font-bold tracking-wide">STRATEGY PARAMETERS</span>
              </button>
              {showParams && (
                <div className="p-4">
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
              className="flex items-center gap-2 rounded px-4 py-3 mb-4 text-[11px]"
              style={{ backgroundColor: `${F.RED}20`, border: `1px solid ${F.RED}`, color: F.RED }}
            >
              <AlertCircle size={16} />
              {error}
            </div>
          )}

          {success && (
            <div
              className="flex items-center gap-2 rounded px-4 py-3 mb-4 text-[11px]"
              style={{ backgroundColor: `${F.GREEN}20`, border: `1px solid ${F.GREEN}`, color: F.GREEN }}
            >
              <CheckCircle size={16} />
              {success}
            </div>
          )}

          {/* Live Trading Warning */}
          {config.mode === 'live' && (
            <div
              className="flex items-start gap-2 rounded px-4 py-3 mb-4 text-[11px]"
              style={{ backgroundColor: `${F.YELLOW}10`, border: `1px solid ${F.YELLOW}`, color: F.YELLOW }}
            >
              <AlertCircle size={16} className="shrink-0 mt-0.5" />
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
          className="flex items-center justify-between px-5 py-4 shrink-0"
          style={{ borderTop: `1px solid ${F.BORDER}`, backgroundColor: F.HEADER_BG }}
        >
          <button onClick={onClose} className={S.btnOutline}>CANCEL</button>
          <button
            onClick={handleDeploy}
            disabled={deploying || !config.symbol.trim()}
            className={`${S.btnPrimary} border-none`}
            style={{
              padding: '10px 24px',
              backgroundColor: config.mode === 'live' ? F.RED : F.GREEN,
              color: F.WHITE,
              cursor: deploying || !config.symbol.trim() ? 'not-allowed' : 'pointer',
              opacity: deploying || !config.symbol.trim() ? 0.5 : 1,
            }}
          >
            {deploying ? (
              <>
                <Loader2 size={14} className="animate-spin" />
                DEPLOYING...
              </>
            ) : (
              <>
                <Rocket size={14} />
                {config.mode === 'live' ? 'DEPLOY LIVE' : 'DEPLOY PAPER'}
              </>
            )}
          </button>
        </div>
      </div>
    </div>
  );
};

export default PythonDeployPanel;
