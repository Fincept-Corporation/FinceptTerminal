import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Info, Settings, RefreshCw, Clock, Zap, Newspaper, ExternalLink, Copy, BookOpen } from 'lucide-react';
import { fetchAllNews, type NewsArticle, getRSSFeedCount, getActiveSources, isUsingMockData, setNewsCacheTTL } from '@/services/news/newsService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { TimezoneSelector } from '@/components/common/TimezoneSelector';
import { useTranslation } from 'react-i18next';
import { analyzeNewsArticle, type NewsAnalysisData, getSentimentColor, getUrgencyColor, getRiskColor } from '@/services/news/newsAnalysisService';
import { createNewsTabTour } from '@/components/tabs/tours/newsTabTour';
import RSSFeedSettingsModal from './RSSFeedSettingsModal';

declare global {
  interface Window { __TAURI__?: any; }
}

// ── FINCEPT Design System ──
const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F', HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A', HOVER: '#1F1F1F', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD700', BLUE: '#0088FF', PURPLE: '#9D4EDD',
};
const FONT = '"IBM Plex Mono","Consolas",monospace';

// ── Helpers ──
const priColor = (p: string) => ({ FLASH: F.RED, URGENT: F.ORANGE, BREAKING: F.YELLOW, ROUTINE: F.GRAY }[p] || F.MUTED);
const sentColor = (s: string) => ({ BULLISH: F.GREEN, BEARISH: F.RED, NEUTRAL: F.YELLOW }[s] || F.MUTED);
const impColor = (i: string) => ({ HIGH: F.RED, MEDIUM: F.YELLOW, LOW: F.GREEN }[i] || F.MUTED);

// ── Section Header ──
const SectionHead: React.FC<{ title: string; right?: React.ReactNode }> = ({ title, right }) => (
  <div style={{
    padding: '4px 8px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`,
    display: 'flex', justifyContent: 'space-between', alignItems: 'center', flexShrink: 0,
  }}>
    <span style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>{title}</span>
    {right && <span style={{ fontSize: '8px', color: F.CYAN }}>{right}</span>}
  </div>
);

// ── Compact Row ──
const NewsRow: React.FC<{
  article: NewsArticle; selected: boolean; onClick: () => void;
}> = ({ article, selected, onClick }) => (
  <div
    onClick={onClick}
    style={{
      display: 'flex', alignItems: 'center', gap: '6px',
      padding: '3px 6px', cursor: 'pointer',
      backgroundColor: selected ? `${F.ORANGE}15` : 'transparent',
      borderLeft: selected ? `2px solid ${F.ORANGE}` : `2px solid transparent`,
      borderBottom: `1px solid ${F.BORDER}08`,
      transition: 'background 0.15s',
      minHeight: '20px',
    }}
    onMouseEnter={e => { if (!selected) e.currentTarget.style.backgroundColor = F.HOVER; }}
    onMouseLeave={e => { if (!selected) e.currentTarget.style.backgroundColor = 'transparent'; }}
  >
    <span style={{ color: F.MUTED, fontSize: '8px', minWidth: '38px', flexShrink: 0 }}>{article.time}</span>
    <span style={{
      color: priColor(article.priority), fontSize: '7px', fontWeight: 700,
      minWidth: '40px', flexShrink: 0, letterSpacing: '0.3px',
    }}>{article.priority}</span>
    <span style={{
      color: F.CYAN, fontSize: '7px', fontWeight: 700,
      minWidth: '55px', flexShrink: 0, letterSpacing: '0.3px',
    }}>{article.source}</span>
    <span style={{
      color: F.WHITE, fontSize: '9px', fontWeight: selected ? 700 : 400,
      flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
    }}>{article.headline}</span>
    <span style={{
      color: sentColor(article.sentiment), fontSize: '7px', fontWeight: 700,
      minWidth: '28px', flexShrink: 0, textAlign: 'right',
    }}>{article.sentiment?.charAt(0) || '-'}</span>
    <span style={{
      color: impColor(article.impact), fontSize: '7px', fontWeight: 700,
      minWidth: '10px', flexShrink: 0, textAlign: 'center',
    }}>{article.impact?.charAt(0) || '-'}</span>
  </div>
);

