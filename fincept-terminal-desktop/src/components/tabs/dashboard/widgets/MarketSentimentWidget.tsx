import React from 'react';
import { TrendingUp, TrendingDown, Minus, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { fetchAllNews, NewsArticle } from '@/services/news/newsService';
import { useCache } from '@/hooks/useCache';

interface MarketSentimentWidgetProps {
  id: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface SentimentData {
  bullish: number;
  bearish: number;
  neutral: number;
  total: number;
  score: number; // -100 to 100
  topBullish: string[];
  topBearish: string[];
  highImpact: number;
}

export const MarketSentimentWidget: React.FC<MarketSentimentWidgetProps> = ({ id, onRemove, onNavigate }) => {
  const { data, isLoading, isFetching, error, refresh } = useCache<SentimentData>({
    key: 'widget:market-sentiment',
    category: 'news',
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      const articles: NewsArticle[] = await fetchAllNews();
      const recent = articles.slice(0, 100); // last 100 articles

      const bullish = recent.filter(a => a.sentiment === 'BULLISH').length;
      const bearish = recent.filter(a => a.sentiment === 'BEARISH').length;
      const neutral = recent.filter(a => a.sentiment === 'NEUTRAL').length;
      const total = recent.length;

      // Score: (bullish - bearish) / total * 100
      const score = total > 0 ? Math.round(((bullish - bearish) / total) * 100) : 0;

      // Top bullish/bearish tickers mentioned
      const bullishArticles = recent.filter(a => a.sentiment === 'BULLISH' && a.impact === 'HIGH');
      const bearishArticles = recent.filter(a => a.sentiment === 'BEARISH' && a.impact === 'HIGH');

      const topBullish = [...new Set(bullishArticles.flatMap(a => a.tickers || []))].slice(0, 5);
      const topBearish = [...new Set(bearishArticles.flatMap(a => a.tickers || []))].slice(0, 5);
      const highImpact = recent.filter(a => a.impact === 'HIGH').length;

      return { bullish, bearish, neutral, total, score, topBullish, topBearish, highImpact };
    }
  });

  const scoreColor = !data ? 'var(--ft-color-text-muted)'
    : data.score > 20 ? 'var(--ft-color-success)'
    : data.score < -20 ? 'var(--ft-color-alert)'
    : 'var(--ft-color-warning)';

  const scoreLabel = !data ? 'N/A'
    : data.score > 40 ? 'STRONGLY BULLISH'
    : data.score > 20 ? 'BULLISH'
    : data.score > -20 ? 'NEUTRAL'
    : data.score > -40 ? 'BEARISH'
    : 'STRONGLY BEARISH';

  const ScoreIcon = !data ? Minus : data.score > 10 ? TrendingUp : data.score < -10 ? TrendingDown : Minus;

  return (
    <BaseWidget
      id={id}
      title="MARKET SENTIMENT"
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={(isLoading && !data) || isFetching}
      error={error?.message || null}
      headerColor={scoreColor}
    >
      {!data || data.total === 0 ? (
        <div style={{ padding: '24px', textAlign: 'center', color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-small)' }}>
          No news data available
        </div>
      ) : (
        <div style={{ padding: '8px' }}>
          {/* Score Banner */}
          <div style={{ padding: '10px', backgroundColor: 'var(--ft-color-panel)', borderRadius: '2px', marginBottom: '8px', textAlign: 'center' }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '8px', marginBottom: '4px' }}>
              <ScoreIcon size={18} color={scoreColor} />
              <span style={{ fontSize: '24px', fontWeight: 'bold', color: scoreColor }}>
                {data.score > 0 ? '+' : ''}{data.score}
              </span>
            </div>
            <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: scoreColor, fontWeight: 'bold' }}>{scoreLabel}</div>
            <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginTop: '2px' }}>
              Based on {data.total} recent articles
            </div>
          </div>

          {/* Sentiment Bar */}
          <div style={{ marginBottom: '12px' }}>
            <div style={{ display: 'flex', height: '8px', borderRadius: '4px', overflow: 'hidden', gap: '1px' }}>
              <div style={{ flex: data.bullish, backgroundColor: 'var(--ft-color-success)' }} />
              <div style={{ flex: data.neutral, backgroundColor: 'var(--ft-color-text-muted)' }} />
              <div style={{ flex: data.bearish, backgroundColor: 'var(--ft-color-alert)' }} />
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '4px', fontSize: '9px' }}>
              <span style={{ color: 'var(--ft-color-success)' }}>▲ {data.bullish} BULL</span>
              <span style={{ color: 'var(--ft-color-text-muted)' }}>{data.neutral} NEUTRAL</span>
              <span style={{ color: 'var(--ft-color-alert)' }}>▼ {data.bearish} BEAR</span>
            </div>
          </div>

          {/* Top Tickers */}
          {data.topBullish.length > 0 && (
            <div style={{ marginBottom: '8px' }}>
              <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>TOP BULLISH MENTIONS</div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {data.topBullish.map(t => (
                  <span key={t} style={{ fontSize: '9px', fontWeight: 'bold', color: 'var(--ft-color-success)', backgroundColor: 'var(--ft-color-panel)', padding: '2px 6px', borderRadius: '2px' }}>{t}</span>
                ))}
              </div>
            </div>
          )}
          {data.topBearish.length > 0 && (
            <div style={{ marginBottom: '8px' }}>
              <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)', marginBottom: '4px' }}>TOP BEARISH MENTIONS</div>
              <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                {data.topBearish.map(t => (
                  <span key={t} style={{ fontSize: '9px', fontWeight: 'bold', color: 'var(--ft-color-alert)', backgroundColor: 'var(--ft-color-panel)', padding: '2px 6px', borderRadius: '2px' }}>{t}</span>
                ))}
              </div>
            </div>
          )}

          <div style={{ fontSize: '9px', color: 'var(--ft-color-text-muted)' }}>
            {data.highImpact} HIGH IMPACT events
          </div>

          {onNavigate && (
            <div onClick={onNavigate} style={{ marginTop: '8px', textAlign: 'center', cursor: 'pointer', color: 'var(--ft-color-primary)', fontSize: 'var(--ft-font-size-tiny)', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px' }}>
              <span>OPEN NEWS</span><ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
