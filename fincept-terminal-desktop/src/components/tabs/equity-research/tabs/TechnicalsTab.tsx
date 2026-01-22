import React, { useMemo, useState } from 'react';
import { IndicatorBox } from '../components/IndicatorBox';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../../portfolio-tab/finceptStyles';
import type { HistoricalData, TechnicalsData } from '../types';
import { TrendingUp, TrendingDown, Minus, Activity, BarChart3, Waves, Volume2, Percent, ChevronDown, ChevronRight, Target, AlertTriangle, Gauge, Zap } from 'lucide-react';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  MUTED: FINCEPT.MUTED,
  BLUE: FINCEPT.BLUE,
  CYAN: FINCEPT.CYAN,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  PURPLE: FINCEPT.PURPLE,
};

interface TechnicalsTabProps {
  technicalsData: TechnicalsData | null;
  technicalsLoading: boolean;
  historicalData: HistoricalData[];
}

// Helper to get signal from indicator
const getIndicatorSignal = (name: string, latestValue: number, prevValue: number): 'BUY' | 'SELL' | 'NEUTRAL' => {
  const nameUpper = name.toUpperCase();

  // RSI
  if (nameUpper.includes('RSI')) {
    if (latestValue <= 30) return 'BUY';
    if (latestValue >= 70) return 'SELL';
    return 'NEUTRAL';
  }

  // Stochastic
  if (nameUpper.includes('STOCH') && !nameUpper.includes('RSI')) {
    if (latestValue <= 20) return 'BUY';
    if (latestValue >= 80) return 'SELL';
    return 'NEUTRAL';
  }

  // MACD
  if (nameUpper.includes('MACD') && !nameUpper.includes('SIGNAL') && !nameUpper.includes('DIFF')) {
    if (prevValue < 0 && latestValue > 0) return 'BUY';
    if (prevValue > 0 && latestValue < 0) return 'SELL';
    return 'NEUTRAL';
  }

  // CCI
  if (nameUpper.includes('CCI')) {
    if (latestValue <= -100) return 'BUY';
    if (latestValue >= 100) return 'SELL';
    return 'NEUTRAL';
  }

  // Williams %R
  if (nameUpper.includes('WILLIAMS')) {
    if (latestValue <= -80) return 'BUY';
    if (latestValue >= -20) return 'SELL';
    return 'NEUTRAL';
  }

  // MFI
  if (nameUpper === 'MFI') {
    if (latestValue <= 20) return 'BUY';
    if (latestValue >= 80) return 'SELL';
    return 'NEUTRAL';
  }

  // ADX (trend strength)
  if (nameUpper.includes('ADX') && !nameUpper.includes('POS') && !nameUpper.includes('NEG')) {
    if (latestValue > 25) return 'BUY'; // Strong trend
    return 'NEUTRAL';
  }

  // Bollinger Bands %B
  if (nameUpper.includes('BB_PBAND')) {
    if (latestValue < 0.2) return 'BUY';
    if (latestValue > 0.8) return 'SELL';
    return 'NEUTRAL';
  }

  // CMF
  if (nameUpper.includes('CMF')) {
    if (latestValue > 0.1) return 'BUY';
    if (latestValue < -0.1) return 'SELL';
    return 'NEUTRAL';
  }

  // ROC
  if (nameUpper.includes('ROC')) {
    if (latestValue > 5) return 'BUY';
    if (latestValue < -5) return 'SELL';
    return 'NEUTRAL';
  }

  return 'NEUTRAL';
};

// Key indicators for quick view
const KEY_INDICATORS = ['RSI', 'MACD', 'STOCH_K', 'ADX', 'CCI', 'MFI', 'BB_PBAND', 'WILLIAMS_R'];

