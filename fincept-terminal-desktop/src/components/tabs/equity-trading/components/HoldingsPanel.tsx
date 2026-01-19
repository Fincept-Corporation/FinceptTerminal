/**
 * Holdings Panel Component - Fincept Style
 *
 * Displays portfolio holdings (long-term investments) with proper scrolling
 */

import React, { useState } from 'react';
import { RefreshCw, TrendingUp, TrendingDown, Search, SortAsc, SortDesc, PieChart } from 'lucide-react';
import { useStockTradingData } from '@/contexts/StockBrokerContext';
import type { Holding } from '@/brokers/stocks/types';

// Fincept color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

type SortField = 'symbol' | 'quantity' | 'investedValue' | 'currentValue' | 'pnl' | 'pnlPercent';
type SortDirection = 'asc' | 'desc';

interface HoldingsPanelProps {
  onSell?: (holding: Holding) => void;
}

export function HoldingsPanel({ onSell }: HoldingsPanelProps) {
  const { holdings, refreshHoldings, isRefreshing, isReady } = useStockTradingData();

  const [searchQuery, setSearchQuery] = useState('');
  const [sortField, setSortField] = useState<SortField>('currentValue');
  const [sortDirection, setSortDirection] = useState<SortDirection>('desc');

  // Calculate totals
  const totals = React.useMemo(() => {
    return holdings.reduce(
      (acc, h) => ({
        investedValue: acc.investedValue + h.investedValue,
        currentValue: acc.currentValue + h.currentValue,
        totalPnl: acc.totalPnl + h.pnl,
      }),
      { investedValue: 0, currentValue: 0, totalPnl: 0 }
    );
  }, [holdings]);

  const totalPnlPercent = totals.investedValue > 0
    ? ((totals.currentValue - totals.investedValue) / totals.investedValue) * 100
    : 0;

  // Filter and sort holdings
  const filteredHoldings = React.useMemo(() => {
    let result = [...holdings];

    // Filter by search
    if (searchQuery) {
      const query = searchQuery.toLowerCase();
      result = result.filter(
        (h) =>
          h.symbol.toLowerCase().includes(query) ||
          (h.isin && h.isin.toLowerCase().includes(query))
      );
    }

    // Sort
    result.sort((a, b) => {
      let aVal = a[sortField];
      let bVal = b[sortField];

      if (typeof aVal === 'string') {
        return sortDirection === 'asc'
          ? aVal.localeCompare(bVal as string)
          : (bVal as string).localeCompare(aVal);
      }

      return sortDirection === 'asc'
        ? (aVal as number) - (bVal as number)
        : (bVal as number) - (aVal as number);
    });

    return result;
  }, [holdings, searchQuery, sortField, sortDirection]);

  // Handle sort click
  const handleSort = (field: SortField) => {
    if (sortField === field) {
      setSortDirection(sortDirection === 'asc' ? 'desc' : 'asc');
    } else {
      setSortField(field);
      setSortDirection('desc');
    }
  };

  // Sort indicator component
  const SortIndicator = ({ field }: { field: SortField }) => {
    if (sortField !== field) return null;
    return sortDirection === 'asc' ? (
      <SortAsc size={10} color={COLORS.ORANGE} />
    ) : (
      <SortDesc size={10} color={COLORS.ORANGE} />
    );
  };

  if (!isReady) {
    return (
      <div style={{
        height: '100%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: COLORS.GRAY,
        fontSize: '11px'
      }}>
        Connect to your broker to view holdings
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden',
      backgroundColor: COLORS.PANEL_BG,
      fontFamily: '"IBM Plex Mono", "Consolas", monospace'
    }}>
      {/* Header */}
      <div style={{
        padding: '6px 12px',
        borderBottom: `1px solid ${COLORS.BORDER}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        backgroundColor: COLORS.HEADER_BG,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: '11px', fontWeight: 600, color: COLORS.WHITE }}>HOLDINGS</span>
          <span style={{ fontSize: '10px', color: COLORS.GRAY }}>({holdings.length})</span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Search */}
          <div style={{ position: 'relative' }}>
            <Search size={12} color={COLORS.GRAY} style={{ position: 'absolute', left: '8px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search..."
              style={{
                width: '120px',
                padding: '4px 8px 4px 26px',
                backgroundColor: COLORS.DARK_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '10px',
                outline: 'none'
              }}
            />
          </div>

          {/* Refresh */}
          <button
            onClick={() => refreshHoldings()}
            disabled={isRefreshing}
            style={{
              padding: '4px',
              backgroundColor: 'transparent',
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.GRAY,
              cursor: 'pointer'
            }}
          >
            <RefreshCw size={12} className={isRefreshing ? 'animate-spin' : ''} />
          </button>
        </div>
      </div>

      {/* Portfolio Summary */}
      {holdings.length > 0 && (
        <div style={{
          padding: '8px 12px',
          backgroundColor: COLORS.DARK_BG,
          borderBottom: `1px solid ${COLORS.BORDER}`,
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: '12px',
          flexShrink: 0
        }}>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>INVESTED</div>
            <div style={{ fontSize: '11px', fontFamily: 'monospace', color: COLORS.WHITE }}>
              ₹{totals.investedValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>CURRENT</div>
            <div style={{ fontSize: '11px', fontFamily: 'monospace', color: COLORS.CYAN }}>
              ₹{totals.currentValue.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>P&L</div>
            <div style={{
              fontSize: '11px',
              fontFamily: 'monospace',
              color: totals.totalPnl >= 0 ? COLORS.GREEN : COLORS.RED
            }}>
              {totals.totalPnl >= 0 ? '+' : ''}₹{totals.totalPnl.toLocaleString('en-IN', { minimumFractionDigits: 2 })}
            </div>
          </div>
          <div>
            <div style={{ fontSize: '9px', color: COLORS.GRAY, marginBottom: '2px' }}>RETURNS</div>
            <div style={{
              fontSize: '11px',
              fontFamily: 'monospace',
              color: totalPnlPercent >= 0 ? COLORS.GREEN : COLORS.RED
            }}>
              {totalPnlPercent >= 0 ? '+' : ''}{totalPnlPercent.toFixed(2)}%
            </div>
          </div>
        </div>
      )}

      {/* Holdings Table */}
      <div style={{ flex: 1, overflow: 'auto', minHeight: 0 }}>
        {filteredHoldings.length === 0 ? (
          <div style={{
            padding: '24px',
            textAlign: 'center',
            color: COLORS.GRAY
          }}>
            <PieChart size={32} color={COLORS.MUTED} style={{ margin: '0 auto 12px' }} />
            <p style={{ fontSize: '11px' }}>
              {searchQuery ? 'No holdings match your search' : 'No holdings in your portfolio'}
            </p>
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: COLORS.HEADER_BG, zIndex: 1 }}>
              <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                <th
                  style={{ padding: '6px 8px', textAlign: 'left', color: COLORS.GRAY, fontWeight: 600, cursor: 'pointer' }}
                  onClick={() => handleSort('symbol')}
                >
                  <span style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    SYMBOL <SortIndicator field="symbol" />
                  </span>
                </th>
                <th
                  style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600, cursor: 'pointer' }}
                  onClick={() => handleSort('quantity')}
                >
                  <span style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                    QTY <SortIndicator field="quantity" />
                  </span>
                </th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>AVG</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>LTP</th>
                <th
                  style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600, cursor: 'pointer' }}
                  onClick={() => handleSort('currentValue')}
                >
                  <span style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                    VALUE <SortIndicator field="currentValue" />
                  </span>
                </th>
                <th
                  style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600, cursor: 'pointer' }}
                  onClick={() => handleSort('pnl')}
                >
                  <span style={{ display: 'flex', alignItems: 'center', justifyContent: 'flex-end', gap: '4px' }}>
                    P&L <SortIndicator field="pnl" />
                  </span>
                </th>
                <th style={{ padding: '6px 8px', textAlign: 'center', color: COLORS.GRAY, fontWeight: 600 }}>ACT</th>
              </tr>
            </thead>
            <tbody>
              {filteredHoldings.map((holding, idx) => {
                const isProfit = holding.pnl >= 0;

                return (
                  <tr
                    key={`${holding.symbol}-${holding.exchange}`}
                    style={{
                      borderBottom: `1px solid ${COLORS.BORDER}40`,
                      backgroundColor: idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`
                    }}
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.HEADER_BG}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`}
                  >
                    <td style={{ padding: '8px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        <div style={{
                          width: '18px',
                          height: '18px',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          backgroundColor: isProfit ? `${COLORS.GREEN}20` : `${COLORS.RED}20`,
                          flexShrink: 0
                        }}>
                          {isProfit ? (
                            <TrendingUp size={10} color={COLORS.GREEN} />
                          ) : (
                            <TrendingDown size={10} color={COLORS.RED} />
                          )}
                        </div>
                        <div>
                          <span style={{ fontSize: '10px', color: COLORS.WHITE, fontWeight: 600 }}>{holding.symbol}</span>
                          <span style={{ fontSize: '9px', color: COLORS.GRAY, marginLeft: '4px' }}>{holding.exchange}</span>
                        </div>
                      </div>
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right' }}>
                      <span style={{ fontFamily: 'monospace', color: COLORS.WHITE }}>{holding.quantity}</span>
                      {holding.t1Quantity && holding.t1Quantity > 0 && (
                        <div style={{ fontSize: '9px', color: COLORS.ORANGE }}>+{holding.t1Quantity} T1</div>
                      )}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.WHITE }}>
                      {holding.averagePrice.toFixed(2)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.YELLOW }}>
                      {holding.lastPrice.toFixed(2)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.WHITE }}>
                      ₹{holding.currentValue.toLocaleString('en-IN', { minimumFractionDigits: 0, maximumFractionDigits: 0 })}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right' }}>
                      <div style={{
                        fontFamily: 'monospace',
                        color: isProfit ? COLORS.GREEN : COLORS.RED
                      }}>
                        {isProfit ? '+' : ''}{holding.pnl.toFixed(0)}
                      </div>
                      <div style={{
                        fontSize: '9px',
                        color: isProfit ? COLORS.GREEN : COLORS.RED
                      }}>
                        {isProfit ? '+' : ''}{holding.pnlPercent.toFixed(2)}%
                      </div>
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center' }}>
                      <button
                        onClick={() => onSell?.(holding)}
                        style={{
                          padding: '3px 8px',
                          fontSize: '9px',
                          fontWeight: 600,
                          backgroundColor: `${COLORS.RED}20`,
                          color: COLORS.RED,
                          border: `1px solid ${COLORS.RED}40`,
                          cursor: 'pointer'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = `${COLORS.RED}40`;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = `${COLORS.RED}20`;
                        }}
                      >
                        SELL
                      </button>
                    </td>
                  </tr>
                );
              })}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
