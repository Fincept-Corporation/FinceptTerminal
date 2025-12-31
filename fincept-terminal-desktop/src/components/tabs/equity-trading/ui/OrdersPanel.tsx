/**
 * Orders Panel - Bloomberg Style Real-time Order Display
 */

import React from 'react';
import { UnifiedOrder, OrderStatus, OrderSide } from '../core/types';
import { RefreshCw, X } from 'lucide-react';

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

interface OrdersPanelProps {
  orders: UnifiedOrder[];
  loading: boolean;
  onRefresh: () => void;
  onCancel: (brokerId: string, orderId: string) => void;
}

const OrdersPanel: React.FC<OrdersPanelProps> = ({ orders, loading, onRefresh, onCancel }) => {
  const getStatusColor = (status: OrderStatus) => {
    switch (status) {
      case OrderStatus.COMPLETE: return BLOOMBERG.GREEN;
      case OrderStatus.CANCELLED: return BLOOMBERG.GRAY;
      case OrderStatus.REJECTED: return BLOOMBERG.RED;
      case OrderStatus.PENDING: return BLOOMBERG.YELLOW;
      case OrderStatus.OPEN: return BLOOMBERG.CYAN;
      default: return BLOOMBERG.MUTED;
    }
  };

  const getSideColor = (side: OrderSide) => {
    return side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.RED;
  };

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', fontFamily: 'monospace' }}>
      {/* Table */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {orders.length === 0 ? (
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
            NO ORDERS
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
                }}>TIME</th>
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
                }}>SIDE</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>TYPE</th>
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
                }}>PRICE</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'right',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>FILLED</th>
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
                  textAlign: 'left',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>STATUS</th>
                <th style={{
                  padding: '8px 12px',
                  textAlign: 'center',
                  fontSize: '9px',
                  fontWeight: 700,
                  color: BLOOMBERG.GRAY,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>ACTION</th>
              </tr>
            </thead>
            <tbody>
              {orders.map((order, index) => (
                <tr
                  key={`${order.brokerId}-${order.id}`}
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
                    fontWeight: 600,
                    color: BLOOMBERG.MUTED
                  }}>
                    {order.orderTime.toLocaleTimeString('en-US', { hour12: false })}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '10px',
                    fontWeight: 700,
                    color: BLOOMBERG.ORANGE,
                    textTransform: 'uppercase',
                    letterSpacing: '0.3px'
                  }}>
                    {order.brokerId}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '11px',
                    fontWeight: 700,
                    color: BLOOMBERG.WHITE,
                    letterSpacing: '0.3px'
                  }}>
                    {order.symbol}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '10px',
                    fontWeight: 700,
                    color: getSideColor(order.side),
                    letterSpacing: '0.5px'
                  }}>
                    {order.side}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '10px',
                    fontWeight: 600,
                    color: BLOOMBERG.GRAY,
                    textTransform: 'uppercase'
                  }}>
                    {order.type}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: BLOOMBERG.WHITE,
                    textAlign: 'right'
                  }}>
                    {order.quantity}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: BLOOMBERG.CYAN,
                    textAlign: 'right'
                  }}>
                    {order.price ? `₹${order.price.toFixed(2)}` : 'MKT'}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: BLOOMBERG.WHITE,
                    textAlign: 'right'
                  }}>
                    {order.filledQuantity}/{order.quantity}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '11px',
                    fontWeight: 600,
                    color: BLOOMBERG.CYAN,
                    textAlign: 'right'
                  }}>
                    {order.averagePrice > 0 ? `₹${order.averagePrice.toFixed(2)}` : '-'}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    fontSize: '10px',
                    fontWeight: 700,
                    color: getStatusColor(order.status),
                    letterSpacing: '0.3px',
                    textTransform: 'uppercase'
                  }}>
                    {order.status}
                  </td>
                  <td style={{
                    padding: '10px 12px',
                    textAlign: 'center'
                  }}>
                    {(order.status === OrderStatus.OPEN || order.status === OrderStatus.PENDING) && (
                      <button
                        onClick={() => onCancel(order.brokerId, order.id)}
                        style={{
                          padding: '4px 8px',
                          backgroundColor: 'transparent',
                          border: `1px solid ${BLOOMBERG.RED}`,
                          borderRadius: '0',
                          color: BLOOMBERG.RED,
                          fontSize: '9px',
                          fontWeight: 700,
                          letterSpacing: '0.5px',
                          cursor: 'pointer',
                          display: 'inline-flex',
                          alignItems: 'center',
                          gap: '4px',
                          transition: 'all 0.15s ease',
                          outline: 'none'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = BLOOMBERG.RED;
                          e.currentTarget.style.color = BLOOMBERG.WHITE;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'transparent';
                          e.currentTarget.style.color = BLOOMBERG.RED;
                        }}
                      >
                        <X size={10} />
                        CANCEL
                      </button>
                    )}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
};

export default OrdersPanel;
