import React, { useState, useEffect, useCallback, useRef } from 'react';
import { Info, Settings, RefreshCw, Clock, Zap, Newspaper, ExternalLink, Copy, X, TrendingUp, TrendingDown, Minus, AlertTriangle, Search } from 'lucide-react';
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
const FONT = '"IBM Plex Mono", "Consolas", monospace';

// ── Helpers ──
const priColor = (p: string) => ({ FLASH: F.RED, URGENT: F.ORANGE, BREAKING: F.YELLOW, ROUTINE: F.GRAY }[p] || F.MUTED);
const sentColor = (s: string) => ({ BULLISH: F.GREEN, BEARISH: F.RED, NEUTRAL: F.YELLOW }[s] || F.MUTED);
const impColor = (i: string) => ({ HIGH: F.RED, MEDIUM: F.YELLOW, LOW: F.GREEN }[i] || F.MUTED);
const SentIcon: React.FC<{ s: string; size?: number }> = ({ s, size = 10 }) =>
  s === 'BULLISH' ? <TrendingUp size={size} /> : s === 'BEARISH' ? <TrendingDown size={size} /> : <Minus size={size} />;

// ══════════════════════════════════════════════════════════════
// NEWS GRID CARD – compact, shows key info at a glance
// ══════════════════════════════════════════════════════════════
const NewsGridCard: React.FC<{
  article: NewsArticle;
  onClick: () => void;
}> = ({ article, onClick }) => {
  const [hovered, setHovered] = useState(false);
  const isHighPri = article.priority === 'FLASH' || article.priority === 'URGENT';
  return (
    <div
      onClick={onClick}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        padding: '10px 12px',
        backgroundColor: hovered ? F.HOVER : F.PANEL_BG,
        border: `1px solid ${isHighPri ? `${priColor(article.priority)}40` : F.BORDER}`,
        borderLeft: `3px solid ${priColor(article.priority)}`,
        borderRadius: '2px',
        cursor: 'pointer',
        transition: 'background 0.15s',
        display: 'flex',
        flexDirection: 'column',
        justifyContent: 'space-between',
        minHeight: '100px',
      }}
    >
      {/* Top: Source + Time */}
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '6px' }}>
        <span style={{ fontSize: '9px', fontWeight: 700, color: F.CYAN, letterSpacing: '0.3px' }}>
          {article.source}
        </span>
        {article.time && article.time.trim() !== '' && (
          <span style={{ fontSize: '9px', color: F.MUTED }}>{article.time}</span>
        )}
      </div>

      {/* Headline */}
      <div style={{
        color: F.WHITE,
        fontSize: '11px',
        fontWeight: 600,
        lineHeight: '1.4',
        flex: 1,
        overflow: 'hidden',
        textOverflow: 'ellipsis',
        display: '-webkit-box',
        WebkitLineClamp: 3,
        WebkitBoxOrient: 'vertical' as any,
        marginBottom: '8px',
      }}>{article.headline}</div>

      {/* Bottom: badges row */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
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
          color: F.GRAY, backgroundColor: `${F.MUTED}20`,
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
// ARTICLE DETAIL MODAL (overlay)
// ══════════════════════════════════════════════════════════════
const ArticleDetailModal: React.FC<{
  article: NewsArticle;
  onClose: () => void;
  onAnalyze: () => void;
  analyzing: boolean;
  analysisData: NewsAnalysisData | null;
  showAnalysis: boolean;
  openExternal: (url: string) => void;
}> = ({ article, onClose, onAnalyze, analyzing, analysisData, showAnalysis, openExternal }) => (
  <div
    onClick={onClose}
    style={{
      position: 'fixed', inset: 0, zIndex: 9999,
      backgroundColor: 'rgba(0,0,0,0.75)',
      display: 'flex', alignItems: 'center', justifyContent: 'center',
      fontFamily: FONT,
    }}
  >
    <div
      onClick={e => e.stopPropagation()}
      style={{
        width: '700px', maxWidth: '90vw', maxHeight: '85vh',
        backgroundColor: F.PANEL_BG, border: `1px solid ${F.ORANGE}40`,
        borderRadius: '2px', display: 'flex', flexDirection: 'column',
        overflow: 'hidden', boxShadow: `0 8px 32px rgba(0,0,0,0.6), 0 0 0 1px ${F.BORDER}`,
      }}
    >
      {/* Modal Header */}
      <div style={{
        padding: '10px 16px', backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        display: 'flex', alignItems: 'center', justifyContent: 'space-between', flexShrink: 0,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Newspaper size={13} style={{ color: F.ORANGE }} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: F.ORANGE, letterSpacing: '0.5px' }}>ARTICLE DETAIL</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ fontSize: '10px', color: F.CYAN, fontWeight: 700 }}>{article.source}</span>
          {article.time && article.time.trim() !== '' && (
            <span style={{ fontSize: '10px', color: F.MUTED }}>{article.time}</span>
          )}
        </div>
        <button onClick={onClose} style={{
          background: 'none', border: `1px solid ${F.BORDER}`, color: F.GRAY,
          cursor: 'pointer', borderRadius: '2px', padding: '3px 6px',
          display: 'flex', alignItems: 'center', gap: '4px', fontSize: '9px', fontWeight: 700,
        }}><X size={11} />CLOSE</button>
      </div>

      {/* Modal Content */}
      <div style={{ flex: 1, overflow: 'auto', padding: '16px' }}>
        {/* Badges */}
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap', marginBottom: '12px' }}>
          {[
            { label: article.priority, color: priColor(article.priority) },
            { label: article.category, color: F.CYAN },
            { label: article.sentiment, color: sentColor(article.sentiment) },
            { label: article.region, color: F.PURPLE },
            { label: `IMPACT: ${article.impact}`, color: impColor(article.impact) },
          ].map((b, i) => (
            <span key={i} style={{
              padding: '3px 10px', backgroundColor: `${b.color}15`, color: b.color,
              fontSize: '9px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${b.color}30`,
            }}>{b.label}</span>
          ))}
        </div>

        {/* Headline */}
        <div style={{
          color: F.WHITE, fontSize: '14px', fontWeight: 700, lineHeight: '1.45',
          marginBottom: '12px', padding: '10px 14px', backgroundColor: F.DARK_BG,
          borderLeft: `3px solid ${priColor(article.priority)}`, borderRadius: '2px',
        }}>{article.headline}</div>

        {/* Tickers */}
        {article.tickers && article.tickers.length > 0 && (
          <div style={{ display: 'flex', gap: '5px', marginBottom: '12px', flexWrap: 'wrap' }}>
            {article.tickers.map((ticker, i) => (
              <span key={i} style={{
                padding: '2px 10px', backgroundColor: `${F.GREEN}15`, color: F.GREEN,
                fontSize: '9px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${F.GREEN}30`,
              }}>{ticker}</span>
            ))}
          </div>
        )}

        {/* Summary */}
        <div style={{ color: F.GRAY, fontSize: '11px', lineHeight: '1.7', marginBottom: '14px' }}>
          {article.summary}
        </div>

        {/* Action Buttons */}
        {article.link && (
          <div style={{ display: 'flex', gap: '6px', marginBottom: '16px' }}>
            <button
              onClick={() => openExternal(article.link!)}
              style={{
                flex: 1, padding: '8px 12px', backgroundColor: F.BLUE, color: F.WHITE,
                border: 'none', fontSize: '10px', fontWeight: 700, cursor: 'pointer',
                borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
              }}
            ><ExternalLink size={12} />OPEN IN BROWSER</button>
            <button
              onClick={() => navigator.clipboard.writeText(article.link!)}
              style={{
                padding: '8px 14px', background: 'none', border: `1px solid ${F.BORDER}`,
                color: F.GRAY, fontSize: '10px', fontWeight: 700, cursor: 'pointer',
                borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '5px',
              }}
            ><Copy size={12} />COPY</button>
            <button
              onClick={onAnalyze}
              disabled={analyzing}
              style={{
                flex: 1, padding: '8px 12px',
                backgroundColor: analyzing ? F.MUTED : F.PURPLE,
                color: F.WHITE, border: 'none', fontSize: '10px', fontWeight: 700,
                cursor: analyzing ? 'not-allowed' : 'pointer',
                borderRadius: '2px', opacity: analyzing ? 0.5 : 1,
                display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
              }}
            ><Zap size={12} />{analyzing ? 'ANALYZING...' : 'AI ANALYZE'}</button>
          </div>
        )}

        {/* AI Analysis Results */}
        {showAnalysis && analysisData && (
          <div style={{ borderTop: `1px solid ${F.BORDER}`, paddingTop: '14px' }}>
            <div style={{
              padding: '8px 12px', backgroundColor: `${F.PURPLE}15`, border: `1px solid ${F.PURPLE}`,
              borderRadius: '2px', marginBottom: '14px', display: 'flex', justifyContent: 'space-between',
              alignItems: 'center', fontSize: '10px',
            }}>
              <span style={{ color: F.PURPLE, fontWeight: 700, letterSpacing: '0.5px' }}>AI POWERED ANALYSIS</span>
              <span style={{ color: F.GRAY }}>{analysisData.credits_used} CR | {analysisData.credits_remaining.toLocaleString()} REM</span>
            </div>

            <div style={{
              fontSize: '11px', color: F.WHITE, lineHeight: '1.6', padding: '10px 14px',
              backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', marginBottom: '14px',
            }}>{analysisData.analysis.summary}</div>

            {analysisData.analysis.key_points.length > 0 && (
              <div style={{ marginBottom: '14px' }}>
                <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '6px', letterSpacing: '0.5px' }}>KEY POINTS</div>
                <div style={{ padding: '10px 14px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
                  {analysisData.analysis.key_points.map((pt, i) => (
                    <div key={i} style={{
                      fontSize: '10px', color: F.WHITE, paddingLeft: '10px',
                      borderLeft: `2px solid ${F.ORANGE}`, marginBottom: '6px', lineHeight: '1.5',
                    }}>{pt}</div>
                  ))}
                </div>
              </div>
            )}

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '14px' }}>
              <div style={{ padding: '10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
                <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '6px' }}>SENTIMENT</div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: '3px' }}>
                  <span style={{ color: F.GRAY }}>Score</span>
                  <span style={{ color: getSentimentColor(analysisData.analysis.sentiment.score), fontWeight: 700 }}>
                    {analysisData.analysis.sentiment.score.toFixed(2)}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: '3px' }}>
                  <span style={{ color: F.GRAY }}>Intensity</span>
                  <span style={{ color: F.CYAN, fontWeight: 700 }}>{analysisData.analysis.sentiment.intensity.toFixed(2)}</span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                  <span style={{ color: F.GRAY }}>Confidence</span>
                  <span style={{ color: F.GREEN, fontWeight: 700 }}>{(analysisData.analysis.sentiment.confidence * 100).toFixed(0)}%</span>
                </div>
              </div>
              <div style={{ padding: '10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
                <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '6px' }}>MARKET IMPACT</div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: '3px' }}>
                  <span style={{ color: F.GRAY }}>Urgency</span>
                  <span style={{ color: getUrgencyColor(analysisData.analysis.market_impact.urgency), fontWeight: 700 }}>
                    {analysisData.analysis.market_impact.urgency}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px' }}>
                  <span style={{ color: F.GRAY }}>Prediction</span>
                  <span style={{ color: F.CYAN, fontWeight: 700 }}>
                    {analysisData.analysis.market_impact.prediction.replace('_', ' ').toUpperCase()}
                  </span>
                </div>
              </div>
            </div>

            {/* Risk Signals */}
            <div style={{ marginBottom: '14px' }}>
              <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '6px', letterSpacing: '0.5px' }}>RISK SIGNALS</div>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
                {Object.entries(analysisData.analysis.risk_signals).map(([type, signal]) => (
                  <div key={type} style={{
                    padding: '8px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
                  }}>
                    <div style={{ color: getRiskColor(signal.level), fontWeight: 700, fontSize: '10px', marginBottom: '2px' }}>
                      {type.toUpperCase()}: {signal.level.toUpperCase()}
                    </div>
                    {signal.details && <div style={{ color: F.MUTED, fontSize: '9px', lineHeight: '1.3' }}>{signal.details}</div>}
                  </div>
                ))}
              </div>
            </div>

            {/* Topics + Keywords */}
            {(analysisData.analysis.topics.length > 0 || analysisData.analysis.keywords.length > 0) && (
              <div style={{ marginBottom: '14px' }}>
                {analysisData.analysis.topics.length > 0 && (
                  <div style={{ marginBottom: '8px' }}>
                    <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '4px' }}>TOPICS</div>
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                      {analysisData.analysis.topics.map((topic, idx) => (
                        <span key={idx} style={{
                          padding: '2px 8px', backgroundColor: `${F.PURPLE}20`, color: F.WHITE,
                          fontSize: '9px', fontWeight: 700, borderRadius: '2px', border: `1px solid ${F.PURPLE}`,
                        }}>{topic}</span>
                      ))}
                    </div>
                  </div>
                )}
                {analysisData.analysis.keywords.length > 0 && (
                  <div>
                    <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '4px' }}>KEYWORDS</div>
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                      {analysisData.analysis.keywords.slice(0, 8).map((kw, idx) => (
                        <span key={idx} style={{
                          padding: '2px 8px', backgroundColor: `${F.MUTED}20`, color: F.GRAY,
                          fontSize: '9px', borderRadius: '2px', border: `1px solid ${F.MUTED}`,
                        }}>{kw}</span>
                      ))}
                    </div>
                  </div>
                )}
              </div>
            )}

            {/* Entities */}
            {analysisData.analysis.entities.organizations.length > 0 && (
              <div>
                <div style={{ fontSize: '10px', color: F.GRAY, fontWeight: 700, marginBottom: '4px' }}>ORGANIZATIONS</div>
                {analysisData.analysis.entities.organizations.map((org, idx) => (
                  <div key={idx} style={{
                    padding: '6px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
                    borderRadius: '2px', marginBottom: '4px',
                  }}>
                    <span style={{ color: F.WHITE, fontWeight: 700, fontSize: '10px' }}>{org.name}</span>
                    <span style={{ color: F.GRAY, fontSize: '10px', marginLeft: '6px' }}>
                      {org.sector} {org.ticker ? `| ${org.ticker}` : ''}
                    </span>
                  </div>
                ))}
              </div>
            )}
          </div>
        )}
      </div>
    </div>
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
  const [refreshInterval, setRefreshInterval] = useState(10);
  const [showIntervalSettings, setShowIntervalSettings] = useState(false);
  const [isRecording, setIsRecording] = useState(false);
  const [analysisData, setAnalysisData] = useState<NewsAnalysisData | null>(null);
  const [analyzingArticle, setAnalyzingArticle] = useState(false);
  const [showAnalysis, setShowAnalysis] = useState(false);
  const [showFeedSettings, setShowFeedSettings] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');

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
  return (
    <div style={{
      height: '100%',
      backgroundColor: F.DARK_BG,
      color: F.WHITE,
      fontFamily: FONT,
      overflow: 'hidden',
      display: 'flex',
      flexDirection: 'column',
      fontSize: '11px',
    }}>

      {/* ── TOP BAR: Title + Filters + Actions (single merged bar) ── */}
      <div style={{
        backgroundColor: F.HEADER_BG,
        borderBottom: `2px solid ${F.ORANGE}`,
        padding: '0 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${F.ORANGE}20`,
        flexShrink: 0,
        height: '38px',
        position: 'relative',
      }}>
        {/* Left: branding + live indicator */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Newspaper size={13} style={{ color: F.ORANGE }} />
          <span style={{ color: F.ORANGE, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
            {t('title')}
          </span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.RED, fontWeight: 700, fontSize: '9px', animation: 'ntBlink 1s infinite' }}>LIVE</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.YELLOW, fontSize: '9px', fontWeight: 700 }}>
            <AlertTriangle size={9} style={{ display: 'inline', verticalAlign: 'middle', marginRight: '3px' }} />
            {alertCount}
          </span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: F.GREEN, fontSize: '9px', fontWeight: 700 }}>{feedCount} SRC</span>
          <span style={{ color: F.BORDER }}>|</span>
          <TimezoneSelector compact />
        </div>

        {/* Right: actions */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
          <button onClick={() => createNewsTabTour().drive()} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
          }}><Info size={10} /></button>
          <button id="news-refresh" onClick={() => handleRefresh(true)} disabled={loading} style={{
            padding: '4px 10px', backgroundColor: loading ? F.MUTED : F.ORANGE,
            color: loading ? F.GRAY : F.DARK_BG, border: 'none', fontSize: '9px', fontWeight: 700,
            cursor: loading ? 'not-allowed' : 'pointer', borderRadius: '2px',
            display: 'flex', alignItems: 'center', gap: '4px',
          }}><RefreshCw size={10} />{loading ? 'LOADING' : 'REFRESH'}</button>
          <button id="news-interval-settings" onClick={() => setShowIntervalSettings(!showIntervalSettings)} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}><Clock size={10} />{refreshInterval}M</button>
          <button onClick={() => setShowFeedSettings(true)} style={{
            padding: '4px 8px', background: 'none', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, fontSize: '9px', fontWeight: 700, cursor: 'pointer', borderRadius: '2px',
          }}><Settings size={10} /></button>
          <RecordingControlPanel tabName="News" onRecordingChange={setIsRecording} onRecordingStart={recordCurrentData} />
        </div>

        {/* Interval dropdown */}
        {showIntervalSettings && (
          <div style={{
            position: 'absolute', top: '100%', right: 16,
            backgroundColor: F.PANEL_BG, border: `1px solid ${F.ORANGE}`,
            borderRadius: '2px', padding: '6px', zIndex: 1000, marginTop: '2px',
          }}>
            <div style={{ fontSize: '9px', color: F.GRAY, fontWeight: 700, marginBottom: '4px', letterSpacing: '0.5px' }}>
              AUTO-REFRESH
            </div>
            {[1, 2, 5, 10, 15, 30].map(min => (
              <button key={min}
                onClick={() => { setRefreshInterval(min); setShowIntervalSettings(false); }}
                style={{
                  width: '100%', backgroundColor: refreshInterval === min ? F.ORANGE : 'transparent',
                  color: refreshInterval === min ? F.DARK_BG : F.WHITE,
                  border: `1px solid ${refreshInterval === min ? F.ORANGE : F.BORDER}`,
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
        backgroundColor: F.PANEL_BG, borderBottom: `1px solid ${F.BORDER}`,
        padding: '3px 10px', flexShrink: 0, display: 'flex', alignItems: 'center', gap: '2px',
      }}>
        {filters.map(item => (
          <button key={item.key}
            onClick={() => setActiveFilter(item.f)}
            style={{
              padding: '3px 10px',
              backgroundColor: activeFilter === item.f ? F.ORANGE : 'transparent',
              color: activeFilter === item.f ? F.DARK_BG : F.GRAY,
              border: 'none', fontSize: '9px', fontWeight: 700,
              letterSpacing: '0.3px', cursor: 'pointer', borderRadius: '2px',
            }}>
            <span style={{ color: activeFilter === item.f ? F.DARK_BG : F.MUTED, marginRight: '3px', fontSize: '8px' }}>{item.key}</span>
            {item.label}
          </button>
        ))}
        {/* Search Bar */}
        <div style={{
          marginLeft: '12px',
          display: 'flex',
          alignItems: 'center',
          gap: '4px',
          backgroundColor: F.DARK_BG,
          border: `1px solid ${F.BORDER}`,
          borderRadius: '2px',
          padding: '2px 8px',
          flex: '0 1 220px',
          minWidth: '120px',
        }}>
          <Search size={10} style={{ color: F.MUTED, flexShrink: 0 }} />
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
              color: F.WHITE,
              fontSize: '9px',
              fontFamily: FONT,
              padding: '2px 0',
            }}
          />
          {searchQuery && (
            <button
              onClick={() => setSearchQuery('')}
              style={{
                background: 'none', border: 'none', color: F.MUTED,
                cursor: 'pointer', padding: '0', display: 'flex', alignItems: 'center',
              }}
            >
              <X size={9} />
            </button>
          )}
        </div>
        <div style={{ marginLeft: 'auto', display: 'flex', gap: '12px', fontSize: '9px' }}>
          <span style={{ color: F.GRAY }}>TOTAL: <span style={{ color: F.CYAN, fontWeight: 700 }}>{newsArticles.length}</span></span>
          <span style={{ color: F.GRAY }}>SHOWING: <span style={{ color: F.CYAN, fontWeight: 700 }}>{filteredNews.length}</span></span>
          <span style={{ color: loading ? F.YELLOW : F.GREEN, fontWeight: 700 }}>
            {loading ? 'UPDATING' : (isUsingMockData() ? 'DEMO' : 'LIVE')}
          </span>
        </div>
      </div>

      {/* ══ MAIN CONTENT: Sidebar + Grid ══ */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex' }}>

        {/* ── LEFT SIDEBAR: Sentiment + Stats (fixed, no scroll needed) ── */}
        <div style={{
          width: '200px', flexShrink: 0, backgroundColor: F.PANEL_BG,
          borderRight: `1px solid ${F.BORDER}`,
          display: 'flex', flexDirection: 'column',
          padding: '12px',
          gap: '12px',
          overflow: 'hidden',
        }}>
          {/* Sentiment */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
              MARKET SENTIMENT
            </div>
            <div style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', padding: '10px' }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '5px', fontSize: '10px' }}>
                <span style={{ color: F.GREEN }}>BULL</span>
                <span style={{ color: F.GREEN, fontWeight: 700 }}>{bullish} ({Math.round((bullish / total) * 100)}%)</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '5px', fontSize: '10px' }}>
                <span style={{ color: F.RED }}>BEAR</span>
                <span style={{ color: F.RED, fontWeight: 700 }}>{bearish} ({Math.round((bearish / total) * 100)}%)</span>
              </div>
              <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px', fontSize: '10px' }}>
                <span style={{ color: F.YELLOW }}>NEUT</span>
                <span style={{ color: F.YELLOW, fontWeight: 700 }}>{neutral} ({Math.round((neutral / total) * 100)}%)</span>
              </div>
              {/* Bar */}
              <div style={{ height: '4px', backgroundColor: F.BORDER, borderRadius: '2px', display: 'flex', overflow: 'hidden', marginBottom: '8px' }}>
                <div style={{ width: `${Math.round((bullish / total) * 100)}%`, backgroundColor: F.GREEN }} />
                <div style={{ width: `${Math.round((neutral / total) * 100)}%`, backgroundColor: F.YELLOW }} />
                <div style={{ width: `${Math.round((bearish / total) * 100)}%`, backgroundColor: F.RED }} />
              </div>
              {/* Net Score */}
              <div style={{
                textAlign: 'center', padding: '6px', backgroundColor: `${parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW}10`,
                borderRadius: '2px', border: `1px solid ${parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW}30`,
              }}>
                <div style={{ fontSize: '9px', color: F.GRAY, marginBottom: '2px' }}>NET SCORE</div>
                <div style={{
                  fontSize: '16px', fontWeight: 700,
                  color: parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW,
                }}>{score}</div>
              </div>
            </div>
          </div>

          {/* Feed Stats */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
              FEED STATISTICS
            </div>
            <div style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', padding: '10px' }}>
              {[
                { label: 'ARTICLES', value: newsArticles.length, color: F.CYAN },
                { label: 'FEEDS', value: feedCount, color: F.CYAN },
                { label: 'ALERTS', value: alertCount, color: F.ORANGE },
                { label: 'UPDATES', value: newsUpdateCount, color: F.CYAN },
              ].map((s, i) => (
                <div key={i} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: i < 3 ? '4px' : 0 }}>
                  <span style={{ color: F.GRAY }}>{s.label}</span>
                  <span style={{ color: s.color, fontWeight: 700 }}>{s.value}</span>
                </div>
              ))}
            </div>
          </div>

          {/* Top Categories */}
          <div>
            <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
              CATEGORIES
            </div>
            <div style={{ backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', padding: '10px' }}>
              {topCategories.length > 0 ? topCategories.map(([cat, count], i) => (
                <div key={cat} style={{ display: 'flex', justifyContent: 'space-between', fontSize: '10px', marginBottom: i < topCategories.length - 1 ? '4px' : 0 }}>
                  <span style={{ color: F.GRAY }}>{cat}</span>
                  <span style={{ color: F.PURPLE, fontWeight: 700 }}>{count}</span>
                </div>
              )) : (
                <div style={{ fontSize: '10px', color: F.MUTED }}>No data</div>
              )}
            </div>
          </div>

          {/* Active Sources (compact) */}
          <div style={{ flex: 1, minHeight: 0 }}>
            <div style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
              ACTIVE SOURCES
            </div>
            <div style={{
              backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              padding: '8px 10px', display: 'flex', flexWrap: 'wrap', gap: '4px',
            }}>
              {activeSources.slice(0, 8).map((src, i) => (
                <span key={i} style={{
                  padding: '1px 6px', fontSize: '8px', fontWeight: 700,
                  color: F.CYAN, backgroundColor: `${F.CYAN}10`,
                  borderRadius: '2px', border: `1px solid ${F.CYAN}20`,
                }}>{src}</span>
              ))}
              {activeSources.length > 8 && (
                <span style={{ fontSize: '8px', color: F.MUTED }}>+{activeSources.length - 8}</span>
              )}
            </div>
          </div>
        </div>

        {/* ── CENTER: News Grid ── */}
        <div id="news-primary-feed" style={{
          flex: 1, overflow: 'auto', padding: '12px',
          backgroundColor: F.DARK_BG,
        }}>
          {loading && newsArticles.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '100%', color: F.MUTED,
            }}>
              <RefreshCw size={20} className="animate-spin" style={{ marginBottom: '10px', opacity: 0.5 }} />
              <span style={{ fontSize: '11px' }}>Loading news from {feedCount} RSS feeds...</span>
            </div>
          ) : filteredNews.length === 0 ? (
            <div style={{
              display: 'flex', flexDirection: 'column', alignItems: 'center',
              justifyContent: 'center', height: '100%', color: F.MUTED,
            }}>
              <Newspaper size={20} style={{ marginBottom: '10px', opacity: 0.5 }} />
              <span style={{ fontSize: '11px' }}>No articles found for filter: {activeFilter}</span>
            </div>
          ) : (
            <div style={{
              display: 'grid',
              gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))',
              gap: '8px',
              alignContent: 'start',
            }}>
              {filteredNews.map(article => (
                <NewsGridCard
                  key={article.id}
                  article={article}
                  onClick={() => { setSelectedArticle(article); setShowAnalysis(false); setAnalysisData(null); }}
                />
              ))}
            </div>
          )}
        </div>
      </div>

      {/* ── STATUS BAR ── */}
      <div style={{
        backgroundColor: F.HEADER_BG, borderTop: `1px solid ${F.BORDER}`,
        padding: '3px 16px', fontSize: '9px', color: F.GRAY, flexShrink: 0,
        display: 'flex', justifyContent: 'space-between', alignItems: 'center',
      }}>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>FINCEPT NEWS v1.0</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>FEEDS: <span style={{ color: F.CYAN }}>{feedCount}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>ARTICLES: <span style={{ color: F.CYAN }}>{newsArticles.length}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>FILTER: <span style={{ color: F.ORANGE }}>{activeFilter}</span></span>
        </div>
        <div style={{ display: 'flex', gap: '12px' }}>
          <span>SENTIMENT: <span style={{ color: parseFloat(score) > 0 ? F.GREEN : parseFloat(score) < 0 ? F.RED : F.YELLOW }}>{score}</span></span>
          <span style={{ color: F.BORDER }}>|</span>
          <span>{currentTime.toTimeString().substring(0, 8)}</span>
          <span style={{ color: F.BORDER }}>|</span>
          <span style={{ color: loading ? F.YELLOW : F.GREEN, fontWeight: 700 }}>{loading ? 'UPDATING' : 'LIVE'}</span>
        </div>
      </div>

      {/* ── ARTICLE DETAIL MODAL (overlay, on demand) ── */}
      {selectedArticle && (
        <ArticleDetailModal
          article={selectedArticle}
          onClose={() => setSelectedArticle(null)}
          onAnalyze={handleAnalyzeArticle}
          analyzing={analyzingArticle}
          analysisData={analysisData}
          showAnalysis={showAnalysis}
          openExternal={openExternal}
        />
      )}

      {/* ── CSS ── */}
      <style>{`
        @keyframes ntBlink { 0%, 50% { opacity: 1; } 51%, 100% { opacity: 0.3; } }
        .nt-grid::-webkit-scrollbar { width: 6px; height: 6px; }
        .nt-grid::-webkit-scrollbar-track { background: ${F.DARK_BG}; }
        .nt-grid::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }
        .nt-grid::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${F.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${F.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${F.MUTED}; }
      `}</style>

      <RSSFeedSettingsModal isOpen={showFeedSettings} onClose={() => setShowFeedSettings(false)} onFeedsUpdated={() => handleRefresh(true)} />
    </div>
  );
};

export default NewsTab;
