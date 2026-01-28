// Forum Tab Search Modal Component
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { X, Search, Eye, ThumbsUp } from 'lucide-react';
import type { ForumColors, ForumPost } from '../../types';
import { getPriorityColor } from '../../constants';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface SearchModalProps {
  show: boolean;
  colors: ForumColors;
  query: string;
  results: ForumPost[];
  onQueryChange: (query: string) => void;
  onSearch: () => void;
  onClose: () => void;
  onViewPost: (post: ForumPost) => void;
}

export const SearchModal: React.FC<SearchModalProps> = ({
  show,
  colors,
  query,
  results,
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
      backgroundColor: 'rgba(0,0,0,0.9)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      padding: '20px'
    }}>
      <div style={{
        backgroundColor: colors.DARK_BG,
        border: `2px solid ${colors.ORANGE}`,
        borderRadius: '2px',
        width: '100%',
        maxWidth: '800px',
        maxHeight: '90vh',
        display: 'flex',
        flexDirection: 'column',
        boxShadow: `0 8px 32px ${colors.ORANGE}30`
      }}>
        {/* Header */}
        <div style={{
          padding: '16px',
          backgroundColor: colors.HEADER_BG,
          borderBottom: `2px solid ${colors.ORANGE}`,
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <div style={{
            fontSize: '12px',
            fontWeight: 700,
            color: colors.ORANGE,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY
          }}>
            SEARCH FORUM
          </div>
          <button
            onClick={onClose}
            style={{
              padding: '4px',
              backgroundColor: 'transparent',
              border: 'none',
              color: colors.GRAY,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              transition: 'color 0.2s'
            }}
            onMouseEnter={(e) => e.currentTarget.style.color = colors.WHITE}
            onMouseLeave={(e) => e.currentTarget.style.color = colors.GRAY}
          >
            <X size={16} />
          </button>
        </div>

        {/* Search Bar */}
        <div style={{
          padding: '16px',
          backgroundColor: colors.PANEL_BG,
          borderBottom: `1px solid ${colors.BORDER}`
        }}>
          <div style={{ display: 'flex', gap: '8px' }}>
            <input
              type="text"
              value={query}
              onChange={(e) => onQueryChange(e.target.value)}
              onKeyPress={handleKeyPress}
              autoFocus
              style={{
                flex: 1,
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.BORDER}`,
                borderRadius: '2px',
                color: colors.WHITE,
                padding: '10px 12px',
                fontSize: '11px',
                fontFamily: FONT_FAMILY,
                outline: 'none',
                transition: 'border-color 0.2s'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = colors.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = colors.BORDER}
              placeholder="Search posts and comments..."
            />
            <button
              onClick={onSearch}
              style={{
                padding: '8px 20px',
                backgroundColor: colors.ORANGE,
                border: 'none',
                borderRadius: '2px',
                color: colors.DARK_BG,
                fontSize: '9px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                fontFamily: FONT_FAMILY,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = colors.YELLOW}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.ORANGE}
            >
              <Search size={10} />
              SEARCH
            </button>
          </div>
        </div>

        {/* Results */}
        <div className="forum-scroll" style={{
          flex: 1,
          overflowY: 'auto',
          padding: '16px'
        }}>
          {query && results.length > 0 && (
            <div style={{
              marginBottom: '12px',
              fontSize: '9px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              FOUND {results.length} RESULTS FOR "{query}"
            </div>
          )}

          {query && results.length === 0 && (
            <div style={{
              padding: '32px',
              textAlign: 'center',
              color: colors.MUTED,
              fontSize: '9px',
              fontFamily: FONT_FAMILY
            }}>
              NO RESULTS FOUND FOR "{query}"
            </div>
          )}

          {!query && (
            <div style={{
              padding: '32px',
              textAlign: 'center',
              color: colors.MUTED,
              fontSize: '9px',
              fontFamily: FONT_FAMILY
            }}>
              ENTER A SEARCH QUERY TO BEGIN
            </div>
          )}

          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {results.map((post) => (
              <div
                key={post.id}
                onClick={() => handlePostClick(post)}
                style={{
                  padding: '12px',
                  backgroundColor: colors.PANEL_BG,
                  border: `1px solid ${colors.BORDER}`,
                  borderLeft: `3px solid ${getPriorityColor(post.priority, colors)}`,
                  borderRadius: '2px',
                  cursor: 'pointer',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = colors.ORANGE;
                  e.currentTarget.style.backgroundColor = colors.HOVER;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = colors.BORDER;
                  e.currentTarget.style.borderLeftColor = getPriorityColor(post.priority, colors);
                  e.currentTarget.style.backgroundColor = colors.PANEL_BG;
                }}
              >
                <div style={{
                  display: 'flex',
                  gap: '8px',
                  alignItems: 'center',
                  marginBottom: '8px',
                  fontSize: '8px',
                  fontFamily: FONT_FAMILY
                }}>
                  <span style={{
                    color: colors.CYAN,
                    fontWeight: 700,
                    letterSpacing: '0.5px'
                  }}>
                    @{post.author}
                  </span>
                  <span style={{ color: colors.MUTED }}>•</span>
                  <span style={{
                    padding: '2px 6px',
                    backgroundColor: `${colors.BLUE}20`,
                    color: colors.BLUE,
                    borderRadius: '2px',
                    fontSize: '8px',
                    fontWeight: 700,
                    letterSpacing: '0.5px'
                  }}>
                    {post.category}
                  </span>
                </div>

                <div style={{
                  fontSize: '11px',
                  fontWeight: 700,
                  color: colors.WHITE,
                  marginBottom: '8px',
                  fontFamily: FONT_FAMILY,
                  lineHeight: '1.4'
                }}>
                  {post.title}
                </div>

                <div style={{
                  fontSize: '9px',
                  color: colors.GRAY,
                  marginBottom: '8px',
                  fontFamily: FONT_FAMILY,
                  lineHeight: '1.5'
                }}>
                  {post.content.substring(0, 150)}{post.content.length > 150 ? '...' : ''}
                </div>

                <div style={{
                  display: 'flex',
                  gap: '16px',
                  fontSize: '8px',
                  fontFamily: FONT_FAMILY,
                  color: colors.GRAY
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <Eye size={10} color={colors.CYAN} />
                    <span style={{ color: colors.CYAN, fontWeight: 700 }}>{post.views.toLocaleString()}</span>
                  </div>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                    <ThumbsUp size={10} color={colors.GREEN} />
                    <span style={{ color: colors.GREEN, fontWeight: 700 }}>{post.likes}</span>
                  </div>
                </div>
              </div>
            ))}
          </div>
        </div>
      </div>
    </div>
  );
};
