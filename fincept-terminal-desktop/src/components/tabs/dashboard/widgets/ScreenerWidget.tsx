import React, { useState } from 'react';
import { Search, ExternalLink, TrendingUp } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { screenerApi } from '@/services/screener/screenerApi';
import { useCache } from '@/hooks/useCache';

interface ScreenerWidgetProps {
  id: string;
  preset?: 'value' | 'growth' | 'momentum';
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface ScreenerResult {
  symbol: string;
  name?: string;
  price?: number;
  change_percent?: number;
  market_cap?: number;
  pe_ratio?: number;
  score?: number;
  [key: string]: any;
}

interface ScreenerData {
  results: ScreenerResult[];
  preset: string;
  count: number;
}

const PRESET_LABELS = { value: 'VALUE', growth: 'GROWTH', momentum: 'MOMENTUM' };
const PRESET_COLORS = { value: 'var(--ft-color-primary)', growth: 'var(--ft-color-success)', momentum: 'var(--ft-color-warning)' };

export const ScreenerWidget: React.FC<ScreenerWidgetProps> = ({
  id, preset = 'growth', onRemove, onNavigate
}) => {
  const [activePreset, setActivePreset] = useState<'value' | 'growth' | 'momentum'>(preset);

  const { data, isLoading, isFetching, error, refresh } = useCache<ScreenerData>({
    key: `widget:screener:${activePreset}`,
    category: 'screener',
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        let criteria;
        if (activePreset === 'value') criteria = await screenerApi.getValueScreen();
        else if (activePreset === 'growth') criteria = await screenerApi.getGrowthScreen();
        else criteria = await screenerApi.getMomentumScreen();

        const result = await screenerApi.executeScreen(criteria);
        const results: ScreenerResult[] = (result.matchingStocks || []).slice(0, 8).map((r: any) => ({
          symbol: r.symbol || r.ticker || '',
          name: r.name || r.company_name || '',
          price: r.price ?? r.last_price,
          change_percent: r.change_percent ?? r.pct_change,
          pe_ratio: r.pe_ratio ?? r.pe,
          market_cap: r.market_cap,
          score: r.score
        }));
        return { results, preset: activePreset, count: result.totalStocksScreened || results.length };
      } catch {
        return { results: [], preset: activePreset, count: 0 };
      }
    }
  });

  const color = PRESET_COLORS[activePreset];

  return (
    <BaseWidget
      id={id}
      title="STOCK SCREENER"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor={color}
    >
      <div style={{ padding: '4px' }}>
        {/* Preset Tabs */}
        <div style={{ display: 'flex', gap: '4px', margin: '4px', marginBottom: '8px' }}>
          {(['value', 'growth', 'momentum'] as const).map(p => (
            <button
              key={p}
              onClick={() => setActivePreset(p)}
              style={{
                flex: 1,
                padding: '4px',
                fontSize: '9px',
                fontWeight: 'bold',
                border: `1px solid ${activePreset === p ? PRESET_COLORS[p] : 'var(--ft-border-color)'}`,
                borderRadius: '2px',
                cursor: 'pointer',
                backgroundColor: activePreset === p ? PRESET_COLORS[p] : 'var(--ft-color-panel)',
                color: activePreset === p ? '#000' : 'var(--ft-color-text-muted)',
                textTransform: 'uppercase'
              }}
            >
              {PRESET_LABELS[p]}
            </button>
          ))}
        </div>

        {/* Header */}
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 65px 55px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
          <span>SYMBOL</span>
          <span style={{ textAlign: 'right' }}>PRICE</span>
          <span style={{ textAlign: 'right' }}>CHG%</span>
        </div>

        {(data?.results ?? []).length === 0 ? (
          <div style={{ padding: '20px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)' }}>
            <Search size={24} style={{ margin: '0 auto 8px', opacity: 0.3 }} />
            <div>Run a screen to see results</div>
          </div>
        ) : (data?.results ?? []).map((r, i) => {
          const pos = (r.change_percent ?? 0) >= 0;
          const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
          return (
            <div key={r.symbol || i} style={{ display: 'grid', gridTemplateColumns: '1fr 65px 55px', gap: '4px', padding: '5px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
              <div>
                <div style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{r.symbol}</div>
                {r.name && <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>{r.name.substring(0, 18)}</div>}
              </div>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {r.price != null ? `$${r.price.toFixed(2)}` : '—'}
              </span>
              <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: r.change_percent != null ? clr : 'var(--ft-color-text-muted)' }}>
                {r.change_percent != null ? `${pos ? '+' : ''}${r.change_percent.toFixed(1)}%` : '—'}
              </span>
            </div>
          );
        })}

        {data && data.count > 8 && (
          <div style={{ padding: '4px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', textAlign: 'center' }}>
            +{data.count - 8} more results
          </div>
        )}

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color, fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
            <span>OPEN SCREENER</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
