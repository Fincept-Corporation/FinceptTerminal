import React from 'react';
import { ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { useCache } from '@/hooks/useCache';

const DBNOMICS_API_BASE = 'https://api.db.nomics.world/v22';

interface DBnomicsWidgetProps {
  id: string;
  seriesId?: string; // e.g. 'BLS/ln/LNS14000000' or 'Eurostat/une_rt_m/M.SA.TOTAL.PC_ACT.T.EA20'
  onRemove?: () => void;
  onNavigate?: () => void;
  onConfigure?: () => void;
}

interface DBnomicsData {
  name: string;
  latestValue: number | null;
  latestDate: string | null;
  previousValue: number | null;
  change: number | null;
  changePct: number | null;
  unit: string;
  frequency: string;
}

export const DBnomicsWidget: React.FC<DBnomicsWidgetProps> = ({
  id, seriesId = 'BLS/ln/LNS14000000', onRemove, onNavigate, onConfigure
}) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<DBnomicsData>({
    key: `widget:dbnomics:${seriesId}`,
    category: 'economics',
    ttl: '1h',
    refetchInterval: 60 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const parts = seriesId.split('/');
      if (parts.length < 3) throw new Error(`Invalid series ID: ${seriesId}`);
      const [provider, datasetCode, seriesCode] = parts;

      const response = await fetch(
        `${DBNOMICS_API_BASE}/series/${encodeURIComponent(provider)}/${encodeURIComponent(datasetCode)}/${encodeURIComponent(seriesCode)}?observations=1&format=json`,
      );
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const result = await response.json();

      // API returns a message field when provider/dataset/series is not found
      if (result.message) throw new Error(result.message);

      const seriesDocs = result?.series?.docs ?? [];
      if (seriesDocs.length === 0) throw new Error('No series data found');

      const firstSeries = seriesDocs[0];
      const periods: string[] = firstSeries.period ?? [];
      const values: (number | null)[] = firstSeries.value ?? [];

      // Collect non-null observations
      const observations: { period: string; value: number }[] = [];
      for (let i = 0; i < Math.min(periods.length, values.length); i++) {
        if (values[i] !== null && values[i] !== undefined) {
          observations.push({ period: periods[i], value: values[i] as number });
        }
      }

      if (observations.length === 0) throw new Error('No observations in series');

      const latest = observations[observations.length - 1];
      const prev = observations.length > 1 ? observations[observations.length - 2] : null;
      const change = prev != null ? latest.value - prev.value : null;
      const changePct = prev != null && prev.value !== 0 ? (change! / Math.abs(prev.value)) * 100 : null;

      return {
        name: firstSeries.series_name ?? seriesId,
        latestValue: latest.value,
        latestDate: latest.period,
        previousValue: prev?.value ?? null,
        change,
        changePct,
        unit: '',
        frequency: firstSeries['@frequency'] ?? '',
      };
    }
  });

  const isPositive = (data?.change ?? 0) >= 0;
  const changeColor = data?.change == null ? 'var(--ft-color-text-muted)'
    : isPositive ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';

  const formatVal = (v: number | null) => {
    if (v == null) return '—';
    if (Math.abs(v) >= 1e12) return `${(v / 1e12).toFixed(2)}T`;
    if (Math.abs(v) >= 1e9) return `${(v / 1e9).toFixed(2)}B`;
    if (Math.abs(v) >= 1e6) return `${(v / 1e6).toFixed(2)}M`;
    return v.toFixed(2);
  };

  return (
    <BaseWidget
      id={id}
      title="DBNOMICS"
      onRemove={onRemove}
      onRefresh={refresh}
      onConfigure={onConfigure}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor="var(--ft-color-cyan, #00E5FF)"
    >
      <div style={{ padding: '12px' }}>
        {/* Series Name */}
        <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px', textTransform: 'uppercase' }}>
          {seriesId}
        </div>
        <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', marginBottom: '12px', lineHeight: '1.3' }}>
          {data?.name || 'Loading...'}
        </div>

        {/* Latest Value */}
        <div style={{ padding: '12px', backgroundColor: 'var(--ft-color-panel)', borderRadius: '2px', marginBottom: '8px', textAlign: 'center' }}>
          <div style={{ fontSize: '32px', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>
            {data?.latestValue != null ? formatVal(data.latestValue) : '—'}
          </div>
          {data?.unit && <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginTop: '2px' }}>{data.unit}</div>}
          {data?.latestDate && <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginTop: '2px' }}>{data.latestDate}</div>}
        </div>

        {/* Change */}
        {data?.change != null && (
          <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 'var(--ft-font-size-tiny)', padding: '0 4px' }}>
            <span style={{ color: 'var(--ft-color-text-muted)' }}>vs Previous</span>
            <span style={{ color: changeColor, fontWeight: 'bold' }}>
              {isPositive ? '+' : ''}{formatVal(data.change)}
              {data.changePct != null && ` (${isPositive ? '+' : ''}${data.changePct.toFixed(2)}%)`}
            </span>
          </div>
        )}

        {data?.frequency && (
          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginTop: '6px', textAlign: 'right' }}>
            Frequency: {data.frequency}
          </div>
        )}

        {onNavigate && (
          <div onClick={onNavigate} style={{ marginTop: '12px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
            <span>OPEN DBNOMICS</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
