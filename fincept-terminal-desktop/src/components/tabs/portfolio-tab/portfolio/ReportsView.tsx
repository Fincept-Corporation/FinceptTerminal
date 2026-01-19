import React from 'react';
import { PortfolioSummary, Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent, calculateTaxLiability } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';

interface ReportsViewProps {
  portfolioSummary: PortfolioSummary;
  transactions: Transaction[];
}

const ReportsView: React.FC<ReportsViewProps> = ({ portfolioSummary, transactions }) => {
  const currency = portfolioSummary.portfolio.currency;

  // Calculate tax liability
  const taxCalculation = calculateTaxLiability(transactions, 0.15, 0.20, 365);

  // Calculate dividend income from transactions
  const dividendTransactions = transactions.filter(t => t.transaction_type === 'DIVIDEND');
  const totalDividends = dividendTransactions.reduce((sum, t) => sum + (t.price * t.quantity), 0);
  const dividendYield = portfolioSummary.total_market_value > 0
    ? (totalDividends / portfolioSummary.total_market_value) * 100
    : 0;

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
        TAX REPORTS & STATEMENTS
      </div>

      {/* Summary Cards */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: SPACING.DEFAULT,
        marginBottom: SPACING.XLARGE
      }}>
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.ORANGE
        }}>
          <div style={COMMON_STYLES.dataLabel}>TOTAL TAX LIABILITY</div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.HEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.GREEN
        }}>
          <div style={COMMON_STYLES.dataLabel}>LONG-TERM GAINS</div>
          <div style={{
            color: FINCEPT.GREEN,
            fontSize: TYPOGRAPHY.HEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            marginTop: SPACING.TINY
          }}>
            Tax: {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: `1px solid ${FINCEPT.YELLOW}`
        }}>
          <div style={COMMON_STYLES.dataLabel}>SHORT-TERM GAINS</div>
          <div style={{
            color: FINCEPT.YELLOW,
            fontSize: TYPOGRAPHY.HEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            marginTop: SPACING.TINY
          }}>
            Tax: {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.CYAN
        }}>
          <div style={COMMON_STYLES.dataLabel}>UNREALIZED GAINS</div>
          <div style={{
            color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: TYPOGRAPHY.HEADING,
            fontWeight: TYPOGRAPHY.BOLD
          }}>
            {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: TYPOGRAPHY.TINY,
            marginTop: SPACING.TINY
          }}>
            Not taxable until sold
          </div>
        </div>
      </div>

      {/* Tax Breakdown */}
      <div style={{
        color: FINCEPT.ORANGE,
        fontSize: TYPOGRAPHY.DEFAULT,
        fontWeight: TYPOGRAPHY.BOLD,
        marginBottom: SPACING.MEDIUM,
        paddingBottom: SPACING.SMALL,
        borderBottom: BORDERS.ORANGE
      }}>
        TAX BREAKDOWN (FIFO METHOD)
      </div>

      <div style={{
        padding: SPACING.DEFAULT,
        backgroundColor: FINCEPT.PANEL_BG,
        border: BORDERS.STANDARD,
        marginBottom: SPACING.XLARGE
      }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          marginBottom: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: BORDERS.ORANGE
        }}>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>CATEGORY</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, textAlign: 'right' }}>GAINS</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, textAlign: 'right' }}>TAX RATE</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD, textAlign: 'right' }}>TAX DUE</div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: 'rgba(255,215,0,0.05)',
          borderLeft: `3px solid ${FINCEPT.YELLOW}`,
          marginBottom: SPACING.SMALL
        }}>
          <div style={{ color: FINCEPT.YELLOW, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
            Short-Term Capital Gains (&lt; 1 year)
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.BODY, textAlign: 'right' }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, textAlign: 'right' }}>15%</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
            {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: 'rgba(0,214,111,0.05)',
          borderLeft: `3px solid ${FINCEPT.GREEN}`,
          marginBottom: SPACING.SMALL
        }}>
          <div style={{ color: FINCEPT.GREEN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
            Long-Term Capital Gains (&gt; 1 year)
          </div>
          <div style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.BODY, textAlign: 'right' }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY, textAlign: 'right' }}>20%</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.BODY, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
            {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: 'rgba(255,136,0,0.1)',
          borderTop: `2px solid ${FINCEPT.ORANGE}`,
          marginTop: SPACING.SMALL
        }}>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>TOTAL TAX DUE</div>
          <div></div>
          <div></div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.SUBHEADING, textAlign: 'right', fontWeight: TYPOGRAPHY.BOLD }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>
      </div>

      {/* Additional Information */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
        {/* Transaction Summary */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.MEDIUM,
            paddingBottom: SPACING.SMALL,
            borderBottom: BORDERS.ORANGE
          }}>
            TRANSACTION SUMMARY
          </div>

          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Total Transactions:</span>
              <span style={{ color: FINCEPT.WHITE, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {transactions.length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Buy Transactions:</span>
              <span style={{ color: FINCEPT.GREEN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {transactions.filter(t => t.transaction_type === 'BUY').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Sell Transactions:</span>
              <span style={{ color: FINCEPT.RED, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {transactions.filter(t => t.transaction_type === 'SELL').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between'
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Current Positions:</span>
              <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {portfolioSummary.holdings.length}
              </span>
            </div>
          </div>
        </div>

        {/* Dividend Information */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.MEDIUM,
            paddingBottom: SPACING.SMALL,
            borderBottom: BORDERS.ORANGE
          }}>
            DIVIDEND INCOME
          </div>

          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Total Dividends (YTD):</span>
              <span style={{ color: FINCEPT.GREEN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {formatCurrency(totalDividends, currency)}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between'
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>Portfolio Yield:</span>
              <span style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.BODY, fontWeight: TYPOGRAPHY.BOLD }}>
                {dividendYield.toFixed(2)}%
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* Export Options */}
      <div style={{
        marginTop: SPACING.XLARGE,
        padding: SPACING.DEFAULT,
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: BORDERS.ORANGE
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: TYPOGRAPHY.DEFAULT,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.MEDIUM
        }}>
          EXPORT OPTIONS
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM }}>
          <button style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: FINCEPT.ORANGE,
            borderColor: FINCEPT.ORANGE
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            EXPORT TAX REPORT (PDF)
          </button>

          <button style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: FINCEPT.GREEN,
            borderColor: FINCEPT.GREEN
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            EXPORT TRANSACTIONS (CSV)
          </button>

          <button style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: FINCEPT.CYAN,
            borderColor: FINCEPT.CYAN,
            color: FINCEPT.DARK_BG
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            EXPORT HOLDINGS (XLSX)
          </button>
        </div>

        <div style={{
          marginTop: SPACING.MEDIUM,
          fontSize: TYPOGRAPHY.TINY,
          color: FINCEPT.GRAY
        }}>
          Reports are generated for the current tax year. Consult with a tax professional for accurate filing.
        </div>
      </div>
    </div>
  );
};

export default ReportsView;
