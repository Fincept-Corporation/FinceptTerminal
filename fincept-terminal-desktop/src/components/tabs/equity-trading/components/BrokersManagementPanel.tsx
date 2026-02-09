/**
 * Brokers Management Panel
 *
 * A unified panel to manage stock broker connections:
 * - View all available brokers with connection status
 * - Configure API credentials
 * - Authenticate via OAuth
 * - Check token expiry and re-authenticate
 */

import React, { useState, useEffect, useMemo, useRef } from 'react';
import {
  Settings,
  CheckCircle2,
  XCircle,
  AlertTriangle,
  ExternalLink,
  Eye,
  EyeOff,
  Save,
  LogOut,
  RefreshCw,
  Loader2,
  Clock,
  Globe,
  Zap,
  ChevronRight,
  Search,
  Key,
} from 'lucide-react';
import { open } from '@tauri-apps/plugin-shell';
import {
  useStockBrokerContext,
  useStockBrokerSelection,
  useStockBrokerAuth,
} from '@/contexts/StockBrokerContext';
import { createStockBrokerAdapter } from '@/brokers/stocks';
import { withTimeout } from '@/services/core/apiUtils';
import type { BrokerCredentials, StockBrokerMetadata, Region } from '@/brokers/stocks/types';

// Fincept color palette
const COLORS = {
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#141414',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  MUTED: '#4A4A4A',
  WHITE: '#FFFFFF',
};

type ConnectionStatus = 'connected' | 'expired' | 'configured' | 'not_configured';

interface BrokerStatus {
  brokerId: string;
  metadata: StockBrokerMetadata;
  status: ConnectionStatus;
  lastAuthTime?: Date;
  hasCredentials: boolean;
}

// API credential URLs for brokers
const API_KEYS_URLS: Record<string, string> = {
  zerodha: 'https://developers.kite.trade/',
  fyers: 'https://myapi.fyers.in/dashboard',
  angelone: 'https://smartapi.angelbroking.com/publisher-login',
  angel: 'https://smartapi.angelbroking.com/',
  dhan: 'https://dhanhq.co/api',
  upstox: 'https://upstox.com/developer/api/',
  kotak: 'https://napi.kotaksecurities.com/devportal/',
  saxobank: 'https://www.developer.saxo/openapi/appmanagement',
  alpaca: 'https://app.alpaca.markets/brokerage/new-account',
  ibkr: 'https://www.interactivebrokers.com/en/trading/ib-api.php',
  tradier: 'https://developer.tradier.com/',
};

// Region labels
const REGION_LABELS: Record<Region, string> = {
  india: 'India',
  us: 'United States',
  europe: 'Europe',
  uk: 'United Kingdom',
  asia: 'Asia Pacific',
  australia: 'Australia',
  canada: 'Canada',
};

