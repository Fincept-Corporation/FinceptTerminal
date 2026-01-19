import React, { useState, useEffect } from 'react';
import { TrendingUp, TrendingDown, Activity, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { invoke } from '@tauri-apps/api/core';

interface EconomicIndicatorsWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface Indicator {
  name: string;
  code: string;
  value: string;
  change: number;
  date: string;
}

const FINCEPT_GREEN = '#00FF00';
const FINCEPT_RED = '#FF0000';
const FINCEPT_ORANGE = '#FFA500';
const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GRAY = '#787878';
const FINCEPT_CYAN = '#00E5FF';

// Key economic indicators with their display info
const KEY_INDICATORS = [
  { code: 'GDP', name: 'US GDP Growth', unit: '%' },
  { code: 'UNRATE', name: 'Unemployment Rate', unit: '%' },
  { code: 'CPIAUCSL', name: 'CPI Inflation', unit: '%' },
  { code: 'FEDFUNDS', name: 'Fed Funds Rate', unit: '%' },
  { code: 'DGS10', name: '10Y Treasury', unit: '%' },
];

export const EconomicIndicatorsWidget: React.FC<EconomicIndicatorsWidgetProps> = ({
  id,
  onRemove,
  onNavigate
}) => {
  const [indicators, setIndicators] = useState<Indicator[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadIndicators = async () => {
    try {
      setLoading(true);
      setError(null);

      // Try to fetch from FRED API via Tauri
      const results: Indicator[] = [];

      for (const ind of KEY_INDICATORS) {
        try {
          const data = await invoke('get_fred_series', {
            seriesId: ind.code,
            startDate: new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
            endDate: new Date().toISOString().split('T')[0]
          });

          const parsed = data as any;
          if (parsed?.observations?.length > 0) {
            const latest = parsed.observations[parsed.observations.length - 1];
            const prev = parsed.observations.length > 1
              ? parsed.observations[parsed.observations.length - 2]
              : latest;

            results.push({
              name: ind.name,
              code: ind.code,
              value: `${parseFloat(latest.value).toFixed(2)}${ind.unit}`,
              change: parseFloat(latest.value) - parseFloat(prev.value),
              date: latest.date
            });
          }
        } catch (err) {
          // Add placeholder if API fails
          results.push({
            name: ind.name,
            code: ind.code,
            value: '--',
            change: 0,
            date: '--'
          });
        }
      }

      // If no API data, show static demo data
      if (results.every(r => r.value === '--')) {
        setIndicators([
          { name: 'US GDP Growth', code: 'GDP', value: '2.8%', change: 0.3, date: 'Q3 2024' },
          { name: 'Unemployment Rate', code: 'UNRATE', value: '4.1%', change: -0.1, date: 'Dec 2024' },
          { name: 'CPI Inflation', code: 'CPI', value: '3.2%', change: 0.2, date: 'Dec 2024' },
          { name: 'Fed Funds Rate', code: 'FEDFUNDS', value: '5.33%', change: 0, date: 'Jan 2025' },
          { name: '10Y Treasury', code: 'DGS10', value: '4.58%', change: 0.12, date: 'Jan 2025' },
        ]);
      } else {
        setIndicators(results);
      }
    } catch (err) {
      // Fallback to demo data
      setIndicators([
        { name: 'US GDP Growth', code: 'GDP', value: '2.8%', change: 0.3, date: 'Q3 2024' },
        { name: 'Unemployment Rate', code: 'UNRATE', value: '4.1%', change: -0.1, date: 'Dec 2024' },
        { name: 'CPI Inflation', code: 'CPI', value: '3.2%', change: 0.2, date: 'Dec 2024' },
        { name: 'Fed Funds Rate', code: 'FEDFUNDS', value: '5.33%', change: 0, date: 'Jan 2025' },
        { name: '10Y Treasury', code: 'DGS10', value: '4.58%', change: 0.12, date: 'Jan 2025' },
      ]);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadIndicators();
    const interval = setInterval(loadIndicators, 300000); // Refresh every 5 minutes
    return () => clearInterval(interval);
  }, []);

  return (
    <BaseWidget
      id={id}
      title="ECONOMIC INDICATORS"
      onRemove={onRemove}
      onRefresh={loadIndicators}
      isLoading={loading}
      error={error}
      headerColor={FINCEPT_CYAN}
    >
      <div style={{ padding: '4px' }}>
        {/* Header row */}
        <div style={{
          display: 'grid',
          gridTemplateColumns: '1fr 70px 60px',
          gap: '8px',
          padding: '4px 8px',
          borderBottom: '1px solid #333',
          color: FINCEPT_GRAY,
          fontSize: '9px'
        }}>
          <span>INDICATOR</span>
          <span style={{ textAlign: 'right' }}>VALUE</span>
          <span style={{ textAlign: 'right' }}>CHG</span>
        </div>

        {indicators.map((ind, index) => (
          <div
            key={ind.code}
            style={{
              display: 'grid',
              gridTemplateColumns: '1fr 70px 60px',
              gap: '8px',
              padding: '6px 8px',
              borderBottom: `1px solid #222`,
              alignItems: 'center'
            }}
          >
            <div>
              <div style={{ fontSize: '10px', color: FINCEPT_WHITE }}>{ind.name}</div>
              <div style={{ fontSize: '8px', color: FINCEPT_GRAY }}>{ind.date}</div>
            </div>
            <div style={{
              textAlign: 'right',
              fontSize: '11px',
              fontWeight: 'bold',
              color: FINCEPT_ORANGE
            }}>
              {ind.value}
            </div>
            <div style={{
              textAlign: 'right',
              fontSize: '10px',
              color: ind.change > 0 ? FINCEPT_GREEN : ind.change < 0 ? FINCEPT_RED : FINCEPT_GRAY,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'flex-end',
              gap: '2px'
            }}>
              {ind.change > 0 ? <TrendingUp size={10} /> : ind.change < 0 ? <TrendingDown size={10} /> : null}
              {ind.change !== 0 ? (ind.change > 0 ? '+' : '') + ind.change.toFixed(2) : '--'}
            </div>
          </div>
        ))}

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: FINCEPT_CYAN,
              fontSize: '9px',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>Open Economics Tab</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