export const TechnicalsTab: React.FC<TechnicalsTabProps> = ({
  technicalsData,
  technicalsLoading,
  historicalData,
}) => {
  const [expandedCategories, setExpandedCategories] = useState<Record<string, boolean>>({
    trend: true,
    momentum: true,
    volatility: false,
    volume: false,
    others: false,
  });

  // Calculate signal summary
  const signalSummary = useMemo(() => {
    if (!technicalsData?.data || !technicalsData?.indicator_columns) {
      return { buy: 0, sell: 0, neutral: 0, total: 0, recommendation: 'NEUTRAL' as const };
    }

    let buy = 0, sell = 0, neutral = 0;
    const allIndicators = [
      ...(technicalsData.indicator_columns.trend || []),
      ...(technicalsData.indicator_columns.momentum || []),
      ...(technicalsData.indicator_columns.volatility || []),
      ...(technicalsData.indicator_columns.volume || []),
      ...(technicalsData.indicator_columns.others || []),
    ];

    allIndicators.forEach((indicator) => {
      const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
      if (values.length < 2) return;

      const latestValue = values[values.length - 1];
      const prevValue = values[values.length - 2];
      const signal = getIndicatorSignal(indicator, latestValue, prevValue);

      if (signal === 'BUY') buy++;
      else if (signal === 'SELL') sell++;
      else neutral++;
    });

    const total = buy + sell + neutral;
    let recommendation: 'STRONG BUY' | 'BUY' | 'NEUTRAL' | 'SELL' | 'STRONG SELL' = 'NEUTRAL';

    if (total > 0) {
      const buyPercent = (buy / total) * 100;
      const sellPercent = (sell / total) * 100;

      if (buyPercent >= 70) recommendation = 'STRONG BUY';
      else if (buyPercent >= 50) recommendation = 'BUY';
      else if (sellPercent >= 70) recommendation = 'STRONG SELL';
      else if (sellPercent >= 50) recommendation = 'SELL';
    }

    return { buy, sell, neutral, total, recommendation };
  }, [technicalsData]);

  // Get key indicator values
  const keyIndicatorValues = useMemo(() => {
    if (!technicalsData?.data || !technicalsData?.indicator_columns) return [];

    const allIndicators = [
      ...(technicalsData.indicator_columns.trend || []),
      ...(technicalsData.indicator_columns.momentum || []),
      ...(technicalsData.indicator_columns.volatility || []),
      ...(technicalsData.indicator_columns.volume || []),
    ];

    return KEY_INDICATORS.map((key) => {
      const indicator = allIndicators.find((ind) => ind.toUpperCase().includes(key));
      if (!indicator) return null;

      const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
      if (values.length < 2) return null;

      const latestValue = values[values.length - 1];
      const prevValue = values[values.length - 2];
      const signal = getIndicatorSignal(indicator, latestValue, prevValue);

      return { name: indicator, value: latestValue, signal };
    }).filter(Boolean);
  }, [technicalsData]);

  if (technicalsLoading) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: SPACING.LARGE,
        fontFamily: TYPOGRAPHY.MONO,
      }}>
        <div style={{
          width: '40px',
          height: '40px',
          border: `2px solid ${COLORS.BORDER}`,
          borderTop: `2px solid ${COLORS.ORANGE}`,
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <div style={{
          color: COLORS.ORANGE,
          fontSize: TYPOGRAPHY.SUBHEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          textTransform: 'uppercase',
          letterSpacing: TYPOGRAPHY.WIDE,
        }}>
          COMPUTING TECHNICAL INDICATORS
        </div>
        <div style={{
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.SMALL,
          textTransform: 'uppercase',
        }}>
          ANALYZING {historicalData.length} DATA POINTS
        </div>
      </div>
    );
  }

  if (!technicalsData) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: SPACING.MEDIUM,
        fontFamily: TYPOGRAPHY.MONO,
      }}>
        <AlertTriangle size={32} color={COLORS.RED} />
        <div style={{
          color: COLORS.RED,
          fontSize: TYPOGRAPHY.SUBHEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          textTransform: 'uppercase',
        }}>
          NO TECHNICAL DATA AVAILABLE
        </div>
        <div style={{
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.SMALL,
          textTransform: 'uppercase',
        }}>
          WAITING FOR HISTORICAL DATA
        </div>
      </div>
    );
  }

  if (!technicalsData.indicator_columns || !technicalsData.data || !Array.isArray(technicalsData.data) || technicalsData.data.length === 0) {
    return (
      <div style={{
        ...COMMON_STYLES.panel,
        padding: SPACING.LARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO,
      }}>
        <div style={{
          color: COLORS.YELLOW,
          fontSize: TYPOGRAPHY.SUBHEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.MEDIUM,
          textTransform: 'uppercase',
        }}>
          NO INDICATOR DATA
        </div>
        <div style={{
          fontSize: TYPOGRAPHY.SMALL,
          color: COLORS.GRAY,
          textTransform: 'uppercase',
        }}>
          Technical indicators could not be computed. Try selecting a different symbol.
        </div>
      </div>
    );
  }

  const toggleCategory = (category: string) => {
    setExpandedCategories(prev => ({ ...prev, [category]: !prev[category] }));
  };

  const renderIndicatorCategory = (indicators: string[] | undefined, color: string, category: string, icon: React.ReactNode, categoryKey: string) => {
    if (!indicators || !Array.isArray(indicators) || indicators.length === 0) return null;

    const isExpanded = expandedCategories[categoryKey];

    // Count signals in this category
    let buyCount = 0, sellCount = 0, neutralCount = 0;
    indicators.forEach((indicator) => {
      const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
      if (values.length >= 2) {
        const signal = getIndicatorSignal(indicator, values[values.length - 1], values[values.length - 2]);
        if (signal === 'BUY') buyCount++;
        else if (signal === 'SELL') sellCount++;
        else neutralCount++;
      }
    });

    return (
      <div key={categoryKey} style={{ marginBottom: SPACING.SMALL }}>
        {/* Category Header */}
        <div
          onClick={() => toggleCategory(categoryKey)}
          style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            padding: `${SPACING.MEDIUM} ${SPACING.DEFAULT}`,
            backgroundColor: COLORS.HEADER_BG,
            borderLeft: `2px solid ${color}`,
            borderBottom: isExpanded ? 'none' : BORDERS.STANDARD,
            cursor: 'pointer',
            transition: EFFECTS.TRANSITION_FAST,
            fontFamily: TYPOGRAPHY.MONO,
          }}
          onMouseEnter={(e) => {
            e.currentTarget.style.backgroundColor = COLORS.HOVER;
          }}
          onMouseLeave={(e) => {
            e.currentTarget.style.backgroundColor = COLORS.HEADER_BG;
          }}
        >
          <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.MEDIUM }}>
            {isExpanded ? <ChevronDown size={14} color={color} /> : <ChevronRight size={14} color={color} />}
            {icon}
            <span style={{
              color: COLORS.WHITE,
              fontSize: TYPOGRAPHY.DEFAULT,
              fontWeight: TYPOGRAPHY.BOLD,
              textTransform: 'uppercase',
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              {category}
            </span>
            <span style={{
              color: COLORS.MUTED,
              fontSize: TYPOGRAPHY.TINY,
              backgroundColor: COLORS.DARK_BG,
              padding: `${SPACING.TINY} ${SPACING.SMALL}`,
              border: BORDERS.STANDARD,
              textTransform: 'uppercase',
            }}>
              {indicators.length} INDICATORS
            </span>
          </div>

          {/* Mini Signal Summary for Category */}
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            {buyCount > 0 && (
              <span style={{
                color: COLORS.GREEN,
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                backgroundColor: `${COLORS.GREEN}15`,
                borderLeft: `2px solid ${COLORS.GREEN}`,
                padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                textTransform: 'uppercase',
              }}>
                {buyCount} BUY
              </span>
            )}
            {sellCount > 0 && (
              <span style={{
                color: COLORS.RED,
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                backgroundColor: `${COLORS.RED}15`,
                borderLeft: `2px solid ${COLORS.RED}`,
                padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                textTransform: 'uppercase',
              }}>
                {sellCount} SELL
              </span>
            )}
            {neutralCount > 0 && (
              <span style={{
                color: COLORS.GRAY,
                fontSize: TYPOGRAPHY.TINY,
                fontWeight: TYPOGRAPHY.BOLD,
                backgroundColor: `${COLORS.GRAY}15`,
                borderLeft: `2px solid ${COLORS.GRAY}`,
                padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                textTransform: 'uppercase',
              }}>
                {neutralCount} HOLD
              </span>
            )}
          </div>
        </div>

        {/* Indicator Grid */}
        {isExpanded && (
          <div style={{
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(340px, 1fr))',
            gap: SPACING.MEDIUM,
            padding: SPACING.MEDIUM,
            backgroundColor: COLORS.DARK_BG,
            borderLeft: `2px solid ${color}`,
            borderBottom: BORDERS.STANDARD,
          }}>
            {indicators.map((indicator: string) => {
              const values = technicalsData.data.map((d: any) => d[indicator]).filter((v: any) => v != null);
              if (values.length === 0) return null;

              const latestValue = values[values.length - 1];
              const min = Math.min(...values);
              const max = Math.max(...values);
              const avg = values.reduce((a: number, b: number) => a + b, 0) / values.length;

              return (
                <IndicatorBox
                  key={indicator}
                  name={indicator}
                  latestValue={latestValue}
                  min={min}
                  max={max}
                  avg={avg}
                  values={values}
                  priceData={historicalData}
                  color={color}
                  category={category}
                />
              );
            })}
          </div>
        )}
      </div>
    );
  };

  const recommendationColor = signalSummary.recommendation.includes('BUY') ? COLORS.GREEN :
                              signalSummary.recommendation.includes('SELL') ? COLORS.RED : COLORS.YELLOW;

  return (
    <div style={{
      padding: SPACING.MEDIUM,
      overflow: 'auto',
      height: 'calc(100vh - 280px)',
      fontFamily: TYPOGRAPHY.MONO,
    }} className="custom-scrollbar">
      {/* Summary Dashboard */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: '280px 1fr',
        gap: SPACING.MEDIUM,
        marginBottom: SPACING.MEDIUM,
      }}>
        {/* Overall Signal Panel */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: BORDERS.STANDARD,
          display: 'flex',
          flexDirection: 'column',
        }}>
          {/* Panel Header */}
          <div style={{
            backgroundColor: COLORS.HEADER_BG,
            padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
            borderBottom: `2px solid ${COLORS.ORANGE}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <Gauge size={14} color={COLORS.ORANGE} />
            <span style={{
              color: COLORS.ORANGE,
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
              textTransform: 'uppercase',
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              TECHNICAL RATING
            </span>
          </div>

          {/* Rating Content */}
          <div style={{
            padding: SPACING.DEFAULT,
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: SPACING.MEDIUM,
            flex: 1,
          }}>
            <div style={{
              fontSize: '24px',
              fontWeight: TYPOGRAPHY.BOLD,
              color: recommendationColor,
              textShadow: `0 0 20px ${recommendationColor}40`,
              textTransform: 'uppercase',
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              {signalSummary.recommendation}
            </div>

            {/* Signal Gauge */}
            <div style={{
              width: '100%',
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
            }}>
              <TrendingDown size={12} color={COLORS.RED} />
              <div style={{
                flex: 1,
                height: '6px',
                backgroundColor: COLORS.DARK_BG,
                border: BORDERS.STANDARD,
                overflow: 'hidden',
                display: 'flex',
              }}>
                <div style={{
                  width: `${signalSummary.total > 0 ? (signalSummary.sell / signalSummary.total) * 100 : 0}%`,
                  backgroundColor: COLORS.RED,
                }} />
                <div style={{
                  width: `${signalSummary.total > 0 ? (signalSummary.neutral / signalSummary.total) * 100 : 0}%`,
                  backgroundColor: COLORS.MUTED,
                }} />
                <div style={{
                  width: `${signalSummary.total > 0 ? (signalSummary.buy / signalSummary.total) * 100 : 0}%`,
                  backgroundColor: COLORS.GREEN,
                }} />
              </div>
              <TrendingUp size={12} color={COLORS.GREEN} />
            </div>

            {/* Signal Counts */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1fr 1fr',
              gap: SPACING.SMALL,
              width: '100%',
            }}>
              <div style={{
                textAlign: 'center',
                padding: SPACING.SMALL,
                backgroundColor: `${COLORS.GREEN}10`,
                borderLeft: `2px solid ${COLORS.GREEN}`,
              }}>
                <div style={{
                  color: COLORS.GREEN,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontSize: TYPOGRAPHY.LARGE,
                }}>{signalSummary.buy}</div>
                <div style={{
                  color: COLORS.GRAY,
                  fontSize: TYPOGRAPHY.TINY,
                  textTransform: 'uppercase',
                }}>BUY</div>
              </div>
              <div style={{
                textAlign: 'center',
                padding: SPACING.SMALL,
                backgroundColor: `${COLORS.GRAY}10`,
                borderLeft: `2px solid ${COLORS.GRAY}`,
              }}>
                <div style={{
                  color: COLORS.WHITE,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontSize: TYPOGRAPHY.LARGE,
                }}>{signalSummary.neutral}</div>
                <div style={{
                  color: COLORS.GRAY,
                  fontSize: TYPOGRAPHY.TINY,
                  textTransform: 'uppercase',
                }}>HOLD</div>
              </div>
              <div style={{
                textAlign: 'center',
                padding: SPACING.SMALL,
                backgroundColor: `${COLORS.RED}10`,
                borderLeft: `2px solid ${COLORS.RED}`,
              }}>
                <div style={{
                  color: COLORS.RED,
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontSize: TYPOGRAPHY.LARGE,
                }}>{signalSummary.sell}</div>
                <div style={{
                  color: COLORS.GRAY,
                  fontSize: TYPOGRAPHY.TINY,
                  textTransform: 'uppercase',
                }}>SELL</div>
              </div>
            </div>

            {/* Total Indicators */}
            <div style={{
              color: COLORS.MUTED,
              fontSize: TYPOGRAPHY.TINY,
              textTransform: 'uppercase',
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              {signalSummary.total} INDICATORS ANALYZED
            </div>
          </div>
        </div>

        {/* Key Indicators Quick View */}
        <div style={{
          backgroundColor: COLORS.PANEL_BG,
          border: BORDERS.STANDARD,
          display: 'flex',
          flexDirection: 'column',
        }}>
          {/* Panel Header */}
          <div style={{
            backgroundColor: COLORS.HEADER_BG,
            padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
            borderBottom: `2px solid ${COLORS.CYAN}`,
            display: 'flex',
            alignItems: 'center',
            gap: SPACING.SMALL,
          }}>
            <Zap size={14} color={COLORS.CYAN} />
            <span style={{
              color: COLORS.CYAN,
              fontSize: TYPOGRAPHY.SMALL,
              fontWeight: TYPOGRAPHY.BOLD,
              textTransform: 'uppercase',
              letterSpacing: TYPOGRAPHY.WIDE,
            }}>
              KEY INDICATORS
            </span>
          </div>

          {/* Indicators Grid */}
          <div style={{
            padding: SPACING.MEDIUM,
            display: 'grid',
            gridTemplateColumns: 'repeat(auto-fill, minmax(140px, 1fr))',
            gap: SPACING.SMALL,
            flex: 1,
          }}>
            {keyIndicatorValues.map((ind: any) => {
              if (!ind) return null;
              const signalColor = ind.signal === 'BUY' ? COLORS.GREEN :
                                  ind.signal === 'SELL' ? COLORS.RED : COLORS.GRAY;
              const SignalIcon = ind.signal === 'BUY' ? TrendingUp :
                                ind.signal === 'SELL' ? TrendingDown : Minus;

              return (
                <div
                  key={ind.name}
                  style={{
                    backgroundColor: COLORS.DARK_BG,
                    border: BORDERS.STANDARD,
                    borderLeft: `2px solid ${signalColor}`,
                    padding: SPACING.SMALL,
                    display: 'flex',
                    flexDirection: 'column',
                    gap: SPACING.TINY,
                    transition: EFFECTS.TRANSITION_FAST,
                  }}
                  onMouseEnter={(e) => {
                    e.currentTarget.style.borderColor = signalColor;
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.borderColor = COLORS.BORDER;
                    e.currentTarget.style.borderLeftColor = signalColor;
                  }}
                >
                  <div style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                  }}>
                    <span style={{
                      color: COLORS.GRAY,
                      fontSize: TYPOGRAPHY.TINY,
                      textTransform: 'uppercase',
                      letterSpacing: TYPOGRAPHY.TIGHT,
                    }}>
                      {ind.name.replace(/_/g, ' ').substring(0, 10)}
                    </span>
                    <SignalIcon size={10} color={signalColor} />
                  </div>
                  <div style={{
                    color: COLORS.WHITE,
                    fontSize: TYPOGRAPHY.HEADING,
                    fontWeight: TYPOGRAPHY.BOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                  }}>
                    {typeof ind.value === 'number' ? ind.value.toFixed(2) : ind.value}
                  </div>
                  <div style={{
                    color: signalColor,
                    fontSize: TYPOGRAPHY.TINY,
                    fontWeight: TYPOGRAPHY.BOLD,
                    textTransform: 'uppercase',
                  }}>
                    {ind.signal}
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* Category Sections */}
      {renderIndicatorCategory(
        technicalsData.indicator_columns?.trend,
        COLORS.CYAN,
        'TREND INDICATORS',
        <Activity size={16} color={COLORS.CYAN} />,
        'trend'
      )}

      {renderIndicatorCategory(
        technicalsData.indicator_columns?.momentum,
        COLORS.GREEN,
        'MOMENTUM INDICATORS',
        <BarChart3 size={16} color={COLORS.GREEN} />,
        'momentum'
      )}

      {renderIndicatorCategory(
        technicalsData.indicator_columns?.volatility,
        COLORS.YELLOW,
        'VOLATILITY INDICATORS',
        <Waves size={16} color={COLORS.YELLOW} />,
        'volatility'
      )}

      {renderIndicatorCategory(
        technicalsData.indicator_columns?.volume,
        COLORS.BLUE,
        'VOLUME INDICATORS',
        <Volume2 size={16} color={COLORS.BLUE} />,
        'volume'
      )}

      {renderIndicatorCategory(
        technicalsData.indicator_columns?.others,
        COLORS.PURPLE,
        'RETURN INDICATORS',
        <Percent size={16} color={COLORS.PURPLE} />,
        'others'
      )}

      {/* Trading Notes */}
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: BORDERS.STANDARD,
        marginTop: SPACING.MEDIUM,
      }}>
        <div style={{
          backgroundColor: COLORS.HEADER_BG,
          padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
          borderBottom: `2px solid ${COLORS.YELLOW}`,
          display: 'flex',
          alignItems: 'center',
          gap: SPACING.SMALL,
        }}>
          <AlertTriangle size={12} color={COLORS.YELLOW} />
          <span style={{
            color: COLORS.YELLOW,
            fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD,
            textTransform: 'uppercase',
            letterSpacing: TYPOGRAPHY.WIDE,
          }}>
            TRADING NOTES
          </span>
        </div>
        <div style={{
          padding: SPACING.MEDIUM,
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.SMALL,
          lineHeight: '1.6',
        }}>
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
            <span style={{ color: COLORS.MUTED }}>•</span>
            <span>Technical indicators should be used in conjunction with fundamental analysis and risk management.</span>
          </div>
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
            <span style={{ color: COLORS.MUTED }}>•</span>
            <span>Signal strength increases when multiple indicators align (confluence).</span>
          </div>
          <div style={{ display: 'flex', gap: SPACING.SMALL, marginBottom: SPACING.SMALL }}>
            <span style={{ color: COLORS.MUTED }}>•</span>
            <span>Overbought/oversold conditions can persist in strong trends - use with trend confirmation.</span>
          </div>
          <div style={{ display: 'flex', gap: SPACING.SMALL }}>
            <span style={{ color: COLORS.MUTED }}>•</span>
            <span>Past performance does not guarantee future results. Always use stop-losses.</span>
          </div>
        </div>
      </div>
    </div>
  );
};

export default TechnicalsTab;
