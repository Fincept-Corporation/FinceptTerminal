import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatPercent } from './utils';
import { COMMON_STYLES } from '../finceptStyles';

interface PositionsViewProps {
  portfolioSummary: PortfolioSummary;
}

const PositionsView: React.FC<PositionsViewProps> = ({ portfolioSummary }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const currency = portfolioSummary.portfolio.currency;

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: colors.background,
      overflow: 'hidden',
      fontFamily,
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '0px',
      }}>
        {t('positions.currentPositions', 'CURRENT POSITIONS')}
      </div>

      {/* Table Container */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          fontSize: fontSize.tiny,
          fontWeight: 700,
          letterSpacing: '0.5px',
          borderBottom: `1px solid ${colors.primary}`,
          position: 'sticky',
          top: 0,
          zIndex: 1,
        }}>
          <div style={{ color: colors.primary }}>{t('positions.symbol', 'SYMBOL')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.qty', 'QTY')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.avgPrice', 'AVG PRICE')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.current', 'CURRENT')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.mktValue', 'MKT VALUE')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.costBasis', 'COST BASIS')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.pnl', 'P&L')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.dayChange', 'DAY CHANGE')}</div>
          <div style={{ color: colors.primary, textAlign: 'right' }}>{t('positions.weight', 'WEIGHT')}</div>
        </div>

        {/* Position Rows */}
        {portfolioSummary.holdings.length === 0 ? (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            padding: '24px',
            height: 'auto',
            color: '#4A4A4A',
            fontSize: fontSize.small,
            textAlign: 'center',
          }}>
            {t('positions.noPositions', 'No positions yet. Click BUY to add your first position.')}
          </div>
        ) : (
          portfolioSummary.holdings.map((holding, index) => (
            <div
              key={holding.id || `${holding.symbol}-${index}`}
              style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
                gap: '8px',
                padding: '8px 12px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                borderLeft: `3px solid ${holding.unrealized_pnl >= 0 ? colors.success : colors.alert}`,
                fontSize: fontSize.small,
                marginBottom: '1px',
                fontFamily,
                minHeight: '32px',
                alignItems: 'center',
                transition: 'all 0.2s ease',
              }}
              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = 'var(--ft-color-hover, #1F1F1F)'; }}
              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'; }}
            >
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 700 }}>{holding.symbol}</div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right' }}>{formatNumber(holding.quantity, 4)}</div>
              <div style={{ color: colors.textMuted, textAlign: 'right' }}>{formatCurrency(holding.avg_buy_price, currency)}</div>
              <div style={{ color: 'var(--ft-color-accent, #00E5FF)', textAlign: 'right' }}>{formatCurrency(holding.current_price, currency)}</div>
              <div style={{ color: colors.text, textAlign: 'right', fontWeight: 600 }}>
                {formatCurrency(holding.market_value, currency)}
              </div>
              <div style={{ color: colors.textMuted, textAlign: 'right' }}>{formatCurrency(holding.cost_basis, currency)}</div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
                  fontWeight: 600,
                }}>
                  {holding.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(holding.unrealized_pnl, currency)}
                </div>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
                  fontSize: fontSize.tiny,
                }}>
                  {formatPercent(holding.unrealized_pnl_percent)}
                </div>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.day_change >= 0 ? colors.success : colors.alert,
                }}>
                  {holding.day_change >= 0 ? '+' : ''}{formatCurrency(holding.day_change, currency)}
                </div>
                <div style={{
                  color: holding.day_change >= 0 ? colors.success : colors.alert,
                  fontSize: fontSize.tiny,
                }}>
                  {formatPercent(holding.day_change_percent)}
                </div>
              </div>
              <div style={{ color: 'var(--ft-color-warning, #FFD700)', textAlign: 'right', fontWeight: 600 }}>
                {formatNumber(holding.weight)}%
              </div>
            </div>
          ))
        )}

        {/* Totals Row */}
        {portfolioSummary.holdings.length > 0 && (
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
            gap: '8px',
            padding: '8px 12px',
            backgroundColor: colors.panel,
            borderTop: `2px solid ${colors.primary}`,
            fontSize: fontSize.small,
            fontWeight: 700,
            marginTop: '8px',
            fontFamily,
            minHeight: '36px',
            alignItems: 'center',
          }}>
            <div style={{ color: colors.primary }}>{t('positions.total', 'TOTAL')}</div>
            <div></div>
            <div></div>
            <div></div>
            <div style={{ color: colors.text, textAlign: 'right' }}>
              {formatCurrency(portfolioSummary.total_market_value, currency)}
            </div>
            <div style={{ color: colors.textMuted, textAlign: 'right' }}>
              {formatCurrency(portfolioSummary.total_cost_basis, currency)}
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert,
              }}>
                {portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert,
                fontSize: fontSize.tiny,
              }}>
                {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? colors.success : colors.alert,
              }}>
                {portfolioSummary.total_day_change >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_day_change, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? colors.success : colors.alert,
                fontSize: fontSize.tiny,
              }}>
                {formatPercent(portfolioSummary.total_day_change_percent)}
              </div>
            </div>
            <div style={{ color: 'var(--ft-color-warning, #FFD700)', textAlign: 'right' }}>100.0%</div>
          </div>
        )}
      </div>
    </div>
  );
};

export default PositionsView;
