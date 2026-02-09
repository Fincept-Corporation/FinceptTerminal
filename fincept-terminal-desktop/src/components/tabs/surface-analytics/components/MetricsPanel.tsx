/**
 * Surface Analytics - Metrics Panel Component
 * Left sidebar with key metrics and data info
 */

import React from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import type { ChartMetric, ChartType } from '../types';

interface MetricsPanelProps {
  chartType: ChartType;
  metrics: ChartMetric[];
  dataSource: string;
  frequency: string;
  quality: string;
  symbol?: string;
  textColor: string;
  accentColor: string;
}

export const MetricsPanel: React.FC<MetricsPanelProps> = ({
  chartType,
  metrics,
  dataSource,
  frequency,
  quality,
  symbol,
  textColor,
  accentColor,
}) => {
  const { t } = useTranslation('surfaceAnalytics');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  const chartTitles: Record<ChartType, string> = {
    volatility: t('metricsPanel.ivSurfaceMetrics'),
    correlation: t('metricsPanel.correlationMetrics'),
    'yield-curve': t('metricsPanel.yieldCurveMetrics'),
    pca: t('metricsPanel.pcaFactorMetrics'),
  };

  return (
    <div
      style={{
        width: '280px',
        borderRight: `1px solid ${colors.textMuted}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
        backgroundColor: colors.panel,
        fontFamily,
      }}
    >
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
      }}>
        <span style={{
          fontSize: fontSize.tiny,
          fontWeight: 700,
          color: colors.textMuted,
          letterSpacing: '0.5px',
        }}>
          {chartTitles[chartType]}
        </span>
        {symbol && (
          <div style={{
            fontSize: fontSize.body,
            fontWeight: 700,
            color: accentColor,
            marginTop: '4px',
          }}>
            {symbol}
          </div>
        )}
      </div>

      {/* Metrics List */}
      <div style={{ flex: 1, overflowY: 'auto' }}>
        {metrics.map((metric, idx) => (
          <div
            key={idx}
            style={{
              padding: '10px 12px',
              borderBottom: `1px solid ${colors.textMuted}`,
            }}
          >
            <div style={{
              fontSize: fontSize.tiny,
              fontWeight: 700,
              color: colors.textMuted,
              letterSpacing: '0.5px',
            }}>
              {metric.label}
            </div>
            <div style={{
              display: 'flex',
              alignItems: 'baseline',
              justifyContent: 'space-between',
              marginTop: '4px',
            }}>
              <span style={{
                fontSize: fontSize.subheading,
                fontWeight: 700,
                color: colors.info,
                fontFamily,
              }}>
                {metric.value}
              </span>
              <span
                style={{
                  fontSize: fontSize.tiny,
                  fontWeight: 700,
                  padding: '2px 6px',
                  borderRadius: 'var(--ft-border-radius)',
                  backgroundColor:
                    metric.positive === null
                      ? `${colors.textMuted}20`
                      : metric.positive
                      ? `${colors.success}20`
                      : `${colors.alert}20`,
                  color:
                    metric.positive === null
                      ? colors.textMuted
                      : metric.positive
                      ? colors.success
                      : colors.alert,
                }}
              >
                {metric.change}
              </span>
            </div>
          </div>
        ))}

        {/* Data Info Section */}
        <div style={{ padding: '12px', marginTop: '8px' }}>
          <div style={{
            fontSize: fontSize.tiny,
            fontWeight: 700,
            color: colors.textMuted,
            letterSpacing: '0.5px',
            marginBottom: '8px',
          }}>
            {t('metricsPanel.dataInfo')}
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: fontSize.small }}>
              <span style={{ color: colors.textMuted }}>{t('metricsPanel.source')}:</span>
              <span style={{ color: colors.info }}>{dataSource}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: fontSize.small }}>
              <span style={{ color: colors.textMuted }}>{t('metricsPanel.frequency')}:</span>
              <span style={{ color: textColor }}>{frequency}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: fontSize.small }}>
              <span style={{ color: colors.textMuted }}>{t('metricsPanel.quality')}:</span>
              <span style={{ color: quality === 'ERROR' ? colors.alert : colors.success }}>{quality}</span>
            </div>
          </div>
        </div>

        {/* Legend for correlation */}
        {chartType === 'correlation' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: fontSize.tiny,
              fontWeight: 700,
              color: colors.textMuted,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              {t('metricsPanel.correlationScale')}
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: fontSize.small }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: colors.success, borderRadius: 'var(--ft-border-radius)' }} />
                <span style={{ color: colors.textMuted }}>{t('metricsPanel.strongPositive')}</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: colors.warning, borderRadius: 'var(--ft-border-radius)' }} />
                <span style={{ color: colors.textMuted }}>{t('metricsPanel.neutral')}</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: colors.alert, borderRadius: 'var(--ft-border-radius)' }} />
                <span style={{ color: colors.textMuted }}>{t('metricsPanel.strongNegative')}</span>
              </div>
            </div>
          </div>
        )}

        {/* Greeks legend for volatility */}
        {chartType === 'volatility' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: fontSize.tiny,
              fontWeight: 700,
              color: colors.textMuted,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              {t('metricsPanel.ivInterpretation')}
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: fontSize.small, color: colors.textMuted }}>
              <div>{t('metricsPanel.highIv')}</div>
              <div>{t('metricsPanel.lowIv')}</div>
              <div>{t('metricsPanel.skew')}</div>
              <div>{t('metricsPanel.termStructure')}</div>
            </div>
          </div>
        )}

        {/* PCA legend */}
        {chartType === 'pca' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: fontSize.tiny,
              fontWeight: 700,
              color: colors.textMuted,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              {t('metricsPanel.pcaInterpretation')}
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: fontSize.small, color: colors.textMuted }}>
              <div>{t('metricsPanel.pc1')}</div>
              <div>{t('metricsPanel.pc2')}</div>
              <div>{t('metricsPanel.pc3')}</div>
              <div>{t('metricsPanel.higherVar')}</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
