import React from 'react';
import { FinancialAnalysisPanel } from '../components/FinancialAnalysisPanel';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { formatNumber, formatLargeNumber, formatPercent } from '../utils/formatters';
import type { StockInfo } from '../types';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  BORDER: FINCEPT.BORDER,
  PURPLE: FINCEPT.PURPLE,
};

interface AnalysisTabProps {
  currentSymbol: string;
  stockInfo: StockInfo | null;
}

export const AnalysisTab: React.FC<AnalysisTabProps> = ({
  currentSymbol,
  stockInfo,
}) => {
  return (
    <div id="analysis-tab-content" style={{ padding: SPACING.DEFAULT }}>
      {/* Financial Analysis Panel - CFA-Compliant Analysis */}
      <div id="analysis-cfa-panel" style={{ marginBottom: SPACING.MEDIUM }}>
        <FinancialAnalysisPanel
          ticker={currentSymbol}
          companyName={stockInfo?.company_name || currentSymbol}
          stockInfo={stockInfo}
        />
      </div>

      {/* Financial Metrics Grid - 2x2 */}
      <div id="analysis-metrics-grid" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM }}>
        {/* Financial Health */}
        <div id="analysis-financial-health" style={{
          ...COMMON_STYLES.panel,
          padding: SPACING.LARGE,
        }}>
          <div style={{
            ...COMMON_STYLES.sectionHeader,
            fontSize: TYPOGRAPHY.SUBHEADING,
            paddingBottom: SPACING.SMALL,
            marginBottom: SPACING.LARGE,
            borderBottom: `2px solid ${COLORS.ORANGE}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <div style={{
              width: '4px',
              height: '16px',
              backgroundColor: COLORS.ORANGE,
            }} />
            FINANCIAL HEALTH
          </div>
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: SPACING.LARGE,
          }}>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                TOTAL CASH
              </div>
              <div style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.total_cash)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                TOTAL DEBT
              </div>
              <div style={{
                color: COLORS.RED,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.total_debt)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                FREE CASHFLOW
              </div>
              <div style={{
                color: COLORS.CYAN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.free_cashflow)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                OPERATING CF
              </div>
              <div style={{
                color: COLORS.BLUE,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.operating_cashflow)}
              </div>
            </div>
          </div>
        </div>

        {/* Enterprise Value */}
        <div style={{
          ...COMMON_STYLES.panel,
          padding: SPACING.LARGE,
        }}>
          <div style={{
            color: COLORS.CYAN,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase',
            paddingBottom: SPACING.SMALL,
            marginBottom: SPACING.LARGE,
            borderBottom: `2px solid ${COLORS.CYAN}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <div style={{
              width: '4px',
              height: '16px',
              backgroundColor: COLORS.CYAN,
            }} />
            ENTERPRISE VALUE
          </div>
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: SPACING.LARGE,
          }}>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                ENTERPRISE VALUE
              </div>
              <div style={{
                color: COLORS.YELLOW,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.enterprise_value)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                EV/REVENUE
              </div>
              <div style={{
                color: COLORS.CYAN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.enterprise_to_revenue)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                EV/EBITDA
              </div>
              <div style={{
                color: COLORS.CYAN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.enterprise_to_ebitda)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                BOOK VALUE
              </div>
              <div style={{
                color: COLORS.WHITE,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                ${formatNumber(stockInfo?.book_value)}
              </div>
            </div>
          </div>
        </div>

        {/* Revenue & Profits */}
        <div style={{
          ...COMMON_STYLES.panel,
          padding: SPACING.LARGE,
        }}>
          <div style={{
            color: COLORS.GREEN,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase',
            paddingBottom: SPACING.SMALL,
            marginBottom: SPACING.LARGE,
            borderBottom: `2px solid ${COLORS.GREEN}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <div style={{
              width: '4px',
              height: '16px',
              backgroundColor: COLORS.GREEN,
            }} />
            REVENUE & PROFITS
          </div>
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: SPACING.LARGE,
          }}>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                TOTAL REVENUE
              </div>
              <div style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.total_revenue)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                REVENUE/SHARE
              </div>
              <div style={{
                color: COLORS.CYAN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                ${formatNumber(stockInfo?.revenue_per_share)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                GROSS PROFITS
              </div>
              <div style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatLargeNumber(stockInfo?.gross_profits)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                EBITDA MARGINS
              </div>
              <div style={{
                color: COLORS.YELLOW,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatPercent(stockInfo?.ebitda_margins)}
              </div>
            </div>
          </div>
        </div>

        {/* Key Ratios Summary */}
        <div style={{
          ...COMMON_STYLES.panel,
          padding: SPACING.LARGE,
        }}>
          <div style={{
            color: COLORS.PURPLE,
            fontSize: TYPOGRAPHY.SUBHEADING,
            fontWeight: TYPOGRAPHY.BOLD,
            letterSpacing: TYPOGRAPHY.WIDE,
            textTransform: 'uppercase',
            paddingBottom: SPACING.SMALL,
            marginBottom: SPACING.LARGE,
            borderBottom: `2px solid ${COLORS.PURPLE}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <div style={{
              width: '4px',
              height: '16px',
              backgroundColor: COLORS.PURPLE,
            }} />
            KEY RATIOS
          </div>
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: SPACING.LARGE,
          }}>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                P/E RATIO
              </div>
              <div style={{
                color: COLORS.WHITE,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.pe_ratio)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                PEG RATIO
              </div>
              <div style={{
                color: COLORS.WHITE,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.peg_ratio)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                ROE
              </div>
              <div style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatPercent(stockInfo?.return_on_equity)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                ROA
              </div>
              <div style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatPercent(stockInfo?.return_on_assets)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                BETA
              </div>
              <div style={{
                color: COLORS.CYAN,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.beta)}
              </div>
            </div>
            <div>
              <div style={{
                ...COMMON_STYLES.dataLabel,
                marginBottom: SPACING.TINY,
              }}>
                SHORT RATIO
              </div>
              <div style={{
                color: COLORS.RED,
                fontSize: TYPOGRAPHY.HEADING,
                fontWeight: TYPOGRAPHY.BOLD,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {formatNumber(stockInfo?.short_ratio)}
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default AnalysisTab;