// ── Alert Row (for left sidebar) ──
const AlertRow: React.FC<{ article: NewsArticle; onClick: () => void }> = ({ article, onClick }) => (
  <div onClick={onClick} style={{
    padding: '4px 6px', cursor: 'pointer',
    borderLeft: `2px solid ${priColor(article.priority)}`,
    marginBottom: '2px', transition: 'background 0.15s',
  }}
    onMouseEnter={e => e.currentTarget.style.backgroundColor = F.HOVER}
    onMouseLeave={e => e.currentTarget.style.backgroundColor = 'transparent'}
  >
    <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '1px' }}>
      <span style={{ color: priColor(article.priority), fontSize: '7px', fontWeight: 700 }}>{article.priority}</span>
      <span style={{ color: F.MUTED, fontSize: '7px' }}>{article.time}</span>
    </div>
    <div style={{
      color: F.WHITE, fontSize: '8px', fontWeight: 700, lineHeight: '1.2',
      overflow: 'hidden', textOverflow: 'ellipsis',
      display: '-webkit-box', WebkitLineClamp: 2, WebkitBoxOrient: 'vertical' as any,
    }}>{article.headline}</div>
  </div>
);

// ── Stat Row ──
const StatRow: React.FC<{ label: string; value: number | string; color: string }> = ({ label, value, color }) => (
  <div style={{ display: 'flex', justifyContent: 'space-between', padding: '2px 0', fontSize: '8px' }}>
    <span style={{ color: F.GRAY }}>{label}</span>
    <span style={{ color, fontWeight: 700 }}>{value}</span>
  </div>
);

