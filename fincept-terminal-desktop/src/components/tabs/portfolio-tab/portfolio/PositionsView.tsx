import React from 'react';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatPercent } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';

interface PositionsViewProps {
  portfolioSummary: PortfolioSummary;
}

const PositionsView: React.FC<PositionsViewProps> = ({ portfolioSummary }) => {
  const currency = portfolioSummary.portfolio.currency;

  return (
    <div style={{
      height: '100%',
      display: 'flex',
      flexDirection: 'column',
      backgroundColor: FINCEPT.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'hidden'
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.MEDIUM
      }}>
        CURRENT POSITIONS
      </div>

      {/* Table Container */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
          gap: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: TYPOGRAPHY.BODY,
          fontWeight: TYPOGRAPHY.BOLD,
          borderBottom: BORDERS.ORANGE,
          position: 'sticky',
          top: 0,
          zIndex: 1
        }}>
          <div style={{ color: FINCEPT.ORANGE }}>SYMBOL</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>QTY</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>AVG PRICE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>CURRENT</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>MKT VALUE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>COST BASIS</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>P&L</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>DAY CHANGE</div>
          <div style={{ color: FINCEPT.ORANGE, textAlign: 'right' }}>WEIGHT</div>
        </div>

        {/* Position Rows */}
        {portfolioSummary.holdings.length === 0 ? (
          <div style={{
            padding: SPACING.XLARGE,
            textAlign: 'center',
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            No positions yet. Click BUY to add your first position.
          </div>
        ) : (
          portfolioSummary.holdings.map((holding, index) => (
            <div
              key={holding.id || `${holding.symbol}-${index}`}
              style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
                gap: SPACING.MEDIUM,
                padding: SPACING.MEDIUM,
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                borderLeft: `3px solid ${holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED}`,
                fontSize: TYPOGRAPHY.BODY,
                marginBottom: '1px',
                fontFamily: TYPOGRAPHY.MONO,
                minHeight: '32px',
                alignItems: 'center'
              }}
            >
              <div style={{ color: FINCEPT.CYAN, fontWeight: TYPOGRAPHY.BOLD }}>{holding.symbol}</div>
              <div style={{ color: FINCEPT.WHITE, textAlign: 'right' }}>{formatNumber(holding.quantity, 4)}</div>
              <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>{formatCurrency(holding.avg_buy_price, currency)}</div>
              <div style={{ color: FINCEPT.WHITE, textAlign: 'right' }}>{formatCurrency(holding.current_price, currency)}</div>
              <div style={{ color: FINCEPT.WHITE, textAlign: 'right', fontWeight: TYPOGRAPHY.SEMIBOLD }}>
                {formatCurrency(holding.market_value, currency)}
              </div>
              <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>{formatCurrency(holding.cost_basis, currency)}</div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontWeight: TYPOGRAPHY.SEMIBOLD
                }}>
                  {holding.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(holding.unrealized_pnl, currency)}
                </div>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: TYPOGRAPHY.SMALL
                }}>
                  {formatPercent(holding.unrealized_pnl_percent)}
                </div>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontWeight: TYPOGRAPHY.REGULAR
                }}>
                  {holding.day_change >= 0 ? '+' : ''}{formatCurrency(holding.day_change, currency)}
                </div>
                <div style={{
                  color: holding.day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: TYPOGRAPHY.SMALL
                }}>
                  {formatPercent(holding.day_change_percent)}
                </div>
              </div>
              <div style={{ color: FINCEPT.YELLOW, textAlign: 'right', fontWeight: TYPOGRAPHY.SEMIBOLD }}>
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
            gap: SPACING.MEDIUM,
            padding: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            borderTop: BORDERS.ORANGE_THICK,
            fontSize: TYPOGRAPHY.BODY,
            fontWeight: TYPOGRAPHY.BOLD,
            marginTop: SPACING.MEDIUM,
            fontFamily: TYPOGRAPHY.MONO,
            minHeight: '36px',
            alignItems: 'center'
          }}>
            <div style={{ color: FINCEPT.ORANGE }}>TOTAL</div>
            <div></div>
            <div></div>
            <div></div>
            <div style={{ color: FINCEPT.WHITE, textAlign: 'right' }}>
              {formatCurrency(portfolioSummary.total_market_value, currency)}
            </div>
            <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>
              {formatCurrency(portfolioSummary.total_cost_basis, currency)}
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED
              }}>
                {portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: TYPOGRAPHY.SMALL
              }}>
                {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED
              }}>
                {portfolioSummary.total_day_change >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_day_change, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: TYPOGRAPHY.SMALL
              }}>
                {formatPercent(portfolioSummary.total_day_change_percent)}
              </div>
            </div>
            <div style={{ color: FINCEPT.YELLOW, textAlign: 'right' }}>100.0%</div>
          </div>
        )}
      </div>
    </div>
  );
};

export default PositionsView;
