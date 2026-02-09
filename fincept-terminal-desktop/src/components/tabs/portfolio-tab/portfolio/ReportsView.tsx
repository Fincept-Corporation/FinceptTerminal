import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary, Transaction } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent, calculateTaxLiability } from './utils';

interface ReportsViewProps {
  portfolioSummary: PortfolioSummary;
  transactions: Transaction[];
}

const ReportsView: React.FC<ReportsViewProps> = ({ portfolioSummary, transactions }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
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
        {t('reports.taxReports')}
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
          backgroundColor: colors.panel,
          border: `1px solid ${colors.primary}`,
          borderRadius: '2px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{
            fontSize: fontSize.tiny,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('reports.totalTaxLiability')}</div>
          <div style={{
            color: colors.primary,
            fontSize: fontSize.body,
            fontWeight: 700,
            fontFamily
          }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          border: `1px solid ${colors.success}`,
          borderRadius: '2px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{
            fontSize: fontSize.tiny,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('reports.longTermGains')}</div>
          <div style={{
            color: colors.success,
            fontSize: fontSize.body,
            fontWeight: 700,
            fontFamily
          }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            marginTop: '4px'
          }}>
            Tax: {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          border: `1px solid var(--ft-color-warning, #FFD700)`,
          borderRadius: '2px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{
            fontSize: fontSize.tiny,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('reports.shortTermGains')}</div>
          <div style={{
            color: 'var(--ft-color-warning, #FFD700)',
            fontSize: fontSize.body,
            fontWeight: 700,
            fontFamily
          }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            marginTop: '4px'
          }}>
            Tax: {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          padding: '12px',
          backgroundColor: colors.panel,
          border: `1px solid var(--ft-color-accent, #00E5FF)`,
          borderRadius: '2px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{
            fontSize: fontSize.tiny,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const
          }}>{t('reports.unrealizedGains')}</div>
          <div style={{
            color: portfolioSummary.total_unrealized_pnl >= 0 ? colors.success : colors.alert,
            fontSize: fontSize.body,
            fontWeight: 700,
            fontFamily
          }}>
            {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
          </div>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            marginTop: '4px'
          }}>
            Not taxable until sold
          </div>
        </div>
      </div>

      {/* Tax Breakdown */}
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '12px',
      }}>
        {t('reports.taxBreakdown')}
      </div>

      <div style={{
        padding: '12px',
        backgroundColor: colors.panel,
        border: '1px solid var(--ft-color-border, #2A2A2A)',
        borderRadius: '2px',
        marginBottom: '24px'
      }}>
        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          marginBottom: '12px',
          padding: '12px',
          backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          borderBottom: `1px solid ${colors.primary}`,
          borderRadius: '2px'
        }}>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.category')}</div>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>{t('reports.gains')}</div>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>{t('reports.taxRate')}</div>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const, textAlign: 'right' }}>{t('reports.taxDue')}</div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(255,215,0,0.05)',
          borderLeft: `3px solid var(--ft-color-warning, #FFD700)`,
          borderRadius: '2px',
          marginBottom: '8px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: fontSize.tiny, fontWeight: 700 }}>
            Short-Term Capital Gains (&lt; 1 year)
          </div>
          <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny, textAlign: 'right' }}>
            {formatCurrency(taxCalculation.shortTermGains, currency)}
          </div>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, textAlign: 'right' }}>15%</div>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, textAlign: 'right', fontWeight: 700 }}>
            {formatCurrency(taxCalculation.shortTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(0,214,111,0.05)',
          borderLeft: `3px solid ${colors.success}`,
          borderRadius: '2px',
          marginBottom: '8px',
          transition: 'all 0.2s ease'
        }}>
          <div style={{ color: colors.success, fontSize: fontSize.tiny, fontWeight: 700 }}>
            Long-Term Capital Gains (&gt; 1 year)
          </div>
          <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny, textAlign: 'right' }}>
            {formatCurrency(taxCalculation.longTermGains, currency)}
          </div>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, textAlign: 'right' }}>20%</div>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, textAlign: 'right', fontWeight: 700 }}>
            {formatCurrency(taxCalculation.longTermTax, currency)}
          </div>
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: '2fr 1fr 1fr 1fr',
          gap: '12px',
          padding: '12px',
          backgroundColor: 'rgba(255,136,0,0.1)',
          borderTop: `2px solid ${colors.primary}`,
          borderRadius: '2px',
          marginTop: '8px'
        }}>
          <div style={{ color: colors.primary, fontSize: fontSize.tiny, fontWeight: 700 }}>{t('reports.totalTaxDue')}</div>
          <div></div>
          <div></div>
          <div style={{ color: colors.primary, fontSize: fontSize.small, textAlign: 'right', fontWeight: 700 }}>
            {formatCurrency(taxCalculation.totalTax, currency)}
          </div>
        </div>
      </div>

      {/* Additional Information */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        {/* Transaction Summary */}
        <div>
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
            color: colors.primary,
            fontSize: fontSize.body,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const,
            marginBottom: '12px',
          }}>
            {t('reports.transactionSummary')}
          </div>

          <div style={{
            padding: '12px',
            backgroundColor: colors.panel,
            border: '1px solid var(--ft-color-border, #2A2A2A)',
            borderRadius: '2px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.totalTransactions')}</span>
              <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny, fontWeight: 700 }}>
                {transactions.length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.buyTransactions')}</span>
              <span style={{ color: colors.success, fontSize: fontSize.tiny, fontWeight: 700 }}>
                {transactions.filter(t => t.transaction_type === 'BUY').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.sellTransactions')}</span>
              <span style={{ color: colors.alert, fontSize: fontSize.tiny, fontWeight: 700 }}>
                {transactions.filter(t => t.transaction_type === 'SELL').length}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.currentPositions')}</span>
              <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny, fontWeight: 700 }}>
                {portfolioSummary.holdings.length}
              </span>
            </div>
          </div>
        </div>

        {/* Dividend Information */}
        <div>
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
            color: colors.primary,
            fontSize: fontSize.body,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const,
            marginBottom: '12px',
          }}>
            {t('reports.dividendIncome')}
          </div>

          <div style={{
            padding: '12px',
            backgroundColor: colors.panel,
            border: '1px solid var(--ft-color-border, #2A2A2A)',
            borderRadius: '2px'
          }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.totalDividends')}</span>
              <span style={{ color: colors.success, fontSize: fontSize.tiny, fontWeight: 700 }}>
                {formatCurrency(totalDividends, currency)}
              </span>
            </div>

            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              transition: 'all 0.2s ease'
            }}>
              <span style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' as const }}>{t('reports.portfolioYield')}</span>
              <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.tiny, fontWeight: 700 }}>
                {dividendYield.toFixed(2)}%
              </span>
            </div>
          </div>
        </div>
      </div>

      {/* Export Options */}
      <div style={{
        marginTop: '24px',
        padding: '12px',
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: `1px solid ${colors.primary}`,
        borderRadius: '2px'
      }}>
        <div style={{
          fontSize: fontSize.tiny,
          fontWeight: 700,
          color: colors.primary,
          letterSpacing: '0.5px',
          textTransform: 'uppercase' as const,
          marginBottom: '12px'
        }}>
          {t('reports.exportOptions')}
        </div>

        <div style={{ display: 'flex', gap: '12px' }}>
          <button style={{
            padding: '8px 16px',
            backgroundColor: colors.primary,
            color: colors.background,
            border: 'none',
            borderRadius: '2px',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            cursor: 'pointer',
            transition: 'all 0.2s ease',
            textTransform: 'uppercase',
            letterSpacing: '0.5px',
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            {t('reports.exportTaxReport')}
          </button>

          <button style={{
            padding: '8px 16px',
            backgroundColor: colors.success,
            color: colors.background,
            border: 'none',
            borderRadius: '2px',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            cursor: 'pointer',
            transition: 'all 0.2s ease',
            textTransform: 'uppercase',
            letterSpacing: '0.5px',
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            {t('reports.exportTransactions')}
          </button>

          <button style={{
            padding: '8px 16px',
            backgroundColor: 'var(--ft-color-accent, #00E5FF)',
            color: colors.background,
            border: 'none',
            borderRadius: '2px',
            fontSize: fontSize.tiny,
            fontWeight: 700,
            cursor: 'pointer',
            transition: 'all 0.2s ease',
            textTransform: 'uppercase',
            letterSpacing: '0.5px',
          }}
            onMouseEnter={(e) => {
              e.currentTarget.style.opacity = '0.85';
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.opacity = '1';
            }}>
            {t('reports.exportHoldings')}
          </button>
        </div>

        <div style={{
          marginTop: '12px',
          fontSize: fontSize.tiny,
          color: colors.textMuted
        }}>
          Reports are generated for the current tax year. Consult with a tax professional for accurate filing.
        </div>
      </div>
    </div>
  );
};

export default ReportsView;
