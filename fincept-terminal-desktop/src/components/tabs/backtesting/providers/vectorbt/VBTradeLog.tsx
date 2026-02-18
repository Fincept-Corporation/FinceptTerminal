/**
 * VBTradeLog - Sortable trade table with 8 columns
 */

import React, { useState, useMemo } from 'react';
import { ArrowUp, ArrowDown } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, BORDERS, getValueColor } from '../../../portfolio-tab/finceptStyles';

interface VBTradeLogProps {
  trades: any[] | undefined;
}

type SortKey = '#' | 'side' | 'symbol' | 'entry' | 'exit' | 'pnl' | 'pnlPct' | 'duration';
type SortDir = 'asc' | 'desc';

const COLUMNS: { key: SortKey; label: string; width: string }[] = [
  { key: '#', label: '#', width: '30px' },
  { key: 'side', label: 'SIDE', width: '50px' },
  { key: 'symbol', label: 'SYM', width: '55px' },
  { key: 'entry', label: 'ENTRY $', width: '70px' },
  { key: 'exit', label: 'EXIT $', width: '70px' },
  { key: 'pnl', label: 'P&L', width: '70px' },
  { key: 'pnlPct', label: 'P&L %', width: '60px' },
  { key: 'duration', label: 'DUR', width: '45px' },
];

const MAX_VISIBLE = 100;

function getTrade(trade: any, field: string): any {
  // Normalize field names across different result formats
  const map: Record<string, string[]> = {
    side: ['side', 'direction', 'type'],
    symbol: ['symbol', 'ticker', 'asset'],
    entryPrice: ['entryPrice', 'entry_price', 'openPrice', 'open_price'],
    exitPrice: ['exitPrice', 'exit_price', 'closePrice', 'close_price'],
    pnl: ['pnl', 'profit', 'gain', 'return_amount'],
    pnlPct: ['pnlPct', 'pnl_pct', 'return_pct', 'returnPct', 'profitPct'],
    duration: ['duration', 'holdingPeriod', 'holding_period', 'bars'],
  };
  const keys = map[field] || [field];
  for (const k of keys) {
    if (trade[k] !== undefined) return trade[k];
  }
  return undefined;
}

