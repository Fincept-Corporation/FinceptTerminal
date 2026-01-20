// Forum Tab Center Panel Component (Posts List)

import React from 'react';
import type { ForumColors, ForumPost } from '../types';
import { getPriorityColor, getSentimentColor } from '../constants';

interface CenterPanelProps {
  colors: ForumColors;
  activeCategory: string;
  sortBy: string;
  onSortChange: (sort: string) => void;
  filteredPosts: ForumPost[];
  isLoading: boolean;
  onViewPost: (post: ForumPost) => void;
}

export const CenterPanel: React.FC<CenterPanelProps> = ({
  colors,
  activeCategory,
  sortBy,
  onSortChange,
  filteredPosts,
  isLoading,
  onViewPost
}) => {
  return (
    <div style={{
      flex: 1,
      backgroundColor: colors.PANEL_BG,
      border: `1px solid ${colors.GRAY}`,
      padding: '4px',
      overflow: 'auto'
    }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
        <div style={{ color: colors.ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
          FORUM POSTS [{activeCategory}]
          {isLoading && <span style={{ color: colors.YELLOW, marginLeft: '8px' }}>LOADING...</span>}
        </div>
        <div style={{ display: 'flex', gap: '4px', fontSize: '9px' }}>
          <button
            onClick={() => onSortChange('latest')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'latest' ? colors.ORANGE : colors.DARK_BG,
              color: sortBy === 'latest' ? colors.DARK_BG : colors.WHITE,
              border: `1px solid ${colors.GRAY}`,
              cursor: 'pointer'
            }}
          >
            LATEST
          </button>
          <button
            onClick={() => onSortChange('popular')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'popular' ? colors.ORANGE : colors.DARK_BG,
              color: sortBy === 'popular' ? colors.DARK_BG : colors.WHITE,
              border: `1px solid ${colors.GRAY}`,
              cursor: 'pointer'
            }}
          >
            HOT
          </button>
          <button
            onClick={() => onSortChange('views')}
            style={{
              padding: '2px 6px',
              backgroundColor: sortBy === 'views' ? colors.ORANGE : colors.DARK_BG,
              color: sortBy === 'views' ? colors.DARK_BG : colors.WHITE,
              border: `1px solid ${colors.GRAY}`,
              cursor: 'pointer'
            }}
          >
            TOP
          </button>
        </div>
      </div>
      <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '8px' }}></div>

      {filteredPosts.length === 0 && !isLoading && (
        <div style={{ color: colors.GRAY, textAlign: 'center', padding: '20px' }}>
          No posts available. Be the first to post!
        </div>
      )}

      {filteredPosts.map((post, index) => (
        <div
          key={post.id}
          style={{
            marginBottom: '6px',
            padding: '6px',
            backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
            borderLeft: `3px solid ${getPriorityColor(post.priority, colors)}`,
            paddingLeft: '6px',
            cursor: 'pointer'
          }}
          onClick={() => onViewPost(post)}
        >
          <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '8px' }}>
            <span style={{ color: colors.GRAY }}>{post.time}</span>
            <span style={{ color: getPriorityColor(post.priority, colors), fontWeight: 'bold' }}>[{post.priority}]</span>
            <span style={{ color: colors.BLUE }}>[{post.category}]</span>
            <span style={{ color: getSentimentColor(post.sentiment, colors) }}>{post.sentiment}</span>
            {post.verified && <span style={{ color: colors.CYAN }}>[OK] VERIFIED</span>}
          </div>

          <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '2px', fontSize: '9px' }}>
            <span style={{ color: colors.CYAN }}>@{post.author}</span>
            <span style={{ color: colors.GRAY }}>•</span>
            <span style={{ color: colors.WHITE, fontWeight: 'bold' }}>{post.title}</span>
          </div>

          <div style={{ color: colors.GRAY, fontSize: '9px', lineHeight: '1.3', marginBottom: '3px' }}>
            {post.content.substring(0, 200)}{post.content.length > 200 ? '...' : ''}
          </div>

          <div style={{ display: 'flex', gap: '6px', fontSize: '8px', marginBottom: '2px' }}>
            {post.tags.map((tag, i) => (
              <span
                key={i}
                style={{
                  color: colors.YELLOW,
                  backgroundColor: 'rgba(255,255,0,0.1)',
                  padding: '1px 4px',
                  borderRadius: '2px'
                }}
              >
                #{tag}
              </span>
            ))}
          </div>

          <div style={{ display: 'flex', gap: '10px', fontSize: '8px' }}>
            <span style={{ color: colors.CYAN }}>👁️ {post.views.toLocaleString()}</span>
            <span style={{ color: colors.PURPLE }}>💬 {post.replies}</span>
            <span style={{ color: colors.GREEN }}>👍 {post.likes}</span>
            <span style={{ color: colors.RED }}>👎 {post.dislikes}</span>
          </div>
        </div>
      ))}
    </div>
  );
};
