import React from 'react';
import { IndicatorBox } from '../components/IndicatorBox';
import { FINCEPT } from '../../portfolio-tab/finceptStyles';
import type { HistoricalData, TechnicalsData } from '../types';

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
};

interface TechnicalsTabProps {
  technicalsData: TechnicalsData | null;
  technicalsLoading: boolean;
  historicalData: HistoricalData[];
}

export const TechnicalsTab: React.FC<TechnicalsTabProps> = ({
  technicalsData,
  technicalsLoading,
  historicalData,
}) => {
  if (technicalsLoading) {
    return (
      <div style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        height: '400px',
        gap: '16px',
      }}>
        <div style={{
          width: '50px',
          height: '50px',
          border: '4px solid #404040',
          borderTop: '4px solid #ea580c',
          borderRadius: '50%',
          animation: 'spin 1s linear infinite'
        }} />
        <div style={{ color: COLORS.YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
          COMPUTING TECHNICAL INDICATORS...
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          Analyzing {historicalData.length} data points
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
        gap: '16px',
      }}>
        <div style={{ color: COLORS.RED, fontSize: '14px', fontWeight: 'bold' }}>
          [WARN] NO TECHNICAL DATA AVAILABLE
        </div>
        <div style={{ color: COLORS.GRAY, fontSize: '11px' }}>
          Please wait for historical data to load
        </div>
      </div>
    );
  }

  // Check if we have valid indicator data
  if (!technicalsData.indicator_columns || !technicalsData.data || !Array.isArray(technicalsData.data) || technicalsData.data.length === 0) {
    return (
      <div style={{
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        padding: '16px',
        textAlign: 'center',
        color: COLORS.YELLOW,
      }}>
        <div style={{ fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
          [WARN] NO INDICATOR DATA
        </div>
        <div style={{ fontSize: '11px', color: COLORS.GRAY }}>
          Technical indicators could not be computed. Please try again or select a different symbol.
        </div>
      </div>
    );
  }

  const renderIndicatorCategory = (indicators: string[] | undefined, color: string, category: string) => {
    if (!indicators || !Array.isArray(indicators)) return null;

    return indicators.map((indicator: string) => {
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
    });
  };

  return (
    <div style={{ padding: '8px', overflow: 'auto', height: 'calc(100vh - 280px)' }} className="custom-scrollbar">
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(420px, 1fr))', gap: '16px' }}>
        {/* Trend Indicators */}
        {renderIndicatorCategory(technicalsData.indicator_columns?.trend, COLORS.CYAN, 'TREND')}

        {/* Momentum Indicators */}
        {renderIndicatorCategory(technicalsData.indicator_columns?.momentum, COLORS.GREEN, 'MOMENTUM')}

        {/* Volatility Indicators */}
        {renderIndicatorCategory(technicalsData.indicator_columns?.volatility, COLORS.YELLOW, 'VOLATILITY')}

        {/* Volume Indicators */}
        {renderIndicatorCategory(technicalsData.indicator_columns?.volume, COLORS.BLUE, 'VOLUME')}

        {/* Others Indicators */}
        {renderIndicatorCategory(technicalsData.indicator_columns?.others, COLORS.MAGENTA, 'RETURN')}
      </div>
    </div>
  );
};

export default TechnicalsTab;
