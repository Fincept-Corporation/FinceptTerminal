import React, { useState, useEffect } from 'react';
import { BaseWidget } from './BaseWidget';
import { fetchNewsWithCache, NewsArticle } from '../../../../services/newsService';

const BLOOMBERG_WHITE = '#FFFFFF';
const BLOOMBERG_GREEN = '#00C800';
const BLOOMBERG_RED = '#FF0000';
const BLOOMBERG_YELLOW = '#FFFF00';
const BLOOMBERG_GRAY = '#787878';
const BLOOMBERG_BLUE = '#6496FA';

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
  const [news, setNews] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  const loadNews = async () => {
    setLoading(true);
    setError(null);
    try {
      const articles = await fetchNewsWithCache();
      const filtered = category === 'ALL'
        ? articles
        : articles.filter(a => a.category === category);
      setNews(filtered.slice(0, limit));
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load news');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    loadNews();
    const interval = setInterval(loadNews, 10 * 60 * 1000); // Refresh every 10 minutes
    return () => clearInterval(interval);
  }, [category, limit]);

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'FLASH': return BLOOMBERG_RED;
      case 'URGENT': return '#FFA500';
      case 'BREAKING': return BLOOMBERG_YELLOW;
      default: return BLOOMBERG_GREEN;
    }
  };

  return (
    <BaseWidget
      id={id}
      title={`NEWS - ${category}`}
      onRemove={onRemove}
      onRefresh={loadNews}
      isLoading={loading}
      error={error}
    >
      <div style={{ padding: '8px' }}>
        {news.map((article, index) => (
          <div
            key={article.id}
            style={{
              marginBottom: '8px',
              paddingBottom: '8px',
              borderBottom: index < news.length - 1 ? `1px solid ${BLOOMBERG_GRAY}` : 'none'
            }}
          >
            <div style={{ display: 'flex', gap: '4px', marginBottom: '2px', fontSize: '8px' }}>
              <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold' }}>
                [{article.priority}]
              </span>
              <span style={{ color: BLOOMBERG_BLUE }}>[{article.category}]</span>
              <span style={{ color: BLOOMBERG_GRAY }}>{article.time}</span>
            </div>
            <div style={{ color: BLOOMBERG_WHITE, fontSize: '10px', fontWeight: 'bold', marginBottom: '2px', lineHeight: '1.2' }}>
              {article.headline}
            </div>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '9px', lineHeight: '1.2' }}>
              {article.summary.substring(0, 100)}...
            </div>
            <div style={{ color: BLOOMBERG_GRAY, fontSize: '8px', marginTop: '2px' }}>
              {article.source}
            </div>
          </div>
        ))}
      </div>
    </BaseWidget>
  );
};
