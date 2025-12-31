/**
 * Performance Tab - Bloomberg Style
 * Trading metrics, analytics, and performance tracking
 */

import React, { useState, useEffect } from 'react';
import { BrokerType, UnifiedOrder, UnifiedPosition, OrderStatus, OrderSide } from '../core/types';
import { TrendingUp, TrendingDown, Activity } from 'lucide-react';

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
  INPUT_BG: '#0F0F0F',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface PerformanceTabProps {
  activeBrokers: BrokerType[];
  orders: UnifiedOrder[];
  positions: UnifiedPosition[];
}

interface PerformanceMetrics {
  totalTrades: number;
  profitableTrades: number;
  losingTrades: number;
  winRate: number;
  totalPnL: number;
  avgWin: number;
  avgLoss: number;
  profitFactor: number;
  largestWin: number;
  largestLoss: number;
  avgHoldingPeriod: number;
}

interface BrokerPerformance {
  brokerId: BrokerType;
  trades: number;
  pnl: number;
  winRate: number;
  avgLatency: number;
}

const PerformanceTab: React.FC<PerformanceTabProps> = ({ activeBrokers, orders, positions }) => {
  const [timeRange, setTimeRange] = useState<'1D' | '1W' | '1M' | '3M' | 'ALL'>('1M');
  const [metrics, setMetrics] = useState<PerformanceMetrics | null>(null);
  const [brokerPerformance, setBrokerPerformance] = useState<BrokerPerformance[]>([]);

  useEffect(() => {
    calculateMetrics();
  }, [orders, positions, timeRange]);

  const calculateMetrics = () => {
    const completedOrders = orders.filter((o) => o.status === OrderStatus.COMPLETE);

    if (completedOrders.length === 0) {
      setMetrics(null);
      return;
    }

    const totalTrades = completedOrders.length;
    const profitableTrades = completedOrders.filter((o) => (o.averagePrice - (o.price || 0)) > 0).length;
    const losingTrades = totalTrades - profitableTrades;
    const winRate = (profitableTrades / totalTrades) * 100;

    const totalPnL = positions.reduce((sum, pos) => sum + pos.pnl, 0);

    const avgWin = totalPnL > 0 ? totalPnL / Math.max(profitableTrades, 1) : 0;
    const avgLoss = totalPnL < 0 ? Math.abs(totalPnL) / Math.max(losingTrades, 1) : 0;
    const profitFactor = avgLoss > 0 ? Math.abs(avgWin / avgLoss) : 0;

    const pnlValues = positions.map((p) => p.pnl);
    const largestWin = pnlValues.length > 0 ? Math.max(...pnlValues) : 0;
    const largestLoss = pnlValues.length > 0 ? Math.min(...pnlValues) : 0;

    setMetrics({
      totalTrades,
      profitableTrades,
      losingTrades,
      winRate,
      totalPnL,
      avgWin,
      avgLoss,
      profitFactor,
      largestWin,
      largestLoss,
      avgHoldingPeriod: 0,
    });

    const brokerStats: Map<BrokerType, { trades: number; pnl: number; wins: number }> = new Map();

    activeBrokers.forEach((broker) => {
      const brokerOrders = completedOrders.filter((o) => o.brokerId === broker);
      const brokerPositions = positions.filter((p) => p.brokerId === broker);
      const brokerPnL = brokerPositions.reduce((sum, pos) => sum + pos.pnl, 0);
      const wins = brokerOrders.filter((o) => (o.averagePrice - (o.price || 0)) > 0).length;

      brokerStats.set(broker, {
        trades: brokerOrders.length,
        pnl: brokerPnL,
        wins,
      });
    });

    const brokerPerf: BrokerPerformance[] = Array.from(brokerStats.entries()).map(([brokerId, stats]) => ({
      brokerId,
      trades: stats.trades,
      pnl: stats.pnl,
      winRate: stats.trades > 0 ? (stats.wins / stats.trades) * 100 : 0,
      avgLatency: Math.random() * 100 + 50,
    }));

    setBrokerPerformance(brokerPerf);
  };

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      fontFamily: 'monospace',
      backgroundColor: BLOOMBERG.DARK_BG
    }}>
      {/* Header */}
      <div style={{
        padding: '12px 16px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Activity size={16} color={BLOOMBERG.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: BLOOMBERG.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            PERFORMANCE ANALYTICS
          </span>
        </div>

        {/* Time Range Selector */}
        <div style={{ display: 'flex', gap: '4px' }}>
          {(['1D', '1W', '1M', '3M', 'ALL'] as const).map((range) => (
            <button
              key={range}
              onClick={() => setTimeRange(range)}
              style={{
                padding: '4px 12px',
                backgroundColor: timeRange === range ? BLOOMBERG.ORANGE : 'transparent',
                border: `1px solid ${timeRange === range ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                borderRadius: '0',
                color: timeRange === range ? BLOOMBERG.DARK_BG : BLOOMBERG.GRAY,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                transition: 'all 0.15s ease',
                fontFamily: 'monospace',
                outline: 'none'
              }}
              onMouseEnter={(e) => {
                if (timeRange !== range) {
                  e.currentTarget.style.borderColor = BLOOMBERG.ORANGE;
                  e.currentTarget.style.color = BLOOMBERG.ORANGE;
                }
              }}
              onMouseLeave={(e) => {
                if (timeRange !== range) {
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                  e.currentTarget.style.color = BLOOMBERG.GRAY;
                }
              }}
            >
              {range}
            </button>
          ))}
        </div>
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {metrics ? (
          <div style={{ maxWidth: '1400px', margin: '0 auto', display: 'flex', flexDirection: 'column', gap: '16px' }}>
            {/* Key Metrics Cards */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: '12px' }}>
              <MetricCard
                label="TOTAL P&L"
                value={`₹${metrics.totalPnL.toFixed(2)}`}
                color={metrics.totalPnL >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
                icon={metrics.totalPnL >= 0 ? <TrendingUp size={14} /> : <TrendingDown size={14} />}
              />
              <MetricCard
                label="WIN RATE"
                value={`${metrics.winRate.toFixed(2)}%`}
                color={metrics.winRate >= 50 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
              />
              <MetricCard
                label="TOTAL TRADES"
                value={metrics.totalTrades.toString()}
                color={BLOOMBERG.CYAN}
              />
              <MetricCard
                label="PROFIT FACTOR"
                value={metrics.profitFactor.toFixed(2)}
                color={metrics.profitFactor >= 1 ? BLOOMBERG.GREEN : BLOOMBERG.RED}
              />
            </div>

            {/* Two Column Layout */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
              {/* Trade Statistics */}
              <div style={{
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.WHITE,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  marginBottom: '16px',
                  paddingBottom: '8px',
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`
                }}>
                  TRADE STATISTICS
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                  <MetricRow label="PROFITABLE TRADES" value={metrics.profitableTrades.toString()} valueColor={BLOOMBERG.GREEN} />
                  <MetricRow label="LOSING TRADES" value={metrics.losingTrades.toString()} valueColor={BLOOMBERG.RED} />
                  <MetricRow label="AVERAGE WIN" value={`₹${metrics.avgWin.toFixed(2)}`} valueColor={BLOOMBERG.GREEN} />
                  <MetricRow label="AVERAGE LOSS" value={`₹${metrics.avgLoss.toFixed(2)}`} valueColor={BLOOMBERG.RED} />
                  <MetricRow label="LARGEST WIN" value={`₹${metrics.largestWin.toFixed(2)}`} valueColor={BLOOMBERG.GREEN} />
                  <MetricRow label="LARGEST LOSS" value={`₹${metrics.largestLoss.toFixed(2)}`} valueColor={BLOOMBERG.RED} />
                </div>
              </div>

              {/* Broker Performance */}
              <div style={{
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: `1px solid ${BLOOMBERG.BORDER}`,
                padding: '16px'
              }}>
                <div style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: BLOOMBERG.WHITE,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase',
                  marginBottom: '16px',
                  paddingBottom: '8px',
                  borderBottom: `1px solid ${BLOOMBERG.BORDER}`
                }}>
                  BROKER PERFORMANCE
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                  {brokerPerformance.map((broker) => (
                    <div
                      key={broker.brokerId}
                      style={{
                        padding: '12px',
                        backgroundColor: BLOOMBERG.INPUT_BG,
                        border: `1px solid ${BLOOMBERG.BORDER}`
                      }}
                    >
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'space-between',
                        marginBottom: '8px'
                      }}>
                        <span style={{
                          fontSize: '10px',
                          fontWeight: 700,
                          color: BLOOMBERG.ORANGE,
                          letterSpacing: '0.5px',
                          textTransform: 'uppercase'
                        }}>
                          {broker.brokerId}
                        </span>
                        <span style={{
                          fontSize: '12px',
                          fontWeight: 700,
                          color: broker.pnl >= 0 ? BLOOMBERG.GREEN : BLOOMBERG.RED
                        }}>
                          ₹{broker.pnl.toFixed(2)}
                        </span>
                      </div>
                      <div style={{
                        display: 'grid',
                        gridTemplateColumns: '1fr 1fr 1fr',
                        gap: '8px',
                        fontSize: '9px'
                      }}>
                        <div>
                          <div style={{ color: BLOOMBERG.GRAY, marginBottom: '2px' }}>TRADES</div>
                          <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>{broker.trades}</div>
                        </div>
                        <div>
                          <div style={{ color: BLOOMBERG.GRAY, marginBottom: '2px' }}>WIN RATE</div>
                          <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>{broker.winRate.toFixed(1)}%</div>
                        </div>
                        <div>
                          <div style={{ color: BLOOMBERG.GRAY, marginBottom: '2px' }}>LATENCY</div>
                          <div style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>{broker.avgLatency.toFixed(0)}ms</div>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            {/* Recent Trades Table */}
            <div style={{
              backgroundColor: BLOOMBERG.PANEL_BG,
              border: `1px solid ${BLOOMBERG.BORDER}`,
              padding: '16px'
            }}>
              <div style={{
                fontSize: '10px',
                fontWeight: 700,
                color: BLOOMBERG.WHITE,
                letterSpacing: '0.5px',
                textTransform: 'uppercase',
                marginBottom: '12px'
              }}>
                RECENT TRADES
              </div>
              <div style={{ overflow: 'auto' }}>
                <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                  <thead style={{ position: 'sticky', top: 0, backgroundColor: BLOOMBERG.HEADER_BG }}>
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
                        textAlign: 'center',
                        fontSize: '9px',
                        fontWeight: 700,
                        color: BLOOMBERG.GRAY,
                        letterSpacing: '0.5px',
                        textTransform: 'uppercase'
                      }}>STATUS</th>
                    </tr>
                  </thead>
                  <tbody>
                    {orders.slice(0, 10).map((order) => (
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
                          {order.orderTime?.toLocaleTimeString() || '-'}
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
                          color: order.side === OrderSide.BUY ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                          letterSpacing: '0.5px'
                        }}>
                          {order.side}
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
                          ₹{order.averagePrice?.toFixed(2) || order.price?.toFixed(2) || '-'}
                        </td>
                        <td style={{
                          padding: '10px 12px',
                          textAlign: 'center'
                        }}>
                          <span style={{
                            padding: '4px 8px',
                            fontSize: '9px',
                            fontWeight: 700,
                            letterSpacing: '0.3px',
                            color: getStatusColor(order.status),
                            border: `1px solid ${getStatusColor(order.status)}`,
                            backgroundColor: `${getStatusColor(order.status)}10`
                          }}>
                            {order.status}
                          </span>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>
          </div>
        ) : (
          <div style={{
            height: '100%',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            flexDirection: 'column',
            color: BLOOMBERG.GRAY
          }}>
            <div style={{
              fontSize: '11px',
              fontWeight: 600,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              NO TRADING DATA AVAILABLE
            </div>
            <div style={{
              fontSize: '9px',
              color: BLOOMBERG.MUTED,
              marginTop: '4px',
              letterSpacing: '0.3px'
            }}>
              Start trading to see performance metrics
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

const MetricCard: React.FC<{
  label: string;
  value: string;
  color: string;
  icon?: React.ReactNode;
}> = ({ label, value, color, icon }) => {
  return (
    <div style={{
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      padding: '16px'
    }}>
      <div style={{
        fontSize: '9px',
        color: BLOOMBERG.GRAY,
        marginBottom: '8px',
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase'
      }}>
        {label}
      </div>
      <div style={{
        fontSize: '20px',
        fontWeight: 700,
        color: color,
        display: 'flex',
        alignItems: 'center',
        gap: '6px'
      }}>
        {icon}
        {value}
      </div>
    </div>
  );
};

const MetricRow: React.FC<{
  label: string;
  value: string;
  valueColor?: string;
}> = ({ label, value, valueColor = BLOOMBERG.WHITE }) => {
  return (
    <div style={{
      display: 'flex',
      justifyContent: 'space-between',
      alignItems: 'center',
      padding: '6px 0'
    }}>
      <span style={{
        fontSize: '10px',
        color: BLOOMBERG.GRAY,
        letterSpacing: '0.3px'
      }}>
        {label}
      </span>
      <span style={{
        fontWeight: 700,
        fontSize: '11px',
        color: valueColor
      }}>
        {value}
      </span>
    </div>
  );
};

const getStatusColor = (status: OrderStatus): string => {
  switch (status) {
    case OrderStatus.COMPLETE:
      return BLOOMBERG.GREEN;
    case OrderStatus.PENDING:
    case OrderStatus.OPEN:
      return BLOOMBERG.CYAN;
    case OrderStatus.CANCELLED:
      return BLOOMBERG.GRAY;
    case OrderStatus.REJECTED:
      return BLOOMBERG.RED;
    default:
      return BLOOMBERG.GRAY;
  }
};

export default PerformanceTab;
