import { useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import type { NewsData } from '../types';

interface UseNewsReturn {
  newsData: NewsData | null;
  newsLoading: boolean;
  fetchNews: (symbol: string, companyName?: string) => Promise<void>;
}

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

    try {
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

      if (typeof response === 'string') {
        const parsed = JSON.parse(response);
        setNewsData(parsed);
      } else {
        setNewsData(response);
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
