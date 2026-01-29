/**
 * Surface Analytics - Metrics Panel Component
 * Left sidebar with key metrics and data info
 * Follows UI Design System (UI_DESIGN_SYSTEM.md)
 */

import React from 'react';
import type { ChartMetric, ChartType } from '../types';
import { FINCEPT_COLORS, TYPOGRAPHY } from '../constants';

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
  const chartTitles: Record<ChartType, string> = {
    volatility: 'IV SURFACE METRICS',
    correlation: 'CORRELATION METRICS',
    'yield-curve': 'YIELD CURVE METRICS',
    pca: 'PCA FACTOR METRICS',
  };

  return (
    <div
      style={{
        width: '280px',
        borderRight: `1px solid ${FINCEPT_COLORS.BORDER}`,
        display: 'flex',
        flexDirection: 'column',
        flexShrink: 0,
        backgroundColor: FINCEPT_COLORS.PANEL_BG,
        fontFamily: TYPOGRAPHY.FONT_FAMILY,
      }}
    >
      {/* Header */}
      <div style={{
        padding: '12px',
        backgroundColor: FINCEPT_COLORS.HEADER_BG,
        borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
      }}>
        <span style={{
          fontSize: TYPOGRAPHY.LABEL_SIZE,
          fontWeight: 700,
          color: FINCEPT_COLORS.GRAY,
          letterSpacing: '0.5px',
        }}>
          {chartTitles[chartType]}
        </span>
        {symbol && (
          <div style={{
            fontSize: TYPOGRAPHY.HEADER_SIZE,
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
              borderBottom: `1px solid ${FINCEPT_COLORS.BORDER}`,
            }}
          >
            <div style={{
              fontSize: TYPOGRAPHY.LABEL_SIZE,
              fontWeight: 700,
              color: FINCEPT_COLORS.GRAY,
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
                fontSize: '14px',
                fontWeight: 700,
                color: FINCEPT_COLORS.CYAN,
                fontFamily: TYPOGRAPHY.FONT_FAMILY,
              }}>
                {metric.value}
              </span>
              <span
                style={{
                  fontSize: '8px',
                  fontWeight: 700,
                  padding: '2px 6px',
                  borderRadius: '2px',
                  backgroundColor:
                    metric.positive === null
                      ? `${FINCEPT_COLORS.MUTED}20`
                      : metric.positive
                      ? `${FINCEPT_COLORS.GREEN}20`
                      : `${FINCEPT_COLORS.RED}20`,
                  color:
                    metric.positive === null
                      ? FINCEPT_COLORS.MUTED
                      : metric.positive
                      ? FINCEPT_COLORS.GREEN
                      : FINCEPT_COLORS.RED,
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
            fontSize: TYPOGRAPHY.LABEL_SIZE,
            fontWeight: 700,
            color: FINCEPT_COLORS.GRAY,
            letterSpacing: '0.5px',
            marginBottom: '8px',
          }}>
            DATA INFO
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.DATA_SIZE }}>
              <span style={{ color: FINCEPT_COLORS.GRAY }}>SOURCE:</span>
              <span style={{ color: FINCEPT_COLORS.CYAN }}>{dataSource}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.DATA_SIZE }}>
              <span style={{ color: FINCEPT_COLORS.GRAY }}>FREQUENCY:</span>
              <span style={{ color: textColor }}>{frequency}</span>
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: TYPOGRAPHY.DATA_SIZE }}>
              <span style={{ color: FINCEPT_COLORS.GRAY }}>QUALITY:</span>
              <span style={{ color: quality === 'ERROR' ? FINCEPT_COLORS.RED : FINCEPT_COLORS.GREEN }}>{quality}</span>
            </div>
          </div>
        </div>

        {/* Legend for correlation */}
        {chartType === 'correlation' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: TYPOGRAPHY.LABEL_SIZE,
              fontWeight: 700,
              color: FINCEPT_COLORS.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              CORRELATION SCALE
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px', fontSize: TYPOGRAPHY.DATA_SIZE }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: '#2e7d32', borderRadius: '2px' }} />
                <span style={{ color: FINCEPT_COLORS.MUTED }}>Strong Positive (0.7-1.0)</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: '#ffeb3b', borderRadius: '2px' }} />
                <span style={{ color: FINCEPT_COLORS.MUTED }}>Neutral (0.3-0.7)</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <div style={{ width: '12px', height: '12px', backgroundColor: '#b71c1c', borderRadius: '2px' }} />
                <span style={{ color: FINCEPT_COLORS.MUTED }}>Strong Negative (-1.0-0.3)</span>
              </div>
            </div>
          </div>
        )}

        {/* Greeks legend for volatility */}
        {chartType === 'volatility' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: TYPOGRAPHY.LABEL_SIZE,
              fontWeight: 700,
              color: FINCEPT_COLORS.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              IV INTERPRETATION
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: TYPOGRAPHY.DATA_SIZE, color: FINCEPT_COLORS.MUTED }}>
              <div>• High IV = Expensive Options</div>
              <div>• Low IV = Cheap Options</div>
              <div>• Skew = Put/Call IV Diff</div>
              <div>• Term Structure = Time Effect</div>
            </div>
          </div>
        )}

        {/* PCA legend */}
        {chartType === 'pca' && (
          <div style={{ padding: '12px', marginTop: '8px' }}>
            <div style={{
              fontSize: TYPOGRAPHY.LABEL_SIZE,
              fontWeight: 700,
              color: FINCEPT_COLORS.GRAY,
              letterSpacing: '0.5px',
              marginBottom: '8px',
            }}>
              PCA INTERPRETATION
            </div>
            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px', fontSize: TYPOGRAPHY.DATA_SIZE, color: FINCEPT_COLORS.MUTED }}>
              <div>• PC1 = Market Factor</div>
              <div>• PC2 = Size/Value Factor</div>
              <div>• PC3 = Sector Rotation</div>
              <div>• Higher Var = More Explanatory</div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
