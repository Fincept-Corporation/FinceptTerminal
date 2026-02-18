import React from 'react';
import { Bell, TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { watchlistService } from '@/services/core/watchlistService';
import { marketDataService, QuoteData } from '@/services/markets/marketDataService';
import { useCache } from '@/hooks/useCache';

interface WatchlistAlertsWidgetProps {
  id: string;
  changeThreshold?: number; // percent threshold to trigger alert (default 3%)
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface AlertItem {
  symbol: string;
  price: number;
  change_percent: number;
  alertType: 'big_gainer' | 'big_loser';
}

interface AlertsData {
  alerts: AlertItem[];
  totalWatched: number;
  scanned: number;
}

export const WatchlistAlertsWidget: React.FC<WatchlistAlertsWidgetProps> = ({
  id, changeThreshold = 3, onRemove, onNavigate
}) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<AlertsData>({
    key: `widget:watchlist-alerts:${changeThreshold}`,
    category: 'market',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        await watchlistService.initialize();
        const watchlists = await watchlistService.getWatchlistsWithCounts();
        if (watchlists.length === 0) return { alerts: [], totalWatched: 0, scanned: 0 };

        // Collect all unique symbols from all watchlists
        const allItems = await Promise.all(
          watchlists.map(wl => watchlistService.getWatchlistStocks(wl.id))
        );
        const allSymbols: string[] = [...new Set(allItems.flat().map((i: { symbol: string }) => i.symbol))];

        if (allSymbols.length === 0) return { alerts: [], totalWatched: 0, scanned: 0 };

        // Fetch quotes in parallel (limit to 30)
        const symbols = allSymbols.slice(0, 30);
        const quotes = await Promise.allSettled(
          symbols.map(sym => marketDataService.getQuote(sym))
        );

        const alerts: AlertItem[] = [];
        quotes.forEach((r) => {
          if (r.status !== 'fulfilled' || !r.value) return;
          const q: QuoteData = r.value;
          if (Math.abs(q.change_percent) >= changeThreshold) {
            alerts.push({
              symbol: q.symbol,
              price: q.price,
              change_percent: q.change_percent,
              alertType: q.change_percent >= 0 ? 'big_gainer' : 'big_loser',
            });
          }
        });

        // Sort by absolute change
        alerts.sort((a, b) => Math.abs(b.change_percent) - Math.abs(a.change_percent));

        return { alerts: alerts.slice(0, 8), totalWatched: allSymbols.length, scanned: symbols.length };
      } catch {
        return { alerts: [], totalWatched: 0, scanned: 0 };
      }
    }
  });

  const hasAlerts = (data?.alerts.length ?? 0) > 0;

  return (
    <BaseWidget
      id={id}
      title="WATCHLIST ALERTS"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor={hasAlerts ? 'var(--ft-color-warning)' : 'var(--ft-color-text-muted)'}
    >
      <div style={{ padding: '4px' }}>
        {/* Stats bar */}
        {data && (
          <div style={{ padding: '6px 8px', backgroundColor: 'var(--ft-color-panel)', margin: '4px', borderRadius: '2px', display: 'flex', justifyContent: 'space-between', fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
            <span>WATCHING: <strong style={{ color: 'var(--ft-color-primary)' }}>{data.totalWatched}</strong></span>
            <span>THRESHOLD: <strong style={{ color: 'var(--ft-color-warning)' }}>±{changeThreshold}%</strong></span>
            <span>ALERTS: <strong style={{ color: hasAlerts ? 'var(--ft-color-warning)' : 'var(--ft-color-text-muted)' }}>{data.alerts.length}</strong></span>
          </div>
        )}

        {!hasAlerts ? (
          <div style={{ padding: '20px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
            <Bell size={28} style={{ margin: '0 auto 10px', opacity: 0.3 }} />
            <div>No alerts triggered</div>
            <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>Threshold: ±{changeThreshold}% change</div>
          </div>
        ) : (
          <>
            {/* Header */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 65px 60px', gap: '4px', padding: '3px 8px', fontSize: '9px', color: 'var(--ft-color-text-muted)', borderBottom: '1px solid var(--ft-border-color)' }}>
              <span>SYMBOL</span>
              <span style={{ textAlign: 'right' }}>PRICE</span>
              <span style={{ textAlign: 'right' }}>CHANGE</span>
            </div>

            {data!.alerts.map(alert => {
              const pos = alert.change_percent >= 0;
              const clr = pos ? 'var(--ft-color-success)' : 'var(--ft-color-alert)';
              return (
                <div key={alert.symbol} style={{ display: 'grid', gridTemplateColumns: '1fr 65px 60px', gap: '4px', padding: '6px 8px', borderBottom: '1px solid var(--ft-border-color)', alignItems: 'center' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    {pos ? <TrendingUp size={10} color={clr} /> : <TrendingDown size={10} color={clr} />}
                    <span style={{ fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: 'var(--ft-color-text)' }}>{alert.symbol}</span>
                  </div>
                  <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                    ${alert.price.toFixed(2)}
                  </span>
                  <span style={{ textAlign: 'right', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', color: clr }}>
                    {pos ? '+' : ''}{alert.change_percent.toFixed(1)}%
                  </span>
                </div>
              );
            })}
          </>
        )}

        {onNavigate && (
          <div onClick={onNavigate} style={{ padding: '6px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-warning)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px', borderTop: '1px solid var(--ft-border-color)' }}>
            <span>OPEN WATCHLIST</span><ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
