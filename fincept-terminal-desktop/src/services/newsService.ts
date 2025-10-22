// FinceptTerminal Professional News RSS Feed Service
// Fetches real-time news from 20+ financial RSS feeds

export interface NewsArticle {
  id: string;
  time: string;
  priority: 'FLASH' | 'URGENT' | 'BREAKING' | 'ROUTINE';
  category: string;
  headline: string;
  summary: string;
  source: string;
  region: string;
  sentiment: 'BULLISH' | 'BEARISH' | 'NEUTRAL';
  impact: 'HIGH' | 'MEDIUM' | 'LOW';
  tickers: string[];
  classification: string;
  link?: string;
  pubDate?: Date;
}

// 20+ RSS Feed Sources - Real working feeds
const RSS_FEEDS = [
  // Major Financial News
  {
    name: 'Reuters Business',
    url: 'https://www.reutersagency.com/feed/?taxonomy=best-topics&post_type=best',
    category: 'MARKETS',
    region: 'GLOBAL',
    source: 'REUTERS'
  },
  {
    name: 'CNBC Top News',
    url: 'https://www.cnbc.com/id/100003114/device/rss/rss.html',
    category: 'MARKETS',
    region: 'US',
    source: 'CNBC'
  },
  {
    name: 'MarketWatch',
    url: 'https://www.marketwatch.com/rss/topstories',
    category: 'MARKETS',
    region: 'US',
    source: 'MARKETWATCH'
  },
  {
    name: 'WSJ Markets',
    url: 'https://feeds.a.dj.com/rss/RSSMarketsMain.xml',
    category: 'MARKETS',
    region: 'US',
    source: 'WSJ'
  },
  {
    name: 'Yahoo Finance',
    url: 'https://finance.yahoo.com/news/rssindex',
    category: 'MARKETS',
    region: 'US',
    source: 'YAHOO'
  },
  {
    name: 'Seeking Alpha',
    url: 'https://seekingalpha.com/feed.xml',
    category: 'MARKETS',
    region: 'US',
    source: 'SEEKING ALPHA'
  },
  {
    name: 'Investing.com',
    url: 'https://www.investing.com/rss/news.rss',
    category: 'MARKETS',
    region: 'GLOBAL',
    source: 'INVESTING.COM'
  },
  {
    name: 'Barrons',
    url: 'https://www.barrons.com/articles/rss',
    category: 'MARKETS',
    region: 'US',
    source: 'BARRONS'
  },

  // Cryptocurrency
  {
    name: 'CoinDesk',
    url: 'https://www.coindesk.com/arc/outboundfeeds/rss/',
    category: 'CRYPTO',
    region: 'GLOBAL',
    source: 'COINDESK'
  },
  {
    name: 'CoinTelegraph',
    url: 'https://cointelegraph.com/rss',
    category: 'CRYPTO',
    region: 'GLOBAL',
    source: 'COINTELEGRAPH'
  },
  {
    name: 'Decrypt',
    url: 'https://decrypt.co/feed',
    category: 'CRYPTO',
    region: 'GLOBAL',
    source: 'DECRYPT'
  },

  // Technology
  {
    name: 'TechCrunch',
    url: 'https://techcrunch.com/feed/',
    category: 'TECH',
    region: 'US',
    source: 'TECHCRUNCH'
  },
  {
    name: 'The Verge',
    url: 'https://www.theverge.com/rss/index.xml',
    category: 'TECH',
    region: 'US',
    source: 'THE VERGE'
  },
  {
    name: 'Ars Technica',
    url: 'https://feeds.arstechnica.com/arstechnica/index',
    category: 'TECH',
    region: 'US',
    source: 'ARS TECHNICA'
  },

  // Business & Economics
  {
    name: 'Forbes',
    url: 'https://www.forbes.com/business/feed/',
    category: 'MARKETS',
    region: 'US',
    source: 'FORBES'
  },
  {
    name: 'Business Insider',
    url: 'https://www.businessinsider.com/rss',
    category: 'MARKETS',
    region: 'US',
    source: 'BUSINESS INSIDER'
  },
  {
    name: 'The Economist',
    url: 'https://www.economist.com/finance-and-economics/rss.xml',
    category: 'ECONOMIC',
    region: 'GLOBAL',
    source: 'THE ECONOMIST'
  },

  // Energy & Commodities
  {
    name: 'Oil Price',
    url: 'https://oilprice.com/rss/main',
    category: 'ENERGY',
    region: 'GLOBAL',
    source: 'OILPRICE'
  },
  {
    name: 'Rigzone',
    url: 'https://www.rigzone.com/news/rss.asp',
    category: 'ENERGY',
    region: 'GLOBAL',
    source: 'RIGZONE'
  },

  // Global Markets
  {
    name: 'FXStreet',
    url: 'https://www.fxstreet.com/rss/news',
    category: 'MARKETS',
    region: 'GLOBAL',
    source: 'FXSTREET'
  },
  {
    name: 'Benzinga',
    url: 'https://www.benzinga.com/feed',
    category: 'MARKETS',
    region: 'US',
    source: 'BENZINGA'
  }
];

