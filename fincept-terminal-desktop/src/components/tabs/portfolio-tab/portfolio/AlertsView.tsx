import React from 'react';
import { PortfolioSummary } from '../../../../services/portfolioService';
import { formatCurrency } from './utils';
import { Bell } from 'lucide-react';
import { BLOOMBERG, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../bloombergStyles';

interface AlertsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AlertsView: React.FC<AlertsViewProps> = ({ portfolioSummary }) => {
  const currency = portfolioSummary.portfolio.currency;

  return (
    <div style={{
      height: '100%',
      backgroundColor: BLOOMBERG.DARK_BG,
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
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: BORDERS.ORANGE,
        marginBottom: SPACING.LARGE
      }}>
        <Bell size={64} color={BLOOMBERG.ORANGE} style={{ margin: `0 auto ${SPACING.MEDIUM}` }} />
        <div style={{ color: BLOOMBERG.ORANGE, fontSize: TYPOGRAPHY.SUBHEADING, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.SMALL }}>
          PRICE ALERTS FEATURE
        </div>
        <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.DEFAULT, marginBottom: SPACING.MEDIUM }}>
          This feature is not yet implemented.
        </div>
        <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.BODY }}>
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
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: BORDERS.STANDARD
        }}>
          <div style={{ color: BLOOMBERG.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.SMALL }}>
            PLANNED FEATURES
          </div>
          <ul style={{ margin: 0, paddingLeft: SPACING.DEFAULT, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.WHITE }}>
            <li style={{ marginBottom: SPACING.TINY }}>Price target alerts</li>
            <li style={{ marginBottom: SPACING.TINY }}>Stop loss notifications</li>
            <li style={{ marginBottom: SPACING.TINY }}>Take profit alerts</li>
            <li style={{ marginBottom: SPACING.TINY }}>Custom price conditions</li>
            <li style={{ marginBottom: SPACING.TINY }}>Volume spike alerts</li>
          </ul>
        </div>

        <div style={{
          padding: SPACING.MEDIUM,
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: BORDERS.STANDARD
        }}>
          <div style={{ color: BLOOMBERG.GREEN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.SMALL }}>
            NOTIFICATION CHANNELS
          </div>
          <ul style={{ margin: 0, paddingLeft: SPACING.DEFAULT, fontSize: TYPOGRAPHY.BODY, color: BLOOMBERG.WHITE }}>
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
        color: BLOOMBERG.ORANGE,
        fontSize: TYPOGRAPHY.DEFAULT,
        fontWeight: TYPOGRAPHY.BOLD,
        marginBottom: SPACING.MEDIUM,
        paddingBottom: SPACING.SMALL,
        borderBottom: BORDERS.ORANGE
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

          return (
            <div
              key={holding.id}
              style={{
                padding: SPACING.MEDIUM,
                backgroundColor: BLOOMBERG.PANEL_BG,
                border: BORDERS.STANDARD,
                opacity: 0.6
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: SPACING.SMALL
              }}>
                <div style={{ color: BLOOMBERG.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>
                  {holding.symbol}
                </div>
                <div style={{ color: BLOOMBERG.WHITE, fontSize: TYPOGRAPHY.BODY }}>
                  {formatCurrency(holding.current_price, currency)}
                </div>
              </div>

              <div style={{ fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.SMALL }}>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: SPACING.TINY,
                  color: BLOOMBERG.GRAY
                }}>
                  <span>Stop Loss (10%):</span>
                  <span style={{ color: BLOOMBERG.RED }}>{formatCurrency(stopLoss, currency)}</span>
                </div>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  color: BLOOMBERG.GRAY
                }}>
                  <span>Take Profit (20%):</span>
                  <span style={{ color: BLOOMBERG.GREEN }}>{formatCurrency(takeProfit, currency)}</span>
                </div>
              </div>

              <button
                disabled
                style={{
                  width: '100%',
                  padding: SPACING.TINY,
                  backgroundColor: BLOOMBERG.GRAY,
                  color: BLOOMBERG.DARK_BG,
                  border: 'none',
                  fontSize: TYPOGRAPHY.SMALL,
                  fontWeight: TYPOGRAPHY.BOLD,
                  cursor: 'not-allowed',
                  fontFamily: TYPOGRAPHY.MONO,
                  opacity: 0.5
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
        border: BORDERS.ORANGE
      }}>
        <div style={{
          color: BLOOMBERG.ORANGE,
          fontSize: TYPOGRAPHY.DEFAULT,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.SMALL
        }}>
          IMPLEMENTATION STATUS
        </div>
        <div style={{ color: BLOOMBERG.GRAY, fontSize: TYPOGRAPHY.BODY }}>
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
