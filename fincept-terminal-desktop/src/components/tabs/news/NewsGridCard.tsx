/**
 * NewsGridCard.tsx — Flat article row + shared helpers
 * Bloomberg Terminal Design — Dense wire-style news display
 */
import React, { useState } from 'react';
import { TrendingUp, TrendingDown, Minus, ExternalLink, Copy, Zap } from 'lucide-react';
import type { NewsArticle } from '@/services/news/newsService';

export type TimeRange = '1H' | '6H' | '24H' | '48H' | '7D';
export type ViewMode = 'clustered' | 'flat';

// ── Bloomberg-style design tokens (matching NewsTab.tsx) ─────────────────────
const C = {
  BG: '#000000', SURFACE: '#080808', PANEL: '#0D0D0D',
  RAISED: '#141414', TOOLBAR: '#1A1A1A',
  BORDER: '#1E1E1E', DIVIDER: '#2A2A2A',
  TEXT: '#D4D4D4', TEXT_DIM: '#888888', TEXT_MUTE: '#555555', TEXT_OFF: '#333333',
  AMBER: '#FF8800', BLUE: '#4DA6FF', CYAN: '#00D4AA',
  GREEN: '#26C281', RED: '#E55A5A', YELLOW: '#E8B634',
  PURPLE: '#9D6EDD', WHITE: '#FFFFFF',
} as const;
const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';

// ── Shared exports ───────────────────────────────────────────────────────────
export const SentIcon: React.FC<{ s: string; size?: number }> = ({ s, size = 10 }) =>
  s === 'BULLISH' ? <TrendingUp size={size} /> :
  s === 'BEARISH' ? <TrendingDown size={size} /> :
  <Minus size={size} />;

export const TIER_COLORS: Record<number, string> = {
  1: C.CYAN, 2: C.GREEN, 3: C.YELLOW, 4: C.TEXT_MUTE,
};

export const TIME_RANGE_SECONDS: Record<TimeRange, number> = {
  '1H': 3600, '6H': 21600, '24H': 86400, '48H': 172800, '7D': 604800,
};

export function filterByTimeRange(articles: NewsArticle[], range: TimeRange): NewsArticle[] {
  const cutoff = Math.floor(Date.now() / 1000) - TIME_RANGE_SECONDS[range];
  return articles.filter(a => !a.sort_ts || a.sort_ts >= cutoff);
}

export function interleaveBySource(articles: NewsArticle[]): NewsArticle[] {
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
}

// ── Helpers ──────────────────────────────────────────────────────────────────
function priColor(p: string): string {
  return ({ FLASH: C.RED, URGENT: C.AMBER, BREAKING: C.YELLOW, ROUTINE: C.TEXT_MUTE })[p] ?? C.TEXT_MUTE;
}
function sentColor(s: string): string {
  return ({ BULLISH: C.GREEN, BEARISH: C.RED, NEUTRAL: C.YELLOW })[s] ?? C.TEXT_MUTE;
}

function relTime(ts?: number): string {
  if (!ts) return '';
  const s = Math.floor(Date.now() / 1000) - ts;
  if (s < 60) return `${s}s`;
  if (s < 3600) return `${Math.floor(s / 60)}m`;
  if (s < 86400) return `${Math.floor(s / 3600)}h`;
  return `${Math.floor(s / 86400)}d`;
}

