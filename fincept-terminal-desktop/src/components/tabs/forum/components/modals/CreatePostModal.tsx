// Forum Tab Create Post Modal Component
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { X, Send } from 'lucide-react';
import type { ForumColors } from '../../types';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface CreatePostModalProps {
  show: boolean;
  colors: ForumColors;
  title: string;
  content: string;
  tags: string;
  category: string;
  onTitleChange: (title: string) => void;
  onContentChange: (content: string) => void;
  onTagsChange: (tags: string) => void;
  onCategoryChange: (category: string) => void;
  onSubmit: () => void;
  onClose: () => void;
}

export const CreatePostModal: React.FC<CreatePostModalProps> = ({
  show,
  colors,
  title,
  content,
  tags,
  category,
  onTitleChange,
  onContentChange,
  onTagsChange,
  onCategoryChange,
  onSubmit,
  onClose
}) => {
  if (!show) return null;

  const isValid = title.trim().length >= 5 && content.trim().length >= 10;

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
        maxWidth: '700px',
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
            CREATE NEW POST
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
          {/* Title Field */}
          <div style={{ marginBottom: '20px' }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '8px'
            }}>
              <label style={{
                fontSize: '9px',
                fontWeight: 700,
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                TITLE
              </label>
              <span style={{
                fontSize: '8px',
                color: title.length < 5 ? colors.RED : colors.GREEN,
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px'
              }}>
                {title.length}/500 {title.length < 5 ? '(MIN 5)' : ''}
              </span>
            </div>
            <input
              type="text"
              value={title}
              onChange={(e) => onTitleChange(e.target.value)}
              maxLength={500}
              style={{
                width: '100%',
                backgroundColor: colors.PANEL_BG,
                border: `1px solid ${title.length > 0 && title.length < 5 ? colors.RED : colors.BORDER}`,
                borderRadius: '2px',
                color: colors.WHITE,
                padding: '10px 12px',
                fontSize: '11px',
                fontFamily: FONT_FAMILY,
                outline: 'none',
                transition: 'border-color 0.2s'
              }}
              onFocus={(e) => {
                if (title.length >= 5 || title.length === 0) {
                  e.currentTarget.style.borderColor = colors.ORANGE;
                }
              }}
              onBlur={(e) => {
                if (title.length >= 5) {
                  e.currentTarget.style.borderColor = colors.BORDER;
                } else if (title.length > 0) {
                  e.currentTarget.style.borderColor = colors.RED;
                }
              }}
              placeholder="Enter post title (minimum 5 characters)..."
            />
          </div>

          {/* Category Field */}
          <div style={{ marginBottom: '20px' }}>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY,
              marginBottom: '8px'
            }}>
              CATEGORY
            </label>
            <select
              value={category}
              onChange={(e) => onCategoryChange(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: colors.PANEL_BG,
                border: `1px solid ${colors.BORDER}`,
                borderRadius: '2px',
                color: colors.WHITE,
                padding: '10px 12px',
                fontSize: '11px',
                fontFamily: FONT_FAMILY,
                outline: 'none',
                cursor: 'pointer',
                transition: 'border-color 0.2s'
              }}
              onFocus={(e) => e.currentTarget.style.borderColor = colors.ORANGE}
              onBlur={(e) => e.currentTarget.style.borderColor = colors.BORDER}
            >
              <option value="">Select a category</option>
              <option value="General Discussion">General Discussion</option>
              <option value="Market Analysis">Market Analysis</option>
              <option value="Trading Strategies">Trading Strategies</option>
              <option value="Technical Analysis">Technical Analysis</option>
              <option value="News & Events">News & Events</option>
              <option value="Questions">Questions</option>
              <option value="Feedback">Feedback</option>
            </select>
          </div>

          {/* Tags Field */}
          <div style={{ marginBottom: '20px' }}>
            <label style={{
              display: 'block',
              fontSize: '9px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY,
              marginBottom: '8px'
            }}>
              TAGS (COMMA SEPARATED)
            </label>
            <input
              type="text"
              value={tags}
              onChange={(e) => onTagsChange(e.target.value)}
              style={{
                width: '100%',
                backgroundColor: colors.PANEL_BG,
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
              placeholder="e.g., stocks, crypto, analysis"
            />
          </div>

          {/* Content Field */}
          <div style={{ marginBottom: '20px' }}>
            <div style={{
              display: 'flex',
              justifyContent: 'space-between',
              alignItems: 'center',
              marginBottom: '8px'
            }}>
              <label style={{
                fontSize: '9px',
                fontWeight: 700,
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                CONTENT
              </label>
              <span style={{
                fontSize: '8px',
                color: content.length < 10 ? colors.RED : colors.GREEN,
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px'
              }}>
                {content.length}/10000 {content.length < 10 ? '(MIN 10)' : ''}
              </span>
            </div>
            <textarea
              value={content}
              onChange={(e) => onContentChange(e.target.value)}
              maxLength={10000}
              rows={12}
              style={{
                width: '100%',
                backgroundColor: colors.PANEL_BG,
                border: `1px solid ${content.length > 0 && content.length < 10 ? colors.RED : colors.BORDER}`,
                borderRadius: '2px',
                color: colors.WHITE,
                padding: '10px 12px',
                fontSize: '11px',
                fontFamily: FONT_FAMILY,
                resize: 'vertical',
                outline: 'none',
                transition: 'border-color 0.2s',
                lineHeight: '1.6'
              }}
              onFocus={(e) => {
                if (content.length >= 10 || content.length === 0) {
                  e.currentTarget.style.borderColor = colors.ORANGE;
                }
              }}
              onBlur={(e) => {
                if (content.length >= 10) {
                  e.currentTarget.style.borderColor = colors.BORDER;
                } else if (content.length > 0) {
                  e.currentTarget.style.borderColor = colors.RED;
                }
              }}
              placeholder="Enter post content (minimum 10 characters)..."
            />
          </div>
        </div>

        {/* Footer */}
        <div style={{
          padding: '16px',
          backgroundColor: colors.HEADER_BG,
          borderTop: `1px solid ${colors.BORDER}`,
          display: 'flex',
          justifyContent: 'flex-end',
          gap: '12px'
        }}>
          <button
            onClick={onClose}
            style={{
              padding: '8px 20px',
              backgroundColor: 'transparent',
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              color: colors.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: 'pointer',
              fontFamily: FONT_FAMILY,
              transition: 'all 0.2s'
            }}
            onMouseEnter={(e) => {
              e.currentTarget.style.borderColor = colors.ORANGE;
              e.currentTarget.style.color = colors.WHITE;
            }}
            onMouseLeave={(e) => {
              e.currentTarget.style.borderColor = colors.BORDER;
              e.currentTarget.style.color = colors.GRAY;
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onSubmit}
            disabled={!isValid}
            style={{
              padding: '8px 20px',
              backgroundColor: isValid ? colors.ORANGE : colors.MUTED,
              border: 'none',
              borderRadius: '2px',
              color: isValid ? colors.DARK_BG : colors.GRAY,
              fontSize: '9px',
              fontWeight: 700,
              letterSpacing: '0.5px',
              cursor: isValid ? 'pointer' : 'not-allowed',
              fontFamily: FONT_FAMILY,
              display: 'flex',
              alignItems: 'center',
              gap: '6px',
              transition: 'all 0.2s',
              opacity: isValid ? 1 : 0.5
            }}
            onMouseEnter={(e) => {
              if (isValid) {
                e.currentTarget.style.backgroundColor = colors.YELLOW;
              }
            }}
            onMouseLeave={(e) => {
              if (isValid) {
                e.currentTarget.style.backgroundColor = colors.ORANGE;
              }
            }}
          >
            <Send size={10} />
            CREATE POST
          </button>
        </div>
      </div>
    </div>
  );
};
