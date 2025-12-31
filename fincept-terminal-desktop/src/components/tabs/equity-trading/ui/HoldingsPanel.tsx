/**
 * Holdings Panel - Bloomberg Style Investment Portfolio View
 */

import React from 'react';
import { UnifiedHolding } from '../core/types';
import { TrendingUp, TrendingDown } from 'lucide-react';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface HoldingsPanelProps {
  holdings: UnifiedHolding[];
  loading: boolean;
  onRefresh: () => void;
}

const HoldingsPanel: React.FC<HoldingsPanelProps> = ({ holdings, loading, onRefresh }) => {
  const getPnLColor = (pnl: number) => {
    return pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED;
  };

  const totalInvested = holdings.reduce((sum, h) => sum + h.investedValue, 0);
  const totalCurrent = holdings.reduce((sum, h) => sum + h.currentValue, 0);
  const totalPnL = holdings.reduce((sum, h) => sum + h.pnl, 0);
  const totalPnLPercent = totalInvested > 0 ? (totalPnL / totalInvested) * 100 : 0;

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', fontFamily: 'monospace' }}>
      {/* Table */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {holdings.length === 0 ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: BLOOMBERG.GRAY,
            fontSize: '11px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            NO HOLDINGS
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: BLOOMBERG.HEADER_BG, zIndex: 1 }}>
              <tr style={{ borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>BROKER</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>SYMBOL</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>ISIN</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>QTY</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>AVG PRICE</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>LTP</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>INVESTED</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>CURRENT</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>TOTAL P&L</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>P&L %</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>DAY CHANGE</th>
              </tr>
            </thead>
            <tbody>
              {holdings.map((holding, idx) => {
                const dayChange = holding.lastPrice - (holding.currentValue / holding.quantity);
                const dayChangePercent = ((dayChange / (holding.currentValue / holding.quantity)) * 100) || 0;

                return (
                  <tr
                    key={`${holding.brokerId}-${holding.id}-${idx}`}
                    style={{
                      borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                      backgroundColor: 'transparent',
                      transition: 'background-color 0.15s ease'
                    }}
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = 'transparent'}
                  >
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '10px',
                      fontWeight: 700,
                      color: BLOOMBERG.ORANGE,
                      textTransform: 'uppercase',
                      letterSpacing: '0.3px'
                    }}>
                      {holding.brokerId}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.WHITE,
                      letterSpacing: '0.3px'
                    }}>
                      {holding.symbol}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '9px',
                      fontWeight: 600,
                      color: BLOOMBERG.MUTED,
                      letterSpacing: '0.3px'
                    }}>
                      {holding.isin || '-'}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.WHITE,
                      textAlign: 'right'
                    }}>
                      {holding.quantity}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.CYAN,
                      textAlign: 'right'
                    }}>
                      ₹{holding.averagePrice.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.YELLOW,
                      textAlign: 'right'
                    }}>
                      ₹{holding.lastPrice.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.GRAY,
                      textAlign: 'right'
                    }}>
                      ₹{holding.investedValue.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.WHITE,
                      textAlign: 'right'
                    }}>
                      ₹{holding.currentValue.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '12px',
                      fontWeight: 700,
                      color: getPnLColor(holding.pnl),
                      textAlign: 'right'
                    }}>
                      {holding.pnl >= 0 ? '+' : ''}₹{holding.pnl.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: getPnLColor(holding.pnlPercentage),
                      textAlign: 'right',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'flex-end',
                      gap: '4px'
                    }}>
                      {holding.pnlPercentage >= 0 ? (
                        <TrendingUp size={12} />
                      ) : (
                        <TrendingDown size={12} />
                      )}
                      {holding.pnlPercentage >= 0 ? '+' : ''}{holding.pnlPercentage.toFixed(2)}%
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '10px',
                      fontWeight: 600,
                      color: getPnLColor(dayChange),
                      textAlign: 'right'
                    }}>
                      {dayChange >= 0 ? '+' : ''}₹{dayChange.toFixed(2)} ({dayChangePercent >= 0 ? '+' : ''}{dayChangePercent.toFixed(2)}%)
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
};

export default HoldingsPanel;
