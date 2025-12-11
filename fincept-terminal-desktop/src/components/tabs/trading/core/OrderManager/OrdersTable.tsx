/**
 * Orders Table
 * Universal component - works with ALL exchanges
 */

import React from 'react';
import { useOrders, useCancelOrder } from '../../hooks/useOrders';
import { formatPrice, formatCurrency, formatDateTime, formatOrderStatus } from '../../utils/formatters';
import type { UnifiedOrder } from '../../types';

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

export function OrdersTable() {
  const { openOrders, isLoading, refresh } = useOrders(undefined, true, 2000);
  const { cancelOrder, cancelAllOrders, isCanceling } = useCancelOrder();

  const handleCancelOrder = async (order: UnifiedOrder) => {
    if (!confirm(`Cancel ${order.type} ${order.side} order for ${order.symbol}?`)) {
      return;
    }

    try {
      await cancelOrder(order.id, order.symbol);
      await refresh();
      alert(`✓ Order canceled: ${order.id}`);
    } catch (error) {
      alert(`✗ Failed to cancel order: ${(error as Error).message}`);
    }
  };

  const handleCancelAll = async () => {
    if (!confirm(`Cancel ALL open orders?`)) {
      return;
    }

    try {
      await cancelAllOrders();
      await refresh();
      alert('✓ All orders canceled');
    } catch (error) {
      alert(`✗ Failed to cancel all orders: ${(error as Error).message}`);
    }
  };

  if (isLoading && openOrders.length === 0) {
    return (
      <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
        Loading orders...
      </div>
    );
  }

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      {/* Header with Cancel All button */}
      {openOrders.length > 0 && (
        <div
          style={{
            padding: '8px 12px',
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderBottom: `1px solid ${BLOOMBERG.BORDER}`,
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}
        >
          <span style={{ fontSize: '10px', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
            OPEN ORDERS ({openOrders.length})
          </span>
          <button
            onClick={handleCancelAll}
            disabled={isCanceling}
            style={{
              padding: '4px 10px',
              backgroundColor: `${BLOOMBERG.RED}20`,
              border: `1px solid ${BLOOMBERG.RED}`,
              color: BLOOMBERG.RED,
              fontSize: '9px',
              fontWeight: 700,
              cursor: isCanceling ? 'not-allowed' : 'pointer',
              opacity: isCanceling ? 0.5 : 1,
            }}
          >
            CANCEL ALL
          </button>
        </div>
      )}

      {/* Orders Table */}
      <div style={{ flex: 1, overflowX: 'auto', overflowY: 'auto' }}>
        {openOrders.length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: BLOOMBERG.GRAY, fontSize: '11px' }}>
            No open orders
          </div>
        ) : (
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
                  FILLED
                </th>
                <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
                  STATUS
                </th>
                <th style={{ padding: '8px 10px', textAlign: 'left', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
                  TIME
                </th>
                <th style={{ padding: '8px 10px', textAlign: 'center', color: BLOOMBERG.GRAY, fontWeight: 700 }}>
                  ACTION
                </th>
              </tr>
            </thead>
            <tbody>
              {openOrders.map((order, index) => {
                const status = formatOrderStatus(order.status);
                const fillPercent = (order.filled / order.quantity) * 100;

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
                    <td style={{ padding: '10px', color: BLOOMBERG.GRAY, textTransform: 'uppercase' }}>
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

                    {/* Filled */}
                    <td style={{ padding: '10px', textAlign: 'right' }}>
                      <div style={{ color: BLOOMBERG.WHITE }}>{order.filled.toFixed(4)}</div>
                      <div style={{ fontSize: '8px', color: BLOOMBERG.GRAY }}>
                        ({fillPercent.toFixed(0)}%)
                      </div>
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

                    {/* Action */}
                    <td style={{ padding: '10px', textAlign: 'center' }}>
                      <button
                        onClick={() => handleCancelOrder(order)}
                        disabled={isCanceling}
                        style={{
                          padding: '4px 10px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${BLOOMBERG.RED}`,
                          color: BLOOMBERG.RED,
                          fontSize: '9px',
                          fontWeight: 700,
                          cursor: isCanceling ? 'not-allowed' : 'pointer',
                          opacity: isCanceling ? 0.5 : 1,
                        }}
                        onMouseEnter={(e) => {
                          if (!isCanceling) {
                            e.currentTarget.style.backgroundColor = BLOOMBERG.RED;
                            e.currentTarget.style.color = BLOOMBERG.WHITE;
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (!isCanceling) {
                            e.currentTarget.style.backgroundColor = 'transparent';
                            e.currentTarget.style.color = BLOOMBERG.RED;
                          }
                        }}
                      >
                        CANCEL
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
