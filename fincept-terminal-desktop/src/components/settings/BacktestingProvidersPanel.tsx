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
} from 'lucide-react';
import { sqliteService, type BacktestingProvider } from '@/services/sqliteService';
import { backtestingRegistry } from '@/services/backtesting/BacktestingProviderRegistry';
import { LeanAdapter } from '@/services/backtesting/adapters/lean';

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

export function BacktestingProvidersPanel({ colors }: ProviderPanelProps) {
  const [providers, setProviders] = useState<BacktestingProvider[]>([]);
  const [loading, setLoading] = useState(true);
  const [statuses, setStatuses] = useState<Record<string, ProviderStatus>>({});
  const [editingProvider, setEditingProvider] = useState<string | null>(null);
  const [showAddForm, setShowAddForm] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  // New provider form
  const [newProviderForm, setNewProviderForm] = useState({
    name: 'lean',
    adapter_type: 'LeanAdapter',
    config: JSON.stringify(
      {
        leanCliPath: 'lean',
        projectsPath: './lean_projects',
        dataPath: './lean_data',
        resultsPath: './lean_results',
        environment: 'backtesting',
      },
      null,
      2
    ),
  });

  useEffect(() => {
    loadProviders();
    registerDefaultProviders();
  }, []);

  /**
   * Register default providers with the registry
   */
  const registerDefaultProviders = () => {
    try {
      // Register Lean adapter
      const leanAdapter = new LeanAdapter();
      backtestingRegistry.registerProvider(leanAdapter);

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
      const initResult = await registeredProvider.initialize(config);

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

      // Reset form
      setNewProviderForm({
        name: 'lean',
        adapter_type: 'LeanAdapter',
        config: JSON.stringify(
          {
            leanCliPath: 'lean',
            projectsPath: './lean_projects',
            dataPath: './lean_data',
            resultsPath: './lean_results',
            environment: 'backtesting',
          },
          null,
          2
        ),
      });
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
      <div style={{ padding: '20px', color: colors.text }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '10px' }}>
          <Loader size={20} className="animate-spin" />
          <span>Loading providers...</span>
        </div>
      </div>
    );
  }

  return (
    <div style={{ padding: '20px' }}>
      {/* Header */}
      <div style={{ marginBottom: '20px' }}>
        <h2 style={{ color: colors.text, fontSize: '1.5rem', marginBottom: '8px' }}>
          Backtesting Providers
        </h2>
        <p style={{ color: colors.textSecondary, fontSize: '0.9rem' }}>
          Manage backtesting engines. Switch between Lean, Backtrader, and other providers seamlessly.
        </p>
      </div>

      {/* Message Banner */}
      {message && (
        <div
          style={{
            padding: '12px',
            marginBottom: '20px',
            borderRadius: '6px',
            backgroundColor: message.type === 'success' ? colors.success + '20' : colors.error + '20',
            border: `1px solid ${message.type === 'success' ? colors.success : colors.error}`,
            color: message.type === 'success' ? colors.success : colors.error,
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
          }}
        >
          {message.type === 'success' ? <CheckCircle size={18} /> : <AlertCircle size={18} />}
          <span>{message.text}</span>
        </div>
      )}

      {/* Add Provider Button */}
      <button
        onClick={() => setShowAddForm(!showAddForm)}
        style={{
          padding: '10px 16px',
          marginBottom: '20px',
          backgroundColor: colors.accent,
          color: colors.background,
          border: 'none',
          borderRadius: '6px',
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          fontWeight: '500',
        }}
      >
        <Plus size={18} />
        Add Provider
      </button>

      {/* Add Provider Form */}
      {showAddForm && (
        <div
          style={{
            marginBottom: '20px',
            padding: '20px',
            backgroundColor: colors.surface,
            border: `1px solid ${colors.border}`,
            borderRadius: '8px',
          }}
        >
          <h3 style={{ color: colors.text, marginBottom: '16px' }}>Add New Provider</h3>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
            <div>
              <label style={{ color: colors.text, display: 'block', marginBottom: '6px' }}>
                Provider Type
              </label>
              <select
                value={newProviderForm.name}
                onChange={(e) => setNewProviderForm({ ...newProviderForm, name: e.target.value })}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: colors.background,
                  color: colors.text,
                  border: `1px solid ${colors.border}`,
                  borderRadius: '4px',
                }}
              >
                <option value="lean">QuantConnect Lean</option>
                <option value="backtrader">Backtrader</option>
                <option value="vectorbt">VectorBT</option>
                <option value="quantlib">QuantLib</option>
                <option value="custom">Custom</option>
              </select>
            </div>

            <div>
              <label style={{ color: colors.text, display: 'block', marginBottom: '6px' }}>
                Configuration (JSON)
              </label>
              <textarea
                value={newProviderForm.config}
                onChange={(e) => setNewProviderForm({ ...newProviderForm, config: e.target.value })}
                rows={10}
                style={{
                  width: '100%',
                  padding: '8px',
                  backgroundColor: colors.background,
                  color: colors.text,
                  border: `1px solid ${colors.border}`,
                  borderRadius: '4px',
                  fontFamily: 'monospace',
                  fontSize: '0.9rem',
                }}
              />
            </div>

            <div style={{ display: 'flex', gap: '10px' }}>
              <button
                onClick={saveNewProvider}
                style={{
                  padding: '8px 16px',
                  backgroundColor: colors.success,
                  color: 'white',
                  border: 'none',
                  borderRadius: '4px',
                  cursor: 'pointer',
                }}
              >
                Save
              </button>
              <button
                onClick={() => setShowAddForm(false)}
                style={{
                  padding: '8px 16px',
                  backgroundColor: colors.surface,
                  color: colors.text,
                  border: `1px solid ${colors.border}`,
                  borderRadius: '4px',
                  cursor: 'pointer',
                }}
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Providers List */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
        {providers.length === 0 ? (
          <div
            style={{
              padding: '40px',
              textAlign: 'center',
              color: colors.textSecondary,
              backgroundColor: colors.surface,
              border: `1px dashed ${colors.border}`,
              borderRadius: '8px',
            }}
          >
            <Activity size={48} style={{ margin: '0 auto 16px', opacity: 0.5 }} />
            <p>No providers configured</p>
            <p style={{ fontSize: '0.9rem', marginTop: '8px' }}>
              Click "Add Provider" to configure your first backtesting engine
            </p>
          </div>
        ) : (
          providers.map((provider) => {
            const status = statuses[provider.name] || { connected: false, testing: false };
            const capabilities = getProviderCapabilities(provider.name);

            return (
              <div
                key={provider.id}
                style={{
                  padding: '20px',
                  backgroundColor: colors.surface,
                  border: `2px solid ${provider.is_active ? colors.accent : colors.border}`,
                  borderRadius: '8px',
                }}
              >
                {/* Header */}
                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '12px' }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
                    <Activity size={24} style={{ color: colors.accent }} />
                    <div>
                      <h3 style={{ color: colors.text, margin: 0 }}>{provider.name}</h3>
                      <p style={{ color: colors.textSecondary, fontSize: '0.85rem', margin: 0 }}>
                        {provider.adapter_type}
                      </p>
                    </div>
                  </div>

                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    {/* Active Badge */}
                    {provider.is_active && (
                      <span
                        style={{
                          padding: '4px 12px',
                          backgroundColor: colors.success + '20',
                          color: colors.success,
                          borderRadius: '12px',
                          fontSize: '0.85rem',
                          fontWeight: '500',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}
                      >
                        <Check size={14} />
                        Active
                      </span>
                    )}

                    {/* Connection Status */}
                    {status.connected && (
                      <span
                        style={{
                          padding: '4px 12px',
                          backgroundColor: colors.success + '20',
                          color: colors.success,
                          borderRadius: '12px',
                          fontSize: '0.85rem',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px',
                        }}
                      >
                        <CheckCircle size={14} />
                        Connected
                      </span>
                    )}
                  </div>
                </div>

                {/* Capabilities */}
                {capabilities && (
                  <div style={{ marginBottom: '12px' }}>
                    <div style={{ display: 'flex', flexWrap: 'wrap', gap: '8px' }}>
                      {capabilities.backtesting && (
                        <span style={{ ...capabilityBadgeStyle, color: colors.text }}>Backtesting</span>
                      )}
                      {capabilities.optimization && (
                        <span style={{ ...capabilityBadgeStyle, color: colors.text }}>Optimization</span>
                      )}
                      {capabilities.liveTrading && (
                        <span style={{ ...capabilityBadgeStyle, color: colors.text }}>Live Trading</span>
                      )}
                      {capabilities.research && (
                        <span style={{ ...capabilityBadgeStyle, color: colors.text }}>Research</span>
                      )}
                    </div>
                  </div>
                )}

                {/* Status Message */}
                {status.message && (
                  <div
                    style={{
                      padding: '8px 12px',
                      marginBottom: '12px',
                      backgroundColor: status.connected ? colors.success + '10' : colors.error + '10',
                      borderRadius: '4px',
                      fontSize: '0.9rem',
                      color: status.connected ? colors.success : colors.error,
                    }}
                  >
                    {status.message}
                  </div>
                )}

                {/* Error Message */}
                {status.error && (
                  <div
                    style={{
                      padding: '8px 12px',
                      marginBottom: '12px',
                      backgroundColor: colors.error + '10',
                      borderRadius: '4px',
                      fontSize: '0.9rem',
                      color: colors.error,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '8px',
                    }}
                  >
                    <AlertCircle size={16} />
                    {status.error}
                  </div>
                )}

                {/* Actions */}
                <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                  <button
                    onClick={() => testConnection(provider)}
                    disabled={status.testing}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: colors.accent,
                      color: colors.background,
                      border: 'none',
                      borderRadius: '4px',
                      cursor: status.testing ? 'not-allowed' : 'pointer',
                      opacity: status.testing ? 0.6 : 1,
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                    }}
                  >
                    {status.testing ? (
                      <>
                        <Loader size={16} className="animate-spin" />
                        Testing...
                      </>
                    ) : (
                      <>
                        <PlayCircle size={16} />
                        Test Connection
                      </>
                    )}
                  </button>

                  {!provider.is_active && (
                    <button
                      onClick={() => activateProvider(provider.name)}
                      style={{
                        padding: '8px 16px',
                        backgroundColor: colors.success,
                        color: 'white',
                        border: 'none',
                        borderRadius: '4px',
                        cursor: 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px',
                      }}
                    >
                      <Check size={16} />
                      Activate
                    </button>
                  )}

                  <button
                    onClick={() => deleteProvider(provider.name)}
                    style={{
                      padding: '8px 16px',
                      backgroundColor: colors.error,
                      color: 'white',
                      border: 'none',
                      borderRadius: '4px',
                      cursor: 'pointer',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                    }}
                  >
                    <Trash2 size={16} />
                    Delete
                  </button>
                </div>
              </div>
            );
          })
        )}
      </div>

      {/* Registry Info */}
      <div
        style={{
          marginTop: '20px',
          padding: '16px',
          backgroundColor: colors.surface,
          border: `1px solid ${colors.border}`,
          borderRadius: '8px',
        }}
      >
        <h4 style={{ color: colors.text, marginBottom: '12px' }}>Provider Registry Status</h4>
        <div style={{ color: colors.textSecondary, fontSize: '0.9rem' }}>
          <p>
            Registered providers: {backtestingRegistry.listProviders().length}
          </p>
          <p>
            Active provider: {backtestingRegistry.getActiveProvider()?.name || 'None'}
          </p>
        </div>
      </div>
    </div>
  );
}

const capabilityBadgeStyle: React.CSSProperties = {
  padding: '4px 8px',
  backgroundColor: 'rgba(255, 255, 255, 0.1)',
  borderRadius: '4px',
  fontSize: '0.8rem',
};
