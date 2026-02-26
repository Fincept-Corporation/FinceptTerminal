import React, { useState, useEffect } from 'react';
import type { AlgoTrade } from '../types';
import { getAlgoTrades } from '../services/algoTradingService';
import { F } from '../constants/theme';
import { S } from '../constants/styles';

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
      <div className="p-4 text-[11px]" style={{ color: F.MUTED }}>
        Loading trades...
      </div>
    );
  }

  if (trades.length === 0) {
    return (
      <div className="p-4 text-[11px]" style={{ color: F.MUTED }}>
        No trades yet
      </div>
    );
  }

  return (
    <div className="overflow-auto">
      <table className="w-full border-collapse font-mono">
        <thead>
          <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            <th className={S.th} style={{ textAlign: 'left' }}>TIME</th>
            <th className={S.th} style={{ textAlign: 'left' }}>SIDE</th>
            <th className={S.th} style={{ textAlign: 'right' }}>QTY</th>
            <th className={S.th} style={{ textAlign: 'right' }}>PRICE</th>
            <th className={S.th} style={{ textAlign: 'right' }}>P&L</th>
            <th className={S.th} style={{ textAlign: 'left' }}>REASON</th>
          </tr>
        </thead>
        <tbody>
          {trades.map(trade => (
            <tr key={trade.id} style={{ borderBottom: `1px solid ${F.BORDER}15` }}>
              <td className={S.td} style={{ textAlign: 'left', color: F.MUTED }}>
                {new Date(trade.timestamp).toLocaleString()}
              </td>
              <td className={S.td + ' font-bold'} style={{
                textAlign: 'left',
                color: trade.side === 'BUY' ? F.GREEN : F.RED,
              }}>
                {trade.side}
              </td>
              <td className={S.td} style={{ textAlign: 'right', color: F.WHITE }}>
                {trade.quantity}
              </td>
              <td className={S.td} style={{ textAlign: 'right', color: F.CYAN }}>
                {trade.price.toFixed(2)}
              </td>
              <td className={S.td + ' font-bold'} style={{
                textAlign: 'right',
                color: trade.pnl >= 0 ? F.GREEN : F.RED,
              }}>
                {trade.pnl >= 0 ? '+' : ''}{trade.pnl.toFixed(2)}
              </td>
              <td
                className={S.td + ' max-w-[220px] overflow-hidden text-ellipsis whitespace-nowrap'}
                style={{ textAlign: 'left', color: F.MUTED }}
              >
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
