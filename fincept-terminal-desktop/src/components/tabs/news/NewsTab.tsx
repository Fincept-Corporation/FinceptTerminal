import React, { useState, useEffect, useCallback, useRef } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import { Info, Settings, RefreshCw, Clock, Zap, Newspaper, ExternalLink, Copy, X, TrendingUp, TrendingDown, Minus, AlertTriangle, Search } from 'lucide-react';
import { fetchAllNews, type NewsArticle, getRSSFeedCount, getActiveSources, isUsingMockData, setNewsCacheTTL } from '@/services/news/newsService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { TimezoneSelector } from '@/components/common/TimezoneSelector';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { analyzeNewsArticle, type NewsAnalysisData, getSentimentColor, getUrgencyColor, getRiskColor } from '@/services/news/newsAnalysisService';
import { createNewsTabTour } from '@/components/tabs/tours/newsTabTour';
import RSSFeedSettingsModal from './RSSFeedSettingsModal';

declare global {
  interface Window { __TAURI__?: any; }
}

// Theme colors provided by useTerminalTheme() hook
const SentIcon: React.FC<{ s: string; size?: number }> = ({ s, size = 10 }) =>
  s === 'BULLISH' ? <TrendingUp size={size} /> : s === 'BEARISH' ? <TrendingDown size={size} /> : <Minus size={size} />;