// Multiple CORS proxies for fallback
const CORS_PROXIES = [
  'https://api.allorigins.win/raw?url=',
  'https://corsproxy.io/?',
  'https://api.codetabs.com/v1/proxy?quest='
];

// Parse RSS XML to articles
function parseRSSFeed(xmlText: string, feedConfig: typeof RSS_FEEDS[0]): NewsArticle[] {
  const articles: NewsArticle[] = [];

  try {
    const parser = new DOMParser();
    const xmlDoc = parser.parseFromString(xmlText, 'text/xml');

    // Check for parsing errors
    const parserError = xmlDoc.querySelector('parsererror');
    if (parserError) {
      console.error('XML parsing error:', parserError.textContent);
      return articles;
    }

    // Handle both RSS 2.0 and Atom feeds
    const items = xmlDoc.querySelectorAll('item, entry');

    items.forEach((item, index) => {
      try {
        const title = item.querySelector('title')?.textContent || '';
        const description = item.querySelector('description, summary')?.textContent || '';
        const link = item.querySelector('link')?.textContent || item.querySelector('link')?.getAttribute('href') || '';
        const pubDateText = item.querySelector('pubDate, published, updated')?.textContent || '';

        if (!title) return; // Skip items without title

        const pubDate = pubDateText ? new Date(pubDateText) : new Date();
        const timeStr = pubDate.toLocaleTimeString('en-US', { hour12: false });

        // Extract tickers from title and description (simple regex)
        const tickerRegex = /\b[A-Z]{1,5}\b/g;
        const tickersFound = (title + ' ' + description).match(tickerRegex) || [];
        const tickers = [...new Set(tickersFound.slice(0, 5))]; // Limit to 5 unique tickers

        // Determine priority based on keywords
        let priority: NewsArticle['priority'] = 'ROUTINE';
        const titleLower = title.toLowerCase();
        if (titleLower.includes('breaking') || titleLower.includes('alert')) {
          priority = 'FLASH';
        } else if (titleLower.includes('urgent') || titleLower.includes('emergency')) {
          priority = 'URGENT';
        } else if (titleLower.includes('announce') || titleLower.includes('report')) {
          priority = 'BREAKING';
        }

        // Determine sentiment based on keywords
        let sentiment: NewsArticle['sentiment'] = 'NEUTRAL';
        const textLower = (title + ' ' + description).toLowerCase();
        const bullishWords = ['surge', 'rally', 'gain', 'beat', 'growth', 'up', 'rise', 'jump', 'soar', 'high', 'record'];
        const bearishWords = ['fall', 'drop', 'decline', 'loss', 'down', 'crash', 'plunge', 'miss', 'weak', 'low'];

        const bullishCount = bullishWords.filter(word => textLower.includes(word)).length;
        const bearishCount = bearishWords.filter(word => textLower.includes(word)).length;

        if (bullishCount > bearishCount && bullishCount > 0) sentiment = 'BULLISH';
        else if (bearishCount > bullishCount && bearishCount > 0) sentiment = 'BEARISH';

        // Determine impact
        let impact: NewsArticle['impact'] = 'LOW';
        if (priority === 'FLASH' || priority === 'URGENT') impact = 'HIGH';
        else if (priority === 'BREAKING') impact = 'MEDIUM';

        // Determine category from feed config or keywords
        let category = feedConfig.category;
        if (textLower.includes('earnings') || textLower.includes('profit') || textLower.includes('revenue')) category = 'EARNINGS';
        else if (textLower.includes('fed') || textLower.includes('inflation') || textLower.includes('gdp') || textLower.includes('jobs')) category = 'ECONOMIC';
        else if (textLower.includes('tech') || textLower.includes('ai') || textLower.includes('software') || textLower.includes('cloud')) category = 'TECH';
        else if (textLower.includes('energy') || textLower.includes('oil') || textLower.includes('gas') || textLower.includes('opec')) category = 'ENERGY';
        else if (textLower.includes('bank') || textLower.includes('financial') || textLower.includes('credit')) category = 'BANKING';
        else if (textLower.includes('crypto') || textLower.includes('bitcoin') || textLower.includes('ethereum') || textLower.includes('blockchain')) category = 'CRYPTO';
        else if (textLower.includes('china') || textLower.includes('russia') || textLower.includes('ukraine') || textLower.includes('war')) category = 'GEOPOLITICS';

        articles.push({
          id: `${feedConfig.source}-${Date.now()}-${index}`,
          time: timeStr,
          priority,
          category,
          headline: title.slice(0, 200), // Limit headline length
          summary: description.slice(0, 300).replace(/<[^>]*>/g, ''), // Remove HTML tags
          source: feedConfig.source,
          region: feedConfig.region,
          sentiment,
          impact,
          tickers,
          classification: 'PUBLIC',
          link,
          pubDate
        });
      } catch (err) {
        console.error('Error parsing item:', err);
      }
    });
  } catch (error) {
    console.error('Error parsing RSS feed:', error);
  }

  return articles;
}

