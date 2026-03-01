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
import { open } from '@tauri-apps/plugin-shell';
import {
  useStockBrokerContext,
  useStockBrokerAuth,
} from '@/contexts/StockBrokerContext';
import { createStockBrokerAdapter } from '@/brokers/stocks';
import { withTimeout } from '@/services/core/apiUtils';
import type { BrokerCredentials, Region } from '@/brokers/stocks/types';
import { ConnectionStatus, BrokerStatus } from './brokers-management/types';
import { BrokerListPanel } from './brokers-management/BrokerListPanel';
import { BrokerConfigPanel } from './brokers-management/BrokerConfigPanel';

export function BrokersManagementPanel() {
  const { availableBrokers, activeBroker, setActiveBroker, adapter, tradingMode } = useStockBrokerContext();
  const { isAuthenticated, authenticate, logout, getAuthUrl, isConnecting, error, clearError } = useStockBrokerAuth();

  // UI State
  const [selectedBrokerId, setSelectedBrokerId] = useState<string | null>(activeBroker);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedRegion, setSelectedRegion] = useState<Region | 'all'>('all');

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
    const grouped = new Map<Region, typeof filteredBrokers>();
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

  return (
    <div style={{ display: 'flex', height: '100%', backgroundColor: '#000000', fontFamily: '"IBM Plex Mono", monospace' }}>
      <BrokerListPanel
        availableBrokers={availableBrokers}
        activeBroker={activeBroker}
        isAuthenticated={isAuthenticated}
        selectedBrokerId={selectedBrokerId}
        searchQuery={searchQuery}
        selectedRegion={selectedRegion}
        availableRegions={availableRegions}
        brokersByRegion={brokersByRegion}
        brokerStatuses={brokerStatuses}
        onSelectBroker={handleSelectBroker}
        onSearchChange={setSearchQuery}
        onRegionChange={setSelectedRegion}
      />
      <BrokerConfigPanel
        selectedBroker={selectedBroker}
        selectedBrokerId={selectedBrokerId}
        activeBroker={activeBroker}
        isAuthenticated={isAuthenticated}
        tradingMode={tradingMode}
        error={error}
        configError={configError}
        saveSuccess={saveSuccess}
        isSaving={isSaving}
        isConnecting={isConnecting}
        isCheckingToken={isCheckingToken}
        showSecret={showSecret}
        setShowSecret={setShowSecret}
        config={config}
        setConfig={setConfig}
        mobileNumber={mobileNumber}
        setMobileNumber={setMobileNumber}
        callbackUrl={callbackUrl}
        setCallbackUrl={setCallbackUrl}
        showCallbackInput={showCallbackInput}
        brokerStatuses={brokerStatuses}
        onSaveConfig={handleSaveConfig}
        onOAuthLogin={handleOAuthLogin}
        onCallbackSubmit={handleCallbackSubmit}
        onDisconnect={handleDisconnect}
        onCheckToken={handleCheckToken}
      />
    </div>
  );
}