// ══════════════════════════════════════════════════════════════
// NEWS GRID CARD – compact, shows key info + inline actions
// ══════════════════════════════════════════════════════════════
const NewsGridCard: React.FC<{
  article: NewsArticle;
  onAnalyze: (article: NewsArticle) => void;
  analyzing: boolean;
  currentAnalyzingId: string | null;
  openExternal: (url: string) => void;
  colors: any;
  priColor: (p: string) => string;
  sentColor: (s: string) => string;
  impColor: (i: string) => string;
  fontFamily: string;
}> = ({ article, onAnalyze, analyzing, currentAnalyzingId, openExternal, colors, priColor, sentColor, impColor, fontFamily }) => {
  const [hovered, setHovered] = useState(false);
  const [expanded, setExpanded] = useState(false);
  const isHighPri = article.priority === 'FLASH' || article.priority === 'URGENT';
  const isAnalyzing = analyzing && currentAnalyzingId === article.id;

  return (
    <div
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        padding: '10px 12px',
        backgroundColor: hovered ? '#1F1F1F' : colors.panel,
        border: `1px solid ${isHighPri ? `${priColor(article.priority)}40` : colors.panel}`,
        borderLeft: `3px solid ${priColor(article.priority)}`,
        borderRadius: '2px',
        transition: 'background 0.15s',
        display: 'flex',
        flexDirection: 'column',
        minHeight: '100px',
      }}
    >
      {/* Top: Source + Time */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: 'var(--ft-color-accent, #00E5FF)', letterSpacing: '0.3px' }}>
          {article.source}
        </span>
        {article.time && article.time.trim() !== '' && (
          <span style={{ fontSize: '9px', color: colors.textMuted }}>{article.time}</span>
        )}
      </div>

      {/* Headline */}
      <div
        onClick={() => setExpanded(!expanded)}
        style={{
          color: colors.text,
          fontSize: '11px',
          fontWeight: 600,
          lineHeight: '1.4',
          overflow: 'hidden',
          textOverflow: 'ellipsis',
          display: '-webkit-box',
          WebkitLineClamp: expanded ? 'unset' : 3,
          WebkitBoxOrient: 'vertical' as any,
          marginBottom: '8px',
          cursor: 'pointer',
        }}
      >{article.headline}</div>

      {/* Summary (when expanded) */}
      {expanded && (
        <div style={{
          color: colors.textMuted,
          fontSize: '10px',
          lineHeight: '1.5',
          marginBottom: '8px',
          paddingLeft: '8px',
          borderLeft: `2px solid ${colors.panel}`,
        }}>{article.summary}</div>
      )}

      {/* Tickers (when expanded) */}
      {expanded && article.tickers && article.tickers.length > 0 && (
        <div style={{ display: 'flex', gap: '4px', marginBottom: '8px', flexWrap: 'wrap' }}>
          {article.tickers.map((ticker, i) => (
            <span key={i} style={{
              padding: '1px 6px', backgroundColor: `${colors.success}15`, color: colors.success,
              fontSize: '8px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${colors.success}30`,
            }}>{ticker}</span>
          ))}
        </div>
      )}

      {/* Action Buttons (visible on hover or when expanded) */}
      {(hovered || expanded) && article.link && (
        <div style={{
          display: 'flex',
          gap: '4px',
          marginBottom: '8px',
          opacity: hovered || expanded ? 1 : 0,
          transition: 'opacity 0.2s',
        }}>
          <button
            onClick={(e) => { e.stopPropagation(); openExternal(article.link!); }}
            style={{
              flex: 1, padding: '5px 8px', backgroundColor: 'var(--ft-color-blue, #0088FF)', color: colors.text,
              border: 'none', fontSize: '8px', fontWeight: 700, cursor: 'pointer',
              borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          ><ExternalLink size={9} />OPEN</button>
          <button
            onClick={(e) => { e.stopPropagation(); navigator.clipboard.writeText(article.link!); }}
            style={{
              padding: '5px 8px', background: 'none', border: `1px solid ${colors.panel}`,
              color: colors.textMuted, fontSize: '8px', fontWeight: 700, cursor: 'pointer',
              borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px',
            }}
          ><Copy size={9} /></button>
          <button
            onClick={(e) => { e.stopPropagation(); onAnalyze(article); }}
            disabled={isAnalyzing}
            style={{
              flex: 1, padding: '5px 8px',
              backgroundColor: isAnalyzing ? colors.textMuted : 'var(--ft-color-purple, #9D4EDD)',
              color: colors.text, border: 'none', fontSize: '8px', fontWeight: 700,
              cursor: isAnalyzing ? 'not-allowed' : 'pointer',
              borderRadius: '2px', opacity: isAnalyzing ? 0.5 : 1,
              display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}
          ><Zap size={9} />{isAnalyzing ? 'ANALYZING...' : 'ANALYZE'}</button>
        </div>
      )}


      {/* Bottom: badges row */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap', marginTop: '8px' }}>
        {isHighPri && (
          <span style={{
            padding: '1px 6px', fontSize: '8px', fontWeight: 700, borderRadius: '2px',
            backgroundColor: `${priColor(article.priority)}20`, color: priColor(article.priority),
            border: `1px solid ${priColor(article.priority)}40`,
          }}>{article.priority}</span>
        )}
        <span style={{
          padding: '1px 6px', fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          backgroundColor: `${sentColor(article.sentiment)}15`, color: sentColor(article.sentiment),
          display: 'flex', alignItems: 'center', gap: '3px',
        }}>
          <SentIcon s={article.sentiment} size={8} />{article.sentiment}
        </span>
        <span style={{
          padding: '1px 6px', fontSize: '8px', fontWeight: 700, borderRadius: '2px',
          color: colors.textMuted, backgroundColor: `${colors.textMuted}20`,
        }}>{article.category}</span>
        <span style={{
          marginLeft: 'auto', fontSize: '8px', fontWeight: 700,
          color: impColor(article.impact),
        }}>{article.impact}</span>
      </div>
    </div>
  );
};

// ══════════════════════════════════════════════════════════════
// MAIN COMPONENT
// ══════════════════════════════════════════════════════════════
const NewsTab: React.FC = () => {
  const { t } = useTranslation('news');
  const { colors, fontSize, fontFamily } = useTerminalTheme();

  // Helper functions for priority/sentiment colors
  const priColor = (p: string) => ({
    FLASH: colors.alert,
    URGENT: colors.primary,
    BREAKING: colors.warning,
    ROUTINE: colors.textMuted
  }[p] || colors.textMuted);

  const sentColor = (s: string) => ({
    BULLISH: colors.success,
    BEARISH: colors.alert,
    NEUTRAL: colors.warning
  }[s] || colors.textMuted);

  const impColor = (i: string) => ({
    HIGH: colors.alert,
    MEDIUM: colors.warning,
    LOW: colors.success
  }[i] || colors.textMuted);

  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeFilter, setActiveFilter] = useState('ALL');
  const [newsUpdateCount, setNewsUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(0);
  const [newsArticles, setNewsArticles] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeSources, setActiveSources] = useState<string[]>([]);
  const [feedCount, setFeedCount] = useState(9);
  const [refreshInterval, setRefreshInterval] = useState(10);
  const [showIntervalSettings, setShowIntervalSettings] = useState(false);
  const [isRecording, setIsRecording] = useState(false);
  const [analysisData, setAnalysisData] = useState<NewsAnalysisData | null>(null);
  const [analyzingArticle, setAnalyzingArticle] = useState(false);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const [showFeedSettings, setShowFeedSettings] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');

  // Workspace tab state
  const getWorkspaceState = useCallback(() => ({
    activeFilter, feedCount, refreshInterval, searchQuery,
  }), [activeFilter, feedCount, refreshInterval, searchQuery]);

  const setWorkspaceState = useCallback((state: Record<string, unknown>) => {
    if (typeof state.activeFilter === 'string') setActiveFilter(state.activeFilter);
    if (typeof state.feedCount === 'number') setFeedCount(state.feedCount);
    if (typeof state.refreshInterval === 'number') setRefreshInterval(state.refreshInterval);
    if (typeof state.searchQuery === 'string') setSearchQuery(state.searchQuery);
  }, []);

  useWorkspaceTabState('news', getWorkspaceState, setWorkspaceState);
  const [currentAnalyzingId, setCurrentAnalyzingId] = useState<string | null>(null);
  const [analysisPanelOpen, setAnalysisPanelOpen] = useState(false);
  const [currentAnalyzedArticle, setCurrentAnalyzedArticle] = useState<NewsArticle | null>(null);

  // ── Data loading ──
  const recordCurrentData = async () => {
    if (newsArticles.length > 0) {
      try {
        const sources = await getActiveSources();
        await contextRecorderService.recordApiResponse('News', 'news-feed', {
          articles: newsArticles, totalCount: newsArticles.length,
          alertCount: newsArticles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length,
          sources, timestamp: new Date().toISOString(), filter: activeFilter,
        }, `News Feed (Snapshot) - ${new Date().toLocaleString()}`, ['news', 'rss', 'snapshot', activeFilter.toLowerCase()]);
      } catch { /* silent */ }
    }
  };

  const handleRefresh = useCallback(async (forceRefresh = true) => {
    setLoading(true);
    try {
      const [articles, sources, count] = await Promise.all([
        fetchAllNews(forceRefresh), getActiveSources(), getRSSFeedCount(),
      ]);
      setNewsArticles(articles);
      setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
      setActiveSources(sources);
      setFeedCount(count);
      setNewsUpdateCount(prev => prev + 1);
      if (isRecording && articles.length > 0) {
        try {
          await contextRecorderService.recordApiResponse('News', 'news-feed', {
            articles, totalCount: articles.length,
            alertCount: articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length,
            sources, timestamp: new Date().toISOString(), filter: activeFilter,
          }, `News Feed - ${new Date().toLocaleString()}`, ['news', 'rss', 'live-feed', activeFilter.toLowerCase()]);
        } catch { /* silent */ }
      }
    } catch { /* silent */ } finally { setLoading(false); }
  }, [isRecording, activeFilter]);

  const handleAnalyzeArticle = async (article: NewsArticle) => {
    if (!article?.link) return;
    setAnalyzingArticle(true);
    setShowAnalysis(false);
    setAnalysisData(null);
    setCurrentAnalyzingId(article.id);
    setCurrentAnalyzedArticle(article);
    setAnalysisPanelOpen(true); // Auto-open the panel
    try {
      const result = await analyzeNewsArticle(article.link);
      if (result.success && 'data' in result) {
        setAnalysisData(result.data);
        setShowAnalysis(true);
      }
    } catch { /* silent */ } finally {
      setAnalyzingArticle(false);
    }
  };

  const isInitialMount = useRef(true);
  useEffect(() => { setNewsCacheTTL(refreshInterval); }, [refreshInterval]);

  useEffect(() => {
    const loadNews = async (force = false) => {
      setLoading(true);
      try {
        const [articles, sources, count] = await Promise.all([fetchAllNews(force), getActiveSources(), getRSSFeedCount()]);
        setNewsArticles(articles);
        setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
        setActiveSources(sources);
        setFeedCount(count);
        setNewsUpdateCount(prev => prev + 1);
      } catch { /* silent */ } finally { setLoading(false); }
    };
    if (isInitialMount.current) { isInitialMount.current = false; loadNews(false); } else { loadNews(true); }
    const iv = setInterval(() => loadNews(true), refreshInterval * 60 * 1000);
    return () => clearInterval(iv);
  }, [refreshInterval, isRecording, activeFilter]);

  useEffect(() => {
    const timer = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(timer);
  }, []);

  // ── Derived data ──
  // Interleave articles from different sources so no single publication dominates
  const interleaveBySource = (articles: NewsArticle[]): NewsArticle[] => {
    if (articles.length === 0) return articles;
    const bySource: Record<string, NewsArticle[]> = {};
    articles.forEach(a => {
      const src = a.source || 'Unknown';
      if (!bySource[src]) bySource[src] = [];
      bySource[src].push(a);
    });
    const sources = Object.keys(bySource);
    const result: NewsArticle[] = [];
    let remaining = articles.length;
    while (remaining > 0) {
      for (const src of sources) {
        if (bySource[src].length > 0) {
          result.push(bySource[src].shift()!);
          remaining--;
        }
      }
    }
    return result;
  };

  const filteredNews = (() => {
    let articles = activeFilter === 'ALL'
      ? newsArticles
      : newsArticles.filter(a => a.category === activeFilter);
    // Apply search filter
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      articles = articles.filter(a =>
        a.headline.toLowerCase().includes(q) ||
        a.summary.toLowerCase().includes(q) ||
        a.source.toLowerCase().includes(q) ||
        (a.tickers && a.tickers.some(t => t.toLowerCase().includes(q)))
      );
    }
    // Interleave sources for even distribution
    return interleaveBySource(articles);
  })();

  const bullish = newsArticles.filter(a => a.sentiment === 'BULLISH').length;
  const bearish = newsArticles.filter(a => a.sentiment === 'BEARISH').length;
  const neutral = newsArticles.filter(a => a.sentiment === 'NEUTRAL').length;
  const total = newsArticles.length || 1;
  const score = ((bullish - bearish) / total).toFixed(2);

  const filters = [
    { key: 'F1', label: 'ALL', f: 'ALL' },
    { key: 'F2', label: 'MARKETS', f: 'MARKETS' },
    { key: 'F3', label: 'EARNINGS', f: 'EARNINGS' },
    { key: 'F4', label: 'ECONOMIC', f: 'ECONOMIC' },
    { key: 'F5', label: 'TECH', f: 'TECH' },
    { key: 'F6', label: 'ENERGY', f: 'ENERGY' },
    { key: 'F7', label: 'BANKING', f: 'BANKING' },
    { key: 'F8', label: 'CRYPTO', f: 'CRYPTO' },
    { key: 'F9', label: 'GEO', f: 'GEOPOLITICS' },
  ];

  const openExternal = async (url: string) => {
    try {
      if (window.__TAURI__) {
        const { openUrl } = await import('@tauri-apps/plugin-opener');
        await openUrl(url);
      } else {
        window.open(url, '_blank', 'noopener');
      }
    } catch { await navigator.clipboard.writeText(url); }
  };

  // Category distribution for sidebar
  const catCounts: Record<string, number> = {};
  newsArticles.forEach(a => { catCounts[a.category] = (catCounts[a.category] || 0) + 1; });
  const topCategories = Object.entries(catCounts).sort((a, b) => b[1] - a[1]).slice(0, 6);

  // ══════════════════════════════════════════════════════════════
  // RENDER
  // ══════════════════════════════════════════════════════════════
  const scrollbarCSS = [
    '@keyframes ntBlink { 0%, 50% { opacity: 1; } 51%, 100% { opacity: 0.3; } }',
    '.nt-grid::-webkit-scrollbar { width: 6px; height: 6px; }',
    `.nt-grid::-webkit-scrollbar-track { background: ${colors.background}; }`,
    `.nt-grid::-webkit-scrollbar-thumb { background: ${colors.panel}; border-radius: 3px; }`,
    `.nt-grid::-webkit-scrollbar-thumb:hover { background: ${colors.textMuted}; }`,
    '*::-webkit-scrollbar { width: 6px; height: 6px; }',
    `*::-webkit-scrollbar-track { background: ${colors.background}; }`,
    `*::-webkit-scrollbar-thumb { background: ${colors.panel}; border-radius: 3px; }`,
    `*::-webkit-scrollbar-thumb:hover { background: ${colors.textMuted}; }`,
  ].join('\n');

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      color: colors.text,
      fontFamily: fontFamily,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
    }}>

      {/* ── TOP BAR: Title + Filters + Actions (single merged bar) ── */}
      <div style={{
        backgroundColor: colors.panel,
        borderBottom: `2px solid ${colors.primary}`,
        padding: '0 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${colors.primary}20`,
        flexShrink: 0,
        height: '38px',
        position: 'relative',
      }}>
        {/* Left: branding + live indicator */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Newspaper size={13} style={{ color: colors.primary }} />
          <span style={{ color: colors.primary, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
            {t('title')}
          </span>
          <span style={{ color: colors.panel }}>|</span>
          <span style={{ color: colors.alert, fontWeight: 700, fontSize: '9px', animation: 'ntBlink 1s infinite' }}>LIVE</span>
          <span style={{ color: colors.panel }}>|</span>
          <span style={{ color: colors.warning, fontSize: '9px', fontWeight: 700 }}>
            <AlertTriangle size={9} style={{ display: 'inline', verticalAlign: 'middle', marginRight: '3px' }} />
            {alertCount}
          </span>
          <span style={{ color: colors.panel }}>|</span>
          <span style={{ color: colors.success, fontSize: '9px', fontWeight: 700 }}>{feedCount} SRC</span>
          <span style={{ color: colors.panel }}>|</span>
          <TimezoneSelector compact />
        </div>

        {/* Right: actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button onClick={() => createNewsTabTour().drive()} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${colors.panel}`,
            color: colors.textMuted, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
          }}><Info size={10} /></button>
          <button id="news-refresh" onClick={() => handleRefresh(true)} disabled={loading} style={{
            padding: '4px 10px', backgroundColor: loading ? colors.textMuted : colors.primary,
            color: loading ? colors.textMuted : colors.background, border: 'none', fontSize: '9px', fontWeight: 700,
            cursor: loading ? 'not-allowed' : 'pointer', borderRadius: '2px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}><RefreshCw size={10} />{loading ? 'LOADING' : 'REFRESH'}</button>
          <button id="news-interval-settings" onClick={() => setShowIntervalSettings(!showIntervalSettings)} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${colors.panel}`,
            color: colors.textMuted, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}><Clock size={10} />{refreshInterval}M</button>
          <button onClick={() => setShowFeedSettings(true)} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${colors.panel}`,
            color: colors.textMuted, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
          }}><Settings size={10} /></button>
          <RecordingControlPanel tabName="News" onRecordingChange={setIsRecording} onRecordingStart={recordCurrentData} />
        </div>

        {/* Interval dropdown */}
        {showIntervalSettings && (
          <div style={{
            position: 'absolute', top: '100%', right: 16,
            backgroundColor: colors.panel, border: `1px solid ${colors.primary}`,
            borderRadius: '2px', padding: '6px', zIndex: 1000, marginTop: '2px',
          }}>
            <div style={{ fontSize: '9px', color: colors.textMuted, fontWeight: 700, marginBottom: '4px', letterSpacing: '0.5px' }}>
              AUTO-REFRESH
            </div>
            {[1, 2, 5, 10, 15, 30].map(min => (
              <button key={min}
                onClick={() => { setRefreshInterval(min); setShowIntervalSettings(false); }}
                style={{
                  width: '100%', backgroundColor: refreshInterval === min ? colors.primary : 'transparent',
                  color: refreshInterval === min ? colors.background : colors.text,
                  border: `1px solid ${refreshInterval === min ? colors.primary : colors.panel}`,
                  padding: '3px 8px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
                  marginBottom: '2px', textAlign: 'left', borderRadius: '2px',
                }}
              >{min} MIN</button>
            ))}
          </div>
        )}
      </div>

      {/* ── FILTER BAR ── */}
      <div id="news-filters" style={{
        backgroundColor: colors.panel, borderBottom: `1px solid ${colors.panel}`,
        padding: '3px 10px', flexShrink: 0, display: 'flex', alignItems: 'center', gap: '2px',
      }}>
        {filters.map(item => (
          <button key={item.key}
            onClick={() => setActiveFilter(item.f)}
            style={{
              padding: '3px 10px',
              backgroundColor: activeFilter === item.f ? colors.primary : 'transparent',
              color: activeFilter === item.f ? colors.background : colors.textMuted,
              border: 'none', fontSize: '9px', fontWeight: 700,
              letterSpacing: '0.3px', cursor: 'pointer', borderRadius: '2px',
            }}>
            <span style={{ color: activeFilter === item.f ? colors.background : colors.textMuted, marginRight: '3px', fontSize: '8px' }}>{item.key}</span>
            {item.label}
          </button>
        ))}
        {/* Search Bar */}
        <div style={{
          marginLeft: '12px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          backgroundColor: colors.background,
          border: `1px solid ${colors.panel}`,
          borderRadius: '2px',
          padding: '2px 8px',
          flex: '0 1 220px',
          minWidth: '120px',
        }}>
          <Search size={10} style={{ color: colors.textMuted, flexShrink: 0 }} />
          <input
            type="text"
            value={searchQuery}
            onChange={e => setSearchQuery(e.target.value)}
            placeholder="Search headlines, sources, tickers..."
            style={{
              flex: 1,
              backgroundColor: 'transparent',
              border: 'none',
              outline: 'none',
              color: colors.text,
              fontSize: '9px',
              fontFamily: fontFamily,
              padding: '2px 0',
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                background: 'none', border: 'none', color: colors.textMuted,
                cursor: 'pointer', padding: '0', display: 'flex', alignItems: 'center',
              }}
            >
              <X size={9} />
            </button>
          )}
        </div>
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '12px', fontSize: '9px' }}>
          <span style={{ color: colors.textMuted }}>TOTAL: <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 700 }}>{newsArticles.length}</span></span>
          <span style={{ color: colors.textMuted }}>SHOWING: <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 700 }}>{filteredNews.length}</span></span>
          <span style={{ color: loading ? colors.warning : colors.success, fontWeight: 700 }}>
            {loading ? 'UPDATING' : (isUsingMockData() ? 'DEMO' : 'LIVE')}
          </span>
        </div>
      </div>

      {/* ══ MAIN CONTENT: Left Sidebar + Grid + Right Analysis Panel ══ */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex' }}>

        {/* ── LEFT SIDEBAR: Sentiment + Stats (fixed, no scroll needed) ── */}
        <div style={{
          width: '200px', flexShrink: 0, backgroundColor: colors.panel,
          borderRight: `1px solid ${colors.panel}`,
          display: 'flex', flexDirection: 'column',
          padding: '12px',
          gap: '12px',
          overflow: 'hidden',
        }}>
          {/* Sentiment */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: colors.textMuted, letterSpacing: '0.5px', marginBottom: '8px' }}>
              MARKET SENTIMENT
            </div>
            <div style={{ backgroundColor: colors.background, border: `1px solid ${colors.panel}`, borderRadius: '2px', padding: '10px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '5px', fontSize: '10px' }}>
                <span style={{ color: colors.success }}>BULL</span>
                <span style={{ color: colors.success, fontWeight: 700 }}>{bullish} ({Math.round((bullish / total) * 100)}%)</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '5px', fontSize: '10px' }}>
                <span style={{ color: colors.alert }}>BEAR</span>
                <span style={{ color: colors.alert, fontWeight: 700 }}>{bearish} ({Math.round((bearish / total) * 100)}%)</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', fontSize: '10px' }}>
                <span style={{ color: colors.warning }}>NEUT</span>
                <span style={{ color: colors.warning, fontWeight: 700 }}>{neutral} ({Math.round((neutral / total) * 100)}%)</span>
              </div>
              {/* Bar */}
              <div style={{ height: '4px', backgroundColor: colors.panel, borderRadius: '2px', display: 'flex', overflow: 'hidden', marginBottom: '8px' }}>
                <div style={{ width: `${Math.round((bullish / total) * 100)}%`, backgroundColor: colors.success }} />
                <div style={{ width: `${Math.round((neutral / total) * 100)}%`, backgroundColor: colors.warning }} />
                <div style={{ width: `${Math.round((bearish / total) * 100)}%`, backgroundColor: colors.alert }} />
              </div>
              {/* Net Score */}
              <div style={{
                textAlign: 'center', padding: '6px', backgroundColor: `${parseFloat(score) > 0 ? colors.success : parseFloat(score) < 0 ? colors.alert : colors.warning}10`,
                borderRadius: '2px', border: `1px solid ${parseFloat(score) > 0 ? colors.success : parseFloat(score) < 0 ? colors.alert : colors.warning}30`,
              }}>
                <div style={{ fontSize: '9px', color: colors.textMuted, marginBottom: '2px' }}>NET SCORE</div>
                <div style={{
                  fontSize: '16px', fontWeight: 700,
                  color: parseFloat(score) > 0 ? colors.success : parseFloat(score) < 0 ? colors.alert : colors.warning,
                }}>{score}</div>
              </div>
            </div>
          </div>

          {/* Feed Stats */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: colors.textMuted, letterSpacing: '0.5px', marginBottom: '8px' }}>
              FEED STATISTICS
            </div>
            <div style={{ backgroundColor: colors.background, border: `1px solid ${colors.panel}`, borderRadius: '2px', padding: '10px' }}>
              {[
                { label: 'ARTICLES', value: newsArticles.length, color: 'var(--ft-color-accent, #00E5FF)' },
                { label: 'FEEDS', value: feedCount, color: 'var(--ft-color-accent, #00E5FF)' },
                { label: 'ALERTS', value: alertCount, color: colors.primary },
                { label: 'UPDATES', value: newsUpdateCount, color: 'var(--ft-color-accent, #00E5FF)' },
              ].map((s, i) => (
                <div key={i} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: i < 3 ? '4px' : 0 }}>
                  <span style={{ color: colors.textMuted }}>{s.label}</span>
                  <span style={{ color: s.color, fontWeight: 700 }}>{s.value}</span>
                </div>
              ))}
            </div>
          </div>

          {/* Top Categories */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: colors.textMuted, letterSpacing: '0.5px', marginBottom: '8px' }}>
              CATEGORIES
            </div>
            <div style={{ backgroundColor: colors.background, border: `1px solid ${colors.panel}`, borderRadius: '2px', padding: '10px' }}>
              {topCategories.length > 0 ? topCategories.map(([cat, count], i) => (
                <div key={cat} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: i < topCategories.length - 1 ? '4px' : 0 }}>
                  <span style={{ color: colors.textMuted }}>{cat}</span>
                  <span style={{ color: 'var(--ft-color-purple, #9D4EDD)', fontWeight: 700 }}>{count}</span>
                </div>
              )) : (
                <div style={{ fontSize: '10px', color: colors.textMuted }}>No data</div>
              )}
            </div>
          </div>

          {/* Active Sources (compact) */}
          <div style={{ flex: 1, minHeight: 0 }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: colors.textMuted, letterSpacing: '0.5px', marginBottom: '8px' }}>
              ACTIVE SOURCES
            </div>
            <div style={{
              backgroundColor: colors.background, border: `1px solid ${colors.panel}`, borderRadius: '2px',
              padding: '8px 10px', display: 'flex', flexWrap: 'wrap', gap: '4px',
            }}>
              {activeSources.slice(0, 8).map((src, i) => (
                <span key={i} style={{
                  padding: '1px 6px', fontSize: '8px', fontWeight: 700,
                  color: 'var(--ft-color-accent, #00E5FF)', backgroundColor: 'rgba(0, 229, 255, 0.1)',
                  borderRadius: '2px', border: '1px solid rgba(0, 229, 255, 0.2)',
                }}>{src}</span>
              ))}
              {activeSources.length > 8 && (
                <span style={{ fontSize: '8px', color: colors.textMuted }}>+{activeSources.length - 8}</span>
              )}
            </div>
          </div>
        </div>

        {/* ── CENTER: News Grid ── */}
        <div id="news-primary-feed" style={{
          flex: 1, overflow: 'auto', padding: '12px',
          backgroundColor: colors.background,
        }}>
          {loading && newsArticles.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '100%', color: colors.textMuted,
            }}>
              <RefreshCw size={20} className="animate-spin" style={{ marginBottom: '10px', opacity: 0.5 }} />
              <span style={{ fontSize: '11px' }}>Loading news from {feedCount} RSS feeds...</span>
            </div>
          ) : filteredNews.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '100%', color: colors.textMuted,
            }}>
              <Newspaper size={20} style={{ marginBottom: '10px', opacity: 0.5 }} />
              <span style={{ fontSize: '11px' }}>No articles found for filter: {activeFilter}</span>
            </div>
          ) : (
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))',
              gap: '8px',
              alignContent: 'start',
            }}>
              {filteredNews.map(article => (
                <NewsGridCard
                  key={article.id}
                  article={article}
                  onAnalyze={handleAnalyzeArticle}
                  analyzing={analyzingArticle}
                  currentAnalyzingId={currentAnalyzingId}
                  openExternal={openExternal}
                  colors={colors}
                  priColor={priColor}
                  sentColor={sentColor}
                  impColor={impColor}
                  fontFamily={fontFamily}
                />
              ))}
            </div>
          )}
        </div>

        {/* ── RIGHT ANALYSIS PANEL (Collapsible) ── */}
        {analysisPanelOpen && (
          <div style={{
            width: '400px',
            flexShrink: 0,
            backgroundColor: colors.panel,
            borderLeft: `2px solid ${colors.primary}`,
            display: 'flex',
            flexDirection: 'column',
            overflow: 'hidden',
            boxShadow: `-4px 0 12px rgba(0,0,0,0.3)`,
            transition: 'width 0.3s ease',
          }}>
            {/* Panel Header */}
            <div style={{
              padding: '10px 14px',
              backgroundColor: colors.panel,
              borderBottom: `2px solid ${colors.primary}`,
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'space-between',
              flexShrink: 0,
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <Zap size={13} style={{ color: 'var(--ft-color-purple, #9D4EDD)' }} />
                <span style={{ fontSize: '10px', fontWeight: 700, color: 'var(--ft-color-purple, #9D4EDD)', letterSpacing: '0.5px' }}>
                  AI ANALYSIS
                </span>
              </div>
              <button
                onClick={() => setAnalysisPanelOpen(false)}
                style={{
                  background: 'none',
                  border: `1px solid ${colors.panel}`,
                  color: colors.textMuted,
                  cursor: 'pointer',
                  borderRadius: '2px',
                  padding: '3px 8px',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  fontSize: '9px',
                  fontWeight: 700,
                }}
              >
                <X size={11} />CLOSE
              </button>
            </div>

            {/* Panel Content */}
            <div style={{
              flex: 1,
              overflow: 'auto',
              padding: '14px',
            }}>
              {/* Article Info */}
              {currentAnalyzedArticle && (
                <div style={{ marginBottom: '14px' }}>
                  <div style={{
                    fontSize: '9px',
                    color: colors.textMuted,
                    fontWeight: 700,
                    marginBottom: '6px',
                    letterSpacing: '0.5px',
                  }}>
                    ANALYZING ARTICLE
                  </div>
                  <div style={{
                    padding: '10px 12px',
                    backgroundColor: colors.background,
                    border: `1px solid ${colors.panel}`,
                    borderLeft: `3px solid ${priColor(currentAnalyzedArticle.priority)}`,
                    borderRadius: '2px',
                  }}>
                    <div style={{
                      fontSize: '11px',
                      color: colors.text,
                      fontWeight: 600,
                      lineHeight: '1.4',
                      marginBottom: '8px',
                    }}>
                      {currentAnalyzedArticle.headline}
                    </div>
                    <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginBottom: '6px' }}>
                      <span style={{
                        fontSize: '8px',
                        color: 'var(--ft-color-accent, #00E5FF)',
                        fontWeight: 700,
                      }}>
                        {currentAnalyzedArticle.source}
                      </span>
                      {currentAnalyzedArticle.time && (
                        <span style={{ fontSize: '8px', color: colors.textMuted }}>
                          {currentAnalyzedArticle.time}
                        </span>
                      )}
                    </div>
                    <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                      <span style={{
                        padding: '1px 6px',
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        backgroundColor: `${priColor(currentAnalyzedArticle.priority)}20`,
                        color: priColor(currentAnalyzedArticle.priority),
                        border: `1px solid ${priColor(currentAnalyzedArticle.priority)}40`,
                      }}>
                        {currentAnalyzedArticle.priority}
                      </span>
                      <span style={{
                        padding: '1px 6px',
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        backgroundColor: `${sentColor(currentAnalyzedArticle.sentiment)}15`,
                        color: sentColor(currentAnalyzedArticle.sentiment),
                      }}>
                        {currentAnalyzedArticle.sentiment}
                      </span>
                      <span style={{
                        padding: '1px 6px',
                        fontSize: '8px',
                        fontWeight: 700,
                        borderRadius: '2px',
                        color: colors.textMuted,
                        backgroundColor: `${colors.textMuted}20`,
                      }}>
                        {currentAnalyzedArticle.category}
                      </span>
                    </div>
                  </div>
                </div>
              )}

              {/* Loading State */}
              {analyzingArticle && (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  padding: '40px 20px',
                  gap: '12px',
                }}>
                  <div style={{
                    width: '40px',
                    height: '40px',
                    border: `3px solid ${colors.panel}`,
                    borderTop: '3px solid var(--ft-color-purple, #9D4EDD)',
                    borderRadius: '50%',
                    animation: 'spin 1s linear infinite',
                  }} />
                  <div style={{
                    color: 'var(--ft-color-purple, #9D4EDD)',
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                  }}>
                    ANALYZING WITH AI...
                  </div>
                  <div style={{
                    color: colors.textMuted,
                    fontSize: '9px',
                    textAlign: 'center',
                    lineHeight: '1.5',
                  }}>
                    Processing article content and generating insights
                  </div>
                </div>
              )}

              {/* Analysis Results */}
              {showAnalysis && analysisData && !analyzingArticle && (
                <div>
                  {/* Credits Info */}
                  <div style={{
                    padding: '8px 12px',
                    backgroundColor: 'rgba(157, 78, 221, 0.1)',
                    border: '1px solid var(--ft-color-purple, #9D4EDD)',
                    borderRadius: '2px',
                    marginBottom: '14px',
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    fontSize: '9px',
                  }}>
                    <span style={{ color: 'var(--ft-color-purple, #9D4EDD)', fontWeight: 700, letterSpacing: '0.5px' }}>
                      ANALYSIS COMPLETE
                    </span>
                    <span style={{ color: colors.textMuted }}>
                      {analysisData.credits_used} CR | {analysisData.credits_remaining.toLocaleString()} REM
                    </span>
                  </div>

                  {/* Summary */}
                  {analysisData.analysis.summary && typeof analysisData.analysis.summary === 'string' && (
                  <div style={{ marginBottom: '14px' }}>
                    <div style={{
                      fontSize: '9px',
                      color: colors.textMuted,
                      fontWeight: 700,
                      marginBottom: '6px',
                      letterSpacing: '0.5px',
                    }}>
                      SUMMARY
                    </div>
                    <div style={{
                      fontSize: '10px',
                      color: colors.text,
                      lineHeight: '1.6',
                      padding: '10px 12px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.panel}`,
                      borderRadius: '2px',
                    }}>
                      {analysisData.analysis.summary}
                    </div>
                  </div>
                  )}

                  {/* Key Points */}
                  {analysisData.analysis.key_points && Array.isArray(analysisData.analysis.key_points) && analysisData.analysis.key_points.length > 0 && (
                    <div style={{ marginBottom: '14px' }}>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                        letterSpacing: '0.5px',
                      }}>
                        KEY POINTS
                      </div>
                      <div style={{
                        padding: '10px 12px',
                        backgroundColor: colors.background,
                        border: `1px solid ${colors.panel}`,
                        borderRadius: '2px',
                      }}>
                        {analysisData.analysis.key_points.filter(pt => typeof pt === 'string').map((pt, i) => (
                          <div
                            key={i}
                            style={{
                              fontSize: '9px',
                              color: colors.text,
                              paddingLeft: '10px',
                              borderLeft: `2px solid ${colors.primary}`,
                              marginBottom: i < analysisData.analysis.key_points.length - 1 ? '8px' : 0,
                              lineHeight: '1.5',
                            }}
                          >
                            {pt}
                          </div>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* Sentiment & Market Impact */}
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: '1fr 1fr',
                    gap: '8px',
                    marginBottom: '14px',
                  }}>
                    <div style={{
                      padding: '10px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.panel}`,
                      borderRadius: '2px',
                    }}>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                      }}>
                        SENTIMENT
                      </div>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '9px',
                        marginBottom: '3px',
                      }}>
                        <span style={{ color: colors.textMuted }}>Score</span>
                        <span style={{
                          color: getSentimentColor(analysisData.analysis.sentiment.score),
                          fontWeight: 700,
                        }}>
                          {analysisData.analysis.sentiment.score.toFixed(2)}
                        </span>
                      </div>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '9px',
                        marginBottom: '3px',
                      }}>
                        <span style={{ color: colors.textMuted }}>Intensity</span>
                        <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 700 }}>
                          {analysisData.analysis.sentiment.intensity.toFixed(2)}
                        </span>
                      </div>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '9px',
                      }}>
                        <span style={{ color: colors.textMuted }}>Confidence</span>
                        <span style={{ color: colors.success, fontWeight: 700 }}>
                          {(analysisData.analysis.sentiment.confidence * 100).toFixed(0)}%
                        </span>
                      </div>
                    </div>
                    <div style={{
                      padding: '10px',
                      backgroundColor: colors.background,
                      border: `1px solid ${colors.panel}`,
                      borderRadius: '2px',
                    }}>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                      }}>
                        MARKET IMPACT
                      </div>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '9px',
                        marginBottom: '3px',
                      }}>
                        <span style={{ color: colors.textMuted }}>Urgency</span>
                        <span style={{
                          color: getUrgencyColor(analysisData.analysis.market_impact.urgency),
                          fontWeight: 700,
                        }}>
                          {analysisData.analysis.market_impact.urgency}
                        </span>
                      </div>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        fontSize: '9px',
                      }}>
                        <span style={{ color: colors.textMuted }}>Prediction</span>
                        <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontWeight: 700 }}>
                          {analysisData.analysis.market_impact.prediction.replace('_', ' ').toUpperCase()}
                        </span>
                      </div>
                    </div>
                  </div>

                  {/* Risk Signals */}
                  {analysisData.analysis.risk_signals && typeof analysisData.analysis.risk_signals === 'object' && !('error' in analysisData.analysis.risk_signals) && (
                  <div style={{ marginBottom: '14px' }}>
                    <div style={{
                      fontSize: '9px',
                      color: colors.textMuted,
                      fontWeight: 700,
                      marginBottom: '6px',
                      letterSpacing: '0.5px',
                    }}>
                      RISK SIGNALS
                    </div>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                      {Object.entries(analysisData.analysis.risk_signals).map(([type, signal]) => {
                        // Skip if signal is not a valid object with level/details
                        if (!signal || typeof signal !== 'object' || 'error' in signal || !('level' in signal)) {
                          return null;
                        }
                        const riskSignal = signal as { level: string; details?: string };
                        return (
                        <div
                          key={type}
                          style={{
                            padding: '8px',
                            backgroundColor: colors.background,
                            border: `1px solid ${colors.panel}`,
                            borderRadius: '2px',
                          }}
                        >
                          <div style={{
                            color: getRiskColor(riskSignal.level || 'none'),
                            fontWeight: 700,
                            fontSize: '9px',
                            marginBottom: '2px',
                          }}>
                            {type.toUpperCase()}: {(riskSignal.level || 'NONE').toUpperCase()}
                          </div>
                          {riskSignal.details && typeof riskSignal.details === 'string' && (
                            <div style={{
                              color: colors.textMuted,
                              fontSize: '8px',
                              lineHeight: '1.3',
                            }}>
                              {riskSignal.details}
                            </div>
                          )}
                        </div>
                      );
                      })}
                    </div>
                  </div>
                  )}

                  {/* Topics */}
                  {analysisData.analysis.topics && Array.isArray(analysisData.analysis.topics) && analysisData.analysis.topics.length > 0 && (
                    <div style={{ marginBottom: '14px' }}>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                      }}>
                        TOPICS
                      </div>
                      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                        {analysisData.analysis.topics.filter(topic => typeof topic === 'string').map((topic, idx) => (
                          <span
                            key={idx}
                            style={{
                              padding: '2px 8px',
                              backgroundColor: 'rgba(157, 78, 221, 0.15)',
                              color: colors.text,
                              fontSize: '8px',
                              fontWeight: 700,
                              borderRadius: '2px',
                              border: '1px solid var(--ft-color-purple, #9D4EDD)',
                            }}
                          >
                            {topic}
                          </span>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* Keywords */}
                  {analysisData.analysis.keywords && Array.isArray(analysisData.analysis.keywords) && analysisData.analysis.keywords.length > 0 && (
                    <div style={{ marginBottom: '14px' }}>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                      }}>
                        KEYWORDS
                      </div>
                      <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                        {analysisData.analysis.keywords.filter(kw => typeof kw === 'string').slice(0, 10).map((kw, idx) => (
                          <span
                            key={idx}
                            style={{
                              padding: '2px 8px',
                              backgroundColor: `${colors.textMuted}20`,
                              color: colors.textMuted,
                              fontSize: '8px',
                              borderRadius: '2px',
                              border: `1px solid ${colors.textMuted}`,
                            }}
                          >
                            {kw}
                          </span>
                        ))}
                      </div>
                    </div>
                  )}

                  {/* Organizations */}
                  {analysisData.analysis.entities && analysisData.analysis.entities.organizations && Array.isArray(analysisData.analysis.entities.organizations) && analysisData.analysis.entities.organizations.length > 0 && (
                    <div>
                      <div style={{
                        fontSize: '9px',
                        color: colors.textMuted,
                        fontWeight: 700,
                        marginBottom: '6px',
                      }}>
                        ORGANIZATIONS
                      </div>
                      {analysisData.analysis.entities.organizations.filter(org => org && typeof org === 'object' && 'name' in org).map((org, idx) => (
                        <div
                          key={idx}
                          style={{
                            padding: '6px 10px',
                            backgroundColor: colors.background,
                            border: `1px solid ${colors.panel}`,
                            borderRadius: '2px',
                            marginBottom: '4px',
                          }}
                        >
                          <span style={{
                            color: colors.text,
                            fontWeight: 700,
                            fontSize: '9px',
                          }}>
                            {typeof org.name === 'string' ? org.name : ''}
                          </span>
                          <span style={{
                            color: colors.textMuted,
                            fontSize: '9px',
                            marginLeft: '6px',
                          }}>
                            {typeof org.sector === 'string' ? org.sector : ''} {org.ticker && typeof org.ticker === 'string' ? `| ${org.ticker}` : ''}
                          </span>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}

              {/* No Analysis Yet */}
              {!analyzingArticle && !showAnalysis && (
                <div style={{
                  display: 'flex',
                  flexDirection: 'column',
                  alignItems: 'center',
                  justifyContent: 'center',
                  padding: '40px 20px',
                  gap: '12px',
                  color: colors.textMuted,
                }}>
                  <Zap size={32} style={{ opacity: 0.3 }} />
                  <div style={{
                    fontSize: '10px',
                    fontWeight: 700,
                    letterSpacing: '0.5px',
                  }}>
                    NO ANALYSIS YET
                  </div>
                  <div style={{
                    fontSize: '9px',
                    textAlign: 'center',
                    lineHeight: '1.5',
                  }}>
                    Click "ANALYZE" on any article to see AI-powered insights
                  </div>
                </div>
              )}
            </div>
          </div>
        )}
      </div>

      {/* ── STATUS BAR ── */}
      <div style={{
        backgroundColor: colors.panel, borderTop: `1px solid ${colors.panel}`,
        padding: '3px 16px', fontSize: '9px', color: colors.textMuted, flexShrink: 0,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>FINCEPT NEWS v1.0</span>
          <span style={{ color: colors.panel }}>|</span>
          <span>FEEDS: <span style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{feedCount}</span></span>
          <span style={{ color: colors.panel }}>|</span>
          <span>ARTICLES: <span style={{ color: 'var(--ft-color-accent, #00E5FF)' }}>{newsArticles.length}</span></span>
          <span style={{ color: colors.panel }}>|</span>
          <span>FILTER: <span style={{ color: colors.primary }}>{activeFilter}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>SENTIMENT: <span style={{ color: parseFloat(score) > 0 ? colors.success : parseFloat(score) < 0 ? colors.alert : colors.warning }}>{score}</span></span>
          <span style={{ color: colors.panel }}>|</span>
          <span>{currentTime.toTimeString().substring(0, 8)}</span>
          <span style={{ color: colors.panel }}>|</span>
          <span style={{ color: loading ? colors.warning : colors.success, fontWeight: 700 }}>{loading ? 'UPDATING' : 'LIVE'}</span>
        </div>
      </div>

      {/* ── CSS ── */}
      <style dangerouslySetInnerHTML={{ __html: scrollbarCSS }} />

      <RSSFeedSettingsModal isOpen={showFeedSettings} onClose={() => setShowFeedSettings(false)} onFeedsUpdated={() => handleRefresh(true)} />
    </div>
  );
};

export default NewsTab;
