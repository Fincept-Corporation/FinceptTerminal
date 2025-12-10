import React, { useState, useEffect } from 'react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { fetchNewsWithCache, type NewsArticle, getRSSFeedCount, getActiveSources, isUsingMockData } from '../../services/newsService';
import { contextRecorderService } from '../../services/contextRecorderService';
import RecordingControlPanel from '../common/RecordingControlPanel';

// Extend Window interface for Tauri
declare global {
  interface Window {
    __TAURI__?: any;
  }
}

const NewsTab: React.FC = () => {
  const { colors, fontSize, fontFamily, fontWeight, fontStyle } = useTerminalTheme();
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeFilter, setActiveFilter] = useState('ALL');
  const [newsUpdateCount, setNewsUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(0);
  const [newsArticles, setNewsArticles] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeSources, setActiveSources] = useState<string[]>([]);
  const [selectedArticle, setSelectedArticle] = useState<NewsArticle | null>(null);
  const [showWebView, setShowWebView] = useState(false);
  const [refreshInterval, setRefreshInterval] = useState(10); // minutes
  const [showIntervalSettings, setShowIntervalSettings] = useState(false);
  const [isRecording, setIsRecording] = useState(false);

  // Function to record current news data
  const recordCurrentData = async () => {
    if (newsArticles.length > 0) {
      try {
        const recordData = {
          articles: newsArticles,
          totalCount: newsArticles.length,
          alertCount: newsArticles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length,
          sources: getActiveSources(),
          timestamp: new Date().toISOString(),
          filter: activeFilter
        };
        await contextRecorderService.recordApiResponse(
          'News',
          'news-feed',
          recordData,
          `News Feed (Snapshot) - ${new Date().toLocaleString()}`,
          ['news', 'rss', 'snapshot', activeFilter.toLowerCase()]
        );
      } catch (error) {
        // Silently handle recording errors
      }
    }
  };

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

      // Record data if recording is active
      if (isRecording && articles.length > 0) {
        try {
          const recordData = {
            articles: articles,
            totalCount: articles.length,
            alertCount: articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length,
            sources: getActiveSources(),
            timestamp: new Date().toISOString(),
            filter: activeFilter
          };
          await contextRecorderService.recordApiResponse(
            'News',
            'news-feed',
            recordData,
            `News Feed - ${new Date().toLocaleString()}`,
            ['news', 'rss', 'live-feed', activeFilter.toLowerCase()]
          );
        } catch (error) {
          // Silently handle recording errors
        }
      }
    } catch (error) {
      // Silently handle fetch errors
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

        // Record data if recording is active
        if (isRecording && articles.length > 0) {
          try {
            const recordData = {
              articles: articles,
              totalCount: articles.length,
              alertCount: articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length,
              sources: getActiveSources(),
              timestamp: new Date().toISOString(),
              filter: activeFilter
            };
            await contextRecorderService.recordApiResponse(
              'News',
              'news-feed',
              recordData,
              `News Feed - ${new Date().toLocaleString()}`,
              ['news', 'rss', 'live-feed', activeFilter.toLowerCase()]
            );
          } catch (error) {
            // Silently handle recording errors
          }
        }
      } catch (error) {
        // Silently handle fetch errors
      } finally {
        setLoading(false);
      }
    };

    loadNews();

    // Refresh news based on user interval
    const newsRefreshInterval = setInterval(loadNews, refreshInterval * 60 * 1000);

    return () => clearInterval(newsRefreshInterval);
  }, [refreshInterval, isRecording, activeFilter]);

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
      case 'FLASH': return colors.alert;
      case 'URGENT': return colors.primary;
      case 'BREAKING': return colors.warning;
      case 'ROUTINE': return colors.secondary;
      default: return colors.textMuted;
    }
  };

  const getSentimentColor = (sentiment: string) => {
    switch (sentiment) {
      case 'BULLISH': return colors.secondary;
      case 'BEARISH': return colors.alert;
      case 'NEUTRAL': return colors.warning;
      default: return colors.textMuted;
    }
  };

  const getImpactColor = (impact: string) => {
    switch (impact) {
      case 'HIGH': return colors.alert;
      case 'MEDIUM': return colors.warning;
      case 'LOW': return colors.secondary;
      default: return colors.textMuted;
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
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      fontWeight: fontWeight,
      fontStyle: fontStyle,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: fontSize.body
    }}>
      {/* Complex Header with News Ticker */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
        padding: '4px 8px',
        flexShrink: 0
      }}>
        {/* Main Status Line */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px' }}>
          <span style={{ color: colors.primary, fontWeight: 'bold' }}>FINCEPT TERMINAL PROFESSIONAL NEWS</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.alert, fontWeight: 'bold', animation: 'blink 1s infinite' }}>LIVE FEED</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.warning }}>ALERTS: {alertCount}</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.secondary }}>● {getRSSFeedCount()} SOURCES</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.text }}>{currentTime.toISOString().replace('T', ' ').substring(0, 19)} UTC</span>
          <span style={{ color: colors.text }}>|</span>
          <button
            onClick={handleRefresh}
            disabled={loading}
            style={{
              backgroundColor: loading ? colors.textMuted : colors.secondary,
              color: colors.background,
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
          <span style={{ color: colors.text }}>|</span>
          <button
            onClick={() => setShowIntervalSettings(!showIntervalSettings)}
            style={{
              backgroundColor: colors.info,
              color: colors.background,
              border: 'none',
              padding: '2px 8px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer',
              borderRadius: '2px'
            }}
            title="Auto-refresh settings"
          >
            ⏱ {refreshInterval}min
          </button>
          <span style={{ color: colors.text }}>|</span>
          <RecordingControlPanel
            tabName="News"
            onRecordingChange={setIsRecording}
            onRecordingStart={recordCurrentData}
          />
        </div>

        {/* Interval Settings Dropdown */}
        {showIntervalSettings && (
          <div style={{
            position: 'absolute',
            top: '30px',
            right: '8px',
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            borderRadius: '4px',
            padding: '8px',
            zIndex: 1000,
            boxShadow: `0 4px 8px rgba(0,0,0,0.3)`
          }}>
            <div style={{ fontSize: '11px', color: colors.warning, fontWeight: 'bold', marginBottom: '6px' }}>
              AUTO-REFRESH INTERVAL
            </div>
            {[1, 2, 5, 10, 15, 30].map(min => (
              <button
                key={min}
                onClick={() => {
                  setRefreshInterval(min);
                  setShowIntervalSettings(false);
                }}
                style={{
                  width: '100%',
                  backgroundColor: refreshInterval === min ? colors.primary : colors.background,
                  color: refreshInterval === min ? colors.background : colors.text,
                  border: `1px solid ${colors.textMuted}`,
                  padding: '4px 8px',
                  fontSize: '10px',
                  fontWeight: refreshInterval === min ? 'bold' : 'normal',
                  cursor: 'pointer',
                  marginBottom: '2px',
                  textAlign: 'left'
                }}
              >
                {min} minute{min > 1 ? 's' : ''}
              </button>
            ))}
          </div>
        )}

        {/* News Ticker - Continuous seamless scroll like Bloomberg */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px', marginBottom: '2px' }}>
          <span style={{ color: colors.warning, fontWeight: 'bold' }}>BREAKING:</span>
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
          <span style={{ color: colors.secondary, fontSize: '9px' }}>
            ● LIVE
          </span>
        </div>

        {/* Sources and Stats */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '10px', flexWrap: 'wrap' }}>
          <span style={{ color: colors.textMuted }}>ACTIVE SOURCES:</span>
          {activeSources.slice(0, 12).map((source, idx) => (
            <span key={idx} style={{ color: colors.secondary }}>{source}●</span>
          ))}
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.textMuted }}>ARTICLES:</span>
          <span style={{ color: colors.accent }}>{newsArticles.length}</span>
          <span style={{ color: colors.text }}>|</span>
          <span style={{ color: colors.textMuted }}>STATUS:</span>
          <span style={{ color: loading ? colors.warning : colors.secondary }}>
            {loading ? 'UPDATING...' : (isUsingMockData() ? '● DEMO MODE' : '● LIVE')}
          </span>
        </div>
      </div>

      {/* Function Keys Bar */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `1px solid ${colors.textMuted}`,
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
                backgroundColor: activeFilter === item.filter ? colors.primary : colors.background,
                border: `1px solid ${colors.textMuted}`,
                color: activeFilter === item.filter ? colors.background : colors.text,
                padding: '2px 4px',
                fontSize: '9px',
                height: '16px',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                fontWeight: activeFilter === item.filter ? 'bold' : 'normal',
                cursor: 'pointer'
              }}>
              <span style={{ color: colors.warning }}>{item.key}:</span>
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
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              PRIMARY NEWS FEED [{activeFilter}]
            </div>
            <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

            {loading && newsArticles.length === 0 ? (
              <div style={{ color: colors.warning, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
                Loading news from {getRSSFeedCount()} RSS feeds...
              </div>
            ) : filteredNews.length === 0 ? (
              <div style={{ color: colors.textMuted, fontSize: '10px', textAlign: 'center', padding: '20px' }}>
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
                  <span style={{ color: colors.textMuted, minWidth: '50px' }}>{article.time}</span>
                  <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{article.priority}]
                  </span>
                  <span style={{ color: colors.info, minWidth: '50px' }}>[{article.category}]</span>
                  <span style={{ color: colors.purple, minWidth: '30px' }}>{article.region}</span>
                  <span style={{ color: getSentimentColor(article.sentiment), minWidth: '50px' }}>{article.sentiment}</span>
                </div>

                <div style={{ color: colors.text, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.headline}
                </div>

                <div style={{ color: colors.textMuted, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.summary}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                  <span style={{ color: colors.accent }}>{article.source}</span>
                  <span style={{ color: getImpactColor(article.impact) }}>IMPACT: {article.impact}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Center Panel - Secondary News Feed */}
          <div style={{
            flex: 1,
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
              SECONDARY NEWS FEED [{activeFilter}]
            </div>
            <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

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
                  <span style={{ color: colors.textMuted, minWidth: '50px' }}>{article.time}</span>
                  <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                    [{article.priority}]
                  </span>
                  <span style={{ color: colors.info, minWidth: '50px' }}>[{article.category}]</span>
                  <span style={{ color: colors.purple, minWidth: '30px' }}>{article.region}</span>
                  <span style={{ color: getSentimentColor(article.sentiment), minWidth: '50px' }}>{article.sentiment}</span>
                </div>

                <div style={{ color: colors.text, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.headline}
                </div>

                <div style={{ color: colors.textMuted, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                  {article.summary}
                </div>

                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                  <span style={{ color: colors.accent }}>{article.source}</span>
                  <span style={{ color: getImpactColor(article.impact) }}>IMPACT: {article.impact}</span>
                </div>
              </div>
            ))}
          </div>

          {/* Right Panel - Tertiary Feed + Analytics */}
          <div style={{
            width: '400px',
            backgroundColor: colors.panel,
            border: `1px solid ${colors.textMuted}`,
            padding: '4px',
            overflow: 'auto'
          }}>
            {/* News Analytics Section */}
            <div style={{ marginBottom: '12px' }}>
              <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                REAL-TIME NEWS ANALYTICS
              </div>
              <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '6px' }}></div>

              {/* Sentiment Analysis */}
              <div style={{ marginBottom: '8px', fontSize: '9px' }}>
                <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '3px' }}>
                  SENTIMENT ANALYSIS
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Bullish Articles:</span>
                  <span style={{ color: colors.secondary }}>{bullishCount} ({Math.round((bullishCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Bearish Articles:</span>
                  <span style={{ color: colors.alert }}>{bearishCount} ({Math.round((bearishCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Neutral Articles:</span>
                  <span style={{ color: colors.warning }}>{neutralCount} ({Math.round((neutralCount/totalCount)*100)}%)</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '2px', paddingTop: '2px', borderTop: `1px solid ${colors.textMuted}` }}>
                  <span style={{ color: colors.accent, fontWeight: 'bold' }}>Sentiment Score:</span>
                  <span style={{ color: parseFloat(sentimentScore) > 0 ? colors.secondary : parseFloat(sentimentScore) < 0 ? colors.alert : colors.warning, fontWeight: 'bold' }}>
                    {sentimentScore} ({parseFloat(sentimentScore) > 0 ? 'BULLISH' : parseFloat(sentimentScore) < 0 ? 'BEARISH' : 'NEUTRAL'})
                  </span>
                </div>
              </div>

              {/* News Flow Statistics */}
              <div style={{ marginBottom: '8px', fontSize: '9px' }}>
                <div style={{ color: colors.warning, fontWeight: 'bold', marginBottom: '3px' }}>
                  NEWS FLOW STATS
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Total Articles:</span>
                  <span style={{ color: colors.text }}>{newsArticles.length}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Breaking News:</span>
                  <span style={{ color: colors.alert }}>{breakingCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Earnings Reports:</span>
                  <span style={{ color: colors.info }}>{earningsCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>Economic Data:</span>
                  <span style={{ color: colors.purple }}>{economicCount}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
                  <span style={{ color: colors.textMuted }}>High Priority:</span>
                  <span style={{ color: colors.primary }}>{alertCount}</span>
                </div>
              </div>
            </div>

            {/* Tertiary News Feed */}
            <div style={{ borderTop: `2px solid ${colors.textMuted}`, paddingTop: '4px' }}>
              <div style={{ color: colors.primary, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                ADDITIONAL HEADLINES
              </div>
              <div style={{ borderBottom: `1px solid ${colors.textMuted}`, marginBottom: '4px' }}></div>

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
                    <span style={{ color: colors.textMuted, minWidth: '50px' }}>{article.time}</span>
                    <span style={{ color: getPriorityColor(article.priority), fontWeight: 'bold', minWidth: '60px' }}>
                      [{article.priority}]
                    </span>
                    <span style={{ color: colors.info, minWidth: '50px' }}>[{article.category}]</span>
                  </div>

                  <div style={{ color: colors.text, fontWeight: 'bold', fontSize: '10px', lineHeight: '1.2', marginBottom: '2px' }}>
                    {article.headline}
                  </div>

                  <div style={{ color: colors.textMuted, fontSize: '9px', lineHeight: '1.2', marginBottom: '2px' }}>
                    {article.summary.slice(0, 150)}...
                  </div>

                  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
                    <span style={{ color: colors.accent }}>{article.source}</span>
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
        borderTop: `1px solid ${colors.textMuted}`,
        backgroundColor: colors.panel,
        padding: '2px 8px',
        fontSize: '10px',
        color: colors.textMuted,
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
            <span style={{ color: loading ? colors.warning : colors.secondary }}>
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
            backgroundColor: colors.panel,
            border: `2px solid ${colors.primary}`,
            borderRadius: '4px',
            width: showWebView ? '90vw' : '600px',
            maxWidth: '95vw',
            height: showWebView ? '90vh' : 'auto',
            maxHeight: '90vh',
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: `0 0 20px ${colors.primary}`,
            transition: 'all 0.3s ease'
          }}
          onClick={(e) => e.stopPropagation()}
          >
            {/* Modal Header */}
            <div style={{
              backgroundColor: colors.background,
              borderBottom: `2px solid ${colors.primary}`,
              padding: '8px 12px',
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              flexShrink: 0
            }}>
              <div style={{ display: 'flex', gap: '8px', alignItems: 'center', fontSize: '11px' }}>
                <span style={{ color: colors.primary, fontWeight: 'bold' }}>ARTICLE DETAILS</span>
                <span style={{ color: colors.text }}>|</span>
                <span style={{ color: getPriorityColor(selectedArticle.priority), fontWeight: 'bold' }}>
                  [{selectedArticle.priority}]
                </span>
                <span style={{ color: colors.info }}>[{selectedArticle.category}]</span>
              </div>
              <button
                onClick={() => {
                  setSelectedArticle(null);
                  setShowWebView(false);
                }}
                style={{
                  backgroundColor: colors.alert,
                  color: colors.text,
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
                    backgroundColor: colors.background,
                    padding: '8px',
                    borderBottom: `1px solid ${colors.textMuted}`,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    fontSize: '10px'
                  }}>
                    <button
                      onClick={() => setShowWebView(false)}
                      style={{
                        backgroundColor: colors.primary,
                        color: colors.background,
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
                    <span style={{ color: colors.textMuted }}>|</span>
                    <span style={{ color: colors.warning, fontWeight: 'bold' }}>READER VIEW</span>
                    <span style={{ color: colors.textMuted }}>|</span>
                    <span style={{ color: colors.text, flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
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
                        backgroundColor: colors.info,
                        color: colors.text,
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
                      borderBottom: `2px solid ${colors.primary}`
                    }}>
                      <div style={{
                        display: 'flex',
                        gap: '8px',
                        marginBottom: '12px',
                        fontSize: '11px'
                      }}>
                        <span style={{
                          backgroundColor: getPriorityColor(selectedArticle.priority),
                          color: colors.background,
                          padding: '2px 8px',
                          borderRadius: '2px',
                          fontWeight: 'bold'
                        }}>
                          {selectedArticle.priority}
                        </span>
                        <span style={{
                          backgroundColor: colors.info,
                          color: colors.text,
                          padding: '2px 8px',
                          borderRadius: '2px',
                          fontWeight: 'bold'
                        }}>
                          {selectedArticle.category}
                        </span>
                        <span style={{
                          backgroundColor: getSentimentColor(selectedArticle.sentiment),
                          color: colors.background,
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
                        color: colors.text
                      }}>
                        {selectedArticle.headline}
                      </h1>

                      <div style={{
                        display: 'flex',
                        gap: '16px',
                        fontSize: '12px',
                        color: colors.textMuted
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
                        border: `1px solid ${colors.primary}`,
                        borderRadius: '4px',
                        padding: '16px',
                        marginTop: '24px'
                      }}>
                        <p style={{ fontSize: '14px', color: colors.warning, marginBottom: '8px', fontWeight: 'bold' }}>
                          ℹ️ Reader View Limitation
                        </p>
                        <p style={{ fontSize: '13px', lineHeight: '1.6', color: colors.textMuted, marginBottom: '12px' }}>
                          Most news websites block embedding for security reasons. This reader view shows the article summary from the RSS feed.
                        </p>
                        <p style={{ fontSize: '13px', lineHeight: '1.6', color: colors.textMuted }}>
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
                    border: `1px solid ${colors.textMuted}`,
                    fontSize: '10px'
                  }}>
                <div>
                  <span style={{ color: colors.textMuted }}>SOURCE: </span>
                  <span style={{ color: colors.accent, fontWeight: 'bold' }}>{selectedArticle.source}</span>
                </div>
                <div>
                  <span style={{ color: colors.textMuted }}>TIME: </span>
                  <span style={{ color: colors.text }}>{selectedArticle.time}</span>
                </div>
                <div>
                  <span style={{ color: colors.textMuted }}>REGION: </span>
                  <span style={{ color: colors.purple }}>{selectedArticle.region}</span>
                </div>
                <div>
                  <span style={{ color: colors.textMuted }}>SENTIMENT: </span>
                  <span style={{ color: getSentimentColor(selectedArticle.sentiment), fontWeight: 'bold' }}>
                    {selectedArticle.sentiment}
                  </span>
                </div>
                <div>
                  <span style={{ color: colors.textMuted }}>IMPACT: </span>
                  <span style={{ color: getImpactColor(selectedArticle.impact), fontWeight: 'bold' }}>
                    {selectedArticle.impact}
                  </span>
                </div>
                {selectedArticle.tickers && selectedArticle.tickers.length > 0 && (
                  <div>
                    <span style={{ color: colors.textMuted }}>TICKERS: </span>
                    <span style={{ color: colors.secondary, fontWeight: 'bold' }}>
                      {selectedArticle.tickers.join(', ')}
                    </span>
                  </div>
                )}
              </div>

              {/* Article Headline */}
              <div style={{
                color: colors.text,
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
                color: colors.textMuted,
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
                      borderTop: `1px solid ${colors.textMuted}`
                    }}>
                      <button
                        onClick={(e) => {
                          e.stopPropagation();
                          setShowWebView(true);
                        }}
                        style={{
                          backgroundColor: colors.secondary,
                          color: colors.background,
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

                          if (!link) {
                            return;
                          }

                          try {
                            if (window.__TAURI__) {
                              const { openUrl } = await import('@tauri-apps/plugin-opener');
                              await openUrl(link);
                            } else {
                              const a = document.createElement('a');
                              a.href = link;
                              a.target = '_blank';
                              a.rel = 'noopener noreferrer';
                              document.body.appendChild(a);
                              a.click();
                              document.body.removeChild(a);
                            }
                          } catch (error) {
                            await navigator.clipboard.writeText(link);
                          }
                        }}
                        style={{
                          backgroundColor: colors.info,
                          color: colors.text,
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
                            e.currentTarget.textContent = '✓ COPIED!';
                            setTimeout(() => {
                              e.currentTarget.textContent = '📋 COPY';
                            }, 2000);
                          }
                        }}
                        style={{
                          backgroundColor: colors.textMuted,
                          color: colors.text,
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
          color: ${colors.text};
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
