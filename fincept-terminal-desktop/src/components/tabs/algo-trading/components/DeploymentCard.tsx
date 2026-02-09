import React, { useState } from 'react';
import { Square, ChevronDown, ChevronUp, TrendingUp, TrendingDown } from 'lucide-react';
import type { AlgoDeployment } from '../types';
import { stopAlgoDeployment } from '../services/algoTradingService';
import TradeHistoryTable from './TradeHistoryTable';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A',
  CYAN: '#00E5FF', YELLOW: '#FFD700', BLUE: '#0088FF',
};

const statusStyle = (status: string): React.CSSProperties => {
  const base: React.CSSProperties = {
    padding: '2px 8px', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
    letterSpacing: '0.5px',
  };
  switch (status) {
    case 'running': return { ...base, color: F.GREEN, backgroundColor: `${F.GREEN}15` };
    case 'stopped': return { ...base, color: F.GRAY, backgroundColor: `${F.GRAY}15` };
    case 'error': return { ...base, color: F.RED, backgroundColor: `${F.RED}15` };
    case 'starting': return { ...base, color: F.YELLOW, backgroundColor: `${F.YELLOW}15` };
    case 'pending': return { ...base, color: F.BLUE, backgroundColor: `${F.BLUE}15` };
    default: return { ...base, color: F.GRAY, backgroundColor: `${F.GRAY}15` };
  }
};

interface DeploymentCardProps {
  deployment: AlgoDeployment;
  onStopped: () => void;
}

const DeploymentCard: React.FC<DeploymentCardProps> = ({ deployment, onStopped }) => {
  const [showTrades, setShowTrades] = useState(false);
  const [stopping, setStopping] = useState(false);

  const handleStop = async () => {
    setStopping(true);
    await stopAlgoDeployment(deployment.id);
    setStopping(false);
    onStopped();
  };

  const m = deployment.metrics;
  const totalPnl = m.total_pnl + m.unrealized_pnl;
  const isPositive = totalPnl >= 0;

  return (
    <div style={{
      backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
      borderLeft: deployment.status === 'running' ? `2px solid ${F.GREEN}` : `2px solid transparent`,
      overflow: 'hidden',
    }}>
      {/* Main row */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between', padding: '10px 12px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flex: 1, minWidth: 0 }}>
          <div style={statusStyle(deployment.status)}>
            {deployment.status.toUpperCase()}
          </div>
          <div style={{ minWidth: 0 }}>
            <div style={{
              fontSize: '10px', fontWeight: 700, color: F.WHITE,
              overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
            }}>
              {deployment.symbol}
              <span style={{ color: F.MUTED, fontWeight: 400, marginLeft: '8px' }}>
                {deployment.strategy_name || deployment.strategy_id}
              </span>
            </div>
            <div style={{ display: 'flex', gap: '12px', fontSize: '9px', color: F.MUTED, marginTop: '2px' }}>
              <span>{deployment.mode.toUpperCase()}</span>
              <span>{deployment.timeframe}</span>
              <span>QTY: <span style={{ color: F.CYAN }}>{deployment.quantity}</span></span>
              {m.current_position_qty > 0 && (
                <span style={{ color: F.BLUE }}>
                  POS: {m.current_position_side} {m.current_position_qty} @ {m.current_position_entry.toFixed(2)}
                </span>
              )}
            </div>
          </div>
        </div>

        {/* Metrics */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', marginLeft: '12px', marginRight: '12px' }}>
          <div style={{ textAlign: 'right' as const }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>P&L</div>
            <div style={{
              fontSize: '10px', fontWeight: 700, color: isPositive ? F.GREEN : F.RED,
              display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '2px',
            }}>
              {isPositive ? <TrendingUp size={10} /> : <TrendingDown size={10} />}
              {totalPnl >= 0 ? '+' : ''}{totalPnl.toFixed(2)}
            </div>
          </div>
          <div style={{ textAlign: 'right' as const }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>TRADES</div>
            <div style={{ fontSize: '10px', color: F.CYAN }}>{m.total_trades}</div>
          </div>
          <div style={{ textAlign: 'right' as const }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>WIN%</div>
            <div style={{ fontSize: '10px', color: F.WHITE }}>{m.win_rate.toFixed(1)}%</div>
          </div>
          <div style={{ textAlign: 'right' as const }}>
            <div style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px' }}>MAX DD</div>
            <div style={{ fontSize: '10px', color: F.RED }}>{m.max_drawdown.toFixed(2)}</div>
          </div>
        </div>

        {/* Actions */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {deployment.status === 'running' && (
            <button
              onClick={handleStop}
              disabled={stopping}
              style={{
                padding: '4px 8px', backgroundColor: `${F.RED}20`, color: F.RED,
                border: 'none', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '3px',
                opacity: stopping ? 0.5 : 1, transition: 'all 0.2s',
              }}
            >
              <Square size={10} />
              {stopping ? '...' : 'STOP'}
            </button>
          )}
          <button
            onClick={() => setShowTrades(!showTrades)}
            style={{
              padding: '4px 6px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
              color: F.GRAY, borderRadius: '2px', cursor: 'pointer', transition: 'all 0.2s',
            }}
          >
            {showTrades ? <ChevronUp size={12} /> : <ChevronDown size={12} />}
          </button>
        </div>
      </div>

      {/* Trade history */}
      {showTrades && (
        <div style={{ borderTop: `1px solid ${F.BORDER}` }}>
          <TradeHistoryTable deploymentId={deployment.id} />
        </div>
      )}
    </div>
  );
};

export default DeploymentCard;
