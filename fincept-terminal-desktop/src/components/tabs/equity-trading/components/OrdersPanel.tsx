/**
 * Orders Panel Component - Bloomberg Style
 *
 * Displays open orders and order history with actions
 */

import React, { useState } from 'react';
import {
  RefreshCw, X, Clock, CheckCircle2, XCircle, AlertCircle,
  Loader2, Edit2, Filter, FileText
} from 'lucide-react';
import { useStockTradingData, useStockBrokerContext } from '@/contexts/StockBrokerContext';
import type { Order, OrderStatus } from '@/brokers/stocks/types';

// Bloomberg color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  PURPLE: '#A855F7',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

const STATUS_CONFIG: Record<OrderStatus, { label: string; color: string; icon: React.ElementType }> = {
  PENDING: { label: 'PENDING', color: COLORS.YELLOW, icon: Clock },
  OPEN: { label: 'OPEN', color: COLORS.CYAN, icon: Clock },
  PARTIALLY_FILLED: { label: 'PARTIAL', color: COLORS.ORANGE, icon: Clock },
  FILLED: { label: 'FILLED', color: COLORS.GREEN, icon: CheckCircle2 },
  CANCELLED: { label: 'CANCELLED', color: COLORS.GRAY, icon: XCircle },
  REJECTED: { label: 'REJECTED', color: COLORS.RED, icon: XCircle },
  EXPIRED: { label: 'EXPIRED', color: COLORS.MUTED, icon: AlertCircle },
  TRIGGER_PENDING: { label: 'TRIGGER', color: COLORS.PURPLE, icon: Clock },
};

interface OrdersPanelProps {
  showHistory?: boolean;
  onModifyOrder?: (order: Order) => void;
}

