/**
 * NewsTab.tsx — FINCEPT NEWS TERMINAL
 * Bloomberg-style multi-quadrant news dashboard
 *
 * Layout: Three-panel with multi-quadrant center
 * ┌──────────┬──────────────────────┬──────────────┐
 * │ LEFT 260 │  CENTER (flex:1)     │  RIGHT 320   │
 * │ Sentiment│  [Breaking Banner]   │  Reader/AI   │
 * │ TopStory │  [Live TV optional]  │              │
 * │ Category │  [Wire Feed]         │              │
 * │ Sources  │  [Map optional]      │              │
 * │ Monitors │                      │              │
 * └──────────┴──────────────────────┴──────────────┘
 */
import React, { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import { useWorkspaceTabState } from '@/hooks/useWorkspaceTabState';
import {
  RefreshCw, Settings, X, Search, Zap, AlertTriangle, Info,
  TrendingUp, TrendingDown, Minus, Clock, Newspaper, Rss,
  Activity, Radio, Globe, ExternalLink, Copy, ChevronDown,
  ChevronRight, Eye, Layers, BookOpen, Hash, ArrowUpRight,
} from 'lucide-react';
import { filterByTimeRange, interleaveBySource } from './NewsGridCard';
import type { TimeRange, ViewMode } from './NewsGridCard';
import {
  fetchAllNews, type NewsArticle, getRSSFeedCount, getActiveSources,
  isUsingMockData, setNewsCacheTTL,
} from '@/services/news/newsService';
import { contextRecorderService } from '@/services/data-sources/contextRecorderService';
import RecordingControlPanel from '@/components/common/RecordingControlPanel';
import { useCurrentTime } from '@/contexts/TimezoneContext';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useAuth } from '@/contexts/AuthContext';
import {
  analyzeNewsArticle, type NewsAnalysisData,
  getSentimentColor, getUrgencyColor, getRiskColor,
} from '@/services/news/newsAnalysisService';
import { createNewsTabTour } from '@/components/tabs/tours/newsTabTour';
import RSSFeedSettingsModal from './RSSFeedSettingsModal';
import {
  clusterArticles, getBreakingClusters, buildNewsMarkers, type NewsCluster,
} from '@/services/news/newsClusterService';
import {
  getMonitors, addMonitor, updateMonitor, deleteMonitor, toggleMonitor,
  scanMonitors, checkForNewBreakingMatches, type NewsMonitor,
} from '@/services/news/newsMonitorService';
import { NewsBreakingBanner } from './NewsBreakingBanner';
import { NewsClusterCard } from './NewsClusterCard';
import { NewsMapPanel } from './NewsMapPanel';
import { NewsMonitorsSection } from './NewsMonitorsSection';
import { LiveNewsPanel } from './LiveNewsPanel';
import { notificationService } from '@/services/notifications';

// ═══════════════════════════════════════════════════════════════════════════════
// DESIGN TOKENS — Bloomberg-inspired dark terminal palette
// ═══════════════════════════════════════════════════════════════════════════════
const C = {
  // Backgrounds — layered depth
  BG:        '#000000',
  SURFACE:   '#080808',
  PANEL:     '#0D0D0D',
  RAISED:    '#141414',
  TOOLBAR:   '#1A1A1A',
  // Borders
  BORDER:    '#1E1E1E',
  DIVIDER:   '#2A2A2A',
  // Text
  TEXT:      '#D4D4D4',
  TEXT_DIM:  '#888888',
  TEXT_MUTE: '#555555',
  TEXT_OFF:  '#333333',
  // Bloomberg-style accent colors
  AMBER:     '#FF8800',   // Primary accent (Fincept orange)
  BLUE:      '#4DA6FF',   // Bloomberg blue — links, info
  CYAN:      '#00D4AA',   // Teal/cyan — data values
  GREEN:     '#26C281',   // Positive
  RED:       '#E55A5A',   // Negative / alert
  YELLOW:    '#E8B634',   // Warning / prices
  PURPLE:    '#9D6EDD',   // AI / tertiary
  WHITE:     '#FFFFFF',
} as const;

const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';
const WIRE_PAGE_SIZE = 50;

declare global { interface Window { __TAURI__?: unknown; } }

// ═══════════════════════════════════════════════════════════════════════════════
// UTILITY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════════
function priColor(p: string) { return ({ FLASH: C.RED, URGENT: C.AMBER, BREAKING: C.YELLOW, ROUTINE: C.TEXT_MUTE })[p] ?? C.TEXT_MUTE; }
function sentColor(s: string) { return ({ BULLISH: C.GREEN, BEARISH: C.RED, NEUTRAL: C.YELLOW })[s] ?? C.TEXT_MUTE; }
function relTime(ts?: number): string {
  if (!ts) return '';
  const d = Math.floor(Date.now() / 1000) - ts;
  if (d < 60) return `${d}s`;
  if (d < 3600) return `${Math.floor(d / 60)}m`;
  if (d < 86400) return `${Math.floor(d / 3600)}h`;
  return `${Math.floor(d / 86400)}d`;
}

// ═══════════════════════════════════════════════════════════════════════════════
// BLOOMBERG-STYLE WIRE ROW — single-line dense article
// ═══════════════════════════════════════════════════════════════════════════════
const WireRow: React.FC<{
  article: NewsArticle;
  selected: boolean;
  onSelect: () => void;
}> = ({ article, selected, onSelect }) => {
  const [hov, setHov] = useState(false);
  const pc = priColor(article.priority);
  const isHot = article.priority === 'FLASH' || article.priority === 'URGENT';

  return (
    <div
      onClick={onSelect}
      onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{
        display: 'flex', alignItems: 'center', gap: '6px',
        padding: '4px 10px', minHeight: '24px',
        backgroundColor: selected ? `${C.BLUE}12` : hov ? `${C.WHITE}06` : C.SURFACE,
        borderLeft: selected ? `2px solid ${C.BLUE}` : isHot ? `2px solid ${pc}` : '2px solid transparent',
        borderBottom: `1px solid ${C.BORDER}`,
        borderRadius: '2px', margin: '1px 4px',
        boxShadow: selected ? `0 0 8px ${C.BLUE}10` : '0 1px 2px rgba(0,0,0,0.2)',
        cursor: 'pointer', fontFamily: FONT,
        transition: 'all 0.1s ease',
      }}
    >
      {/* Time */}
      <span style={{ fontSize: '8px', color: C.TEXT_MUTE, width: '24px', flexShrink: 0, textAlign: 'right' }}>
        {relTime(article.sort_ts) || article.time}
      </span>
      {/* Priority dot */}
      <div style={{
        width: '4px', height: '4px', borderRadius: '50%', flexShrink: 0,
        backgroundColor: pc, boxShadow: isHot ? `0 0 3px ${pc}` : 'none',
      }} />
      {/* Source */}
      <span style={{
        fontSize: '8px', fontWeight: 700, color: C.CYAN, width: '56px', flexShrink: 0,
        overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
      }}>{article.source}</span>
      {/* Headline */}
      <span style={{
        fontSize: '10px', color: selected ? C.WHITE : isHot ? C.TEXT : '#B8B8B8',
        fontWeight: isHot ? 600 : 400, flex: 1,
        overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
      }}>{article.headline}</span>
      {/* Sentiment arrow */}
      <span style={{ color: sentColor(article.sentiment), flexShrink: 0, display: 'flex' }}>
        {article.sentiment === 'BULLISH' ? <TrendingUp size={9} /> :
         article.sentiment === 'BEARISH' ? <TrendingDown size={9} /> :
         <Minus size={8} />}
      </span>
      {/* Tickers */}
      {article.tickers?.slice(0, 2).map(t => (
        <span key={t} style={{ fontSize: '7px', color: C.YELLOW, fontWeight: 700, flexShrink: 0 }}>${t}</span>
      ))}
    </div>
  );
};

