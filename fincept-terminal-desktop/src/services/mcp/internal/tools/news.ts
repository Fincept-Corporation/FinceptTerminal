// News MCP Tools - Comprehensive news fetching, filtering, management and analysis
// Allows AI to interact with all news functionality via chat

import { invoke } from '@tauri-apps/api/core';
import type { InternalTool, InternalToolResult, NewsArticleInfo } from '../types';

/**
 * Helper to format news articles for AI consumption
 */
function formatNewsArticles(articles: NewsArticleInfo[], maxArticles = 20): string {
  if (!articles || articles.length === 0) {
    return 'No news articles found';
  }

  const limited = articles.slice(0, maxArticles);
  const lines = limited.map((article, idx) => {
    const priorityBadge = article.priority === 'FLASH' || article.priority === 'URGENT' ? `[${article.priority}] ` : '';
    const tickerStr = article.tickers?.length > 0 ? ` | Tickers: ${article.tickers.join(', ')}` : '';
    return `${idx + 1}. ${priorityBadge}${article.headline}
   Source: ${article.source} | Category: ${article.category} | Sentiment: ${article.sentiment} | Impact: ${article.impact}${tickerStr}
   Time: ${article.time || 'N/A'}${article.link ? ` | Link: ${article.link}` : ''}`;
  });

  const summary = `Found ${articles.length} articles${articles.length > maxArticles ? ` (showing first ${maxArticles})` : ''}:\n\n`;
  return summary + lines.join('\n\n');
}