// ── NewsGridCard ─────────────────────────────────────────────────────────────
export const NewsGridCard: React.FC<{
  article: NewsArticle;
  onAnalyze: (article: NewsArticle) => void;
  analyzing: boolean;
  currentAnalyzingId: string | null;
  openExternal: (url: string) => void;
  colors: Record<string, string>;
  priColor: (p: string) => string;
  sentColor: (s: string) => string;
  impColor: (i: string) => string;
  fontFamily: string;
}> = ({ article, onAnalyze, analyzing, currentAnalyzingId, openExternal }) => {
  const [hovered, setHovered] = useState(false);
  const [expanded, setExpanded] = useState(false);
  const isHighPri = article.priority === 'FLASH' || article.priority === 'URGENT';
  const isAnalyzing = analyzing && currentAnalyzingId === article.id;
  const tierColor = TIER_COLORS[article.tier] ?? TIER_COLORS[4];
  const pc = priColor(article.priority);
  const sc = sentColor(article.sentiment);
  const rt = relTime(article.sort_ts) || article.time || '';

  return (
    <div
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        padding: '6px 10px',
        backgroundColor: hovered ? C.RAISED : C.BG,
        borderBottom: `1px solid ${C.BORDER}`,
        borderLeft: `2px solid ${isHighPri ? pc : 'transparent'}`,
        transition: 'background 0.08s',
        display: 'flex', flexDirection: 'column',
        fontFamily: FONT, cursor: 'pointer',
      }}
    >
      {/* Row 1: metadata line */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px' }}>
        <span style={{
          fontSize: '8px', fontWeight: 700, color: C.CYAN,
          letterSpacing: '0.3px',
        }}>{article.source}</span>
        <span style={{
          fontSize: '7px', fontWeight: 700, padding: '1px 4px',
          backgroundColor: `${tierColor}20`, color: tierColor,
          borderRadius: '2px',
        }}>T{article.tier ?? 4}</span>
        {isHighPri && (
          <span style={{
            fontSize: '7px', fontWeight: 700, padding: '1px 5px',
            backgroundColor: `${pc}20`, color: pc,
            borderRadius: '2px', border: `1px solid ${pc}40`,
          }}>{article.priority}</span>
        )}
        <div style={{ flex: 1 }} />
        <span style={{ fontSize: '8px', color: sc, display: 'flex', alignItems: 'center', gap: '2px' }}>
          <SentIcon s={article.sentiment} size={8} />{article.sentiment}
        </span>
        <span style={{ fontSize: '8px', color: C.TEXT_MUTE }}>{rt}</span>
      </div>

      {/* Row 2: headline */}
      <div
        onClick={() => setExpanded(!expanded)}
        style={{
          color: isHighPri ? C.WHITE : C.TEXT,
          fontSize: '10px', fontWeight: isHighPri ? 700 : 500,
          lineHeight: '1.4',
          overflow: 'hidden', display: '-webkit-box',
          WebkitLineClamp: expanded ? 'unset' : 2,
          WebkitBoxOrient: 'vertical' as React.CSSProperties['WebkitBoxOrient'],
          marginBottom: '3px',
        }}
      >{article.headline}</div>

      {/* Expanded: summary + tickers */}
      {expanded && article.summary && (
        <div style={{
          color: C.TEXT_DIM, fontSize: '9px', lineHeight: '1.5',
          marginBottom: '5px', paddingLeft: '8px',
          borderLeft: `2px solid ${C.BORDER}`,
        }}>{article.summary}</div>
      )}

      {/* Bottom row: category + tickers + actions */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '4px', flexWrap: 'wrap' }}>
        <span style={{
          fontSize: '7px', fontWeight: 700, color: C.TEXT_MUTE,
          padding: '1px 5px', backgroundColor: `${C.TEXT_MUTE}15`, borderRadius: '2px',
        }}>{article.category}</span>
        {article.tickers?.slice(0, 3).map(t => (
          <span key={t} style={{
            fontSize: '7px', fontWeight: 700, color: C.YELLOW,
            padding: '1px 4px', backgroundColor: `${C.YELLOW}12`, borderRadius: '2px',
          }}>${t}</span>
        ))}
        <span style={{
          fontSize: '7px', fontWeight: 700, marginLeft: 'auto',
          color: ({ HIGH: C.RED, MEDIUM: C.YELLOW, LOW: C.TEXT_MUTE } as Record<string, string>)[article.impact] ?? C.TEXT_MUTE,
        }}>{article.impact}</span>

        {/* Hover actions */}
        {(hovered || expanded) && article.link && (
          <>
            <button
              onClick={e => { e.stopPropagation(); openExternal(article.link!); }}
              style={{
                padding: '2px 6px', fontSize: '7px', fontWeight: 700,
                backgroundColor: `${C.BLUE}15`, border: `1px solid ${C.BLUE}40`,
                color: C.BLUE, cursor: 'pointer', borderRadius: '2px',
                fontFamily: FONT, display: 'flex', alignItems: 'center', gap: '3px',
              }}
            ><ExternalLink size={8} />OPEN</button>
            <button
              onClick={e => { e.stopPropagation(); navigator.clipboard.writeText(article.link!); }}
              style={{
                padding: '2px 5px', background: 'none',
                border: `1px solid ${C.BORDER}`, color: C.TEXT_MUTE,
                fontSize: '7px', cursor: 'pointer', borderRadius: '2px',
                fontFamily: FONT, display: 'flex', alignItems: 'center',
              }}
            ><Copy size={8} /></button>
            <button
              onClick={e => { e.stopPropagation(); onAnalyze(article); }}
              disabled={isAnalyzing}
              style={{
                padding: '2px 6px', fontSize: '7px', fontWeight: 700,
                backgroundColor: isAnalyzing ? `${C.PURPLE}15` : `${C.AMBER}15`,
                border: `1px solid ${isAnalyzing ? C.PURPLE : C.AMBER}40`,
                color: isAnalyzing ? C.PURPLE : C.AMBER,
                cursor: isAnalyzing ? 'not-allowed' : 'pointer',
                borderRadius: '2px', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px',
              }}
            ><Zap size={8} />{isAnalyzing ? 'WAIT' : 'AI'}</button>
          </>
        )}
      </div>
    </div>
  );
};
