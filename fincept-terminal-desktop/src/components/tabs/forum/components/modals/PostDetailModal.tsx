// Forum Tab Post Detail Modal Component

import React from 'react';
import type { ForumColors, ForumPost, ForumComment } from '../../types';
import { getSentimentColor } from '../../constants';

interface PostDetailModalProps {
  colors: ForumColors;
  show: boolean;
  selectedPost: ForumPost | null;
  postComments: ForumComment[];
  newComment: string;
  onCommentChange: (comment: string) => void;
  onClose: () => void;
  onAddComment: () => void;
  onVotePost: (postId: string, voteType: 'up' | 'down') => void;
  onVoteComment: (commentId: string, voteType: 'up' | 'down') => void;
}

export const PostDetailModal: React.FC<PostDetailModalProps> = ({
  colors,
  show,
  selectedPost,
  postComments,
  newComment,
  onCommentChange,
  onClose,
  onAddComment,
  onVotePost,
  onVoteComment
}) => {
  if (!show || !selectedPost) return null;

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
        width: '800px',
        maxHeight: '80vh',
        overflow: 'auto'
      }}>
        <>
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
            <div style={{ color: colors.ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
              POST DETAILS
            </div>
            <button
              onClick={onClose}
              style={{
                backgroundColor: 'transparent',
                border: `1px solid ${colors.GRAY}`,
                color: colors.WHITE,
                padding: '2px 8px',
                fontSize: '10px',
                cursor: 'pointer'
              }}
            >
              CLOSE [X]
            </button>
          </div>
          <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '12px' }}></div>

          {/* Post content */}
          <div style={{ marginBottom: '16px' }}>
            <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '8px', fontSize: '10px' }}>
              <span style={{ color: colors.CYAN }}>@{selectedPost.author}</span>
              <span style={{ color: colors.GRAY }}>•</span>
              <span style={{ color: colors.BLUE }}>[{selectedPost.category}]</span>
              <span style={{ color: getSentimentColor(selectedPost.sentiment, colors) }}>{selectedPost.sentiment}</span>
            </div>

            <div style={{ color: colors.WHITE, fontSize: '13px', fontWeight: 'bold', marginBottom: '8px' }}>
              {selectedPost.title}
            </div>

            <div style={{ color: colors.GRAY, fontSize: '11px', lineHeight: '1.5', marginBottom: '8px', whiteSpace: 'pre-wrap' }}>
              {selectedPost.content}
            </div>

            <div style={{ display: 'flex', gap: '12px', fontSize: '10px', marginBottom: '8px' }}>
              <button
                onClick={() => onVotePost(selectedPost.id, 'up')}
                style={{
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.GREEN}`,
                  color: colors.GREEN,
                  padding: '4px 12px',
                  fontSize: '10px',
                  cursor: 'pointer'
                }}
              >
                ▲ UPVOTE ({selectedPost.likes})
              </button>
              <button
                onClick={() => onVotePost(selectedPost.id, 'down')}
                style={{
                  backgroundColor: 'transparent',
                  border: `1px solid ${colors.RED}`,
                  color: colors.RED,
                  padding: '4px 12px',
                  fontSize: '10px',
                  cursor: 'pointer'
                }}
              >
                ▼ DOWNVOTE ({selectedPost.dislikes})
              </button>
            </div>
          </div>

          <div style={{ borderTop: `1px solid ${colors.GRAY}`, paddingTop: '12px', marginBottom: '12px' }}>
            <div style={{ color: colors.YELLOW, fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
              COMMENTS ({postComments.length})
            </div>

            {/* Add comment form */}
            <div style={{ marginBottom: '12px' }}>
              <textarea
                value={newComment}
                onChange={(e) => onCommentChange(e.target.value)}
                maxLength={2000}
                rows={3}
                style={{
                  width: '100%',
                  backgroundColor: colors.DARK_BG,
                  border: `1px solid ${colors.GRAY}`,
                  color: colors.WHITE,
                  padding: '6px',
                  fontSize: '11px',
                  fontFamily: 'Consolas, monospace',
                  marginBottom: '4px'
                }}
                placeholder="Add a comment..."
              />
              <button
                onClick={onAddComment}
                style={{
                  backgroundColor: colors.ORANGE,
                  color: colors.DARK_BG,
                  border: 'none',
                  padding: '4px 12px',
                  fontSize: '10px',
                  fontWeight: 'bold',
                  cursor: 'pointer'
                }}
              >
                POST COMMENT
              </button>
            </div>

            {/* Comments list */}
            {postComments.map((comment, index) => (
              <div key={comment.id} style={{
                backgroundColor: index % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                padding: '8px',
                marginBottom: '6px',
                borderLeft: `2px solid ${colors.BLUE}`
              }}>
                <div style={{ display: 'flex', gap: '4px', alignItems: 'center', marginBottom: '4px', fontSize: '9px' }}>
                  <span style={{ color: colors.CYAN }}>@{comment.author}</span>
                  <span style={{ color: colors.GRAY }}>•</span>
                  <span style={{ color: colors.GRAY }}>{comment.time}</span>
                </div>

                <div style={{ color: colors.WHITE, fontSize: '10px', marginBottom: '4px', whiteSpace: 'pre-wrap' }}>
                  {comment.content}
                </div>

                <div style={{ display: 'flex', gap: '8px', fontSize: '9px' }}>
                  <button
                    onClick={() => onVoteComment(comment.id, 'up')}
                    style={{
                      backgroundColor: 'transparent',
                      border: 'none',
                      color: colors.GREEN,
                      cursor: 'pointer',
                      fontSize: '9px'
                    }}
                  >
                    ▲ {comment.likes}
                  </button>
                  <button
                    onClick={() => onVoteComment(comment.id, 'down')}
                    style={{
                      backgroundColor: 'transparent',
                      border: 'none',
                      color: colors.RED,
                      cursor: 'pointer',
                      fontSize: '9px'
                    }}
                  >
                    ▼ {comment.dislikes}
                  </button>
                </div>
              </div>
            ))}
          </div>
        </>
      </div>
    </div>
  );
};
