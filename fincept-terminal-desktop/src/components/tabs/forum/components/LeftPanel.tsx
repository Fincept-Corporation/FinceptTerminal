// Forum Tab Left Panel Component (Categories & Stats)

import React from 'react';
import type { ForumColors, ForumCategoryUI, ForumUser } from '../types';
import { ForumStats } from '@/services/forum/forumApi';
import { TOP_CONTRIBUTORS } from '../constants';

interface LeftPanelProps {
  colors: ForumColors;
  categories: ForumCategoryUI[];
  activeCategory: string;
  onCategoryChange: (category: string) => void;
  onViewUserProfile: (username: string) => void;
  forumStats: ForumStats | null;
  totalPosts: number;
  postsToday: number;
  onlineUsers: number;
}

export const LeftPanel: React.FC<LeftPanelProps> = ({
  colors,
  categories,
  activeCategory,
  onCategoryChange,
  onViewUserProfile,
  forumStats,
  totalPosts,
  postsToday,
  onlineUsers
}) => {
  return (
    <div style={{
      width: '280px',
      backgroundColor: colors.PANEL_BG,
      border: `1px solid ${colors.GRAY}`,
      padding: '4px',
      overflow: 'auto'
    }}>
      <div style={{ color: colors.ORANGE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
        FORUM CATEGORIES
      </div>
      <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '4px' }}></div>

      {categories.map((cat, index) => (
        <button
          key={index}
          onClick={() => onCategoryChange(cat.name)}
          style={{
            width: '100%',
            display: 'flex',
            justifyContent: 'space-between',
            padding: '4px',
            marginBottom: '2px',
            backgroundColor: activeCategory === cat.name ? 'rgba(255,165,0,0.1)' : 'transparent',
            border: `1px solid ${activeCategory === cat.name ? colors.ORANGE : 'transparent'}`,
            color: cat.color,
            fontSize: '10px',
            cursor: 'pointer',
            textAlign: 'left'
          }}
        >
          <span>{cat.name}</span>
          <span style={{ color: colors.CYAN }}>{cat.count}</span>
        </button>
      ))}

      <div style={{ borderTop: `1px solid ${colors.GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
        <div style={{ color: colors.YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
          TOP CONTRIBUTORS
        </div>
        {TOP_CONTRIBUTORS.map((user, index) => (
          <div
            key={index}
            onClick={() => onViewUserProfile(user.username)}
            style={{
              padding: '3px',
              marginBottom: '2px',
              backgroundColor: 'rgba(255,255,255,0.02)',
              fontSize: '9px',
              cursor: 'pointer'
            }}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '1px' }}>
              <span style={{ color: colors.WHITE }}>{user.username}</span>
              <span style={{ color: user.status === 'ONLINE' ? colors.GREEN : colors.GRAY }}>
                ●
              </span>
            </div>
            <div style={{ display: 'flex', gap: '6px', color: colors.GRAY }}>
              <span>REP: {user.reputation}</span>
              <span>POSTS: {user.posts}</span>
            </div>
          </div>
        ))}
      </div>

      <div style={{ borderTop: `1px solid ${colors.GRAY}`, marginTop: '8px', paddingTop: '8px' }}>
        <div style={{ color: colors.YELLOW, fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>
          FORUM STATISTICS
        </div>
        <div style={{ fontSize: '9px', lineHeight: '1.4', color: colors.GRAY }}>
          <div>Total Posts: {totalPosts.toLocaleString()}</div>
          <div>Total Users: {(forumStats?.total_comments || 47832).toLocaleString()}</div>
          <div>Posts Today: {postsToday.toLocaleString()}</div>
          <div>Active Now: {onlineUsers}</div>
          <div style={{ color: colors.GREEN }}>Avg Response: 12 min</div>
        </div>
      </div>
    </div>
  );
};
