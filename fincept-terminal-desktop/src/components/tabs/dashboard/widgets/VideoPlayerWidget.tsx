import React, { useEffect, useRef, useState } from 'react';
import { Play, Settings } from 'lucide-react';
import { BaseWidget } from './BaseWidget';

interface VideoPlayerWidgetProps {
  id: string;
  videoUrl?: string;
  videoTitle?: string;
  onRemove?: () => void;
  onConfigure?: () => void;
}

interface PresetChannel {
  name: string;
  label: string;
  url: string;
  type: 'youtube' | 'hls' | 'mp4';
}

const PRESET_CHANNELS: PresetChannel[] = [
  {
    name: 'Bloomberg TV',
    label: 'BLOOMBERG',
    url: 'https://www.youtube.com/watch?v=dp8PhLsUcFE',
    type: 'youtube',
  },
  {
    name: 'CNBC Live',
    label: 'CNBC',
    url: 'https://www.youtube.com/watch?v=9ApRjK3RVSY',
    type: 'youtube',
  },
  {
    name: 'Reuters TV',
    label: 'REUTERS',
    url: 'https://www.youtube.com/watch?v=vAvNMaWlkrc',
    type: 'youtube',
  },
  {
    name: 'Yahoo Finance',
    label: 'YAHOO FINANCE',
    url: 'https://www.youtube.com/watch?v=WUm7b7GiTFM',
    type: 'youtube',
  },
  {
    name: 'Fox Business',
    label: 'FOX BUSINESS',
    url: 'https://www.youtube.com/watch?v=6IlWB5R_Oso',
    type: 'youtube',
  },
];

function getYouTubeEmbedUrl(url: string): string | null {
  const patterns = [
    /(?:youtube\.com\/watch\?v=|youtu\.be\/)([A-Za-z0-9_-]{11})/,
    /youtube\.com\/live\/([A-Za-z0-9_-]{11})/,
    /youtube\.com\/embed\/([A-Za-z0-9_-]{11})/,
  ];
  for (const pattern of patterns) {
    const match = url.match(pattern);
    if (match) return `https://www.youtube.com/embed/${match[1]}?autoplay=1&mute=1`;
  }
  return null;
}

function isYouTubeUrl(url: string): boolean {
  return url.includes('youtube.com') || url.includes('youtu.be');
}

function isHlsUrl(url: string): boolean {
  return url.includes('.m3u8');
}

