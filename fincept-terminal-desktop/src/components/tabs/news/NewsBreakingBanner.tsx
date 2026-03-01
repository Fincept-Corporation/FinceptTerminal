/**
 * NewsBreakingBanner.tsx — Breaking news alert strip
 * Bloomberg Terminal Design — Flash/urgent alert banner with auto-dismiss
 */
import React, { useEffect, useState } from 'react';
import { AlertTriangle, X } from 'lucide-react';
import { NewsCluster } from '@/services/news/newsClusterService';

// ── Bloomberg-style design tokens (matching NewsTab.tsx) ─────────────────────
const C = {
  BG: '#000000', BORDER: '#1E1E1E',
  TEXT_MUTE: '#555555', TEXT_DIM: '#888888',
  AMBER: '#FF8800', RED: '#E55A5A', YELLOW: '#E8B634',
  CYAN: '#00D4AA', WHITE: '#FFFFFF',
} as const;
const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';

interface Props {
  cluster: NewsCluster;
  onDismiss: (clusterId: string) => void;
  onScrollTo: (clusterId: string) => void;
  colors: Record<string, string>;
}

export const NewsBreakingBanner: React.FC<Props> = ({ cluster, onDismiss, onScrollTo }) => {
  const [visible, setVisible] = useState(true);

  useEffect(() => {
    const timer = setTimeout(() => { setVisible(false); onDismiss(cluster.id); }, 60_000);
    return () => clearTimeout(timer);
  }, [cluster.id, onDismiss]);

  if (!visible) return null;

  const isFlash = cluster.leadArticle.priority === 'FLASH';
  const ac = isFlash ? C.RED : C.AMBER;

  return (
    <div
      onClick={() => onScrollTo(cluster.id)}
      style={{
        display: 'flex', alignItems: 'center', gap: '8px',
        padding: '5px 12px',
        backgroundColor: `${ac}10`,
        borderLeft: `3px solid ${ac}`,
        borderBottom: `1px solid ${C.BORDER}`,
        cursor: 'pointer', userSelect: 'none', flexShrink: 0,
        fontFamily: FONT,
      }}
    >
      <style>{`@keyframes ntBanner{0%,100%{opacity:1}50%{opacity:0.3}}`}</style>

      <AlertTriangle size={11} style={{ color: ac, animation: 'ntBanner 1s step-start infinite', flexShrink: 0 }} />

      <span style={{
        fontSize: '8px', fontWeight: 700, padding: '2px 6px',
        backgroundColor: ac, color: C.BG, borderRadius: '2px',
        letterSpacing: '0.5px', flexShrink: 0,
      }}>{cluster.leadArticle.priority}</span>

      <span style={{
        fontSize: '8px', fontWeight: 700, color: C.TEXT_DIM, flexShrink: 0,
      }}>[{cluster.category}]</span>

      <span style={{
        fontSize: '10px', fontWeight: 600, color: C.WHITE,
        overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', flex: 1,
      }}>
        {cluster.leadArticle.headline.substring(0, 100)}
        {cluster.leadArticle.headline.length > 100 ? '...' : ''}
      </span>

      {cluster.sourceCount > 1 && (
        <span style={{
          fontSize: '7px', fontWeight: 700, padding: '1px 5px',
          backgroundColor: `${ac}25`, color: ac, borderRadius: '2px', flexShrink: 0,
        }}>{cluster.sourceCount} SRC</span>
      )}

      <span style={{ fontSize: '8px', color: C.TEXT_MUTE, flexShrink: 0 }}>{cluster.latestTime}</span>

      <button
        onClick={e => { e.stopPropagation(); setVisible(false); onDismiss(cluster.id); }}
        style={{
          background: 'none', border: 'none', color: C.TEXT_MUTE,
          cursor: 'pointer', padding: '0 2px', display: 'flex', flexShrink: 0,
        }}
      ><X size={11} /></button>
    </div>
  );
};
