import React from 'react';
import { PortfolioSummary, Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent, calculateTaxLiability } from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';

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
          border: BORDERS.ORANGE,
          borderRadius: '2px',
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>TOTAL TAX LIABILITY</div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: '15px',
            fontWeight: 700,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.GREEN,
          borderRadius: '2px',
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>LONG-TERM GAINS</div>
          <div style={{
            color: FINCEPT.GREEN,
            fontSize: '15px',
            fontWeight: 700,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginTop: SPACING.TINY
          }}>
            Tax: {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: `1px solid ${FINCEPT.YELLOW}`,
          borderRadius: '2px',
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>SHORT-TERM GAINS</div>
          <div style={{
            color: FINCEPT.YELLOW,
            fontSize: '15px',
            fontWeight: 700,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginTop: SPACING.TINY
          }}>
            Tax: {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.CYAN,
          borderRadius: '2px',
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{
            fontSize: '9px',
            fontWeight: 700,
            color: FINCEPT.GRAY,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>UNREALIZED GAINS</div>
          <div style={{
            color: portfolioSummary.total_unrealized_pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '15px',
            fontWeight: 700,
            fontFamily: TYPOGRAPHY.MONO
          }}>
            {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            marginTop: SPACING.TINY
          }}>
            Not taxable until sold
          </div>
        </div>
      </div>

      {/* Tax Breakdown */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.MEDIUM
      }}>
        TAX BREAKDOWN (FIFO METHOD)
      </div>

      <div style={{
        padding: SPACING.DEFAULT,
        backgroundColor: FINCEPT.PANEL_BG,
        border: BORDERS.STANDARD,
        borderRadius: '2px',
        marginBottom: SPACING.XLARGE
      }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          marginBottom: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: FINCEPT.HEADER_BG,
          borderBottom: BORDERS.ORANGE,
          borderRadius: '2px'
        }}>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>CATEGORY</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>GAINS</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>TAX RATE</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>TAX DUE</div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: SPACING.MEDIUM,
          padding: SPACING.MEDIUM,
          backgroundColor: 'rgba(255,215,0,0.05)',
          borderLeft: `3px solid ${FINCEPT.YELLOW}`,
          borderRadius: '2px',
          marginBottom: SPACING.SMALL,
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{ color: FINCEPT.YELLOW, fontSize: '10px', fontWeight: 700 }}>
            Short-Term Capital Gains (&lt; 1 year)
          </div>
          <div style={{ color: FINCEPT.CYAN, fontSize: '10px', textAlign: 'right' }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '10px', textAlign: 'right' }}>15%</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '10px', textAlign: 'right', fontWeight: 700 }}>
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
          borderRadius: '2px',
          marginBottom: SPACING.SMALL,
          transition: EFFECTS.TRANSITION_STANDARD
        }}>
          <div style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700 }}>
            Long-Term Capital Gains (&gt; 1 year)
          </div>
          <div style={{ color: FINCEPT.CYAN, fontSize: '10px', textAlign: 'right' }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '10px', textAlign: 'right' }}>20%</div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '10px', textAlign: 'right', fontWeight: 700 }}>
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
          borderRadius: '2px',
          marginTop: SPACING.SMALL
        }}>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '11px', fontWeight: 700 }}>TOTAL TAX DUE</div>
          <div></div>
          <div></div>
          <div style={{ color: FINCEPT.ORANGE, fontSize: '13px', textAlign: 'right', fontWeight: 700 }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>
      </div>

      {/* Additional Information */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT }}>
        {/* Transaction Summary */}
        <div>
          <div style={{
            ...COMMON_STYLES.sectionHeader,
            marginBottom: SPACING.MEDIUM
          }}>
            TRANSACTION SUMMARY
          </div>

          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD,
            borderRadius: '2px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Total Transactions:</span>
              <span style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                {transactions.length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Buy Transactions:</span>
              <span style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700 }}>
                {transactions.filter(t => t.transaction_type === 'BUY').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Sell Transactions:</span>
              <span style={{ color: FINCEPT.RED, fontSize: '10px', fontWeight: 700 }}>
                {transactions.filter(t => t.transaction_type === 'SELL').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Current Positions:</span>
              <span style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                {portfolioSummary.holdings.length}
              </span>
            </div>
          </div>
        </div>

        {/* Dividend Information */}
        <div>
          <div style={{
            ...COMMON_STYLES.sectionHeader,
            marginBottom: SPACING.MEDIUM
          }}>
            DIVIDEND INCOME
          </div>

          <div style={{
            padding: SPACING.DEFAULT,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD,
            borderRadius: '2px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: SPACING.SMALL,
              paddingBottom: SPACING.SMALL,
              borderBottom: `1px solid ${FINCEPT.BORDER}`,
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Total Dividends (YTD):</span>
              <span style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700 }}>
                {formatCurrency(totalDividends, currency)}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              transition: EFFECTS.TRANSITION_STANDARD
            }}>
              <span style={{ color: FINCEPT.GRAY, fontSize: '9px', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>Portfolio Yield:</span>
              <span style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
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
        border: BORDERS.ORANGE,
        borderRadius: '2px'
      }}>
        <div style={{
          fontSize: '9px',
          fontWeight: 700,
          color: FINCEPT.ORANGE,
          letterSpacing: '0.5px',
          textTransform: 'uppercase' as const,
          marginBottom: SPACING.MEDIUM
        }}>
          EXPORT OPTIONS
        </div>

        <div style={{ display: 'flex', gap: SPACING.MEDIUM }}>
          <button style={{
            ...COMMON_STYLES.buttonPrimary,
            backgroundColor: FINCEPT.ORANGE,
            color: FINCEPT.DARK_BG
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
            color: FINCEPT.DARK_BG
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
          fontSize: '9px',
          color: FINCEPT.GRAY
        }}>
          Reports are generated for the current tax year. Consult with a tax professional for accurate filing.
        </div>
      </div>
    </div>
  );
};

export default ReportsView;
