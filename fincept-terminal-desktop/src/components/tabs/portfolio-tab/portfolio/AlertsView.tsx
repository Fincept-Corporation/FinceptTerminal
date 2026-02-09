import React, { useState } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { formatCurrency } from './utils';
import { Bell } from 'lucide-react';

interface AlertsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AlertsView: React.FC<AlertsViewProps> = ({ portfolioSummary }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const currency = portfolioSummary.portfolio.currency;
  const [hoveredCard, setHoveredCard] = useState<string | null>(null);

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      padding: '12px',
      overflow: 'auto',
      fontFamily
    }}>
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '16px',
      }}>
        {t('alerts.priceAlerts')}
      </div>

      {/* Feature Not Implemented Notice */}
      <div style={{
        padding: '24px',
        textAlign: 'center',
        backgroundColor: colors.panel,
        border: `1px solid ${colors.primary}`,
        borderRadius: '2px',
        marginBottom: '16px'
      }}>
        <Bell size={64} color={colors.primary} style={{ margin: `0 auto 12px` }} />
        <div style={{
          color: colors.primary,
          fontSize: fontSize.small,
          fontWeight: 700,
          marginBottom: '8px',
          fontFamily
        }}>
          {t('alerts.featureTitle')}
        </div>
        <div style={{
          color: colors.textMuted,
          fontSize: fontSize.tiny,
          marginBottom: '12px',
          fontFamily
        }}>
          {t('alerts.notImplemented')}
        </div>
        <div style={{
          color: colors.textMuted,
          fontSize: fontSize.tiny,
          fontFamily
        }}>
          {t('alerts.willAllowYou')}
        </div>
      </div>

      {/* Feature Description */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '12px',
        marginBottom: '16px'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-border, #2A2A2A)',
          borderRadius: '2px'
        }}>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '8px',
            fontFamily
          }}>
            {t('alerts.plannedFeatures')}
          </div>
          <ul style={{
            margin: 0,
            paddingLeft: '12px',
            fontSize: fontSize.tiny,
            color: colors.text,
            fontFamily
          }}>
            <li style={{ marginBottom: '4px' }}>{t('alerts.priceTargetAlerts')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.stopLossNotifications')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.takeProfitAlerts')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.customPriceConditions')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.volumeSpikeAlerts')}</li>
          </ul>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-border, #2A2A2A)',
          borderRadius: '2px'
        }}>
          <div style={{
            color: colors.success,
            fontSize: fontSize.tiny,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '8px',
            fontFamily
          }}>
            {t('alerts.notificationChannels')}
          </div>
          <ul style={{
            margin: 0,
            paddingLeft: '12px',
            fontSize: fontSize.tiny,
            color: colors.text,
            fontFamily
          }}>
            <li style={{ marginBottom: '4px' }}>{t('alerts.desktopNotifications')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.emailAlerts')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.smsNotifications')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.pushNotifications')}</li>
            <li style={{ marginBottom: '4px' }}>{t('alerts.dailyWeeklySummaries')}</li>
          </ul>
        </div>
      </div>

      {/* Position-Based Alert Suggestions Preview */}
      <div style={{
        color: colors.primary,
        fontSize: fontSize.tiny,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase',
        marginBottom: '12px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${colors.primary}`,
        fontFamily
      }}>
        {t('alerts.suggestedAlerts')}
      </div>

      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '12px',
        marginBottom: '16px'
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
                padding: '12px',
                backgroundColor: isHovered ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                border: isHovered ? `1px solid var(--ft-color-accent, #00E5FF)` : '1px solid var(--ft-color-border, #2A2A2A)',
                borderRadius: '2px',
                opacity: 0.6,
                transition: 'all 0.2s ease',
                cursor: 'default'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                marginBottom: '8px'
              }}>
                <div style={{
                  color: 'var(--ft-color-accent, #00E5FF)',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  fontFamily
                }}>
                  {holding.symbol}
                </div>
                <div style={{
                  color: 'var(--ft-color-accent, #00E5FF)',
                  fontSize: fontSize.tiny,
                  fontFamily
                }}>
                  {formatCurrency(holding.current_price, currency)}
                </div>
              </div>

              <div style={{ fontSize: fontSize.tiny, marginBottom: '8px' }}>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between',
                  marginBottom: '4px'
                }}>
                  <span style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    fontFamily
                  }}>{t('alerts.stopLoss')}</span>
                  <span style={{
                    color: colors.alert,
                    fontSize: fontSize.tiny,
                    fontFamily
                  }}>{formatCurrency(stopLoss, currency)}</span>
                </div>
                <div style={{
                  display: 'flex',
                  justifyContent: 'space-between'
                }}>
                  <span style={{
                    color: colors.textMuted,
                    fontSize: fontSize.tiny,
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase',
                    fontFamily
                  }}>{t('alerts.takeProfit')}</span>
                  <span style={{
                    color: colors.success,
                    fontSize: fontSize.tiny,
                    fontFamily
                  }}>{formatCurrency(takeProfit, currency)}</span>
                </div>
              </div>

              <button
                disabled
                style={{
                  width: '100%',
                  padding: '4px',
                  backgroundColor: colors.textMuted,
                  color: colors.background,
                  border: 'none',
                  borderRadius: '2px',
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  cursor: 'not-allowed',
                  fontFamily,
                  opacity: 0.5,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}
              >
                {t('alerts.comingSoon')}
              </button>
            </div>
          );
        })}
      </div>

      {/* Implementation Note */}
      <div style={{
        padding: '12px',
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: `1px solid ${colors.primary}`,
        borderRadius: '2px'
      }}>
        <div style={{
          color: colors.primary,
          fontSize: fontSize.tiny,
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          marginBottom: '8px',
          fontFamily
        }}>
          {t('alerts.implementationStatus')}
        </div>
        <div style={{
          color: colors.textMuted,
          fontSize: fontSize.tiny,
          fontFamily
        }}>
          The price alerts system requires backend implementation including:
          <ul style={{ marginTop: '8px', paddingLeft: '12px' }}>
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