export const VBTradeLog: React.FC<VBTradeLogProps> = ({ trades }) => {
  const [sortKey, setSortKey] = useState<SortKey>('#');
  const [sortDir, setSortDir] = useState<SortDir>('asc');

  const normalizedTrades = useMemo(() => {
    if (!trades || !Array.isArray(trades)) return [];
    return trades.map((t, i) => ({
      idx: i + 1,
      side: getTrade(t, 'side') || 'LONG',
      symbol: getTrade(t, 'symbol') || '--',
      entryPrice: getTrade(t, 'entryPrice'),
      exitPrice: getTrade(t, 'exitPrice'),
      pnl: getTrade(t, 'pnl'),
      pnlPct: getTrade(t, 'pnlPct'),
      duration: getTrade(t, 'duration'),
      raw: t,
    }));
  }, [trades]);

  const sortedTrades = useMemo(() => {
    const arr = [...normalizedTrades];
    const mult = sortDir === 'asc' ? 1 : -1;

    arr.sort((a, b) => {
      let va: any, vb: any;
      switch (sortKey) {
        case '#': va = a.idx; vb = b.idx; break;
        case 'side': va = a.side; vb = b.side; break;
        case 'symbol': va = a.symbol; vb = b.symbol; break;
        case 'entry': va = a.entryPrice ?? 0; vb = b.entryPrice ?? 0; break;
        case 'exit': va = a.exitPrice ?? 0; vb = b.exitPrice ?? 0; break;
        case 'pnl': va = a.pnl ?? 0; vb = b.pnl ?? 0; break;
        case 'pnlPct': va = a.pnlPct ?? 0; vb = b.pnlPct ?? 0; break;
        case 'duration': va = a.duration ?? 0; vb = b.duration ?? 0; break;
        default: va = a.idx; vb = b.idx;
      }
      if (typeof va === 'string') return va.localeCompare(vb) * mult;
      return (va - vb) * mult;
    });

    return arr.slice(0, MAX_VISIBLE);
  }, [normalizedTrades, sortKey, sortDir]);

  const handleSort = (key: SortKey) => {
    if (sortKey === key) {
      setSortDir(d => d === 'asc' ? 'desc' : 'asc');
    } else {
      setSortKey(key);
      setSortDir('asc');
    }
  };

  if (!trades || trades.length === 0) {
    return (
      <div style={{ padding: '24px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '10px' }}>
        No trades to display
      </div>
    );
  }

  return (
    <div style={{ height: '100%', overflow: 'auto' }}>
      {/* Counter */}
      <div style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.GRAY }}>
        Showing {sortedTrades.length} of {normalizedTrades.length} trades
      </div>

      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        {/* Header */}
        <thead>
          <tr>
            {COLUMNS.map(col => (
              <th
                key={col.key}
                onClick={() => handleSort(col.key)}
                style={{
                  width: col.width,
                  padding: '6px 8px',
                  backgroundColor: FINCEPT.HEADER_BG,
                  color: FINCEPT.ORANGE,
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.3px',
                  textAlign: 'left',
                  cursor: 'pointer',
                  borderBottom: `1px solid ${FINCEPT.ORANGE}`,
                  position: 'sticky' as const,
                  top: 0,
                  zIndex: 1,
                  userSelect: 'none',
                  whiteSpace: 'nowrap',
                }}
              >
                {col.label}
                {sortKey === col.key && (
                  sortDir === 'asc'
                    ? <ArrowUp size={7} style={{ marginLeft: '2px', verticalAlign: 'middle' }} />
                    : <ArrowDown size={7} style={{ marginLeft: '2px', verticalAlign: 'middle' }} />
                )}
              </th>
            ))}
          </tr>
        </thead>

        {/* Body */}
        <tbody>
          {sortedTrades.map((t, i) => {
            const isWin = (t.pnl ?? 0) >= 0;
            return (
              <tr
                key={t.idx}
                className="vb-row"
                style={{
                  backgroundColor: i % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                  borderLeft: `2px solid ${isWin ? '#00D66F' : '#FF3B3B'}`,
                }}
              >
                <td style={{ padding: '5px 8px', color: FINCEPT.GRAY }}>{t.idx}</td>
                <td style={{
                  padding: '5px 8px',
                  color: t.side?.toUpperCase() === 'SHORT' ? '#FF3B3B' : '#00D66F',
                  fontWeight: 700,
                }}>
                  {(t.side || 'LONG').toUpperCase()}
                </td>
                <td style={{ padding: '5px 8px', color: FINCEPT.YELLOW, fontWeight: 600 }}>
                  {t.symbol}
                </td>
                <td style={{ padding: '5px 8px', color: FINCEPT.WHITE, fontFamily: TYPOGRAPHY.MONO }}>
                  {t.entryPrice !== undefined ? `$${Number(t.entryPrice).toFixed(2)}` : '--'}
                </td>
                <td style={{ padding: '5px 8px', color: FINCEPT.WHITE, fontFamily: TYPOGRAPHY.MONO }}>
                  {t.exitPrice !== undefined ? `$${Number(t.exitPrice).toFixed(2)}` : '--'}
                </td>
                <td style={{
                  padding: '5px 8px',
                  color: getValueColor(t.pnl ?? 0),
                  fontWeight: 700,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {t.pnl !== undefined ? `${t.pnl >= 0 ? '+' : ''}$${Number(t.pnl).toFixed(2)}` : '--'}
                </td>
                <td style={{
                  padding: '5px 8px',
                  color: getValueColor(t.pnlPct ?? 0),
                  fontWeight: 700,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {t.pnlPct !== undefined ? `${t.pnlPct >= 0 ? '+' : ''}${(Number(t.pnlPct) * 100).toFixed(2)}%` : '--'}
                </td>
                <td style={{ padding: '5px 8px', color: FINCEPT.GRAY }}>
                  {t.duration !== undefined ? `${t.duration}` : '--'}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};
