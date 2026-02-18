import React, { useMemo } from 'react';
import { Table, Filter, Download, ArrowDown, ArrowUp } from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { heatColor, valColor } from './helpers';
import Sparkline from './Sparkline';
import { formatCurrency, formatPercent, formatNumber } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import type { HoldingWithQuote } from '../../../../services/portfolio/portfolioService';
import type { SortColumn, SortDirection } from '../types';

interface PositionsBlotterProps {
  holdings: HoldingWithQuote[];
  selectedSymbol: string | null;
  onSelectSymbol: (symbol: string) => void;
  sortCol: SortColumn;
  sortDir: SortDirection;
  onSort: (col: SortColumn) => void;
  currency: string;
}

const COLUMNS: { id: SortColumn | 'qty' | 'avgCost' | 'costBasis' | 'trend'; label: string; align: 'left' | 'right' | 'center'; w: string; sortable: boolean }[] = [
  { id: 'symbol', label: 'SYMBOL', align: 'left', w: '70px', sortable: true },
  { id: 'qty', label: 'QTY', align: 'right', w: '55px', sortable: false },
  { id: 'price', label: 'LAST', align: 'right', w: '80px', sortable: true },
  { id: 'avgCost', label: 'AVG COST', align: 'right', w: '80px', sortable: false },
  { id: 'market_value', label: 'MKT VAL', align: 'right', w: '90px', sortable: true },
  { id: 'costBasis', label: 'COST BASIS', align: 'right', w: '85px', sortable: false },
  { id: 'pnl', label: 'P&L', align: 'right', w: '80px', sortable: true },
  { id: 'pnlPct', label: 'P&L%', align: 'right', w: '60px', sortable: true },
  { id: 'change', label: 'CHG%', align: 'right', w: '60px', sortable: true },
  { id: 'trend', label: 'TREND', align: 'center', w: '65px', sortable: false },
  { id: 'weight', label: 'WT%', align: 'right', w: '55px', sortable: true },
];

