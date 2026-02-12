/**
 * Grid Status Panel
 * Fincept Terminal Style - Inline styles
 */

import React from 'react';
import {
  Grid, Play, Pause, Square, Trash2, TrendingUp, TrendingDown,
  DollarSign, Activity, BarChart3, Clock, AlertCircle,
} from 'lucide-react';
import type { GridState } from '../../../../services/gridTrading/types';

const FINCEPT = {
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
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

interface GridStatusPanelProps {
  grid: GridState;
  onStart: () => void;
  onPause: () => void;
  onStop: () => void;
  onDelete: () => void;
}

export function GridStatusPanel({
  grid,
  onStart,
  onPause,
  onStop,
  onDelete,
}: GridStatusPanelProps) {
  const { config, status, currentPrice, levels } = grid;

  const openOrders = levels.filter(l => l.status === 'open').length;
  const filledOrders = levels.filter(l => l.status === 'filled').length;
  const pendingOrders = levels.filter(l => l.status === 'pending').length;
  const buyOrders = levels.filter(l => l.side === 'buy' && l.status === 'open').length;
  const sellOrders = levels.filter(l => l.side === 'sell' && l.status === 'open').length;
  const pricePosition = ((currentPrice - config.lowerPrice) / (config.upperPrice - config.lowerPrice)) * 100;

  const getStatusColor = () => {
    switch (status) {
      case 'running': return FINCEPT.GREEN;
      case 'paused': return FINCEPT.YELLOW;
      case 'stopped': return FINCEPT.GRAY;
      case 'error': return FINCEPT.RED;
      default: return FINCEPT.GRAY;
    }
  };

  const formatDuration = (ms: number): string => {
    const seconds = Math.floor(ms / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}d ${hours % 24}h`;
    if (hours > 0) return `${hours}h ${minutes % 60}m`;
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
    return `${seconds}s`;
  };

  const runningTime = grid.startedAt ? Date.now() - grid.startedAt : 0;
  const statusColor = getStatusColor();

  const btnStyle: React.CSSProperties = {
    flex: 1,
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    gap: '4px',
    padding: '8px 12px',
    fontSize: '10px',
    fontWeight: 700,
    fontFamily: '"IBM Plex Mono", monospace',
    border: 'none',
    cursor: 'pointer',
    transition: 'all 0.15s',
  };

  const statBoxStyle: React.CSSProperties = {
    padding: '10px 12px',
    backgroundColor: FINCEPT.HEADER_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
  };

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", monospace' }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '10px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        marginBottom: '10px',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Grid size={14} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '12px', fontWeight: 700, color: FINCEPT.WHITE }}>{config.symbol}</span>
          <span style={{
            padding: '2px 8px',
            backgroundColor: `${statusColor}20`,
            color: statusColor,
            fontSize: '9px',
            fontWeight: 700,
            textTransform: 'uppercase',
          }}>
            {status}
          </span>
        </div>
        <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>{config.brokerId}</span>
      </div>

      {/* Controls */}
      <div style={{ display: 'flex', gap: '6px', marginBottom: '10px' }}>
        {(status === 'draft' || status === 'paused') && (
          <button onClick={onStart} style={{ ...btnStyle, backgroundColor: FINCEPT.GREEN, color: FINCEPT.DARK_BG }}>
            <Play size={12} />
            {status === 'draft' ? 'START' : 'RESUME'}
          </button>
        )}
        {status === 'running' && (
          <button onClick={onPause} style={{ ...btnStyle, backgroundColor: FINCEPT.YELLOW, color: FINCEPT.DARK_BG }}>
            <Pause size={12} />
            PAUSE
          </button>
        )}
        {(status === 'running' || status === 'paused') && (
          <button onClick={onStop} style={{
            ...btnStyle,
            backgroundColor: FINCEPT.HEADER_BG,
            color: FINCEPT.RED,
            border: `1px solid ${FINCEPT.RED}40`,
          }}>
            <Square size={12} />
            STOP
          </button>
        )}
        <button onClick={onDelete} style={{
          padding: '8px 10px',
          backgroundColor: FINCEPT.HEADER_BG,
          color: FINCEPT.RED,
          border: `1px solid ${FINCEPT.BORDER}`,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
        }}>
          <Trash2 size={12} />
        </button>
      </div>

      {/* Price Info */}
      <div style={{ ...statBoxStyle, marginBottom: '10px' }}>
        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8 }}>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY, textTransform: 'uppercase' }}>Current Price</span>
          <span style={{ fontSize: '16px', fontWeight: 700, color: FINCEPT.WHITE, fontFamily: 'monospace' }}>
            ${currentPrice.toFixed(2)}
          </span>
        </div>
        {/* Price position bar */}
        <div style={{
          position: 'relative',
          height: 6,
          backgroundColor: FINCEPT.BORDER,
          borderRadius: 3,
          overflow: 'hidden',
        }}>
          <div style={{
            position: 'absolute',
            top: 0,
            left: 0,
            height: '100%',
            width: '100%',
            background: `linear-gradient(90deg, ${FINCEPT.RED}, ${FINCEPT.YELLOW}, ${FINCEPT.GREEN})`,
          }} />
          <div style={{
            position: 'absolute',
            top: 0,
            height: '100%',
            width: 3,
            backgroundColor: FINCEPT.WHITE,
            left: `${Math.max(0, Math.min(100, pricePosition))}%`,
            transform: 'translateX(-50%)',
            boxShadow: '0 0 4px rgba(255,255,255,0.5)',
          }} />
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: 4, fontSize: '9px', color: FINCEPT.MUTED }}>
          <span>${config.lowerPrice.toFixed(2)}</span>
          <span>${config.upperPrice.toFixed(2)}</span>
        </div>
      </div>

      {/* Statistics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '10px' }}>
        {/* P&L */}
        <div style={statBoxStyle}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: 4 }}>
            <DollarSign size={10} />
            TOTAL P&L
          </div>
          <div style={{
            fontSize: '16px',
            fontWeight: 700,
            fontFamily: 'monospace',
            color: grid.totalPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
          }}>
            {grid.totalPnl >= 0 ? '+' : ''}{grid.totalPnl.toFixed(2)}
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
            Realized: {grid.realizedPnl >= 0 ? '+' : ''}{grid.realizedPnl.toFixed(2)}
          </div>
        </div>

        {/* Grid Profit */}
        <div style={statBoxStyle}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: 4 }}>
            <BarChart3 size={10} />
            GRID PROFIT
          </div>
          <div style={{ fontSize: '16px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.GREEN }}>
            +{grid.gridProfit.toFixed(2)}
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
            {grid.totalBuys + grid.totalSells} trades
          </div>
        </div>

        {/* Position */}
        <div style={statBoxStyle}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: 4 }}>
            <Activity size={10} />
            POSITION
          </div>
          <div style={{
            fontSize: '16px',
            fontWeight: 700,
            fontFamily: 'monospace',
            color: grid.currentPosition > 0 ? FINCEPT.GREEN : grid.currentPosition < 0 ? FINCEPT.RED : FINCEPT.WHITE,
          }}>
            {grid.currentPosition > 0 ? '+' : ''}{grid.currentPosition.toFixed(4)}
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
            Avg: ${grid.averageEntryPrice.toFixed(2)}
          </div>
        </div>

        {/* Running Time */}
        <div style={statBoxStyle}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', color: FINCEPT.GRAY, marginBottom: 4 }}>
            <Clock size={10} />
            RUNNING TIME
          </div>
          <div style={{ fontSize: '16px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.WHITE }}>
            {formatDuration(runningTime)}
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>
            Started: {grid.startedAt ? new Date(grid.startedAt).toLocaleDateString() : 'N/A'}
          </div>
        </div>
      </div>

      {/* Order Status */}
      <div style={{ ...statBoxStyle, marginBottom: '10px' }}>
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, textTransform: 'uppercase', marginBottom: 8 }}>Order Status</div>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: 4, textAlign: 'center' }}>
          <div>
            <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.CYAN }}>{openOrders}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>OPEN</div>
          </div>
          <div>
            <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.GREEN }}>{filledOrders}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>FILLED</div>
          </div>
          <div>
            <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.GRAY }}>{pendingOrders}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>PENDING</div>
          </div>
          <div>
            <div style={{ fontSize: '14px', fontWeight: 700, fontFamily: 'monospace', color: FINCEPT.WHITE }}>{config.gridLevels}</div>
            <div style={{ fontSize: '8px', color: FINCEPT.MUTED }}>TOTAL</div>
          </div>
        </div>
        <div style={{ display: 'flex', justifyContent: 'center', gap: 16, marginTop: 8, fontSize: '9px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
            <TrendingUp size={10} color={FINCEPT.GREEN} />
            <span style={{ color: FINCEPT.MUTED }}>{buyOrders} buys open</span>
          </div>
          <div style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
            <TrendingDown size={10} color={FINCEPT.RED} />
            <span style={{ color: FINCEPT.MUTED }}>{sellOrders} sells open</span>
          </div>
        </div>
      </div>

      {/* Error Display */}
      {grid.lastError && (
        <div style={{
          padding: '10px 12px',
          backgroundColor: `${FINCEPT.RED}15`,
          border: `1px solid ${FINCEPT.RED}30`,
          marginBottom: '10px',
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: '10px', color: FINCEPT.RED, marginBottom: 4 }}>
            <AlertCircle size={12} />
            LAST ERROR
          </div>
          <div style={{ fontSize: '9px', color: FINCEPT.RED }}>{grid.lastError}</div>
          <div style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: 4 }}>
            Total errors: {grid.errorCount}
          </div>
        </div>
      )}

      {/* Grid Config Summary */}
      <div style={{
        padding: '10px 12px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
      }}>
        <div style={{ fontSize: '9px', color: FINCEPT.GRAY, textTransform: 'uppercase', marginBottom: 8 }}>Grid Configuration</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px 12px', fontSize: '10px' }}>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: FINCEPT.MUTED }}>Type:</span>
            <span style={{ color: FINCEPT.WHITE, textTransform: 'capitalize' }}>{config.gridType}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: FINCEPT.MUTED }}>Direction:</span>
            <span style={{ color: FINCEPT.WHITE, textTransform: 'capitalize' }}>{config.direction}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: FINCEPT.MUTED }}>Investment:</span>
            <span style={{ color: FINCEPT.WHITE }}>${config.totalInvestment}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between' }}>
            <span style={{ color: FINCEPT.MUTED }}>Qty/Grid:</span>
            <span style={{ color: FINCEPT.WHITE }}>{config.quantityPerGrid}</span>
          </div>
          {config.stopLoss && (
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.MUTED }}>Stop Loss:</span>
              <span style={{ color: FINCEPT.RED }}>${config.stopLoss}</span>
            </div>
          )}
          {config.takeProfit && (
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
              <span style={{ color: FINCEPT.MUTED }}>Take Profit:</span>
              <span style={{ color: FINCEPT.GREEN }}>${config.takeProfit}</span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}

export default GridStatusPanel;
