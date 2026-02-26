import React, { useState, useEffect } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { Rocket, X, AlertTriangle, Zap, Server, Activity } from 'lucide-react';
import { TIMEFRAMES } from '../constants/indicators';
import { deployAlgoStrategy, startOrderSignalBridge } from '../services/algoTradingService';
import { useBrokerContext } from '@/contexts/BrokerContext';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import { SymbolSearch } from '@/components/tabs/equity-trading/components/SymbolSearch';
import type { SymbolSearchResult, SupportedBroker } from '@/services/trading/masterContractService';
import type { StockExchange } from '@/brokers/stocks/types';

import { F } from '../constants/theme';
import { S } from '../constants/styles';

type AssetType = 'crypto' | 'stock';

interface DeployPanelProps {
  strategyId: string;
  strategyName: string;
  onClose: () => void;
  onDeployed?: (deployId: string) => void;
}

const DeployPanel: React.FC<DeployPanelProps> = ({ strategyId, strategyName, onClose, onDeployed }) => {
  const { activeBroker: cryptoBroker, activeBrokerMetadata: cryptoMeta, realAdapter: cryptoAdapter, tradingMode } = useBrokerContext();
  const stockCtx = useStockBrokerContextOptional();
  const [symbol, setSymbol] = useState('');
  const [selectedExchange, setSelectedExchange] = useState<StockExchange>('NSE' as StockExchange);
  const [selectedToken, setSelectedToken] = useState<string | null>(null);
  const [mode, setMode] = useState<'paper' | 'live'>('paper');
  const [assetType, setAssetType] = useState<AssetType>('stock');
  const [timeframe, setTimeframe] = useState('5m');
  const [quantity, setQuantity] = useState('1');
  const [deploying, setDeploying] = useState(false);
  const [result, setResult] = useState<{ success: boolean; message: string } | null>(null);
  const [stockCredsStored, setStockCredsStored] = useState<{ broker: string; hasToken: boolean } | null>(null);

  // Check if any Indian stock broker has stored credentials (Rust auto-connects on deploy)
  useEffect(() => {
    const brokers = ['angelone', 'fyers', 'zerodha', 'upstox', 'dhan', 'shoonya'];
    let cancelled = false;
    (async () => {
      for (const b of brokers) {
        try {
          const raw = await invoke<string>('get_indian_broker_credentials', { brokerId: b });
          const creds = JSON.parse(raw);
          if (!cancelled && creds && creds.api_key) {
            setStockCredsStored({ broker: b, hasToken: !!creds.access_token });
            return;
          }
        } catch { /* no creds for this broker */ }
      }
    })();
    return () => { cancelled = true; };
  }, []);

  // Derive broker info based on asset type
  const isCrypto = assetType === 'crypto';
  const stockContextConnected = stockCtx?.isAuthenticated ?? false;
  const stockHasStoredCreds = !!stockCredsStored;
  const brokerConnected = isCrypto ? !!cryptoAdapter : (stockContextConnected || stockHasStoredCreds);
  const brokerName = isCrypto
    ? (cryptoMeta?.displayName || cryptoBroker || 'Kraken')
    : (stockCtx?.activeBrokerMetadata?.displayName || stockCtx?.activeBroker || stockCredsStored?.broker?.toUpperCase() || 'Stock Broker');
  const providerName = isCrypto
    ? (cryptoBroker || 'kraken')
    : (stockCtx?.activeBroker || stockCredsStored?.broker || 'angelone');

  const handleSymbolSelect = (sym: string, exchange: StockExchange, result?: SymbolSearchResult) => {
    setSymbol(sym);
    setSelectedExchange(exchange);
    setSelectedToken(result?.token || null);
  };

  const handleDeploy = async () => {
    if (!symbol.trim()) return;
    if (mode === 'live' && !brokerConnected) {
      setResult({ success: false, message: `No broker connected. Connect ${brokerName} first.` });
      return;
    }
    setDeploying(true); setResult(null);

    // Pass token and exchange through params so Rust backend can use them for WS subscribe
    const deployParams: Record<string, string> = {};
    if (!isCrypto && selectedToken) {
      deployParams.token = selectedToken;
      deployParams.exchange = selectedExchange;
    }

    const res = await deployAlgoStrategy({
      strategyId, symbol: symbol.trim().toUpperCase(), provider: providerName,
      mode, timeframe, quantity: parseFloat(quantity) || 1,
      params: Object.keys(deployParams).length > 0 ? JSON.stringify(deployParams) : undefined,
    });
    setDeploying(false);
    if (res.success) {
      const deployId = (res as unknown as Record<string, unknown>).deploy_id ?? res.data?.deploy_id;
      // Rust backend auto-connects broker WS and subscribes during deploy
      setResult({ success: true, message: `Deployed! ID: ${deployId}` });
      if (mode === 'live') startOrderSignalBridge().catch(() => {});
      // Auto-close and redirect to live dashboard after a brief delay
      setTimeout(() => {
        onClose();
        onDeployed?.(String(deployId));
      }, 2000);
    } else {
      setResult({ success: false, message: res.error || 'Deploy failed' });
    }
  };

  return (
    <div
      className={S.modalOverlay}
      style={{ backgroundColor: 'rgba(0,0,0,0.7)' }}
      onClick={e => { if (e.target === e.currentTarget) onClose(); }}
    >
      <div
        className="rounded overflow-visible"
        style={{ width: '560px', backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}` }}
      >
        {/* Header */}
        <div
          className="flex items-center justify-between px-5 py-4"
          style={{ backgroundColor: F.HEADER_BG, borderBottom: `2px solid ${F.GREEN}` }}
        >
          <div className="flex items-center gap-3">
            <div
              className="w-8 h-8 rounded flex items-center justify-center"
              style={{ backgroundColor: `${F.GREEN}20` }}
            >
              <Rocket size={16} color={F.GREEN} />
            </div>
            <div>
              <div className="text-[13px] font-bold tracking-wide" style={{ color: F.WHITE }}>
                DEPLOY STRATEGY
              </div>
              <div className="text-[10px] mt-0.5" style={{ color: F.MUTED }}>
                {strategyName.toUpperCase()}
              </div>
            </div>
          </div>
          <button onClick={onClose} className={S.btnGhost} style={{ color: F.MUTED }}
            onMouseEnter={e => { e.currentTarget.style.color = F.RED; }}
            onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
          >
            <X size={14} />
          </button>
        </div>

        {/* Asset Type Toggle */}
        <div className="flex" style={{ borderBottom: `1px solid ${F.BORDER}` }}>
          {(['stock', 'crypto'] as AssetType[]).map(at => (
            <button
              key={at}
              onClick={() => setAssetType(at)}
              className="flex-1 py-3 border-none cursor-pointer text-[11px] font-bold tracking-wide transition-all duration-200"
              style={{
                backgroundColor: assetType === at ? `${at === 'crypto' ? F.CYAN : F.ORANGE}12` : 'transparent',
                borderBottom: assetType === at ? `2px solid ${at === 'crypto' ? F.CYAN : F.ORANGE}` : '2px solid transparent',
                color: assetType === at ? (at === 'crypto' ? F.CYAN : F.ORANGE) : F.MUTED,
              }}
            >
              {at === 'crypto' ? 'CRYPTO' : 'INDIAN STOCK'}
            </button>
          ))}
        </div>

        {/* Broker Status Bar */}
        <div
          className="flex items-center gap-3 px-5 py-3"
          style={{
            backgroundColor: brokerConnected ? `${F.GREEN}08` : `${F.YELLOW}08`,
            borderBottom: `1px solid ${F.BORDER}`,
          }}
        >
          <div
            className="w-2 h-2 rounded-full"
            style={{ backgroundColor: brokerConnected ? F.GREEN : F.YELLOW }}
          />
          <Server size={12} color={brokerConnected ? F.GREEN : F.YELLOW} />
          <span className="text-[11px] font-bold tracking-wide" style={{ color: brokerConnected ? F.GREEN : F.YELLOW }}>
            {brokerName.toUpperCase()} -- {brokerConnected
              ? (stockHasStoredCreds && !stockContextConnected && !isCrypto
                ? 'CREDENTIALS STORED (AUTO-CONNECT ON DEPLOY)'
                : 'CONNECTED')
              : 'NOT CONNECTED'}
            {brokerConnected && stockContextConnected && tradingMode === 'paper' && ' (PAPER MODE)'}
          </span>
          {!isCrypto && !brokerConnected && (
            <span className="text-[10px] ml-auto" style={{ color: F.MUTED }}>
              Login via Equity Trading tab
            </span>
          )}
        </div>

        {/* Config Form */}
        <div className="p-5">
          <div className="grid grid-cols-2 gap-4 mb-4">
            <div className="relative z-[100]">
              <label className={S.label}>SYMBOL</label>
              {isCrypto ? (
                <input
                  value={symbol}
                  onChange={e => setSymbol(e.target.value)}
                  placeholder="e.g. BTC/USD"
                  className={S.input}
                  onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                  onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
                />
              ) : (
                <SymbolSearch
                  selectedSymbol={symbol}
                  selectedExchange={selectedExchange}
                  onSymbolSelect={handleSymbolSelect}
                  brokerId={(providerName || 'angelone') as SupportedBroker}
                  placeholder="Search stocks..."
                  showDownloadStatus={false}
                  exchangeFilter="NSE"
                  instrumentTypeFilter="EQ"
                />
              )}
              {!isCrypto && selectedToken && (
                <div className="text-[10px] mt-1" style={{ color: F.CYAN }}>
                  {selectedExchange}:{selectedToken}
                </div>
              )}
            </div>
            <div>
              <label className={S.label}>EXECUTION MODE</label>
              <select
                value={mode}
                onChange={e => setMode(e.target.value as 'paper' | 'live')}
                className={S.select}
                style={{
                  color: mode === 'live' ? F.RED : F.GREEN,
                  borderColor: mode === 'live' ? `${F.RED}40` : `${F.GREEN}40`,
                }}
              >
                <option value="paper">Paper Trading</option>
                <option value="live">Live Trading</option>
              </select>
            </div>
            <div>
              <label className={S.label}>TIMEFRAME</label>
              <select value={timeframe} onChange={e => setTimeframe(e.target.value)} className={S.select}>
                {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
              </select>
            </div>
            <div>
              <label className={S.label}>QUANTITY</label>
              <input
                type="number" value={quantity}
                onChange={e => setQuantity(e.target.value)}
                min="0.01" step="any" className={S.input}
                onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
              />
            </div>
          </div>

          {/* Warnings */}
          {mode === 'live' && !brokerConnected && (
            <div
              className="flex items-center gap-2 rounded px-4 py-3 mb-3"
              style={{ backgroundColor: `${F.RED}12`, border: `1px solid ${F.RED}30` }}
            >
              <AlertTriangle size={14} color={F.RED} />
              <span className="text-[11px] font-bold" style={{ color: F.RED }}>
                LIVE MODE REQUIRES AN ACTIVE BROKER CONNECTION
              </span>
            </div>
          )}
          {mode === 'live' && brokerConnected && (
            <div
              className="flex items-center gap-2 rounded px-4 py-3 mb-3"
              style={{ backgroundColor: `${F.YELLOW}12`, border: `1px solid ${F.YELLOW}30` }}
            >
              <Zap size={14} color={F.YELLOW} />
              <span className="text-[11px] font-bold" style={{ color: F.YELLOW }}>
                LIVE MODE -- REAL ORDERS WILL BE PLACED VIA {brokerName.toUpperCase()}
              </span>
            </div>
          )}

          {/* Result */}
          {result && (
            <div
              className="flex items-center gap-2 rounded px-4 py-3 mb-3"
              style={{
                backgroundColor: result.success ? `${F.GREEN}15` : `${F.RED}15`,
                border: `1px solid ${result.success ? F.GREEN + '40' : F.RED + '40'}`,
              }}
            >
              <Activity size={14} color={result.success ? F.GREEN : F.RED} />
              <span className="text-[11px] font-bold" style={{ color: result.success ? F.GREEN : F.RED }}>
                {result.message}
              </span>
            </div>
          )}

          {/* Deploy button */}
          <button
            onClick={handleDeploy}
            disabled={deploying || !symbol.trim()}
            className={`${S.btnPrimary} w-full border-none`}
            style={{
              padding: '14px',
              backgroundColor: mode === 'live' ? F.RED : F.GREEN,
              color: mode === 'live' ? F.WHITE : F.DARK_BG,
              cursor: (deploying || !symbol.trim()) ? 'not-allowed' : 'pointer',
              opacity: (deploying || !symbol.trim()) ? 0.5 : 1,
            }}
          >
            <Rocket size={14} />
            {deploying ? 'DEPLOYING STRATEGY...' : mode === 'live' ? 'DEPLOY TO LIVE' : 'DEPLOY TO PAPER'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default DeployPanel;
