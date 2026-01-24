import React, { memo } from 'react';
import { Handle, Position } from 'reactflow';
import type { GraphNodeData } from '../../types';
import { NODE_COLORS } from '../../constants';
import { getSignalColor, getSignalLabel } from '../../utils/valuationSignals';

interface PeerNodeProps {
  data: GraphNodeData;
  selected?: boolean;
}

const PeerNode: React.FC<PeerNodeProps> = ({ data, selected }) => {
  const peer = data.peer;
  const valuation = data.peerValuation;
  if (!peer) return null;

  const signalColor = valuation ? getSignalColor(valuation.action) : null;
  const colors = NODE_COLORS.peer;

  const formatNumber = (num: number): string => {
    if (num >= 1e12) return `$${(num / 1e12).toFixed(1)}T`;
    if (num >= 1e9) return `$${(num / 1e9).toFixed(1)}B`;
    if (num >= 1e6) return `$${(num / 1e6).toFixed(0)}M`;
    return `$${num.toLocaleString()}`;
  };

  return (
    <div
      style={{
        background: signalColor ? signalColor.bg : colors.bg,
        border: `1.5px solid ${selected ? '#fff' : (signalColor ? signalColor.border : colors.border)}`,
        borderRadius: '6px',
        padding: '12px',
        width: '260px',
        minHeight: '140px',
        position: 'relative',
      }}
    >
      <Handle type="target" position={Position.Left} style={{ background: colors.border }} />
      <Handle type="source" position={Position.Right} style={{ background: colors.border }} />

      {/* Header */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
        <div>
          <div style={{ fontSize: '14px', fontWeight: 'bold', color: colors.text }}>{peer.ticker}</div>
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', maxWidth: '150px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
            {peer.name}
          </div>
        </div>
        {valuation && signalColor && (
          <div style={{
            background: 'rgba(0,0,0,0.3)',
            border: `1px solid ${signalColor.border}`,
            borderRadius: '3px',
            padding: '2px 6px',
          }}>
            <div style={{ fontSize: '8px', fontWeight: 'bold', color: signalColor.text }}>
              {valuation.action}
            </div>
          </div>
        )}
      </div>

      {/* Metrics */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
        <MiniMetric label="Mkt Cap" value={formatNumber(peer.market_cap)} />
        <MiniMetric label="P/E" value={peer.pe_ratio > 0 ? peer.pe_ratio.toFixed(1) : 'N/A'} />
        <MiniMetric label="P/B" value={peer.price_to_book > 0 ? peer.price_to_book.toFixed(1) : 'N/A'} />
        <MiniMetric label="Growth" value={`${(peer.revenue_growth * 100).toFixed(1)}%`} positive={peer.revenue_growth > 0} />
      </div>

      {/* Signal Status */}
      {valuation && (
        <div style={{ marginTop: '6px', fontSize: '8px', color: 'var(--ft-color-text-muted)', textAlign: 'center' }}>
          {getSignalLabel(valuation.status)} | {valuation.confidence}
        </div>
      )}
    </div>
  );
};

function MiniMetric({ label, value, positive }: { label: string; value: string; positive?: boolean }) {
  return (
    <div>
      <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)' }}>{label}</div>
      <div style={{
        fontSize: '10px', fontWeight: 'bold',
        color: positive !== undefined ? (positive ? 'var(--ft-color-success)' : 'var(--ft-color-alert)') : 'var(--ft-color-text)',
      }}>
        {value}
      </div>
    </div>
  );
}

export default memo(PeerNode);
