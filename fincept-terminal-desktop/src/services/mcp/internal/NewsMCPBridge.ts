// News MCP Bridge - Connects news services to MCP Internal Tools
// Exposes complete news functionality to AI via chat

import { terminalMCPProvider } from './TerminalMCPProvider';
import {
  fetchAllNews,
  getActiveSources,
  getRSSFeedCount,
  getNewsCacheStats,
} from '@/services/news/newsService';
import {
  getUserRSSFeeds,
  getDefaultFeeds,
  getAllRSSFeeds,
  addUserRSSFeed,
  updateUserRSSFeed,
  deleteUserRSSFeed,
  toggleUserRSSFeed,
  toggleDefaultRSSFeed,
  deleteDefaultRSSFeed,
  restoreDefaultRSSFeed,
  restoreAllDefaultFeeds,
  testRSSFeedUrl,
  RSS_CATEGORIES,
  RSS_REGIONS,
} from '@/services/news/rssFeedService';
import { analyzeNewsArticle } from '@/services/news/newsAnalysisService';
import type { NewsArticleInfo, NewsSentimentStats } from './types';

/**
 * Bridge that connects news services to MCP tools
 * Allows AI to fetch, filter, manage, and analyze news via chat
 */
export class NewsMCPBridge {
  private connected: boolean = false;
  private cachedArticles: NewsArticleInfo[] = [];

  /**
   * Connect news services to MCP contexts
   */
  connect(): void {
    if (this.connected) return;

    terminalMCPProvider.setContexts({
      // ==================== NEWS FETCHING ====================

      fetchAllNews: async (forceRefresh?: boolean) => {
        const articles = await fetchAllNews(forceRefresh || false);
        this.cachedArticles = articles as NewsArticleInfo[];
        return this.cachedArticles;
      },

      getNewsByCategory: async (category: string) => {
        // Ensure we have fresh data
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        return this.cachedArticles.filter(
          (a) => a.category.toUpperCase() === category.toUpperCase()
        );
      },

      getNewsBySource: async (source: string) => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        return this.cachedArticles.filter(
          (a) => a.source.toUpperCase().includes(source.toUpperCase())
        );
      },

      searchNews: async (query: string) => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        const q = query.toLowerCase();
        return this.cachedArticles.filter(
          (a) =>
            a.headline.toLowerCase().includes(q) ||
            a.summary.toLowerCase().includes(q) ||
            a.source.toLowerCase().includes(q) ||
            (a.tickers && a.tickers.some((t) => t.toLowerCase().includes(q)))
        );
      },

      getBreakingNews: async () => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        return this.cachedArticles.filter(
          (a) => a.priority === 'FLASH' || a.priority === 'URGENT'
        );
      },

