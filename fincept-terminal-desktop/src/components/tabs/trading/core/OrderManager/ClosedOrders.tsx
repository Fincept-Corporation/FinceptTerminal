/**
 * Closed Orders History
 * Universal component - works with ALL exchanges (if supported)
 */

import React from 'react';
import { useClosedOrders } from '../../hooks/useOrders';
import { formatCurrency, formatDateTime, formatOrderStatus } from '../../utils/formatters';

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
};

export function ClosedOrders() {
  const { closedOrders, isLoading, error } = useClosedOrders(undefined, 100);

  if (error) {
    return (
      <div
        style={{
          padding: '20px',
          textAlign: 'center',
          color: BLOOMBERG.GRAY,
          fontSize: '11px',
        }}
      >
        This exchange does not support closed orders history
      </div>
    );
  }

  if (isLoading) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
        Loading order history...
      </div>
    );
  }

  if (closedOrders.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
        No closed orders
      </div>
    );
  }

  return (
    <div style={{ width: '100%', height: '100%', overflowX: 'auto', overflowY: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
        <thead style={{ position: 'sticky', top: 0, zIndex: 1 }}>
          <tr style={{ backgroundColor: BLOOMBERG.HEADER_BG, borderBottom: `1px solid ${BLOOMBERG.BORDER}` }}>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SYMBOL
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              TYPE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SIDE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              SIZE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              PRICE
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              AVG FILL
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'right', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              COST
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              STATUS
            </th>
            <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
              TIME
            </th>
          </tr>
        </thead>
        <tbody>
          {closedOrders.map((order, index) => {
            const status = formatOrderStatus(order.status);

            return (
              <tr
                key={order.id}
                style={{
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
                  backgroundColor: index % 2 === 0 ? BLOOMBERG.DARK_BG : BLOOMBERG.PANEL_BG,
                }}
              >
                {/* Symbol */}
                <td style={{ padding: '10px', color: BLOOMBERG.WHITE, fontWeight: 600 }}>
                  {order.symbol}
                </td>

                {/* Type */}
                <td style={{ padding: '10px', color: BLOOMBERG.GRAY, textTransform: 'uppercase', fontSize: '9px' }}>
                  {order.type}
                </td>

                {/* Side */}
                <td style={{ padding: '10px' }}>
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor: order.side === 'buy' ? `${BLOOMBERG.GREEN}20` : `${BLOOMBERG.RED}20`,
                      color: order.side === 'buy' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                      fontSize: '9px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {order.side.toUpperCase()}
                  </span>
                </td>

                {/* Size */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                  {order.quantity.toFixed(4)}
                </td>

                {/* Price */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.GRAY }}>
                  {order.price ? formatCurrency(order.price) : 'MARKET'}
                </td>

                {/* Average Fill */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.WHITE }}>
                  {order.average ? formatCurrency(order.average) : '--'}
                </td>

                {/* Cost */}
                <td style={{ padding: '10px', textAlign: 'right', color: BLOOMBERG.ORANGE }}>
                  {order.cost ? formatCurrency(order.cost) : '--'}
                </td>

                {/* Status */}
                <td style={{ padding: '10px' }}>
                  <span
                    style={{
                      padding: '2px 6px',
                      backgroundColor: `${status.color}20`,
                      color: status.color,
                      fontSize: '8px',
                      fontWeight: 700,
                      borderRadius: '2px',
                    }}
                  >
                    {status.text}
                  </span>
                </td>

                {/* Time */}
                <td style={{ padding: '10px', color: BLOOMBERG.GRAY, fontSize: '9px' }}>
                  {formatDateTime(order.timestamp)}
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
