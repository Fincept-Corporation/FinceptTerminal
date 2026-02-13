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

type AssetType = 'crypto' | 'stock';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', YELLOW: '#FFD700', HOVER: '#1F1F1F',
};

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
  color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
  fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none',
  transition: 'border-color 0.2s',
};

const labelStyle: React.CSSProperties = {
  fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px',
  marginBottom: '4px', display: 'block',
};

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
    <div style={{
      position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.7)', display: 'flex',
      alignItems: 'center', justifyContent: 'center', zIndex: 1000,
    }}
      onClick={e => { if (e.target === e.currentTarget) onClose(); }}
    >
      <div style={{
        width: '520px', backgroundColor: F.PANEL_BG,
        border: `1px solid ${F.BORDER}`, borderRadius: '2px',
        overflow: 'visible',
      }}>
        {/* Header */}
        <div style={{
          padding: '14px 16px',
          backgroundColor: F.HEADER_BG,
          borderBottom: `2px solid ${F.GREEN}`,
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
            <div style={{
              width: '28px', height: '28px', borderRadius: '2px',
              backgroundColor: `${F.GREEN}20`,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}>
              <Rocket size={14} color={F.GREEN} />
            </div>
            <div>
              <div style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                DEPLOY STRATEGY
              </div>
              <div style={{ fontSize: '9px', color: F.MUTED, marginTop: '2px' }}>
                {strategyName.toUpperCase()}
              </div>
            </div>
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
              color: F.MUTED, cursor: 'pointer', borderRadius: '2px', transition: 'all 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.RED; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.MUTED; }}
          >
            <X size={12} />
          </button>
        </div>

        {/* Asset Type Toggle */}
        <div style={{
          display: 'flex', padding: '0', borderBottom: `1px solid ${F.BORDER}`,
        }}>
          {(['stock', 'crypto'] as AssetType[]).map(at => (
            <button key={at} onClick={() => setAssetType(at)} style={{
              flex: 1, padding: '10px', border: 'none', cursor: 'pointer',
              backgroundColor: assetType === at ? `${at === 'crypto' ? F.CYAN : F.ORANGE}12` : 'transparent',
              borderBottom: assetType === at ? `2px solid ${at === 'crypto' ? F.CYAN : F.ORANGE}` : '2px solid transparent',
              color: assetType === at ? (at === 'crypto' ? F.CYAN : F.ORANGE) : F.MUTED,
              fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', transition: 'all 0.2s',
            }}>
              {at === 'crypto' ? 'CRYPTO' : 'INDIAN STOCK'}
            </button>
          ))}
        </div>

        {/* Broker Status Bar */}
        <div style={{
          display: 'flex', alignItems: 'center', gap: '8px',
          padding: '10px 16px',
          backgroundColor: brokerConnected ? `${F.GREEN}08` : `${F.YELLOW}08`,
          borderBottom: `1px solid ${F.BORDER}`,
        }}>
          <div style={{
            width: '8px', height: '8px', borderRadius: '50%',
            backgroundColor: brokerConnected ? F.GREEN : F.YELLOW,
          }} />
          <Server size={10} color={brokerConnected ? F.GREEN : F.YELLOW} />
          <span style={{
            fontSize: '9px', fontWeight: 700,
            color: brokerConnected ? F.GREEN : F.YELLOW,
            letterSpacing: '0.5px',
          }}>
            {brokerName.toUpperCase()} -- {brokerConnected
              ? (stockHasStoredCreds && !stockContextConnected && !isCrypto
                ? 'CREDENTIALS STORED (AUTO-CONNECT ON DEPLOY)'
                : 'CONNECTED')
              : 'NOT CONNECTED'}
            {brokerConnected && stockContextConnected && tradingMode === 'paper' && ' (PAPER MODE)'}
          </span>
          {!isCrypto && !brokerConnected && (
            <span style={{ fontSize: '8px', color: F.MUTED, marginLeft: 'auto' }}>
              Login via Equity Trading tab
            </span>
          )}
        </div>

        {/* Config Form */}
        <div style={{ padding: '16px' }}>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '12px' }}>
            <div style={{ position: 'relative', zIndex: 100 }}>
              <label style={labelStyle}>SYMBOL</label>
              {isCrypto ? (
                <input
                  value={symbol}
                  onChange={e => setSymbol(e.target.value)}
                  placeholder="e.g. BTC/USD"
                  style={inputStyle}
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
                <div style={{ fontSize: '8px', color: F.CYAN, marginTop: '2px' }}>
                  {selectedExchange}:{selectedToken}
                </div>
              )}
            </div>
            <div>
              <label style={labelStyle}>EXECUTION MODE</label>
              <select value={mode} onChange={e => setMode(e.target.value as 'paper' | 'live')} style={{
                ...inputStyle,
                color: mode === 'live' ? F.RED : F.GREEN,
                borderColor: mode === 'live' ? `${F.RED}40` : `${F.GREEN}40`,
              }}>
                <option value="paper">Paper Trading</option>
                <option value="live">Live Trading</option>
              </select>
            </div>
            <div>
              <label style={labelStyle}>TIMEFRAME</label>
              <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={inputStyle}>
                {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
              </select>
            </div>
            <div>
              <label style={labelStyle}>QUANTITY</label>
              <input
                type="number" value={quantity}
                onChange={e => setQuantity(e.target.value)}
                min="0.01" step="any" style={inputStyle}
                onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
              />
            </div>
          </div>

          {/* Warnings */}
          {mode === 'live' && !brokerConnected && (
            <div style={{
              padding: '8px 12px', backgroundColor: `${F.RED}12`,
              border: `1px solid ${F.RED}30`, borderRadius: '2px',
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px',
            }}>
              <AlertTriangle size={10} color={F.RED} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.RED }}>
                LIVE MODE REQUIRES AN ACTIVE BROKER CONNECTION
              </span>
            </div>
          )}
          {mode === 'live' && brokerConnected && (
            <div style={{
              padding: '8px 12px', backgroundColor: `${F.YELLOW}12`,
              border: `1px solid ${F.YELLOW}30`, borderRadius: '2px',
              display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '10px',
            }}>
              <Zap size={10} color={F.YELLOW} />
              <span style={{ fontSize: '9px', fontWeight: 700, color: F.YELLOW }}>
                LIVE MODE -- REAL ORDERS WILL BE PLACED VIA {brokerName.toUpperCase()}
              </span>
            </div>
          )}

          {/* Result */}
          {result && (
            <div style={{
              padding: '8px 12px', borderRadius: '2px', marginBottom: '10px',
              display: 'flex', alignItems: 'center', gap: '6px',
              backgroundColor: result.success ? `${F.GREEN}15` : `${F.RED}15`,
              border: `1px solid ${result.success ? F.GREEN + '40' : F.RED + '40'}`,
            }}>
              <Activity size={10} color={result.success ? F.GREEN : F.RED} />
              <span style={{
                fontSize: '9px', fontWeight: 700,
                color: result.success ? F.GREEN : F.RED,
              }}>
                {result.message}
              </span>
            </div>
          )}

          {/* Deploy button */}
          <button
            onClick={handleDeploy}
            disabled={deploying || !symbol.trim()}
            style={{
              width: '100%', padding: '12px',
              backgroundColor: mode === 'live' ? F.RED : F.GREEN,
              color: mode === 'live' ? F.WHITE : F.DARK_BG,
              border: 'none', borderRadius: '2px',
              fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px',
              cursor: (deploying || !symbol.trim()) ? 'not-allowed' : 'pointer',
              opacity: (deploying || !symbol.trim()) ? 0.5 : 1,
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
              transition: 'all 0.2s',
            }}
          >
            <Rocket size={12} />
            {deploying ? 'DEPLOYING STRATEGY...' : mode === 'live' ? 'DEPLOY TO LIVE' : 'DEPLOY TO PAPER'}
          </button>
        </div>
      </div>
    </div>
  );
};

export default DeployPanel;
