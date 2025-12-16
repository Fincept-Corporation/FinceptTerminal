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
import { sqliteService, type BacktestingProvider } from '@/services/sqliteService';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { LeanAdapter } from '@/services/backtesting/adapters/lean';
import { PathService } from '@/services/backtesting/PathService';

// Bloomberg Professional Color Palette
const BLOOMBERG = {
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
      backgroundColor: BLOOMBERG.PANEL_BG,
      border: `1px solid ${BLOOMBERG.BORDER}`,
      borderLeft: `3px solid ${BLOOMBERG.YELLOW}`,
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
          <Settings size={14} color={BLOOMBERG.YELLOW} />
          <span style={{
            color: BLOOMBERG.WHITE,
            fontSize: '11px',
            fontWeight: 600,
            letterSpacing: '0.5px'
          }}>
            DATA STORAGE LOCATION
          </span>
        </div>
        <span style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
          {showPaths ? '▼' : '▶'} {showPaths ? 'Hide' : 'Show'} Paths
        </span>
      </div>

      {showPaths && (
        <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${BLOOMBERG.BORDER}` }}>
          <div style={{ fontSize: '9px', color: BLOOMBERG.GRAY, marginBottom: '8px' }}>
            Platform: <span style={{ color: BLOOMBERG.CYAN }}>{pathsInfo.platform}</span>
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
            backgroundColor: BLOOMBERG.HEADER_BG,
            borderRadius: '2px',
            fontSize: '9px',
            color: BLOOMBERG.GRAY,
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
      <span style={{ color: BLOOMBERG.GRAY }}>{label}:</span>
      <code style={{
        color: BLOOMBERG.CYAN,
        backgroundColor: BLOOMBERG.HEADER_BG,
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
    name: 'QuantConnect Lean',
    adapter_type: 'LeanAdapter',
    config: JSON.stringify(
      {
        leanCliPath: 'lean',
        projectsPath: '',
        dataPath: '',
        resultsPath: '',
        environment: 'backtesting',
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
      const defaultConfig = await PathService.getDefaultLeanConfig();

      setNewProviderForm({
        name: 'QuantConnect Lean',
        adapter_type: 'LeanAdapter',
        config: JSON.stringify(defaultConfig, null, 2),
      });

      console.log('[Backtesting] Initialized OS-specific paths:', defaultConfig);
    } catch (error) {
      console.error('[Backtesting] Failed to initialize default paths:', error);
    }
  };

  /**
   * Migrate existing providers to use OS-specific paths
   * This updates any providers that are using old project-directory paths
   */
  const migrateExistingProviderPaths = async () => {
    try {
      const dbProviders = await sqliteService.getBacktestingProviders();
      const defaultConfig = await PathService.getDefaultLeanConfig();

      for (const provider of dbProviders) {
        try {
          const config = JSON.parse(provider.config);

          // Check if using old paths (containing project directory)
          const needsMigration =
            config.projectsPath?.includes('fincept-terminal-desktop') ||
            config.dataPath?.includes('fincept-terminal-desktop') ||
            config.resultsPath?.includes('fincept-terminal-desktop');

          if (needsMigration) {
            console.log(`[Backtesting] Migrating provider "${provider.name}" to new paths`);

            // Update paths while preserving other config
            const updatedConfig = {
              ...config,
              projectsPath: defaultConfig.projectsPath,
              dataPath: defaultConfig.dataPath,
              resultsPath: defaultConfig.resultsPath,
            };

            // Save updated provider
            await sqliteService.saveBacktestingProvider({
              ...provider,
              config: JSON.stringify(updatedConfig, null, 2),
            });

            console.log(`[Backtesting] Migrated provider "${provider.name}" successfully`);
          }
        } catch (error) {
          console.error(`[Backtesting] Failed to migrate provider "${provider.name}":`, error);
        }
      }

      // Reload providers to show updated configs
      await loadProviders();
    } catch (error) {
      console.error('[Backtesting] Failed to migrate provider paths:', error);
    }
  };

  /**
   * Register default providers with the registry
   */
  const registerDefaultProviders = () => {
    try {
      // Check if already registered to avoid duplicates
      if (!backtestingRegistry.getProvider('QuantConnect Lean')) {
        // Register Lean adapter
        const leanAdapter = new LeanAdapter();
        backtestingRegistry.registerProvider(leanAdapter);

        console.log('[Backtesting] Registered providers:', backtestingRegistry.listProviders());
      }
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
    if (!confirm('Are you sure you want to delete this provider?')) return;

    try {
      await sqliteService.deleteBacktestingProvider(providerName);
      await loadProviders();
      showMessage('success', 'Provider deleted');
    } catch (error) {
      showMessage('error', `Failed to delete provider: ${error}`);
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
        color: BLOOMBERG.GRAY
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
        borderBottom: `2px solid ${BLOOMBERG.ORANGE}`
      }}>
        <Activity size={20} color={BLOOMBERG.ORANGE} style={{ filter: `drop-shadow(0 0 4px ${BLOOMBERG.ORANGE})` }} />
        <h2 style={{
          color: BLOOMBERG.ORANGE,
          fontSize: '16px',
          fontWeight: 700,
          letterSpacing: '1px',
          margin: 0,
          textShadow: `0 0 10px ${BLOOMBERG.ORANGE}40`
        }}>
          BACKTESTING PROVIDERS
        </h2>
      </div>

      {/* Description */}
      <div style={{
        marginBottom: '20px',
        padding: '12px',
        backgroundColor: BLOOMBERG.HEADER_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderLeft: `3px solid ${BLOOMBERG.CYAN}`
      }}>
        <p style={{
          color: BLOOMBERG.GRAY,
          fontSize: '10px',
          margin: 0,
          lineHeight: '1.6'
        }}>
          Manage backtesting engines. Switch between Lean, Backtrader, and other providers seamlessly.
        </p>
      </div>

      {/* Path Info Panel */}
      <PathInfoPanel />

      {/* Message Banner */}
      {message && (
        <div style={{
          padding: '12px 16px',
          marginBottom: '20px',
          backgroundColor: message.type === 'success' ? `${BLOOMBERG.GREEN}15` : `${BLOOMBERG.RED}15`,
          border: `1px solid ${message.type === 'success' ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
          borderLeft: `3px solid ${message.type === 'success' ? BLOOMBERG.GREEN : BLOOMBERG.RED}`,
          borderRadius: '3px',
          display: 'flex',
          alignItems: 'center',
          gap: '10px',
        }}>
          {message.type === 'success' ? <CheckCircle size={16} color={BLOOMBERG.GREEN} /> : <AlertCircle size={16} color={BLOOMBERG.RED} />}
          <span style={{
            color: message.type === 'success' ? BLOOMBERG.GREEN : BLOOMBERG.RED,
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
          backgroundColor: BLOOMBERG.GREEN,
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
          boxShadow: `0 0 12px ${BLOOMBERG.GREEN}40`,
          transition: 'all 0.2s'
        }}
        onMouseEnter={(e) => {
          e.currentTarget.style.boxShadow = `0 0 20px ${BLOOMBERG.GREEN}60`;
          e.currentTarget.style.transform = 'translateY(-1px)';
        }}
        onMouseLeave={(e) => {
          e.currentTarget.style.boxShadow = `0 0 12px ${BLOOMBERG.GREEN}40`;
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
          backgroundColor: BLOOMBERG.PANEL_BG,
          border: `1px solid ${BLOOMBERG.BORDER}`,
          borderLeft: `3px solid ${BLOOMBERG.ORANGE}`,
          borderRadius: '4px',
        }}>
          <h3 style={{
            color: BLOOMBERG.ORANGE,
            marginBottom: '16px',
            fontSize: '13px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>ADD NEW PROVIDER</h3>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <div>
              <label style={{
                color: BLOOMBERG.WHITE,
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
                  if (selectedProvider === 'QuantConnect Lean') {
                    defaultConfig = {
                      leanCliPath: 'C:/Users/Tilak/AppData/Roaming/Python/Python313/Scripts/lean.exe',
                      projectsPath: 'C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/lean_projects',
                      dataPath: 'C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/lean_data',
                      resultsPath: 'C:/windowsdisk/finceptTerminal/fincept-terminal-desktop/lean_results',
                      environment: 'backtesting',
                    };
                  } else if (selectedProvider === 'Backtesting.py') {
                    defaultConfig = {
                      // Backtesting.py doesn't require configuration
                      // Advanced options are set per-backtest
                    };
                  } else if (selectedProvider === 'VectorBT') {
                    defaultConfig = {
                      // VectorBT configuration
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
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
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
                <option value="QuantConnect Lean">QuantConnect Lean</option>
                <option value="Backtrader">Backtrader</option>
                <option value="VectorBT">VectorBT</option>
                <option value="Backtesting.py">Backtesting.py</option>
                <option value="QuantLib">QuantLib</option>
                <option value="Custom">Custom</option>
              </select>
            </div>

            <div>
              <label style={{
                color: BLOOMBERG.WHITE,
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
                  backgroundColor: BLOOMBERG.DARK_BG,
                  color: BLOOMBERG.WHITE,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
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
                  backgroundColor: BLOOMBERG.GREEN,
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
                  color: BLOOMBERG.GRAY,
                  border: `1px solid ${BLOOMBERG.BORDER}`,
                  borderRadius: '4px',
                  cursor: 'pointer',
                  fontWeight: 700,
                  fontSize: '10px',
                  letterSpacing: '0.5px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  e.currentTarget.style.borderColor = BLOOMBERG.RED;
                  e.currentTarget.style.color = BLOOMBERG.RED;
                }}
                onMouseLeave={(e) => {
                  e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                  e.currentTarget.style.color = BLOOMBERG.GRAY;
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
            backgroundColor: BLOOMBERG.PANEL_BG,
            border: `1px dashed ${BLOOMBERG.BORDER}`,
            borderRadius: '4px',
          }}>
            <Activity size={48} style={{ margin: '0 auto 16px', opacity: 0.3, color: BLOOMBERG.ORANGE }} />
            <p style={{ color: BLOOMBERG.WHITE, fontSize: '13px', fontWeight: 600, marginBottom: '8px' }}>
              NO PROVIDERS CONFIGURED
            </p>
            <p style={{ color: BLOOMBERG.GRAY, fontSize: '10px' }}>
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
                  backgroundColor: BLOOMBERG.PANEL_BG,
                  border: `1px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                  borderLeft: `3px solid ${isActive ? BLOOMBERG.ORANGE : BLOOMBERG.BORDER}`,
                  borderRadius: '4px',
                  transition: 'all 0.2s',
                  position: 'relative',
                  overflow: 'hidden'
                }}
                onMouseEnter={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = BLOOMBERG.HOVER;
                    e.currentTarget.style.borderColor = BLOOMBERG.MUTED;
                  }
                }}
                onMouseLeave={(e) => {
                  if (!isActive) {
                    e.currentTarget.style.backgroundColor = BLOOMBERG.PANEL_BG;
                    e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
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
                    background: `linear-gradient(90deg, transparent, ${BLOOMBERG.ORANGE}, transparent)`,
                    opacity: 0.5
                  }} />
                )}

                {/* Header */}
                <div style={{ display: 'flex', alignItems: 'flex-start', justifyContent: 'space-between', marginBottom: '12px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flex: 1 }}>
                    <Activity size={20} style={{ color: isActive ? BLOOMBERG.ORANGE : BLOOMBERG.GRAY }} />
                    <div>
                      <h3 style={{
                        color: isActive ? BLOOMBERG.ORANGE : BLOOMBERG.WHITE,
                        margin: 0,
                        fontSize: '12px',
                        fontWeight: 700,
                        letterSpacing: '0.5px'
                      }}>{provider.name}</h3>
                      <p style={{
                        color: BLOOMBERG.GRAY,
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
                        backgroundColor: `${BLOOMBERG.GREEN}20`,
                        borderRadius: '3px'
                      }}>
                        <div style={{
                          width: '6px',
                          height: '6px',
                          borderRadius: '50%',
                          backgroundColor: BLOOMBERG.GREEN,
                          boxShadow: `0 0 6px ${BLOOMBERG.GREEN}`
                        }} />
                        <span style={{
                          color: BLOOMBERG.GREEN,
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
                        backgroundColor: `${BLOOMBERG.CYAN}20`,
                        borderRadius: '3px'
                      }}>
                        <CheckCircle size={10} color={BLOOMBERG.CYAN} />
                        <span style={{
                          color: BLOOMBERG.CYAN,
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
                          backgroundColor: `${BLOOMBERG.ORANGE}15`,
                          border: `1px solid ${BLOOMBERG.ORANGE}40`,
                          borderRadius: '3px',
                          color: BLOOMBERG.ORANGE,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>BACKTESTING</span>
                      )}
                      {capabilities.optimization && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${BLOOMBERG.CYAN}15`,
                          border: `1px solid ${BLOOMBERG.CYAN}40`,
                          borderRadius: '3px',
                          color: BLOOMBERG.CYAN,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>OPTIMIZATION</span>
                      )}
                      {capabilities.liveTrading && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${BLOOMBERG.GREEN}15`,
                          border: `1px solid ${BLOOMBERG.GREEN}40`,
                          borderRadius: '3px',
                          color: BLOOMBERG.GREEN,
                          fontSize: '8px',
                          fontWeight: 700,
                          letterSpacing: '0.5px'
                        }}>LIVE</span>
                      )}
                      {capabilities.research && (
                        <span style={{
                          padding: '3px 8px',
                          backgroundColor: `${BLOOMBERG.YELLOW}15`,
                          border: `1px solid ${BLOOMBERG.YELLOW}40`,
                          borderRadius: '3px',
                          color: BLOOMBERG.YELLOW,
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
                    backgroundColor: status.connected ? `${BLOOMBERG.GREEN}10` : `${BLOOMBERG.RED}10`,
                    border: `1px solid ${status.connected ? BLOOMBERG.GREEN : BLOOMBERG.RED}40`,
                    borderRadius: '3px',
                    fontSize: '9px',
                    color: status.connected ? BLOOMBERG.GREEN : BLOOMBERG.RED,
                  }}>
                    {status.message}
                  </div>
                )}

                {status.error && (
                  <div style={{
                    padding: '8px 10px',
                    marginBottom: '12px',
                    backgroundColor: `${BLOOMBERG.RED}10`,
                    border: `1px solid ${BLOOMBERG.RED}40`,
                    borderRadius: '3px',
                    fontSize: '9px',
                    color: BLOOMBERG.RED,
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
                      backgroundColor: status.testing ? BLOOMBERG.MUTED : BLOOMBERG.ORANGE,
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
                        backgroundColor: BLOOMBERG.GREEN,
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
                      color: BLOOMBERG.GRAY,
                      border: `1px solid ${BLOOMBERG.BORDER}`,
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
                      e.currentTarget.style.borderColor = BLOOMBERG.RED;
                      e.currentTarget.style.color = BLOOMBERG.RED;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = BLOOMBERG.BORDER;
                      e.currentTarget.style.color = BLOOMBERG.GRAY;
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
        backgroundColor: BLOOMBERG.HEADER_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        borderTop: `2px solid ${BLOOMBERG.CYAN}`,
        borderRadius: '4px',
        display: 'grid',
        gridTemplateColumns: 'repeat(2, 1fr)',
        gap: '16px'
      }}>
        <div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px',
            marginBottom: '6px'
          }}>REGISTERED PROVIDERS</div>
          <div style={{
            color: BLOOMBERG.CYAN,
            fontSize: '20px',
            fontWeight: 700
          }}>{backtestingRegistry.listProviders().length}</div>
        </div>
        <div>
          <div style={{
            color: BLOOMBERG.GRAY,
            fontSize: '9px',
            fontWeight: 600,
            letterSpacing: '0.5px',
            marginBottom: '6px'
          }}>ACTIVE PROVIDER</div>
          <div style={{
            color: BLOOMBERG.GREEN,
            fontSize: '12px',
            fontWeight: 700,
            letterSpacing: '0.5px'
          }}>{backtestingRegistry.getActiveProvider()?.name || 'NONE'}</div>
        </div>
      </div>
    </div>
  );
}
