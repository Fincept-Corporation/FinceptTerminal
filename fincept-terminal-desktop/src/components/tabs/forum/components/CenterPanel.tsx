// Forum Tab Center Panel Component (Posts List)
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { MessageSquare, Eye, ThumbsUp, ThumbsDown, CheckCircle } from 'lucide-react';
import type { ForumColors, ForumPost } from '../types';
import { getPriorityColor, getSentimentColor } from '../constants';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

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
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Header with Sort Controls */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.HEADER_BG,
        borderBottom: `1px solid ${colors.BORDER}`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center'
      }}>
        <div style={{
          fontSize: '9px',
          fontWeight: 700,
          color: colors.GRAY,
          letterSpacing: '0.5px',
          fontFamily: FONT_FAMILY
        }}>
          {activeCategory.toUpperCase()} - {filteredPosts.length} POSTS
          {isLoading && <span style={{ color: colors.YELLOW, marginLeft: '8px' }}>⟳ LOADING...</span>}
        </div>

        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => onSortChange('latest')}
            style={{
              padding: '4px 12px',
              backgroundColor: sortBy === 'latest' ? colors.ORANGE : 'transparent',
              color: sortBy === 'latest' ? colors.DARK_BG : colors.GRAY,
              border: sortBy === 'latest' ? 'none' : `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (sortBy !== 'latest') {
                e.currentTarget.style.borderColor = colors.ORANGE;
                e.currentTarget.style.color = colors.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (sortBy !== 'latest') {
                e.currentTarget.style.borderColor = colors.BORDER;
                e.currentTarget.style.color = colors.GRAY;
              }
            }}
          >
            LATEST
          </button>
          <button
            onClick={() => onSortChange('popular')}
            style={{
              padding: '4px 12px',
              backgroundColor: sortBy === 'popular' ? colors.ORANGE : 'transparent',
              color: sortBy === 'popular' ? colors.DARK_BG : colors.GRAY,
              border: sortBy === 'popular' ? 'none' : `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (sortBy !== 'popular') {
                e.currentTarget.style.borderColor = colors.ORANGE;
                e.currentTarget.style.color = colors.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (sortBy !== 'popular') {
                e.currentTarget.style.borderColor = colors.BORDER;
                e.currentTarget.style.color = colors.GRAY;
              }
            }}
          >
            HOT
          </button>
          <button
            onClick={() => onSortChange('views')}
            style={{
              padding: '4px 12px',
              backgroundColor: sortBy === 'views' ? colors.ORANGE : 'transparent',
              color: sortBy === 'views' ? colors.DARK_BG : colors.GRAY,
              border: sortBy === 'views' ? 'none' : `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              if (sortBy !== 'views') {
                e.currentTarget.style.borderColor = colors.ORANGE;
                e.currentTarget.style.color = colors.WHITE;
              }
            }}
            onMouseLeave={(e) => {
              if (sortBy !== 'views') {
                e.currentTarget.style.borderColor = colors.BORDER;
                e.currentTarget.style.color = colors.GRAY;
              }
            }}
          >
            TOP
          </button>
        </div>
      </div>

      {/* Posts List */}
      <div className="forum-scroll" style={{
        flex: 1,
        overflowY: 'auto',
        padding: '12px',
        backgroundColor: colors.PANEL_BG
      }}>
        {filteredPosts.length === 0 && !isLoading && (
          <div style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            gap: '8px'
          }}>
            <MessageSquare size={32} color={colors.MUTED} />
            <div style={{
              fontSize: '10px',
              color: colors.GRAY,
              fontFamily: FONT_FAMILY,
              textAlign: 'center'
            }}>
              NO POSTS AVAILABLE<br />
              <span style={{ fontSize: '9px', color: colors.MUTED }}>Be the first to post!</span>
            </div>
          </div>
        )}

        {filteredPosts.map((post) => (
          <div
            key={post.id}
            onClick={() => onViewPost(post)}
            style={{
              marginBottom: '8px',
              padding: '12px',
              backgroundColor: colors.DARK_BG,
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
              e.currentTarget.style.backgroundColor = colors.DARK_BG;
            }}
          >
            {/* Post Header - Meta Info */}
            <div style={{
              display: 'flex',
              gap: '8px',
              alignItems: 'center',
              marginBottom: '8px',
              fontSize: '8px',
              fontFamily: FONT_FAMILY,
              flexWrap: 'wrap'
            }}>
              <span style={{ color: colors.GRAY, letterSpacing: '0.5px' }}>{post.time}</span>
              <span style={{ color: colors.MUTED }}>•</span>
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
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${getPriorityColor(post.priority, colors)}20`,
                color: getPriorityColor(post.priority, colors),
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                {post.priority}
              </span>
              <span style={{
                padding: '2px 6px',
                backgroundColor: `${getSentimentColor(post.sentiment, colors)}20`,
                color: getSentimentColor(post.sentiment, colors),
                borderRadius: '2px',
                fontSize: '8px',
                fontWeight: 700,
                letterSpacing: '0.5px'
              }}>
                {post.sentiment}
              </span>
              {post.verified && (
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px',
                  color: colors.CYAN
                }}>
                  <CheckCircle size={10} />
                  <span style={{ fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px' }}>VERIFIED</span>
                </div>
              )}
            </div>

            {/* Post Title */}
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

            {/* Post Content Preview */}
            <div style={{
              fontSize: '9px',
              color: colors.GRAY,
              marginBottom: '8px',
              fontFamily: FONT_FAMILY,
              lineHeight: '1.5'
            }}>
              {post.content.substring(0, 200)}{post.content.length > 200 ? '...' : ''}
            </div>

            {/* Tags */}
            {post.tags.length > 0 && (
              <div style={{
                display: 'flex',
                gap: '6px',
                marginBottom: '8px',
                flexWrap: 'wrap'
              }}>
                {post.tags.map((tag, i) => (
                  <span
                    key={i}
                    style={{
                      padding: '2px 8px',
                      backgroundColor: `${colors.YELLOW}15`,
                      color: colors.YELLOW,
                      borderRadius: '2px',
                      fontSize: '8px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      fontFamily: FONT_FAMILY
                    }}
                  >
                    #{tag}
                  </span>
                ))}
              </div>
            )}

            {/* Post Stats */}
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
                <MessageSquare size={10} color={colors.PURPLE} />
                <span style={{ color: colors.PURPLE, fontWeight: 700 }}>{post.replies}</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <ThumbsUp size={10} color={colors.GREEN} />
                <span style={{ color: colors.GREEN, fontWeight: 700 }}>{post.likes}</span>
              </div>
              <div style={{ display: 'flex', alignItems: 'center', gap: '4px' }}>
                <ThumbsDown size={10} color={colors.RED} />
                <span style={{ color: colors.RED, fontWeight: 700 }}>{post.dislikes}</span>
              </div>
            </div>
          </div>
        ))}
      </div>
    </div>
  );
};