      getNewsBySentiment: async (sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL') => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        return this.cachedArticles.filter(
          (a) => a.sentiment.toUpperCase() === sentiment.toUpperCase()
        );
      },

      getNewsByPriority: async (priority: 'FLASH' | 'URGENT' | 'BREAKING' | 'ROUTINE') => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        return this.cachedArticles.filter(
          (a) => a.priority.toUpperCase() === priority.toUpperCase()
        );
      },

      getNewsByTicker: async (ticker: string) => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        const t = ticker.toUpperCase();
        return this.cachedArticles.filter(
          (a) => a.tickers && a.tickers.some((tk) => tk.toUpperCase() === t)
        );
      },

      // ==================== PROVIDERS / SOURCES ====================

      getActiveSources: async () => {
        return await getActiveSources();
      },

      getRSSFeedCount: async () => {
        return await getRSSFeedCount();
      },

      getUserRSSFeeds: async () => {
        return await getUserRSSFeeds();
      },

      getDefaultFeeds: async () => {
        return await getDefaultFeeds();
      },

      getAllRSSFeeds: async () => {
        return await getAllRSSFeeds();
      },

      // ==================== FEED MANAGEMENT ====================

      addUserRSSFeed: async (params) => {
        return await addUserRSSFeed(params);
      },

      updateUserRSSFeed: async (id, params) => {
        await updateUserRSSFeed(id, params);
      },

      deleteUserRSSFeed: async (id) => {
        await deleteUserRSSFeed(id);
      },

      toggleUserRSSFeed: async (id, enabled) => {
        await toggleUserRSSFeed(id, enabled);
      },

      toggleDefaultRSSFeed: async (feedId, enabled) => {
        await toggleDefaultRSSFeed(feedId, enabled);
      },

      deleteDefaultRSSFeed: async (feedId) => {
        await deleteDefaultRSSFeed(feedId);
      },

      restoreDefaultRSSFeed: async (feedId) => {
        await restoreDefaultRSSFeed(feedId);
      },

      restoreAllDefaultFeeds: async () => {
        await restoreAllDefaultFeeds();
      },

      testRSSFeedUrl: async (url) => {
        return await testRSSFeedUrl(url);
      },

      // ==================== ANALYSIS ====================

      analyzeNewsArticle: async (url: string) => {
        const result = await analyzeNewsArticle(url);
        if ('error' in result) {
          return {
            success: false,
            error: result.error,
          };
        }
        return {
          success: result.success,
          data: result.data,
        };
      },

      // ==================== STATISTICS ====================

      getNewsSentimentStats: async (): Promise<NewsSentimentStats> => {
        if (this.cachedArticles.length === 0) {
          this.cachedArticles = (await fetchAllNews(false)) as NewsArticleInfo[];
        }
        const articles = this.cachedArticles;
        const bullish = articles.filter((a) => a.sentiment === 'BULLISH').length;
        const bearish = articles.filter((a) => a.sentiment === 'BEARISH').length;
        const neutral = articles.filter((a) => a.sentiment === 'NEUTRAL').length;
        const total = articles.length || 1;
        const alertCount = articles.filter(
          (a) => a.priority === 'FLASH' || a.priority === 'URGENT'
        ).length;
        const netScore = (bullish - bearish) / total;

        return {
          bullish,
          bearish,
          neutral,
          total: articles.length,
          netScore,
          alertCount,
        };
      },

      getNewsCacheStats: async () => {
        return await getNewsCacheStats();
      },

      getAvailableCategories: () => {
        return [...RSS_CATEGORIES];
      },

      getAvailableRegions: () => {
        return [...RSS_REGIONS];
      },
    });

    this.connected = true;
    console.log('[NewsMCPBridge] Connected news services to MCP');
  }

  /**
   * Disconnect and clear contexts
   */
  disconnect(): void {
    if (!this.connected) return;

    terminalMCPProvider.setContexts({
      // News fetching
      fetchAllNews: undefined,
      getNewsByCategory: undefined,
      getNewsBySource: undefined,
      searchNews: undefined,
      getBreakingNews: undefined,
      getNewsBySentiment: undefined,
      getNewsByPriority: undefined,
      getNewsByTicker: undefined,

      // Providers / Sources
      getActiveSources: undefined,
      getRSSFeedCount: undefined,
      getUserRSSFeeds: undefined,
      getDefaultFeeds: undefined,
      getAllRSSFeeds: undefined,

      // Feed management
      addUserRSSFeed: undefined,
      updateUserRSSFeed: undefined,
      deleteUserRSSFeed: undefined,
      toggleUserRSSFeed: undefined,
      toggleDefaultRSSFeed: undefined,
      deleteDefaultRSSFeed: undefined,
      restoreDefaultRSSFeed: undefined,
      restoreAllDefaultFeeds: undefined,
      testRSSFeedUrl: undefined,

      // Analysis
      analyzeNewsArticle: undefined,

      // Statistics
      getNewsSentimentStats: undefined,
      getNewsCacheStats: undefined,
      getAvailableCategories: undefined,
      getAvailableRegions: undefined,
    });

    this.cachedArticles = [];
    this.connected = false;
    console.log('[NewsMCPBridge] Disconnected news services from MCP');
  }

  /**
   * Check if bridge is connected
   */
  isConnected(): boolean {
    return this.connected;
  }

  /**
   * Clear cached articles (useful when news is refreshed elsewhere)
   */
  clearCache(): void {
    this.cachedArticles = [];
  }
}

// Singleton instance
export const newsMCPBridge = new NewsMCPBridge();
