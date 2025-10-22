import React, { useState, useEffect } from 'react';
import { fetchNewsWithCache, type NewsArticle, getRSSFeedCount, getActiveSources, isUsingMockData } from '../../services/newsService';

const NewsTab: React.FC = () => {
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeFilter, setActiveFilter] = useState('ALL');
  const [newsUpdateCount, setNewsUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(0);
  const [newsArticles, setNewsArticles] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeSources, setActiveSources] = useState<string[]>([]);

  // Professional Financial Terminal Colors
  const TERMINAL_ORANGE = '#FFA500';
  const TERMINAL_WHITE = '#FFFFFF';
  const TERMINAL_RED = '#FF0000';
  const TERMINAL_GREEN = '#00C800';
  const TERMINAL_YELLOW = '#FFFF00';
  const TERMINAL_GRAY = '#787878';
  const TERMINAL_BLUE = '#6496FA';
  const TERMINAL_PURPLE = '#C864FF';
  const TERMINAL_CYAN = '#00FFFF';
  const TERMINAL_DARK_BG = '#000000';
  const TERMINAL_PANEL_BG = '#0a0a0a';

  // Fetch real news on mount and every 5 minutes
  useEffect(() => {
    const loadNews = async () => {
      setLoading(true);
      try {
        const articles = await fetchNewsWithCache();
        setNewsArticles(articles);
        setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
        setActiveSources(getActiveSources());
        setNewsUpdateCount(prev => prev + 1);
      } catch (error) {
        console.error('Error fetching news:', error);
      } finally {
        setLoading(false);
      }
    };

    loadNews();

    // Refresh news every 5 minutes
    const newsRefreshInterval = setInterval(loadNews, 5 * 60 * 1000);

    return () => clearInterval(newsRefreshInterval);
  }, []);

  useEffect(() => {
    const timer = setInterval(() => {
      setCurrentTime(new Date());
    }, 1000);
    return () => clearInterval(timer);
  }, []);

  // Generate news ticker from articles
  const newsTickerText = newsArticles.slice(0, 15).map(a =>
    `🔴 ${a.headline.toUpperCase().slice(0, 100)}`
  ).join(' • ') || 'Loading news feeds...';

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'FLASH': return TERMINAL_RED;
      case 'URGENT': return TERMINAL_ORANGE;
      case 'BREAKING': return TERMINAL_YELLOW;
      case 'ROUTINE': return TERMINAL_GREEN;
      default: return TERMINAL_GRAY;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return TERMINAL_GREEN;
      case 'BEARISH': return TERMINAL_RED;
      case 'NEUTRAL': return TERMINAL_YELLOW;
      default: return TERMINAL_GRAY;
    }
  };

  const getImpactColor = (impact: string) => {
    switch (impact) {
      case 'HIGH': return TERMINAL_RED;
      case 'MEDIUM': return TERMINAL_YELLOW;
      case 'LOW': return TERMINAL_GREEN;
      default: return TERMINAL_GRAY;
    }
  };

  const filteredNews = activeFilter === 'ALL'
    ? newsArticles
    : newsArticles.filter(article => article.category === activeFilter);

  // Calculate stats from real data
  const bullishCount = newsArticles.filter(a => a.sentiment === 'BULLISH').length;
  const bearishCount = newsArticles.filter(a => a.sentiment === 'BEARISH').length;
  const neutralCount = newsArticles.filter(a => a.sentiment === 'NEUTRAL').length;
  const totalCount = newsArticles.length || 1;
  const sentimentScore = ((bullishCount - bearishCount) / totalCount).toFixed(2);

  // Category counts
  const breakingCount = newsArticles.filter(a => a.priority === 'BREAKING' || a.priority === 'FLASH').length;
  const earningsCount = newsArticles.filter(a => a.category === 'EARNINGS').length;
  const economicCount = newsArticles.filter(a => a.category === 'ECONOMIC').length;

  return (
    <div style={{
      height: '100%',
      backgroundColor: TERMINAL_DARK_BG,
      color: TERMINAL_WHITE,
      fontFamily: 'Consolas, monospace',
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '12px'
    }}>
      {/* Complex Header with News Ticker */}
      <div style={{
        backgroundColor: TERMINAL_PANEL_BG,
        borderBottom: `1px solid ${TERMINAL_GRAY}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        {/* Main Status Line */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: TERMINAL_ORANGE, fontWeight: 'bold' }}>FINCEPT TERMINAL PROFESSIONAL NEWS</span>
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_RED, fontWeight: 'bold', animation: 'blink 1s infinite' }}>LIVE FEED</span>
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_YELLOW }}>ALERTS: {alertCount}</span>
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_GREEN }}>● {getRSSFeedCount()} SOURCES</span>
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_WHITE }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
        </div>

        {/* News Ticker */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px', marginBottom: '2px' }}>
          <span style={{ color: TERMINAL_YELLOW, fontWeight: 'bold' }}>BREAKING:</span>
          <div style={{
            flex: 1,
            overflow: 'hidden',
            whiteSpace: 'nowrap'
          }}>
            <div style={{
              display: 'inline-block',
              animation: 'scroll-left 90s linear infinite',
              color: TERMINAL_WHITE
            }}>
              {newsTickerText}
            </div>
          </div>
        </div>

        {/* Sources and Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '10px', flexWrap: 'wrap' }}>
          <span style={{ color: TERMINAL_GRAY }}>ACTIVE SOURCES:</span>
          {activeSources.slice(0, 12).map((source, idx) => (
            <span key={idx} style={{ color: TERMINAL_GREEN }}>{source}●</span>
          ))}
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_GRAY }}>ARTICLES:</span>
          <span style={{ color: TERMINAL_CYAN }}>{newsArticles.length}</span>
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <span style={{ color: TERMINAL_GRAY }}>STATUS:</span>
          <span style={{ color: loading ? TERMINAL_YELLOW : TERMINAL_GREEN }}>
            {loading ? 'UPDATING...' : (isUsingMockData() ? '● DEMO MODE' : '● LIVE')}
          </span>
        </div>
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: TERMINAL_PANEL_BG,
        borderBottom: `1px solid ${TERMINAL_GRAY}`,
        padding: '2px 4px',
        flexShrink: 0
      }}>
        <div style={{ display: 'grid', gridTemplateColumns: 'repeat(12, 1fr)', gap: '2px' }}>
          {[
            { key: "F1", label: "ALL", filter: "ALL" },
            { key: "F2", label: "MARKETS", filter: "MARKETS" },
            { key: "F3", label: "EARNINGS", filter: "EARNINGS" },
            { key: "F4", label: "ECON", filter: "ECONOMIC" },
            { key: "F5", label: "TECH", filter: "TECH" },
            { key: "F6", label: "ENERGY", filter: "ENERGY" },
            { key: "F7", label: "BANKING", filter: "BANKING" },
            { key: "F8", label: "CRYPTO", filter: "CRYPTO" },
            { key: "F9", label: "GEOPO", filter: "GEOPOLITICS" },
            { key: "F10", label: "FLASH", filter: "FLASH" },
            { key: "F11", label: "URGENT", filter: "URGENT" },
            { key: "F12", label: "REFRESH", filter: "REFRESH" }
          ].map(item => (
            <button key={item.key}
              onClick={() => {
                if (item.filter === 'REFRESH') {
                  window.location.reload();
                } else if (item.filter === 'FLASH') {
                  setActiveFilter('ALL');
                  setNewsArticles(prev => prev.filter(a => a.priority === 'FLASH'));
                } else if (item.filter === 'URGENT') {
                  setActiveFilter('ALL');
                  setNewsArticles(prev => prev.filter(a => a.priority === 'URGENT' || a.priority === 'FLASH'));
                } else {
                  setActiveFilter(item.filter);
                }
              }}
              style={{
                backgroundColor: activeFilter === item.filter ? TERMINAL_ORANGE : TERMINAL_DARK_BG,
                border: `1px solid ${TERMINAL_GRAY}`,
                color: activeFilter === item.filter ? TERMINAL_DARK_BG : TERMINAL_WHITE,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeFilter === item.filter ? 'bold' : 'normal',
                cursor: 'pointer'
              }}>
              <span style={{ color: TERMINAL_YELLOW }}>{item.key}:</span>
              <span style={{ marginLeft: '2px' }}>{item.label}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Main Content - 3 Column Dense News Layout */}
      <div style={{ flex: 1, overflow: 'auto', padding: '4px', backgroundColor: '#050505' }}>
        <div style={{ display: 'flex', gap: '4px', height: '100%' }}>

          {/* Left Panel - Breaking News Stream (Primary Feed) */}
          <div style={{
            flex: 1,
            backgroundColor: TERMINAL_PANEL_BG,
            border: `1px solid ${TERMINAL_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: TERMINAL_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              PRIMARY NEWS FEED [{activeFilter}]
            </div>
            <div style={{ borderBottom: `1px solid ${TERMINAL_GRAY}`, marginBottom: '4px' }}></div>

            {loading && newsArticles.length === 0 ? (
              <div style={{ color: TERMINAL_YELLOW, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
                Loading news from {getRSSFeedCount()} RSS feeds...
              </div>
            ) : filteredNews.length === 0 ? (
              <div style={{ color: TERMINAL_GRAY, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
                No articles found for filter: {activeFilter}
              </div>
            ) : null}

            {filteredNews.slice(0, Math.ceil(filteredNews.length / 3)).map((article, index) => (
              <div key={article.id} style={{
                marginBottom: '6px',
                borderLeft: `3px solid ${getPriorityColor(article.priority)}`,
                paddingLeft: '4px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                cursor: article.link ? 'pointer' : 'default'
              }}
              onClick={() => article.link && window.open(article.link, '_blank')}
              >
                <div style={{ display: 'flex', gap: '4px', marginBottom: '1px', fontSize: '8px' }}>
                  <span style={{ color: TERMINAL_GRAY, minWidth: '50px' }}>{article.time}</span>
                  <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{article.priority}]
                  </span>
                  <span style={{ color: TERMINAL_BLUE, minWidth: '50px' }}>[{article.category}]</span>
                  <span style={{ color: TERMINAL_PURPLE, minWidth: '30px' }}>{article.region}</span>
                  <span style={{ color: getSentimentColor(article.sentiment), minWidth: '50px' }}>{article.sentiment}</span>
                </div>

                <div style={{ color: TERMINAL_WHITE, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.headline}
                </div>

                <div style={{ color: TERMINAL_GRAY, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.summary}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                  <span style={{ color: TERMINAL_CYAN }}>{article.source}</span>
                  <span style={{ color: getImpactColor(article.impact) }}>IMPACT: {article.impact}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Center Panel - Secondary News Feed */}
          <div style={{
            flex: 1,
            backgroundColor: TERMINAL_PANEL_BG,
            border: `1px solid ${TERMINAL_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: TERMINAL_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              SECONDARY NEWS FEED [{activeFilter}]
            </div>
            <div style={{ borderBottom: `1px solid ${TERMINAL_GRAY}`, marginBottom: '4px' }}></div>

            {filteredNews.slice(Math.ceil(filteredNews.length / 3), Math.ceil(filteredNews.length * 2 / 3)).map((article, index) => (
              <div key={article.id} style={{
                marginBottom: '6px',
                borderLeft: `3px solid ${getPriorityColor(article.priority)}`,
                paddingLeft: '4px',
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                cursor: article.link ? 'pointer' : 'default'
              }}
              onClick={() => article.link && window.open(article.link, '_blank')}
              >
                <div style={{ display: 'flex', gap: '4px', marginBottom: '1px', fontSize: '8px' }}>
                  <span style={{ color: TERMINAL_GRAY, minWidth: '50px' }}>{article.time}</span>
                  <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{article.priority}]
                  </span>
                  <span style={{ color: TERMINAL_BLUE, minWidth: '50px' }}>[{article.category}]</span>
                  <span style={{ color: TERMINAL_PURPLE, minWidth: '30px' }}>{article.region}</span>
                  <span style={{ color: getSentimentColor(article.sentiment), minWidth: '50px' }}>{article.sentiment}</span>
                </div>

                <div style={{ color: TERMINAL_WHITE, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.headline}
                </div>

                <div style={{ color: TERMINAL_GRAY, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.summary}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                  <span style={{ color: TERMINAL_CYAN }}>{article.source}</span>
                  <span style={{ color: getImpactColor(article.impact) }}>IMPACT: {article.impact}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Right Panel - Tertiary Feed + Analytics */}
          <div style={{
            width: '400px',
            backgroundColor: TERMINAL_PANEL_BG,
            border: `1px solid ${TERMINAL_GRAY}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            {/* News Analytics Section */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: TERMINAL_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                REAL-TIME NEWS ANALYTICS
              </div>
              <div style={{ borderBottom: `1px solid ${TERMINAL_GRAY}`, marginBottom: '6px' }}></div>

              {/* Sentiment Analysis */}
              <div style={{ marginBottom: '8px', fontSize: '9px' }}>
                <div style={{ color: TERMINAL_YELLOW, fontWeight: 'bold', marginBottom: '3px' }}>
                  SENTIMENT ANALYSIS
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Bullish Articles:</span>
                  <span style={{ color: TERMINAL_GREEN }}>{bullishCount} ({Math.round((bullishCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Bearish Articles:</span>
                  <span style={{ color: TERMINAL_RED }}>{bearishCount} ({Math.round((bearishCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Neutral Articles:</span>
                  <span style={{ color: TERMINAL_YELLOW }}>{neutralCount} ({Math.round((neutralCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px', paddingTop: '2px', borderTop: `1px solid ${TERMINAL_GRAY}` }}>
                  <span style={{ color: TERMINAL_CYAN, fontWeight: 'bold' }}>Sentiment Score:</span>
                  <span style={{ color: parseFloat(sentimentScore) > 0 ? TERMINAL_GREEN : parseFloat(sentimentScore) < 0 ? TERMINAL_RED : TERMINAL_YELLOW, fontWeight: 'bold' }}>
                    {sentimentScore} ({parseFloat(sentimentScore) > 0 ? 'BULLISH' : parseFloat(sentimentScore) < 0 ? 'BEARISH' : 'NEUTRAL'})
                  </span>
                </div>
              </div>

              {/* News Flow Statistics */}
              <div style={{ marginBottom: '8px', fontSize: '9px' }}>
                <div style={{ color: TERMINAL_YELLOW, fontWeight: 'bold', marginBottom: '3px' }}>
                  NEWS FLOW STATS
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Total Articles:</span>
                  <span style={{ color: TERMINAL_WHITE }}>{newsArticles.length}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Breaking News:</span>
                  <span style={{ color: TERMINAL_RED }}>{breakingCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Earnings Reports:</span>
                  <span style={{ color: TERMINAL_BLUE }}>{earningsCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>Economic Data:</span>
                  <span style={{ color: TERMINAL_PURPLE }}>{economicCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: TERMINAL_GRAY }}>High Priority:</span>
                  <span style={{ color: TERMINAL_ORANGE }}>{alertCount}</span>
                </div>
              </div>
            </div>

            {/* Tertiary News Feed */}
            <div style={{ borderTop: `2px solid ${TERMINAL_GRAY}`, paddingTop: '4px' }}>
              <div style={{ color: TERMINAL_ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                ADDITIONAL HEADLINES
              </div>
              <div style={{ borderBottom: `1px solid ${TERMINAL_GRAY}`, marginBottom: '4px' }}></div>

              {filteredNews.slice(Math.ceil(filteredNews.length * 2 / 3)).map((article, index) => (
                <div key={article.id} style={{
                  marginBottom: '6px',
                  borderLeft: `3px solid ${getPriorityColor(article.priority)}`,
                  paddingLeft: '4px',
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  cursor: article.link ? 'pointer' : 'default'
                }}
                onClick={() => article.link && window.open(article.link, '_blank')}
                >
                  <div style={{ display: 'flex', gap: '4px', marginBottom: '1px', fontSize: '8px' }}>
                    <span style={{ color: TERMINAL_GRAY, minWidth: '50px' }}>{article.time}</span>
                    <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                      [{article.priority}]
                    </span>
                    <span style={{ color: TERMINAL_BLUE, minWidth: '50px' }}>[{article.category}]</span>
                  </div>

                  <div style={{ color: TERMINAL_WHITE, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                    {article.headline}
                  </div>

                  <div style={{ color: TERMINAL_GRAY, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                    {article.summary.slice(0, 150)}...
                  </div>

                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                    <span style={{ color: TERMINAL_CYAN }}>{article.source}</span>
                    <span style={{ color: getSentimentColor(article.sentiment) }}>{article.sentiment}</span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      </div>

      {/* Status Bar */}
      <div style={{
        borderTop: `1px solid ${TERMINAL_GRAY}`,
        backgroundColor: TERMINAL_PANEL_BG,
        padding: '2px 8px',
        fontSize: '10px',
        color: TERMINAL_GRAY,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>FinceptTerminal Professional News v1.0.0 | Real-time from {getRSSFeedCount()} RSS feeds</span>
            <span>Articles: {newsArticles.length} | Filter: {activeFilter} | Updates: {newsUpdateCount}</span>
          </div>
          <div style={{ display: 'flex', gap: '16px' }}>
            <span>High Priority: {alertCount} | Sentiment: {sentimentScore}</span>
            <span>Session: {currentTime.toTimeString().substring(0, 8)}</span>
            <span style={{ color: loading ? TERMINAL_YELLOW : TERMINAL_GREEN }}>
              {loading ? 'UPDATING...' : 'FEED: LIVE'}
            </span>
          </div>
        </div>
      </div>

      {/* CSS for animations and transparent scrollbars */}
      <style>{`
        @keyframes scroll-left {
          0% { transform: translateX(100%); }
          100% { transform: translateX(-100%); }
        }
        @keyframes blink {
          0%, 50% { opacity: 1; }
          51%, 100% { opacity: 0.3; }
        }

        /* Transparent scrollbars for all browsers */
        * {
          scrollbar-width: thin;
          scrollbar-color: rgba(120, 120, 120, 0.3) transparent;
        }

        *::-webkit-scrollbar {
          width: 8px;
          height: 8px;
        }

        *::-webkit-scrollbar-track {
          background: transparent;
        }

        *::-webkit-scrollbar-thumb {
          background-color: rgba(120, 120, 120, 0.3);
          border-radius: 4px;
          border: 2px solid transparent;
        }

        *::-webkit-scrollbar-thumb:hover {
          background-color: rgba(120, 120, 120, 0.5);
        }

        *::-webkit-scrollbar-corner {
          background: transparent;
        }
      `}</style>
    </div>
  );
};

export default NewsTab;
