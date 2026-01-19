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

const FINCEPT = {
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
  // Refresh positions every 1 second for live P&L updates
  const { positions, isLoading, refresh } = usePositions(undefined, true, 1000);
  const { closePosition, isClosing } = useClosePosition();
  const capabilities = useExchangeCapabilities();
  const { activeBroker } = useBrokerContext();

  const handleClosePosition = async (position: UnifiedPosition) => {
    if (!confirm(`Close ${position.side.toUpperCase()} position for ${position.symbol}?`)) {
      return;
    }

    try {
      console.log('[PositionsTable] Closing position:', position);
      await closePosition(position);
      console.log('[PositionsTable] Position closed, refreshing...');

      // Force immediate refresh
      await refresh();

      // Additional refresh after 500ms to ensure DB sync
      setTimeout(() => refresh(), 500);

      alert(`[OK] Position closed: ${position.symbol}`);
    } catch (error) {
      console.error('[PositionsTable] Close error:', error);
      alert(`[FAIL] Failed to close position: ${(error as Error).message}`);
    }
  };

  if (isLoading && positions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        Loading positions...
      </div>
    );
  }

  if (positions.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '11px' }}>
        No open positions
      </div>
    );
  }

  return (
    <div style={{ width: '100%', overflowX: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>
              SYMBOL
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: FINCEPT.GRAY, fontWeight: 700 }}>
              SIDE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              SIZE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              ENTRY
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              CURRENT
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700 }}>
              PNL
            </th>
            {capabilities.supportsLeverage && (
              <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 700 }}>
                LEV
              </th>
            )}
            <th style={{ padding: '8px 10px', textAlign: 'center', color: FINCEPT.GRAY, fontWeight: 700 }}>
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
                  borderBottom: `1px solid ${FINCEPT.BORDER}`,
                  backgroundColor: index % 2 === 0 ? FINCEPT.DARK_BG : FINCEPT.PANEL_BG,
                }}
              >
                {/* Symbol */}
                <td style={{ padding: '10px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                  {position.symbol}
                </td>

                {/* Side */}
                <td style={{ padding: '10px' }}>
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor: position.side === 'long' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
                      color: position.side === 'long' ? FINCEPT.GREEN : FINCEPT.RED,
                      fontSize: '9px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {position.side.toUpperCase()}
                  </span>
                </td>

                {/* Size */}
                <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.WHITE }}>
                  {position.quantity.toFixed(4)}
                </td>

                {/* Entry Price */}
                <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.GRAY }}>
                  {formatCurrency(position.entryPrice)}
                </td>

                {/* Current Price */}
                <td style={{ padding: '10px', textAlign: 'right', color: FINCEPT.WHITE }}>
                  {formatCurrency(position.currentPrice)}
                </td>

                {/* PnL */}
                <td style={{ padding: '10px', textAlign: 'right', color: pnl.color, fontWeight: 700 }}>
                  {pnl.text}
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY }}>
                    ({pnl.sign}{position.pnlPercent.toFixed(2)}%)
                  </div>
                </td>

                {/* Leverage (if supported) */}
                {capabilities.supportsLeverage && (
                  <td style={{ padding: '10px', textAlign: 'center', color: FINCEPT.ORANGE, fontWeight: 700 }}>
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
                      backgroundColor: FINCEPT.RED,
                      border: 'none',
                      color: FINCEPT.WHITE,
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