export const VideoPlayerWidget: React.FC<VideoPlayerWidgetProps> = ({
  id,
  videoUrl = '',
  videoTitle = 'Video Player',
  onRemove,
  onConfigure,
}) => {
  const playerContainerRef = useRef<HTMLDivElement>(null);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  const playerInstanceRef = useRef<any>(null);
  const [playerError, setPlayerError] = useState<string | null>(null);

  const trimmedUrl = videoUrl.trim();
  const isYouTube = trimmedUrl ? isYouTubeUrl(trimmedUrl) : false;
  const isHls = trimmedUrl ? isHlsUrl(trimmedUrl) : false;
  const useXgplayer = trimmedUrl && !isYouTube;

  // Initialize xgplayer for HLS/MP4 URLs
  useEffect(() => {
    if (!useXgplayer || !playerContainerRef.current) return;

    let destroyed = false;

    const initPlayer = async () => {
      try {
        // Dynamically import xgplayer to avoid SSR issues
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        const { default: Player } = await import('xgplayer') as { default: any };

        const playerConfig: Record<string, unknown> = {
          el: playerContainerRef.current,
          url: trimmedUrl,
          width: '100%',
          height: '100%',
          autoplay: false,
          volume: 0.5,
          lang: 'en',
          fluid: true,
        };

        if (isHls) {
          // eslint-disable-next-line @typescript-eslint/no-explicit-any
          const { default: HlsPlugin } = await import('xgplayer-hls') as { default: any };
          playerConfig.plugins = [HlsPlugin];
        }

        if (destroyed) return;

        playerInstanceRef.current = new Player(playerConfig);
      } catch (err: unknown) {
        if (!destroyed) {
          setPlayerError(err instanceof Error ? err.message : 'Failed to load player');
        }
      }
    };

    setPlayerError(null);
    initPlayer();

    return () => {
      destroyed = true;
      if (playerInstanceRef.current) {
        try {
          playerInstanceRef.current.destroy();
        } catch {
          /* ignore destroy errors */
        }
        playerInstanceRef.current = null;
      }
    };
  }, [trimmedUrl, useXgplayer, isHls]);

  const renderContent = () => {
    // No URL configured — show preset channels
    if (!trimmedUrl) {
      return (
        <div style={{ padding: '8px', height: '100%', overflowY: 'auto' }}>
          <div style={{
            fontSize: '9px',
            color: '#787878',
            marginBottom: '10px',
            textTransform: 'uppercase',
            letterSpacing: '1px',
          }}>
            SELECT A CHANNEL OR CONFIGURE A CUSTOM URL
          </div>
          {PRESET_CHANNELS.map((ch) => (
            <div
              key={ch.name}
              onClick={onConfigure}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '8px',
                padding: '8px',
                marginBottom: '4px',
                backgroundColor: '#0F0F0F',
                border: '1px solid #2A2A2A',
                borderRadius: '2px',
                cursor: 'pointer',
                transition: 'border-color 0.15s',
              }}
              onMouseEnter={(e) => { (e.currentTarget as HTMLDivElement).style.borderColor = '#FF8800'; }}
              onMouseLeave={(e) => { (e.currentTarget as HTMLDivElement).style.borderColor = '#2A2A2A'; }}
            >
              <Play size={10} color="#FF8800" />
              <div>
                <div style={{ fontSize: '10px', color: '#FFFFFF', fontWeight: 'bold', fontFamily: 'monospace' }}>
                  {ch.label}
                </div>
                <div style={{ fontSize: '9px', color: '#787878' }}>{ch.name}</div>
              </div>
            </div>
          ))}
          <div
            onClick={onConfigure}
            style={{
              marginTop: '8px',
              padding: '8px',
              border: '1px dashed #2A2A2A',
              borderRadius: '2px',
              textAlign: 'center',
              cursor: 'pointer',
              fontSize: '9px',
              color: '#787878',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px',
            }}
            onMouseEnter={(e) => { (e.currentTarget as HTMLDivElement).style.borderColor = '#FF8800'; (e.currentTarget as HTMLDivElement).style.color = '#FF8800'; }}
            onMouseLeave={(e) => { (e.currentTarget as HTMLDivElement).style.borderColor = '#2A2A2A'; (e.currentTarget as HTMLDivElement).style.color = '#787878'; }}
          >
            <Settings size={10} /> CONFIGURE CUSTOM URL
          </div>
        </div>
      );
    }

    // YouTube — use iframe embed
    if (isYouTube) {
      const embedUrl = getYouTubeEmbedUrl(trimmedUrl);
      if (!embedUrl) {
        return (
          <div style={{ padding: '16px', textAlign: 'center', color: '#FF3B3B', fontSize: '11px' }}>
            Invalid YouTube URL
          </div>
        );
      }
      return (
        <iframe
          src={embedUrl}
          style={{ width: '100%', height: '100%', border: 'none', display: 'block' }}
          allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
          allowFullScreen
          title={videoTitle}
        />
      );
    }

    // HLS / MP4 — xgplayer container
    if (playerError) {
      return (
        <div style={{ padding: '16px', textAlign: 'center', color: '#FF3B3B', fontSize: '11px' }}>
          {playerError}
        </div>
      );
    }

    return (
      <div
        ref={playerContainerRef}
        style={{ width: '100%', height: '100%' }}
      />
    );
  };

  return (
    <BaseWidget
      id={id}
      title={videoTitle.toUpperCase() || 'VIDEO PLAYER'}
      onRemove={onRemove}
      onConfigure={onConfigure}
      headerColor="#9D4EDD"
    >
      <div style={{ width: '100%', height: 'calc(100% - 32px)', overflow: 'hidden' }}>
        {renderContent()}
      </div>
    </BaseWidget>
  );
};