const PositionsBlotter: React.FC<PositionsBlotterProps> = ({
  holdings, selectedSymbol, onSelectSymbol, sortCol, sortDir, onSort, currency,
}) => {
  const { t } = useTranslation('portfolio');
  const sorted = useMemo(() => {
    const arr = [...holdings];
    const mul = sortDir === 'desc' ? -1 : 1;
    arr.sort((a, b) => {
      switch (sortCol) {
        case 'symbol': return mul * a.symbol.localeCompare(b.symbol);
        case 'price': return mul * (a.current_price - b.current_price);
        case 'market_value': return mul * (a.market_value - b.market_value);
        case 'change': return mul * (a.day_change_percent - b.day_change_percent);
        case 'pnl': return mul * (a.unrealized_pnl - b.unrealized_pnl);
        case 'pnlPct': return mul * (a.unrealized_pnl_percent - b.unrealized_pnl_percent);
        case 'weight': return mul * (a.weight - b.weight);
        default: return mul * (a.weight - b.weight);
      }
    });
    return arr;
  }, [holdings, sortCol, sortDir]);

  const SortIcon: React.FC<{ col: string }> = ({ col }) => {
    if (sortCol !== col) return <ArrowDown size={7} style={{ opacity: 0.2 }} />;
    return sortDir === 'desc'
      ? <ArrowDown size={7} color={FINCEPT.ORANGE} />
      : <ArrowUp size={7} color={FINCEPT.ORANGE} />;
  };

  return (
    <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minHeight: '150px' }}>
      {/* Header */}
      <div style={{
        padding: '4px 10px', backgroundColor: '#0A0A0A', borderBottom: '1px solid #111',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Table size={10} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{t('blotter.positionsBlotter', 'POSITIONS BLOTTER')}</span>
          <span style={{ fontSize: '9px', color: FINCEPT.GRAY }}>{holdings.length} {t('blotter.instruments', 'instruments')}</span>
        </div>
      </div>

      {/* Table */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse', tableLayout: 'fixed' }}>
          <thead>
            <tr style={{ backgroundColor: '#0A0A0A', position: 'sticky', top: 0, zIndex: 2 }}>
              {COLUMNS.map(col => (
                <th
                  key={col.id}
                  onClick={() => col.sortable && onSort(col.id as SortColumn)}
                  style={{
                    padding: '5px 6px', textAlign: col.align,
                    fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY,
                    letterSpacing: '0.3px', borderBottom: `1px solid ${FINCEPT.ORANGE}`,
                    whiteSpace: 'nowrap', width: col.w,
                    cursor: col.sortable ? 'pointer' : 'default',
                  }}
                >
                  <span style={{ display: 'inline-flex', alignItems: 'center', gap: '2px' }}>
                    {col.label}
                    {col.sortable && <SortIcon col={col.id} />}
                  </span>
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {sorted.map((h, i) => {
              const isSelected = h.symbol === selectedSymbol;
              return (
                <tr
                  key={h.symbol}
                  onClick={() => onSelectSymbol(h.symbol)}
                  className="port-row"
                  style={{
                    backgroundColor: isSelected ? `${FINCEPT.ORANGE}08` : i % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.01)',
                    cursor: 'pointer',
                    borderBottom: '1px solid #0A0A0A',
                    borderLeft: isSelected ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  }}
                >
                  <td style={{ padding: '5px 6px', color: FINCEPT.CYAN, fontWeight: 700, fontSize: '10px' }}>{h.symbol}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: FINCEPT.WHITE, fontSize: '10px' }}>{formatNumber(h.quantity, 4)}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: FINCEPT.WHITE, fontWeight: 600, fontSize: '10px' }}>{formatCurrency(h.current_price, currency)}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: FINCEPT.GRAY, fontSize: '10px' }}>{formatCurrency(h.avg_buy_price, currency)}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: FINCEPT.YELLOW, fontWeight: 600, fontSize: '10px' }}>{formatCurrency(h.market_value, currency)}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: FINCEPT.GRAY, fontSize: '10px' }}>{formatCurrency(h.cost_basis, currency)}</td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', color: valColor(h.unrealized_pnl), fontWeight: 700, fontSize: '10px', backgroundColor: heatColor(h.unrealized_pnl_percent) }}>
                    {h.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(h.unrealized_pnl, currency)}
                  </td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', fontSize: '10px' }}>
                    <span style={{
                      padding: '1px 4px', fontWeight: 600,
                      backgroundColor: heatColor(h.unrealized_pnl_percent),
                      color: valColor(h.unrealized_pnl_percent),
                    }}>
                      {formatPercent(h.unrealized_pnl_percent)}
                    </span>
                  </td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', fontSize: '10px' }}>
                    <span style={{
                      padding: '1px 4px', fontWeight: 600,
                      backgroundColor: heatColor(h.day_change_percent, 3),
                      color: valColor(h.day_change_percent),
                    }}>
                      {h.day_change_percent >= 0 ? '+' : ''}{h.day_change_percent.toFixed(2)}%
                    </span>
                  </td>
                  <td style={{ padding: '5px 4px', textAlign: 'center' }}>
                    <Sparkline
                      data={[h.avg_buy_price, (h.avg_buy_price + h.current_price) / 2, h.current_price]}
                      color={h.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED}
                      width={50}
                      height={14}
                    />
                  </td>
                  <td style={{ padding: '5px 6px', textAlign: 'right', fontSize: '10px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '3px' }}>
                      <div style={{ width: '24px', height: '3px', backgroundColor: '#111' }}>
                        <div style={{ height: '100%', width: `${Math.min(h.weight * 4, 100)}%`, backgroundColor: FINCEPT.ORANGE }} />
                      </div>
                      <span style={{ color: FINCEPT.ORANGE, fontWeight: 600, fontSize: '9px' }}>{h.weight.toFixed(1)}</span>
                    </div>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default PositionsBlotter;
