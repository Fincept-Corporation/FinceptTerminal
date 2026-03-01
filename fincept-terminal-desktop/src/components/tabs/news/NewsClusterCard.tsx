/**
 * NewsClusterCard.tsx — Clustered story card
 * Bloomberg Terminal Design — Dense cluster display with expandable related articles
 */
import React, { useState } from 'react';
import { TrendingUp, TrendingDown, Minus, ExternalLink, Copy, Zap } from 'lucide-react';
import { NewsCluster } from '@/services/news/newsClusterService';
import { NewsArticle } from '@/services/news/newsService';

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

const TIER_COLORS: Record<number, string> = { 1: C.CYAN, 2: C.GREEN, 3: C.YELLOW, 4: C.TEXT_MUTE };
const PRI_COLORS: Record<string, string> = { FLASH: C.RED, URGENT: C.AMBER, BREAKING: C.YELLOW, ROUTINE: C.TEXT_MUTE };
const SENT_COLORS: Record<string, string> = { BULLISH: C.GREEN, BEARISH: C.RED, NEUTRAL: C.YELLOW };
const VEL_COLORS: Record<string, string> = { rising: C.GREEN, stable: C.TEXT_MUTE, falling: C.RED };

function relTime(sortTs: number): string {
  if (!sortTs) return '';
  const d = Math.floor(Date.now() / 1000) - sortTs;
  if (d < 60) return `${d}s`;
  if (d < 3600) return `${Math.floor(d / 60)}m`;
  if (d < 86400) return `${Math.floor(d / 3600)}h`;
  return `${Math.floor(d / 86400)}d`;
}

interface Props {
  cluster: NewsCluster;
  isHighlighted?: boolean;
  onAnalyze: (article: NewsArticle) => void;
  colors: Record<string, string>;
}

