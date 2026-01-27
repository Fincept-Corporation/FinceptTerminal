import React from 'react';
import { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatNumber, formatPercent } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../finceptStyles';

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
      overflow: 'hidden',
      fontFamily: TYPOGRAPHY.MONO,
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: '0px',
      }}>
        CURRENT POSITIONS
      </div>

      {/* Table Container */}
      <div style={{ flex: 1, overflow: 'auto' }}>
        {/* Table Header */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
          gap: '8px',
          padding: '8px 12px',
          backgroundColor: FINCEPT.HEADER_BG,
          fontSize: '9px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          borderBottom: `1px solid ${FINCEPT.ORANGE}`,
          position: 'sticky',
          top: 0,
          zIndex: 1,
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
            ...COMMON_STYLES.emptyState,
            padding: '24px',
            height: 'auto',
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
                gap: '8px',
                padding: '8px 12px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent',
                borderLeft: `3px solid ${holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED}`,
                fontSize: '10px',
                marginBottom: '1px',
                fontFamily: TYPOGRAPHY.MONO,
                minHeight: '32px',
                alignItems: 'center',
                transition: EFFECTS.TRANSITION_STANDARD,
              }}
              onMouseEnter={(e) => { e.currentTarget.style.backgroundColor = FINCEPT.HOVER; }}
              onMouseLeave={(e) => { e.currentTarget.style.backgroundColor = index % 2 === 0 ? 'rgba(255,255,255,0.03)' : 'transparent'; }}
            >
              <div style={{ color: FINCEPT.CYAN, fontWeight: 700 }}>{holding.symbol}</div>
              <div style={{ color: FINCEPT.CYAN, textAlign: 'right' }}>{formatNumber(holding.quantity, 4)}</div>
              <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>{formatCurrency(holding.avg_buy_price, currency)}</div>
              <div style={{ color: FINCEPT.CYAN, textAlign: 'right' }}>{formatCurrency(holding.current_price, currency)}</div>
              <div style={{ color: FINCEPT.WHITE, textAlign: 'right', fontWeight: 600 }}>
                {formatCurrency(holding.market_value, currency)}
              </div>
              <div style={{ color: FINCEPT.GRAY, textAlign: 'right' }}>{formatCurrency(holding.cost_basis, currency)}</div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontWeight: 600,
                }}>
                  {holding.unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(holding.unrealized_pnl, currency)}
                </div>
                <div style={{
                  color: holding.unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: '9px',
                }}>
                  {formatPercent(holding.unrealized_pnl_percent)}
                </div>
              </div>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  color: holding.day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                }}>
                  {holding.day_change >= 0 ? '+' : ''}{formatCurrency(holding.day_change, currency)}
                </div>
                <div style={{
                  color: holding.day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                  fontSize: '9px',
                }}>
                  {formatPercent(holding.day_change_percent)}
                </div>
              </div>
              <div style={{ color: FINCEPT.YELLOW, textAlign: 'right', fontWeight: 600 }}>
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
            backgroundColor: FINCEPT.PANEL_BG,
            borderTop: `2px solid ${FINCEPT.ORANGE}`,
            fontSize: '10px',
            fontWeight: 700,
            marginTop: '8px',
            fontFamily: TYPOGRAPHY.MONO,
            minHeight: '36px',
            alignItems: 'center',
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
                color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
              }}>
                {portfolioSummary.total_unrealized_pnl >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: '9px',
              }}>
                {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
              }}>
                {portfolioSummary.total_day_change >= 0 ? '+' : ''}{formatCurrency(portfolioSummary.total_day_change, currency)}
              </div>
              <div style={{
                color: portfolioSummary.total_day_change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontSize: '9px',
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
