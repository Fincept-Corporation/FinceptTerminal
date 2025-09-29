import React, { useState, useEffect } from 'react';
import { ExternalLink, Clock, TrendingUp, Globe, DollarSign, Building } from 'lucide-react';

interface NewsArticle {
  id: string;
  title: string;
  summary: string;
  source: string;
  publishedAt: string;
  category: string;
  url: string;
  sentiment: 'positive' | 'negative' | 'neutral';
}

interface NewsCategory {
  key: string;
  label: string;
  icon: React.ReactNode;
  color: string;
}

const NewsTab: React.FC = () => {
  const [activeCategory, setActiveCategory] = useState('all');
  const [articles, setArticles] = useState<NewsArticle[]>([
    {
      id: '1',
      title: 'Federal Reserve Signals Potential Rate Cut in Q2 2024',
      summary: 'Fed officials hint at possible monetary policy adjustments as inflation shows signs of cooling across major economic indicators.',
      source: 'Reuters',
      publishedAt: '2 minutes ago',
      category: 'economics',
      url: '#',
      sentiment: 'positive'
    },
    {
      id: '2',
      title: 'Tech Stocks Rally on AI Investment Surge',
      summary: 'Major technology companies see significant gains as artificial intelligence investments continue to drive market optimism.',
      source: 'Bloomberg',
      publishedAt: '5 minutes ago',
      category: 'technology',
      url: '#',
      sentiment: 'positive'
    },
    {
      id: '3',
      title: 'Oil Prices Drop on Increased Production Outlook',
      summary: 'Crude oil futures decline as OPEC+ members signal potential production increases in response to global demand.',
      source: 'Wall Street Journal',
      publishedAt: '8 minutes ago',
      category: 'commodities',
      url: '#',
      sentiment: 'negative'
    },
    {
      id: '4',
      title: 'European Markets Open Higher on GDP Data',
      summary: 'European indices gain as stronger-than-expected GDP growth data boosts investor confidence in regional recovery.',
      source: 'Financial Times',
      publishedAt: '12 minutes ago',
      category: 'markets',
      url: '#',
      sentiment: 'positive'
    },
    {
      id: '5',
      title: 'Cryptocurrency Regulation Bill Advances in Senate',
      summary: 'Bipartisan cryptocurrency regulation framework moves forward, providing clearer guidelines for digital asset trading.',
      source: 'CNBC',
      publishedAt: '15 minutes ago',
      category: 'cryptocurrency',
      url: '#',
      sentiment: 'neutral'
    },
    {
      id: '6',
      title: 'Banking Sector Shows Strong Q4 Earnings',
      summary: 'Major banks report better-than-expected quarterly results, driven by higher interest margins and loan growth.',
      source: 'MarketWatch',
      publishedAt: '18 minutes ago',
      category: 'banking',
      url: '#',
      sentiment: 'positive'
    },
    {
      id: '7',
      title: 'Supply Chain Disruptions Impact Manufacturing',
      summary: 'Global manufacturing faces headwinds as supply chain bottlenecks persist in key industrial sectors.',
      source: 'Associated Press',
      publishedAt: '22 minutes ago',
      category: 'economics',
      url: '#',
      sentiment: 'negative'
    },
    {
      id: '8',
      title: 'Green Energy Investments Reach Record High',
      summary: 'Renewable energy sector attracts unprecedented investment levels as governments accelerate clean energy transitions.',
      source: 'Reuters',
      publishedAt: '25 minutes ago',
      category: 'energy',
      url: '#',
      sentiment: 'positive'
    }
  ]);

  const categories: NewsCategory[] = [
    { key: 'all', label: 'ALL NEWS', icon: <Globe size={16} />, color: 'text-blue-400' },
    { key: 'markets', label: 'MARKETS', icon: <TrendingUp size={16} />, color: 'text-green-400' },
    { key: 'economics', label: 'ECONOMICS', icon: <DollarSign size={16} />, color: 'text-yellow-400' },
    { key: 'technology', label: 'TECHNOLOGY', icon: <Building size={16} />, color: 'text-purple-400' },
    { key: 'banking', label: 'BANKING', icon: <Building size={16} />, color: 'text-orange-400' },
    { key: 'commodities', label: 'COMMODITIES', icon: <TrendingUp size={16} />, color: 'text-red-400' },
    { key: 'cryptocurrency', label: 'CRYPTO', icon: <DollarSign size={16} />, color: 'text-cyan-400' },
    { key: 'energy', label: 'ENERGY', icon: <Globe size={16} />, color: 'text-emerald-400' }
  ];

  const [lastUpdated, setLastUpdated] = useState(new Date());

  useEffect(() => {
    const timer = setInterval(() => {
      setLastUpdated(new Date());
    }, 30000);

    return () => clearInterval(timer);
  }, []);

  const filteredArticles = activeCategory === 'all'
    ? articles
    : articles.filter(article => article.category === activeCategory);

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'positive': return 'text-green-400';
      case 'negative': return 'text-red-400';
      default: return 'text-gray-400';
    }
  };

  const getSentimentIndicator = (sentiment: string) => {
    switch (sentiment) {
      case 'positive': return '↗';
      case 'negative': return '↘';
      default: return '→';
    }
  };

  return (
    <div className="h-full bg-black text-white flex flex-col">
      {/* Category Navigation */}
      <div className="bg-gray-900 border-b border-gray-700 p-2">
        <div className="flex flex-wrap gap-2">
          {categories.map((category) => (
            <button
              key={category.key}
              onClick={() => setActiveCategory(category.key)}
              className={`flex items-center gap-2 px-3 py-2 text-sm font-medium rounded transition-colors ${
                activeCategory === category.key
                  ? 'bg-orange-600 text-white'
                  : 'bg-gray-800 text-gray-300 hover:bg-gray-700'
              }`}
            >
              <span className={category.color}>{category.icon}</span>
              <span>{category.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Header */}
      <div className="bg-gray-800 border-b border-gray-700 p-4">
        <div className="flex justify-between items-center">
          <h1 className="text-xl font-bold text-orange-500">
            FINANCIAL NEWS ANALYSIS
          </h1>
          <div className="flex items-center gap-4 text-sm text-gray-400">
            <div className="flex items-center gap-1">
              <Clock size={14} />
              <span>Last Updated: {lastUpdated.toLocaleTimeString()}</span>
            </div>
            <div className="text-green-400">● LIVE FEED</div>
          </div>
        </div>
      </div>

      {/* News Counter */}
      <div className="bg-gray-900 border-b border-gray-700 p-3">
        <div className="flex justify-between items-center text-sm">
          <div>
            <span className="text-gray-400">Showing </span>
            <span className="text-white font-semibold">{filteredArticles.length}</span>
            <span className="text-gray-400"> articles</span>
            {activeCategory !== 'all' && (
              <>
                <span className="text-gray-400"> in </span>
                <span className="text-orange-400">
                  {categories.find(c => c.key === activeCategory)?.label}
                </span>
              </>
            )}
          </div>
          <div className="flex gap-4">
            <span className="text-gray-400">Positive: </span>
            <span className="text-green-400">{articles.filter(a => a.sentiment === 'positive').length}</span>
            <span className="text-gray-400">Negative: </span>
            <span className="text-red-400">{articles.filter(a => a.sentiment === 'negative').length}</span>
          </div>
        </div>
      </div>

      {/* News Articles */}
      <div className="flex-1 overflow-y-auto">
        <div className="p-4 space-y-4">
          {filteredArticles.map((article) => (
            <div
              key={article.id}
              className="bg-gray-900 border border-gray-700 rounded-lg p-4 hover:bg-gray-800 transition-colors"
            >
              <div className="flex justify-between items-start mb-2">
                <div className="flex items-center gap-2">
                  <span className={`font-semibold ${getSentimentColor(article.sentiment)}`}>
                    {getSentimentIndicator(article.sentiment)}
                  </span>
                  <span className="text-sm font-medium text-orange-400 uppercase">
                    {article.category}
                  </span>
                </div>
                <div className="flex items-center gap-2 text-sm text-gray-400">
                  <span>{article.source}</span>
                  <span>•</span>
                  <span>{article.publishedAt}</span>
                </div>
              </div>

              <h3 className="text-lg font-semibold text-white mb-2 hover:text-orange-400 cursor-pointer">
                {article.title}
              </h3>

              <p className="text-gray-300 text-sm mb-3 leading-relaxed">
                {article.summary}
              </p>

              <div className="flex justify-between items-center">
                <div className="flex items-center gap-4">
                  <span className={`text-sm font-medium ${getSentimentColor(article.sentiment)}`}>
                    {article.sentiment.toUpperCase()}
                  </span>
                </div>
                <button className="flex items-center gap-1 text-sm text-orange-400 hover:text-orange-300 transition-colors">
                  <span>Read More</span>
                  <ExternalLink size={14} />
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Footer */}
      <div className="bg-gray-900 border-t border-gray-700 p-3">
        <div className="flex justify-between items-center text-sm">
          <div className="text-gray-400">
            News sources: Reuters, Bloomberg, WSJ, FT, CNBC, MarketWatch, AP
          </div>
          <div className="text-gray-400">
            Auto-refresh: 30s
          </div>
        </div>
      </div>
    </div>
  );
};

export default NewsTab;