// ═══════════════════════════════════════════════════════════════════════════════
// LEFT SIDEBAR TOP STORIES — numbered list (Bloomberg TOP style)
// ═══════════════════════════════════════════════════════════════════════════════
const TopStoryItem: React.FC<{
  article: NewsArticle; rank: number; selected: boolean; onSelect: () => void;
}> = ({ article, rank, selected, onSelect }) => {
  const [hov, setHov] = useState(false);
  return (
    <div
      onClick={onSelect}
      onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
      style={{
        display: 'flex', gap: '6px', padding: '4px 10px',
        backgroundColor: selected ? `${C.BLUE}10` : hov ? `${C.WHITE}04` : 'transparent',
        borderLeft: selected ? `2px solid ${C.AMBER}` : '2px solid transparent',
        cursor: 'pointer', transition: 'background 0.08s',
      }}
    >
      <span style={{
        fontSize: '10px', fontWeight: 700, color: C.AMBER, width: '14px', flexShrink: 0,
        textAlign: 'right',
      }}>{rank}</span>
      <div style={{ minWidth: 0, flex: 1 }}>
        <div style={{
          fontSize: '9px', color: selected ? C.WHITE : '#B0B0B0',
          fontWeight: selected ? 600 : 400,
          lineHeight: '1.3',
          overflow: 'hidden', display: '-webkit-box',
          WebkitLineClamp: 2, WebkitBoxOrient: 'vertical',
        }}>{article.headline}</div>
        <div style={{ display: 'flex', gap: '4px', marginTop: '2px' }}>
          <span style={{ fontSize: '7px', color: C.CYAN, fontWeight: 700 }}>{article.source}</span>
          <span style={{ fontSize: '7px', color: C.TEXT_MUTE }}>{relTime(article.sort_ts)}</span>
        </div>
      </div>
    </div>
  );
};

// ═══════════════════════════════════════════════════════════════════════════════
// PANEL HEADER (reusable)
// ═══════════════════════════════════════════════════════════════════════════════
const PH: React.FC<{ label: string; color?: string; right?: React.ReactNode }> = ({ label, color = C.TEXT_MUTE, right }) => (
  <div style={{
    padding: '4px 10px', backgroundColor: C.TOOLBAR,
    borderBottom: `1px solid ${C.BORDER}`,
    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
    flexShrink: 0, fontFamily: FONT,
  }}>
    <span style={{ fontSize: '8px', fontWeight: 700, color, letterSpacing: '1px' }}>{label}</span>
    {right}
  </div>
);

// ═══════════════════════════════════════════════════════════════════════════════
// STAT ROW
// ═══════════════════════════════════════════════════════════════════════════════
const SR: React.FC<{ k: string; v: string | number; c?: string }> = ({ k, v, c = C.CYAN }) => (
  <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '9px', padding: '1px 0', fontFamily: FONT }}>
    <span style={{ color: C.TEXT_MUTE }}>{k}</span>
    <span style={{ color: c, fontWeight: 600 }}>{v}</span>
  </div>
);

