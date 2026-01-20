import React from 'react';
import { AlertCircle } from 'lucide-react';
import { ProChartWithToolkit } from '../../trading/charts/ProChartWithToolkit';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';
import { formatNumber, formatLargeNumber, formatPercent, getRecommendationColor, getRecommendationText } from '../utils/formatters';
import type { StockInfo, QuoteData, HistoricalData, ChartPeriod } from '../types';

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
  MAGENTA: '#FF00FF',
  PURPLE: FINCEPT.PURPLE,
};

interface OverviewTabProps {
  currentSymbol: string;
  stockInfo: StockInfo | null;
  quoteData: QuoteData | null;
  historicalData: HistoricalData[];
  chartPeriod: ChartPeriod;
  setChartPeriod: (period: ChartPeriod) => void;
  loading: boolean;
}

export const OverviewTab: React.FC<OverviewTabProps> = ({
  currentSymbol,
  stockInfo,
  quoteData,
  historicalData,
  chartPeriod,
  setChartPeriod,
  loading,
}) => {
  return (
    <div id="research-overview" style={{
      display: 'flex',
      flexDirection: 'column',
      gap: SPACING.MEDIUM,
      padding: SPACING.DEFAULT,
      height: '100%',
      boxSizing: 'border-box',
    }}>
      {/* Top Data Grid - 4 Columns */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: SPACING.MEDIUM,
        flex: '0 0 auto',
        minHeight: '400px',
      }}>
        {/* Column 1 - Trading & Valuation */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
          {/* Today's Trading */}
          <div id="overview-trading" style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
          }}>
            <div style={{
              ...COMMON_STYLES.sectionHeader,
              fontSize: TYPOGRAPHY.BODY,
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              TODAY'S TRADING
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>OPEN</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(quoteData?.open)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>HIGH</span>
                <span style={{ color: COLORS.GREEN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(quoteData?.high)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>LOW</span>
                <span style={{ color: COLORS.RED, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(quoteData?.low)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>PREV CLOSE</span>
                <span style={{ color: COLORS.WHITE, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(quoteData?.previous_close)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>VOLUME</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(quoteData?.volume || 0, 0)}</span>
              </div>
            </div>
          </div>

          {/* Valuation Metrics */}
          <div id="overview-valuation" style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
          }}>
            <div style={{
              color: COLORS.CYAN,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              VALUATION
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>MARKET CAP</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatLargeNumber(stockInfo?.market_cap)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>P/E RATIO</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.pe_ratio)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>FWD P/E</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.forward_pe)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>PEG RATIO</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.peg_ratio)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>P/B RATIO</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.price_to_book)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>DIV YIELD</span>
                <span style={{ color: COLORS.GREEN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>
                  {formatPercent(stockInfo?.dividend_yield)}
                </span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>BETA</span>
                <span style={{ color: COLORS.WHITE, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.beta)}</span>
              </div>
            </div>
          </div>

          {/* Share Statistics */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
            flex: 1,
          }}>
            <div style={{
              color: COLORS.PURPLE,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              SHARE STATS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>SHARES OUT</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatLargeNumber(stockInfo?.shares_outstanding)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>FLOAT</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatLargeNumber(stockInfo?.float_shares)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>INSIDERS</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatPercent(stockInfo?.held_percent_insiders)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>INSTITUTIONS</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatPercent(stockInfo?.held_percent_institutions)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>SHORT %</span>
                <span style={{ color: COLORS.RED, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatPercent(stockInfo?.short_percent_of_float)}</span>
              </div>
            </div>
          </div>
        </div>

        {/* Column 2 - Price Chart */}
        <div id="overview-price-chart" style={{
          display: 'flex',
          flexDirection: 'column',
          gap: SPACING.MEDIUM,
          gridColumn: 'span 2',
          overflow: 'hidden',
          minHeight: 0,
        }}>
          {/* Chart Period Selector */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.SMALL,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
            flexShrink: 0,
          }}>
            <span style={{
              ...COMMON_STYLES.dataLabel,
              marginRight: SPACING.TINY,
            }}>
              PERIOD
            </span>
            {(['1M', '3M', '6M', '1Y', '5Y'] as const).map((period) => (
              <button
                key={period}
                onClick={() => setChartPeriod(period)}
                style={{
                  padding: `${SPACING.TINY} ${SPACING.MEDIUM}`,
                  backgroundColor: chartPeriod === period ? COLORS.ORANGE : 'transparent',
                  border: BORDERS.STANDARD,
                  borderColor: chartPeriod === period ? COLORS.ORANGE : COLORS.BORDER,
                  color: chartPeriod === period ? COLORS.DARK_BG : COLORS.WHITE,
                  fontSize: TYPOGRAPHY.SMALL,
                  cursor: 'pointer',
                  fontWeight: TYPOGRAPHY.BOLD,
                  transition: 'all 0.15s ease',
                }}
                onMouseEnter={(e) => {
                  if (chartPeriod !== period) {
                    e.currentTarget.style.backgroundColor = '#333';
                    e.currentTarget.style.borderColor = COLORS.ORANGE;
                  }
                }}
                onMouseLeave={(e) => {
                  if (chartPeriod !== period) {
                    e.currentTarget.style.backgroundColor = 'transparent';
                    e.currentTarget.style.borderColor = COLORS.BORDER;
                  }
                }}
              >
                {period}
              </button>
            ))}
          </div>

          {/* Chart with Volume */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: '0',
            overflow: 'hidden',
            flex: 1,
            display: 'flex',
            flexDirection: 'column',
            minHeight: 0,
          }}>
            {historicalData && historicalData.length > 0 ? (
              <ProChartWithToolkit
                data={historicalData.map(h => ({
                  time: h.timestamp,
                  open: h.open,
                  high: h.high,
                  low: h.low,
                  close: h.close,
                  volume: h.volume,
                }))}
                symbol={`${currentSymbol} - ${chartPeriod}`}
                height={400}
                showVolume={true}
                showToolbar={true}
                onToolChange={(tool) => {
                  console.log('[EquityResearch] Active tool:', tool);
                }}
              />
            ) : (
              <div style={{
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                height: '100%',
                color: COLORS.GRAY,
                fontSize: TYPOGRAPHY.DEFAULT,
                flexDirection: 'column',
                gap: SPACING.MEDIUM,
              }}>
                {loading ? (
                  <>
                    <div style={{
                      width: '40px',
                      height: '40px',
                      border: '3px solid #404040',
                      borderTop: '3px solid #ea580c',
                      borderRadius: '50%',
                      animation: 'spin 1s linear infinite'
                    }} />
                    <span>Loading chart data...</span>
                  </>
                ) : (
                  <>
                    <AlertCircle size={32} color={COLORS.GRAY} />
                    <span>No chart data available</span>
                  </>
                )}
              </div>
            )}
          </div>
        </div>

        {/* Column 3 - Analyst & Performance */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.MEDIUM }}>
          {/* Analyst Targets */}
          <div id="research-analyst-ratings" style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
          }}>
            <div style={{
              color: COLORS.MAGENTA,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              ANALYST TARGETS
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>HIGH</span>
                <span style={{ color: COLORS.GREEN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(stockInfo?.target_high_price)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>MEAN</span>
                <span style={{ color: COLORS.YELLOW, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(stockInfo?.target_mean_price)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>LOW</span>
                <span style={{ color: COLORS.RED, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(stockInfo?.target_low_price)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>ANALYSTS</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{stockInfo?.number_of_analyst_opinions || 'N/A'}</span>
              </div>
            </div>
          </div>

          {/* 52 Week Range */}
          <div style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
          }}>
            <div style={{
              color: COLORS.YELLOW,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              52 WEEK RANGE
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>HIGH</span>
                <span style={{ color: COLORS.GREEN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(stockInfo?.fifty_two_week_high)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>LOW</span>
                <span style={{ color: COLORS.RED, fontWeight: TYPOGRAPHY.SEMIBOLD }}>${formatNumber(stockInfo?.fifty_two_week_low)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>AVG VOL</span>
                <span style={{ color: COLORS.CYAN, fontWeight: TYPOGRAPHY.SEMIBOLD }}>{formatNumber(stockInfo?.average_volume, 0)}</span>
              </div>
            </div>
          </div>

          {/* Profitability */}
          <div id="overview-profitability" style={{
            ...COMMON_STYLES.panel,
            padding: SPACING.MEDIUM,
            flex: 1,
          }}>
            <div style={{
              color: COLORS.GREEN,
              fontSize: TYPOGRAPHY.BODY,
              fontWeight: TYPOGRAPHY.BOLD,
              letterSpacing: TYPOGRAPHY.WIDE,
              textTransform: 'uppercase',
              paddingBottom: SPACING.TINY,
              marginBottom: SPACING.SMALL,
              borderBottom: BORDERS.STANDARD,
            }}>
              PROFITABILITY
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.SMALL }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ ...COMMON_STYLES.dataLabel }}>GROSS MARGIN</span>
                <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.gross_margins)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>OPER. MARGIN:</span>
                <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.operating_margins)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>PROFIT MARGIN:</span>
                <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.profit_margins)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>ROA:</span>
                <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_assets)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>ROE:</span>
                <span style={{ color: COLORS.CYAN }}>{formatPercent(stockInfo?.return_on_equity)}</span>
              </div>
            </div>
          </div>

          {/* Growth */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '6px',
          }}>
            <div style={{ color: COLORS.BLUE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
              GROWTH RATES
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: '9px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>REVENUE:</span>
                <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.revenue_growth)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>EARNINGS:</span>
                <span style={{ color: COLORS.GREEN }}>{formatPercent(stockInfo?.earnings_growth)}</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Bottom Section - Company Overview & Key Info */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '2fr 1fr',
        gap: '6px',
        flex: '0 0 auto',
        minHeight: '120px',
      }}>
        {/* Company Description */}
        {stockInfo?.description && stockInfo.description !== 'N/A' && (
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '6px',
            display: 'flex',
            flexDirection: 'column',
            minHeight: 0,
          }}>
            <div style={{ color: COLORS.CYAN, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px', flexShrink: 0 }}>
              COMPANY OVERVIEW
            </div>
            <div style={{
              color: COLORS.WHITE,
              fontSize: '9px',
              lineHeight: '1.5',
              overflow: 'auto',
              flex: 1,
              minHeight: 0,
            }} className="custom-scrollbar">
              {stockInfo.description}
            </div>
          </div>
        )}

        {/* Company Info & Financial Health */}
        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {/* Company Info */}
          {stockInfo && (
            <div style={{
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              padding: '6px',
            }}>
              <div style={{ color: COLORS.WHITE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
                COMPANY INFO
              </div>
              <div style={{ display: 'flex', flexDirection: 'column', gap: '3px', fontSize: '9px' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: COLORS.GRAY }}>EMPLOYEES:</span>
                  <span style={{ color: COLORS.CYAN }}>{formatNumber(stockInfo.employees, 0)}</span>
                </div>
                {stockInfo.website && stockInfo.website !== 'N/A' && (
                  <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                    <span style={{ color: COLORS.GRAY }}>WEBSITE:</span>
                    <span style={{ color: COLORS.BLUE, fontSize: '8px' }}>{stockInfo.website.substring(0, 25)}</span>
                  </div>
                )}
                <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                  <span style={{ color: COLORS.GRAY }}>CURRENCY:</span>
                  <span style={{ color: COLORS.CYAN }}>{stockInfo.currency || 'N/A'}</span>
                </div>
              </div>
            </div>
          )}

          {/* Key Financial Health */}
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: '6px',
          }}>
            <div style={{ color: COLORS.ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '6px', borderBottom: `1px solid ${COLORS.BORDER}`, paddingBottom: '3px' }}>
              FINANCIAL HEALTH
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '3px', fontSize: '9px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>CASH:</span>
                <span style={{ color: COLORS.GREEN }}>{formatLargeNumber(stockInfo?.total_cash)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>DEBT:</span>
                <span style={{ color: COLORS.RED }}>{formatLargeNumber(stockInfo?.total_debt)}</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <span style={{ color: COLORS.GRAY }}>FREE CF:</span>
                <span style={{ color: COLORS.CYAN }}>{formatLargeNumber(stockInfo?.free_cashflow)}</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default OverviewTab;
