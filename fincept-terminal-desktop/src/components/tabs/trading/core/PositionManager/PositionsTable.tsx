/**
 * Positions Table
 * Universal component - works with ALL exchanges
 */

import React from 'react';
import { usePositions, useClosePosition } from '../../hooks/usePositions';
import { useExchangeCapabilities } from '../../hooks/useExchangeCapabilities';
import { useBrokerContext } from '../../../../../contexts/BrokerContext';
import { formatPrice, formatCurrency, formatPnL, formatLeverage } from '../../utils/formatters';
import type { UnifiedPosition } from '../../types';

const BLOOMBERG = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
};

export function PositionsTable() {
  const { positions, isLoading, refresh } = usePositions(undefined, true, 2000);
  const { closePosition, isClosing } = useClosePosition();
  const capabilities = useExchangeCapabilities();
  const { activeBroker } = useBrokerContext();

  const handleClosePosition = async (position: UnifiedPosition) => {
    if (!confirm(`Close ${position.side.toUpperCase()} position for ${position.symbol}?`)) {
      return;
    }

    try {
      await closePosition(position);
      await refresh();
      alert(`✓ Position closed: ${position.symbol}`);
    } catch (error) {
      alert(`✗ Failed to close position: ${(error as Error).message}`);
    }
  };

  if (isLoading && positions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
        Loading positions...
      </div>
    );
  }

  if (positions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
        No open positions
      </div>
    );
  }

  return (
    <div style={{ width: '100%', overflowX: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        <thead>
          <tr style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SYMBOL
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SIDE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SIZE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              ENTRY
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              CURRENT
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              PNL
            </th>
            {capabilities.supportsLeverage && (
              <th style={{ padding: '8px 10px', textAlign: 'center', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
                LEV
              </th>
            )}
            <th style={{ padding: '8px 10px', textAlign: 'center', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              ACTIONS
            </th>
          </tr>
        </thead>
        <tbody>
          {positions.map((position, index) => {
            const pnl = formatPnL(position.unrealizedPnl);

            return (
              <tr
                key={position.positionId || index}
                style={{
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                  backgroundColor: index % 2 === 0 ? BLOOMBERG.DARK_BG : BLOOMBERG.PANEL_BG,
                }}
              >
                {/* Symbol */}
                <td style={{ padding: '10px', color: BLOOMBERG.WHITE, fontWeight: 600 }}>
                  {position.symbol}
                </td>

                {/* Side */}
                <td style={{ padding: '10px' }}>
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor: position.side === 'long' ? `${BLOOMBERG.GREEN}20` : `${BLOOMBERG.RED}20`,
                      color: position.side === 'long' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                      fontSize: '9px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {position.side.toUpperCase()}
                  </span>
                </td>

                {/* Size */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                  {position.quantity.toFixed(4)}
                </td>

                {/* Entry Price */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.GRAY }}>
                  {formatCurrency(position.entryPrice)}
                </td>

                {/* Current Price */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                  {formatCurrency(position.currentPrice)}
                </td>

                {/* PnL */}
                <td style={{ padding: '10px', textAlign: 'right', color: pnl.color, fontWeight: 700 }}>
                  {pnl.text}
                  <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY }}>
                    ({pnl.sign}{position.pnlPercent.toFixed(2)}%)
                  </div>
                </td>

                {/* Leverage (if supported) */}
                {capabilities.supportsLeverage && (
                  <td style={{ padding: '10px', textAlign: 'center', color: BLOOMBERG.ORANGE, fontWeight: 700 }}>
                    {formatLeverage(position.leverage || 1)}
                  </td>
                )}

                {/* Actions */}
                <td style={{ padding: '10px', textAlign: 'center' }}>
                  <button
                    onClick={() => handleClosePosition(position)}
                    disabled={isClosing}
                    style={{
                      padding: '4px 10px',
                      backgroundColor: BLOOMBERG.RED,
                      border: 'none',
                      color: BLOOMBERG.WHITE,
                      fontSize: '9px',
                      fontWeight: 700,
                      cursor: isClosing ? 'not-allowed' : 'pointer',
                      opacity: isClosing ? 0.5 : 1,
                    }}
                    onMouseEnter={(e) => {
                      if (!isClosing) e.currentTarget.style.opacity = '0.8';
                    }}
                    onMouseLeave={(e) => {
                      if (!isClosing) e.currentTarget.style.opacity = '1';
                    }}
                  >
                    CLOSE
                  </button>
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
