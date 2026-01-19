import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { fetchAllNews, NewsArticle } from '../../../../services/news/newsService';
import { useCache } from '../../../../hooks/useCache';

const FINCEPT_WHITE = '#FFFFFF';
const FINCEPT_GREEN = '#00C800';
const FINCEPT_RED = '#FF0000';
const FINCEPT_YELLOW = '#FFFF00';
const FINCEPT_GRAY = '#787878';
const FINCEPT_BLUE = '#6496FA';

interface NewsWidgetProps {
  id: string;
  category?: string;
  limit?: number;
  onRemove?: () => void;
}

export const NewsWidget: React.FC<NewsWidgetProps> = ({
  id,
  category = 'ALL',
  limit = 5,
  onRemove
}) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for news data
  const {
    data: allNews,
    isLoading: loading,
    error,
    refresh
  } = useCache<NewsArticle[]>({
    key: 'news:all-feeds',
    category: 'news',
    fetcher: fetchAllNews,
    ttl: '10m',
    refetchInterval: 10 * 60 * 1000, // Auto-refresh every 10 minutes
    staleWhileRevalidate: true
  });

  // Filter and limit news based on category
  const news = useMemo(() => {
    if (!allNews) return [];
    const filtered = category === 'ALL'
      ? allNews
      : allNews.filter(a => a.category === category);
    return filtered.slice(0, limit);
  }, [allNews, category, limit]);

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'FLASH': return FINCEPT_RED;
      case 'URGENT': return '#FFA500';
      case 'BREAKING': return FINCEPT_YELLOW;
      default: return FINCEPT_GREEN;
    }
  };

  return (
    <BaseWidget
      id={id}
      title={`${t('widgets.news')} - ${category}`}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={loading}
      error={error?.message}
    >
      <div style={{ padding: '8px' }}>
        {news.map((article, index) => (
          <div
            key={article.id}
            style={{
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: index < news.length - 1 ? `1px solid ${FINCEPT_GRAY}` : 'none'
            }}
          >
            <div style={{ display: 'flex', gap: '4px', marginBottom: '2px', fontSize: '8px' }}>
              <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold' }}>
                [{article.priority}]
              </span>
              <span style={{ color: FINCEPT_BLUE }}>[{article.category}]</span>
              <span style={{ color: FINCEPT_GRAY }}>{article.time}</span>
            </div>
            <div style={{ color: FINCEPT_WHITE, fontSize: '10px', fontWeight: 'bold', marginBottom: '2px', lineHeight: '1.2' }}>
              {article.headline}
            </div>
            <div style={{ color: FINCEPT_GRAY, fontSize: '9px', lineHeight: '1.2' }}>
              {article.summary.substring(0, 100)}...
            </div>
            <div style={{ color: FINCEPT_GRAY, fontSize: '8px', marginTop: '2px' }}>
              {article.source}
            </div>
          </div>
        ))}
      </div>
    </BaseWidget>
  );
};
