// Fyers Orders Panel
// View and place orders with live updates

import React, { useState, useEffect } from 'react';
import { FileText, RefreshCw, Plus } from 'lucide-react';
import { fyersService, FyersOrder } from '../../../services/fyersService';
import { fyersWebSocket } from '../../../services/fyersWebSocket';

interface FyersOrdersPanelProps {
  wsConnected: boolean;
}

const FyersOrdersPanel: React.FC<FyersOrdersPanelProps> = ({ wsConnected }) => {
  const [orders, setOrders] = useState<FyersOrder[]>([]);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState('');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';
  const BLOOMBERG_GREEN = '#00C800';
  const BLOOMBERG_RED = '#FF0000';
  const BLOOMBERG_YELLOW = '#FFFF00';

  useEffect(() => {
    loadOrders();

    // Subscribe to order updates if WebSocket connected
    if (wsConnected) {
      fyersWebSocket.onOrderUpdate((update) => {
        console.log('[Orders] WebSocket update:', update);
        loadOrders(); // Refresh orders on update
      });
    }

    return () => {
      fyersWebSocket.clearCallbacks();
    };
  }, [wsConnected]);

  const loadOrders = async () => {
    setIsLoading(true);
    setError('');

    try {
      const data = await fyersService.getOrders();
      setOrders(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load orders');
    } finally {
      setIsLoading(false);
    }
  };

  const getOrderStatusColor = (status: number) => {
    // Status codes: 1=Pending, 2=Placed, 5=Partially Filled, 6=Filled, 7=Rejected, 8=Cancelled
    if (status === 6) return BLOOMBERG_GREEN; // Filled
    if (status === 7) return BLOOMBERG_RED; // Rejected
    if (status === 8) return BLOOMBERG_GRAY; // Cancelled
    if (status === 5) return BLOOMBERG_YELLOW; // Partially Filled
    return BLOOMBERG_ORANGE; // Pending/Placed
  };

  const getOrderStatusText = (status: number) => {
    const statusMap: Record<number, string> = {
      1: 'PENDING',
      2: 'PLACED',
      5: 'PARTIAL',
      6: 'FILLED',
      7: 'REJECTED',
      8: 'CANCELLED',
      90: 'COMPLETE'
    };
    return statusMap[status] || `STATUS_${status}`;
  };

  const formatCurrency = (amount: number) => {
    return `₹${amount.toLocaleString('en-IN', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}`;
  };

  if (isLoading && orders.length === 0) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: BLOOMBERG_DARK_BG,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center'
      }}>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>Loading orders...</div>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG_DARK_BG,
      display: 'flex',
      flexDirection: 'column'
    }}>
      {/* Header */}
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '12px',
        flexShrink: 0,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <FileText size={12} color={BLOOMBERG_ORANGE} />
          <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
            ORDER BOOK ({orders.length})
          </span>
          {wsConnected && (
            <span style={{
              color: BLOOMBERG_GREEN,
              fontSize: '8px',
              backgroundColor: BLOOMBERG_DARK_BG,
              padding: '2px 6px',
              border: `1px solid ${BLOOMBERG_GREEN}`
            }}>
              LIVE
            </span>
          )}
        </div>

        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={loadOrders}
            disabled={isLoading}
            style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_GRAY}`,
              color: BLOOMBERG_WHITE,
              padding: '4px 8px',
              fontSize: '9px',
              cursor: isLoading ? 'not-allowed' : 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              opacity: isLoading ? 0.5 : 1
            }}
          >
            <RefreshCw size={10} />
            REFRESH
          </button>
        </div>
      </div>

      {/* Orders Table */}
      <div style={{ flex: 1, overflow: 'auto', padding: '12px' }}>
        {error && (
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_RED}`,
            color: BLOOMBERG_RED,
            padding: '10px',
            fontSize: '9px',
            marginBottom: '12px'
          }}>
            {error}
          </div>
        )}

        {orders.length === 0 ? (
          <div style={{
            textAlign: 'center',
            color: BLOOMBERG_GRAY,
            padding: '40px',
            fontSize: '10px'
          }}>
            No orders found
          </div>
        ) : (
          <div style={{
            backgroundColor: BLOOMBERG_PANEL_BG,
            border: `1px solid ${BLOOMBERG_GRAY}`
          }}>
            <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px' }}>
              <thead>
                <tr style={{ backgroundColor: BLOOMBERG_DARK_BG, borderBottom: `1px solid ${BLOOMBERG_GRAY}` }}>
                  <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    TIME
                  </th>
                  <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    SYMBOL
                  </th>
                  <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    TYPE
                  </th>
                  <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    SIDE
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    QTY
                  </th>
                  <th style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    PRICE
                  </th>
                  <th style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    STATUS
                  </th>
                  <th style={{ padding: '8px', textAlign: 'left', color: BLOOMBERG_ORANGE, fontWeight: 'bold' }}>
                    ORDER ID
                  </th>
                </tr>
              </thead>
              <tbody>
                {orders.map((order, index) => (
                  <tr
                    key={order.id}
                    style={{
                      borderBottom: index < orders.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none',
                      backgroundColor: index % 2 === 0 ? BLOOMBERG_DARK_BG : BLOOMBERG_PANEL_BG
                    }}
                  >
                    <td style={{ padding: '8px', color: BLOOMBERG_GRAY, fontSize: '8px' }}>
                      {order.orderDateTime}
                    </td>
                    <td style={{ padding: '8px', color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>
                      {order.symbol}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center', color: BLOOMBERG_GRAY, fontSize: '8px' }}>
                      {order.type === 1 ? 'LIMIT' : order.type === 2 ? 'MARKET' : `TYPE_${order.type}`}
                    </td>
                    <td style={{
                      padding: '8px',
                      textAlign: 'center',
                      color: order.side === 1 ? BLOOMBERG_GREEN : BLOOMBERG_RED,
                      fontWeight: 'bold',
                      fontSize: '8px'
                    }}>
                      {order.side === 1 ? 'BUY' : 'SELL'}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>
                      {order.filledQty}/{order.qty}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', color: BLOOMBERG_GRAY, fontFamily: 'Consolas, monospace' }}>
                      {formatCurrency(order.limitPrice)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center' }}>
                      <span style={{
                        color: getOrderStatusColor(order.status),
                        fontSize: '8px',
                        fontWeight: 'bold',
                        backgroundColor: BLOOMBERG_DARK_BG,
                        padding: '2px 6px',
                        border: `1px solid ${getOrderStatusColor(order.status)}`
                      }}>
                        {getOrderStatusText(order.status)}
                      </span>
                    </td>
                    <td style={{ padding: '8px', color: BLOOMBERG_GRAY, fontSize: '8px', fontFamily: 'Consolas, monospace' }}>
                      {order.id}
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  );
};

export default FyersOrdersPanel;
