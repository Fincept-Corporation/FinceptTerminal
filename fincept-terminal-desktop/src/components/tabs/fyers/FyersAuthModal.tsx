// Fyers Authentication Modal
// Automated authentication with TOTP

import React, { useState } from 'react';
import { X } from 'lucide-react';
import { fyersService } from '../../../services/fyersService';
import { authenticateFyers, FyersAuthConfig } from '../../../services/fyersAuth';

interface FyersAuthModalProps {
  onClose: () => void;
  onSuccess: () => void;
}

const FyersAuthModal: React.FC<FyersAuthModalProps> = ({ onClose, onSuccess }) => {
  const [clientId, setClientId] = useState('');
  const [pin, setPin] = useState('');
  const [appId, setAppId] = useState('');
  const [appType, setAppType] = useState('100');
  const [appSecret, setAppSecret] = useState('');
  const [totpKey, setTotpKey] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState('');
  const [statusMessage, setStatusMessage] = useState('');

  const BLOOMBERG_ORANGE = '#FFA500';
  const BLOOMBERG_WHITE = '#FFFFFF';
  const BLOOMBERG_GRAY = '#787878';
  const BLOOMBERG_DARK_BG = '#000000';
  const BLOOMBERG_PANEL_BG = '#0a0a0a';

  const handleConnect = async () => {
    if (!clientId || !pin || !appId || !appSecret || !totpKey) {
      setError('Please fill all required fields');
      return;
    }

    setIsLoading(true);
    setError('');
    setStatusMessage('Starting authentication...');

    try {
      const config: FyersAuthConfig = {
        clientId,
        pin,
        appId,
        appType,
        appSecret,
        totpKey
      };

      setStatusMessage('Generating TOTP and authenticating...');
      const authResult = await authenticateFyers(config);

      setStatusMessage('Saving credentials...');

      // Save to database with appType
      await fyersService.saveCredentials({
        appId,
        appType,
        redirectUrl: 'https://trade.fyers.in/api-login/redirect-uri/index.html',
        accessToken: authResult.accessToken
      });

      // Also save the full config for re-authentication
      const { sqliteService } = await import('../../../services/sqliteService');
      await sqliteService.saveCredential({
        service_name: 'fyers_config',
        username: clientId,
        password: pin,
        api_key: appId,
        api_secret: appSecret,
        additional_data: JSON.stringify({
          appType,
          totpKey,
          connectionString: authResult.connectionString
        })
      });

      setStatusMessage('Testing connection...');
      await fyersService.getProfile();

      setStatusMessage('Connected successfully!');
      setTimeout(() => {
        setIsLoading(false);
        onSuccess();
      }, 1000);
    } catch (err) {
      setIsLoading(false);
      setStatusMessage('');
      setError(err instanceof Error ? err.message : 'Authentication failed');
    }
  };

  return (
    <div style={{
      position: 'fixed',
      top: 0,
      left: 0,
      right: 0,
      bottom: 0,
      backgroundColor: 'rgba(0, 0, 0, 0.9)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 1000,
      fontFamily: 'Consolas, monospace'
    }}>
      <div style={{
        backgroundColor: BLOOMBERG_PANEL_BG,
        border: `2px solid ${BLOOMBERG_ORANGE}`,
        width: '500px',
        maxWidth: '90%'
      }}>
        {/* Header */}
        <div style={{
          backgroundColor: BLOOMBERG_DARK_BG,
          borderBottom: `1px solid ${BLOOMBERG_ORANGE}`,
          padding: '10px 12px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center'
        }}>
          <span style={{
            color: BLOOMBERG_ORANGE,
            fontSize: '12px',
            fontWeight: 'bold'
          }}>
            CONNECT FYERS ACCOUNT
          </span>
          <button
            onClick={onClose}
            style={{
              backgroundColor: 'transparent',
              border: 'none',
              color: BLOOMBERG_WHITE,
              cursor: 'pointer',
              padding: '4px'
            }}
          >
            <X size={16} />
          </button>
        </div>

        {/* Body */}
        <div style={{ padding: '20px' }}>
          <div style={{
            color: BLOOMBERG_GRAY,
            fontSize: '10px',
            marginBottom: '20px',
            lineHeight: '1.5'
          }}>
            Enter your Fyers credentials. Authentication will be automated using TOTP.
          </div>

          {/* Client ID */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              Client ID *
            </label>
            <input
              type="text"
              value={clientId}
              onChange={(e) => setClientId(e.target.value.toUpperCase())}
              placeholder="XB02507"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* PIN */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              PIN *
            </label>
            <input
              type="password"
              value={pin}
              onChange={(e) => setPin(e.target.value)}
              placeholder="1590"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* App ID */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              App ID *
            </label>
            <input
              type="text"
              value={appId}
              onChange={(e) => setAppId(e.target.value.toUpperCase())}
              placeholder="NDM5TDN9DB"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* App Type */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              App Type
            </label>
            <input
              type="text"
              value={appType}
              onChange={(e) => setAppType(e.target.value)}
              placeholder="100"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* App Secret */}
          <div style={{ marginBottom: '12px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              App Secret *
            </label>
            <input
              type="password"
              value={appSecret}
              onChange={(e) => setAppSecret(e.target.value.toUpperCase())}
              placeholder="UR9IJU1U0E"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '10px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* TOTP Key */}
          <div style={{ marginBottom: '15px' }}>
            <label style={{
              display: 'block',
              color: BLOOMBERG_WHITE,
              fontSize: '10px',
              marginBottom: '5px',
              fontWeight: 'bold'
            }}>
              TOTP Key *
            </label>
            <input
              type="password"
              value={totpKey}
              onChange={(e) => setTotpKey(e.target.value.toUpperCase())}
              placeholder="XOYMD3QRDV6QPNSWK3WSKMD642KGODJQ"
              disabled={isLoading}
              style={{
                width: '100%',
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px',
                fontSize: '9px',
                fontFamily: 'Consolas, monospace',
                boxSizing: 'border-box'
              }}
            />
          </div>

          {/* Status Message */}
          {statusMessage && (
            <div style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: `1px solid ${BLOOMBERG_ORANGE}`,
              color: BLOOMBERG_ORANGE,
              padding: '8px',
              fontSize: '9px',
              marginBottom: '15px',
              textAlign: 'center'
            }}>
              {statusMessage}
            </div>
          )}

          {/* Error Message */}
          {error && (
            <div style={{
              backgroundColor: BLOOMBERG_DARK_BG,
              border: '1px solid #FF0000',
              color: '#FF0000',
              padding: '8px',
              fontSize: '9px',
              marginBottom: '15px'
            }}>
              {error}
            </div>
          )}

          {/* Actions */}
          <div style={{ display: 'flex', gap: '10px', justifyContent: 'flex-end' }}>
            <button
              onClick={onClose}
              disabled={isLoading}
              style={{
                backgroundColor: BLOOMBERG_DARK_BG,
                border: `1px solid ${BLOOMBERG_GRAY}`,
                color: BLOOMBERG_WHITE,
                padding: '8px 16px',
                fontSize: '10px',
                cursor: isLoading ? 'not-allowed' : 'pointer',
                fontFamily: 'Consolas, monospace',
                opacity: isLoading ? 0.5 : 1
              }}
            >
              CANCEL
            </button>
            <button
              onClick={handleConnect}
              disabled={isLoading}
              style={{
                backgroundColor: BLOOMBERG_ORANGE,
                border: 'none',
                color: 'black',
                padding: '8px 16px',
                fontSize: '10px',
                fontWeight: 'bold',
                cursor: isLoading ? 'not-allowed' : 'pointer',
                fontFamily: 'Consolas, monospace',
                opacity: isLoading ? 0.5 : 1
              }}
            >
              {isLoading ? 'CONNECTING...' : 'CONNECT'}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default FyersAuthModal;
