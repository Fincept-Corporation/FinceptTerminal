/**
 * TradeListView - Scrollable, sortable trade table
 */

import React, { useState, useMemo } from 'react';
import { F, cellStyle, headerCellStyle, ExtendedTrade, formatPercent } from './backtestingStyles';
import { ArrowUp, ArrowDown } from 'lucide-react';

interface TradeListViewProps {
  trades: ExtendedTrade[];
}

type SortKey = 'entryDate' | 'pnl' | 'pnlPercent' | 'holdingPeriod';

const TradeListView: React.FC<TradeListViewProps> = ({ trades }) => {
  const [sortKey, setSortKey] = useState<SortKey>('entryDate');
  const [sortAsc, setSortAsc] = useState(false);

  const sortedTrades = useMemo(() => {
    const sorted = [...trades].sort((a, b) => {
      const va = a[sortKey] ?? 0;
      const vb = b[sortKey] ?? 0;
      if (typeof va === 'string' && typeof vb === 'string') return va.localeCompare(vb);
      return (va as number) - (vb as number);
    });
    return sortAsc ? sorted : sorted.reverse();
  }, [trades, sortKey, sortAsc]);

  const toggleSort = (key: SortKey) => {
    if (sortKey === key) setSortAsc(!sortAsc);
    else { setSortKey(key); setSortAsc(true); }
  };

  if (!trades || trades.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No trades executed
      </div>
    );
  }

  const SortIcon = ({ col }: { col: SortKey }) => {
    if (sortKey !== col) return null;
    return sortAsc
      ? <ArrowUp size={8} color={F.ORANGE} />
      : <ArrowDown size={8} color={F.ORANGE} />;
  };

  return (
    <div style={{ overflowY: 'auto', overflowX: 'auto', height: '100%' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', minWidth: '500px' }}>
        <thead>
          <tr style={{ position: 'sticky', top: 0, backgroundColor: F.HEADER_BG, zIndex: 1 }}>
            <th style={{ ...headerCellStyle, cursor: 'pointer' }} onClick={() => toggleSort('entryDate')}>
              DATE <SortIcon col="entryDate" />
            </th>
            <th style={headerCellStyle}>SIDE</th>
            <th style={headerCellStyle}>ENTRY</th>
            <th style={headerCellStyle}>EXIT</th>
            <th style={{ ...headerCellStyle, cursor: 'pointer' }} onClick={() => toggleSort('pnl')}>
              P&L <SortIcon col="pnl" />
            </th>
            <th style={{ ...headerCellStyle, cursor: 'pointer' }} onClick={() => toggleSort('pnlPercent')}>
              P&L% <SortIcon col="pnlPercent" />
            </th>
            <th style={{ ...headerCellStyle, cursor: 'pointer' }} onClick={() => toggleSort('holdingPeriod')}>
              DAYS <SortIcon col="holdingPeriod" />
            </th>
            <th style={headerCellStyle}>REASON</th>
          </tr>
        </thead>
        <tbody>
          {sortedTrades.map((trade, i) => (
            <tr key={trade.id || i} style={{ backgroundColor: i % 2 === 0 ? 'transparent' : F.HEADER_BG }}>
              <td style={{ ...cellStyle, color: F.WHITE }}>
                {trade.entryDate?.split('T')[0]?.split(' ')[0] || '-'}
              </td>
              <td style={{ ...cellStyle, color: trade.side === 'long' ? F.GREEN : F.RED, fontWeight: 700, textTransform: 'uppercase' }}>
                {trade.side}
              </td>
              <td style={{ ...cellStyle, color: F.WHITE }}>
                ${trade.entryPrice?.toFixed(2) || '-'}
              </td>
              <td style={{ ...cellStyle, color: F.WHITE }}>
                ${trade.exitPrice?.toFixed(2) || '-'}
              </td>
              <td style={{ ...cellStyle, color: (trade.pnl ?? 0) >= 0 ? F.GREEN : F.RED, fontWeight: 700 }}>
                {trade.pnl != null ? `$${trade.pnl.toFixed(0)}` : '-'}
              </td>
              <td style={{ ...cellStyle, color: (trade.pnlPercent ?? 0) >= 0 ? F.GREEN : F.RED }}>
                {trade.pnlPercent != null ? `${(trade.pnlPercent * 100).toFixed(2)}%` : '-'}
              </td>
              <td style={{ ...cellStyle, color: F.GRAY }}>
                {trade.holdingPeriod ?? '-'}
              </td>
              <td style={{ ...cellStyle, color: F.MUTED, fontSize: '9px' }}>
                {trade.exitReason || '-'}
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
};

export default TradeListView;