export const NewsClusterCard: React.FC<Props> = ({ cluster, isHighlighted = false, onAnalyze }) => {
  const [expanded, setExpanded] = useState(false);
  const [hovered, setHovered] = useState(false);
  const { leadArticle, articles, sourceCount, velocity, sentiment, tier, isBreaking } = cluster;

  const tierColor = TIER_COLORS[tier] ?? TIER_COLORS[4];
  const pc = PRI_COLORS[leadArticle.priority] ?? PRI_COLORS.ROUTINE;
  const sc = SENT_COLORS[sentiment] ?? C.TEXT_MUTE;
  const vc = VEL_COLORS[velocity] ?? C.TEXT_MUTE;
  const related = articles.filter(a => a.id !== leadArticle.id);
  const sources = [...new Set(articles.map(a => a.source))];

  return (
    <div
      id={`cluster-${cluster.id}`}
      onMouseEnter={() => setHovered(true)}
      onMouseLeave={() => setHovered(false)}
      style={{
        backgroundColor: isHighlighted ? `${pc}08` : hovered ? C.RAISED : C.BG,
        borderBottom: `1px solid ${C.BORDER}`,
        borderLeft: `2px solid ${isBreaking ? pc : tierColor}`,
        padding: '6px 10px',
        transition: 'background 0.08s',
        fontFamily: FONT,
      }}
    >
      {/* Row 1: sources + velocity + time */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '3px', flexWrap: 'wrap' }}>
        <span style={{ fontSize: '8px', color: C.CYAN, fontWeight: 700 }}>
          {sources.slice(0, 3).join(' / ')}
          {sources.length > 3 && ` +${sources.length - 3}`}
        </span>
        <div style={{ flex: 1 }} />
        <span style={{
          fontSize: '7px', fontWeight: 700, color: vc,
          display: 'flex', alignItems: 'center', gap: '2px',
        }}>
          {velocity === 'rising' ? <TrendingUp size={8} /> : velocity === 'falling' ? <TrendingDown size={8} /> : <Minus size={8} />}
          {velocity.toUpperCase()}
        </span>
        <span style={{ fontSize: '8px', color: C.TEXT_MUTE }}>{relTime(cluster.latestSortTs)}</span>
      </div>

      {/* Row 2: badges */}
      <div style={{ display: 'flex', gap: '3px', alignItems: 'center', marginBottom: '4px', flexWrap: 'wrap' }}>
        <span style={{
          fontSize: '7px', fontWeight: 700, padding: '1px 4px',
          backgroundColor: `${tierColor}20`, color: tierColor, borderRadius: '2px',
        }}>T{tier}</span>
        {leadArticle.priority !== 'ROUTINE' && (
          <span style={{
            fontSize: '7px', fontWeight: 700, padding: '1px 5px',
            backgroundColor: pc, color: C.BG, borderRadius: '2px',
          }}>{leadArticle.priority}</span>
        )}
        {sourceCount > 1 && (
          <span style={{
            fontSize: '7px', fontWeight: 700, padding: '1px 5px',
            backgroundColor: `${C.CYAN}15`, color: C.CYAN, borderRadius: '2px',
          }}>{sourceCount} SRC</span>
        )}
        <span style={{
          fontSize: '7px', fontWeight: 700, padding: '1px 5px',
          backgroundColor: `${C.TEXT_MUTE}15`, color: C.TEXT_MUTE, borderRadius: '2px',
        }}>{cluster.category}</span>
        <span style={{
          fontSize: '7px', fontWeight: 700, color: sc,
          display: 'flex', alignItems: 'center', gap: '2px',
        }}>
          {sentiment === 'BULLISH' ? '▲' : sentiment === 'BEARISH' ? '▼' : '–'} {sentiment}
        </span>
      </div>

      {/* Headline */}
      <div
        onClick={() => setExpanded(e => !e)}
        style={{
          color: isBreaking ? C.WHITE : C.TEXT,
          fontSize: '10px', fontWeight: isBreaking ? 700 : 600,
          lineHeight: 1.4, marginBottom: '5px', cursor: 'pointer',
        }}
      >{leadArticle.headline}</div>

      {/* Related articles (expanded) */}
      {expanded && related.length > 0 && (
        <div style={{ borderTop: `1px solid ${C.BORDER}`, paddingTop: '5px', marginBottom: '5px' }}>
          {related.map(a => (
            <div key={a.id} style={{
              display: 'flex', gap: '6px', alignItems: 'baseline',
              padding: '2px 0', borderBottom: `1px solid ${C.BORDER}20`,
              cursor: a.link ? 'pointer' : 'default',
            }} onClick={() => a.link && window.open(a.link, '_blank')}>
              <span style={{ color: C.AMBER, fontSize: '8px' }}>▸</span>
              <span style={{
                color: '#B0B0B0', fontSize: '9px', flex: 1,
                overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
              }}>{a.headline}</span>
              <span style={{ color: C.TEXT_MUTE, fontSize: '8px', flexShrink: 0 }}>
                {a.source} {relTime(a.sort_ts)}
              </span>
            </div>
          ))}
        </div>
      )}

      {/* Bottom: expand toggle + actions */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '5px' }}>
        {related.length > 0 && (
          <button onClick={() => setExpanded(e => !e)} style={{
            background: 'none', border: 'none', color: C.AMBER,
            fontSize: '8px', fontWeight: 700, cursor: 'pointer', padding: 0, fontFamily: FONT,
          }}>
            {expanded ? '▲ HIDE' : `▼ ${related.length} MORE`}
          </button>
        )}
        <div style={{ flex: 1 }} />
        {(hovered || expanded) && (
          <>
            {leadArticle.link && (
              <button onClick={() => window.open(leadArticle.link!, '_blank')} style={{
                padding: '2px 6px', fontSize: '7px', fontWeight: 700,
                backgroundColor: `${C.BLUE}15`, border: `1px solid ${C.BLUE}40`,
                color: C.BLUE, cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
                display: 'flex', alignItems: 'center', gap: '3px',
              }}><ExternalLink size={8} />OPEN</button>
            )}
            {leadArticle.link && (
              <button onClick={() => navigator.clipboard.writeText(leadArticle.link!)} style={{
                padding: '2px 5px', background: 'none',
                border: `1px solid ${C.BORDER}`, color: C.TEXT_MUTE,
                fontSize: '7px', cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
              }}><Copy size={8} /></button>
            )}
            <button onClick={() => onAnalyze(leadArticle)} style={{
              padding: '2px 6px', fontSize: '7px', fontWeight: 700,
              backgroundColor: `${C.AMBER}15`, border: `1px solid ${C.AMBER}40`,
              color: C.AMBER, cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
              display: 'flex', alignItems: 'center', gap: '3px',
            }}><Zap size={8} />ANALYZE</button>
          </>
        )}
      </div>
    </div>
  );
};
