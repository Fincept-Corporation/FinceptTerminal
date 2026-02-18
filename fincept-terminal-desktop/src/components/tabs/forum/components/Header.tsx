// Forum Tab Header Component

import React from 'react';
import { useTranslation } from 'react-i18next';
import type { ForumColors, TrendingTopic } from '../types';
import { useTimezone } from '@/contexts/TimezoneContext';

interface HeaderProps {
  colors: ForumColors;
  currentTime: Date;
  onlineUsers: number;
  postsToday: number;
  trendingTopics: TrendingTopic[];
  isRefreshing: boolean;
  lastRefreshTime: Date | null;
  onRefresh: () => void;
}

export const Header: React.FC<HeaderProps> = ({
  colors,
  currentTime,
  onlineUsers,
  postsToday,
  trendingTopics,
  isRefreshing,
  lastRefreshTime,
  onRefresh
}) => {
  const { t } = useTranslation('forum');
  const { timezone, formatTime } = useTimezone();

  return (
    <div style={{
      backgroundColor: colors.PANEL_BG,
      borderBottom: `1px solid ${colors.GRAY}`,
      padding: '4px 8px',
      flexShrink: 0
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '13px', marginBottom: '2px', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ color: colors.ORANGE, fontWeight: 'bold' }}>{t('title')}</span>
          <span style={{ color: colors.WHITE }}>|</span>
          <span style={{ color: colors.GREEN }}>● LIVE</span>
          <span style={{ color: colors.WHITE }}>|</span>
          <span style={{ color: colors.YELLOW }}>USERS ONLINE: {onlineUsers.toLocaleString()}</span>
          <span style={{ color: colors.WHITE }}>|</span>
          <span style={{ color: colors.CYAN }}>POSTS TODAY: {postsToday.toLocaleString()}</span>
          <span style={{ color: colors.WHITE }}>|</span>
          <span style={{ color: colors.WHITE }}>
            {formatTime(currentTime, {
              year: 'numeric', month: '2-digit', day: '2-digit',
              hour: '2-digit', minute: '2-digit', second: '2-digit'
            })} {timezone.shortLabel}
          </span>
        </div>
        <button
          onClick={onRefresh}
          disabled={isRefreshing}
          style={{
            backgroundColor: isRefreshing ? colors.GRAY : colors.ORANGE,
            color: colors.DARK_BG,
            border: 'none',
            padding: '2px 8px',
            fontSize: '10px',
            fontWeight: 'bold',
            cursor: isRefreshing ? 'not-allowed' : 'pointer',
            opacity: isRefreshing ? 0.6 : 1
          }}
          title={lastRefreshTime ? `Last refreshed: ${lastRefreshTime.toLocaleTimeString()}` : 'Refresh forum data'}
        >
          {isRefreshing ? '⟳ REFRESHING...' : '⟳ REFRESH'}
        </button>
      </div>

      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '11px' }}>
        <span style={{ color: colors.GRAY }}>{t('header.trending')}:</span>
        {trendingTopics.slice(0, 5).map((topic, i) => (
          <span key={i} style={{ color: i % 2 === 0 ? colors.RED : i % 3 === 0 ? colors.CYAN : colors.PURPLE }}>
            {topic.topic.replace('#', '').toUpperCase()}
          </span>
        ))}
        <span style={{ color: colors.WHITE }}>|</span>
        <span style={{ color: colors.GRAY }}>SENTIMENT:</span>
        <span style={{ color: colors.GREEN }}>BULLISH 68%</span>
        <span style={{ color: colors.RED }}>BEARISH 18%</span>
        <span style={{ color: colors.YELLOW }}>NEUTRAL 14%</span>
      </div>
    </div>
  );
};
