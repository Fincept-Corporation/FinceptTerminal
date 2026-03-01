/**
 * LiveNewsPanel.tsx — Live TV stream player
 * Bloomberg Terminal Design — Embedded live news channels with HLS/YouTube proxy
 */
import React, { useEffect, useRef, useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { Play, Volume2, VolumeX, RotateCcw, ExternalLink } from 'lucide-react';

// ── Bloomberg-style design tokens (matching NewsTab.tsx) ─────────────────────
const C = {
  BG: '#000000', SURFACE: '#080808', PANEL: '#0D0D0D',
  RAISED: '#141414', TOOLBAR: '#1A1A1A',
  BORDER: '#1E1E1E', DIVIDER: '#2A2A2A',
  TEXT_MUTE: '#555555', TEXT_DIM: '#888888',
  AMBER: '#FF8800', BLUE: '#4DA6FF', CYAN: '#00D4AA',
  RED: '#E55A5A', WHITE: '#FFFFFF',
} as const;
const FONT = '"IBM Plex Mono", "SF Mono", "Consolas", monospace';

export interface LiveChannel {
  id: string; name: string; handle: string;
  fallbackVideoId: string; hlsUrl?: string;
}

const DIRECT_HLS: Record<string, string> = {
  'dw':         'https://dwamdstream103.akamaized.net/hls/live/2015526/dwstream103/master.m3u8',
  'france24':   'https://amg00106-france24-france24-samsunguk-qvpp8.amagi.tv/playlist/amg00106-france24-france24-samsunguk/playlist.m3u8',
  'sky':        'https://linear901-oo-hls0-prd-gtm.delivery.skycdp.com/17501/sde-fast-skynews/master.m3u8',
  'cbs-news':   'https://cbsn-us.cbsnstream.cbsnews.com/out/v1/55a8648e8f134e82a470f83d562deeca/master.m3u8',
  'trt-world':  'https://tv-trtworld.medya.trt.com.tr/master.m3u8',
  'al-hadath':  'https://av.alarabiya.net/alarabiapublish/alhadath.smil/playlist.m3u8',
  'sky-arabia': 'https://live-stream.skynewsarabia.com/c-horizontal-channel/horizontal-stream/index.m3u8',
  'alarabiya':  'https://live.alarabiya.net/alarabiapublish/alarabiya.smil/playlist.m3u8',
};

export const DEFAULT_CHANNELS: LiveChannel[] = [
  { id: 'bloomberg',  name: 'Bloomberg',   handle: '@markets',          fallbackVideoId: 'iEpJwprxDdk' },
  { id: 'cnbc',       name: 'CNBC',        handle: '@CNBC',             fallbackVideoId: '9NyxcX3rhQs' },
  { id: 'sky',        name: 'Sky News',    handle: '@SkyNews',          fallbackVideoId: 'uvviIF4725I', hlsUrl: DIRECT_HLS['sky'] },
  { id: 'dw',         name: 'DW News',     handle: '@DWNews',           fallbackVideoId: 'LuKwFajn37U', hlsUrl: DIRECT_HLS['dw'] },
  { id: 'france24',   name: 'France 24',   handle: '@France24_en',      fallbackVideoId: 'Ap-UM1O9RBU', hlsUrl: DIRECT_HLS['france24'] },
  { id: 'aljazeera',  name: 'Al Jazeera',  handle: '@AlJazeeraEnglish', fallbackVideoId: 'gCNeDWCI0vo' },
  { id: 'alarabiya',  name: 'Al Arabiya',  handle: '@AlArabiya',        fallbackVideoId: 'n7eQejkXbnM', hlsUrl: DIRECT_HLS['alarabiya'] },
  { id: 'cbs-news',   name: 'CBS News',    handle: '@CBSNews',          fallbackVideoId: 'R9L8sDK8iEc', hlsUrl: DIRECT_HLS['cbs-news'] },
  { id: 'trt-world',  name: 'TRT World',   handle: '@TRTWorld',         fallbackVideoId: 'ABfFhWzWs0s', hlsUrl: DIRECT_HLS['trt-world'] },
  { id: 'euronews',   name: 'Euronews',    handle: '@euronews',         fallbackVideoId: 'pykpO5kQJ98' },
  { id: 'wion',       name: 'WION',        handle: '@WION',             fallbackVideoId: '7n7wH8PnOQE' },
  { id: 'nhk',        name: 'NHK World',   handle: '@NHKWORLDJAPAN',    fallbackVideoId: 'OpGjrfMFaSI' },
  { id: 'cna',        name: 'CNA',         handle: '@channelnewsasia',  fallbackVideoId: 'XWq5kBlakcQ' },
  { id: 'al-hadath',  name: 'Al Hadath',   handle: '@AlHadath',         fallbackVideoId: 'xWXpl7azI8k', hlsUrl: DIRECT_HLS['al-hadath'] },
  { id: 'sky-arabia', name: 'Sky Arabia',  handle: '@skynewsarabia',    fallbackVideoId: 'U--OjmpjF5o', hlsUrl: DIRECT_HLS['sky-arabia'] },
];

const STORAGE_KEY = 'fincept_live_channels_order';
function loadOrder(): string[] {
  try { const r = localStorage.getItem(STORAGE_KEY); if (r) return JSON.parse(r); } catch {}
  return DEFAULT_CHANNELS.map(c => c.id);
}
function getChannels(): LiveChannel[] {
  const order = loadOrder();
  const map = new Map(DEFAULT_CHANNELS.map(c => [c.id, c]));
  const result: LiveChannel[] = [];
  for (const id of order) { const ch = map.get(id); if (ch) result.push(ch); }
  for (const ch of DEFAULT_CHANNELS) { if (!order.includes(ch.id)) result.push(ch); }
  return result;
}

type PlayerState = 'idle' | 'loading' | 'playing' | 'error';

interface Props { colors: Record<string, string>; }

export const LiveNewsPanel: React.FC<Props> = () => {
  const [channels] = useState<LiveChannel[]>(getChannels);
  const [activeId, setActiveId] = useState<string>(channels[0]?.id ?? '');
  const [muted, setMuted] = useState(true);
  const [playerState, setPlayerState] = useState<PlayerState>('idle');
  const [embedPort, setEmbedPort] = useState(0);
  const [errorMsg, setErrorMsg] = useState('');

  const iframeRef = useRef<HTMLIFrameElement>(null);
  const videoRef = useRef<HTMLVideoElement>(null);
  const embedOriginRef = useRef('');
  const activeCh = channels.find(c => c.id === activeId) ?? channels[0];

  useEffect(() => {
    invoke<number>('get_embed_port')
      .then(port => { setEmbedPort(port); embedOriginRef.current = `http://127.0.0.1:${port}`; })
      .catch(() => setEmbedPort(0));
  }, []);

  useEffect(() => {
    const handler = (e: MessageEvent) => {
      if (!embedOriginRef.current || e.origin !== embedOriginRef.current) return;
      const msg = e.data as { type: string; code?: number; muted?: boolean };
      if (!msg?.type) return;
      if (msg.type === 'yt-ready') { setPlayerState('playing'); setErrorMsg(''); }
      else if (msg.type === 'yt-error') { setPlayerState('error'); setErrorMsg(`YouTube error ${msg.code ?? ''}`); }
      else if (msg.type === 'yt-mute-state' && typeof msg.muted === 'boolean') setMuted(msg.muted);
    };
    window.addEventListener('message', handler);
    return () => window.removeEventListener('message', handler);
  }, []);

  const postEmbed = useCallback((msg: Record<string, unknown>) => {
    iframeRef.current?.contentWindow?.postMessage(msg, embedOriginRef.current);
  }, []);

  const toggleMute = () => {
    const next = !muted; setMuted(next);
    postEmbed({ type: next ? 'mute' : 'unmute' });
    if (videoRef.current) videoRef.current.muted = next;
  };

  const switchCh = (ch: LiveChannel) => {
    if (ch.id === activeId) return;
    setActiveId(ch.id); setPlayerState('loading'); setErrorMsg('');
  };

  const embedSrc = (ch: LiveChannel): string => {
    if (!embedPort) return '';
    return `http://127.0.0.1:${embedPort}/yt-embed?videoId=${ch.fallbackVideoId}&autoplay=1&mute=${muted ? '1' : '0'}`;
  };

  const btn = (active = false): React.CSSProperties => ({
    padding: '2px 7px', fontSize: '8px', fontWeight: 700,
    backgroundColor: active ? `${C.AMBER}15` : 'transparent',
    border: `1px solid ${active ? C.AMBER : C.BORDER}`,
    color: active ? C.AMBER : C.TEXT_MUTE,
    cursor: 'pointer', borderRadius: '2px', fontFamily: FONT,
    display: 'flex', alignItems: 'center', gap: '3px',
    letterSpacing: '0.3px',
  });

  return (
    <div style={{ display: 'flex', flexDirection: 'column', flexShrink: 0, borderBottom: `1px solid ${C.BORDER}`, fontFamily: FONT }}>
      {/* Control bar */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '4px',
        padding: '4px 10px', backgroundColor: C.TOOLBAR,
        borderBottom: `1px solid ${C.BORDER}`, flexWrap: 'wrap',
      }}>
        <span style={{ fontSize: '8px', fontWeight: 700, color: C.RED, letterSpacing: '0.5px' }}>● LIVE</span>

        {/* Channel tabs — scrollable row */}
        <div style={{
          display: 'flex', gap: '2px', flex: 1, overflowX: 'auto',
          scrollbarWidth: 'none', msOverflowStyle: 'none',
        }}>
          {channels.map(ch => (
            <button key={ch.id} onClick={() => switchCh(ch)} style={{
              padding: '2px 7px', fontSize: '8px',
              fontWeight: ch.id === activeId ? 700 : 400,
              backgroundColor: ch.id === activeId ? `${C.AMBER}20` : 'transparent',
              border: `1px solid ${ch.id === activeId ? C.AMBER : 'transparent'}`,
              color: ch.id === activeId ? C.AMBER : C.TEXT_MUTE,
              cursor: 'pointer', borderRadius: '2px', whiteSpace: 'nowrap', fontFamily: FONT,
            }}>{ch.name}</button>
          ))}
        </div>

        {/* Mute */}
        <button onClick={toggleMute} style={btn()} title={muted ? 'Unmute' : 'Mute'}>
          {muted ? <VolumeX size={10} /> : <Volume2 size={10} />}
        </button>
      </div>

      {/* Player */}
      <div style={{ height: 240, position: 'relative', background: C.BG, flexShrink: 0 }}>
        {playerState === 'idle' && (
          <Overlay>
            <button onClick={() => setPlayerState('loading')} style={{
              padding: '8px 24px', fontSize: '9px', fontWeight: 700,
              backgroundColor: `${C.AMBER}15`, border: `1px solid ${C.AMBER}`,
              color: C.AMBER, borderRadius: '2px', cursor: 'pointer', fontFamily: FONT,
              display: 'flex', alignItems: 'center', gap: '6px',
            }}><Play size={12} />LOAD {activeCh?.name?.toUpperCase()}</button>
          </Overlay>
        )}

        {playerState === 'error' && (
          <Overlay>
            <div style={{ color: C.RED, fontSize: '9px', marginBottom: '8px', textAlign: 'center', maxWidth: 260 }}>{errorMsg}</div>
            {activeCh?.fallbackVideoId && (
              <a href={`https://www.youtube.com/watch?v=${activeCh.fallbackVideoId}`}
                target="_blank" rel="noopener noreferrer"
                style={{ color: C.CYAN, fontSize: '9px', display: 'flex', alignItems: 'center', gap: '4px', textDecoration: 'none' }}>
                <ExternalLink size={10} />Open on YouTube
              </a>
            )}
            <button onClick={() => { setPlayerState('loading'); setErrorMsg(''); }}
              style={{ ...btn(), marginTop: '8px' }}><RotateCcw size={9} />RETRY</button>
          </Overlay>
        )}

        {(playerState === 'loading' || playerState === 'playing') && activeCh?.hlsUrl && (
          <video key={`hls-${activeId}`} ref={videoRef}
            src={activeCh.hlsUrl} autoPlay muted={muted} playsInline controls
            style={{ width: '100%', height: '100%', objectFit: 'contain', background: C.BG }}
            onPlay={() => setPlayerState('playing')}
            onError={() => { videoRef.current?.removeAttribute('src'); setPlayerState('loading'); }}
          />
        )}

        {(playerState === 'loading' || playerState === 'playing') && !activeCh?.hlsUrl && embedPort > 0 && (
          <iframe key={`yt-${activeId}-${embedPort}`} ref={iframeRef}
            src={embedSrc(activeCh!)}
            style={{ width: '100%', height: '100%', border: 'none', background: C.BG }}
            allow="autoplay; encrypted-media; picture-in-picture; fullscreen"
            allowFullScreen referrerPolicy="strict-origin-when-cross-origin"
            title={`${activeCh?.name} live`}
          />
        )}

        {(playerState === 'loading' || playerState === 'playing') && !activeCh?.hlsUrl && embedPort === 0 && (
          <Overlay><span style={{ color: C.TEXT_MUTE, fontSize: '9px' }}>INITIALIZING...</span></Overlay>
        )}

        {playerState === 'loading' && activeCh?.hlsUrl && (
          <Overlay style={{ pointerEvents: 'none' }}>
            <span style={{ color: C.TEXT_MUTE, fontSize: '9px' }}>CONNECTING...</span>
          </Overlay>
        )}
      </div>
    </div>
  );
};

// Small overlay helper
const Overlay: React.FC<{ children: React.ReactNode; style?: React.CSSProperties }> = ({ children, style }) => (
  <div style={{
    position: 'absolute', inset: 0, display: 'flex', flexDirection: 'column',
    alignItems: 'center', justifyContent: 'center', background: 'rgba(0,0,0,0.92)', zIndex: 2,
    ...style,
  }}>{children}</div>
);