// Fetch single RSS feed with single proxy attempt (optimized for speed)
async function fetchRSSFeed(feedConfig: typeof RSS_FEEDS[0]): Promise<NewsArticle[]> {
  try {
    // Only use first proxy for speed
    const proxyUrl = `${CORS_PROXIES[0]}${encodeURIComponent(feedConfig.url)}`;

    const response = await fetch(proxyUrl, {
      headers: {
        'Accept': 'application/rss+xml, application/xml, text/xml, */*'
      },
      signal: AbortSignal.timeout(5000) // Reduced to 5 second timeout
    });

    if (!response.ok) {
      return [];
    }

    const xmlText = await response.text();
    const articles = parseRSSFeed(xmlText, feedConfig);

    if (articles.length > 0) {
      console.log(`✓ Fetched ${articles.length} articles from ${feedConfig.name}`);
    }

    return articles;
  } catch (error) {
    // Silent fail for speed
    return [];
  }
}

// Fetch all RSS feeds with batching (optimized for speed)
export async function fetchAllNews(): Promise<NewsArticle[]> {
  console.log('Fetching news feeds (optimized batch mode)...');

  const allArticles: NewsArticle[] = [];
  const BATCH_SIZE = 5; // Fetch 5 feeds at a time
  const TARGET_ARTICLES = 30; // Stop early if we get enough articles

  // Process feeds in batches
  for (let i = 0; i < RSS_FEEDS.length; i += BATCH_SIZE) {
    const batch = RSS_FEEDS.slice(i, i + BATCH_SIZE);
    const promises = batch.map(feed => fetchRSSFeed(feed));
    const results = await Promise.allSettled(promises);

    results.forEach((result) => {
      if (result.status === 'fulfilled' && result.value.length > 0) {
        allArticles.push(...result.value);
      }
    });

    console.log(`Batch ${Math.floor(i / BATCH_SIZE) + 1}: Total articles: ${allArticles.length}`);

    // Early exit if we have enough articles
    if (allArticles.length >= TARGET_ARTICLES) {
      console.log(`✓ Reached ${allArticles.length} articles, stopping fetch`);
      break;
    }
  }

  // Sort by publication date (newest first)
  allArticles.sort((a, b) => {
    const dateA = a.pubDate || new Date(0);
    const dateB = b.pubDate || new Date(0);
    return dateB.getTime() - dateA.getTime();
  });

  console.log(`✓ Total articles loaded: ${allArticles.length}`);
  return allArticles;
}

