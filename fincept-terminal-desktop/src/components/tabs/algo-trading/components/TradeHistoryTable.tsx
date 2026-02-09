import React, { useState, useEffect } from 'react';
import type { AlgoTrade } from '../types';
import { getAlgoTrades } from '../services/algoTradingService';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A', CYAN: '#00E5FF',
};

const thStyle: React.CSSProperties = {
  padding: '6px 12px', fontSize: '8px', fontWeight: 700, color: F.GRAY,
  letterSpacing: '0.5px',
};

const tdStyle: React.CSSProperties = {
  padding: '5px 12px', fontSize: '9px',
  fontFamily: '"IBM Plex Mono", monospace',
};

interface TradeHistoryTableProps {
  deploymentId: string;
}

const TradeHistoryTable: React.FC<TradeHistoryTableProps> = ({ deploymentId }) => {
  const [trades, setTrades] = useState<AlgoTrade[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const load = async () => {
      const result = await getAlgoTrades(deploymentId, 50);
      if (result.success && result.data) {
        setTrades(result.data);
      }
      setLoading(false);
    };
    load();
  }, [deploymentId]);

  if (loading) {
    return (
      <div style={{ padding: '12px', fontSize: '9px', color: F.MUTED }}>
        Loading trades...
      </div>
    );
  }

  if (trades.length === 0) {
    return (
      <div style={{ padding: '12px', fontSize: '9px', color: F.MUTED }}>
        No trades yet
      </div>
    );
  }

  return (
    <div style={{ overflow: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
        <thead>
          <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            <th style={{ ...thStyle, textAlign: 'left' }}>TIME</th>
            <th style={{ ...thStyle, textAlign: 'left' }}>SIDE</th>
            <th style={{ ...thStyle, textAlign: 'right' }}>QTY</th>
            <th style={{ ...thStyle, textAlign: 'right' }}>PRICE</th>
            <th style={{ ...thStyle, textAlign: 'right' }}>P&L</th>
            <th style={{ ...thStyle, textAlign: 'left' }}>REASON</th>
          </tr>
        </thead>
        <tbody>
          {trades.map(trade => (
            <tr key={trade.id} style={{ borderBottom: `1px solid ${F.BORDER}15` }}>
              <td style={{ ...tdStyle, textAlign: 'left', color: F.MUTED }}>
                {new Date(trade.timestamp).toLocaleString()}
              </td>
              <td style={{
                ...tdStyle, textAlign: 'left', fontWeight: 700,
                color: trade.side === 'BUY' ? F.GREEN : F.RED,
              }}>
                {trade.side}
              </td>
              <td style={{ ...tdStyle, textAlign: 'right', color: F.WHITE }}>
                {trade.quantity}
              </td>
              <td style={{ ...tdStyle, textAlign: 'right', color: F.CYAN }}>
                {trade.price.toFixed(2)}
              </td>
              <td style={{
                ...tdStyle, textAlign: 'right', fontWeight: 700,
                color: trade.pnl >= 0 ? F.GREEN : F.RED,
              }}>
                {trade.pnl >= 0 ? '+' : ''}{trade.pnl.toFixed(2)}
              </td>
              <td style={{
                ...tdStyle, textAlign: 'left', color: F.MUTED,
                maxWidth: '200px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
              }}>
                {trade.signal_reason}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

export default TradeHistoryTable;
