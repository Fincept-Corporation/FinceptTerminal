import React, { useMemo } from 'react';
import { useTranslation } from 'react-i18next';
import { BaseWidget } from './BaseWidget';
import { fetchAllNews, NewsArticle } from '../../../../services/news/newsService';
import { useCache } from '../../../../hooks/useCache';


interface NewsWidgetProps {
  id: string;
  category?: string;
  limit?: number;
  onRemove?: () => void;
  onConfigure?: () => void;
}

export const NewsWidget: React.FC<NewsWidgetProps> = ({
  id,
  category = 'ALL',
  limit = 5,
  onRemove,
  onConfigure
}) => {
  const { t } = useTranslation('dashboard');

  // Use unified cache hook for news data
  const {
    data: allNews,
    isLoading: loading,
    isFetching,
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
      case 'FLASH': return 'var(--ft-color-alert)';
      case 'URGENT': return 'var(--ft-color-primary)';
      case 'BREAKING': return 'var(--ft-color-warning)';
      default: return 'var(--ft-color-success)';
    }
  };

  return (
    <BaseWidget
      id={id}
      title={`${t('widgets.news')} - ${t(`widgets.category${category.charAt(0).toUpperCase() + category.slice(1).toLowerCase()}`, category)}`}
      onRemove={onRemove}
      onRefresh={refresh}
      onConfigure={onConfigure}
      isLoading={(loading && !allNews) || isFetching}
      error={error?.message}
    >
      <div style={{ padding: '8px' }}>
        {news.map((article, index) => (
          <div
            key={article.id}
            style={{
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: index < news.length - 1 ? '1px solid var(--ft-border-color)' : 'none'
            }}
          >
            <div style={{ display: 'flex', gap: '4px', marginBottom: '2px', fontSize: 'var(--ft-font-size-tiny)' }}>
              <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold' }}>
                [{article.priority}]
              </span>
              <span style={{ color: 'var(--ft-color-info)' }}>[{article.category}]</span>
              <span style={{ color: 'var(--ft-color-text-muted)' }}>{article.time}</span>
            </div>
            <div style={{ color: 'var(--ft-color-text)', fontSize: 'var(--ft-font-size-small)', fontWeight: 'bold', marginBottom: '2px', lineHeight: '1.2' }}>
              {article.headline}
            </div>
            <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)', lineHeight: '1.2' }}>
              {article.summary.substring(0, 100)}...
            </div>
            <div style={{ color: 'var(--ft-color-text-muted)', fontSize: 'var(--ft-font-size-tiny)', marginTop: '2px' }}>
              {article.source}
            </div>
          </div>
        ))}
      </div>
    </BaseWidget>
  );
};