// Fetch news with caching (cache for 5 minutes)
let cachedNews: NewsArticle[] = [];
let lastFetchTime = 0;
const CACHE_DURATION = 5 * 60 * 1000; // 5 minutes
let useMockData = false;

export async function fetchNewsWithCache(): Promise<NewsArticle[]> {
  const now = Date.now();

  // Return cached news if still valid
  if (cachedNews.length > 0 && (now - lastFetchTime) < CACHE_DURATION) {
    console.log('Returning cached news');
    return cachedNews;
  }

  console.log('Fetching fresh news...');

  // Start with mock data immediately for instant display
  if (cachedNews.length === 0) {
    cachedNews = generateMockNews();
    useMockData = true;
  }

  // Try to fetch real news in background
  fetchAllNews().then(fetchedNews => {
    // If we got at least 10 articles, replace with real data
    if (fetchedNews.length >= 10) {
      console.log(`✓ Successfully loaded ${fetchedNews.length} real news articles`);
      cachedNews = fetchedNews;
      useMockData = false;
      lastFetchTime = Date.now();
    } else {
      console.warn(`⚠ Only fetched ${fetchedNews.length} articles, keeping mock data`);
    }
  }).catch(err => {
    console.error('Failed to fetch news:', err);
  });

  lastFetchTime = now;
  return cachedNews;
}

// Check if currently using mock data
export function isUsingMockData(): boolean {
  return useMockData;
}

// Get RSS feed count
export function getRSSFeedCount(): number {
  return RSS_FEEDS.length;
}

// Get active source names
export function getActiveSources(): string[] {
  return RSS_FEEDS.map(feed => feed.source);
}

