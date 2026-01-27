import React, { useState } from 'react';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { formatCurrency } from './utils';
import { Bell } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';

interface AlertsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AlertsView: React.FC<AlertsViewProps> = ({ portfolioSummary }) => {
  const currency = portfolioSummary.portfolio.currency;
  const [hoveredCard, setHoveredCard] = useState<string | null>(null);

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.LARGE
      }}>
        PRICE ALERTS & NOTIFICATIONS
      </div>

      {/* Feature Not Implemented Notice */}
      <div style={{
        padding: SPACING.XLARGE,
        textAlign: 'center',
        backgroundColor: FINCEPT.PANEL_BG,
        border: BORDERS.ORANGE,
        borderRadius: '2px',
        marginBottom: SPACING.LARGE
      }}>
        <Bell size={64} color={FINCEPT.ORANGE} style={{ margin: `0 auto ${SPACING.MEDIUM}` }} />
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '13px',
          fontWeight: 700,
          marginBottom: SPACING.SMALL,
          fontFamily: TYPOGRAPHY.MONO
        }}>
          PRICE ALERTS FEATURE
        </div>
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '11px',
          marginBottom: SPACING.MEDIUM,
          fontFamily: TYPOGRAPHY.MONO
        }}>
          This feature is not yet implemented.
        </div>
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '11px',
          fontFamily: TYPOGRAPHY.MONO
        }}>
          Price alerts will allow you to set notifications for:
        </div>
      </div>

      {/* Feature Description */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: SPACING.DEFAULT,
        marginBottom: SPACING.LARGE
      }}>
        <div style={{
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.STANDARD,
          borderRadius: '2px'
        }}>
          <div style={{
            color: FINCEPT.CYAN,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: SPACING.SMALL,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            PLANNED FEATURES
          </div>
          <ul style={{
            margin: 0,
            paddingLeft: SPACING.DEFAULT,
            fontSize: '10px',
            color: FINCEPT.WHITE,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            <li style={{ marginBottom: SPACING.TINY }}>Price target alerts</li>
            <li style={{ marginBottom: SPACING.TINY }}>Stop loss notifications</li>
            <li style={{ marginBottom: SPACING.TINY }}>Take profit alerts</li>
            <li style={{ marginBottom: SPACING.TINY }}>Custom price conditions</li>
            <li style={{ marginBottom: SPACING.TINY }}>Volume spike alerts</li>
          </ul>
        </div>

        <div style={{
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.STANDARD,
          borderRadius: '2px'
        }}>
          <div style={{
            color: FINCEPT.GREEN,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: SPACING.SMALL,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            NOTIFICATION CHANNELS
          </div>
          <ul style={{
            margin: 0,
            paddingLeft: SPACING.DEFAULT,
            fontSize: '10px',
            color: FINCEPT.WHITE,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            <li style={{ marginBottom: SPACING.TINY }}>Desktop notifications</li>
            <li style={{ marginBottom: SPACING.TINY }}>Email alerts</li>
            <li style={{ marginBottom: SPACING.TINY }}>SMS notifications (Premium)</li>
            <li style={{ marginBottom: SPACING.TINY }}>Push notifications</li>
            <li style={{ marginBottom: SPACING.TINY }}>Daily/weekly summaries</li>
          </ul>
        </div>
      </div>

      {/* Position-Based Alert Suggestions Preview */}
      <div style={{
        color: FINCEPT.ORANGE,
        fontSize: '9px',
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase',
        marginBottom: SPACING.MEDIUM,
        paddingBottom: SPACING.SMALL,
        borderBottom: BORDERS.ORANGE,
        fontFamily: TYPOGRAPHY.MONO
      }}>
        SUGGESTED ALERTS FOR YOUR POSITIONS (PREVIEW)
      </div>

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: SPACING.DEFAULT,
        marginBottom: SPACING.LARGE
      }}>
        {portfolioSummary.holdings.slice(0, 4).map(holding => {
          const stopLoss = holding.avg_buy_price * 0.9;
          const takeProfit = holding.avg_buy_price * 1.2;
          const isHovered = hoveredCard === holding.id;

          return (
            <div
              key={holding.id}
              onMouseEnter={() => setHoveredCard(holding.id)}
              onMouseLeave={() => setHoveredCard(null)}
              style={{
                padding: SPACING.MEDIUM,
                backgroundColor: isHovered ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                border: isHovered ? BORDERS.CYAN : BORDERS.STANDARD,
                borderRadius: '2px',
                opacity: 0.6,
                transition: EFFECTS.TRANSITION_STANDARD,
                cursor: 'default'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: SPACING.SMALL
              }}>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  fontFamily: TYPOGRAPHY.MONO
                }}>
                  {holding.symbol}
                </div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontFamily: TYPOGRAPHY.MONO
                }}>
                  {formatCurrency(holding.current_price, currency)}
                </div>
              </div>

              <div style={{ fontSize: '10px', marginBottom: SPACING.SMALL }}>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: SPACING.TINY
                }}>
                  <span style={{
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    fontFamily: TYPOGRAPHY.MONO
                  }}>Stop Loss (10%):</span>
                  <span style={{
                    color: FINCEPT.RED,
                    fontSize: '10px',
                    fontFamily: TYPOGRAPHY.MONO
                  }}>{formatCurrency(stopLoss, currency)}</span>
                </div>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between'
                }}>
                  <span style={{
                    color: FINCEPT.GRAY,
                    fontSize: '9px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    fontFamily: TYPOGRAPHY.MONO
                  }}>Take Profit (20%):</span>
                  <span style={{
                    color: FINCEPT.GREEN,
                    fontSize: '10px',
                    fontFamily: TYPOGRAPHY.MONO
                  }}>{formatCurrency(takeProfit, currency)}</span>
                </div>
              </div>

              <button
                disabled
                style={{
                  width: '100%',
                  padding: SPACING.TINY,
                  backgroundColor: FINCEPT.GRAY,
                  color: FINCEPT.DARK_BG,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'not-allowed',
                  fontFamily: TYPOGRAPHY.MONO,
                  opacity: 0.5,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}
              >
                COMING SOON
              </button>
            </div>
          );
        })}
      </div>

      {/* Implementation Note */}
      <div style={{
        padding: SPACING.DEFAULT,
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: BORDERS.ORANGE,
        borderRadius: '2px'
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '9px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          marginBottom: SPACING.SMALL,
          fontFamily: TYPOGRAPHY.MONO
        }}>
          IMPLEMENTATION STATUS
        </div>
        <div style={{
          color: FINCEPT.GRAY,
          fontSize: '10px',
          fontFamily: TYPOGRAPHY.MONO
        }}>
          The price alerts system requires backend implementation including:
          <ul style={{ marginTop: SPACING.SMALL, paddingLeft: SPACING.DEFAULT }}>
            <li>Database schema for price alert conditions</li>
            <li>Real-time price monitoring service</li>
            <li>Notification delivery system</li>
            <li>Alert creation and management API endpoints</li>
          </ul>
        </div>
      </div>
    </div>
  );
};

export default AlertsView;
