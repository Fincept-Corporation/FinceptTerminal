import React, { useState, useEffect } from 'react';
import { PortfolioSummary } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency } from './utils';
import { Bell, TrendingUp, TrendingDown, Target } from 'lucide-react';

interface PriceAlert {
  id: string;
  symbol: string;
  type: string;
  condition: string;
  target_value: number;
  is_triggered: boolean;
  created_at: string;
}

interface AlertsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AlertsView: React.FC<AlertsViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

  const [alerts, setAlerts] = useState<PriceAlert[]>([]);
  const [loading, setLoading] = useState(false);
  const [stats, setStats] = useState({ total: 0, active: 0, triggered: 0, byType: {} as Record<string, number> });

  // Stub - alerts service removed
  useEffect(() => {
    setLoading(false);
  }, [portfolioSummary.portfolio.id]);

  const handleDeleteAlert = async (alertId: string) => {
    // Stub - alerts service removed
    setAlerts(prev => prev.filter(a => a.id !== alertId));
  };

  const getAlertIcon = (type: string) => {
    switch (type) {
      case 'PRICE_TARGET':
        return <Target size={14} />;
      case 'STOP_LOSS':
        return <TrendingDown size={14} />;
      case 'TAKE_PROFIT':
        return <TrendingUp size={14} />;
      default:
        return <Bell size={14} />;
    }
  };

  const getAlertColor = (type: string) => {
    switch (type) {
      case 'PRICE_TARGET':
        return CYAN;
      case 'STOP_LOSS':
        return RED;
      case 'TAKE_PROFIT':
        return GREEN;
      default:
        return YELLOW;
    }
  };

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        PRICE ALERTS & NOTIFICATIONS
      </div>

      {/* Alert Statistics */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: '12px',
        marginBottom: '20px'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>TOTAL ALERTS</div>
          <div style={{ color: ORANGE, fontSize: '16px', fontWeight: 'bold' }}>
            {stats.total}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(0,200,0,0.1)',
          border: `1px solid ${GREEN}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>ACTIVE</div>
          <div style={{ color: GREEN, fontSize: '16px', fontWeight: 'bold' }}>
            {stats.active}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(255,0,0,0.1)',
          border: `1px solid ${RED}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>TRIGGERED</div>
          <div style={{ color: RED, fontSize: '16px', fontWeight: 'bold' }}>
            {stats.triggered}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(0,255,255,0.1)',
          border: `1px solid ${CYAN}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>POSITIONS TRACKED</div>
          <div style={{ color: CYAN, fontSize: '16px', fontWeight: 'bold' }}>
            {new Set(alerts.map(a => a.symbol)).size}
          </div>
        </div>
      </div>

      {/* Create Alert Button */}
      <div style={{ marginBottom: '20px' }}>
        <button style={{
          padding: '12px 20px',
          backgroundColor: ORANGE,
          color: 'black',
          border: 'none',
          borderRadius: '4px',
          fontSize: '11px',
          fontWeight: 'bold',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px'
        }}>
          <Bell size={14} />
          CREATE NEW ALERT
        </button>
      </div>

      {/* Active Alerts */}
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '12px',
        paddingBottom: '4px',
        borderBottom: `1px solid ${ORANGE}`
      }}>
        ACTIVE ALERTS
      </div>

      {loading ? (
        <div style={{
          padding: '32px',
          textAlign: 'center',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px',
          marginBottom: '20px'
        }}>
          <div style={{ color: ORANGE, fontSize: '11px' }}>
            Loading alerts...
          </div>
        </div>
      ) : alerts.filter(a => !a.is_triggered).length === 0 ? (
        <div style={{
          padding: '32px',
          textAlign: 'center',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `1px solid ${GRAY}`,
          borderRadius: '4px',
          marginBottom: '20px'
        }}>
          <Bell size={48} color={GRAY} style={{ margin: '0 auto 12px' }} />
          <div style={{ color: GRAY, fontSize: '11px' }}>
            No active alerts. Create one to get notified about price movements.
          </div>
        </div>
      ) : (
        <div style={{ marginBottom: '20px' }}>
          {alerts.filter(a => !a.is_triggered).map(alert => {
            // Find current price from portfolio holdings
            const holding = portfolioSummary.holdings.find(h => h.symbol === alert.symbol);
            const currentPrice = holding?.current_price || 0;

            return (
            <div
              key={alert.id}
              style={{
                padding: '12px',
                marginBottom: '8px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                border: `1px solid ${getAlertColor(alert.type)}`,
                borderLeft: `4px solid ${getAlertColor(alert.type)}`,
                borderRadius: '4px',
                display: 'grid',
                gridTemplateColumns: '40px 100px 1fr 150px 150px 80px',
                gap: '12px',
                alignItems: 'center'
              }}
            >
              <div style={{ color: getAlertColor(alert.type), display: 'flex', justifyContent: 'center' }}>
                {getAlertIcon(alert.type)}
              </div>

              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>
                  {alert.symbol}
                </div>
                <div style={{ color: GRAY, fontSize: '8px' }}>
                  {alert.type.replace('_', ' ')}
                </div>
              </div>

              <div>
                <div style={{ color: WHITE, fontSize: '10px' }}>
                  Alert when price {alert.condition.toLowerCase().replace('_', ' ')}
                </div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Created: {alert.created_at.split('T')[0]}
                </div>
              </div>

              <div>
                <div style={{ color: GRAY, fontSize: '8px' }}>TARGET PRICE</div>
                <div style={{ color: getAlertColor(alert.type), fontSize: '12px', fontWeight: 'bold' }}>
                  {formatCurrency(alert.target_value, currency)}
                </div>
              </div>

              <div>
                <div style={{ color: GRAY, fontSize: '8px' }}>CURRENT PRICE</div>
                <div style={{ color: WHITE, fontSize: '12px', fontWeight: 'bold' }}>
                  {currentPrice > 0 ? formatCurrency(currentPrice, currency) : 'N/A'}
                </div>
              </div>

              <div style={{ display: 'flex', gap: '4px', justifyContent: 'flex-end' }}>
                <button
                  onClick={() => handleDeleteAlert(alert.id)}
                  style={{
                    padding: '6px 10px',
                    backgroundColor: RED,
                    color: 'white',
                    border: 'none',
                    borderRadius: '2px',
                    fontSize: '9px',
                    fontWeight: 'bold',
                    cursor: 'pointer'
                  }}>
                  DELETE
                </button>
              </div>
            </div>
          );
        })}
        </div>
      )}

      {/* Position-Based Alert Suggestions */}
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '12px',
        paddingBottom: '4px',
        borderBottom: `1px solid ${ORANGE}`
      }}>
        SUGGESTED ALERTS FOR YOUR POSITIONS
      </div>

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '12px',
        marginBottom: '20px'
      }}>
        {portfolioSummary.holdings.slice(0, 4).map(holding => {
          const stopLoss = holding.avg_buy_price * 0.9; // 10% below buy price
          const takeProfit = holding.avg_buy_price * 1.2; // 20% above buy price

          return (
            <div
              key={holding.id}
              style={{
                padding: '12px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                border: `1px solid ${GRAY}`,
                borderRadius: '4px'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: '8px'
              }}>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>
                  {holding.symbol}
                </div>
                <div style={{ color: WHITE, fontSize: '10px' }}>
                  {formatCurrency(holding.current_price, currency)}
                </div>
              </div>

              <div style={{ fontSize: '9px', marginBottom: '8px' }}>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: '4px',
                  color: GRAY
                }}>
                  <span>Stop Loss (10%):</span>
                  <span style={{ color: RED }}>{formatCurrency(stopLoss, currency)}</span>
                </div>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  color: GRAY
                }}>
                  <span>Take Profit (20%):</span>
                  <span style={{ color: GREEN }}>{formatCurrency(takeProfit, currency)}</span>
                </div>
              </div>

              <button style={{
                width: '100%',
                padding: '6px',
                backgroundColor: ORANGE,
                color: 'black',
                border: 'none',
                borderRadius: '2px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: 'pointer'
              }}>
                CREATE ALERTS
              </button>
            </div>
          );
        })}
      </div>

      {/* Alert Settings */}
      <div style={{
        padding: '16px',
        backgroundColor: 'rgba(255,165,0,0.05)',
        border: `1px solid ${ORANGE}`,
        borderRadius: '4px'
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '11px',
          fontWeight: 'bold',
          marginBottom: '12px'
        }}>
          NOTIFICATION SETTINGS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
          <div>
            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" defaultChecked />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                Desktop notifications
              </span>
            </label>

            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" defaultChecked />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                Email notifications
              </span>
            </label>

            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                SMS notifications (Premium)
              </span>
            </label>
          </div>

          <div>
            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" defaultChecked />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                Daily portfolio summary
              </span>
            </label>

            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                Weekly performance report
              </span>
            </label>

            <label style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              cursor: 'pointer'
            }}>
              <input type="checkbox" defaultChecked />
              <span style={{ color: WHITE, fontSize: '10px' }}>
                Large position movements (&gt;5%)
              </span>
            </label>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AlertsView;
