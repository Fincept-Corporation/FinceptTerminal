// Fyers Profile Panel
// Display user profile information

import React, { useState, useEffect } from 'react';
import { User } from 'lucide-react';
import { fyersService, FyersProfile } from '../../../services/fyersService';

const FyersProfilePanel: React.FC = () => {
  const [profile, setProfile] = useState<FyersProfile | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState('');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  useEffect(() => {
    loadProfile();
  }, []);

  const loadProfile = async () => {
    setIsLoading(true);
    setError('');

    try {
      const data = await fyersService.getProfile();
      setProfile(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load profile');
    } finally {
      setIsLoading(false);
    }
  };

  if (isLoading) {
    return (
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `1px solid ${BLOOMBERG_GRAY}`,
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ color: BLOOMBERG_GRAY, fontSize: '10px' }}>Loading profile...</div>
      </div>
    );
  }

  if (error || !profile) {
    return (
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `1px solid #FF0000`,
        padding: '12px',
        marginBottom: '12px'
      }}>
        <div style={{ color: '#FF0000', fontSize: '10px' }}>{error || 'Failed to load profile'}</div>
      </div>
    );
  }

  return (
    <div style={{
      backgroundColor: BLOOMBERG_PANEL_BG,
      border: `1px solid ${BLOOMBERG_ORANGE}`,
      padding: '12px',
      marginBottom: '12px'
    }}>
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '8px',
        marginBottom: '12px',
        paddingBottom: '8px',
        borderBottom: `1px solid ${BLOOMBERG_GRAY}`
      }}>
        <User size={12} color={BLOOMBERG_ORANGE} />
        <span style={{ color: BLOOMBERG_ORANGE, fontSize: '11px', fontWeight: 'bold' }}>
          PROFILE
        </span>
      </div>

      <div style={{ display: 'flex', alignItems: 'flex-start', gap: '12px' }}>
        {/* Profile Image */}
        {profile.image && (
          <img
            src={profile.image}
            alt={profile.name}
            style={{
              width: '50px',
              height: '50px',
              borderRadius: '2px',
              border: `1px solid ${BLOOMBERG_GRAY}`
            }}
          />
        )}

        {/* Profile Details */}
        <div style={{ flex: 1 }}>
          <div style={{ display: 'grid', gridTemplateColumns: '120px 1fr', gap: '8px', fontSize: '10px' }}>
            <div style={{ color: BLOOMBERG_GRAY }}>Name:</div>
            <div style={{ color: BLOOMBERG_WHITE, fontWeight: 'bold' }}>{profile.name}</div>

            <div style={{ color: BLOOMBERG_GRAY }}>Display Name:</div>
            <div style={{ color: BLOOMBERG_WHITE }}>{profile.display_name}</div>

            <div style={{ color: BLOOMBERG_GRAY }}>Email:</div>
            <div style={{ color: BLOOMBERG_WHITE }}>{profile.email_id}</div>

            <div style={{ color: BLOOMBERG_GRAY }}>PAN:</div>
            <div style={{ color: BLOOMBERG_WHITE, fontFamily: 'Consolas, monospace' }}>{profile.PAN}</div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FyersProfilePanel;
