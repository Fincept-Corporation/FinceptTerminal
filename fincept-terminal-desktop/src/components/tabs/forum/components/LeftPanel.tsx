// Forum Tab Left Panel Component (Categories & Stats)
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { BarChart3, Users, MessageSquare } from 'lucide-react';
import type { ForumColors, ForumCategoryUI } from '../types';
import { ForumStats } from '@/services/forum/forumApi';
import { TOP_CONTRIBUTORS } from '../constants';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

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
      borderRight: `1px solid ${colors.BORDER}`,
      display: 'flex',
      flexDirection: 'column',
      overflow: 'hidden'
    }}>
      {/* Forum Statistics Section */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.HEADER_BG,
        borderBottom: `1px solid ${colors.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '12px' }}>
          <BarChart3 size={12} color={colors.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: colors.GRAY,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY
          }}>
            STATISTICS
          </span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
          <div style={{
            padding: '8px',
            backgroundColor: colors.DARK_BG,
            border: `1px solid ${colors.BORDER}`,
            borderRadius: '2px'
          }}>
            <div style={{
              fontSize: '9px',
              color: colors.GRAY,
              marginBottom: '4px',
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              TOTAL POSTS
            </div>
            <div style={{
              fontSize: '16px',
              fontWeight: 700,
              color: colors.CYAN,
              fontFamily: FONT_FAMILY
            }}>
              {totalPosts.toLocaleString()}
            </div>
          </div>

          <div style={{
            padding: '8px',
            backgroundColor: colors.DARK_BG,
            border: `1px solid ${colors.BORDER}`,
            borderRadius: '2px'
          }}>
            <div style={{
              fontSize: '9px',
              color: colors.GRAY,
              marginBottom: '4px',
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              ACTIVE USERS
            </div>
            <div style={{
              fontSize: '16px',
              fontWeight: 700,
              color: colors.GREEN,
              fontFamily: FONT_FAMILY
            }}>
              {onlineUsers}
            </div>
          </div>

          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr',
            gap: '8px'
          }}>
            <div style={{
              padding: '6px',
              backgroundColor: colors.DARK_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center'
            }}>
              <div style={{
                fontSize: '8px',
                color: colors.GRAY,
                marginBottom: '2px',
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                TODAY
              </div>
              <div style={{
                fontSize: '12px',
                fontWeight: 700,
                color: colors.YELLOW,
                fontFamily: FONT_FAMILY
              }}>
                {postsToday}
              </div>
            </div>
            <div style={{
              padding: '6px',
              backgroundColor: colors.DARK_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center'
            }}>
              <div style={{
                fontSize: '8px',
                color: colors.GRAY,
                marginBottom: '2px',
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                USERS
              </div>
              <div style={{
                fontSize: '12px',
                fontWeight: 700,
                color: colors.CYAN,
                fontFamily: FONT_FAMILY
              }}>
                {(forumStats?.total_users || 0).toLocaleString()}
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Top Contributors Section */}
      <div style={{
        padding: '12px',
        backgroundColor: colors.HEADER_BG,
        borderBottom: `1px solid ${colors.BORDER}`
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginBottom: '8px' }}>
          <Users size={12} color={colors.ORANGE} />
          <span style={{
            fontSize: '9px',
            fontWeight: 700,
            color: colors.GRAY,
            letterSpacing: '0.5px',
            fontFamily: FONT_FAMILY
          }}>
            TOP CONTRIBUTORS
          </span>
        </div>

        <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
          {TOP_CONTRIBUTORS.map((user, index) => (
            <div
              key={index}
              onClick={() => onViewUserProfile(user.username)}
              style={{
                padding: '8px',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.BORDER}`,
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
                e.currentTarget.style.backgroundColor = colors.DARK_BG;
              }}
            >
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                <span style={{
                  fontSize: '10px',
                  fontWeight: 700,
                  color: colors.WHITE,
                  fontFamily: FONT_FAMILY
                }}>
                  {user.username}
                </span>
                <span style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: user.status === 'ONLINE' ? colors.GREEN : colors.GRAY,
                  display: 'inline-block'
                }}></span>
              </div>
              <div style={{
                display: 'flex',
                gap: '12px',
                fontSize: '8px',
                color: colors.GRAY,
                fontFamily: FONT_FAMILY,
                letterSpacing: '0.5px'
              }}>
                <span>REP: <span style={{ color: colors.CYAN }}>{user.reputation}</span></span>
                <span>POSTS: <span style={{ color: colors.CYAN }}>{user.posts}</span></span>
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Categories Section */}
      <div style={{ flex: 1, overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
        <div style={{
          padding: '12px',
          backgroundColor: colors.HEADER_BG,
          borderBottom: `1px solid ${colors.BORDER}`
        }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
            <MessageSquare size={12} color={colors.ORANGE} />
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              CATEGORIES
            </span>
          </div>
        </div>

        <div className="forum-scroll" style={{
          flex: 1,
          overflowY: 'auto',
          padding: '8px'
        }}>
          {categories.map((cat, index) => (
            <div
              key={index}
              onClick={() => onCategoryChange(cat.name)}
              style={{
                padding: '8px',
                marginBottom: '4px',
                backgroundColor: activeCategory === cat.name ? `${colors.ORANGE}15` : 'transparent',
                borderLeft: activeCategory === cat.name ? `2px solid ${colors.ORANGE}` : '2px solid transparent',
                cursor: 'pointer',
                transition: 'all 0.2s',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center'
              }}
              onMouseEnter={(e) => {
                if (activeCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = colors.HOVER;
                }
              }}
              onMouseLeave={(e) => {
                if (activeCategory !== cat.name) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                }
              }}
            >
              <span style={{
                fontSize: '10px',
                fontWeight: 700,
                color: activeCategory === cat.name ? colors.WHITE : colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                {cat.name}
              </span>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: activeCategory === cat.name ? colors.CYAN : colors.MUTED,
                fontFamily: FONT_FAMILY
              }}>
                {cat.count}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};