// ══════════════════════════════════════════════════════════════
// MAIN COMPONENT
// ══════════════════════════════════════════════════════════════
const NewsTab: React.FC = () => {
  const { t } = useTranslation('news');
  const [currentTime, setCurrentTime] = useState(new Date());
  const [activeFilter, setActiveFilter] = useState('ALL');
  const [newsUpdateCount, setNewsUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(0);
  const [newsArticles, setNewsArticles] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeSources, setActiveSources] = useState<string[]>([]);
  const [feedCount, setFeedCount] = useState(9);
  const [selectedArticle, setSelectedArticle] = useState<NewsArticle | null>(null);
  const [showWebView, setShowWebView] = useState(false);
  const [refreshInterval, setRefreshInterval] = useState(10);
  const [showIntervalSettings, setShowIntervalSettings] = useState(false);
  const [isRecording, setIsRecording] = useState(false);
  const [analysisData, setAnalysisData] = useState<NewsAnalysisData | null>(null);
  const [analyzingArticle, setAnalyzingArticle] = useState(false);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const [showFeedSettings, setShowFeedSettings] = useState(false);

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

  const handleAnalyzeArticle = async () => {
    if (!selectedArticle?.link) return;
    setAnalyzingArticle(true); setShowAnalysis(false); setAnalysisData(null);
    try {
      const result = await analyzeNewsArticle(selectedArticle.link);
      if (result.success && 'data' in result) { setAnalysisData(result.data); setShowAnalysis(true); }
    } catch { /* silent */ } finally { setAnalyzingArticle(false); }
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
        setActiveSources(sources); setFeedCount(count);
        setNewsUpdateCount(prev => prev + 1);
      } catch { /* silent */ } finally { setLoading(false); }
    };
    if (isInitialMount.current) { isInitialMount.current = false; loadNews(false); } else { loadNews(true); }
    const iv = setInterval(() => loadNews(true), refreshInterval * 60 * 1000);
    return () => clearInterval(iv);
  }, [refreshInterval, isRecording, activeFilter]);

  useEffect(() => {
    const t = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(t);
  }, []);

  // ── Derived data ──
  const filteredNews = activeFilter === 'ALL'
    ? newsArticles
    : newsArticles.filter(a => a.category === activeFilter);

  const urgentNews = newsArticles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT' || a.priority === 'BREAKING');

  const bullish = newsArticles.filter(a => a.sentiment === 'BULLISH').length;
  const bearish = newsArticles.filter(a => a.sentiment === 'BEARISH').length;
  const neutral = newsArticles.filter(a => a.sentiment === 'NEUTRAL').length;
  const total = newsArticles.length || 1;
  const score = ((bullish - bearish) / total).toFixed(2);

  const catCounts: Record<string, number> = {};
  newsArticles.forEach(a => { catCounts[a.category] = (catCounts[a.category] || 0) + 1; });

  // Ticker
  const tickerText = newsArticles.length === 0 ? t('messages.loadingFeeds')
    : newsArticles.map(a => `${a.priority} | ${a.source} | ${a.headline.toUpperCase()}     `).join('');
  const tickerDur = Math.max(1200, Math.min(4800, (tickerText.length / 1000) * 1600));

  // Filter defs
  const filters = [
    { key: 'F1', label: 'ALL', f: 'ALL' }, { key: 'F2', label: 'MKT', f: 'MARKETS' },
    { key: 'F3', label: 'ERN', f: 'EARNINGS' }, { key: 'F4', label: 'ECO', f: 'ECONOMIC' },
    { key: 'F5', label: 'TCH', f: 'TECH' }, { key: 'F6', label: 'NRG', f: 'ENERGY' },
    { key: 'F7', label: 'BNK', f: 'BANKING' }, { key: 'F8', label: 'CRY', f: 'CRYPTO' },
    { key: 'F9', label: 'GEO', f: 'GEOPOLITICS' },
  ];

  // ── Open article externally ──
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

  // ══════════════════════════════════════════════════════════════
  // RENDER
  // ══════════════════════════════════════════════════════════════
  return (
    <div style={{
      height: '100%', backgroundColor: F.DARK_BG, color: F.WHITE, fontFamily: FONT,
      overflow: 'hidden', display: 'flex', flexDirection: 'column', fontSize: '9px',
    }}>

      {/* ── TOP NAV ── */}
      <div style={{
        backgroundColor: F.HEADER_BG, borderBottom: `2px solid ${F.ORANGE}`,
        padding: '4px 12px', display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`, flexShrink: 0, position: 'relative',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Newspaper size={12} style={{ color: F.ORANGE }} />
          <span style={{ color: F.ORANGE, fontWeight: 700, fontSize: '10px', letterSpacing: '0.5px' }}>
            {t('title')}
          </span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.RED, fontWeight: 700, fontSize: '8px', animation: 'blink 1s infinite' }}>LIVE</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.YELLOW, fontSize: '8px', fontWeight: 700 }}>ALT:{alertCount}</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.GREEN, fontSize: '8px', fontWeight: 700 }}>{feedCount}SRC</span>
          <span style={{ color: F.BORDER }}>|</span>
          <TimezoneSelector compact />
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button onClick={() => { createNewsTabTour().drive(); }}
            style={{ padding: '2px 6px', background: 'none', border: `1px solid ${F.BORDER}`, color: F.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px' }}
          ><Info size={9} />HELP</button>
          <button id="news-refresh" onClick={() => handleRefresh(true)} disabled={loading}
            style={{ padding: '2px 6px', backgroundColor: loading ? F.MUTED : F.ORANGE, color: loading ? F.GRAY : F.DARK_BG, border: 'none', fontSize: '8px', fontWeight: 700, cursor: loading ? 'not-allowed' : 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px' }}
          ><RefreshCw size={9} />{loading ? 'WAIT' : 'REFRESH'}</button>
          <button id="news-interval-settings" onClick={() => setShowIntervalSettings(!showIntervalSettings)}
            style={{ padding: '2px 6px', background: 'none', border: `1px solid ${F.BORDER}`, color: F.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px' }}
          ><Clock size={9} />{refreshInterval}M</button>
          <button onClick={() => setShowFeedSettings(true)}
            style={{ padding: '2px 6px', background: 'none', border: `1px solid ${F.BORDER}`, color: F.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px' }}
          ><Settings size={9} />SRC</button>
          <RecordingControlPanel tabName="News" onRecordingChange={setIsRecording} onRecordingStart={recordCurrentData} />
        </div>

        {showIntervalSettings && (
          <div style={{ position: 'absolute', top: '100%', right: 12, backgroundColor: F.PANEL_BG, border: `1px solid ${F.ORANGE}`, borderRadius: '2px', padding: '6px', zIndex: 1000, marginTop: '2px' }}>
            <div style={{ fontSize: '8px', color: F.GRAY, fontWeight: 700, marginBottom: '4px', letterSpacing: '0.5px' }}>INTERVAL</div>
            {[1, 2, 5, 10, 15, 30].map(m => (
              <button key={m} onClick={() => { setRefreshInterval(m); setShowIntervalSettings(false); }}
                style={{ width: '100%', backgroundColor: refreshInterval === m ? F.ORANGE : 'transparent', color: refreshInterval === m ? F.DARK_BG : F.WHITE, border: `1px solid ${refreshInterval === m ? F.ORANGE : F.BORDER}`, padding: '2px 6px', fontSize: '8px', fontWeight: 700, cursor: 'pointer', marginBottom: '1px', textAlign: 'left', borderRadius: '2px' }}
              >{m}min</button>
            ))}
          </div>
        )}
      </div>

      {/* ── TICKER ── */}
      <div style={{ backgroundColor: F.PANEL_BG, borderBottom: `1px solid ${F.BORDER}`, padding: '2px 12px', display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
        <span style={{ color: F.RED, fontWeight: 700, fontSize: '8px', flexShrink: 0 }}>BREAKING:</span>
        <div className="ticker-wrap" style={{ flex: 1, overflow: 'hidden', height: '14px' }}>
          <div className="ticker-move">
            <span className="ticker-item">{tickerText}</span>
            <span className="ticker-item">{tickerText}</span>
          </div>
        </div>
        <span style={{ color: loading ? F.YELLOW : F.GREEN, fontSize: '8px', fontWeight: 700, flexShrink: 0 }}>
          {loading ? 'UPD' : (isUsingMockData() ? 'DEMO' : 'LIVE')}
        </span>
      </div>

      {/* ── FILTER BAR ── */}
      <div id="news-filters" style={{ backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, padding: '2px 6px', flexShrink: 0, display: 'flex', gap: '2px' }}>
        {filters.map(item => (
          <button key={item.key} onClick={() => setActiveFilter(item.f)}
            style={{
              padding: '2px 8px', backgroundColor: activeFilter === item.f ? F.ORANGE : 'transparent',
              color: activeFilter === item.f ? F.DARK_BG : F.GRAY, border: 'none',
              fontSize: '8px', fontWeight: 700, letterSpacing: '0.3px', cursor: 'pointer', borderRadius: '2px',
            }}>
            <span style={{ color: activeFilter === item.f ? F.DARK_BG : F.MUTED, marginRight: '2px', fontSize: '7px' }}>{item.key}</span>
            {item.label}
            {catCounts[item.f] ? <span style={{ marginLeft: '3px', opacity: 0.6 }}>({catCounts[item.f]})</span> : null}
          </button>
        ))}
      </div>

      {/* ── COLUMN HEADER for feed table ── */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '6px', padding: '2px 6px',
        backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
        fontSize: '7px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.5px',
      }}>
        <span style={{ minWidth: '38px' }}>TIME</span>
        <span style={{ minWidth: '40px' }}>PRI</span>
        <span style={{ minWidth: '55px' }}>SOURCE</span>
        <span style={{ flex: 1 }}>HEADLINE</span>
        <span style={{ minWidth: '28px', textAlign: 'right' }}>SNT</span>
        <span style={{ minWidth: '10px', textAlign: 'center' }}>I</span>
      </div>

      {/* ══ MAIN CONTENT ══ */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', gap: '1px', backgroundColor: F.BORDER }}>

        {/* ── LEFT SIDEBAR: Alerts + Stats ── */}
        <div style={{ width: '200px', flexShrink: 0, backgroundColor: F.PANEL_BG, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {/* Breaking Alerts */}
          <SectionHead title="BREAKING / URGENT" right={`${urgentNews.length}`} />
          <div style={{ flex: 1, overflow: 'auto', padding: '2px' }}>
            {urgentNews.length === 0 ? (
              <div style={{ padding: '12px', color: F.MUTED, fontSize: '8px', textAlign: 'center' }}>NO ALERTS</div>
            ) : urgentNews.slice(0, 20).map(a => (
              <AlertRow key={a.id} article={a} onClick={() => setSelectedArticle(a)} />
            ))}
          </div>

          {/* Category Breakdown */}
          <SectionHead title="CATEGORY BREAKDOWN" />
          <div style={{ padding: '4px 6px', borderBottom: `1px solid ${F.BORDER}` }}>
            {Object.entries(catCounts).sort((a, b) => b[1] - a[1]).slice(0, 8).map(([cat, count]) => (
              <StatRow key={cat} label={cat} value={count} color={F.CYAN} />
            ))}
          </div>

          {/* Sentiment */}
          <SectionHead title="SENTIMENT" />
          <div style={{ padding: '4px 6px' }}>
            <StatRow label="BULLISH" value={`${bullish} (${Math.round((bullish / total) * 100)}%)`} color={F.GREEN} />
            <StatRow label="BEARISH" value={`${bearish} (${Math.round((bearish / total) * 100)}%)`} color={F.RED} />
            <StatRow label="NEUTRAL" value={`${neutral} (${Math.round((neutral / total) * 100)}%)`} color={F.YELLOW} />
            <div style={{ marginTop: '4px', paddingTop: '4px', borderTop: `1px solid ${F.BORDER}` }}>
              <StatRow label="NET SCORE" value={`${score} ${parseFloat(score) > 0 ? 'BULL' : parseFloat(score) < 0 ? 'BEAR' : 'FLAT'}`}
                color={parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW} />
            </div>
            {/* Sentiment bar */}
            <div style={{ marginTop: '4px', height: '3px', backgroundColor: F.BORDER, borderRadius: '2px', display: 'flex', overflow: 'hidden' }}>
              <div style={{ width: `${Math.round((bullish / total) * 100)}%`, backgroundColor: F.GREEN }} />
              <div style={{ width: `${Math.round((neutral / total) * 100)}%`, backgroundColor: F.YELLOW }} />
              <div style={{ width: `${Math.round((bearish / total) * 100)}%`, backgroundColor: F.RED }} />
            </div>
          </div>

          {/* Quick Stats */}
          <SectionHead title="FEED STATS" />
          <div style={{ padding: '4px 6px' }}>
            <StatRow label="TOTAL" value={newsArticles.length} color={F.CYAN} />
            <StatRow label="FEEDS" value={feedCount} color={F.CYAN} />
            <StatRow label="UPDATES" value={newsUpdateCount} color={F.CYAN} />
            <StatRow label="FILTER" value={activeFilter} color={F.ORANGE} />
          </div>
        </div>

        {/* ── CENTER: Dense News Feed ── */}
        <div id="news-primary-feed" style={{ flex: 1, backgroundColor: F.PANEL_BG, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          <div style={{ flex: 1, overflow: 'auto' }}>
            {loading && newsArticles.length === 0 ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '9px' }}>
                <RefreshCw size={16} className="animate-spin" style={{ marginBottom: '6px', opacity: 0.5 }} />
                Loading {feedCount} feeds...
              </div>
            ) : filteredNews.length === 0 ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '9px' }}>
                <Newspaper size={16} style={{ marginBottom: '6px', opacity: 0.5 }} />
                No articles for: {activeFilter}
              </div>
            ) : filteredNews.map(article => (
              <NewsRow key={article.id} article={article}
                selected={selectedArticle?.id === article.id}
                onClick={() => { setSelectedArticle(article); setShowAnalysis(false); setAnalysisData(null); setShowWebView(false); }}
              />
            ))}
          </div>
        </div>

        {/* ── RIGHT: Article Detail + AI Analysis ── */}
        <div id="news-ai-analysis" style={{ width: '340px', flexShrink: 0, backgroundColor: F.PANEL_BG, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
          {selectedArticle ? (
            <>
              {/* Article Detail */}
              <SectionHead title="ARTICLE DETAIL" right={selectedArticle.time} />
              <div style={{ overflow: 'auto', flex: 1, padding: '6px' }}>
                {/* Show web view or detail */}
                {showWebView ? (
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                    <div style={{ display: 'flex', gap: '4px' }}>
                      <button onClick={() => setShowWebView(false)}
                        style={{ padding: '3px 8px', backgroundColor: F.ORANGE, color: F.DARK_BG, border: 'none', fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px' }}
                      >BACK</button>
                      <span style={{ color: F.GRAY, fontSize: '8px', fontWeight: 700, display: 'flex', alignItems: 'center' }}>READER VIEW</span>
                    </div>
                    <div style={{ padding: '4px 6px', borderBottom: `2px solid ${F.ORANGE}` }}>
                      <div style={{ display: 'flex', gap: '4px', marginBottom: '6px' }}>
                        {[{ label: selectedArticle.priority, color: priColor(selectedArticle.priority) },
                          { label: selectedArticle.category, color: F.CYAN },
                          { label: selectedArticle.sentiment, color: sentColor(selectedArticle.sentiment) },
                        ].map((b, i) => (
                          <span key={i} style={{ padding: '1px 4px', backgroundColor: `${b.color}20`, color: b.color, fontSize: '7px', fontWeight: 700, borderRadius: '2px' }}>{b.label}</span>
                        ))}
                      </div>
                      <div style={{ fontSize: '11px', fontWeight: 700, lineHeight: '1.3', color: F.WHITE, marginBottom: '4px' }}>{selectedArticle.headline}</div>
                      <div style={{ fontSize: '8px', color: F.GRAY, display: 'flex', gap: '8px' }}>
                        <span>{selectedArticle.source}</span>
                        <span>{selectedArticle.region}</span>
                        {selectedArticle.tickers?.length > 0 && <span style={{ color: F.CYAN }}>{selectedArticle.tickers.join(', ')}</span>}
                      </div>
                    </div>
                    <div style={{ fontSize: '9px', lineHeight: '1.6', color: F.WHITE }}>{selectedArticle.summary}</div>
                    <div style={{ padding: '8px', backgroundColor: `${F.ORANGE}10`, border: `1px solid ${F.ORANGE}`, borderRadius: '2px', fontSize: '8px', color: F.GRAY }}>
                      <span style={{ color: F.YELLOW, fontWeight: 700 }}>NOTE: </span>
                      RSS summary shown. Click "OPEN" for full article.
                    </div>
                    {selectedArticle.link && (
                      <button onClick={() => openExternal(selectedArticle.link!)}
                        style={{ padding: '4px 8px', backgroundColor: F.BLUE, color: F.WHITE, border: 'none', fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '4px', justifyContent: 'center' }}
                      ><ExternalLink size={10} />OPEN FULL ARTICLE</button>
                    )}
                  </div>
                ) : (
                  <>
                    {/* Priority + Meta badges */}
                    <div style={{ display: 'flex', gap: '3px', flexWrap: 'wrap', marginBottom: '4px' }}>
                      {[
                        { label: selectedArticle.priority, color: priColor(selectedArticle.priority) },
                        { label: selectedArticle.category, color: F.CYAN },
                        { label: selectedArticle.sentiment, color: sentColor(selectedArticle.sentiment) },
                        { label: selectedArticle.region, color: F.PURPLE },
                        { label: `IMPACT: ${selectedArticle.impact}`, color: impColor(selectedArticle.impact) },
                      ].map((b, i) => (
                        <span key={i} style={{ padding: '1px 5px', backgroundColor: `${b.color}15`, color: b.color, fontSize: '7px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${b.color}30` }}>{b.label}</span>
                      ))}
                    </div>

                    {/* Source + time */}
                    <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px', color: F.GRAY, marginBottom: '6px' }}>
                      <span style={{ color: F.CYAN, fontWeight: 700 }}>{selectedArticle.source}</span>
                      <span>{selectedArticle.time}</span>
                    </div>

                    {/* Headline */}
                    <div style={{
                      color: F.WHITE, fontSize: '11px', fontWeight: 700, lineHeight: '1.3', marginBottom: '6px',
                      padding: '6px', backgroundColor: F.DARK_BG, borderLeft: `2px solid ${priColor(selectedArticle.priority)}`, borderRadius: '2px',
                    }}>{selectedArticle.headline}</div>

                    {/* Tickers */}
                    {selectedArticle.tickers && selectedArticle.tickers.length > 0 && (
                      <div style={{ display: 'flex', gap: '3px', marginBottom: '6px', flexWrap: 'wrap' }}>
                        {selectedArticle.tickers.map((t, i) => (
                          <span key={i} style={{ padding: '1px 5px', backgroundColor: `${F.GREEN}15`, color: F.GREEN, fontSize: '7px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${F.GREEN}30` }}>{t}</span>
                        ))}
                      </div>
                    )}

                    {/* Summary */}
                    <div style={{ color: F.GRAY, fontSize: '9px', lineHeight: '1.5', marginBottom: '8px' }}>{selectedArticle.summary}</div>

                    {/* Action buttons */}
                    {selectedArticle.link && (
                      <div style={{ display: 'flex', gap: '3px', marginBottom: '8px' }}>
                        <button onClick={() => setShowWebView(true)}
                          style={{ flex: 1, padding: '4px', backgroundColor: F.GREEN, color: F.DARK_BG, border: 'none', fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px' }}
                        ><BookOpen size={9} />READ</button>
                        <button onClick={() => openExternal(selectedArticle.link!)}
                          style={{ flex: 1, padding: '4px', backgroundColor: F.BLUE, color: F.WHITE, border: 'none', fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px' }}
                        ><ExternalLink size={9} />OPEN</button>
                        <button onClick={() => { navigator.clipboard.writeText(selectedArticle.link!); }}
                          style={{ padding: '4px 8px', background: 'none', border: `1px solid ${F.BORDER}`, color: F.GRAY, fontSize: '8px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '3px' }}
                        ><Copy size={9} /></button>
                        <button onClick={handleAnalyzeArticle} disabled={analyzingArticle}
                          style={{ flex: 1, padding: '4px', backgroundColor: analyzingArticle ? F.MUTED : F.PURPLE, color: F.WHITE, border: 'none', fontSize: '8px', fontWeight: 700, cursor: analyzingArticle ? 'not-allowed' : 'pointer', borderRadius: '2px', opacity: analyzingArticle ? 0.5 : 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px' }}
                        ><Zap size={9} />{analyzingArticle ? 'WAIT' : 'AI'}</button>
                      </div>
                    )}

                    {/* AI Analysis Results */}
                    {showAnalysis && analysisData && (
                      <div style={{ borderTop: `1px solid ${F.BORDER}`, paddingTop: '6px' }}>
                        <div style={{ padding: '4px 6px', backgroundColor: `${F.PURPLE}15`, border: `1px solid ${F.PURPLE}`, borderRadius: '2px', marginBottom: '6px', display: 'flex', justifyContent: 'space-between', fontSize: '7px' }}>
                          <span style={{ color: F.PURPLE, fontWeight: 700 }}>AI ANALYSIS</span>
                          <span style={{ color: F.GRAY }}>{analysisData.credits_used}CR | {analysisData.credits_remaining.toLocaleString()}REM</span>
                        </div>

                        {/* Summary */}
                        <div style={{ fontSize: '8px', color: F.WHITE, lineHeight: '1.5', padding: '4px 6px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', marginBottom: '6px' }}>
                          {analysisData.analysis.summary}
                        </div>

                        {/* Key points */}
                        {analysisData.analysis.key_points.length > 0 && (
                          <div style={{ marginBottom: '6px' }}>
                            <div style={{ fontSize: '7px', color: F.GRAY, fontWeight: 700, marginBottom: '3px', letterSpacing: '0.5px' }}>KEY POINTS</div>
                            {analysisData.analysis.key_points.map((p, i) => (
                              <div key={i} style={{ fontSize: '8px', color: F.WHITE, paddingLeft: '6px', borderLeft: `2px solid ${F.ORANGE}`, marginBottom: '3px', lineHeight: '1.3' }}>{p}</div>
                            ))}
                          </div>
                        )}

                        {/* Sentiment + Impact grid */}
                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', marginBottom: '6px' }}>
                          <div style={{ padding: '4px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '7px' }}>
                            <div style={{ color: F.GRAY, fontWeight: 700, marginBottom: '2px' }}>SENTIMENT</div>
                            <StatRow label="Score" value={analysisData.analysis.sentiment.score.toFixed(2)} color={getSentimentColor(analysisData.analysis.sentiment.score)} />
                            <StatRow label="Intensity" value={analysisData.analysis.sentiment.intensity.toFixed(2)} color={F.CYAN} />
                            <StatRow label="Confidence" value={`${(analysisData.analysis.sentiment.confidence * 100).toFixed(0)}%`} color={F.GREEN} />
                          </div>
                          <div style={{ padding: '4px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '7px' }}>
                            <div style={{ color: F.GRAY, fontWeight: 700, marginBottom: '2px' }}>MARKET IMPACT</div>
                            <StatRow label="Urgency" value={analysisData.analysis.market_impact.urgency} color={getUrgencyColor(analysisData.analysis.market_impact.urgency)} />
                            <StatRow label="Prediction" value={analysisData.analysis.market_impact.prediction.replace('_', ' ').toUpperCase()} color={F.CYAN} />
                          </div>
                        </div>

                        {/* Risk Signals */}
                        <div style={{ marginBottom: '6px' }}>
                          <div style={{ fontSize: '7px', color: F.GRAY, fontWeight: 700, marginBottom: '3px', letterSpacing: '0.5px' }}>RISK SIGNALS</div>
                          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '3px' }}>
                            {Object.entries(analysisData.analysis.risk_signals).map(([type, signal]) => (
                              <div key={type} style={{ padding: '3px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '7px' }}>
                                <span style={{ color: getRiskColor(signal.level), fontWeight: 700 }}>{type.toUpperCase()}: {signal.level.toUpperCase()}</span>
                                {signal.details && <div style={{ color: F.MUTED, fontSize: '7px', marginTop: '1px' }}>{signal.details}</div>}
                              </div>
                            ))}
                          </div>
                        </div>

                        {/* Topics + Keywords */}
                        {(analysisData.analysis.topics.length > 0 || analysisData.analysis.keywords.length > 0) && (
                          <div style={{ marginBottom: '6px' }}>
                            {analysisData.analysis.topics.length > 0 && (
                              <>
                                <div style={{ fontSize: '7px', color: F.GRAY, fontWeight: 700, marginBottom: '2px' }}>TOPICS</div>
                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px', marginBottom: '4px' }}>
                                  {analysisData.analysis.topics.map((t, i) => (
                                    <span key={i} style={{ padding: '1px 4px', backgroundColor: `${F.PURPLE}20`, color: F.WHITE, fontSize: '7px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${F.PURPLE}` }}>{t}</span>
                                  ))}
                                </div>
                              </>
                            )}
                            {analysisData.analysis.keywords.length > 0 && (
                              <>
                                <div style={{ fontSize: '7px', color: F.GRAY, fontWeight: 700, marginBottom: '2px' }}>KEYWORDS</div>
                                <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px' }}>
                                  {analysisData.analysis.keywords.slice(0, 8).map((k, i) => (
                                    <span key={i} style={{ padding: '1px 4px', backgroundColor: `${F.MUTED}20`, color: F.GRAY, fontSize: '7px', borderRadius: '2px', border: `1px solid ${F.MUTED}` }}>{k}</span>
                                  ))}
                                </div>
                              </>
                            )}
                          </div>
                        )}

                        {/* Entities */}
                        {analysisData.analysis.entities.organizations.length > 0 && (
                          <div>
                            <div style={{ fontSize: '7px', color: F.GRAY, fontWeight: 700, marginBottom: '2px' }}>ORGANIZATIONS</div>
                            {analysisData.analysis.entities.organizations.map((org, i) => (
                              <div key={i} style={{ padding: '3px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '7px', marginBottom: '2px' }}>
                                <span style={{ color: F.WHITE, fontWeight: 700 }}>{org.name}</span>
                                <span style={{ color: F.GRAY, marginLeft: '4px' }}>{org.sector} {org.ticker ? `| ${org.ticker}` : ''}</span>
                              </div>
                            ))}
                          </div>
                        )}
                      </div>
                    )}
                  </>
                )}
              </div>
            </>
          ) : (
            /* No article selected */
            <>
              <SectionHead title="ARTICLE DETAIL" />
              <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', color: F.MUTED, padding: '20px', textAlign: 'center' }}>
                <Newspaper size={20} style={{ marginBottom: '8px', opacity: 0.3 }} />
                <div style={{ fontSize: '9px', fontWeight: 700, marginBottom: '4px' }}>SELECT AN ARTICLE</div>
                <div style={{ fontSize: '8px' }}>Click any row in the feed to view details, read the full article, or run AI analysis.</div>
              </div>
            </>
          )}
        </div>
      </div>

      {/* ── STATUS BAR ── */}
      <div style={{
        backgroundColor: F.HEADER_BG, borderTop: `1px solid ${F.BORDER}`,
        padding: '2px 12px', fontSize: '8px', color: F.GRAY, flexShrink: 0,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>FINCEPT NEWS v1.0</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>FEEDS:<span style={{ color: F.CYAN }}>{feedCount}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>ART:<span style={{ color: F.CYAN }}>{newsArticles.length}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>FLT:<span style={{ color: F.ORANGE }}>{activeFilter}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>UPD:<span style={{ color: F.CYAN }}>{newsUpdateCount}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>PRI:<span style={{ color: F.RED }}>{alertCount}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>SNT:<span style={{ color: parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW }}>{score}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>{currentTime.toTimeString().substring(0, 8)}</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: loading ? F.YELLOW : F.GREEN, fontWeight: 700 }}>{loading ? 'UPD' : 'LIVE'}</span>
        </div>
      </div>

      {/* ── CSS ── */}
      <style>{`
        .ticker-wrap{width:100%;overflow:hidden;background:transparent;padding:0}
        .ticker-move{display:inline-block;white-space:nowrap;animation:tickerScroll ${tickerDur}s linear infinite;padding-right:100%}
        .ticker-item{display:inline-block;padding-right:3em;color:${F.WHITE};font-size:9px;white-space:nowrap;font-family:${FONT}}
        @keyframes tickerScroll{0%{transform:translateX(0)}100%{transform:translateX(-50%)}}
        @keyframes blink{0%,50%{opacity:1}51%,100%{opacity:.3}}
        *::-webkit-scrollbar{width:5px;height:5px}
        *::-webkit-scrollbar-track{background:${F.DARK_BG}}
        *::-webkit-scrollbar-thumb{background:${F.BORDER};border-radius:3px}
        *::-webkit-scrollbar-thumb:hover{background:${F.MUTED}}
      `}</style>

      <RSSFeedSettingsModal isOpen={showFeedSettings} onClose={() => setShowFeedSettings(false)} onFeedsUpdated={() => handleRefresh(true)} />
    </div>
  );
};

export default NewsTab;