export const newsTools: InternalTool[] = [
  // ============================================================================
  // NEWS FETCHING & FILTERING
  // ============================================================================

  {
    name: 'get_all_news',
    description: 'Get all news articles from all configured RSS feeds. Use this to see the latest financial news. Returns headlines, sources, sentiment, priority, and tickers.',
    inputSchema: {
      type: 'object',
      properties: {
        force_refresh: {
          type: 'boolean',
          description: 'Force refresh from sources (bypass cache). Default is false.',
          default: false,
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 20.',
          default: 20,
        },
      },
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.fetchAllNews) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const articles = await contexts.fetchAllNews(args.force_refresh || false);
        const limit = args.limit || 20;
        return {
          success: true,
          data: articles.slice(0, limit),
          message: formatNewsArticles(articles, limit),
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch news: ${error}` };
      }
    },
  },

  {
    name: 'get_news_by_category',
    description: 'Get news articles filtered by category. Categories include: MARKETS, CRYPTO, TECH, ENERGY, REGULATORY, ECONOMIC, EARNINGS, BANKING, GEOPOLITICS, GENERAL.',
    inputSchema: {
      type: 'object',
      properties: {
        category: {
          type: 'string',
          description: 'Category to filter by (MARKETS, CRYPTO, TECH, ENERGY, REGULATORY, ECONOMIC, EARNINGS, BANKING, GEOPOLITICS, GENERAL)',
          enum: ['MARKETS', 'CRYPTO', 'TECH', 'ENERGY', 'REGULATORY', 'ECONOMIC', 'EARNINGS', 'BANKING', 'GEOPOLITICS', 'GENERAL'],
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 15.',
          default: 15,
        },
      },
      required: ['category'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getNewsByCategory) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const category = (args.category || '').toUpperCase();
        const articles = await contexts.getNewsByCategory(category);
        const limit = args.limit || 15;
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `News in category "${category}":\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch news by category: ${error}` };
      }
    },
  },

  {
    name: 'get_news_by_source',
    description: 'Get news articles from a specific source/provider (e.g., YAHOO, COINDESK, BLOOMBERG, REUTERS, etc.)',
    inputSchema: {
      type: 'object',
      properties: {
        source: {
          type: 'string',
          description: 'Source name to filter by (e.g., YAHOO, COINDESK, COINTELEGRAPH, TECHCRUNCH)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 15.',
          default: 15,
        },
      },
      required: ['source'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getNewsBySource) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const source = (args.source || '').toUpperCase();
        const articles = await contexts.getNewsBySource(source);
        const limit = args.limit || 15;
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `News from "${source}":\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch news by source: ${error}` };
      }
    },
  },

  {
    name: 'search_news',
    description: 'Search news articles by keyword. Searches headlines, summaries, sources, and tickers.',
    inputSchema: {
      type: 'object',
      properties: {
        query: {
          type: 'string',
          description: 'Search query (keyword, ticker symbol, company name, etc.)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 15.',
          default: 15,
        },
      },
      required: ['query'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.searchNews) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const articles = await contexts.searchNews(args.query || '');
        const limit = args.limit || 15;
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `Search results for "${args.query}":\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to search news: ${error}` };
      }
    },
  },

  {
    name: 'get_breaking_news',
    description: 'Get breaking and urgent news articles (FLASH or URGENT priority). These are the most important market-moving headlines.',
    inputSchema: {
      type: 'object',
      properties: {
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 10.',
          default: 10,
        },
      },
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getBreakingNews) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const articles = await contexts.getBreakingNews();
        const limit = args.limit || 10;
        if (articles.length === 0) {
          return {
            success: true,
            data: [],
            message: 'No breaking news at the moment. All quiet on the market front',
          };
        }
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `BREAKING NEWS ALERTS:\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch breaking news: ${error}` };
      }
    },
  },

  {
    name: 'get_news_by_sentiment',
    description: 'Get news articles filtered by market sentiment (BULLISH, BEARISH, or NEUTRAL)',
    inputSchema: {
      type: 'object',
      properties: {
        sentiment: {
          type: 'string',
          description: 'Sentiment filter: BULLISH (positive), BEARISH (negative), or NEUTRAL',
          enum: ['BULLISH', 'BEARISH', 'NEUTRAL'],
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 15.',
          default: 15,
        },
      },
      required: ['sentiment'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getNewsBySentiment) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const sentiment = (args.sentiment || '').toUpperCase() as 'BULLISH' | 'BEARISH' | 'NEUTRAL';
        const articles = await contexts.getNewsBySentiment(sentiment);
        const limit = args.limit || 15;
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `${sentiment} news articles:\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch news by sentiment: ${error}` };
      }
    },
  },

  {
    name: 'get_news_by_ticker',
    description: 'Get news articles mentioning a specific stock ticker symbol (e.g., AAPL, TSLA, BTC)',
    inputSchema: {
      type: 'object',
      properties: {
        ticker: {
          type: 'string',
          description: 'Stock ticker symbol (e.g., AAPL, MSFT, TSLA, BTC)',
        },
        limit: {
          type: 'number',
          description: 'Maximum number of articles to return. Default is 15.',
          default: 15,
        },
      },
      required: ['ticker'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getNewsByTicker) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const ticker = (args.ticker || '').toUpperCase();
        const articles = await contexts.getNewsByTicker(ticker);
        const limit = args.limit || 15;
        if (articles.length === 0) {
          return {
            success: true,
            data: [],
            message: `No news articles found mentioning ticker "${ticker}"`,
          };
        }
        return {
          success: true,
          data: articles.slice(0, limit),
          message: `News mentioning ${ticker}:\n\n${formatNewsArticles(articles, limit)}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to fetch news by ticker: ${error}` };
      }
    },
  },

  // ============================================================================
  // NEWS PROVIDERS / SOURCES
  // ============================================================================

  {
    name: 'list_active_news_sources',
    description: 'Get list of all active news sources/providers currently fetching news',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getActiveSources) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const sources = await contexts.getActiveSources();
        return {
          success: true,
          data: sources,
          message: `Active news sources (${sources.length}):\n${sources.map((s, i) => `${i + 1}. ${s}`).join('\n')}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get active sources: ${error}` };
      }
    },
  },

  {
    name: 'get_rss_feed_count',
    description: 'Get the total number of RSS feeds configured',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getRSSFeedCount) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const count = await contexts.getRSSFeedCount();
        return {
          success: true,
          data: { count },
          message: `There are ${count} RSS feeds configured in total.`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get feed count: ${error}` };
      }
    },
  },

  {
    name: 'list_user_rss_feeds',
    description: 'List all custom RSS feeds added by the user',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getUserRSSFeeds) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const feeds = await contexts.getUserRSSFeeds();
        if (feeds.length === 0) {
          return {
            success: true,
            data: [],
            message: 'No custom RSS feeds configured. User can add custom feeds in the News tab settings',
          };
        }
        const feedList = feeds.map((f, i) =>
          `${i + 1}. ${f.name} (${f.source_name})\n   Category: ${f.category} | Region: ${f.region} | Enabled: ${f.enabled ? 'Yes' : 'No'}\n   URL: ${f.url}`
        ).join('\n\n');
        return {
          success: true,
          data: feeds,
          message: `Custom RSS feeds (${feeds.length}):\n\n${feedList}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get user feeds: ${error}` };
      }
    },
  },

  {
    name: 'list_default_rss_feeds',
    description: 'List all default (built-in) RSS feeds',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getDefaultFeeds) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const feeds = await contexts.getDefaultFeeds();
        const feedList = feeds.map((f, i) =>
          `${i + 1}. ${f.name} (${f.source})\n   Category: ${f.category} | Region: ${f.region} | Enabled: ${f.enabled ? 'Yes' : 'No'}`
        ).join('\n\n');
        return {
          success: true,
          data: feeds,
          message: `Default RSS feeds (${feeds.length}):\n\n${feedList}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get default feeds: ${error}` };
      }
    },
  },

  {
    name: 'list_all_rss_feeds',
    description: 'List all RSS feeds (both default and custom)',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getAllRSSFeeds) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const feeds = await contexts.getAllRSSFeeds();
        const enabledCount = feeds.filter(f => f.enabled).length;
        const feedList = feeds.map((f, i) =>
          `${i + 1}. ${f.name} (${f.source}) [${f.is_default ? 'Default' : 'Custom'}]\n   Category: ${f.category} | Region: ${f.region} | Enabled: ${f.enabled ? 'Yes' : 'No'}`
        ).join('\n\n');
        return {
          success: true,
          data: feeds,
          message: `All RSS feeds (${feeds.length} total, ${enabledCount} enabled):\n\n${feedList}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get all feeds: ${error}` };
      }
    },
  },

  // ============================================================================
  // RSS FEED MANAGEMENT
  // ============================================================================

  {
    name: 'add_rss_feed',
    description: 'Add a new custom RSS feed. Requires name, URL, category, region, and source name.',
    inputSchema: {
      type: 'object',
      properties: {
        name: {
          type: 'string',
          description: 'Display name for the feed (e.g., "Bloomberg Markets")',
        },
        url: {
          type: 'string',
          description: 'RSS feed URL (must be valid RSS/XML feed)',
        },
        category: {
          type: 'string',
          description: 'News category',
          enum: ['MARKETS', 'CRYPTO', 'TECH', 'ENERGY', 'REGULATORY', 'ECONOMIC', 'EARNINGS', 'BANKING', 'GEOPOLITICS', 'GENERAL'],
          default: 'GENERAL',
        },
        region: {
          type: 'string',
          description: 'Geographic region',
          enum: ['GLOBAL', 'US', 'EU', 'ASIA', 'UK', 'INDIA', 'CHINA', 'LATAM'],
          default: 'GLOBAL',
        },
        source_name: {
          type: 'string',
          description: 'Short source name for display (e.g., "BLOOMBERG")',
        },
      },
      required: ['name', 'url', 'source_name'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.addUserRSSFeed) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const feed = await contexts.addUserRSSFeed({
          name: args.name,
          url: args.url,
          category: args.category || 'GENERAL',
          region: args.region || 'GLOBAL',
          source_name: (args.source_name || '').toUpperCase(),
        });
        return {
          success: true,
          data: feed,
          message: `Successfully added RSS feed "${feed.name}" (${feed.source_name}).\nCategory: ${feed.category} | Region: ${feed.region}\nURL: ${feed.url}`,
        };
      } catch (error) {
        return { success: false, error: `Failed to add RSS feed: ${error}` };
      }
    },
  },

  {
    name: 'toggle_rss_feed',
    description: 'Enable or disable an RSS feed by ID',
    inputSchema: {
      type: 'object',
      properties: {
        feed_id: {
          type: 'string',
          description: 'Feed ID to toggle',
        },
        enabled: {
          type: 'boolean',
          description: 'True to enable, false to disable',
        },
        is_default: {
          type: 'boolean',
          description: 'True if this is a default feed, false if custom feed',
          default: false,
        },
      },
      required: ['feed_id', 'enabled'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      try {
        if (args.is_default) {
          if (!contexts.toggleDefaultRSSFeed) {
            return { success: false, error: 'News service not available' };
          }
          await contexts.toggleDefaultRSSFeed(args.feed_id, args.enabled);
        } else {
          if (!contexts.toggleUserRSSFeed) {
            return { success: false, error: 'News service not available' };
          }
          await contexts.toggleUserRSSFeed(args.feed_id, args.enabled);
        }
        return {
          success: true,
          message: `RSS feed ${args.enabled ? 'enabled' : 'disabled'} successfully.`,
        };
      } catch (error) {
        return { success: false, error: `Failed to toggle feed: ${error}` };
      }
    },
  },

  {
    name: 'delete_rss_feed',
    description: 'Delete a custom RSS feed by ID',
    inputSchema: {
      type: 'object',
      properties: {
        feed_id: {
          type: 'string',
          description: 'Feed ID to delete',
        },
      },
      required: ['feed_id'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.deleteUserRSSFeed) {
        return { success: false, error: 'News service not available' };
      }
      try {
        await contexts.deleteUserRSSFeed(args.feed_id);
        return {
          success: true,
          message: 'RSS feed deleted successfully.',
        };
      } catch (error) {
        return { success: false, error: `Failed to delete feed: ${error}` };
      }
    },
  },

  {
    name: 'test_rss_feed_url',
    description: 'Test if an RSS feed URL is valid and accessible',
    inputSchema: {
      type: 'object',
      properties: {
        url: {
          type: 'string',
          description: 'RSS feed URL to test',
        },
      },
      required: ['url'],
    },
    handler: async (args, _contexts): Promise<InternalToolResult> => {
      try {
        const result = await invoke<{ valid: boolean; error?: string; status?: number; response_preview?: string }>('test_rss_feed_url', { url: args.url });
        return {
          success: true,
          data: { ...result, url: args.url },
          message: result.valid
            ? `RSS feed URL is valid: ${args.url}`
            : `RSS feed URL invalid. Reason: ${result.error || 'unknown'}${result.status ? ` (HTTP ${result.status})` : ''}${result.response_preview ? `\nResponse preview: ${result.response_preview}` : ''}`,
        };
      } catch (error) {
        return {
          success: true,
          data: { valid: false, url: args.url, invoke_error: String(error) },
          message: `Invoke failed: ${error}`,
        };
      }
    },
  },

  {
    name: 'restore_all_default_feeds',
    description: 'Restore all deleted default RSS feeds',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.restoreAllDefaultFeeds) {
        return { success: false, error: 'News service not available' };
      }
      try {
        await contexts.restoreAllDefaultFeeds();
        return {
          success: true,
          message: 'All default RSS feeds have been restored.',
        };
      } catch (error) {
        return { success: false, error: `Failed to restore feeds: ${error}` };
      }
    },
  },

  // ============================================================================
  // NEWS ANALYSIS
  // ============================================================================

  {
    name: 'analyze_news_article',
    description: 'Run AI-powered analysis on a news article by URL. Returns sentiment analysis, market impact, entities, keywords, topics, risk signals, summary, and key points. This uses the ANALYZE button functionality.',
    inputSchema: {
      type: 'object',
      properties: {
        url: {
          type: 'string',
          description: 'Full URL of the news article to analyze',
        },
      },
      required: ['url'],
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.analyzeNewsArticle) {
        return { success: false, error: 'News analysis service not available' };
      }
      try {
        const result = await contexts.analyzeNewsArticle(args.url);
        if (!result.success || !result.data) {
          return {
            success: false,
            error: result.error || 'Analysis failed',
          };
        }

        const analysis = result.data.analysis;
        const sentimentLabel = analysis.sentiment.score > 0.3 ? 'POSITIVE' :
                              analysis.sentiment.score < -0.3 ? 'NEGATIVE' : 'NEUTRAL';

        const formattedAnalysis = `
ARTICLE ANALYSIS
================
Headline: ${result.data.content.headline}
Source: ${result.data.source}
Published: ${result.data.published_at}

SUMMARY
-------
${analysis.summary}

KEY POINTS
----------
${analysis.key_points.map((p, i) => `${i + 1}. ${p}`).join('\n')}

SENTIMENT
---------
Score: ${analysis.sentiment.score.toFixed(2)} (${sentimentLabel})
Intensity: ${analysis.sentiment.intensity.toFixed(2)}
Confidence: ${(analysis.sentiment.confidence * 100).toFixed(0)}%

MARKET IMPACT
-------------
Urgency: ${analysis.market_impact.urgency}
Prediction: ${analysis.market_impact.prediction.replace('_', ' ').toUpperCase()}

RISK SIGNALS
------------
${Object.entries(analysis.risk_signals).map(([type, signal]) =>
  `${type.toUpperCase()}: ${signal.level}${signal.details ? ` - ${signal.details}` : ''}`
).join('\n')}

TOPICS: ${analysis.topics.join(', ')}

KEYWORDS: ${analysis.keywords.slice(0, 10).join(', ')}

ORGANIZATIONS
-------------
${analysis.entities.organizations.length > 0
  ? analysis.entities.organizations.map(org =>
      `- ${org.name}${org.ticker ? ` (${org.ticker})` : ''} | ${org.sector} | Sentiment: ${org.sentiment.toFixed(2)}`
    ).join('\n')
  : 'None identified'}

Credits used: ${result.data.credits_used} | Remaining: ${result.data.credits_remaining.toLocaleString()}
`;

        return {
          success: true,
          data: result.data,
          message: formattedAnalysis,
        };
      } catch (error) {
        return { success: false, error: `Failed to analyze article: ${error}` };
      }
    },
  },

  // ============================================================================
  // NEWS STATISTICS
  // ============================================================================

  {
    name: 'get_news_sentiment_stats',
    description: 'Get overall market sentiment statistics from current news articles (bullish, bearish, neutral counts and net score)',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      if (!contexts.getNewsSentimentStats) {
        return { success: false, error: 'News service not available' };
      }
      try {
        const stats = await contexts.getNewsSentimentStats();
        const scoreLabel = stats.netScore > 0.1 ? 'BULLISH' :
                          stats.netScore < -0.1 ? 'BEARISH' : 'NEUTRAL';

        return {
          success: true,
          data: stats,
          message: `NEWS SENTIMENT OVERVIEW
=======================
Total Articles: ${stats.total}
Alert Count: ${stats.alertCount} (FLASH/URGENT)

Sentiment Distribution:
- Bullish: ${stats.bullish} (${Math.round((stats.bullish / stats.total) * 100)}%)
- Bearish: ${stats.bearish} (${Math.round((stats.bearish / stats.total) * 100)}%)
- Neutral: ${stats.neutral} (${Math.round((stats.neutral / stats.total) * 100)}%)

Net Sentiment Score: ${stats.netScore.toFixed(2)} (${scoreLabel})`,
        };
      } catch (error) {
        return { success: false, error: `Failed to get sentiment stats: ${error}` };
      }
    },
  },

  {
    name: 'get_available_news_categories',
    description: 'Get list of available news categories for filtering',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      const categories = contexts.getAvailableCategories?.() || [
        'MARKETS', 'CRYPTO', 'TECH', 'ENERGY', 'REGULATORY',
        'ECONOMIC', 'EARNINGS', 'BANKING', 'GEOPOLITICS', 'GENERAL'
      ];
      return {
        success: true,
        data: categories,
        message: `Available news categories:\n${categories.map((c, i) => `${i + 1}. ${c}`).join('\n')}`,
      };
    },
  },

  {
    name: 'get_available_news_regions',
    description: 'Get list of available news regions for filtering',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (args, contexts): Promise<InternalToolResult> => {
      const regions = contexts.getAvailableRegions?.() || [
        'GLOBAL', 'US', 'EU', 'ASIA', 'UK', 'INDIA', 'CHINA', 'LATAM'
      ];
      return {
        success: true,
        data: regions,
        message: `Available news regions:\n${regions.map((r, i) => `${i + 1}. ${r}`).join('\n')}`,
      };
    },
  },
];

export default newsTools;
