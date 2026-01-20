// Forum Tab Profile Modal Component (View and Edit modes)

import React from 'react';
import type { ForumColors, ForumUser, ProfileEdit } from '../../types';

interface ProfileModalProps {
  colors: ForumColors;
  showProfile: boolean;
  showEditProfile: boolean;
  userProfile: ForumUser | null;
  profileEdit: ProfileEdit;
  isOwnProfile: boolean;
  onProfileEditChange: (edit: ProfileEdit) => void;
  onCloseProfile: () => void;
  onCloseEditProfile: () => void;
  onOpenEditProfile: () => void;
  onSaveProfile: () => void;
}

export const ProfileModal: React.FC<ProfileModalProps> = ({
  colors,
  showProfile,
  showEditProfile,
  userProfile,
  profileEdit,
  isOwnProfile,
  onProfileEditChange,
  onCloseProfile,
  onCloseEditProfile,
  onOpenEditProfile,
  onSaveProfile
}) => {
  // Profile View Modal
  if (showProfile && userProfile) {
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
          <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '12px' }}>
            <div style={{ color: colors.ORANGE, fontSize: '14px', fontWeight: 'bold' }}>
              FORUM PROFILE
            </div>
            <button
              onClick={onCloseProfile}
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

          <div style={{ marginBottom: '12px' }}>
            <div style={{ color: colors.CYAN, fontSize: '16px', fontWeight: 'bold', marginBottom: '8px' }}>
              @{userProfile.username}
            </div>
            <div style={{ color: colors.WHITE, fontSize: '12px', marginBottom: '4px' }}>
              {userProfile.display_name || 'No display name set'}
            </div>
            <div style={{ color: colors.GRAY, fontSize: '11px', marginBottom: '12px', fontStyle: 'italic' }}>
              {userProfile.bio || 'No bio available'}
            </div>

            <div style={{ display: 'flex', gap: '16px', fontSize: '11px', marginBottom: '12px' }}>
              <div>
                <span style={{ color: colors.GRAY }}>Posts: </span>
                <span style={{ color: colors.WHITE }}>{userProfile.post_count || 0}</span>
              </div>
              <div>
                <span style={{ color: colors.GRAY }}>Comments: </span>
                <span style={{ color: colors.WHITE }}>{userProfile.comment_count || 0}</span>
              </div>
              <div>
                <span style={{ color: colors.GRAY }}>Reputation: </span>
                <span style={{ color: colors.GREEN }}>{userProfile.reputation || 0}</span>
              </div>
            </div>

            {userProfile.signature && (
              <div style={{
                backgroundColor: 'rgba(255,255,255,0.02)',
                padding: '8px',
                marginBottom: '12px',
                borderLeft: `2px solid ${colors.YELLOW}`
              }}>
                <div style={{ color: colors.YELLOW, fontSize: '10px', marginBottom: '4px' }}>SIGNATURE:</div>
                <div style={{ color: colors.GRAY, fontSize: '10px', fontStyle: 'italic' }}>
                  {userProfile.signature}
                </div>
              </div>
            )}

            {isOwnProfile && (
              <button
                onClick={onOpenEditProfile}
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
                EDIT PROFILE
              </button>
            )}
          </div>
        </div>
      </div>
    );
  }

  // Edit Profile Modal
  if (showEditProfile) {
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
            EDIT PROFILE
          </div>
          <div style={{ borderBottom: `1px solid ${colors.GRAY}`, marginBottom: '12px' }}></div>

          <div style={{ marginBottom: '12px' }}>
            <div style={{ color: colors.WHITE, fontSize: '11px', marginBottom: '4px' }}>Display Name:</div>
            <input
              type="text"
              value={profileEdit.display_name}
              onChange={(e) => onProfileEditChange({ ...profileEdit, display_name: e.target.value })}
              style={{
                width: '100%',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.GRAY}`,
                color: colors.WHITE,
                padding: '6px',
                fontSize: '12px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          <div style={{ marginBottom: '12px' }}>
            <div style={{ color: colors.WHITE, fontSize: '11px', marginBottom: '4px' }}>Bio:</div>
            <textarea
              value={profileEdit.bio}
              onChange={(e) => onProfileEditChange({ ...profileEdit, bio: e.target.value })}
              rows={4}
              style={{
                width: '100%',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.GRAY}`,
                color: colors.WHITE,
                padding: '6px',
                fontSize: '11px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          <div style={{ marginBottom: '12px' }}>
            <div style={{ color: colors.WHITE, fontSize: '11px', marginBottom: '4px' }}>Signature:</div>
            <input
              type="text"
              value={profileEdit.signature}
              onChange={(e) => onProfileEditChange({ ...profileEdit, signature: e.target.value })}
              style={{
                width: '100%',
                backgroundColor: colors.DARK_BG,
                border: `1px solid ${colors.GRAY}`,
                color: colors.WHITE,
                padding: '6px',
                fontSize: '12px',
                fontFamily: 'Consolas, monospace'
              }}
            />
          </div>

          <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
            <button
              onClick={onCloseEditProfile}
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
              onClick={onSaveProfile}
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
              SAVE CHANGES
            </button>
          </div>
        </div>
      </div>
    );
  }

  return null;
};
