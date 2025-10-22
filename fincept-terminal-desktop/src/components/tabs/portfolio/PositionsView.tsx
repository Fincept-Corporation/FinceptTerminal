import React from 'react';
import { PortfolioSummary } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatNumber, formatPercent } from './utils';

interface PositionsViewProps {
  portfolioSummary: PortfolioSummary;
}

const PositionsView: React.FC<PositionsViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '12px'
      }}>
        CURRENT POSITIONS
      </div>

      {/* Table Header */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
        gap: '8px',
        padding: '8px',
        backgroundColor: 'rgba(255,165,0,0.1)',
        fontSize: '10px',
        fontWeight: 'bold',
        marginBottom: '4px'
      }}>
        <div style={{ color: WHITE }}>SYMBOL</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>QTY</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>AVG PRICE</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>CURRENT</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>MKT VALUE</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>COST BASIS</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>P&L</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>DAY CHANGE</div>
        <div style={{ color: WHITE, textAlign: 'right' }}>WEIGHT</div>
      </div>

      {/* Position Rows */}
      {portfolioSummary.holdings.length === 0 ? (
        <div style={{
          padding: '32px',
          textAlign: 'center',
          color: GRAY,
          fontSize: '11px'
        }}>
          No positions yet. Click F2 or "ADD ASSET" to add your first position.
        </div>
      ) : (
        portfolioSummary.holdings.map((holding, index) => (
          <div
            key={holding.id}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1fr 1fr 1fr 1fr 1fr 1fr 1.2fr 1fr',
              gap: '8px',
              padding: '8px',
              backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
              borderLeft: `3px solid ${holding.unrealized_pnl >= 0 ? GREEN : RED}`,
              fontSize: '10px',
              marginBottom: '2px'
            }}
          >
            <div style={{ color: CYAN, fontWeight: 'bold' }}>{holding.symbol}</div>
            <div style={{ color: WHITE, textAlign: 'right' }}>{formatNumber(holding.quantity, 4)}</div>
            <div style={{ color: GRAY, textAlign: 'right' }}>{formatCurrency(holding.avg_buy_price, currency)}</div>
            <div style={{ color: WHITE, textAlign: 'right' }}>{formatCurrency(holding.current_price, currency)}</div>
            <div style={{ color: WHITE, textAlign: 'right', fontWeight: 'bold' }}>
              {formatCurrency(holding.market_value, currency)}
            </div>
            <div style={{ color: GRAY, textAlign: 'right' }}>{formatCurrency(holding.cost_basis, currency)}</div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: holding.unrealized_pnl >= 0 ? GREEN : RED,
                fontWeight: 'bold'
              }}>
                {formatCurrency(holding.unrealized_pnl, currency)}
              </div>
              <div style={{
                color: holding.unrealized_pnl >= 0 ? GREEN : RED,
                fontSize: '9px'
              }}>
                {formatPercent(holding.unrealized_pnl_percent)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{
                color: holding.day_change >= 0 ? GREEN : RED
              }}>
                {formatCurrency(holding.day_change, currency)}
              </div>
              <div style={{
                color: holding.day_change >= 0 ? GREEN : RED,
                fontSize: '9px'
              }}>
                {formatPercent(holding.day_change_percent)}
              </div>
            </div>
            <div style={{ color: YELLOW, textAlign: 'right', fontWeight: 'bold' }}>
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
          padding: '8px',
          backgroundColor: 'rgba(255,165,0,0.15)',
          borderTop: `2px solid ${ORANGE}`,
          fontSize: '10px',
          fontWeight: 'bold',
          marginTop: '8px'
        }}>
          <div style={{ color: ORANGE }}>TOTAL</div>
          <div></div>
          <div></div>
          <div></div>
          <div style={{ color: WHITE, textAlign: 'right' }}>
            {formatCurrency(portfolioSummary.total_market_value, currency)}
          </div>
          <div style={{ color: GRAY, textAlign: 'right' }}>
            {formatCurrency(portfolioSummary.total_cost_basis, currency)}
          </div>
          <div style={{ textAlign: 'right' }}>
            <div style={{
              color: portfolioSummary.total_unrealized_pnl >= 0 ? GREEN : RED
            }}>
              {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
            </div>
            <div style={{
              color: portfolioSummary.total_unrealized_pnl >= 0 ? GREEN : RED,
              fontSize: '9px'
            }}>
              {formatPercent(portfolioSummary.total_unrealized_pnl_percent)}
            </div>
          </div>
          <div style={{ textAlign: 'right' }}>
            <div style={{
              color: portfolioSummary.total_day_change >= 0 ? GREEN : RED
            }}>
              {formatCurrency(portfolioSummary.total_day_change, currency)}
            </div>
            <div style={{
              color: portfolioSummary.total_day_change >= 0 ? GREEN : RED,
              fontSize: '9px'
            }}>
              {formatPercent(portfolioSummary.total_day_change_percent)}
            </div>
          </div>
          <div style={{ color: YELLOW, textAlign: 'right' }}>100.0%</div>
        </div>
      )}
    </div>
  );
};

export default PositionsView;
