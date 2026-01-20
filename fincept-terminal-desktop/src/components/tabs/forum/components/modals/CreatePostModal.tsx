// Forum Tab Create Post Modal Component

import React from 'react';
import type { ForumColors } from '../../types';

interface CreatePostModalProps {
  colors: ForumColors;
  show: boolean;
  newPostTitle: string;
  newPostContent: string;
  onTitleChange: (title: string) => void;
  onContentChange: (content: string) => void;
  onClose: () => void;
  onSubmit: () => void;
}

export const CreatePostModal: React.FC<CreatePostModalProps> = ({
  colors,
  show,
  newPostTitle,
  newPostContent,
  onTitleChange,
  onContentChange,
  onClose,
  onSubmit
}) => {
  if (!show) return null;

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
        width: '600px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <div style={{ color: colors.ORANGE, fontSize: '14px', fontWeight: 'bold', marginBottom: '12px' }}>
          CREATE NEW POST
        </div>
        <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '12px' }}></div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: colors.WHITE, fontSize: '11px', marginBottom: '4px' }}>
            Title: <span style={{ color: newPostTitle.length < 5 ? colors.RED : colors.GREEN }}>
              ({newPostTitle.length}/500 chars - min 5)
            </span>
          </div>
          <input
            type="text"
            value={newPostTitle}
            onChange={(e) => onTitleChange(e.target.value)}
            maxLength={500}
            style={{
              width: '100%',
              backgroundColor: colors.DARK_BG,
              border: `1px solid ${newPostTitle.length > 0 && newPostTitle.length < 5 ? colors.RED : colors.GRAY}`,
              color: colors.WHITE,
              padding: '6px',
              fontSize: '12px',
              fontFamily: 'Consolas, monospace'
            }}
            placeholder="Enter post title (minimum 5 characters)..."
          />
        </div>

        <div style={{ marginBottom: '12px' }}>
          <div style={{ color: colors.WHITE, fontSize: '11px', marginBottom: '4px' }}>
            Content: <span style={{ color: newPostContent.length < 10 ? colors.RED : colors.GREEN }}>
              ({newPostContent.length}/10000 chars - min 10)
            </span>
          </div>
          <textarea
            value={newPostContent}
            onChange={(e) => onContentChange(e.target.value)}
            maxLength={10000}
            rows={10}
            style={{
              width: '100%',
              backgroundColor: colors.DARK_BG,
              border: `1px solid ${newPostContent.length > 0 && newPostContent.length < 10 ? colors.RED : colors.GRAY}`,
              color: colors.WHITE,
              padding: '6px',
              fontSize: '11px',
              fontFamily: 'Consolas, monospace',
              resize: 'vertical'
            }}
            placeholder="Enter post content (minimum 10 characters)... Use #tags for topics"
          />
        </div>

        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
          <button
            onClick={onClose}
            style={{
              backgroundColor: colors.GRAY,
              color: colors.DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CANCEL
          </button>
          <button
            onClick={onSubmit}
            style={{
              backgroundColor: colors.ORANGE,
              color: colors.DARK_BG,
              border: 'none',
              padding: '6px 16px',
              fontSize: '11px',
              fontWeight: 'bold',
              cursor: 'pointer'
            }}
          >
            CREATE POST
          </button>
        </div>
      </div>
    </div>
  );
};
