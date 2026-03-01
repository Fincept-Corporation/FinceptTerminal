/**
 * NewsMapPanel.tsx — Globe/map intel overlay
 * Bloomberg Terminal Design — Compact intelligence map with event markers
 */
import React, { useEffect, useRef, useState } from 'react';
import { Globe, Map, Eye, EyeOff, Crosshair, ChevronUp, ChevronDown } from 'lucide-react';
import { generateMapHTML } from '@/components/tabs/maritime/utils/mapHtmlGenerator';
import { NewsMarker } from '@/services/news/newsClusterService';

// ── Bloomberg-style design tokens (matching NewsTab.tsx) ─────────────────────
const C = {
  BG: '#000000', TOOLBAR: '#1A1A1A',
  BORDER: '#1E1E1E',
  TEXT_MUTE: '#555555', TEXT_DIM: '#888888',
  AMBER: '#FF8800', BLUE: '#4DA6FF', CYAN: '#00D4AA',
} as const;
const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';

interface Props {
  markers: NewsMarker[];
  onMarkerClick: (clusterId: string) => void;
  colors: Record<string, string>;
}

export const NewsMapPanel: React.FC<Props> = ({ markers, onMarkerClick }) => {
  const iframeRef = useRef<HTMLIFrameElement>(null);
  const [collapsed, setCollapsed] = useState(false);
  const [markersVisible, setMarkersVisible] = useState(true);
  const [mapMode, setMapMode] = useState<'3D' | '2D'>('3D');
  const mapHtml = useRef<string>(generateMapHTML());

  useEffect(() => {
    const iframe = iframeRef.current;
    if (!iframe || collapsed) return;
    const send = () => {
      try {
        const w = iframe.contentWindow as (Window & {
          updateNewsMarkers?: (m: NewsMarker[]) => void;
          onNewsMarkerClick?: ((id: string) => void) | null;
        }) | null;
        if (w?.updateNewsMarkers) { w.updateNewsMarkers(markers); w.onNewsMarkerClick = onMarkerClick; }
      } catch { /* iframe not ready */ }
    };
    const t = setTimeout(send, 600);
    return () => clearTimeout(t);
  }, [markers, collapsed, onMarkerClick]);

  const toggleMarkers = () => {
    setMarkersVisible(v => !v);
    try { (iframeRef.current?.contentWindow as Window & { toggleNewsMarkers?: () => void })?.toggleNewsMarkers?.(); } catch {}
  };

  const switchMode = (m: '3D' | '2D') => {
    setMapMode(m);
    try { (iframeRef.current?.contentWindow as Window & { switchMapMode?: (m: string) => void })?.switchMapMode?.(m); } catch {}
  };

  const zoomHotspot = () => {
    const hot = markers.find(m => m.priority === 'FLASH' || m.priority === 'URGENT');
    if (!hot) return;
    try { (iframeRef.current?.contentWindow as Window & { zoomToNewsMarker?: (id: string) => void })?.zoomToNewsMarker?.(hot.clusterId); } catch {}
  };

  const btn = (active: boolean): React.CSSProperties => ({
    padding: '2px 7px', fontSize: '8px', fontWeight: 700,
    backgroundColor: active ? `${C.CYAN}15` : 'transparent',
    border: `1px solid ${active ? C.CYAN : C.BORDER}`,
    color: active ? C.CYAN : C.TEXT_MUTE,
    cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
    display: 'flex', alignItems: 'center', gap: '3px',
    letterSpacing: '0.3px',
  });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', flexShrink: 0 }}>
      {/* Controls */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '4px',
        padding: '4px 10px',
        backgroundColor: C.TOOLBAR, borderBottom: `1px solid ${C.BORDER}`,
        fontFamily: FONT, flexShrink: 0,
      }}>
        <span style={{ fontSize: '8px', fontWeight: 700, color: C.CYAN, letterSpacing: '1px' }}>INTEL MAP</span>
        <span style={{ fontSize: '8px', color: C.TEXT_MUTE }}>{markers.length} EVT</span>
        <div style={{ flex: 1 }} />

        <button onClick={() => switchMode('3D')} style={btn(mapMode === '3D')}><Globe size={9} />3D</button>
        <button onClick={() => switchMode('2D')} style={btn(mapMode === '2D')}><Map size={9} />2D</button>
        <button onClick={toggleMarkers} style={btn(markersVisible)}>
          {markersVisible ? <Eye size={9} /> : <EyeOff size={9} />}MKR
        </button>
        <button onClick={zoomHotspot} style={btn(false)}><Crosshair size={9} />HOT</button>
        <button onClick={() => setCollapsed(c => !c)} style={btn(false)}>
          {collapsed ? <ChevronDown size={9} /> : <ChevronUp size={9} />}
        </button>
      </div>

      {!collapsed && (
        <div style={{ height: '260px', position: 'relative', flexShrink: 0 }}>
          <iframe
            ref={iframeRef}
            srcDoc={mapHtml.current}
            style={{ width: '100%', height: '100%', border: 'none', background: C.BG }}
            title="News Intelligence Map"
          />
        </div>
      )}
    </div>
  );
};