export function OrdersPanel({ showHistory = false, onModifyOrder }: OrdersPanelProps) {
  const { orders, refreshOrders, isRefreshing, isReady } = useStockTradingData();
  const { adapter } = useStockBrokerContext();

  const [statusFilter, setStatusFilter] = useState<OrderStatus | 'ALL'>('ALL');
  const [cancellingOrderId, setCancellingOrderId] = useState<string | null>(null);
  const [isCancellingAll, setIsCancellingAll] = useState(false);
  const [showFilters, setShowFilters] = useState(false);

  // Filter orders
  const filteredOrders = React.useMemo(() => {
    let result = [...orders];

    // Filter by status
    if (statusFilter !== 'ALL') {
      result = result.filter((o) => o.status === statusFilter);
    }

    // If not showing history, only show open/pending orders
    if (!showHistory) {
      result = result.filter((o) =>
        ['PENDING', 'OPEN', 'PARTIALLY_FILLED', 'TRIGGER_PENDING'].includes(o.status)
      );
    }

    // Sort by date
    result.sort((a, b) => new Date(b.placedAt).getTime() - new Date(a.placedAt).getTime());

    return result;
  }, [orders, statusFilter, showHistory]);

  // Count open orders
  const openOrdersCount = orders.filter((o) =>
    ['PENDING', 'OPEN', 'PARTIALLY_FILLED', 'TRIGGER_PENDING'].includes(o.status)
  ).length;

  // Handle cancel order
  const handleCancelOrder = async (orderId: string) => {
    if (!adapter) return;

    setCancellingOrderId(orderId);

    try {
      const response = await adapter.cancelOrder(orderId);
      if (response.success) {
        await refreshOrders();
      }
    } catch (err) {
      console.error('Failed to cancel order:', err);
    } finally {
      setCancellingOrderId(null);
    }
  };

  // Handle cancel all orders
  const handleCancelAll = async () => {
    if (!adapter || openOrdersCount === 0) return;

    setIsCancellingAll(true);

    try {
      const result = await adapter.cancelAllOrders();
      if (result.success) {
        await refreshOrders();
      }
    } catch (err) {
      console.error('Failed to cancel all orders:', err);
    } finally {
      setIsCancellingAll(false);
    }
  };

  // Format time
  const formatTime = (date: Date) => {
    return new Date(date).toLocaleTimeString('en-IN', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false,
    });
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
        Connect to your broker to view orders
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
          <span style={{ fontSize: '11px', fontWeight: 600, color: COLORS.WHITE }}>
            {showHistory ? 'ORDER HISTORY' : 'OPEN ORDERS'}
          </span>
          <span style={{ fontSize: '10px', color: COLORS.GRAY }}>
            ({showHistory ? filteredOrders.length : openOrdersCount})
          </span>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          {/* Filter Toggle */}
          {showHistory && (
            <button
              onClick={() => setShowFilters(!showFilters)}
              style={{
                padding: '4px',
                backgroundColor: showFilters ? `${COLORS.ORANGE}20` : 'transparent',
                border: `1px solid ${showFilters ? COLORS.ORANGE : COLORS.BORDER}`,
                color: showFilters ? COLORS.ORANGE : COLORS.GRAY,
                cursor: 'pointer'
              }}
            >
              <Filter size={12} />
            </button>
          )}

          {/* Refresh */}
          <button
            onClick={() => refreshOrders()}
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

          {/* Cancel All */}
          {!showHistory && openOrdersCount > 0 && (
            <button
              onClick={handleCancelAll}
              disabled={isCancellingAll}
              style={{
                padding: '3px 8px',
                fontSize: '9px',
                fontWeight: 600,
                backgroundColor: `${COLORS.RED}20`,
                color: COLORS.RED,
                border: `1px solid ${COLORS.RED}40`,
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              {isCancellingAll ? (
                <Loader2 size={10} className="animate-spin" />
              ) : (
                <X size={10} />
              )}
              CANCEL ALL
            </button>
          )}
        </div>
      </div>

      {/* Filters */}
      {showFilters && showHistory && (
        <div style={{
          padding: '6px 12px',
          backgroundColor: COLORS.DARK_BG,
          borderBottom: `1px solid ${COLORS.BORDER}`,
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          flexShrink: 0
        }}>
          <span style={{ fontSize: '9px', color: COLORS.GRAY }}>STATUS:</span>
          <select
            value={statusFilter}
            onChange={(e) => setStatusFilter(e.target.value as OrderStatus | 'ALL')}
            style={{
              padding: '4px 8px',
              backgroundColor: COLORS.HEADER_BG,
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.WHITE,
              fontSize: '10px',
              outline: 'none',
              cursor: 'pointer'
            }}
          >
            <option value="ALL">All</option>
            <option value="FILLED">Filled</option>
            <option value="CANCELLED">Cancelled</option>
            <option value="REJECTED">Rejected</option>
            <option value="EXPIRED">Expired</option>
          </select>
        </div>
      )}

      {/* Orders Table */}
      <div style={{ flex: 1, overflow: 'auto', minHeight: 0 }}>
        {filteredOrders.length === 0 ? (
          <div style={{
            padding: '24px',
            textAlign: 'center',
            color: COLORS.GRAY
          }}>
            <FileText size={32} color={COLORS.MUTED} style={{ margin: '0 auto 12px' }} />
            <p style={{ fontSize: '11px' }}>
              {showHistory ? 'No orders match the filter' : 'No open orders'}
            </p>
          </div>
        ) : (
          <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '10px' }}>
            <thead style={{ position: 'sticky', top: 0, backgroundColor: COLORS.HEADER_BG, zIndex: 1 }}>
              <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                <th style={{ padding: '6px 8px', textAlign: 'left', color: COLORS.GRAY, fontWeight: 600 }}>ORDER</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>QTY</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>PRICE</th>
                <th style={{ padding: '6px 8px', textAlign: 'center', color: COLORS.GRAY, fontWeight: 600 }}>STATUS</th>
                <th style={{ padding: '6px 8px', textAlign: 'right', color: COLORS.GRAY, fontWeight: 600 }}>TIME</th>
                <th style={{ padding: '6px 8px', textAlign: 'center', color: COLORS.GRAY, fontWeight: 600 }}>ACT</th>
              </tr>
            </thead>
            <tbody>
              {filteredOrders.map((order, idx) => {
                const statusConfig = STATUS_CONFIG[order.status];
                const StatusIcon = statusConfig.icon;
                const isCancelling = cancellingOrderId === order.orderId;
                const canCancel = ['PENDING', 'OPEN', 'TRIGGER_PENDING'].includes(order.status);
                const canModify = ['PENDING', 'OPEN'].includes(order.status);
                const isBuy = order.side === 'BUY';

                return (
                  <tr
                    key={order.orderId}
                    style={{
                      borderBottom: `1px solid ${COLORS.BORDER}40`,
                      backgroundColor: idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`
                    }}
                    onMouseEnter={(e) => e.currentTarget.style.backgroundColor = COLORS.HEADER_BG}
                    onMouseLeave={(e) => e.currentTarget.style.backgroundColor = idx % 2 === 0 ? 'transparent' : `${COLORS.HEADER_BG}40`}
                  >
                    <td style={{ padding: '8px' }}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                        <span style={{
                          padding: '2px 4px',
                          fontSize: '9px',
                          fontWeight: 600,
                          backgroundColor: isBuy ? `${COLORS.GREEN}20` : `${COLORS.RED}20`,
                          color: isBuy ? COLORS.GREEN : COLORS.RED
                        }}>
                          {order.side}
                        </span>
                        <div>
                          <span style={{ fontSize: '10px', color: COLORS.WHITE, fontWeight: 600 }}>{order.symbol}</span>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                            <span style={{ fontSize: '9px', color: COLORS.GRAY }}>{order.exchange}</span>
                            <span style={{ fontSize: '9px', color: COLORS.MUTED }}>|</span>
                            <span style={{ fontSize: '9px', color: COLORS.ORANGE }}>{order.orderType}</span>
                          </div>
                        </div>
                      </div>
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right' }}>
                      <span style={{ fontFamily: 'monospace', color: COLORS.WHITE }}>{order.quantity}</span>
                      {order.filledQuantity > 0 && order.filledQuantity < order.quantity && (
                        <div style={{ fontSize: '9px', color: COLORS.ORANGE }}>
                          {order.filledQuantity} filled
                        </div>
                      )}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right' }}>
                      {order.price > 0 ? (
                        <span style={{ fontFamily: 'monospace', color: COLORS.WHITE }}>{order.price.toFixed(2)}</span>
                      ) : (
                        <span style={{ fontSize: '9px', color: COLORS.GRAY }}>MKT</span>
                      )}
                      {order.triggerPrice && order.triggerPrice > 0 && (
                        <div style={{ fontSize: '9px', color: COLORS.PURPLE }}>
                          @ {order.triggerPrice.toFixed(2)}
                        </div>
                      )}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center' }}>
                      <div style={{
                        display: 'inline-flex',
                        alignItems: 'center',
                        gap: '4px',
                        padding: '2px 6px',
                        backgroundColor: `${statusConfig.color}20`,
                        color: statusConfig.color
                      }}>
                        <StatusIcon size={10} />
                        <span style={{ fontSize: '9px', fontWeight: 600 }}>{statusConfig.label}</span>
                      </div>
                      {order.statusMessage && (
                        <div style={{ fontSize: '8px', color: COLORS.RED, marginTop: '2px', maxWidth: '80px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                          {order.statusMessage}
                        </div>
                      )}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'right', fontFamily: 'monospace', color: COLORS.GRAY, fontSize: '9px' }}>
                      {formatTime(order.placedAt)}
                    </td>
                    <td style={{ padding: '8px', textAlign: 'center' }}>
                      {(canCancel || canModify) ? (
                        <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
                          {canModify && onModifyOrder && (
                            <button
                              onClick={() => onModifyOrder(order)}
                              style={{
                                padding: '3px',
                                backgroundColor: 'transparent',
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.GRAY,
                                cursor: 'pointer'
                              }}
                              title="Modify Order"
                              onMouseEnter={(e) => {
                                e.currentTarget.style.borderColor = COLORS.ORANGE;
                                e.currentTarget.style.color = COLORS.ORANGE;
                              }}
                              onMouseLeave={(e) => {
                                e.currentTarget.style.borderColor = COLORS.BORDER;
                                e.currentTarget.style.color = COLORS.GRAY;
                              }}
                            >
                              <Edit2 size={10} />
                            </button>
                          )}

                          {canCancel && (
                            <button
                              onClick={() => handleCancelOrder(order.orderId)}
                              disabled={isCancelling}
                              style={{
                                padding: '3px',
                                backgroundColor: `${COLORS.RED}20`,
                                border: `1px solid ${COLORS.RED}40`,
                                color: COLORS.RED,
                                cursor: 'pointer'
                              }}
                              title="Cancel Order"
                              onMouseEnter={(e) => {
                                e.currentTarget.style.backgroundColor = `${COLORS.RED}40`;
                              }}
                              onMouseLeave={(e) => {
                                e.currentTarget.style.backgroundColor = `${COLORS.RED}20`;
                              }}
                            >
                              {isCancelling ? (
                                <Loader2 size={10} className="animate-spin" />
                              ) : (
                                <X size={10} />
                              )}
                            </button>
                          )}
                        </div>
                      ) : (
                        <span style={{ fontSize: '9px', color: COLORS.MUTED }}>-</span>
                      )}
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
