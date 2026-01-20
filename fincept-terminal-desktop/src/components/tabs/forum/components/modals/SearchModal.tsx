// Forum Tab Search Modal Component

import React from 'react';
import type { ForumColors, ForumPost } from '../../types';
import { getPriorityColor } from '../../constants';

interface SearchModalProps {
  colors: ForumColors;
  show: boolean;
  searchQuery: string;
  searchResults: ForumPost[];
  isLoading: boolean;
  onQueryChange: (query: string) => void;
  onSearch: () => void;
  onClose: () => void;
  onViewPost: (post: ForumPost) => void;
}

export const SearchModal: React.FC<SearchModalProps> = ({
  colors,
  show,
  searchQuery,
  searchResults,
  isLoading,
  onQueryChange,
  onSearch,
  onClose,
  onViewPost
}) => {
  if (!show) return null;

  const handleKeyPress = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter') {
      onSearch();
    }
  };

  const handleClose = () => {
    onClose();
  };

  const handlePostClick = (post: ForumPost) => {
    onClose();
    onViewPost(post);
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0,0,0,0.85)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000
    }}>
      <div style={{
        backgroundColor: colors.PANEL_BG,
        border: `2px solid ${colors.ORANGE}`,
        padding: '16px',
        width: '700px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <div style={{ color: colors.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
          SEARCH FORUM
        </div>
        <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '12px' }}></div>

        <div style={{ display: 'flex', gap: '8px', marginBottom: '16px' }}>
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => onQueryChange(e.target.value)}
            onKeyPress={handleKeyPress}
            style={{
              flex: 1,
              backgroundColor: colors.DARK_BG,
              border: `1px solid ${colors.GRAY}`,
              color: colors.WHITE,
              padding: '8px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
            placeholder="Search posts and comments..."
          />
          <button
            onClick={onSearch}
            style={{
              backgroundColor: colors.ORANGE,
              color: colors.DARK_BG,
              border: 'none',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            SEARCH
          </button>
          <button
            onClick={handleClose}
            style={{
              backgroundColor: colors.GRAY,
              color: colors.DARK_BG,
              border: 'none',
              padding: '8px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CLOSE
          </button>
        </div>

        {isLoading && (
          <div style={{ color: colors.YELLOW, textAlign: 'center', padding: '20px' }}>
            SEARCHING...
          </div>
        )}

        {searchResults.length > 0 && (
          <div>
            <div style={{ color: colors.YELLOW, fontSize: '11px', marginBottom: '8px' }}>
              FOUND {searchResults.length} RESULTS
            </div>
            {searchResults.map((post, index) => (
              <div
                key={post.id}
                onClick={() => handlePostClick(post)}
                style={{
                  backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  padding: '8px',
                  marginBottom: '6px',
                  borderLeft: `3px solid ${getPriorityColor(post.priority, colors)}`,
                  cursor: 'pointer'
                }}
              >
                <div style={{ color: colors.WHITE, fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                  {post.title}
                </div>
                <div style={{ color: colors.GRAY, fontSize: '10px', marginBottom: '4px' }}>
                  {post.content.substring(0, 150)}...
                </div>
                <div style={{ display: 'flex', gap: '8px', fontSize: '9px' }}>
                  <span style={{ color: colors.CYAN }}>@{post.author}</span>
                  <span style={{ color: colors.BLUE }}>[{post.category}]</span>
                  <span style={{ color: colors.GREEN }}>👍 {post.likes}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
};