export function BrokersManagementPanel() {
  const { availableBrokers, activeBroker, setActiveBroker, adapter, tradingMode } = useStockBrokerContext();
  const { isAuthenticated, authenticate, logout, getAuthUrl, isConnecting, error, clearError } = useStockBrokerAuth();

  // UI State
  const [selectedBrokerId, setSelectedBrokerId] = useState<string | null>(activeBroker);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedRegion, setSelectedRegion] = useState<Region | 'all'>('all');
  const [showConfigPanel, setShowConfigPanel] = useState(false);

  // Config State
  const [showSecret, setShowSecret] = useState(false);
  const [config, setConfig] = useState<BrokerCredentials>({ apiKey: '', apiSecret: '', userId: '' });
  const [mobileNumber, setMobileNumber] = useState(''); // Kotak-specific
  const [isSaving, setIsSaving] = useState(false);
  const [saveSuccess, setSaveSuccess] = useState(false);
  const [configError, setConfigError] = useState<string | null>(null);

  // Auth State
  const [callbackUrl, setCallbackUrl] = useState('');
  const [showCallbackInput, setShowCallbackInput] = useState(false);
  const [isCheckingToken, setIsCheckingToken] = useState(false);

  // Get broker statuses
  const [brokerStatuses, setBrokerStatuses] = useState<Map<string, BrokerStatus>>(new Map());
  const mountedRef = useRef(true);

  useEffect(() => {
    mountedRef.current = true;
    return () => { mountedRef.current = false; };
  }, []);

  // Selected broker metadata
  const selectedBroker = useMemo(() => {
    return availableBrokers.find(b => b.id === selectedBrokerId) || null;
  }, [availableBrokers, selectedBrokerId]);

  // Filter brokers
  const filteredBrokers = useMemo(() => {
    return availableBrokers.filter(broker => {
      const matchesSearch = !searchQuery ||
        broker.displayName.toLowerCase().includes(searchQuery.toLowerCase()) ||
        broker.id.toLowerCase().includes(searchQuery.toLowerCase());
      const matchesRegion = selectedRegion === 'all' || broker.region === selectedRegion;
      return matchesSearch && matchesRegion;
    });
  }, [availableBrokers, searchQuery, selectedRegion]);

  // Group brokers by region
  const brokersByRegion = useMemo(() => {
    const grouped = new Map<Region, StockBrokerMetadata[]>();
    filteredBrokers.forEach(broker => {
      const existing = grouped.get(broker.region) || [];
      existing.push(broker);
      grouped.set(broker.region, existing);
    });
    return grouped;
  }, [filteredBrokers]);

  // Get unique regions
  const availableRegions = useMemo(() => {
    const regions = new Set(availableBrokers.map(b => b.region));
    return Array.from(regions);
  }, [availableBrokers]);

  // Check broker statuses on mount - SEQUENTIAL with delay to avoid request spam
  useEffect(() => {
    let isCancelled = false;

    const checkBrokerStatuses = async () => {
      const statuses = new Map<string, BrokerStatus>();

      // Sequential check with delay between brokers
      for (const broker of availableBrokers) {
        if (isCancelled) break;

        try {
          const tempAdapter = createStockBrokerAdapter(broker.id);
          const storedCreds: any = await withTimeout(
            (tempAdapter as any).loadCredentials?.() ?? Promise.resolve(null),
            10000
          );
          if (isCancelled || !mountedRef.current) break;

          let status: ConnectionStatus = 'not_configured';
          let hasCredentials = false;

          if (storedCreds?.apiKey) {
            hasCredentials = true;
            status = 'configured';

            if (storedCreds.accessToken) {
              // Has token - check if it's the active connected broker
              if (broker.id === activeBroker && isAuthenticated) {
                status = 'connected';
              } else {
                // Token exists but may be expired - mark as configured
                status = 'configured';
              }
            }
          }

          statuses.set(broker.id, {
            brokerId: broker.id,
            metadata: broker,
            status,
            hasCredentials,
          });

          // Update state incrementally
          if (mountedRef.current && !isCancelled) {
            setBrokerStatuses(new Map(statuses));
          }
        } catch (err) {
          statuses.set(broker.id, {
            brokerId: broker.id,
            metadata: broker,
            status: 'not_configured',
            hasCredentials: false,
          });
        }

        // Small delay between broker checks (100ms) to avoid spamming
        if (!isCancelled) {
          await new Promise(resolve => setTimeout(resolve, 100));
        }
      }

      if (mountedRef.current && !isCancelled) {
        setBrokerStatuses(statuses);
      }
    };

    checkBrokerStatuses();

    return () => {
      isCancelled = true;
    };
  }, [availableBrokers, activeBroker, isAuthenticated]);

  // Update selected broker when active broker changes
  useEffect(() => {
    if (activeBroker && !selectedBrokerId) {
      setSelectedBrokerId(activeBroker);
    }
  }, [activeBroker, selectedBrokerId]);

  // Load existing config when broker is selected
  useEffect(() => {
    const loadConfig = async () => {
      if (!selectedBrokerId) return;

      try {
        const tempAdapter = createStockBrokerAdapter(selectedBrokerId);
        const storedCreds: any = await withTimeout(
          (tempAdapter as any).loadCredentials?.() ?? Promise.resolve(null),
          10000
        );
        if (!mountedRef.current) return;

        if (storedCreds) {
          const restoredConfig: any = {
            apiKey: storedCreds.apiKey || '',
            apiSecret: storedCreds.apiSecret || '',
            userId: storedCreds.userId || '',
          };

          // Restore AngelOne-specific fields from apiSecret (stored as JSON)
          if (selectedBrokerId === 'angelone' && storedCreds.apiSecret) {
            try {
              const secretData = JSON.parse(storedCreds.apiSecret);
              if (secretData.password) restoredConfig.pin = secretData.password;
              if (secretData.totpSecret) restoredConfig.totpSecret = secretData.totpSecret;
              // Don't show the JSON blob as apiSecret in the UI
              restoredConfig.apiSecret = '';
            } catch {
              // apiSecret is not JSON, keep as-is
            }
          }

          setConfig(restoredConfig);
        } else {
          setConfig({ apiKey: '', apiSecret: '' });
        }
      } catch (err) {
        if (mountedRef.current) {
          setConfig({ apiKey: '', apiSecret: '' });
        }
      }
    };

    loadConfig();
    setShowCallbackInput(false);
    setCallbackUrl('');
    setConfigError(null);
    clearError();
  }, [selectedBrokerId, clearError]);

  // Handle broker selection
  const handleSelectBroker = async (brokerId: string) => {
    setSelectedBrokerId(brokerId);
    setShowConfigPanel(true);

    // Also set as active broker in context
    try {
      await withTimeout(setActiveBroker(brokerId), 10000);
    } catch (err) {
      console.error('Failed to set active broker:', err);
    }
  };

  // Save configuration
  const handleSaveConfig = async () => {
    if (!selectedBroker) return;

    setConfigError(null);
    setSaveSuccess(false);

    if (!config.apiKey?.trim()) {
      setConfigError('API Key is required');
      return;
    }

    if (selectedBroker.authType === 'oauth' && !config.apiSecret?.trim()) {
      setConfigError('API Secret is required for OAuth brokers');
      return;
    }

    setIsSaving(true);

    try {
      const tempAdapter = createStockBrokerAdapter(selectedBrokerId!);

      // Prepare credentials
      const credentials: any = {
        apiKey: config.apiKey,
        apiSecret: config.apiSecret || '',
        userId: config.userId || '',
      };

      // Store broker-specific additional data
      const additionalData: any = {};

      if (selectedBroker.id === 'kotak' && mobileNumber) {
        additionalData.mobileNumber = mobileNumber;
      }

      if (selectedBroker.id === 'aliceblue' && (config as any).encryptionKey) {
        additionalData.encryptionKey = (config as any).encryptionKey;
      }

      if (selectedBroker.id === 'fivepaisa') {
        if ((config as any).email) additionalData.email = (config as any).email;
        if ((config as any).pin) additionalData.pin = (config as any).pin;
      }

      if (selectedBroker.id === 'motilal') {
        if ((config as any).password) additionalData.password = (config as any).password;
        if ((config as any).dob) additionalData.dob = (config as any).dob;
      }

      if (selectedBroker.id === 'angelone') {
        if ((config as any).pin) additionalData.pin = (config as any).pin;
        if ((config as any).totpSecret) additionalData.totpSecret = (config as any).totpSecret;
      }

      if (selectedBroker.id === 'shoonya' && (config as any).password) {
        additionalData.password = (config as any).password;
      }

      // Add additionalData to credentials if it has any fields
      if (Object.keys(additionalData).length > 0) {
        credentials.additionalData = JSON.stringify(additionalData);
      }

      // Store credentials
      await withTimeout((tempAdapter as any).storeCredentials(credentials), 10000);
      if (!mountedRef.current) return;

      // Set on adapter
      if (tempAdapter.setCredentials) {
        tempAdapter.setCredentials(config.apiKey, config.apiSecret || '');
      }

      // Kotak-specific: Set MPIN and mobile number
      if (selectedBroker.id === 'kotak') {
        if (config.userId && (tempAdapter as any).setMpin) {
          (tempAdapter as any).setMpin(config.userId);
        }
        if (mobileNumber && (tempAdapter as any).setMobileNumber) {
          (tempAdapter as any).setMobileNumber(mobileNumber);
        }
      }

      // Shoonya-specific: Authenticate immediately with TOTP
      if (selectedBroker.id === 'shoonya') {
        const shoonyaCredentials: any = {
          apiKey: config.apiKey,
          apiSecret: config.apiSecret || '',
          userId: config.userId || '',
          password: (config as any).password || '',
          totpCode: (config as any).totpCode || '',
        };

        const authResult = await withTimeout(tempAdapter.authenticate(shoonyaCredentials), 30000);
        if (!mountedRef.current) return;

        if (!authResult.success) {
          setConfigError(authResult.message || 'Shoonya authentication failed');
          return;
        }

        // Update broker statuses with connected state
        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
        return;
      }

      // AngelOne-specific: Authenticate immediately with Client ID, PIN, TOTP Secret
      if (selectedBroker.id === 'angelone') {
        const angeloneCredentials: any = {
          apiKey: config.apiKey,
          userId: config.userId || '',
          password: (config as any).pin || '', // Adapter expects 'password' field
          totpSecret: (config as any).totpSecret || '', // TOTP secret for automated code generation
        };

        // Set this as the active broker BEFORE authenticating via context
        await withTimeout(setActiveBroker(selectedBroker.id), 10000);
        if (!mountedRef.current) return;

        // Use the context's authenticate() so the global state is updated
        const authResult = await withTimeout(authenticate(angeloneCredentials), 30000);
        if (!mountedRef.current) return;

        if (!authResult.success) {
          setConfigError(authResult.message || 'Angel One authentication failed');
          return;
        }

        // Update broker statuses with connected state
        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
        return;
      }

      // AliceBlue-specific: Authenticate with User ID + Encryption Key
      if (selectedBroker.id === 'aliceblue') {
        const aliceblueCredentials: any = {
          apiKey: config.apiKey,
          apiSecret: config.apiSecret || '',
          userId: config.userId || '',
          encKey: (config as any).encryptionKey || '', // Adapter expects 'encKey' field
        };

        const authResult = await withTimeout(tempAdapter.authenticate(aliceblueCredentials), 30000);
        if (!mountedRef.current) return;

        if (!authResult.success) {
          setConfigError(authResult.message || 'AliceBlue authentication failed');
          return;
        }

        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
        return;
      }

      // 5Paisa-specific: Authenticate with Email, PIN, TOTP
      if (selectedBroker.id === 'fivepaisa') {
        const fivepaisaCredentials: any = {
          apiKey: config.apiKey,
          apiSecret: config.apiSecret || '',
          email: (config as any).email || '',
          pin: (config as any).pin || '',
          totp: (config as any).totpCode || '', // Adapter expects 'totp' field
        };

        const authResult = await withTimeout(tempAdapter.authenticate(fivepaisaCredentials), 30000);
        if (!mountedRef.current) return;

        if (!authResult.success) {
          setConfigError(authResult.message || '5Paisa authentication failed');
          return;
        }

        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
        return;
      }

      // Motilal Oswal-specific: Authenticate with User ID, Password, DOB, TOTP
      if (selectedBroker.id === 'motilal') {
        const motilalCredentials: any = {
          apiKey: config.apiKey,
          apiSecret: (config as any).password || '', // Adapter reads password from apiSecret
          userId: config.userId || '',
          password: (config as any).password || '', // Also provide as password field
          pin: (config as any).dob || '', // Adapter expects DOB in 'pin' field
          totpSecret: (config as any).totpCode || '', // Adapter expects 'totpSecret' field
        };

        const authResult = await withTimeout(tempAdapter.authenticate(motilalCredentials), 30000);
        if (!mountedRef.current) return;

        if (!authResult.success) {
          setConfigError(authResult.message || 'Motilal Oswal authentication failed');
          return;
        }

        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
        return;
      }

      // Update broker statuses
      if (mountedRef.current) {
        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'configured', hasCredentials: true });
          }
          return newMap;
        });

        setSaveSuccess(true);
        setTimeout(() => { if (mountedRef.current) setSaveSuccess(false); }, 3000);
      }
    } catch (err) {
      if (mountedRef.current) {
        setConfigError(err instanceof Error ? err.message : 'Failed to save configuration');
      }
    } finally {
      if (mountedRef.current) {
        setIsSaving(false);
      }
    }
  };

  // Handle OAuth login
  const handleOAuthLogin = async () => {
    try {
      const authUrl = await withTimeout(getAuthUrl(), 15000);
      if (!mountedRef.current) return;
      if (authUrl) {
        await open(authUrl);
        if (mountedRef.current) {
          setShowCallbackInput(true);
        }
      }
    } catch (err) {
      console.error('Failed to get auth URL:', err);
    }
  };

  // Handle callback URL submission
  const handleCallbackSubmit = async () => {
    try {
      const url = new URL(callbackUrl);
      const requestToken = url.searchParams.get('request_token');
      const authCode = url.searchParams.get('auth_code');
      const code = url.searchParams.get('code');

      const token = requestToken || authCode || code;

      if (!token) {
        setConfigError('No auth code found in callback URL');
        return;
      }

      const authCredentials: BrokerCredentials = {
        apiKey: config.apiKey,
        apiSecret: config.apiSecret,
      };

      if (requestToken) {
        (authCredentials as any).requestToken = requestToken;
      } else if (authCode) {
        (authCredentials as any).authCode = authCode;
      } else {
        authCredentials.accessToken = token;
      }

      const response = await withTimeout(authenticate(authCredentials), 30000);
      if (!mountedRef.current) return;

      if (response.success) {
        setShowCallbackInput(false);
        setCallbackUrl('');

        // Update broker status
        setBrokerStatuses(prev => {
          const newMap = new Map(prev);
          const existing = newMap.get(selectedBrokerId!);
          if (existing) {
            newMap.set(selectedBrokerId!, { ...existing, status: 'connected' });
          }
          return newMap;
        });
      }
    } catch (err) {
      if (mountedRef.current) {
        setConfigError('Failed to process callback URL');
      }
    }
  };

  // Handle logout/disconnect
  const handleDisconnect = async () => {
    try {
      await withTimeout(logout(), 10000);
    } catch (err) {
      console.error('Disconnect timeout:', err);
    }

    if (!mountedRef.current) return;

    // Update broker status
    setBrokerStatuses(prev => {
      const newMap = new Map(prev);
      const existing = newMap.get(selectedBrokerId!);
      if (existing) {
        newMap.set(selectedBrokerId!, { ...existing, status: existing.hasCredentials ? 'configured' : 'not_configured' });
      }
      return newMap;
    });
  };

  // Check token validity
  const handleCheckToken = async () => {
    if (!adapter) return;

    setIsCheckingToken(true);
    try {
      // Try to fetch funds - if it fails, token is expired
      await withTimeout(adapter.getFunds(), 15000);
      if (!mountedRef.current) return;

      // Update status to connected
      setBrokerStatuses(prev => {
        const newMap = new Map(prev);
        const existing = newMap.get(selectedBrokerId!);
        if (existing) {
          newMap.set(selectedBrokerId!, { ...existing, status: 'connected' });
        }
        return newMap;
      });
    } catch (err) {
      if (!mountedRef.current) return;
      // Token expired
      setBrokerStatuses(prev => {
        const newMap = new Map(prev);
        const existing = newMap.get(selectedBrokerId!);
        if (existing) {
          newMap.set(selectedBrokerId!, { ...existing, status: 'expired' });
        }
        return newMap;
      });
      setConfigError('Session expired. Please re-authenticate.');
    } finally {
      if (mountedRef.current) {
        setIsCheckingToken(false);
      }
    }
  };

  // Get status badge
  const getStatusBadge = (status: ConnectionStatus) => {
    switch (status) {
      case 'connected':
        return (
          <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.GREEN, fontSize: '10px' }}>
            <CheckCircle2 size={12} />
            CONNECTED
          </span>
        );
      case 'expired':
        return (
          <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.YELLOW, fontSize: '10px' }}>
            <AlertTriangle size={12} />
            EXPIRED
          </span>
        );
      case 'configured':
        return (
          <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.CYAN, fontSize: '10px' }}>
            <Key size={12} />
            CONFIGURED
          </span>
        );
      default:
        return (
          <span style={{ display: 'flex', alignItems: 'center', gap: '4px', color: COLORS.GRAY, fontSize: '10px' }}>
            <XCircle size={12} />
            NOT CONFIGURED
          </span>
        );
    }
  };

  return (
    <div style={{ display: 'flex', height: '100%', backgroundColor: COLORS.DARK_BG, fontFamily: '"IBM Plex Mono", monospace' }}>
      {/* Left Panel - Broker List */}
      <div style={{ width: '320px', borderRight: `1px solid ${COLORS.BORDER}`, display: 'flex', flexDirection: 'column' }}>
        {/* Search & Filter */}
        <div style={{ padding: '12px', borderBottom: `1px solid ${COLORS.BORDER}` }}>
          <div style={{ position: 'relative', marginBottom: '8px' }}>
            <Search size={14} color={COLORS.GRAY} style={{ position: 'absolute', left: '10px', top: '50%', transform: 'translateY(-50%)' }} />
            <input
              type="text"
              placeholder="Search brokers..."
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              style={{
                width: '100%',
                padding: '8px 8px 8px 32px',
                backgroundColor: COLORS.PANEL_BG,
                border: `1px solid ${COLORS.BORDER}`,
                color: COLORS.WHITE,
                fontSize: '11px',
                outline: 'none',
                boxSizing: 'border-box',
              }}
            />
          </div>

          <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
            <button
              onClick={() => setSelectedRegion('all')}
              style={{
                padding: '4px 8px',
                fontSize: '9px',
                backgroundColor: selectedRegion === 'all' ? COLORS.ORANGE : COLORS.PANEL_BG,
                color: selectedRegion === 'all' ? COLORS.DARK_BG : COLORS.GRAY,
                border: `1px solid ${selectedRegion === 'all' ? COLORS.ORANGE : COLORS.BORDER}`,
                cursor: 'pointer',
                fontWeight: 600,
              }}
            >
              ALL
            </button>
            {availableRegions.map(region => (
              <button
                key={region}
                onClick={() => setSelectedRegion(region)}
                style={{
                  padding: '4px 8px',
                  fontSize: '9px',
                  backgroundColor: selectedRegion === region ? COLORS.ORANGE : COLORS.PANEL_BG,
                  color: selectedRegion === region ? COLORS.DARK_BG : COLORS.GRAY,
                  border: `1px solid ${selectedRegion === region ? COLORS.ORANGE : COLORS.BORDER}`,
                  cursor: 'pointer',
                  fontWeight: 600,
                }}
              >
                {REGION_LABELS[region]?.toUpperCase() || region.toUpperCase()}
              </button>
            ))}
          </div>
        </div>

        {/* Broker List */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '8px' }}>
          {Array.from(brokersByRegion.entries()).map(([region, brokers]) => (
            <div key={region} style={{ marginBottom: '16px' }}>
              <div style={{ padding: '4px 8px', color: COLORS.GRAY, fontSize: '9px', fontWeight: 600, letterSpacing: '1px' }}>
                {REGION_LABELS[region] || region}
              </div>
              {brokers.map(broker => {
                const status = brokerStatuses.get(broker.id);
                const isSelected = selectedBrokerId === broker.id;
                const isActive = activeBroker === broker.id && isAuthenticated;

                return (
                  <div
                    key={broker.id}
                    onClick={() => handleSelectBroker(broker.id)}
                    style={{
                      padding: '10px 12px',
                      backgroundColor: isSelected ? COLORS.HEADER_BG : 'transparent',
                      border: `1px solid ${isSelected ? COLORS.ORANGE : 'transparent'}`,
                      cursor: 'pointer',
                      marginBottom: '4px',
                      transition: 'all 0.15s ease',
                    }}
                  >
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <div>
                        <div style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 600 }}>
                          {broker.displayName}
                          {isActive && (
                            <span style={{ marginLeft: '6px', padding: '2px 6px', backgroundColor: COLORS.GREEN, color: COLORS.DARK_BG, fontSize: '8px', fontWeight: 700 }}>
                              ACTIVE
                            </span>
                          )}
                        </div>
                        <div style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '2px' }}>
                          {broker.authType === 'oauth' ? 'OAuth' : broker.authType === 'totp' ? 'TOTP' : 'API Key'}
                        </div>
                      </div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                        {getStatusBadge(status?.status || 'not_configured')}
                        <ChevronRight size={14} color={COLORS.GRAY} />
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>
          ))}
        </div>
      </div>

      {/* Right Panel - Configuration */}
      <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden' }}>
        {selectedBroker ? (
          <>
            {/* Broker Header */}
            <div style={{ padding: '16px 20px', borderBottom: `1px solid ${COLORS.BORDER}`, backgroundColor: COLORS.HEADER_BG }}>
              <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                  <div style={{
                    width: '40px',
                    height: '40px',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    backgroundColor: `${COLORS.ORANGE}20`,
                    border: `1px solid ${COLORS.ORANGE}40`,
                  }}>
                    <Globe size={20} color={COLORS.ORANGE} />
                  </div>
                  <div>
                    <h2 style={{ color: COLORS.WHITE, fontSize: '14px', fontWeight: 600, margin: 0 }}>
                      {selectedBroker.displayName}
                    </h2>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginTop: '4px' }}>
                      {getStatusBadge(brokerStatuses.get(selectedBrokerId!)?.status || 'not_configured')}
                      <span style={{ color: COLORS.MUTED, fontSize: '10px' }}>
                        {selectedBroker.authType === 'oauth' ? 'OAuth 2.0' : selectedBroker.authType === 'totp' ? 'TOTP Auth' : 'API Key Auth'}
                      </span>
                    </div>
                  </div>
                </div>

                <div style={{ display: 'flex', gap: '8px' }}>
                  {API_KEYS_URLS[selectedBrokerId!] && (
                    <a
                      href={API_KEYS_URLS[selectedBrokerId!]}
                      target="_blank"
                      rel="noopener noreferrer"
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        padding: '8px 12px',
                        backgroundColor: `${COLORS.ORANGE}10`,
                        border: `1px solid ${COLORS.ORANGE}40`,
                        color: COLORS.ORANGE,
                        fontSize: '10px',
                        textDecoration: 'none',
                        fontWeight: 600,
                      }}
                    >
                      GET API KEYS
                      <ExternalLink size={12} />
                    </a>
                  )}

                  {selectedBrokerId === activeBroker && isAuthenticated && (
                    <button
                      onClick={handleCheckToken}
                      disabled={isCheckingToken}
                      style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        padding: '8px 12px',
                        backgroundColor: `${COLORS.BLUE}10`,
                        border: `1px solid ${COLORS.BLUE}40`,
                        color: COLORS.BLUE,
                        fontSize: '10px',
                        cursor: 'pointer',
                        fontWeight: 600,
                      }}
                    >
                      {isCheckingToken ? <Loader2 size={12} className="animate-spin" /> : <RefreshCw size={12} />}
                      CHECK TOKEN
                    </button>
                  )}
                </div>
              </div>
            </div>

            {/* Configuration Content */}
            <div style={{ flex: 1, overflow: 'auto', padding: '20px' }}>
              {/* Error Display */}
              {(error || configError) && (
                <div style={{
                  marginBottom: '16px',
                  padding: '12px',
                  backgroundColor: `${COLORS.RED}15`,
                  border: `1px solid ${COLORS.RED}40`,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                }}>
                  <AlertTriangle size={16} color={COLORS.RED} />
                  <p style={{ color: COLORS.RED, fontSize: '11px', margin: 0 }}>{error || configError}</p>
                </div>
              )}

              {/* Success Display */}
              {saveSuccess && (
                <div style={{
                  marginBottom: '16px',
                  padding: '12px',
                  backgroundColor: `${COLORS.GREEN}15`,
                  border: `1px solid ${COLORS.GREEN}40`,
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px',
                }}>
                  <CheckCircle2 size={16} color={COLORS.GREEN} />
                  <p style={{ color: COLORS.GREEN, fontSize: '11px', margin: 0 }}>Configuration saved successfully!</p>
                </div>
              )}

              {/* Connected State */}
              {selectedBrokerId === activeBroker && isAuthenticated ? (
                <div style={{ maxWidth: '500px' }}>
                  <div style={{
                    padding: '20px',
                    backgroundColor: `${COLORS.GREEN}10`,
                    border: `1px solid ${COLORS.GREEN}40`,
                    marginBottom: '20px',
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '12px', marginBottom: '12px' }}>
                      <CheckCircle2 size={24} color={COLORS.GREEN} />
                      <div>
                        <h3 style={{ color: COLORS.WHITE, fontSize: '13px', fontWeight: 600, margin: 0 }}>
                          Connected to {selectedBroker.displayName}
                        </h3>
                        <p style={{ color: COLORS.GREEN, fontSize: '10px', margin: '4px 0 0 0' }}>
                          Trading mode: {tradingMode.toUpperCase()}
                        </p>
                      </div>
                    </div>
                    <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: 0 }}>
                      Your broker account is linked and ready for trading. Indian broker tokens expire at midnight IST.
                    </p>
                  </div>

                  <button
                    onClick={handleDisconnect}
                    style={{
                      width: '100%',
                      padding: '12px 16px',
                      backgroundColor: `${COLORS.RED}20`,
                      border: `1px solid ${COLORS.RED}40`,
                      color: COLORS.RED,
                      fontSize: '11px',
                      fontWeight: 600,
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '8px',
                    }}
                  >
                    <LogOut size={14} />
                    DISCONNECT BROKER
                  </button>
                </div>
              ) : (
                /* Configuration Form */
                <div style={{ maxWidth: '500px' }}>
                  {/* Step 1: API Credentials */}
                  <div style={{ marginBottom: '24px' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                      <div style={{
                        width: '24px',
                        height: '24px',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        backgroundColor: COLORS.ORANGE,
                        color: COLORS.DARK_BG,
                        fontSize: '12px',
                        fontWeight: 700,
                      }}>
                        1
                      </div>
                      <h3 style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, margin: 0 }}>
                        CONFIGURE API CREDENTIALS
                      </h3>
                    </div>

                    {/* ============================================================ */}
                    {/* BROKER-SPECIFIC AUTHENTICATION FORMS */}
                    {/* Based on OpenAlgo authentication patterns */}
                    {/* ============================================================ */}

                    {/* ------------------------------------------------------------ */}
                    {/* OAUTH BROKERS: Zerodha, Fyers, Upstox, Dhan, IIFL, IBKR, Saxo */}
                    {/* Fields: API Key + API Secret */}
                    {/* ------------------------------------------------------------ */}
                    {['zerodha', 'fyers', 'upstox', 'dhan', 'iifl', 'ibkr', 'saxobank', 'flattrade'].includes(selectedBroker.id) && (
                      <>
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            {selectedBroker.id === 'fyers' ? 'APP ID' :
                             selectedBroker.id === 'dhan' ? 'CLIENT ID' :
                             selectedBroker.id === 'flattrade' ? 'API KEY (userid:::api_key format)' :
                             'API KEY'} <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder={
                              selectedBroker.id === 'fyers' ? 'Enter your Fyers App ID (e.g., XYZ123-100)' :
                              selectedBroker.id === 'dhan' ? 'Enter your Dhan Client ID' :
                              selectedBroker.id === 'flattrade' ? 'Enter API Key (userid:::api_key)' :
                              `Enter your ${selectedBroker.displayName} API Key`
                            }
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                        </div>
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            {selectedBroker.id === 'fyers' ? 'SECRET KEY' : 'API SECRET'} <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your API Secret"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '10px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          {selectedBroker.id === 'fyers' && (
                            <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                              Hash is generated as SHA256(api_key:api_secret) for token exchange
                            </p>
                          )}
                          {selectedBroker.id === 'flattrade' && (
                            <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                              Security hash: SHA256(api_key + request_code + api_secret)
                            </p>
                          )}
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* KOTAK NEO: UCC + Mobile + MPIN + TOTP (2-step auth) */}
                    {/* Step 1: TOTP login → view_token */}
                    {/* Step 2: MPIN validation → trading_token */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'kotak' && (
                      <>
                        {/* UCC */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            UCC (USER CLIENT CODE) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="e.g., KS12345"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your UCC (User Client Code) - Primary identifier
                          </p>
                        </div>

                        {/* Access Token / NEO-FIN-KEY */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            ACCESS TOKEN (NEO-FIN-KEY) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your Access Token"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '10px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                        </div>

                        {/* Mobile Number */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            MOBILE NUMBER <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                            <span style={{ color: COLORS.GRAY, fontSize: '12px', fontFamily: 'monospace' }}>+91</span>
                            <input
                              type="text"
                              value={mobileNumber}
                              onChange={(e) => setMobileNumber(e.target.value.replace(/\D/g, '').slice(0, 10))}
                              placeholder="9999955555"
                              maxLength={10}
                              style={{
                                flex: 1,
                                padding: '10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            10-digit mobile number (auto-prefixed with +91)
                          </p>
                        </div>

                        {/* MPIN */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            MPIN (6-DIGIT) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type={showSecret ? 'text' : 'password'}
                            value={(config as any).mpin || ''}
                            onChange={(e) => setConfig({ ...config, mpin: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                            placeholder="******"
                            maxLength={6}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              textAlign: 'center',
                              letterSpacing: '4px',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your 6-digit trading MPIN
                          </p>
                        </div>

                        {/* TOTP Code */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={(config as any).totpCode || ''}
                            onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                            placeholder="123456"
                            maxLength={6}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              textAlign: 'center',
                              letterSpacing: '4px',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            6-digit code from Google/Microsoft Authenticator
                          </p>
                        </div>

                        {/* Kotak Auth Info */}
                        <div style={{ padding: '10px', backgroundColor: `${COLORS.YELLOW}10`, border: `1px solid ${COLORS.YELLOW}40`, marginBottom: '12px' }}>
                          <p style={{ color: COLORS.YELLOW, fontSize: '9px', margin: 0 }}>
                            <strong>TOTP Registration Required:</strong> You must register TOTP in the NEO app before using this feature.
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* GROWW: API Key + API Secret (Server-side TOTP) */}
                    {/* TOTP is generated server-side from API Secret */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'groww' && (
                      <>
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your Groww API Key"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                        </div>
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API SECRET (TOTP SECRET) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your API Secret"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '10px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            TOTP is generated automatically from this secret - no manual entry required
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* US BROKERS: Alpaca, Tradier */}
                    {/* Simple API Key authentication */}
                    {/* ------------------------------------------------------------ */}
                    {['alpaca', 'tradier'].includes(selectedBroker.id) && (
                      <>
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            {selectedBroker.id === 'tradier' ? 'ACCESS TOKEN' : 'API KEY ID'} <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type={selectedBroker.id === 'tradier' ? 'password' : 'text'}
                            value={config.apiKey}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder={selectedBroker.id === 'tradier' ? 'Enter your Tradier Access Token' : 'Enter your Alpaca API Key ID'}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                        </div>
                        {selectedBroker.id === 'alpaca' && (
                          <div style={{ marginBottom: '12px' }}>
                            <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                              SECRET KEY <span style={{ color: COLORS.RED }}>*</span>
                            </label>
                            <div style={{ position: 'relative' }}>
                              <input
                                type={showSecret ? 'text' : 'password'}
                                value={config.apiSecret || ''}
                                onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                                placeholder="Enter your Alpaca Secret Key"
                                style={{
                                  width: '100%',
                                  padding: '10px 40px 10px 12px',
                                  backgroundColor: COLORS.PANEL_BG,
                                  border: `1px solid ${COLORS.BORDER}`,
                                  color: COLORS.WHITE,
                                  fontSize: '12px',
                                  fontFamily: 'monospace',
                                  outline: 'none',
                                  boxSizing: 'border-box',
                                }}
                              />
                              <button
                                type="button"
                                onClick={() => setShowSecret(!showSecret)}
                                style={{
                                  position: 'absolute',
                                  right: '10px',
                                  top: '50%',
                                  transform: 'translateY(-50%)',
                                  background: 'none',
                                  border: 'none',
                                  color: COLORS.GRAY,
                                  cursor: 'pointer',
                                }}
                              >
                                {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                              </button>
                            </div>
                          </div>
                        )}
                        <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                          {selectedBroker.id === 'tradier' ? 'Tradier uses a single bearer token for authentication' : 'Use paper trading keys for testing'}
                        </p>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* YFINANCE: No authentication required */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'yfinance' && (
                      <div style={{ padding: '16px', backgroundColor: `${COLORS.GREEN}10`, border: `1px solid ${COLORS.GREEN}40`, marginBottom: '12px' }}>
                        <p style={{ color: COLORS.GREEN, fontSize: '11px', margin: 0, fontWeight: 600 }}>
                          No API keys required
                        </p>
                        <p style={{ color: COLORS.GRAY, fontSize: '10px', margin: '8px 0 0 0' }}>
                          YFinance provides free market data for paper trading. Click "Save Configuration" to activate paper trading mode.
                        </p>
                      </div>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* SHOONYA (FINVASIA): SHA256 Hash-based Authentication */}
                    {/* Fields: Vendor Code, API Secret, User ID, Password, TOTP */}
                    {/* Hash: SHA256(password), AppKey: SHA256(userid|api_secret) */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'shoonya' && (
                      <>
                        {/* Vendor Code */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            VENDOR CODE <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey || ''}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your Vendor Code"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Vendor Code from Shoonya API settings
                          </p>
                        </div>

                        {/* API Secret Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API SECRET KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your API Secret Key"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '10px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Used to generate AppKey hash: SHA256(userid|api_secret)
                          </p>
                        </div>

                        {/* User ID */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            USER ID <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.userId || ''}
                            onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                            placeholder="Enter your Shoonya User ID"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Shoonya trading account User ID
                          </p>
                        </div>

                        {/* Password */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            PASSWORD <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={(config as any).password || ''}
                              onChange={(e) => setConfig({ ...config, password: e.target.value } as any)}
                              placeholder="Enter your password"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Shoonya trading password (will be SHA256 hashed)
                          </p>
                        </div>

                        {/* TOTP */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={(config as any).totpCode || ''}
                            onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                            placeholder="123456"
                            maxLength={6}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              textAlign: 'center',
                              letterSpacing: '4px',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            6-digit TOTP from your authenticator app
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* ANGEL ONE: TOTP-based Authentication */}
                    {/* Fields: API Key, Client ID, PIN, TOTP Secret */}
                    {/* Headers: X-PrivateKey (api_key), Content-Type */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'angelone' && (
                      <>
                        {/* API Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey || ''}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your SmartAPI Key"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Angel One SmartAPI Key from publisher dashboard
                          </p>
                        </div>

                        {/* Client ID (Trading Account) */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            CLIENT ID (TRADING ACCOUNT) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.userId || ''}
                            onChange={(e) => setConfig({ ...config, userId: e.target.value.toUpperCase() })}
                            placeholder="e.g. T55289"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Angel One trading account login ID (letters and numbers only)
                          </p>
                        </div>

                        {/* PIN */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            PIN <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={(config as any).pin || ''}
                              onChange={(e) => setConfig({ ...config, pin: e.target.value } as any)}
                              placeholder="Enter your trading PIN"
                              minLength={4}
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Angel One account PIN (minimum 4 digits)
                          </p>
                        </div>

                        {/* TOTP Secret */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            TOTP SECRET <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={(config as any).totpSecret || ''}
                              onChange={(e) => setConfig({ ...config, totpSecret: e.target.value.trim() } as any)}
                              placeholder="Enter your TOTP secret key"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Base32 secret key from Angel One TOTP setup (enables automated login)
                          </p>
                        </div>

                        {/* Angel One Auth Info */}
                        <div style={{ padding: '10px', backgroundColor: `${COLORS.CYAN}10`, border: `1px solid ${COLORS.CYAN}40`, marginBottom: '12px' }}>
                          <p style={{ color: COLORS.CYAN, fontSize: '9px', margin: 0 }}>
                            <strong>Note:</strong> 6-digit TOTP is generated automatically from the TOTP secret. Authentication happens immediately on save.
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* ALICE BLUE: Encryption Key-based Authentication */}
                    {/* Fields: User ID, API Key, API Secret */}
                    {/* Step 1: Get encKey from API using User ID */}
                    {/* Step 2: Generate checksum: SHA256(userid + api_secret + encKey) */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'aliceblue' && (
                      <>
                        {/* User ID */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            USER ID <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.userId || ''}
                            onChange={(e) => setConfig({ ...config, userId: e.target.value.toUpperCase() })}
                            placeholder="Enter your AliceBlue User ID"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your AliceBlue trading account User ID
                          </p>
                        </div>

                        {/* API Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey || ''}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your API Key"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your AliceBlue API Key
                          </p>
                        </div>

                        {/* API Secret */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API SECRET <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your API Secret"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your AliceBlue API Secret
                          </p>
                        </div>

                        {/* AliceBlue Auth Info */}
                        <div style={{ padding: '10px', backgroundColor: `${COLORS.CYAN}10`, border: `1px solid ${COLORS.CYAN}40`, marginBottom: '12px' }}>
                          <p style={{ color: COLORS.CYAN, fontSize: '9px', margin: 0 }}>
                            <strong>Auth Flow:</strong> Encryption key is fetched automatically. Checksum generated as SHA256(userid + api_secret + encKey)
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* 5PAISA: TOTP-based Authentication */}
                    {/* Fields: App Name/User Key, App Secret, Client Code/Mobile, PIN, TOTP */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'fivepaisa' && (
                      <>
                        {/* App Name / User Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            APP NAME / USER KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey || ''}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your App Name or User Key"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your 5Paisa App Name or User Key
                          </p>
                        </div>

                        {/* App Secret / Encryption Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            APP SECRET / ENCRYPTION KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={config.apiSecret || ''}
                              onChange={(e) => setConfig({ ...config, apiSecret: e.target.value })}
                              placeholder="Enter your App Secret"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '10px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                        </div>

                        {/* Client Code / Mobile Number */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            CLIENT CODE / MOBILE NO <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.userId || ''}
                            onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                            placeholder="Enter Client Code or Mobile Number"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your 5Paisa Client Code or registered Mobile Number
                          </p>
                        </div>

                        {/* PIN */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            PIN <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={(config as any).pin || ''}
                              onChange={(e) => setConfig({ ...config, pin: e.target.value } as any)}
                              placeholder="Enter your PIN"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your 5Paisa trading PIN
                          </p>
                        </div>

                        {/* TOTP */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            TOTP CODE <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={(config as any).totpCode || ''}
                            onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                            placeholder="123456"
                            maxLength={6}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              textAlign: 'center',
                              letterSpacing: '4px',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            6-digit TOTP from your authenticator app
                          </p>
                        </div>
                      </>
                    )}

                    {/* ------------------------------------------------------------ */}
                    {/* MOTILAL OSWAL: Multi-factor Authentication (DOB + optional TOTP) */}
                    {/* Fields: API Key, User ID, Password, DOB (2FA), TOTP (optional) */}
                    {/* Password hash: SHA256(password + api_key) */}
                    {/* ------------------------------------------------------------ */}
                    {selectedBroker.id === 'motilal' && (
                      <>
                        {/* API Key */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            API KEY <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.apiKey || ''}
                            onChange={(e) => setConfig({ ...config, apiKey: e.target.value })}
                            placeholder="Enter your Motilal Oswal API Key"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Motilal Oswal API Key
                          </p>
                        </div>

                        {/* User ID */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            USER ID (CLIENT ID) <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={config.userId || ''}
                            onChange={(e) => setConfig({ ...config, userId: e.target.value })}
                            placeholder="Enter your User ID"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Your Motilal Oswal Client ID
                          </p>
                        </div>

                        {/* Password */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            PASSWORD <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <div style={{ position: 'relative' }}>
                            <input
                              type={showSecret ? 'text' : 'password'}
                              value={(config as any).password || ''}
                              onChange={(e) => setConfig({ ...config, password: e.target.value } as any)}
                              placeholder="Enter your password"
                              style={{
                                width: '100%',
                                padding: '10px 40px 10px 12px',
                                backgroundColor: COLORS.PANEL_BG,
                                border: `1px solid ${COLORS.BORDER}`,
                                color: COLORS.WHITE,
                                fontSize: '12px',
                                fontFamily: 'monospace',
                                outline: 'none',
                                boxSizing: 'border-box',
                              }}
                            />
                            <button
                              type="button"
                              onClick={() => setShowSecret(!showSecret)}
                              style={{
                                position: 'absolute',
                                right: '12px',
                                top: '50%',
                                transform: 'translateY(-50%)',
                                background: 'none',
                                border: 'none',
                                color: COLORS.GRAY,
                                cursor: 'pointer',
                              }}
                            >
                              {showSecret ? <EyeOff size={16} /> : <Eye size={16} />}
                            </button>
                          </div>
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Password hash is generated as SHA256(password + api_key)
                          </p>
                        </div>

                        {/* 2FA - Date of Birth */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            2FA - DATE OF BIRTH <span style={{ color: COLORS.RED }}>*</span>
                          </label>
                          <input
                            type="text"
                            value={(config as any).dob || ''}
                            onChange={(e) => setConfig({ ...config, dob: e.target.value } as any)}
                            placeholder="DD/MM/YYYY (e.g., 18/10/1988)"
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '12px',
                              fontFamily: 'monospace',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Format: DD/MM/YYYY (required as 2FA factor)
                          </p>
                        </div>

                        {/* TOTP (Optional) */}
                        <div style={{ marginBottom: '12px' }}>
                          <label style={{ display: 'block', color: COLORS.GRAY, fontSize: '10px', fontWeight: 600, marginBottom: '6px' }}>
                            TOTP CODE (OPTIONAL)
                          </label>
                          <input
                            type="text"
                            value={(config as any).totpCode || ''}
                            onChange={(e) => setConfig({ ...config, totpCode: e.target.value.replace(/\D/g, '').slice(0, 6) } as any)}
                            placeholder="Leave blank if using OTP via SMS/Email"
                            maxLength={6}
                            style={{
                              width: '100%',
                              padding: '10px 12px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                              color: COLORS.WHITE,
                              fontSize: '16px',
                              fontFamily: 'monospace',
                              textAlign: 'center',
                              letterSpacing: '4px',
                              outline: 'none',
                              boxSizing: 'border-box',
                            }}
                          />
                          <p style={{ color: COLORS.MUTED, fontSize: '9px', marginTop: '4px', margin: '4px 0 0 0' }}>
                            Optional: 6-digit TOTP from authenticator app (leave blank for SMS/Email OTP)
                          </p>
                        </div>
                      </>
                    )}

                    <button
                      onClick={handleSaveConfig}
                      disabled={isSaving || !config.apiKey}
                      style={{
                        width: '100%',
                        padding: '10px 16px',
                        backgroundColor: isSaving || !config.apiKey ? COLORS.MUTED : COLORS.ORANGE,
                        border: 'none',
                        color: isSaving || !config.apiKey ? COLORS.GRAY : COLORS.DARK_BG,
                        fontSize: '11px',
                        fontWeight: 700,
                        cursor: isSaving || !config.apiKey ? 'not-allowed' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '8px',
                      }}
                    >
                      {isSaving ? <Loader2 size={14} className="animate-spin" /> : <Save size={14} />}
                      {['shoonya', 'angelone', 'aliceblue', 'fivepaisa', 'motilal'].includes(selectedBroker.id) ? 'SAVE & AUTHENTICATE' : 'SAVE CONFIGURATION'}
                    </button>
                  </div>

                  {/* Step 2: Authentication */}
                  {selectedBroker.authType === 'oauth' && (
                    <div>
                      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                        <div style={{
                          width: '24px',
                          height: '24px',
                          display: 'flex',
                          alignItems: 'center',
                          justifyContent: 'center',
                          backgroundColor: brokerStatuses.get(selectedBrokerId!)?.hasCredentials ? COLORS.ORANGE : COLORS.MUTED,
                          color: COLORS.DARK_BG,
                          fontSize: '12px',
                          fontWeight: 700,
                        }}>
                          2
                        </div>
                        <h3 style={{ color: COLORS.WHITE, fontSize: '12px', fontWeight: 600, margin: 0 }}>
                          AUTHENTICATE WITH {selectedBroker.displayName.toUpperCase()}
                        </h3>
                      </div>

                      {!brokerStatuses.get(selectedBrokerId!)?.hasCredentials ? (
                        <p style={{ color: COLORS.MUTED, fontSize: '10px', marginBottom: '12px' }}>
                          Save your API credentials first to enable authentication.
                        </p>
                      ) : (
                        <>
                          <button
                            onClick={handleOAuthLogin}
                            disabled={isConnecting}
                            style={{
                              width: '100%',
                              padding: '12px 16px',
                              backgroundColor: isConnecting ? COLORS.MUTED : COLORS.ORANGE,
                              border: 'none',
                              color: isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
                              fontSize: '11px',
                              fontWeight: 700,
                              cursor: isConnecting ? 'not-allowed' : 'pointer',
                              display: 'flex',
                              alignItems: 'center',
                              justifyContent: 'center',
                              gap: '8px',
                              marginBottom: '12px',
                            }}
                          >
                            {isConnecting ? <Loader2 size={14} className="animate-spin" /> : <ExternalLink size={14} />}
                            LOGIN WITH {selectedBroker.displayName.toUpperCase()}
                          </button>

                          {/* Callback URL Input */}
                          {showCallbackInput && (
                            <div style={{
                              padding: '16px',
                              backgroundColor: COLORS.PANEL_BG,
                              border: `1px solid ${COLORS.BORDER}`,
                            }}>
                              <p style={{ color: COLORS.WHITE, fontSize: '11px', fontWeight: 600, marginBottom: '8px' }}>
                                PASTE CALLBACK URL
                              </p>
                              <p style={{ color: COLORS.GRAY, fontSize: '9px', marginBottom: '12px' }}>
                                After authorizing, copy the URL from your browser and paste it below.
                              </p>
                              <input
                                type="text"
                                value={callbackUrl}
                                onChange={(e) => setCallbackUrl(e.target.value)}
                                placeholder="https://127.0.0.1/?auth_code=..."
                                style={{
                                  width: '100%',
                                  padding: '10px 12px',
                                  backgroundColor: COLORS.HEADER_BG,
                                  border: `1px solid ${COLORS.BORDER}`,
                                  color: COLORS.WHITE,
                                  fontSize: '11px',
                                  fontFamily: 'monospace',
                                  outline: 'none',
                                  boxSizing: 'border-box',
                                  marginBottom: '12px',
                                }}
                              />
                              <button
                                onClick={handleCallbackSubmit}
                                disabled={!callbackUrl || isConnecting}
                                style={{
                                  width: '100%',
                                  padding: '10px 16px',
                                  backgroundColor: !callbackUrl || isConnecting ? COLORS.MUTED : COLORS.GREEN,
                                  border: 'none',
                                  color: !callbackUrl || isConnecting ? COLORS.GRAY : COLORS.DARK_BG,
                                  fontSize: '11px',
                                  fontWeight: 700,
                                  cursor: !callbackUrl || isConnecting ? 'not-allowed' : 'pointer',
                                }}
                              >
                                COMPLETE AUTHENTICATION
                              </button>
                            </div>
                          )}
                        </>
                      )}
                    </div>
                  )}

                  {/* Security Note */}
                  <div style={{
                    marginTop: '24px',
                    padding: '12px',
                    backgroundColor: COLORS.PANEL_BG,
                    border: `1px solid ${COLORS.BORDER}`,
                  }}>
                    <p style={{ color: COLORS.MUTED, fontSize: '9px', margin: 0, lineHeight: 1.6 }}>
                      <strong style={{ color: COLORS.GRAY }}>SECURITY:</strong> Your credentials are stored securely on your local device.
                      They are never sent to our servers and only communicate directly with {selectedBroker.displayName}'s API.
                      Indian broker tokens expire at midnight IST daily.
                    </p>
                  </div>
                </div>
              )}
            </div>
          </>
        ) : (
          /* No broker selected */
          <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
            <div style={{ textAlign: 'center' }}>
              <Settings size={48} color={COLORS.MUTED} style={{ marginBottom: '16px' }} />
              <p style={{ color: COLORS.GRAY, fontSize: '12px' }}>Select a broker from the list to configure</p>
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
