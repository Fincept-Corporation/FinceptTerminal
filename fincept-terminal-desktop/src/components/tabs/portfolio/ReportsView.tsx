import React from 'react';
import { PortfolioSummary, Transaction } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent, calculateTaxLiability } from './utils';

interface ReportsViewProps {
  portfolioSummary: PortfolioSummary;
  transactions: Transaction[];
}

const ReportsView: React.FC<ReportsViewProps> = ({ portfolioSummary, transactions }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

  // Calculate tax liability
  const taxCalculation = calculateTaxLiability(transactions, 0.15, 0.20, 365);

  // Calculate dividend info (mock data)
  const totalDividends = 0; // Would come from dividend_transactions table
  const dividendYield = 0;

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        TAX REPORTS & STATEMENTS
      </div>

      {/* Summary Cards */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: '12px',
        marginBottom: '24px'
      }}>
        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>TOTAL TAX LIABILITY</div>
          <div style={{ color: ORANGE, fontSize: '16px', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(0,200,0,0.1)',
          border: `1px solid ${GREEN}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>LONG-TERM GAINS</div>
          <div style={{ color: GREEN, fontSize: '16px', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
            Tax: {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(255,255,0,0.1)',
          border: `1px solid ${YELLOW}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>SHORT-TERM GAINS</div>
          <div style={{ color: YELLOW, fontSize: '16px', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
            Tax: {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: 'rgba(0,255,255,0.1)',
          border: `1px solid ${CYAN}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>UNREALIZED GAINS</div>
          <div style={{
            color: portfolioSummary.total_unrealized_pnl >= 0 ? GREEN : RED,
            fontSize: '16px',
            fontWeight: 'bold'
          }}>
            {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
            Not taxable until sold
          </div>
        </div>
      </div>

      {/* Tax Breakdown */}
      <div style={{
        color: ORANGE,
        fontSize: '11px',
        fontWeight: 'bold',
        marginBottom: '12px',
        paddingBottom: '4px',
        borderBottom: `1px solid ${ORANGE}`
      }}>
        TAX BREAKDOWN (FIFO METHOD)
      </div>

      <div style={{
        padding: '16px',
        backgroundColor: 'rgba(255,255,255,0.02)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px',
        marginBottom: '24px'
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr 1fr 1fr', gap: '12px', marginBottom: '12px' }}>
          <div style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold' }}>CATEGORY</div>
          <div style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold', textAlign: 'right' }}>GAINS</div>
          <div style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold', textAlign: 'right' }}>TAX RATE</div>
          <div style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold', textAlign: 'right' }}>TAX DUE</div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(255,255,0,0.05)',
          borderLeft: `3px solid ${YELLOW}`,
          marginBottom: '8px'
        }}>
          <div style={{ color: YELLOW, fontSize: '10px', fontWeight: 'bold' }}>
            Short-Term Capital Gains (&lt; 1 year)
          </div>
          <div style={{ color: WHITE, fontSize: '10px', textAlign: 'right' }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '10px', textAlign: 'right' }}>15%</div>
          <div style={{ color: ORANGE, fontSize: '10px', textAlign: 'right', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(0,200,0,0.05)',
          borderLeft: `3px solid ${GREEN}`,
          marginBottom: '8px'
        }}>
          <div style={{ color: GREEN, fontSize: '10px', fontWeight: 'bold' }}>
            Long-Term Capital Gains (&gt; 1 year)
          </div>
          <div style={{ color: WHITE, fontSize: '10px', textAlign: 'right' }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '10px', textAlign: 'right' }}>20%</div>
          <div style={{ color: ORANGE, fontSize: '10px', textAlign: 'right', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          borderTop: `2px solid ${ORANGE}`,
          marginTop: '8px'
        }}>
          <div style={{ color: ORANGE, fontSize: '11px', fontWeight: 'bold' }}>TOTAL TAX DUE</div>
          <div></div>
          <div></div>
          <div style={{ color: ORANGE, fontSize: '12px', textAlign: 'right', fontWeight: 'bold' }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>
      </div>

      {/* Additional Information */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px' }}>
        {/* Transaction Summary */}
        <div>
          <div style={{
            color: ORANGE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '12px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${ORANGE}`
          }}>
            TRANSACTION SUMMARY
          </div>

          <div style={{
            padding: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Total Transactions:</span>
              <span style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold' }}>
                {transactions.length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Buy Transactions:</span>
              <span style={{ color: GREEN, fontSize: '10px', fontWeight: 'bold' }}>
                {transactions.filter(t => t.transaction_type === 'BUY').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Sell Transactions:</span>
              <span style={{ color: RED, fontSize: '10px', fontWeight: 'bold' }}>
                {transactions.filter(t => t.transaction_type === 'SELL').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between'
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Current Positions:</span>
              <span style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                {portfolioSummary.holdings.length}
              </span>
            </div>
          </div>
        </div>

        {/* Dividend Information */}
        <div>
          <div style={{
            color: ORANGE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '12px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${ORANGE}`
          }}>
            DIVIDEND INCOME
          </div>

          <div style={{
            padding: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Total Dividends (YTD):</span>
              <span style={{ color: GREEN, fontSize: '10px', fontWeight: 'bold' }}>
                {formatCurrency(totalDividends, currency)}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: `1px solid rgba(120,120,120,0.3)`
            }}>
              <span style={{ color: GRAY, fontSize: '10px' }}>Portfolio Yield:</span>
              <span style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                {dividendYield.toFixed(2)}%
              </span>
            </div>

            <div style={{
              padding: '8px',
              backgroundColor: 'rgba(120,120,120,0.1)',
              borderRadius: '2px',
              marginTop: '8px'
            }}>
              <div style={{ color: GRAY, fontSize: '9px', textAlign: 'center' }}>
                Dividend tracking coming soon...
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Export Options */}
      <div style={{
        marginTop: '24px',
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
          EXPORT OPTIONS
        </div>

        <div style={{ display: 'flex', gap: '12px' }}>
          <button style={{
            padding: '10px 16px',
            backgroundColor: ORANGE,
            color: 'black',
            border: 'none',
            borderRadius: '4px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}>
            EXPORT TAX REPORT (PDF)
          </button>

          <button style={{
            padding: '10px 16px',
            backgroundColor: GREEN,
            color: 'black',
            border: 'none',
            borderRadius: '4px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}>
            EXPORT TRANSACTIONS (CSV)
          </button>

          <button style={{
            padding: '10px 16px',
            backgroundColor: CYAN,
            color: 'black',
            border: 'none',
            borderRadius: '4px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: 'pointer'
          }}>
            EXPORT HOLDINGS (XLSX)
          </button>
        </div>

        <div style={{ marginTop: '12px', fontSize: '8px', color: GRAY }}>
          Reports are generated for the current tax year. Consult with a tax professional for accurate filing.
        </div>
      </div>
    </div>
  );
};

export default ReportsView;
