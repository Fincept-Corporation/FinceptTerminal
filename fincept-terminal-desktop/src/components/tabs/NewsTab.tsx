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
  const [selectedArticle, setSelectedArticle] = useState<NewsArticle | null>(null);
  const [showWebView, setShowWebView] = useState(false);

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

  // Manual refresh function with force refresh
  const handleRefresh = async () => {
    setLoading(true);
    try {
      // Force refresh to bypass cache
      const articles = await fetchNewsWithCache(true);
      setNewsArticles(articles);
      setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
      setActiveSources(getActiveSources());
      setNewsUpdateCount(prev => prev + 1);
      setTickerIndex(0); // Reset ticker to first article
    } catch (error) {
      console.error('Error fetching news:', error);
    } finally {
      setLoading(false);
    }
  };

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

  // Generate continuous ticker text with all news items
  const generateContinuousTickerText = () => {
    if (newsArticles.length === 0) {
      return 'Loading news feeds...';
    }

    // Create a long string of all news items
    return newsArticles.map(article =>
      `🔴 ${article.priority} | ${article.source} | ${article.headline.toUpperCase()}     `
    ).join('');
  };

  const newsTickerText = generateContinuousTickerText();

  // Calculate animation duration based on content length (more content = slower speed)
  // Base: 1600 seconds for 1000 characters, scale proportionally
  const tickerDuration = Math.max(1200, Math.min(4800, (newsTickerText.length / 1000) * 1600));

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
          <span style={{ color: TERMINAL_WHITE }}>|</span>
          <button
            onClick={handleRefresh}
            disabled={loading}
            style={{
              backgroundColor: loading ? TERMINAL_GRAY : TERMINAL_GREEN,
              color: TERMINAL_DARK_BG,
              border: 'none',
              padding: '2px 8px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: loading ? 'not-allowed' : 'pointer',
              borderRadius: '2px',
              display: 'flex',
              alignItems: 'center',
              gap: '4px'
            }}
            title="Refresh news feeds"
          >
            🔄 {loading ? 'UPDATING...' : 'REFRESH'}
          </button>
        </div>

        {/* News Ticker - Continuous seamless scroll like Bloomberg */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px', marginBottom: '2px' }}>
          <span style={{ color: TERMINAL_YELLOW, fontWeight: 'bold' }}>BREAKING:</span>
          <div className="ticker-wrap" style={{
            flex: 1,
            overflow: 'hidden',
            position: 'relative',
            height: '20px'
          }}>
            <div className="ticker-move">
              <span className="ticker-item">{newsTickerText}</span>
              <span className="ticker-item">{newsTickerText}</span>
            </div>
          </div>
          <span style={{ color: TERMINAL_GREEN, fontSize: '9px' }}>
            ● LIVE
          </span>
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
              onClick={() => setSelectedArticle(article)}
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
              onClick={() => setSelectedArticle(article)}
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
                onClick={() => setSelectedArticle(article)}
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

      {/* Article Detail Modal */}
      {selectedArticle && (
        <div style={{
          position: 'fixed',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          backgroundColor: 'rgba(0, 0, 0, 0.8)',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999
        }}
        onClick={() => setSelectedArticle(null)}
        >
          <div style={{
            backgroundColor: TERMINAL_PANEL_BG,
            border: `2px solid ${TERMINAL_ORANGE}`,
            borderRadius: '4px',
            width: showWebView ? '90vw' : '600px',
            maxWidth: '95vw',
            height: showWebView ? '90vh' : 'auto',
            maxHeight: '90vh',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: `0 0 20px ${TERMINAL_ORANGE}`,
            transition: 'all 0.3s ease'
          }}
          onClick={(e) => e.stopPropagation()}
          >
            {/* Modal Header */}
            <div style={{
              backgroundColor: TERMINAL_DARK_BG,
              borderBottom: `2px solid ${TERMINAL_ORANGE}`,
              padding: '8px 12px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              flexShrink: 0
            }}>
              <div style={{ display: 'flex', gap: '8px', alignItems: 'center', fontSize: '11px' }}>
                <span style={{ color: TERMINAL_ORANGE, fontWeight: 'bold' }}>ARTICLE DETAILS</span>
                <span style={{ color: TERMINAL_WHITE }}>|</span>
                <span style={{ color: getPriorityColor(selectedArticle.priority), fontWeight: 'bold' }}>
                  [{selectedArticle.priority}]
                </span>
                <span style={{ color: TERMINAL_BLUE }}>[{selectedArticle.category}]</span>
              </div>
              <button
                onClick={() => {
                  setSelectedArticle(null);
                  setShowWebView(false);
                }}
                style={{
                  backgroundColor: TERMINAL_RED,
                  color: TERMINAL_WHITE,
                  border: 'none',
                  padding: '4px 8px',
                  fontSize: '11px',
                  fontWeight: 'bold',
                  cursor: 'pointer',
                  borderRadius: '2px'
                }}
              >
                ✕ CLOSE
              </button>
            </div>

            {/* Modal Content */}
            <div style={{
              padding: showWebView ? '0' : '12px',
              overflow: 'auto',
              flex: 1,
              display: 'flex',
              flexDirection: 'column'
            }}>
              {/* Reader View */}
              {showWebView && selectedArticle.link ? (
                <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
                  {/* Reader View Header */}
                  <div style={{
                    backgroundColor: TERMINAL_DARK_BG,
                    padding: '8px',
                    borderBottom: `1px solid ${TERMINAL_GRAY}`,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '10px'
                  }}>
                    <button
                      onClick={() => setShowWebView(false)}
                      style={{
                        backgroundColor: TERMINAL_ORANGE,
                        color: TERMINAL_DARK_BG,
                        border: 'none',
                        padding: '4px 8px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        borderRadius: '2px'
                      }}
                    >
                      ← BACK TO SUMMARY
                    </button>
                    <span style={{ color: TERMINAL_GRAY }}>|</span>
                    <span style={{ color: TERMINAL_YELLOW, fontWeight: 'bold' }}>READER VIEW</span>
                    <span style={{ color: TERMINAL_GRAY }}>|</span>
                    <span style={{ color: TERMINAL_WHITE, flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                      {selectedArticle.source}
                    </span>
                    <button
                      onClick={async () => {
                        if (window.__TAURI__) {
                          const { openUrl } = await import('@tauri-apps/plugin-opener');
                          await openUrl(selectedArticle.link!);
                        } else {
                          window.open(selectedArticle.link, '_blank');
                        }
                      }}
                      style={{
                        backgroundColor: TERMINAL_BLUE,
                        color: TERMINAL_WHITE,
                        border: 'none',
                        padding: '4px 8px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        borderRadius: '2px'
                      }}
                    >
                      🔗 OPEN FULL ARTICLE
                    </button>
                  </div>

                  {/* Reader View Content */}
                  <div style={{
                    flex: 1,
                    overflow: 'auto',
                    padding: '24px',
                    backgroundColor: '#1a1a1a',
                    color: '#e0e0e0'
                  }}>
                    {/* Article Header */}
                    <div style={{
                      marginBottom: '24px',
                      paddingBottom: '16px',
                      borderBottom: `2px solid ${TERMINAL_ORANGE}`
                    }}>
                      <div style={{
                        display: 'flex',
                        gap: '8px',
                        marginBottom: '12px',
                        fontSize: '11px'
                      }}>
                        <span style={{
                          backgroundColor: getPriorityColor(selectedArticle.priority),
                          color: TERMINAL_DARK_BG,
                          padding: '2px 8px',
                          borderRadius: '2px',
                          fontWeight: 'bold'
                        }}>
                          {selectedArticle.priority}
                        </span>
                        <span style={{
                          backgroundColor: TERMINAL_BLUE,
                          color: TERMINAL_WHITE,
                          padding: '2px 8px',
                          borderRadius: '2px',
                          fontWeight: 'bold'
                        }}>
                          {selectedArticle.category}
                        </span>
                        <span style={{
                          backgroundColor: getSentimentColor(selectedArticle.sentiment),
                          color: TERMINAL_DARK_BG,
                          padding: '2px 8px',
                          borderRadius: '2px',
                          fontWeight: 'bold'
                        }}>
                          {selectedArticle.sentiment}
                        </span>
                      </div>

                      <h1 style={{
                        fontSize: '24px',
                        fontWeight: 'bold',
                        lineHeight: '1.3',
                        marginBottom: '12px',
                        color: TERMINAL_WHITE
                      }}>
                        {selectedArticle.headline}
                      </h1>

                      <div style={{
                        display: 'flex',
                        gap: '16px',
                        fontSize: '12px',
                        color: TERMINAL_GRAY
                      }}>
                        <span>📰 {selectedArticle.source}</span>
                        <span>🕒 {selectedArticle.time}</span>
                        <span>🌍 {selectedArticle.region}</span>
                        {selectedArticle.tickers && selectedArticle.tickers.length > 0 && (
                          <span>📊 {selectedArticle.tickers.join(', ')}</span>
                        )}
                      </div>
                    </div>

                    {/* Article Content */}
                    <div style={{
                      fontSize: '16px',
                      lineHeight: '1.8',
                      color: '#d0d0d0',
                      maxWidth: '800px',
                      margin: '0 auto'
                    }}>
                      <p style={{ marginBottom: '16px', fontSize: '18px', fontWeight: '500', color: '#e0e0e0' }}>
                        {selectedArticle.summary}
                      </p>

                      <div style={{
                        backgroundColor: 'rgba(255, 165, 0, 0.1)',
                        border: `1px solid ${TERMINAL_ORANGE}`,
                        borderRadius: '4px',
                        padding: '16px',
                        marginTop: '24px'
                      }}>
                        <p style={{ fontSize: '14px', color: TERMINAL_YELLOW, marginBottom: '8px', fontWeight: 'bold' }}>
                          ℹ️ Reader View Limitation
                        </p>
                        <p style={{ fontSize: '13px', lineHeight: '1.6', color: TERMINAL_GRAY, marginBottom: '12px' }}>
                          Most news websites block embedding for security reasons. This reader view shows the article summary from the RSS feed.
                        </p>
                        <p style={{ fontSize: '13px', lineHeight: '1.6', color: TERMINAL_GRAY }}>
                          For the complete article with images, videos, and full content, please click the "🔗 OPEN FULL ARTICLE" button above to view it in your browser.
                        </p>
                      </div>
                    </div>
                  </div>
                </div>
              ) : (
                <>
                  {/* Article Metadata */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gap: '8px',
                    marginBottom: '12px',
                    padding: '8px',
                    backgroundColor: 'rgba(255, 165, 0, 0.05)',
                    border: `1px solid ${TERMINAL_GRAY}`,
                    fontSize: '10px'
                  }}>
                <div>
                  <span style={{ color: TERMINAL_GRAY }}>SOURCE: </span>
                  <span style={{ color: TERMINAL_CYAN, fontWeight: 'bold' }}>{selectedArticle.source}</span>
                </div>
                <div>
                  <span style={{ color: TERMINAL_GRAY }}>TIME: </span>
                  <span style={{ color: TERMINAL_WHITE }}>{selectedArticle.time}</span>
                </div>
                <div>
                  <span style={{ color: TERMINAL_GRAY }}>REGION: </span>
                  <span style={{ color: TERMINAL_PURPLE }}>{selectedArticle.region}</span>
                </div>
                <div>
                  <span style={{ color: TERMINAL_GRAY }}>SENTIMENT: </span>
                  <span style={{ color: getSentimentColor(selectedArticle.sentiment), fontWeight: 'bold' }}>
                    {selectedArticle.sentiment}
                  </span>
                </div>
                <div>
                  <span style={{ color: TERMINAL_GRAY }}>IMPACT: </span>
                  <span style={{ color: getImpactColor(selectedArticle.impact), fontWeight: 'bold' }}>
                    {selectedArticle.impact}
                  </span>
                </div>
                {selectedArticle.tickers && selectedArticle.tickers.length > 0 && (
                  <div>
                    <span style={{ color: TERMINAL_GRAY }}>TICKERS: </span>
                    <span style={{ color: TERMINAL_GREEN, fontWeight: 'bold' }}>
                      {selectedArticle.tickers.join(', ')}
                    </span>
                  </div>
                )}
              </div>

              {/* Article Headline */}
              <div style={{
                color: TERMINAL_WHITE,
                fontSize: '14px',
                fontWeight: 'bold',
                lineHeight: '1.4',
                marginBottom: '12px',
                padding: '8px',
                backgroundColor: 'rgba(255, 255, 255, 0.05)',
                borderLeft: `4px solid ${getPriorityColor(selectedArticle.priority)}`
              }}>
                {selectedArticle.headline}
              </div>

              {/* Article Summary */}
              <div style={{
                color: TERMINAL_GRAY,
                fontSize: '12px',
                lineHeight: '1.6',
                marginBottom: '16px'
              }}>
                {selectedArticle.summary}
              </div>

                  {/* Action Buttons */}
                  {selectedArticle.link && (
                    <div style={{
                      display: 'flex',
                      gap: '8px',
                      paddingTop: '12px',
                      borderTop: `1px solid ${TERMINAL_GRAY}`
                    }}>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setShowWebView(true);
                        }}
                        style={{
                          backgroundColor: TERMINAL_GREEN,
                          color: TERMINAL_DARK_BG,
                          border: 'none',
                          padding: '6px 12px',
                          fontSize: '11px',
                          fontWeight: 'bold',
                          cursor: 'pointer',
                          borderRadius: '2px',
                          flex: 1
                        }}
                      >
                        📖 READ IN APP
                      </button>
                      <button
                        onClick={async (e) => {
                          e.stopPropagation();
                          const link = selectedArticle.link;
                          console.log('📰 Opening link in external browser:', link);

                          if (!link) {
                            console.error('❌ No link available');
                            return;
                          }

                          try {
                            if (window.__TAURI__) {
                              const { openUrl } = await import('@tauri-apps/plugin-opener');
                              await openUrl(link);
                              console.log('✅ Link opened via Tauri opener');
                            } else {
                              const a = document.createElement('a');
                              a.href = link;
                              a.target = '_blank';
                              a.rel = 'noopener noreferrer';
                              document.body.appendChild(a);
                              a.click();
                              document.body.removeChild(a);
                              console.log('✅ Link opened via anchor element');
                            }
                          } catch (error) {
                            console.error('❌ Failed to open link:', error);
                            await navigator.clipboard.writeText(link);
                            console.log('📋 Link copied to clipboard');
                          }
                        }}
                        style={{
                          backgroundColor: TERMINAL_BLUE,
                          color: TERMINAL_WHITE,
                          border: 'none',
                          padding: '6px 12px',
                          fontSize: '11px',
                          fontWeight: 'bold',
                          cursor: 'pointer',
                          borderRadius: '2px',
                          flex: 1
                        }}
                      >
                        🔗 OPEN IN BROWSER
                      </button>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          const link = selectedArticle.link;
                          if (link) {
                            navigator.clipboard.writeText(link);
                            console.log('Link copied to clipboard:', link);
                            e.currentTarget.textContent = '✓ COPIED!';
                            setTimeout(() => {
                              e.currentTarget.textContent = '📋 COPY';
                            }, 2000);
                          }
                        }}
                        style={{
                          backgroundColor: TERMINAL_GRAY,
                          color: TERMINAL_WHITE,
                          border: 'none',
                          padding: '6px 12px',
                          fontSize: '11px',
                          fontWeight: 'bold',
                          cursor: 'pointer',
                          borderRadius: '2px'
                        }}
                        title="Copy link to clipboard"
                      >
                        📋 COPY
                      </button>
                    </div>
                  )}
                </>
              )}
            </div>
          </div>
        </div>
      )}

      {/* CSS for animations and transparent scrollbars */}
      <style>{`
        .ticker-wrap {
          width: 100%;
          overflow: hidden;
          background-color: transparent;
          padding: 0;
        }

        .ticker-move {
          display: inline-block;
          white-space: nowrap;
          animation: tickerScroll ${tickerDuration}s linear infinite;
          padding-right: 100%;
        }

        .ticker-item {
          display: inline-block;
          padding-right: 3em;
          color: ${TERMINAL_WHITE};
          font-size: 11px;
          white-space: nowrap;
        }

        @keyframes tickerScroll {
          0% {
            transform: translateX(0);
          }
          100% {
            transform: translateX(-50%);
          }
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
