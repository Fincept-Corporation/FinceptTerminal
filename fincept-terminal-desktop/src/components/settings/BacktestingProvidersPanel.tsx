/**
 * Backtesting Providers Panel
 *
 * Manages backtesting provider configurations in Settings tab.
 * Allows users to register, configure, activate, and test providers.
 */

import React, { useState, useEffect } from 'react';
import {
  Activity,
  Check,
  X,
  Settings,
  PlayCircle,
  RefreshCw,
  AlertCircle,
  CheckCircle,
  Loader,
  Edit,
  Trash2,
  Plus,
  Zap,
} from 'lucide-react';
import { sqliteService, type BacktestingProvider } from '@/services/core/sqliteService';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { PathService } from '@/services/backtesting/PathService';
import { showConfirm, showSuccess, showError } from '@/utils/notifications';

// Fincept Professional Color Palette
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

interface ProviderPanelProps {
  colors: {
    background: string;
    surface: string;
    text: string;
    textSecondary: string;
    border: string;
    accent: string;
    success: string;
    error: string;
  };
}

interface ProviderStatus {
  connected: boolean;
  testing: boolean;
  error?: string;
  message?: string;
}

/**
 * Path Info Panel - Shows where data will be stored
 */
function PathInfoPanel() {
  const [pathsInfo, setPathsInfo] = useState<{
    platform: string;
    baseDir: string;
    projectsDir: string;
    dataDir: string;
    resultsDir: string;
  } | null>(null);
  const [showPaths, setShowPaths] = useState(false);

  useEffect(() => {
    loadPathsInfo();
  }, []);

  const loadPathsInfo = async () => {
    try {
      const info = await PathService.getPathsInfo();
      setPathsInfo(info);
    } catch (error) {
      console.error('[PathInfo] Failed to load paths:', error);
    }
  };

  if (!pathsInfo) return null;

  return (
    <div style={{
      marginBottom: '20px',
      padding: '12px',
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      borderLeft: `3px solid ${FINCEPT.YELLOW}`,
      borderRadius: '3px'
    }}>
      <div
        style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'space-between',
          cursor: 'pointer'
        }}
        onClick={() => setShowPaths(!showPaths)}
      >
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <Settings size={14} color={FINCEPT.YELLOW} />
          <span style={{
            color: FINCEPT.WHITE,
            fontSize: '11px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            DATA STORAGE LOCATION
          </span>
        </div>
        <span style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
          {showPaths ? '▼' : '▶'} {showPaths ? 'Hide' : 'Show'} Paths
        </span>
      </div>

      {showPaths && (
        <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${FINCEPT.BORDER}` }}>
          <div style={{ fontSize: '9px', color: FINCEPT.GRAY, marginBottom: '8px' }}>
            Platform: <span style={{ color: FINCEPT.CYAN }}>{pathsInfo.platform}</span>
          </div>
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            <PathItem label="Base Directory" path={pathsInfo.baseDir} />
            <PathItem label="Projects" path={pathsInfo.projectsDir} />
            <PathItem label="Data" path={pathsInfo.dataDir} />
            <PathItem label="Results" path={pathsInfo.resultsDir} />
          </div>
          <div style={{
            marginTop: '10px',
            padding: '8px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderRadius: '2px',
            fontSize: '9px',
            color: FINCEPT.GRAY,
            lineHeight: '1.5'
          }}>
            ℹ️ Backtesting data is stored in your application data folder, separate from the project directory.
            This ensures clean project management and follows OS best practices.
          </div>
        </div>
      )}
    </div>
  );
}

function PathItem({ label, path }: { label: string; path: string }) {
  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      gap: '2px',
      fontSize: '9px'
    }}>
      <span style={{ color: FINCEPT.GRAY }}>{label}:</span>
      <code style={{
        color: FINCEPT.CYAN,
        backgroundColor: FINCEPT.HEADER_BG,
        padding: '4px 6px',
        borderRadius: '2px',
        fontFamily: 'monospace',
        wordBreak: 'break-all'
      }}>
        {path}
      </code>
    </div>
  );
}

export function BacktestingProvidersPanel({ colors }: ProviderPanelProps) {
  const [providers, setProviders] = useState<BacktestingProvider[]>([]);
  const [loading, setLoading] = useState(true);
  const [statuses, setStatuses] = useState<Record<string, ProviderStatus>>({});
  const [editingProvider, setEditingProvider] = useState<string | null>(null);
  const [showAddForm, setShowAddForm] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  // New provider form - will be initialized with OS-specific paths
  const [newProviderForm, setNewProviderForm] = useState({
    name: 'Backtrader',
    adapter_type: 'BacktraderAdapter',
    config: JSON.stringify(
      {
        // Provider-specific configuration
      },
      null,
      2
    ),
  });

  useEffect(() => {
    initializeDefaultPaths();
    loadProviders();
    registerDefaultProviders();
    migrateExistingProviderPaths();
  }, []);

  /**
   * Initialize default paths using OS-specific directories
   */
  const initializeDefaultPaths = async () => {
    try {
      // No default configuration needed currently
      setNewProviderForm({
        name: 'Backtrader',
        adapter_type: 'BacktraderAdapter',
        config: JSON.stringify({}, null, 2),
      });

      console.log('[Backtesting] Initialized default provider form');
    } catch (error) {
      console.error('[Backtesting] Failed to initialize default paths:', error);
    }
  };

  /**
   * Migrate existing providers if needed
   */
  const migrateExistingProviderPaths = async () => {
    try {
      // No migration needed currently
      // This function can be used in the future for provider migrations
      console.log('[Backtesting] No migration needed');
    } catch (error) {
      console.error('[Backtesting] Failed during migration check:', error);
    }
  };

  /**
   * Register default providers with the registry
   */
  const registerDefaultProviders = () => {
    try {
      // No default providers to register currently
      // Add custom backtesting providers here when available
      console.log('[Backtesting] Registered providers:', backtestingRegistry.listProviders());
    } catch (error) {
      console.error('[Backtesting] Failed to register providers:', error);
    }
  };

  /**
   * Load providers from database
   */
  const loadProviders = async () => {
    try {
      setLoading(true);
      const dbProviders = await sqliteService.getBacktestingProviders();
      setProviders(dbProviders);

      // Initialize statuses
      const initialStatuses: Record<string, ProviderStatus> = {};
      for (const provider of dbProviders) {
        initialStatuses[provider.name] = {
          connected: false,
          testing: false,
        };
      }
      setStatuses(initialStatuses);
    } catch (error) {
      console.error('[Backtesting] Failed to load providers:', error);
      showMessage('error', 'Failed to load providers');
    } finally {
      setLoading(false);
    }
  };

  /**
   * Test connection to provider
   */
  const testConnection = async (provider: BacktestingProvider) => {
    setStatuses((prev) => ({
      ...prev,
      [provider.name]: { ...prev[provider.name], testing: true, error: undefined },
    }));

    try {
      // Get provider from registry
      const registeredProvider = backtestingRegistry.getProvider(provider.name);
      if (!registeredProvider) {
        throw new Error(`Provider "${provider.name}" not registered`);
      }

      // Initialize with config
      const config = JSON.parse(provider.config);
      const initResult = await registeredProvider.initialize({
        name: provider.name,
        adapterType: provider.adapter_type,
        settings: config
      });

      if (!initResult.success) {
        throw new Error(initResult.error || 'Initialization failed');
      }

      // Test connection
      const testResult = await registeredProvider.testConnection();

      setStatuses((prev) => ({
        ...prev,
        [provider.name]: {
          connected: testResult.success,
          testing: false,
          error: testResult.error,
          message: testResult.message,
        },
      }));

      if (testResult.success) {
        showMessage('success', `${provider.name}: ${testResult.message}`);
      } else {
        showMessage('error', `${provider.name}: ${testResult.error}`);
      }
    } catch (error) {
      setStatuses((prev) => ({
        ...prev,
        [provider.name]: {
          connected: false,
          testing: false,
          error: String(error),
        },
      }));
      showMessage('error', `Connection test failed: ${error}`);
    }
  };

  /**
   * Activate provider
   */
  const activateProvider = async (providerName: string) => {
    try {
      // Get provider from registry
      const registeredProvider = backtestingRegistry.getProvider(providerName);
      if (!registeredProvider) {
        throw new Error(`Provider "${providerName}" not registered`);
      }

      // Get provider config from database
      const dbProvider = providers.find(p => p.name === providerName);
      if (!dbProvider) {
        throw new Error(`Provider "${providerName}" not found in database`);
      }

      // Initialize with config
      const config = JSON.parse(dbProvider.config);
      const initResult = await registeredProvider.initialize({
        name: dbProvider.name,
        adapterType: dbProvider.adapter_type,
        settings: config
      });

      if (!initResult.success) {
        throw new Error(initResult.error || 'Initialization failed');
      }

      // Set active in database
      await sqliteService.setActiveBacktestingProvider(providerName);

      // Set active in registry
      await backtestingRegistry.setActiveProvider(providerName);

      // Reload providers to update UI
      await loadProviders();

      showMessage('success', `Activated provider: ${providerName}`);
    } catch (error) {
      showMessage('error', `Failed to activate provider: ${error}`);
    }
  };

  /**
   * Save new provider
   */
  const saveNewProvider = async () => {
    try {
      // Validate config JSON
      JSON.parse(newProviderForm.config);

      const newProvider: Omit<BacktestingProvider, 'created_at' | 'updated_at'> = {
        id: crypto.randomUUID(),
        name: newProviderForm.name,
        adapter_type: newProviderForm.adapter_type,
        config: newProviderForm.config,
        enabled: true,
        is_active: false,
      };

      await sqliteService.saveBacktestingProvider(newProvider);
      await loadProviders();
      setShowAddForm(false);
      showMessage('success', 'Provider added successfully');

      // Reset form with OS-specific paths
      await initializeDefaultPaths();
    } catch (error) {
      showMessage('error', `Failed to save provider: ${error}`);
    }
  };

  /**
   * Delete provider
   */
  const deleteProvider = async (providerName: string) => {
    const confirmed = await showConfirm(
      `Delete backtesting provider "${providerName}"?\n\nThis action cannot be undone.`,
      { title: 'Delete Provider', type: 'danger', confirmText: 'Delete' }
    );

    if (!confirmed) return;

    try {
      await sqliteService.deleteBacktestingProvider(providerName);
      await loadProviders();
      showSuccess('Provider deleted successfully', [
        { label: 'Provider', value: providerName },
      ]);
    } catch (error) {
      showError(`Failed to delete provider: ${error}`);
    }
  };

  /**
   * Show message
   */
  const showMessage = (type: 'success' | 'error', text: string) => {
    setMessage({ type, text });
    setTimeout(() => setMessage(null), 5000);
  };

  /**
   * Get provider capabilities
   */
  const getProviderCapabilities = (providerName: string) => {
    const provider = backtestingRegistry.getProvider(providerName);
    return provider?.capabilities;
  };

  if (loading) {
    return (
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        padding: '60px',
        color: FINCEPT.GRAY
      }}>
        <Loader size={24} className="animate-spin" style={{ marginRight: '12px' }} />
        <span style={{ fontSize: '12px', fontWeight: 600, letterSpacing: '0.5px' }}>LOADING PROVIDERS...</span>
      </div>
    );
  }

  return (
    <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: '12px',
        marginBottom: '24px',
        paddingBottom: '12px',
        borderBottom: `2px solid ${FINCEPT.ORANGE}`
      }}>
        <Activity size={20} color={FINCEPT.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${FINCEPT.ORANGE})` }} />
        <h2 style={{
          color: FINCEPT.ORANGE,
          fontSize: '16px',
          fontWeight: 700,
          letterSpacing: '1px',
          margin: 0,
          textShadow: `0 0 10px ${FINCEPT.ORANGE}40`
        }}>
          BACKTESTING PROVIDERS
        </h2>
      </div>

      {/* Description */}
      <div style={{
        marginBottom: '20px',
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderLeft: `3px solid ${FINCEPT.CYAN}`
      }}>
        <p style={{
          color: FINCEPT.GRAY,
          fontSize: '10px',
          margin: 0,
          lineHeight: '1.6'
        }}>
          Manage backtesting engines. Switch between Backtrader, VectorBT, and other providers seamlessly.
        </p>
      </div>

      {/* Path Info Panel */}
      <PathInfoPanel />

      {/* Message Banner */}
      {message && (
        <div style={{
          padding: '12px 16px',
          marginBottom: '20px',
          backgroundColor: message.type === 'success' ? `${FINCEPT.GREEN}15` : `${FINCEPT.RED}15`,
          border: `1px solid ${message.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED}`,
          borderLeft: `3px solid ${message.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED}`,
          borderRadius: '3px',
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
        }}>
          {message.type === 'success' ? <CheckCircle size={16} color={FINCEPT.GREEN} /> : <AlertCircle size={16} color={FINCEPT.RED} />}
          <span style={{
            color: message.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '10px',
            fontWeight: 600
          }}>{message.text}</span>
        </div>
      )}

      {/* Add Provider Button */}
      <button
        onClick={() => setShowAddForm(!showAddForm)}
        style={{
          padding: '10px 20px',
          marginBottom: '20px',
          backgroundColor: FINCEPT.GREEN,
          color: '#000000',
          border: 'none',
          borderRadius: '4px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          fontWeight: 700,
          fontSize: '11px',
          letterSpacing: '0.5px',
          boxShadow: `0 0 12px ${FINCEPT.GREEN}40`,
          transition: 'all 0.2s'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.boxShadow = `0 0 20px ${FINCEPT.GREEN}60`;
          e.currentTarget.style.transform = 'translateY(-1px)';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.boxShadow = `0 0 12px ${FINCEPT.GREEN}40`;
          e.currentTarget.style.transform = 'translateY(0)';
        }}
      >
        <Plus size={16} />
        ADD PROVIDER
      </button>

      {/* Add Provider Form */}
      {showAddForm && (
        <div style={{
          marginBottom: '20px',
          padding: '20px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderLeft: `3px solid ${FINCEPT.ORANGE}`,
          borderRadius: '4px',
        }}>
          <h3 style={{
            color: FINCEPT.ORANGE,
            marginBottom: '16px',
            fontSize: '13px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>ADD NEW PROVIDER</h3>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <div>
              <label style={{
                color: FINCEPT.WHITE,
                display: 'block',
                marginBottom: '6px',
                fontSize: '10px',
                fontWeight: 600,
                letterSpacing: '0.5px'
              }}>
                PROVIDER TYPE
              </label>
              <select
                value={newProviderForm.name}
                onChange={(e) => {
                  const selectedProvider = e.target.value;
                  let defaultConfig = {};

                  // Set default config based on provider type
                  if (selectedProvider === 'Backtesting.py') {
                    defaultConfig = {
                      // Backtesting.py doesn't require configuration
                      // Advanced options are set per-backtest
                    };
                  } else if (selectedProvider === 'VectorBT') {
                    defaultConfig = {
                      // VectorBT configuration
                    };
                  } else if (selectedProvider === 'Backtrader') {
                    defaultConfig = {
                      // Backtrader configuration
                    };
                  }

                  setNewProviderForm({
                    ...newProviderForm,
                    name: selectedProvider,
                    config: JSON.stringify(defaultConfig, null, 2)
                  });
                }}
                style={{
                  width: '100%',
                  padding: '10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '3px',
                  fontSize: '11px',
                  fontWeight: 500,
                  cursor: 'pointer',
                  appearance: 'none',
                  backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FF8800' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                  backgroundRepeat: 'no-repeat',
                  backgroundPosition: 'right 10px center',
                  paddingRight: '32px'
                }}
              >
                <option value="Backtrader">Backtrader</option>
                <option value="VectorBT">VectorBT</option>
                <option value="Backtesting.py">Backtesting.py</option>
                <option value="QuantLib">QuantLib</option>
                <option value="Custom">Custom</option>
              </select>
            </div>

            <div>
              <label style={{
                color: FINCEPT.WHITE,
                display: 'block',
                marginBottom: '6px',
                fontSize: '10px',
                fontWeight: 600,
                letterSpacing: '0.5px'
              }}>
                CONFIGURATION (JSON)
              </label>
              <textarea
                value={newProviderForm.config}
                onChange={(e) => setNewProviderForm({ ...newProviderForm, config: e.target.value })}
                rows={10}
                style={{
                  width: '100%',
                  padding: '10px',
                  backgroundColor: FINCEPT.DARK_BG,
                  color: FINCEPT.WHITE,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '3px',
                  fontFamily: 'monospace',
                  fontSize: '10px',
                  lineHeight: '1.5',
                  resize: 'vertical'
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '12px' }}>
              <button
                onClick={saveNewProvider}
                style={{
                  padding: '10px 20px',
                  backgroundColor: FINCEPT.GREEN,
                  color: '#000000',
                  border: 'none',
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontWeight: 700,
                  fontSize: '10px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
              >
                SAVE
              </button>
              <button
                onClick={() => setShowAddForm(false)}
                style={{
                  padding: '10px 20px',
                  backgroundColor: 'transparent',
                  color: FINCEPT.GRAY,
                  border: `1px solid ${FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontWeight: 700,
                  fontSize: '10px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.RED;
                  e.currentTarget.style.color = FINCEPT.RED;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }}
              >
                CANCEL
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Providers List */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(400px, 1fr))', gap: '16px', marginBottom: '20px' }}>
        {providers.length === 0 ? (
          <div style={{
            gridColumn: '1 / -1',
            padding: '60px 40px',
            textAlign: 'center',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px dashed ${FINCEPT.BORDER}`,
            borderRadius: '4px',
          }}>
            <Activity size={48} style={{ margin: '0 auto 16px', opacity: 0.3, color: FINCEPT.ORANGE }} />
            <p style={{ color: FINCEPT.WHITE, fontSize: '13px', fontWeight: 600, marginBottom: '8px' }}>
              NO PROVIDERS CONFIGURED
            </p>
            <p style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
              Click "ADD PROVIDER" to configure your first backtesting engine
            </p>
          </div>
        ) : (
          providers.map((provider) => {
            const status = statuses[provider.name] || { connected: false, testing: false };
            const capabilities = getProviderCapabilities(provider.name);
            const isActive = provider.is_active;

            return (
              <div
                key={provider.id}
                style={{
                  padding: '16px',
                  backgroundColor: FINCEPT.PANEL_BG,
                  border: `1px solid ${isActive ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderLeft: `3px solid ${isActive ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                  borderRadius: '4px',
                  transition: 'all 0.2s',
                  position: 'relative',
                  overflow: 'hidden'
                }}
                onMouseEnter={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                    e.currentTarget.style.borderColor = FINCEPT.MUTED;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = FINCEPT.PANEL_BG;
                    e.currentTarget.style.borderColor = FINCEPT.BORDER;
                  }
                }}
              >
                {/* Active Glow */}
                {isActive && (
                  <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    height: '1px',
                    background: `linear-gradient(90deg, transparent, ${FINCEPT.ORANGE}, transparent)`,
                    opacity: 0.5
                  }} />
                )}

                {/* Header */}
                <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '12px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flex: 1 }}>
                    <Activity size={20} style={{ color: isActive ? FINCEPT.ORANGE : FINCEPT.GRAY }} />
                    <div>
                      <h3 style={{
                        color: isActive ? FINCEPT.ORANGE : FINCEPT.WHITE,
                        margin: 0,
                        fontSize: '12px',
                        fontWeight: 700,
                        letterSpacing: '0.5px'
                      }}>{provider.name}</h3>
                      <p style={{
                        color: FINCEPT.GRAY,
                        fontSize: '9px',
                        margin: 0,
                        fontWeight: 500
                      }}>
                        {provider.adapter_type}
                      </p>
                    </div>
                  </div>

                  {/* Status Badges */}
                  <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'flex-end', gap: '4px' }}>
                    {isActive && (
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        padding: '3px 8px',
                        backgroundColor: `${FINCEPT.GREEN}20`,
                        borderRadius: '3px'
                      }}>
                        <div style={{
                          width: '6px',
                          height: '6px',
                          borderRadius: '50%',
                          backgroundColor: FINCEPT.GREEN,
                          boxShadow: `0 0 6px ${FINCEPT.GREEN}`
                        }} />
                        <span style={{
                          color: FINCEPT.GREEN,
                          fontSize: '9px',
                          fontWeight: 700
                        }}>ACTIVE</span>
                      </div>
                    )}

                    {status.connected && (
                      <div style={{
                        display: 'flex',
                        alignItems: 'center',
                        gap: '4px',
                        padding: '3px 8px',
                        backgroundColor: `${FINCEPT.CYAN}20`,
                        borderRadius: '3px'
                      }}>
                        <CheckCircle size={10} color={FINCEPT.CYAN} />
                        <span style={{
                          color: FINCEPT.CYAN,
                          fontSize: '9px',
                          fontWeight: 700
                        }}>CONNECTED</span>
                      </div>
                    )}
                  </div>
                </div>

                {/* Capabilities */}
                {capabilities && (
                  <div style={{ marginBottom: '12px' }}>
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '6px' }}>
                      {capabilities.backtesting && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${FINCEPT.ORANGE}15`,
                          border: `1px solid ${FINCEPT.ORANGE}40`,
                          borderRadius: '3px',
                          color: FINCEPT.ORANGE,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>BACKTESTING</span>
                      )}
                      {capabilities.optimization && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${FINCEPT.CYAN}15`,
                          border: `1px solid ${FINCEPT.CYAN}40`,
                          borderRadius: '3px',
                          color: FINCEPT.CYAN,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>OPTIMIZATION</span>
                      )}
                      {capabilities.liveTrading && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${FINCEPT.GREEN}15`,
                          border: `1px solid ${FINCEPT.GREEN}40`,
                          borderRadius: '3px',
                          color: FINCEPT.GREEN,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>LIVE</span>
                      )}
                      {capabilities.research && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${FINCEPT.YELLOW}15`,
                          border: `1px solid ${FINCEPT.YELLOW}40`,
                          borderRadius: '3px',
                          color: FINCEPT.YELLOW,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>RESEARCH</span>
                      )}
                    </div>
                  </div>
                )}

                {/* Status/Error Messages */}
                {status.message && (
                  <div style={{
                    padding: '8px 10px',
                    marginBottom: '12px',
                    backgroundColor: status.connected ? `${FINCEPT.GREEN}10` : `${FINCEPT.RED}10`,
                    border: `1px solid ${status.connected ? FINCEPT.GREEN : FINCEPT.RED}40`,
                    borderRadius: '3px',
                    fontSize: '9px',
                    color: status.connected ? FINCEPT.GREEN : FINCEPT.RED,
                  }}>
                    {status.message}
                  </div>
                )}

                {status.error && (
                  <div style={{
                    padding: '8px 10px',
                    marginBottom: '12px',
                    backgroundColor: `${FINCEPT.RED}10`,
                    border: `1px solid ${FINCEPT.RED}40`,
                    borderRadius: '3px',
                    fontSize: '9px',
                    color: FINCEPT.RED,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                  }}>
                    <AlertCircle size={12} />
                    {status.error}
                  </div>
                )}

                {/* Actions */}
                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                  <button
                    onClick={() => testConnection(provider)}
                    disabled={status.testing}
                    style={{
                      flex: 1,
                      padding: '8px 12px',
                      backgroundColor: status.testing ? FINCEPT.MUTED : FINCEPT.ORANGE,
                      color: '#000000',
                      border: 'none',
                      borderRadius: '3px',
                      cursor: status.testing ? 'not-allowed' : 'pointer',
                      opacity: status.testing ? 0.6 : 1,
                      display: 'flex',
                      alignItems: 'center',
                      justifyContent: 'center',
                      gap: '6px',
                      fontSize: '9px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      if (!status.testing) e.currentTarget.style.opacity = '0.8';
                    }}
                    onMouseLeave={(e) => {
                      if (!status.testing) e.currentTarget.style.opacity = '1';
                    }}
                  >
                    {status.testing ? (
                      <>
                        <Loader size={12} className="animate-spin" />
                        TESTING...
                      </>
                    ) : (
                      <>
                        <PlayCircle size={12} />
                        TEST
                      </>
                    )}
                  </button>

                  {!isActive && (
                    <button
                      onClick={() => activateProvider(provider.name)}
                      style={{
                        padding: '8px 12px',
                        backgroundColor: FINCEPT.GREEN,
                        color: '#000000',
                        border: 'none',
                        borderRadius: '3px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                        fontSize: '9px',
                        fontWeight: 700,
                        letterSpacing: '0.5px',
                        transition: 'all 0.2s'
                      }}
                      onMouseEnter={(e) => e.currentTarget.style.opacity = '0.8'}
                      onMouseLeave={(e) => e.currentTarget.style.opacity = '1'}
                    >
                      <Zap size={12} />
                      ACTIVATE
                    </button>
                  )}

                  <button
                    onClick={() => deleteProvider(provider.name)}
                    style={{
                      padding: '8px 12px',
                      backgroundColor: 'transparent',
                      color: FINCEPT.GRAY,
                      border: `1px solid ${FINCEPT.BORDER}`,
                      borderRadius: '3px',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      fontSize: '9px',
                      fontWeight: 700,
                      letterSpacing: '0.5px',
                      transition: 'all 0.2s'
                    }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.RED;
                      e.currentTarget.style.color = FINCEPT.RED;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = FINCEPT.BORDER;
                      e.currentTarget.style.color = FINCEPT.GRAY;
                    }}
                  >
                    <Trash2 size={12} />
                    DELETE
                  </button>
                </div>
              </div>
            );
          })
        )}
      </div>

      {/* Registry Status */}
      <div style={{
        padding: '16px',
        backgroundColor: FINCEPT.HEADER_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderTop: `2px solid ${FINCEPT.CYAN}`,
        borderRadius: '4px',
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '16px'
      }}>
        <div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px',
            marginBottom: '6px'
          }}>REGISTERED PROVIDERS</div>
          <div style={{
            color: FINCEPT.CYAN,
            fontSize: '20px',
            fontWeight: 700
          }}>{backtestingRegistry.listProviders().length}</div>
        </div>
        <div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px',
            marginBottom: '6px'
          }}>ACTIVE PROVIDER</div>
          <div style={{
            color: FINCEPT.GREEN,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>{backtestingRegistry.getActiveProvider()?.name || 'NONE'}</div>
        </div>
      </div>
    </div>
  );
}