// ═══════════════════════════════════════════════════════════════════════════════
// TOOLBAR BUTTON
// ═══════════════════════════════════════════════════════════════════════════════
function TB(active: boolean, color: string = C.AMBER): React.CSSProperties {
  return {
    padding: '3px 8px', fontSize: '8px', fontWeight: 700,
    backgroundColor: active ? `${color}15` : 'transparent',
    border: `1px solid ${active ? color : C.BORDER}`,
    color: active ? color : C.TEXT_MUTE,
    cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
    display: 'flex', alignItems: 'center', gap: '3px',
    letterSpacing: '0.3px',
  };
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN COMPONENT
// ═══════════════════════════════════════════════════════════════════════════════
const NewsTab: React.FC = () => {
  const { t } = useTranslation('news');
  const { fontFamily } = useTerminalTheme();
  const { formattedTime, timezone } = useCurrentTime();
  const { session } = useAuth();

  // ── State ──
  const [newsArticles, setNewsArticles] = useState<NewsArticle[]>([]);
  const [loading, setLoading] = useState(true);
  const [activeSources, setActiveSources] = useState<string[]>([]);
  const [feedCount, setFeedCount] = useState(0);
  const [refreshInterval, setRefreshInterval] = useState(10);
  const [newsUpdateCount, setNewsUpdateCount] = useState(0);
  const [alertCount, setAlertCount] = useState(0);
  const [currentTime, setCurrentTime] = useState(new Date());

  const [activeFilter, setActiveFilter] = useState('ALL');
  const [searchQuery, setSearchQuery] = useState('');
  const [timeRange, setTimeRange] = useState<TimeRange>('24H');
  const [viewMode, setViewMode] = useState<ViewMode>('flat');
  const [groupBySource, setGroupBySource] = useState(false);

  const [wirePage, setWirePage] = useState(1);
  const [showIntervalSettings, setShowIntervalSettings] = useState(false);
  const [showFeedSettings, setShowFeedSettings] = useState(false);
  const [isRecording, setIsRecording] = useState(false);
  const [mapCollapsed, setMapCollapsed] = useState(true);
  const [liveCollapsed, setLiveCollapsed] = useState(true);
  const [showSearch, setShowSearch] = useState(false);

  const [analysisData, setAnalysisData] = useState<NewsAnalysisData | null>(null);
  const [analyzingArticle, setAnalyzingArticle] = useState(false);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const [currentAnalyzedArticle, setCurrentAnalyzedArticle] = useState<NewsArticle | null>(null);
  const [currentAnalyzingId, setCurrentAnalyzingId] = useState<string | null>(null);
  const [selectedArticle, setSelectedArticle] = useState<NewsArticle | null>(null);

  const [clusters, setClusters] = useState<NewsCluster[]>([]);
  const [dismissedBannerIds, setDismissedBannerIds] = useState<Set<string>>(new Set());
  const [highlightedClusterId, setHighlightedClusterId] = useState<string | null>(null);

  const [monitors, setMonitors] = useState<NewsMonitor[]>([]);
  const [monitorMatchCounts, setMonitorMatchCounts] = useState<Map<string, number>>(new Map());
  const [newMatchIds, setNewMatchIds] = useState<Set<string>>(new Set());
  const [activeMonitorId, setActiveMonitorId] = useState<string | null>(null);
  const prevArticleIds = useRef<Set<string>>(new Set());

  // ── Workspace ──
  const getWorkspaceState = useCallback(() => ({
    activeFilter, feedCount, refreshInterval, searchQuery, timeRange,
    viewMode, mapCollapsed, activeMonitorId, groupBySource,
  }), [activeFilter, feedCount, refreshInterval, searchQuery, timeRange, viewMode, mapCollapsed, activeMonitorId, groupBySource]);
  const setWorkspaceState = useCallback((s: Record<string, unknown>) => {
    if (typeof s.activeFilter === 'string') setActiveFilter(s.activeFilter);
    if (typeof s.feedCount === 'number') setFeedCount(s.feedCount);
    if (typeof s.refreshInterval === 'number') setRefreshInterval(s.refreshInterval);
    if (typeof s.searchQuery === 'string') setSearchQuery(s.searchQuery);
    if (typeof s.timeRange === 'string') setTimeRange(s.timeRange as TimeRange);
    if (typeof s.viewMode === 'string') setViewMode(s.viewMode as ViewMode);
    if (typeof s.mapCollapsed === 'boolean') setMapCollapsed(s.mapCollapsed);
    if (typeof s.groupBySource === 'boolean') setGroupBySource(s.groupBySource);
    if (s.activeMonitorId === null || typeof s.activeMonitorId === 'string')
      setActiveMonitorId(s.activeMonitorId as string | null);
  }, []);
  useWorkspaceTabState('news', getWorkspaceState, setWorkspaceState);

  useEffect(() => { getMonitors().then(setMonitors).catch(() => {}); }, []);
  useEffect(() => { setNewsCacheTTL(refreshInterval); }, [refreshInterval]);
  useEffect(() => {
    const iv = setInterval(() => setCurrentTime(new Date()), 1000);
    return () => clearInterval(iv);
  }, []);

  // ── Refresh ──
  const handleRefresh = useCallback(async (force = true) => {
    setLoading(true);
    try {
      const [articles, sources, count] = await Promise.all([fetchAllNews(force), getActiveSources(), getRSSFeedCount()]);
      setNewsArticles(articles);
      setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
      setActiveSources(sources); setFeedCount(count);
      setNewsUpdateCount(p => p + 1);
      const nc = clusterArticles(articles); setClusters(nc);
      const allMon = await getMonitors().catch(() => monitors); setMonitors(allMon);
      const scanR = scanMonitors(allMon, articles);
      const counts = new Map<string, number>(); const newIds = new Set<string>();
      scanR.forEach((matched, mid) => {
        counts.set(mid, matched.length);
        if (matched.some(a => !prevArticleIds.current.has(a.id))) newIds.add(mid);
      });
      setMonitorMatchCounts(counts); setNewMatchIds(newIds);
      checkForNewBreakingMatches(allMon, articles, prevArticleIds.current).forEach(({ monitor, article }) => {
        notificationService.notify({
          type: 'warning', title: `Monitor: ${monitor.label}`,
          body: `"${article.headline}" — ${article.source}`,
          timestamp: new Date().toISOString(),
          metadata: { monitor: monitor.label, source: article.source, priority: article.priority },
        });
      });
      prevArticleIds.current = new Set(articles.map(a => a.id));
      if (isRecording && articles.length > 0) {
        try { await contextRecorderService.recordApiResponse('News', 'news-feed', { articles, totalCount: articles.length, alertCount, sources, timestamp: new Date().toISOString(), filter: activeFilter }, `News Feed - ${new Date().toLocaleString()}`, ['news', 'rss', activeFilter.toLowerCase()]); } catch {}
      }
    } catch {} finally { setLoading(false); }
  }, [isRecording, activeFilter, monitors, alertCount]);

  const isInitialMount = useRef(true);
  useEffect(() => {
    const load = async (force = false) => {
      setLoading(true);
      try {
        const [articles, sources, count] = await Promise.all([fetchAllNews(force), getActiveSources(), getRSSFeedCount()]);
        setNewsArticles(articles);
        setAlertCount(articles.filter(a => a.priority === 'FLASH' || a.priority === 'URGENT').length);
        setActiveSources(sources); setFeedCount(count);
        setNewsUpdateCount(p => p + 1);
        setClusters(clusterArticles(articles));
        prevArticleIds.current = new Set(articles.map(a => a.id));
      } catch {} finally { setLoading(false); }
    };
    if (isInitialMount.current) { isInitialMount.current = false; load(false); } else load(true);
    const iv = setInterval(() => load(true), refreshInterval * 60 * 1000);
    return () => clearInterval(iv);
  }, [refreshInterval]);

  // ── Analyze ──
  const handleAnalyzeArticle = async (article: NewsArticle) => {
    if (!article?.link) return;
    setAnalyzingArticle(true); setShowAnalysis(false); setAnalysisData(null);
    setCurrentAnalyzingId(article.id); setCurrentAnalyzedArticle(article);
    try {
      const result = await analyzeNewsArticle(article.link, session?.api_key ?? '');
      if (result.success && 'data' in result) { setAnalysisData(result.data); setShowAnalysis(true); }
    } catch {} finally { setAnalyzingArticle(false); }
  };

  const openExternal = async (url: string) => {
    try {
      if (window.__TAURI__) { const { openUrl } = await import('@tauri-apps/plugin-opener'); await openUrl(url); }
      else window.open(url, '_blank', 'noopener');
    } catch { await navigator.clipboard.writeText(url); }
  };

  const scrollToCluster = (clusterId: string) => {
    setHighlightedClusterId(clusterId);
    document.getElementById(`cluster-${clusterId}`)?.scrollIntoView({ behavior: 'smooth', block: 'start' });
    setTimeout(() => setHighlightedClusterId(null), 3000);
  };

  // ── Monitor handlers ──
  const handleAddMonitor = async (l: string, kw: string[]) => { const m = await addMonitor(l, kw).catch(() => null); if (m) setMonitors(p => [...p, m]); };
  const handleToggleMonitor = async (id: string, en: boolean) => { await toggleMonitor(id, en).catch(() => {}); setMonitors(p => p.map(m => m.id === id ? { ...m, enabled: en } : m)); };
  const handleDeleteMonitor = async (id: string) => { await deleteMonitor(id).catch(() => {}); setMonitors(p => p.filter(m => m.id !== id)); if (activeMonitorId === id) setActiveMonitorId(null); };
  const handleUpdateMonitor = async (id: string, l: string, kw: string[], clr: string, en: boolean) => { await updateMonitor(id, l, kw, clr, en).catch(() => {}); setMonitors(p => p.map(m => m.id === id ? { ...m, label: l, keywords: kw, color: clr, enabled: en } : m)); };

  // ── Derived data ──
  const timeFiltered = filterByTimeRange(newsArticles, timeRange);
  const filteredArticles = useMemo(() => {
    let arts = activeFilter === 'ALL' ? timeFiltered : timeFiltered.filter(a => a.category === activeFilter);
    if (activeMonitorId) {
      const ids = new Set((scanMonitors(monitors.filter(m => m.id === activeMonitorId), arts).get(activeMonitorId) ?? []).map(a => a.id));
      arts = arts.filter(a => ids.has(a.id));
    }
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      arts = arts.filter(a => a.headline.toLowerCase().includes(q) || a.summary?.toLowerCase().includes(q) || a.source.toLowerCase().includes(q) || a.tickers?.some(t => t.toLowerCase().includes(q)));
    }
    return arts;
  }, [timeFiltered, activeFilter, activeMonitorId, monitors, searchQuery]);

  // Reset pagination when filters change
  useEffect(() => { setWirePage(1); }, [activeFilter, searchQuery, timeRange, viewMode, groupBySource, activeMonitorId]);

  const filteredClusters = useMemo(() => {
    const ids = new Set(filteredArticles.map(a => a.id));
    let cls = clusters.filter(c => c.articles.some(a => ids.has(a.id)));
    if (activeFilter !== 'ALL') cls = cls.filter(c => c.category === activeFilter);
    if (searchQuery.trim()) {
      const q = searchQuery.toLowerCase();
      cls = cls.filter(c => c.leadArticle.headline.toLowerCase().includes(q) || c.articles.some(a => a.source.toLowerCase().includes(q)));
    }
    return cls;
  }, [filteredArticles, clusters, activeFilter, searchQuery]);

  const breakingClusters = getBreakingClusters(filteredClusters).filter(c => !dismissedBannerIds.has(c.id));
  const topBreaking = breakingClusters[0] ?? null;
  const newsMarkers = buildNewsMarkers(filteredClusters);

  const bullish = newsArticles.filter(a => a.sentiment === 'BULLISH').length;
  const bearish = newsArticles.filter(a => a.sentiment === 'BEARISH').length;
  const neutral = newsArticles.filter(a => a.sentiment === 'NEUTRAL').length;
  const total = newsArticles.length || 1;
  const score = ((bullish - bearish) / total).toFixed(2);
  const scoreNum = parseFloat(score);

  const catCounts: Record<string, number> = {};
  newsArticles.forEach(a => { catCounts[a.category] = (catCounts[a.category] || 0) + 1; });
  const topCategories = Object.entries(catCounts).sort((a, b) => b[1] - a[1]).slice(0, 12);

  // Top stories — highest priority articles
  const topStories = useMemo(() => {
    const priOrd: Record<string, number> = { FLASH: 0, URGENT: 1, BREAKING: 2, ROUTINE: 3 };
    return [...filteredArticles].sort((a, b) => (priOrd[a.priority] ?? 9) - (priOrd[b.priority] ?? 9) || (b.sort_ts || 0) - (a.sort_ts || 0)).slice(0, 10);
  }, [filteredArticles]);

  // Group by source for BY SOURCE mode
  const articlesBySource = useMemo(() => {
    if (!groupBySource) return {};
    const m: Record<string, NewsArticle[]> = {};
    filteredArticles.forEach(a => { if (!m[a.source]) m[a.source] = []; m[a.source].push(a); });
    return m;
  }, [filteredArticles, groupBySource]);
  const sourceOrder = Object.keys(articlesBySource).sort((a, b) => articlesBySource[b].length - articlesBySource[a].length);

  const FKEYS = [
    { k: 'F1', l: 'ALL', f: 'ALL' }, { k: 'F2', l: 'MKT', f: 'MARKETS' },
    { k: 'F3', l: 'EARN', f: 'EARNINGS' }, { k: 'F4', l: 'ECO', f: 'ECONOMIC' },
    { k: 'F5', l: 'TECH', f: 'TECH' }, { k: 'F6', l: 'NRG', f: 'ENERGY' },
    { k: 'F7', l: 'CRPT', f: 'CRYPTO' }, { k: 'F8', l: 'GEO', f: 'GEOPOLITICS' },
    { k: 'F9', l: 'DEF', f: 'DEFENSE' },
  ];
  const TR: TimeRange[] = ['1H', '6H', '24H', '48H', '7D'];

  const CSS = `
    @keyframes ntP{0%,50%{opacity:1}51%,100%{opacity:.3}}
    @keyframes ntS{from{transform:rotate(0)}to{transform:rotate(360deg)}}
    .nt-s::-webkit-scrollbar{width:4px;height:4px}
    .nt-s::-webkit-scrollbar-track{background:${C.BG}}
    .nt-s::-webkit-scrollbar-thumb{background:${C.DIVIDER};border-radius:2px}
    .nt-s::-webkit-scrollbar-thumb:hover{background:${C.TEXT_MUTE}}
  `;

  // ═════════════════════════════════════════════════════════════════════════════
  // RENDER
  // ═════════════════════════════════════════════════════════════════════════════
  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: C.BG, color: C.TEXT, fontFamily: FONT, fontSize: '11px', overflow: 'hidden' }}>
      <style dangerouslySetInnerHTML={{ __html: CSS }} />

      {/* ═══ TOP BAR — matches dashboard tab strip style ═══ */}
      <div style={{
        backgroundColor: '#2a2a2a',
        borderBottom: '1px solid #333',
        padding: '0 4px', height: '26px',
        display: 'flex', alignItems: 'center', gap: '1px',
        flexShrink: 0,
      }}>
        {/* Brand label */}
        <div style={{
          padding: '3px 8px',
          backgroundColor: '#ea580c', color: '#fff',
          fontSize: '10px', fontWeight: 700, fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '4px',
          whiteSpace: 'nowrap',
        }}>
          <Activity size={10} />NEWS
        </div>

        {/* F-key filters */}
        {FKEYS.map(item => (
          <button key={item.k}
            onClick={() => setActiveFilter(activeFilter === item.f && item.f !== 'ALL' ? 'ALL' : item.f)}
            style={{
              padding: '3px 8px',
              backgroundColor: activeFilter === item.f ? '#ea580c' : 'transparent',
              color: activeFilter === item.f ? '#fff' : '#d4d4d4',
              border: 'none',
              fontSize: '10px', fontWeight: activeFilter === item.f ? 700 : 400,
              cursor: 'pointer', fontFamily: FONT,
              whiteSpace: 'nowrap',
            }}>
            <span style={{ opacity: 0.4, marginRight: '1px', fontSize: '8px' }}>{item.k}</span>{item.l}
          </button>
        ))}

        <div style={{ width: '1px', height: '14px', background: '#555', margin: '0 3px' }} />

        {/* Time ranges */}
        {TR.map(tr => (
          <button key={tr} onClick={() => setTimeRange(tr)} style={{
            padding: '3px 6px',
            backgroundColor: timeRange === tr ? '#ea580c' : 'transparent',
            color: timeRange === tr ? '#fff' : '#d4d4d4',
            border: 'none',
            fontSize: '10px', fontWeight: timeRange === tr ? 700 : 400,
            cursor: 'pointer', fontFamily: FONT,
            whiteSpace: 'nowrap',
          }}>{tr}</button>
        ))}

        <div style={{ width: '1px', height: '14px', background: '#555', margin: '0 3px' }} />

        {/* View toggles */}
        <button onClick={() => setViewMode(viewMode === 'flat' ? 'clustered' : 'flat')} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: viewMode === 'clustered' ? '#ea580c' : 'transparent',
          color: viewMode === 'clustered' ? '#fff' : '#d4d4d4',
          border: 'none', whiteSpace: 'nowrap',
        }}>
          <Layers size={9} />{viewMode === 'flat' ? 'WIRE' : 'CLU'}
        </button>
        {viewMode === 'flat' && (
          <button onClick={() => setGroupBySource(g => !g)} style={{
            padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
            display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
            backgroundColor: groupBySource ? '#ea580c' : 'transparent',
            color: groupBySource ? '#fff' : '#d4d4d4',
            border: 'none', whiteSpace: 'nowrap',
          }}>
            <Hash size={9} />SRC
          </button>
        )}
        <button onClick={() => setLiveCollapsed(c => !c)} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: !liveCollapsed ? '#ea580c' : 'transparent',
          color: !liveCollapsed ? '#fff' : '#d4d4d4',
          border: 'none', whiteSpace: 'nowrap',
        }}>
          <Radio size={9} />TV
        </button>
        <button onClick={() => setMapCollapsed(c => !c)} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: !mapCollapsed ? '#ea580c' : 'transparent',
          color: !mapCollapsed ? '#fff' : '#d4d4d4',
          border: 'none', whiteSpace: 'nowrap',
        }}>
          <Globe size={9} />MAP
        </button>

        {/* Spacer → right controls */}
        <div style={{ flex: 1 }} />

        {/* Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginRight: '4px' }}>
          <div style={{ width: '6px', height: '6px', borderRadius: '50%', background: C.RED, animation: 'ntP 1.2s infinite' }} />
          {alertCount > 0 && <span style={{ fontSize: '10px', color: C.RED, fontWeight: 700 }}>{alertCount}</span>}
          <span style={{ fontSize: '10px', color: '#d4d4d4' }}>
            {filteredArticles.length}<span style={{ color: '#777' }}>/</span>{newsArticles.length}
          </span>
        </div>

        {/* Inline search field */}
        <div style={{
          display: 'flex', alignItems: 'center', gap: '4px',
          backgroundColor: '#1a1a1a', border: '1px solid #444',
          borderRadius: '2px', padding: '1px 6px', height: '18px',
        }}>
          <Search size={9} style={{ color: '#777', flexShrink: 0 }} />
          <input type="text" value={searchQuery} onChange={e => setSearchQuery(e.target.value)} placeholder="Search..."
            style={{ width: '120px', backgroundColor: 'transparent', border: 'none', outline: 'none', color: '#d4d4d4', fontSize: '10px', fontFamily: FONT, padding: 0 }} />
          {searchQuery && <button onClick={() => setSearchQuery('')} style={{ background: 'none', border: 'none', color: '#777', cursor: 'pointer', padding: 0, display: 'flex', flexShrink: 0 }}><X size={9} /></button>}
        </div>
        <button onClick={() => setShowIntervalSettings(s => !s)} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: 'transparent', color: '#d4d4d4', border: 'none',
        }}><Clock size={9} />{refreshInterval}M</button>
        <button onClick={() => setShowFeedSettings(true)} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: 'transparent', color: '#d4d4d4', border: 'none',
        }}><Settings size={9} /></button>
        <button onClick={() => handleRefresh(true)} disabled={loading} style={{
          padding: '3px 8px', fontSize: '10px', fontWeight: 700,
          backgroundColor: '#ea580c', color: '#fff',
          border: 'none', cursor: loading ? 'not-allowed' : 'pointer',
          fontFamily: FONT, display: 'flex', alignItems: 'center', gap: '3px',
        }}>
          <RefreshCw size={9} style={{ animation: loading ? 'ntS 1s linear infinite' : 'none' }} />
          {loading ? 'SYNC' : 'GO'}
        </button>
        <button onClick={() => createNewsTabTour().drive()} style={{
          padding: '3px 8px', fontSize: '10px', fontFamily: FONT,
          display: 'flex', alignItems: 'center', gap: '3px', cursor: 'pointer',
          backgroundColor: 'transparent', color: '#d4d4d4', border: 'none',
        }}><Info size={9} /></button>
        <RecordingControlPanel tabName="News" onRecordingChange={setIsRecording} onRecordingStart={async () => {
          if (newsArticles.length > 0) {
            const sources = await getActiveSources();
            await contextRecorderService.recordApiResponse('News', 'news-feed', { articles: newsArticles, totalCount: newsArticles.length, alertCount, sources, timestamp: new Date().toISOString(), filter: activeFilter }, `News Feed (Snapshot)`, ['news', 'rss', 'snapshot']);
          }
        }} />


        {/* Interval dropdown */}
        {showIntervalSettings && (
          <div style={{ position: 'absolute', top: '26px', right: 130, zIndex: 2000, backgroundColor: C.PANEL, border: `1px solid ${C.AMBER}`, borderRadius: '2px', padding: '4px', minWidth: '80px' }}>
            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '2px', letterSpacing: '0.5px' }}>INTERVAL</div>
            {[1, 2, 5, 10, 15, 30].map(min => (
              <button key={min} onClick={() => { setRefreshInterval(min); setShowIntervalSettings(false); }}
                style={{
                  display: 'block', width: '100%', textAlign: 'left',
                  padding: '3px 6px', borderRadius: '1px',
                  backgroundColor: refreshInterval === min ? C.AMBER : 'transparent',
                  color: refreshInterval === min ? C.BG : C.TEXT,
                  border: 'none', fontSize: '9px', fontWeight: 600, cursor: 'pointer', fontFamily: FONT,
                }}>{min} MIN</button>
            ))}
          </div>
        )}
      </div>

      {/* ═══ BREAKING BANNER ═══ */}
      {topBreaking && (
        <NewsBreakingBanner
          cluster={topBreaking}
          onDismiss={id => setDismissedBannerIds(prev => new Set([...prev, id]))}
          onScrollTo={scrollToCluster} colors={{}}
        />
      )}

      {/* ═══ THREE-PANEL BODY ═══ */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex' }}>

        {/* ─── LEFT SIDEBAR ─── 260px intelligence panel */}
        <div style={{
          width: '260px', flexShrink: 0, backgroundColor: C.PANEL,
          borderRight: `1px solid ${C.BORDER}`,
          display: 'flex', flexDirection: 'column', overflow: 'hidden',
        }}>
          {/* Sentiment gauge */}
          <PH label="MARKET SENTIMENT" color={C.TEXT_MUTE} right={
            <span style={{ fontSize: '13px', fontWeight: 700, fontFamily: FONT, color: scoreNum > 0 ? C.GREEN : scoreNum < 0 ? C.RED : C.YELLOW }}>{score}</span>
          } />
          <div style={{ padding: '6px 10px', borderBottom: `1px solid ${C.BORDER}` }}>
            {/* Bar chart */}
            <div style={{ display: 'flex', height: '6px', borderRadius: '1px', overflow: 'hidden', gap: '1px', marginBottom: '5px' }}>
              <div style={{ width: `${Math.round((bullish / total) * 100)}%`, backgroundColor: C.GREEN, borderRadius: '1px' }} />
              <div style={{ width: `${Math.round((neutral / total) * 100)}%`, backgroundColor: C.YELLOW, borderRadius: '1px' }} />
              <div style={{ width: `${Math.round((bearish / total) * 100)}%`, backgroundColor: C.RED, borderRadius: '1px' }} />
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '8px' }}>
              <span style={{ color: C.GREEN, fontWeight: 700 }}>BUL {bullish}</span>
              <span style={{ color: C.YELLOW }}>NEU {neutral}</span>
              <span style={{ color: C.RED, fontWeight: 700 }}>BEA {bearish}</span>
            </div>
          </div>

          {/* Top stories */}
          <PH label="TOP STORIES" color={C.AMBER} />
          <div className="nt-s" style={{ maxHeight: '200px', overflow: 'auto', borderBottom: `1px solid ${C.BORDER}` }}>
            {topStories.map((a, i) => (
              <TopStoryItem key={a.id} article={a} rank={i + 1}
                selected={selectedArticle?.id === a.id}
                onSelect={() => setSelectedArticle(a)} />
            ))}
          </div>

          {/* Categories */}
          <PH label="CATEGORIES" />
          <div className="nt-s" style={{ maxHeight: '140px', overflow: 'auto', borderBottom: `1px solid ${C.BORDER}` }}>
            {topCategories.map(([cat, count]) => (
              <div key={cat}
                onClick={() => setActiveFilter(activeFilter === cat ? 'ALL' : cat)}
                style={{
                  display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                  padding: '2px 10px', cursor: 'pointer',
                  backgroundColor: activeFilter === cat ? `${C.AMBER}10` : 'transparent',
                  borderLeft: activeFilter === cat ? `2px solid ${C.AMBER}` : '2px solid transparent',
                }}>
                <span style={{ fontSize: '8px', fontWeight: activeFilter === cat ? 700 : 400, color: activeFilter === cat ? C.AMBER : C.TEXT_MUTE }}>{cat}</span>
                <span style={{ fontSize: '8px', color: C.CYAN, fontWeight: 600 }}>{count}</span>
              </div>
            ))}
          </div>

          {/* Stats */}
          <PH label="FEED STATS" />
          <div style={{ padding: '4px 10px', borderBottom: `1px solid ${C.BORDER}` }}>
            <SR k="FEEDS" v={feedCount} />
            <SR k="ARTICLES" v={newsArticles.length} />
            <SR k="CLUSTERS" v={clusters.length} />
            <SR k="ALERTS" v={alertCount} c={alertCount > 0 ? C.RED : C.TEXT_MUTE} />
            <SR k="SOURCES" v={activeSources.length} />
          </div>

          {/* Monitors — fills remaining */}
          <div style={{ flex: 1, overflow: 'auto' }} className="nt-s">
            <NewsMonitorsSection
              monitors={monitors} matchCounts={monitorMatchCounts}
              newMatchIds={newMatchIds} activeMonitorId={activeMonitorId}
              onActivate={setActiveMonitorId} onAdd={handleAddMonitor}
              onToggle={handleToggleMonitor} onDelete={handleDeleteMonitor}
              onUpdate={handleUpdateMonitor} colors={{}} />
          </div>
        </div>

        {/* ─── CENTER CONTENT ─── */}
        <div style={{
          flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column', backgroundColor: C.BG,
          backgroundImage: `linear-gradient(${C.WHITE}03 1px, transparent 1px), linear-gradient(90deg, ${C.WHITE}03 1px, transparent 1px)`,
          backgroundSize: '20px 20px',
        }}>
          {/* Live TV (optional) */}
          {!liveCollapsed && <LiveNewsPanel colors={{}} />}
          {/* Map (optional) */}
          {!mapCollapsed && <NewsMapPanel markers={newsMarkers} onMarkerClick={scrollToCluster} colors={{}} />}

          {/* Wire feed header */}
          <PH label={viewMode === 'clustered' ? `CLUSTERS (${filteredClusters.length})` : groupBySource ? `BY SOURCE (${filteredArticles.length})` : `WIRE (${filteredArticles.length})`} color={C.TEXT_MUTE} right={
            <div style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
              {searchQuery && <span style={{ fontSize: '7px', color: C.AMBER, backgroundColor: `${C.AMBER}15`, padding: '1px 5px', borderRadius: '2px' }}>"{searchQuery}"</span>}
              {activeFilter !== 'ALL' && <span style={{ fontSize: '7px', color: C.AMBER, backgroundColor: `${C.AMBER}15`, padding: '1px 5px', borderRadius: '2px' }}>{activeFilter}</span>}
              {activeMonitorId && (
                <span style={{ fontSize: '7px', color: monitors.find(m => m.id === activeMonitorId)?.color ?? C.AMBER, backgroundColor: `${monitors.find(m => m.id === activeMonitorId)?.color ?? C.AMBER}15`, padding: '1px 5px', borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '2px' }}>
                  MON: {monitors.find(m => m.id === activeMonitorId)?.label}
                  <button onClick={() => setActiveMonitorId(null)} style={{ background: 'none', border: 'none', color: 'inherit', cursor: 'pointer', padding: 0, display: 'flex' }}><X size={7} /></button>
                </span>
              )}
              <span style={{ fontSize: '8px', color: loading ? C.YELLOW : isUsingMockData() ? C.PURPLE : C.GREEN, fontWeight: 700 }}>
                {loading ? '● SYNC' : isUsingMockData() ? '● DEMO' : '● LIVE'}
              </span>
            </div>
          } />

          {/* News content */}
          <div style={{ flex: 1, overflow: 'auto' }} className="nt-s" id="news-primary-feed">
            {loading && newsArticles.length === 0 ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '8px' }}>
                <RefreshCw size={16} style={{ color: C.TEXT_MUTE, opacity: 0.4, animation: 'ntS 1s linear infinite' }} />
                <span style={{ fontSize: '9px', color: C.TEXT_MUTE }}>LOADING {feedCount} FEEDS...</span>
              </div>
            ) : viewMode === 'clustered' ? (
              filteredClusters.length === 0
                ? <EmptyState msg="NO CLUSTERS MATCH FILTERS" />
                : <>
                    {filteredClusters.slice(0, wirePage * WIRE_PAGE_SIZE).map(c => <NewsClusterCard key={c.id} cluster={c} isHighlighted={highlightedClusterId === c.id} onAnalyze={handleAnalyzeArticle} colors={{}} />)}
                    {filteredClusters.length > wirePage * WIRE_PAGE_SIZE && (
                      <button onClick={() => setWirePage(p => p + 1)} style={{
                        display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                        width: '100%', padding: '8px', margin: '4px 0',
                        background: C.TOOLBAR, border: `1px solid ${C.BORDER}`, borderRadius: '3px',
                        color: C.AMBER, fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: FONT,
                        letterSpacing: '0.5px',
                      }}>
                        LOAD MORE ({filteredClusters.length - wirePage * WIRE_PAGE_SIZE} remaining)
                      </button>
                    )}
                  </>
            ) : filteredArticles.length === 0
              ? <EmptyState msg="NO ARTICLES MATCH FILTERS" />
              : groupBySource ? (
                <>{sourceOrder.map(src => {
                  const srcArts = articlesBySource[src];
                  const srcVisible = srcArts.slice(0, wirePage * WIRE_PAGE_SIZE);
                  return (
                    <div key={src}>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '2px 8px', backgroundColor: C.TOOLBAR, borderBottom: `1px solid ${C.BORDER}`, position: 'sticky', top: 0, zIndex: 1, fontFamily: FONT }}>
                        <div style={{ width: '3px', height: '10px', backgroundColor: C.AMBER, borderRadius: '1px' }} />
                        <span style={{ fontSize: '8px', fontWeight: 700, color: C.AMBER, letterSpacing: '0.5px' }}>{src}</span>
                        <span style={{ fontSize: '7px', color: C.TEXT_MUTE }}>{srcArts.length}</span>
                      </div>
                      {srcVisible.map(a => <WireRow key={a.id} article={a} selected={selectedArticle?.id === a.id} onSelect={() => setSelectedArticle(a)} />)}
                    </div>
                  );
                })}</>
              ) : (
                <>
                  {interleaveBySource(filteredArticles).slice(0, wirePage * WIRE_PAGE_SIZE).map(a => <WireRow key={a.id} article={a} selected={selectedArticle?.id === a.id} onSelect={() => setSelectedArticle(a)} />)}
                  {filteredArticles.length > wirePage * WIRE_PAGE_SIZE && (
                    <button onClick={() => setWirePage(p => p + 1)} style={{
                      display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                      width: '100%', padding: '8px', margin: '4px 0',
                      background: C.TOOLBAR, border: `1px solid ${C.BORDER}`, borderRadius: '3px',
                      color: C.AMBER, fontSize: '9px', fontWeight: 700, cursor: 'pointer', fontFamily: FONT,
                      letterSpacing: '0.5px',
                    }}>
                      LOAD MORE ({filteredArticles.length - wirePage * WIRE_PAGE_SIZE} remaining)
                    </button>
                  )}
                </>
              )
            }
          </div>
        </div>

        {/* ─── RIGHT PANEL — Reader + AI Analysis ─── 320px */}
        <div style={{
          width: '320px', flexShrink: 0, backgroundColor: C.PANEL,
          borderLeft: `1px solid ${C.BORDER}`,
          display: 'flex', flexDirection: 'column', overflow: 'hidden',
        }}>
          {/* Reader/AI toggle header */}
          <PH label={selectedArticle ? 'ARTICLE DETAIL' : 'READER'} color={C.BLUE} right={
            <div style={{ display: 'flex', gap: '4px' }}>
              {selectedArticle?.link && <button onClick={() => openExternal(selectedArticle.link!)} style={{
                padding: '3px 8px', fontSize: '8px', fontWeight: 700,
                backgroundColor: `${C.BLUE}20`, border: `1px solid ${C.BLUE}60`,
                color: C.BLUE, cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px', letterSpacing: '0.3px',
              }}><ExternalLink size={8} />OPEN</button>}
              {selectedArticle?.link && <button onClick={() => navigator.clipboard.writeText(selectedArticle.link!)} style={{
                padding: '3px 8px', fontSize: '8px', fontWeight: 700,
                backgroundColor: C.RAISED, border: `1px solid ${C.DIVIDER}`,
                color: C.TEXT_DIM, cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px',
              }}><Copy size={8} /></button>}
              {selectedArticle && <button onClick={() => handleAnalyzeArticle(selectedArticle)} disabled={analyzingArticle} style={{
                padding: '3px 8px', fontSize: '8px', fontWeight: 700,
                backgroundColor: `${C.PURPLE}20`, border: `1px solid ${C.PURPLE}60`,
                color: C.PURPLE, cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px', letterSpacing: '0.3px',
                opacity: analyzingArticle ? 0.5 : 1,
              }}><Zap size={8} />{analyzingArticle && currentAnalyzingId === selectedArticle.id ? 'WAIT' : 'AI'}</button>}
            </div>
          } />

          <div className="nt-s" style={{ flex: 1, overflow: 'auto' }}>
            {selectedArticle ? (
              <div style={{ padding: '10px' }}>
                {/* Article header */}
                <div style={{ marginBottom: '10px' }}>
                  <div style={{ fontSize: '12px', color: C.WHITE, fontWeight: 600, lineHeight: 1.4, marginBottom: '6px' }}>
                    {selectedArticle.headline}
                  </div>
                  <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap', marginBottom: '6px' }}>
                    <span style={{ fontSize: '8px', color: C.CYAN, fontWeight: 700 }}>{selectedArticle.source}</span>
                    <span style={{ fontSize: '8px', color: C.TEXT_MUTE }}>{relTime(selectedArticle.sort_ts) || selectedArticle.time}</span>
                    <span style={{ padding: '1px 5px', fontSize: '7px', fontWeight: 700, borderRadius: '2px', backgroundColor: `${priColor(selectedArticle.priority)}18`, color: priColor(selectedArticle.priority), border: `1px solid ${priColor(selectedArticle.priority)}35` }}>{selectedArticle.priority}</span>
                    <span style={{ padding: '1px 5px', fontSize: '7px', fontWeight: 700, borderRadius: '2px', backgroundColor: `${sentColor(selectedArticle.sentiment)}18`, color: sentColor(selectedArticle.sentiment), border: `1px solid ${sentColor(selectedArticle.sentiment)}35` }}>{selectedArticle.sentiment}</span>
                    <span style={{ fontSize: '7px', color: C.TEXT_MUTE, backgroundColor: `${C.TEXT_MUTE}12`, padding: '1px 4px', borderRadius: '1px' }}>{selectedArticle.category}</span>
                  </div>
                  {selectedArticle.tickers && selectedArticle.tickers.length > 0 && (
                    <div style={{ display: 'flex', gap: '3px', marginBottom: '6px' }}>
                      {selectedArticle.tickers.map(t => (
                        <span key={t} style={{ fontSize: '8px', color: C.YELLOW, fontWeight: 700, padding: '1px 5px', backgroundColor: `${C.YELLOW}12`, borderRadius: '2px' }}>${t}</span>
                      ))}
                    </div>
                  )}
                </div>

                {/* Summary */}
                {selectedArticle.summary && (
                  <div style={{ borderTop: `1px solid ${C.BORDER}`, paddingTop: '8px', marginBottom: '10px' }}>
                    <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, letterSpacing: '0.5px', marginBottom: '4px' }}>SUMMARY</div>
                    <div style={{ fontSize: '10px', color: '#B0B0B0', lineHeight: 1.6 }}>{selectedArticle.summary}</div>
                  </div>
                )}

                {/* Impact info */}
                <div style={{ borderTop: `1px solid ${C.BORDER}`, paddingTop: '8px', marginBottom: '10px' }}>
                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px' }}>
                    <div style={{ padding: '6px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>
                      <SR k="TIER" v={`T${selectedArticle.tier ?? 4}`} c={({ 1: C.CYAN, 2: C.GREEN, 3: C.YELLOW, 4: C.TEXT_MUTE } as Record<number, string>)[selectedArticle.tier ?? 4] ?? C.TEXT_MUTE} />
                    </div>
                    <div style={{ padding: '6px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>
                      <SR k="IMPACT" v={selectedArticle.impact || 'N/A'} c={({ HIGH: C.RED, MEDIUM: C.YELLOW, LOW: C.TEXT_MUTE } as Record<string, string>)[selectedArticle.impact] ?? C.TEXT_MUTE} />
                    </div>
                  </div>
                </div>

                {/* AI Analysis section */}
                {(analyzingArticle || showAnalysis) && (
                  <div style={{ borderTop: `1px solid ${C.PURPLE}40`, paddingTop: '8px' }}>
                    <div style={{ fontSize: '8px', color: C.PURPLE, fontWeight: 700, letterSpacing: '0.5px', marginBottom: '6px', display: 'flex', alignItems: 'center', gap: '4px' }}>
                      <Zap size={9} />AI ANALYSIS
                    </div>

                    {analyzingArticle && (
                      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', padding: '20px', gap: '8px' }}>
                        <div style={{ width: '24px', height: '24px', border: `2px solid ${C.BORDER}`, borderTop: `2px solid ${C.PURPLE}`, borderRadius: '50%', animation: 'ntS 1s linear infinite' }} />
                        <span style={{ fontSize: '8px', color: C.PURPLE, fontWeight: 700 }}>ANALYZING...</span>
                      </div>
                    )}

                    {showAnalysis && analysisData && !analyzingArticle && (
                      <>
                        {analysisData.analysis.summary && (
                          <div style={{ marginBottom: '8px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>AI SUMMARY</div>
                            <div style={{ fontSize: '9px', color: '#B8B8B8', lineHeight: 1.6, padding: '6px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>{analysisData.analysis.summary}</div>
                          </div>
                        )}

                        {Array.isArray(analysisData.analysis.key_points) && analysisData.analysis.key_points.length > 0 && (
                          <div style={{ marginBottom: '8px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>KEY POINTS</div>
                            <div style={{ padding: '6px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>
                              {analysisData.analysis.key_points.filter(p => typeof p === 'string').map((pt, i) => (
                                <div key={i} style={{ fontSize: '9px', color: '#B8B8B8', paddingLeft: '6px', borderLeft: `2px solid ${C.AMBER}`, marginBottom: i < analysisData.analysis.key_points.length - 1 ? '4px' : 0, lineHeight: 1.5 }}>{pt}</div>
                              ))}
                            </div>
                          </div>
                        )}

                        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '4px', marginBottom: '8px' }}>
                          <div style={{ padding: '5px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>SENTIMENT</div>
                            <SR k="Score" v={analysisData.analysis.sentiment.score.toFixed(2)} c={getSentimentColor(analysisData.analysis.sentiment.score)} />
                            <SR k="Conf" v={`${(analysisData.analysis.sentiment.confidence * 100).toFixed(0)}%`} c={C.GREEN} />
                          </div>
                          <div style={{ padding: '5px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>MARKET</div>
                            <SR k="Urgency" v={analysisData.analysis.market_impact.urgency} c={getUrgencyColor(analysisData.analysis.market_impact.urgency)} />
                            <SR k="Dir" v={analysisData.analysis.market_impact.prediction.replace('_', ' ').toUpperCase()} c={C.CYAN} />
                          </div>
                        </div>

                        {analysisData.analysis.risk_signals && typeof analysisData.analysis.risk_signals === 'object' && (
                          <div style={{ marginBottom: '8px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>RISK SIGNALS</div>
                            {Object.entries(analysisData.analysis.risk_signals).map(([type, signal]) => {
                              if (!signal || typeof signal !== 'object' || !('level' in signal)) return null;
                              const rs = signal as { level: string; details?: string };
                              return <div key={type} style={{ padding: '3px 5px', backgroundColor: C.SURFACE, border: `1px solid ${C.BORDER}`, borderRadius: '2px', marginBottom: '2px' }}>
                                <span style={{ color: getRiskColor(rs.level || 'none'), fontWeight: 700, fontSize: '8px' }}>{type.toUpperCase()}: {(rs.level || 'NONE').toUpperCase()}</span>
                                {rs.details && <div style={{ color: C.TEXT_MUTE, fontSize: '8px', marginTop: '1px', lineHeight: 1.3 }}>{rs.details}</div>}
                              </div>;
                            })}
                          </div>
                        )}

                        {Array.isArray(analysisData.analysis.topics) && analysisData.analysis.topics.length > 0 && (
                          <div style={{ marginBottom: '8px' }}>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>TOPICS</div>
                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px' }}>
                              {analysisData.analysis.topics.filter(t => typeof t === 'string').map((topic, i) => (
                                <span key={i} style={{ padding: '1px 5px', backgroundColor: `${C.PURPLE}15`, color: C.TEXT, fontSize: '7px', fontWeight: 600, borderRadius: '2px', border: `1px solid ${C.PURPLE}30` }}>{topic}</span>
                              ))}
                            </div>
                          </div>
                        )}

                        {Array.isArray(analysisData.analysis.keywords) && analysisData.analysis.keywords.length > 0 && (
                          <div>
                            <div style={{ fontSize: '7px', color: C.TEXT_MUTE, fontWeight: 700, marginBottom: '3px' }}>KEYWORDS</div>
                            <div style={{ display: 'flex', flexWrap: 'wrap', gap: '2px' }}>
                              {analysisData.analysis.keywords.filter(k => typeof k === 'string').slice(0, 10).map((kw, i) => (
                                <span key={i} style={{ padding: '1px 4px', backgroundColor: `${C.TEXT_MUTE}15`, color: C.TEXT_MUTE, fontSize: '7px', borderRadius: '2px', border: `1px solid ${C.BORDER}` }}>{kw}</span>
                              ))}
                            </div>
                          </div>
                        )}
                      </>
                    )}
                  </div>
                )}
              </div>
            ) : (
              /* Empty reader */
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '8px', color: C.TEXT_MUTE }}>
                <BookOpen size={22} style={{ opacity: 0.15 }} />
                <div style={{ fontSize: '9px', fontWeight: 700 }}>SELECT AN ARTICLE</div>
                <div style={{ fontSize: '8px', textAlign: 'center', lineHeight: 1.5, maxWidth: '180px' }}>
                  Click any headline in the wire feed or top stories to view details and run AI analysis
                </div>
              </div>
            )}
          </div>
        </div>
      </div>

      {/* ═══ STATUS BAR ═══ */}
      <div style={{
        backgroundColor: C.TOOLBAR, borderTop: `1px solid ${C.BORDER}`,
        padding: '0 10px', height: '18px', flexShrink: 0,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        fontSize: '8px', color: C.TEXT_MUTE, fontFamily: FONT,
      }}>
        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          <span style={{ color: C.AMBER, fontWeight: 700, letterSpacing: '0.5px' }}>FINCEPT</span>
          <Divider />
          <span>FEEDS:<b style={{ color: C.CYAN }}>{feedCount}</b></span>
          <Divider />
          <span>ART:<b style={{ color: C.CYAN }}>{newsArticles.length}</b></span>
          <Divider />
          <span>CLU:<b style={{ color: C.CYAN }}>{clusters.length}</b></span>
          <Divider />
          <span>FLT:<b style={{ color: C.AMBER }}>{activeFilter}</b></span>
          <Divider />
          <span>RNG:<b style={{ color: C.AMBER }}>{timeRange}</b></span>
        </div>
        <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
          <span>SENT:<b style={{ color: scoreNum > 0 ? C.GREEN : scoreNum < 0 ? C.RED : C.YELLOW }}>{score}</b></span>
          <Divider />
          <span>{formattedTime} <b style={{ color: C.AMBER }}>{timezone.shortLabel}</b></span>
          <Divider />
          <span style={{ color: loading ? C.YELLOW : C.GREEN, fontWeight: 700 }}>{loading ? '⟳ SYNC' : '● LIVE'}</span>
        </div>
      </div>

      <RSSFeedSettingsModal isOpen={showFeedSettings} onClose={() => setShowFeedSettings(false)} onFeedsUpdated={() => handleRefresh(true)} />
    </div>
  );
};

// ─── Micro helpers ───
const Divider = () => <span style={{ color: C.BORDER }}>│</span>;
const EmptyState: React.FC<{ msg: string }> = ({ msg }) => (
  <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '6px', color: C.TEXT_MUTE }}>
    <Newspaper size={18} style={{ opacity: 0.15 }} />
    <span style={{ fontSize: '9px', fontWeight: 700 }}>{msg}</span>
  </div>
);

export default NewsTab;
