/**
 * Positions Panel - Bloomberg Style Real-time P&L Tracking
 */

import React from 'react';
import { UnifiedPosition, OrderSide } from '../core/types';
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

interface PositionsPanelProps {
  positions: UnifiedPosition[];
  loading: boolean;
  onRefresh: () => void;
}

const PositionsPanel: React.FC<PositionsPanelProps> = ({ positions, loading, onRefresh }) => {
  const getSideColor = (side: OrderSide) => {
    return side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.RED;
  };

  const getPnLColor = (pnl: number) => {
    return pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED;
  };

  const totalPnL = positions.reduce((sum, pos) => sum + pos.pnl, 0);
  const totalValue = positions.reduce((sum, pos) => sum + pos.value, 0);
  const totalRealized = positions.reduce((sum, pos) => sum + pos.realizedPnl, 0);
  const totalUnrealized = positions.reduce((sum, pos) => sum + pos.unrealizedPnl, 0);

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', fontFamily: 'monospace' }}>
      {/* Table */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {positions.length === 0 ? (
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
            NO POSITIONS
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
                }}>PRODUCT</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>SIDE</th>
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
                }}>VALUE</th>
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
                }}>REALIZED</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>UNREALIZED</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>P&L %</th>
              </tr>
            </thead>
            <tbody>
              {positions.map((position, idx) => {
                const pnlPercent = (position.pnl / (position.averagePrice * Math.abs(position.quantity))) * 100;

                return (
                  <tr
                    key={`${position.brokerId}-${position.id}-${idx}`}
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
                      {position.brokerId}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.WHITE,
                      letterSpacing: '0.3px'
                    }}>
                      {position.symbol}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '10px',
                      fontWeight: 600,
                      color: BLOOMBERG.GRAY,
                      textTransform: 'uppercase'
                    }}>
                      {position.product}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '10px',
                      fontWeight: 700,
                      color: getSideColor(position.side),
                      letterSpacing: '0.5px'
                    }}>
                      {position.side}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.WHITE,
                      textAlign: 'right'
                    }}>
                      {Math.abs(position.quantity)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.CYAN,
                      textAlign: 'right'
                    }}>
                      ₹{position.averagePrice.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: BLOOMBERG.YELLOW,
                      textAlign: 'right'
                    }}>
                      ₹{position.lastPrice.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: BLOOMBERG.WHITE,
                      textAlign: 'right'
                    }}>
                      ₹{position.value.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '12px',
                      fontWeight: 700,
                      color: getPnLColor(position.pnl),
                      textAlign: 'right'
                    }}>
                      {position.pnl >= 0 ? '+' : ''}₹{position.pnl.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: getPnLColor(position.realizedPnl),
                      textAlign: 'right'
                    }}>
                      {position.realizedPnl >= 0 ? '+' : ''}₹{position.realizedPnl.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 600,
                      color: getPnLColor(position.unrealizedPnl),
                      textAlign: 'right'
                    }}>
                      {position.unrealizedPnl >= 0 ? '+' : ''}₹{position.unrealizedPnl.toFixed(2)}
                    </td>
                    <td style={{
                      padding: '10px 12px',
                      fontSize: '11px',
                      fontWeight: 700,
                      color: getPnLColor(pnlPercent),
                      textAlign: 'right',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'flex-end',
                      gap: '4px'
                    }}>
                      {pnlPercent >= 0 ? (
                        <TrendingUp size={12} />
                      ) : (
                        <TrendingDown size={12} />
                      )}
                      {pnlPercent >= 0 ? '+' : ''}{pnlPercent.toFixed(2)}%
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

export default PositionsPanel;