// Generate mock news articles as fallback
function generateMockNews(): NewsArticle[] {
  const mockArticles: NewsArticle[] = [
    {
      id: 'mock-1',
      time: new Date().toLocaleTimeString('en-US', { hour12: false }),
      priority: 'FLASH',
      category: 'MARKETS',
      headline: 'BREAKING: S&P 500 Reaches All-Time High on Strong Tech Earnings',
      summary: 'The S&P 500 index surged to a record high today as major technology companies reported better-than-expected quarterly earnings. Investors are optimistic about continued growth in the AI and cloud computing sectors.',
      source: 'REUTERS',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['SPY', 'AAPL', 'MSFT', 'GOOGL'],
      classification: 'PUBLIC',
      link: 'https://www.reuters.com',
      pubDate: new Date()
    },
    {
      id: 'mock-2',
      time: new Date(Date.now() - 300000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'URGENT',
      category: 'ECONOMIC',
      headline: 'Federal Reserve Signals Potential Rate Cut in Q4 2025',
      summary: 'Fed Chairman indicated in today\'s speech that the central bank is considering interest rate adjustments based on recent inflation data showing continued cooling in consumer prices.',
      source: 'WSJ',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['TLT', 'IEF', 'SHY'],
      classification: 'PUBLIC',
      link: 'https://www.wsj.com',
      pubDate: new Date(Date.now() - 300000)
    },
    {
      id: 'mock-3',
      time: new Date(Date.now() - 600000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'BREAKING',
      category: 'EARNINGS',
      headline: 'Apple Reports Record iPhone Sales, Beats Estimates',
      summary: 'Apple Inc. posted quarterly revenue of $89.5 billion, exceeding analyst expectations. iPhone sales drove the growth with strong demand in China and India markets.',
      source: 'CNBC',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['AAPL'],
      classification: 'PUBLIC',
      link: 'https://www.cnbc.com',
      pubDate: new Date(Date.now() - 600000)
    },
    {
      id: 'mock-4',
      time: new Date(Date.now() - 900000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'BREAKING',
      category: 'CRYPTO',
      headline: 'Bitcoin Surges Past $75,000 on ETF Inflows',
      summary: 'Bitcoin reached a new milestone as institutional investors continue pouring money into spot Bitcoin ETFs. Total crypto market cap exceeds $2.5 trillion.',
      source: 'COINDESK',
      region: 'GLOBAL',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['BTC', 'ETH'],
      classification: 'PUBLIC',
      link: 'https://www.coindesk.com',
      pubDate: new Date(Date.now() - 900000)
    },
    {
      id: 'mock-5',
      time: new Date(Date.now() - 1200000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'ROUTINE',
      category: 'ENERGY',
      headline: 'Oil Prices Steady as OPEC+ Maintains Production Levels',
      summary: 'Crude oil futures traded flat as OPEC+ announced it will maintain current production quotas through the end of the year. WTI crude at $78.50/barrel.',
      source: 'OILPRICE',
      region: 'GLOBAL',
      sentiment: 'NEUTRAL',
      impact: 'LOW',
      tickers: ['USO', 'XLE'],
      classification: 'PUBLIC',
      link: 'https://oilprice.com',
      pubDate: new Date(Date.now() - 1200000)
    },
    {
      id: 'mock-6',
      time: new Date(Date.now() - 1500000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'BREAKING',
      category: 'TECH',
      headline: 'Microsoft Unveils Advanced AI Models for Enterprise',
      summary: 'Microsoft announced new AI capabilities integrated into Azure cloud platform, targeting enterprise customers with enhanced automation and analytics tools.',
      source: 'TECHCRUNCH',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['MSFT'],
      classification: 'PUBLIC',
      link: 'https://techcrunch.com',
      pubDate: new Date(Date.now() - 1500000)
    },
    {
      id: 'mock-7',
      time: new Date(Date.now() - 1800000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'URGENT',
      category: 'BANKING',
      headline: 'JPMorgan Chase Reports Strong Q3 Results, Raises Guidance',
      summary: 'JPMorgan posted net income of $13.2 billion for the quarter, driven by robust investment banking and trading revenue. Bank raised full-year earnings guidance.',
      source: 'MARKETWATCH',
      region: 'US',
      sentiment: 'BULLISH',
      impact: 'HIGH',
      tickers: ['JPM', 'BAC', 'XLF'],
      classification: 'PUBLIC',
      link: 'https://www.marketwatch.com',
      pubDate: new Date(Date.now() - 1800000)
    },
    {
      id: 'mock-8',
      time: new Date(Date.now() - 2100000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'ROUTINE',
      category: 'MARKETS',
      headline: 'European Markets Close Mixed on Economic Data',
      summary: 'European stock indices ended the session with mixed results. FTSE 100 up 0.3%, DAX down 0.2% as investors digest latest GDP figures from Germany and France.',
      source: 'INVESTING.COM',
      region: 'EUROPE',
      sentiment: 'NEUTRAL',
      impact: 'LOW',
      tickers: ['EWG', 'EWU', 'VGK'],
      classification: 'PUBLIC',
      link: 'https://www.investing.com',
      pubDate: new Date(Date.now() - 2100000)
    },
    {
      id: 'mock-9',
      time: new Date(Date.now() - 2400000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'BREAKING',
      category: 'GEOPOLITICS',
      headline: 'US-China Trade Talks Resume, Markets Rally',
      summary: 'Renewed trade negotiations between Washington and Beijing boosted investor sentiment. Both sides expressed optimism about reaching a comprehensive trade agreement.',
      source: 'REUTERS',
      region: 'GLOBAL',
      sentiment: 'BULLISH',
      impact: 'MEDIUM',
      tickers: ['FXI', 'MCHI', 'SPY'],
      classification: 'PUBLIC',
      link: 'https://www.reuters.com',
      pubDate: new Date(Date.now() - 2400000)
    },
    {
      id: 'mock-10',
      time: new Date(Date.now() - 2700000).toLocaleTimeString('en-US', { hour12: false }),
      priority: 'ROUTINE',
      category: 'EARNINGS',
      headline: 'Tesla Misses Delivery Targets Despite Production Increase',
      summary: 'Tesla reported Q3 deliveries below analyst expectations despite ramping up production. Stock declined 3% in after-hours trading on the news.',
      source: 'CNBC',
      region: 'US',
      sentiment: 'BEARISH',
      impact: 'MEDIUM',
      tickers: ['TSLA'],
      classification: 'PUBLIC',
      link: 'https://www.cnbc.com',
      pubDate: new Date(Date.now() - 2700000)
    }
  ];

  return mockArticles;
}
