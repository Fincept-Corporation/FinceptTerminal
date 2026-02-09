import React, { useState } from 'react';
import { Rocket, X, Link, AlertTriangle } from 'lucide-react';
import { TIMEFRAMES } from '../constants/indicators';
import { deployAlgoStrategy, startOrderSignalBridge } from '../services/algoTradingService';
import { useBrokerContext } from '@/contexts/BrokerContext';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', YELLOW: '#FFD700',
};

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
  color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
  fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none',
};

const labelStyle: React.CSSProperties = {
  fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
  marginBottom: '4px', display: 'block',
};

interface DeployPanelProps {
  strategyId: string;
  strategyName: string;
  onClose: () => void;
}

const DeployPanel: React.FC<DeployPanelProps> = ({ strategyId, strategyName, onClose }) => {
  const { activeBroker, activeBrokerMetadata, realAdapter, tradingMode } = useBrokerContext();
  const [symbol, setSymbol] = useState('');
  const [mode, setMode] = useState<'paper' | 'live'>('paper');
  const [timeframe, setTimeframe] = useState('5m');
  const [quantity, setQuantity] = useState('1');
  const [deploying, setDeploying] = useState(false);
  const [result, setResult] = useState<{ success: boolean; message: string } | null>(null);

  const brokerConnected = !!realAdapter;
  const brokerName = activeBrokerMetadata?.displayName || activeBroker;

  const handleDeploy = async () => {
    if (!symbol.trim()) return;
    if (mode === 'live' && !brokerConnected) {
      setResult({ success: false, message: `No broker connected. Connect ${brokerName} first.` });
      return;
    }
    setDeploying(true); setResult(null);
    const res = await deployAlgoStrategy({
      strategyId, symbol: symbol.trim().toUpperCase(), provider: activeBroker,
      mode, timeframe, quantity: parseFloat(quantity) || 1,
    });
    setDeploying(false);
    if (res.success) {
      setResult({ success: true, message: `Deployed! ID: ${res.data?.deploy_id}` });
      if (mode === 'live') startOrderSignalBridge().catch(() => {});
    } else {
      setResult({ success: false, message: res.error || 'Deploy failed' });
    }
  };

  return (
    <div style={{
      backgroundColor: F.PANEL_BG, border: `1px solid ${F.GREEN}30`,
      borderLeft: `2px solid ${F.GREEN}`, borderRadius: '2px', padding: '12px',
    }}>
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '8px' }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GREEN, letterSpacing: '0.5px' }}>
          DEPLOY: {strategyName.toUpperCase()}
        </span>
        <button onClick={onClose} style={{ padding: '4px', backgroundColor: 'transparent', border: 'none', color: F.MUTED, cursor: 'pointer' }}>
          <X size={12} />
        </button>
      </div>

      {/* Broker Status */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px',
        padding: '4px 8px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
        backgroundColor: brokerConnected ? `${F.GREEN}15` : `${F.YELLOW}15`,
        color: brokerConnected ? F.GREEN : F.YELLOW,
      }}>
        <Link size={10} />
        <span>
          {brokerName.toUpperCase()} — {brokerConnected ? 'CONNECTED' : 'NOT CONNECTED'}
          {brokerConnected && tradingMode === 'paper' && ' (PAPER MODE)'}
        </span>
      </div>

      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr 1fr', gap: '8px' }}>
        <div><label style={labelStyle}>SYMBOL</label><input value={symbol} onChange={e => setSymbol(e.target.value)} placeholder="RELIANCE" style={inputStyle} /></div>
        <div><label style={labelStyle}>MODE</label>
          <select value={mode} onChange={e => setMode(e.target.value as 'paper' | 'live')} style={inputStyle}>
            <option value="paper">Paper</option><option value="live">Live</option>
          </select>
        </div>
        <div><label style={labelStyle}>TIMEFRAME</label>
          <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={inputStyle}>
            {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
          </select>
        </div>
        <div><label style={labelStyle}>QUANTITY</label>
          <input type="number" value={quantity} onChange={e => setQuantity(e.target.value)} min="0.01" step="any" style={inputStyle} />
        </div>
      </div>

      {mode === 'live' && !brokerConnected && (
        <div style={{ marginTop: '6px', padding: '4px 8px', backgroundColor: `${F.RED}15`, color: F.RED, fontSize: '8px', fontWeight: 700, borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px' }}>
          <AlertTriangle size={10} /> LIVE MODE REQUIRES ACTIVE BROKER CONNECTION
        </div>
      )}

      {mode === 'live' && brokerConnected && (
        <div style={{ marginTop: '6px', padding: '4px 8px', backgroundColor: `${F.YELLOW}15`, color: F.YELLOW, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
          LIVE MODE — REAL ORDERS VIA {brokerName.toUpperCase()}
        </div>
      )}

      {result && (
        <div style={{
          marginTop: '6px', padding: '4px 8px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
          backgroundColor: result.success ? `${F.GREEN}20` : `${F.RED}20`,
          color: result.success ? F.GREEN : F.RED,
        }}>{result.message}</div>
      )}

      <button onClick={handleDeploy} disabled={deploying || !symbol.trim()} style={{
        marginTop: '8px', padding: '8px 16px', backgroundColor: F.GREEN, color: F.DARK_BG,
        border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
        opacity: (deploying || !symbol.trim()) ? 0.5 : 1,
        display: 'flex', alignItems: 'center', gap: '4px',
      }}>
        <Rocket size={10} />
        {deploying ? 'DEPLOYING...' : 'DEPLOY STRATEGY'}
      </button>
    </div>
  );
};

export default DeployPanel;
