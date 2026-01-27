import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { cacheService, CacheService, TTL } from '../../../../services/cache/cacheService';
import type { NewsData } from '../types';

interface UseNewsReturn {
  newsData: NewsData | null;
  newsLoading: boolean;
  fetchNews: (symbol: string, companyName?: string) => Promise<void>;
}

const NEWS_CATEGORY = 'news';

export function useNews(): UseNewsReturn {
  const [newsData, setNewsData] = useState<NewsData | null>(null);
  const [newsLoading, setNewsLoading] = useState(false);

  const fetchNews = useCallback(async (symbol: string, companyName?: string) => {
    if (!symbol) {
      console.log('No symbol selected for news fetch');
      return;
    }

    setNewsLoading(true);
    console.log('Fetching news for:', symbol);

    const cacheKey = CacheService.key(NEWS_CATEGORY, 'company', symbol);

    try {
      // Check cache first
      const cached = await cacheService.get<NewsData>(cacheKey);
      if (cached) {
        console.log('Using cached news for', symbol);
        setNewsData(cached.data);
        setNewsLoading(false);
        return;
      }

      // Use company name if available, otherwise use symbol
      const query = companyName || symbol;

      const response: any = await invoke('fetch_company_news', {
        query: query,
        maxResults: 20,
        period: '7d',
        language: 'en',
        country: 'US'
      });

      console.log('News response:', response);

      let parsed: NewsData;
      if (typeof response === 'string') {
        parsed = JSON.parse(response);
      } else {
        parsed = response;
      }

      setNewsData(parsed);

      // Cache the news data
      if (parsed.success !== false) {
        await cacheService.set(cacheKey, parsed, NEWS_CATEGORY, TTL['10m']);
      }
    } catch (error) {
      console.error('Error fetching news:', error);
      setNewsData({
        success: false,
        error: String(error),
        data: []
      });
    } finally {
      setNewsLoading(false);
    }
  }, []);

  return {
    newsData,
    newsLoading,
    fetchNews,
  };
}

export default useNews;
