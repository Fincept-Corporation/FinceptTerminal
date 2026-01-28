// Forum Tab Profile Modal Component
// Redesigned to follow Fincept Terminal UI Design System

import React from 'react';
import { X, User, MessageSquare, Award, Calendar, Edit } from 'lucide-react';
import type { ForumColors, ForumUser } from '../../types';

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface ProfileModalProps {
  show: boolean;
  colors: ForumColors;
  profile: ForumUser | null;
  isOwnProfile: boolean;
  onEdit: () => void;
  onClose: () => void;
}

export const ProfileModal: React.FC<ProfileModalProps> = ({
  show,
  colors,
  profile,
  isOwnProfile,
  onEdit,
  onClose
}) => {
  if (!show || !profile) return null;

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
        maxWidth: '600px',
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
            USER PROFILE
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
          {/* Profile Header */}
          <div style={{
            padding: '16px',
            backgroundColor: colors.PANEL_BG,
            border: `1px solid ${colors.BORDER}`,
            borderRadius: '2px',
            marginBottom: '16px',
            textAlign: 'center'
          }}>
            <div style={{
              width: '80px',
              height: '80px',
              backgroundColor: colors.ORANGE,
              borderRadius: '50%',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              margin: '0 auto 16px',
              border: `3px solid ${colors.DARK_BG}`
            }}>
              <User size={40} color={colors.DARK_BG} />
            </div>

            <div style={{
              fontSize: '16px',
              fontWeight: 700,
              color: colors.CYAN,
              marginBottom: '8px',
              fontFamily: FONT_FAMILY,
              letterSpacing: '0.5px'
            }}>
              @{profile.username}
            </div>

            {profile.display_name && (
              <div style={{
                fontSize: '11px',
                color: colors.WHITE,
                marginBottom: '8px',
                fontFamily: FONT_FAMILY
              }}>
                {profile.display_name}
              </div>
            )}

            <div style={{
              display: 'inline-flex',
              alignItems: 'center',
              gap: '4px',
              padding: '4px 12px',
              backgroundColor: profile.status === 'ONLINE' ? `${colors.GREEN}20` : `${colors.GRAY}20`,
              borderRadius: '2px',
              fontSize: '8px',
              fontWeight: 700,
              color: profile.status === 'ONLINE' ? colors.GREEN : colors.GRAY,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY
            }}>
              <span style={{
                width: '6px',
                height: '6px',
                borderRadius: '50%',
                backgroundColor: profile.status === 'ONLINE' ? colors.GREEN : colors.GRAY
              }} />
              {profile.status}
            </div>
          </div>

          {/* Stats Grid */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '1fr 1fr 1fr',
            gap: '12px',
            marginBottom: '16px'
          }}>
            <div style={{
              padding: '12px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center'
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'center',
                marginBottom: '8px'
              }}>
                <MessageSquare size={16} color={colors.CYAN} />
              </div>
              <div style={{
                fontSize: '16px',
                fontWeight: 700,
                color: colors.CYAN,
                marginBottom: '4px',
                fontFamily: FONT_FAMILY
              }}>
                {(profile.post_count || 0).toLocaleString()}
              </div>
              <div style={{
                fontSize: '8px',
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                POSTS
              </div>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center'
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'center',
                marginBottom: '8px'
              }}>
                <MessageSquare size={16} color={colors.PURPLE} />
              </div>
              <div style={{
                fontSize: '16px',
                fontWeight: 700,
                color: colors.PURPLE,
                marginBottom: '4px',
                fontFamily: FONT_FAMILY
              }}>
                {(profile.comment_count || 0).toLocaleString()}
              </div>
              <div style={{
                fontSize: '8px',
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                COMMENTS
              </div>
            </div>

            <div style={{
              padding: '12px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              textAlign: 'center'
            }}>
              <div style={{
                display: 'flex',
                justifyContent: 'center',
                marginBottom: '8px'
              }}>
                <Award size={16} color={colors.YELLOW} />
              </div>
              <div style={{
                fontSize: '16px',
                fontWeight: 700,
                color: colors.YELLOW,
                marginBottom: '4px',
                fontFamily: FONT_FAMILY
              }}>
                {(profile.reputation || 0).toLocaleString()}
              </div>
              <div style={{
                fontSize: '8px',
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                REPUTATION
              </div>
            </div>
          </div>

          {/* Bio Section */}
          {profile.bio && (
            <div style={{
              padding: '16px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderRadius: '2px',
              marginBottom: '16px'
            }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY,
                marginBottom: '8px'
              }}>
                BIO
              </div>
              <div style={{
                fontSize: '10px',
                color: colors.WHITE,
                fontFamily: FONT_FAMILY,
                lineHeight: '1.6',
                fontStyle: 'italic'
              }}>
                {profile.bio}
              </div>
            </div>
          )}

          {/* Signature Section */}
          {profile.signature && (
            <div style={{
              padding: '16px',
              backgroundColor: colors.PANEL_BG,
              border: `1px solid ${colors.BORDER}`,
              borderLeft: `3px solid ${colors.YELLOW}`,
              borderRadius: '2px',
              marginBottom: '16px'
            }}>
              <div style={{
                fontSize: '9px',
                fontWeight: 700,
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY,
                marginBottom: '8px'
              }}>
                SIGNATURE
              </div>
              <div style={{
                fontSize: '10px',
                color: colors.YELLOW,
                fontFamily: FONT_FAMILY,
                lineHeight: '1.6',
                fontStyle: 'italic'
              }}>
                {profile.signature}
              </div>
            </div>
          )}

          {/* Member Since */}
          <div style={{
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            padding: '12px',
            backgroundColor: colors.PANEL_BG,
            border: `1px solid ${colors.BORDER}`,
            borderRadius: '2px',
            marginBottom: '16px'
          }}>
            <Calendar size={14} color={colors.GRAY} />
            <div>
              <span style={{
                fontSize: '9px',
                color: colors.GRAY,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                MEMBER SINCE:{' '}
              </span>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: colors.WHITE,
                letterSpacing: '0.5px',
                fontFamily: FONT_FAMILY
              }}>
                {profile.joined}
              </span>
            </div>
          </div>

          {/* Edit Button */}
          {isOwnProfile && (
            <button
              onClick={onEdit}
              style={{
                width: '100%',
                padding: '12px',
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
                justifyContent: 'center',
                gap: '8px',
                transition: 'all 0.2s'
              }}
              onMouseEnter={(e) => e.currentTarget.style.backgroundColor = colors.YELLOW}
              onMouseLeave={(e) => e.currentTarget.style.backgroundColor = colors.ORANGE}
            >
              <Edit size={12} />
              EDIT PROFILE
            </button>
          )}
        </div>
      </div>
    </div>
  );
};
