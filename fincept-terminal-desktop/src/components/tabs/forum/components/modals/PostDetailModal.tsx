// Forum Tab Post Detail Modal Component
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { X, ThumbsUp, ThumbsDown, MessageSquare, Eye, Send, CheckCircle } from 'lucide-react';
import type { ForumColors, ForumPost, ForumComment } from '../../types';
import { getSentimentColor, getPriorityColor } from '../../constants';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface PostDetailModalProps {
  show: boolean;
  colors: ForumColors;
  post: ForumPost | null;
  comments: ForumComment[];
  newComment: string;
  onCommentChange: (comment: string) => void;
  onAddComment: () => void;
  onVote: (itemId: string, voteType: 'up' | 'down', itemType: 'post' | 'comment') => void;
  onClose: () => void;
}

export const PostDetailModal: React.FC<PostDetailModalProps> = ({
  show,
  colors,
  post,
  comments,
  newComment,
  onCommentChange,
  onAddComment,
  onVote,
  onClose
}) => {
  if (!show || !post) return null;

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
        maxWidth: '900px',
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
            POST DETAILS
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

        {/* Content */}
        <div className="forum-scroll" style={{
          flex: 1,
          overflowY: 'auto',
          padding: '20px'
        }}>
          {/* Post Content */}
          <div style={{
            padding: '16px',
            backgroundColor: colors.PANEL_BG,
            border: `1px solid ${colors.BORDER}`,
            borderLeft: `3px solid ${getPriorityColor(post.priority, colors)}`,
            borderRadius: '2px',
            marginBottom: '20px'
          }}>
            {/* Post Meta */}
            <div style={{
              display: 'flex',
              gap: '8px',
              alignItems: 'center',
              marginBottom: '12px',
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
              fontSize: '14px',
              fontWeight: 700,
              color: colors.WHITE,
              marginBottom: '12px',
              fontFamily: FONT_FAMILY,
              lineHeight: '1.4'
            }}>
              {post.title}
            </div>

            {/* Post Content */}
            <div style={{
              fontSize: '10px',
              color: colors.GRAY,
              marginBottom: '12px',
              fontFamily: FONT_FAMILY,
              lineHeight: '1.6',
              whiteSpace: 'pre-wrap'
            }}>
              {post.content}
            </div>

            {/* Tags */}
            {post.tags.length > 0 && (
              <div style={{
                display: 'flex',
                gap: '6px',
                marginBottom: '12px',
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

            {/* Post Stats & Actions */}
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '16px',
              paddingTop: '12px',
              borderTop: `1px solid ${colors.BORDER}`
            }}>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontSize: '9px',
                fontFamily: FONT_FAMILY,
                color: colors.GRAY
              }}>
                <Eye size={12} color={colors.CYAN} />
                <span style={{ color: colors.CYAN, fontWeight: 700 }}>{post.views.toLocaleString()}</span>
              </div>
              <div style={{
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                fontSize: '9px',
                fontFamily: FONT_FAMILY,
                color: colors.GRAY
              }}>
                <MessageSquare size={12} color={colors.PURPLE} />
                <span style={{ color: colors.PURPLE, fontWeight: 700 }}>{post.replies}</span>
              </div>
              <button
                onClick={() => onVote(post.id, 'up', 'post')}
                style={{
                  padding: '4px 12px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.GREEN}`,
                  borderRadius: '2px',
                  color: colors.GREEN,
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: FONT_FAMILY,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = `${colors.GREEN}20`;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <ThumbsUp size={10} />
                {post.likes}
              </button>
              <button
                onClick={() => onVote(post.id, 'down', 'post')}
                style={{
                  padding: '4px 12px',
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.RED}`,
                  borderRadius: '2px',
                  color: colors.RED,
                  fontSize: '9px',
                  fontWeight: 700,
                  cursor: 'pointer',
                  fontFamily: FONT_FAMILY,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.backgroundColor = `${colors.RED}20`;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <ThumbsDown size={10} />
                {post.dislikes}
              </button>
            </div>
          </div>

          {/* Comments Section */}
          <div>
            <div style={{
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              marginBottom: '12px',
              fontSize: '10px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              <MessageSquare size={14} color={colors.ORANGE} />
              COMMENTS ({comments.length})
            </div>

            {/* Add Comment Form */}
            <div style={{
              padding: '16px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              marginBottom: '16px'
            }}>
              <label style={{
                display: 'block',
                fontSize: '9px',
                fontWeight: 700,
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY,
                marginBottom: '8px'
              }}>
                ADD COMMENT
              </label>
              <textarea
                value={newComment}
                onChange={(e) => onCommentChange(e.target.value)}
                maxLength={2000}
                rows={4}
                style={{
                  width: '100%',
                  backgroundColor: colors.DARK_BG,
                  border: `1px solid ${colors.BORDER}`,
                  borderRadius: '2px',
                  color: colors.WHITE,
                  padding: '10px 12px',
                  fontSize: '10px',
                  fontFamily: FONT_FAMILY,
                  resize: 'vertical',
                  outline: 'none',
                  marginBottom: '8px',
                  lineHeight: '1.5',
                  transition: 'border-color 0.2s'
                }}
                onFocus={(e) => e.currentTarget.style.borderColor = colors.ORANGE}
                onBlur={(e) => e.currentTarget.style.borderColor = colors.BORDER}
                placeholder="Write your comment..."
              />
              <button
                onClick={onAddComment}
                disabled={!newComment.trim()}
                style={{
                  padding: '6px 16px',
                  backgroundColor: newComment.trim() ? colors.ORANGE : colors.MUTED,
                  border: 'none',
                  borderRadius: '2px',
                  color: newComment.trim() ? colors.DARK_BG : colors.GRAY,
                  fontSize: '9px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  cursor: newComment.trim() ? 'pointer' : 'not-allowed',
                  fontFamily: FONT_FAMILY,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '6px',
                  transition: 'all 0.2s',
                  opacity: newComment.trim() ? 1 : 0.5
                }}
                onMouseEnter={(e) => {
                  if (newComment.trim()) {
                    e.currentTarget.style.backgroundColor = colors.YELLOW;
                  }
                }}
                onMouseLeave={(e) => {
                  if (newComment.trim()) {
                    e.currentTarget.style.backgroundColor = colors.ORANGE;
                  }
                }}
              >
                <Send size={10} />
                POST COMMENT
              </button>
            </div>

            {/* Comments List */}
            {comments.length === 0 ? (
              <div style={{
                padding: '32px',
                textAlign: 'center',
                color: colors.MUTED,
                fontSize: '9px',
                fontFamily: FONT_FAMILY
              }}>
                NO COMMENTS YET - BE THE FIRST TO COMMENT!
              </div>
            ) : (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
                {comments.map((comment) => (
                  <div
                    key={comment.id}
                    style={{
                      padding: '12px',
                      backgroundColor: colors.PANEL_BG,
                      border: `1px solid ${colors.BORDER}`,
                      borderLeft: `2px solid ${colors.CYAN}`,
                      borderRadius: '2px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderLeftColor = colors.ORANGE;
                      e.currentTarget.style.backgroundColor = colors.HOVER;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderLeftColor = colors.CYAN;
                      e.currentTarget.style.backgroundColor = colors.PANEL_BG;
                    }}
                  >
                    {/* Comment Meta */}
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
                        @{comment.author}
                      </span>
                      <span style={{ color: colors.MUTED }}>•</span>
                      <span style={{ color: colors.GRAY, letterSpacing: '0.5px' }}>{comment.time}</span>
                    </div>

                    {/* Comment Content */}
                    <div style={{
                      fontSize: '10px',
                      color: colors.WHITE,
                      marginBottom: '8px',
                      fontFamily: FONT_FAMILY,
                      lineHeight: '1.5',
                      whiteSpace: 'pre-wrap'
                    }}>
                      {comment.content}
                    </div>

                    {/* Comment Actions */}
                    <div style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '12px'
                    }}>
                      <button
                        onClick={() => onVote(comment.id, 'up', 'comment')}
                        style={{
                          padding: '3px 10px',
                          backgroundColor: 'transparent',
                          border: 'none',
                          color: colors.GREEN,
                          fontSize: '8px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          transition: 'all 0.2s'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = `${colors.GREEN}20`;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }}
                      >
                        <ThumbsUp size={9} />
                        {comment.likes}
                      </button>
                      <button
                        onClick={() => onVote(comment.id, 'down', 'comment')}
                        style={{
                          padding: '3px 10px',
                          backgroundColor: 'transparent',
                          border: 'none',
                          color: colors.RED,
                          fontSize: '8px',
                          fontWeight: 700,
                          cursor: 'pointer',
                          fontFamily: FONT_FAMILY,
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                          transition: 'all 0.2s'
                        }}
                        onMouseEnter={(e) => {
                          e.currentTarget.style.backgroundColor = `${colors.RED}20`;
                        }}
                        onMouseLeave={(e) => {
                          e.currentTarget.style.backgroundColor = 'transparent';
                        }}
                      >
                        <ThumbsDown size={9} />
                        {comment.dislikes}
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};
