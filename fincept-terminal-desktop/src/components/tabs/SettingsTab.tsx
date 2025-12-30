import React, { useState, useEffect } from 'react';
import { Save, RefreshCw, Lock, User, Settings as SettingsIcon, Database, Terminal, Bell, Bot, Edit3, Type, Palette, Wifi, WifiOff, Activity, Zap, Link, Globe, Check, Plus, Trash2, ToggleLeft, ToggleRight, Eye, EyeOff } from 'lucide-react';
import { sqliteService, type LLMConfig, type LLMGlobalSettings, type LLMModelConfig, PREDEFINED_API_KEYS, type ApiKeys } from '@/services/sqliteService';
import { ollamaService } from '@/services/ollamaService';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { terminalThemeService, FONT_FAMILIES, COLOR_THEMES, FontSettings } from '@/services/terminalThemeService';
import { getWebSocketManager, ConnectionStatus, getAvailableProviders } from '@/services/websocketBridge';
import { DataSourcesPanel } from '@/components/settings/DataSourcesPanel';
import { BacktestingProvidersPanel } from '@/components/settings/BacktestingProvidersPanel';
import { LanguageSelector } from '@/components/settings/LanguageSelector';
import { TerminalConfigPanel } from '@/components/settings/TerminalConfigPanel';
import { useTimezone, TIMEZONE_OPTIONS } from '@/contexts/TimezoneContext';
import { Clock } from 'lucide-react';
import { useTranslation } from 'react-i18next';
import { TabFooter } from '@/components/common/TabFooter';
import { useAuth } from '@/contexts/AuthContext';

export default function SettingsTab() {
  const { t } = useTranslation('settings');
  const { session } = useAuth();
  const { theme, updateTheme, resetTheme, colors, fontSize: themeFontSize, fontFamily: themeFontFamily, fontWeight: themeFontWeight, fontStyle } = useTerminalTheme();
  const { defaultTimezone, setDefaultTimezone, options: timezoneOptions } = useTimezone();
  const [activeSection, setActiveSection] = useState<'credentials' | 'terminal' | 'terminalConfig' | 'llm' | 'dataConnections' | 'backtesting' | 'language'>('credentials');
  const [apiKeys, setApiKeys] = useState<ApiKeys>({});
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [dbInitialized, setDbInitialized] = useState(false);

  // WebSocket Data Connections State
  // WebSocket manager removed - now using Rust backend
  const [wsProviders, setWsProviders] = useState<Array<{
    id: number;
    provider_name: string;
    enabled: boolean;
    api_key: string | null;
    api_secret: string | null;
    endpoint: string | null;
    config_data: string | null;
  }>>([]);
  const [selectedProvider, setSelectedProvider] = useState<string | null>(null);
  const [providerForm, setProviderForm] = useState({
    provider_name: '',
    enabled: true,
    api_key: '',
    api_secret: '',
    endpoint: '',
    config_data: ''
  });
  const [expandedProvider, setExpandedProvider] = useState<string | null>(null);
  const [programmaticProviders, setProgrammaticProviders] = useState<Set<string>>(new Set());

  // Terminal appearance states
  const [fontFamily, setFontFamily] = useState(theme.font.family);
  const [baseSize, setBaseSize] = useState(theme.font.baseSize);
  const [fontWeight, setFontWeight] = useState(theme.font.weight);
  const [fontItalic, setFontItalic] = useState(theme.font.italic);
  const [selectedTheme, setSelectedTheme] = useState(terminalThemeService.getCurrentThemeKey());

  // Key mapping configuration
  const DEFAULT_KEY_MAPPINGS = {
    F1: 'dashboard',
    F2: 'markets',
    F3: 'news',
    F4: 'portfolio',
    F5: 'analytics',
    F6: 'watchlist',
    F7: 'equity-research',
    F8: 'screener',
    F9: 'trading',
    F10: 'chat',
    F11: 'fullscreen',
    F12: 'profile'
  };

  const [keyMappings, setKeyMappings] = useState<Record<string, string>>(DEFAULT_KEY_MAPPINGS);

  // LLM Configuration States
  const [llmConfigs, setLlmConfigs] = useState<LLMConfig[]>([]);
  const [llmGlobalSettings, setLlmGlobalSettings] = useState<LLMGlobalSettings>({
    temperature: 0.7,
    max_tokens: 2048,
    system_prompt: ''
  });
  const [activeProvider, setActiveProvider] = useState<string>('ollama');

  // Ollama model selection
  const [ollamaModels, setOllamaModels] = useState<string[]>([]);
  const [ollamaLoading, setOllamaLoading] = useState(false);
  const [ollamaError, setOllamaError] = useState<string | null>(null);
  const [useManualEntry, setUseManualEntry] = useState(false);
  const [showPasswords, setShowPasswords] = useState<Record<number, boolean>>({});

  // Multi-model configuration state
  const [modelConfigs, setModelConfigs] = useState<LLMModelConfig[]>([]);
  const [showAddModel, setShowAddModel] = useState(false);
  const [newModelConfig, setNewModelConfig] = useState<{
    provider: string;
    model_id: string;
    display_name: string;
    api_key: string;
    base_url: string;
  }>({
    provider: 'openai',
    model_id: '',
    display_name: '',
    api_key: '',
    base_url: ''
  });
  const [showApiKeys, setShowApiKeys] = useState<Record<string, boolean>>({});



  useEffect(() => {
    checkAndLoadData();
  }, []);

  // Track programmatic providers that get initialized
  // TODO: Re-implement with new Rust WebSocket backend
  // useEffect(() => {
  //   const detectedProviders = new Set<string>([
  //     ...Array.from(statuses.keys()),
  //     ...manager.getSubscriptions().map(sub => sub.topic.split('.')[0])
  //   ]);

  //   // Filter out database-configured providers
  //   detectedProviders.forEach(p => {
  //     if (wsProviders.find(wp => wp.provider_name === p)) {
  //       detectedProviders.delete(p);
  //     }
  //   });

  //   if (detectedProviders.size > 0) {
  //     setProgrammaticProviders(prev => new Set([...prev, ...detectedProviders]));
  //   }
  // }, [wsProviders]);

  const checkAndLoadData = async () => {
    try {
      setLoading(true);

      // Check if database is already initialized
      const isReady = sqliteService.isReady();

      if (!isReady) {
        // Database not initialized, initialize it now
        console.log('[SettingsTab] Database not initialized, initializing now...');
        await sqliteService.initialize();
      }

      // Verify database health
      const healthCheck = await sqliteService.healthCheck();
      if (!healthCheck.healthy) {
        throw new Error(healthCheck.message);
      }

      setDbInitialized(true);

      // Load initial data
      await loadApiKeys();
      await loadLLMConfigs();
      await loadWSProviders();

      console.log('[SettingsTab] Data loaded successfully');
    } catch (error) {
      console.error('[SettingsTab] Failed to load data:', error);
      const errorMsg = error instanceof Error ? error.message : 'Unknown error';
      showMessage('error', `Failed to load settings: ${errorMsg}`);
      setDbInitialized(false);
    } finally {
      setLoading(false);
    }
  };

  const loadWSProviders = async () => {
    try {
      const providers = await sqliteService.getWSProviderConfigs();
      setWsProviders(providers);

      // TODO: Re-implement auto-initialization with new Rust WebSocket backend
      // const wsManager = getWebSocketManager();
      // for (const provider of providers) {
      //   if (provider.enabled) {
      //     wsManager.setProviderConfig(provider.provider_name, {
      //       provider_name: provider.provider_name,
      //       enabled: provider.enabled,
      //       api_key: provider.api_key || undefined,
      //       api_secret: provider.api_secret || undefined,
      //       endpoint: provider.endpoint || undefined
      //     });
      //     console.log(`[Settings] Auto-initialized provider: ${provider.provider_name}`);
      //   }
      // }
    } catch (error) {
      console.error('Failed to load WS providers:', error);
    }
  };

  const handleSaveWSProvider = async () => {
    if (!providerForm.provider_name.trim()) {
      showMessage('error', 'Provider name is required');
      return;
    }

    try {
      setLoading(true);
      const result = await sqliteService.saveWSProviderConfig(providerForm);

      if (result.success) {
        showMessage('success', result.message);

        // TODO: Re-implement with new Rust WebSocket backend
        // const manager = getWebSocketManager();
        // manager.setProviderConfig(providerForm.provider_name, {
        //   provider_name: providerForm.provider_name,
        //   enabled: providerForm.enabled,
        //   api_key: providerForm.api_key || undefined,
        //   api_secret: providerForm.api_secret || undefined,
        //   endpoint: providerForm.endpoint || undefined
        // });

        // Reset form
        setProviderForm({
          provider_name: '',
          enabled: true,
          api_key: '',
          api_secret: '',
          endpoint: '',
          config_data: ''
        });
        setSelectedProvider(null);
        await loadWSProviders();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to save provider configuration');
    } finally {
      setLoading(false);
    }
  };

  const handleToggleWSProvider = async (providerName: string) => {
    try {
      const result = await sqliteService.toggleWSProviderEnabled(providerName);
      if (result.success) {
        showMessage('success', result.message);
        await loadWSProviders();

        // TODO: Re-implement with new Rust WebSocket backend
        // const manager = getWebSocketManager();
        // if (result.enabled) {
        //   const config = await sqliteService.getWSProviderConfig(providerName);
        //   if (config) {
        //     manager.setProviderConfig(providerName, config);
        //   }
        // } else {
        //   await manager.disconnect(providerName);
        // }
      }
    } catch (error) {
      showMessage('error', 'Failed to toggle provider');
    }
  };

  const handleDeleteWSProvider = async (providerName: string) => {
    if (!confirm(`Delete configuration for ${providerName}?`)) return;

    try {
      setLoading(true);
      const result = await sqliteService.deleteWSProviderConfig(providerName);

      if (result.success) {
        showMessage('success', result.message);

        // Disconnect from WebSocket Manager
        const manager = getWebSocketManager();
        await manager.disconnect(providerName);

        await loadWSProviders();
      }
    } catch (error) {
      showMessage('error', 'Failed to delete provider');
    } finally {
      setLoading(false);
    }
  };

  const handleConnectProvider = async (providerName: string) => {
    try {
      const manager = getWebSocketManager();

      // First try to get config from database
      const config = await sqliteService.getWSProviderConfig(providerName);

      if (config) {
        // TODO: Re-implement with new Rust WebSocket backend
        // manager.setProviderConfig(providerName, config);
        // const hasSubscriptions = manager.getProviderSubscriptions(providerName).length > 0;
        // if (hasSubscriptions) {
        //   await manager.reconnect(providerName);
        // } else {
          await manager.connect(providerName);
        // }

        showMessage('success', `Connected to ${providerName}`);
      } else {
        showMessage('error', `No configuration found for ${providerName}`);
      }
    } catch (error) {
      showMessage('error', `Failed to connect to ${providerName}: ${error}`);
    }
  };

  const handleDisconnectProvider = async (providerName: string) => {
    try {
      const manager = getWebSocketManager();
      await manager.disconnect(providerName);
      showMessage('success', `Disconnected from ${providerName}`);
    } catch (error) {
      showMessage('error', `Failed to disconnect from ${providerName}`);
    }
  };

  const handleTestProvider = async (providerName: string) => {
    try {
      // TODO: Re-implement ping with new Rust WebSocket backend
      // const manager = getWebSocketManager();
      // const latency = await manager.ping(providerName);
      // showMessage('success', `Ping: ${latency}ms`);
      showMessage('success', `Test connection feature coming soon`);
    } catch (error) {
      showMessage('error', `Ping failed for ${providerName}`);
    }
  };

  const handleUnsubscribe = async (subscriptionId: string, topic: string) => {
    try {
      // TODO: Re-implement with new Rust WebSocket backend
      // const manager = getWebSocketManager();
      // const subscriptions = manager.getSubscriptions();
      // const sub = subscriptions.find(s => s.id === subscriptionId);

      // if (sub) {
      //   sub.unsubscribe();
      //   showMessage('success', `Unsubscribed from ${topic}`);
      // } else {
        showMessage('error', 'Subscription management coming soon');
      // }
    } catch (error) {
      showMessage('error', `Failed to unsubscribe from ${topic}`);
    }
  };

  const loadApiKeys = async () => {
    try {
      const keys = await sqliteService.getAllApiKeys();
      setApiKeys(keys);
    } catch (error) {
      console.error('Failed to load API keys:', error);
    }
  };

  const handleSaveApiKeyField = async (keyName: string, value: string) => {
    setLoading(true);
    try {
      await sqliteService.setApiKey(keyName, value);
      setApiKeys({ ...apiKeys, [keyName]: value });
      setMessage({ type: 'success', text: `${keyName} saved` });
    } catch (error) {
      setMessage({ type: 'error', text: 'Failed to save' });
    } finally {
      setLoading(false);
      setTimeout(() => setMessage(null), 3000);
    }
  };

  const loadLLMConfigs = async () => {
    try {
      let configs = await sqliteService.getLLMConfigs();

      // Initialize Fincept LLM as default if no configs exist
      if (configs.length === 0) {
        // Try to get API key from session profile
        const userApiKey = session?.api_key || '';

        const now = new Date().toISOString();
        const finceptConfig = {
          provider: 'fincept',
          api_key: userApiKey,
          base_url: 'https://finceptbackend.share.zrok.io/research/llm',
          model: 'fincept-llm',
          is_active: true,
          created_at: now,
          updated_at: now,
        };
        await sqliteService.saveLLMConfig(finceptConfig);
        configs = await sqliteService.getLLMConfigs();
      }

      setLlmConfigs(configs);

      const globalSettings = await sqliteService.getLLMGlobalSettings();
      setLlmGlobalSettings(globalSettings);

      const activeConfig = configs.find(c => c.is_active);
      if (activeConfig) {
        setActiveProvider(activeConfig.provider);
      }

      // Load model configs (multi-model support)
      const modelCfgs = await sqliteService.getLLMModelConfigs();
      setModelConfigs(modelCfgs);
    } catch (error) {
      console.error('Failed to load LLM configs:', error);
    }
  };

  // Add a new model configuration
  const handleAddModelConfig = async () => {
    if (!newModelConfig.model_id) {
      showMessage('error', 'Model ID is required');
      return;
    }

    try {
      setLoading(true);
      const id = crypto.randomUUID();
      const displayName = newModelConfig.display_name || `${newModelConfig.provider.toUpperCase()}: ${newModelConfig.model_id}`;

      const result = await sqliteService.saveLLMModelConfig({
        id,
        provider: newModelConfig.provider,
        model_id: newModelConfig.model_id,
        display_name: displayName,
        api_key: newModelConfig.api_key || undefined,
        base_url: newModelConfig.base_url || undefined,
        is_enabled: true,
        is_default: modelConfigs.filter(m => m.provider === newModelConfig.provider).length === 0
      });

      if (result.success) {
        showMessage('success', 'Model configuration added');
        setNewModelConfig({
          provider: 'openai',
          model_id: '',
          display_name: '',
          api_key: '',
          base_url: ''
        });
        setShowAddModel(false);
        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to add model configuration');
    } finally {
      setLoading(false);
    }
  };

  // Delete a model configuration
  const handleDeleteModelConfig = async (id: string) => {
    try {
      setLoading(true);
      const result = await sqliteService.deleteLLMModelConfig(id);
      if (result.success) {
        showMessage('success', 'Model configuration deleted');
        await loadLLMConfigs();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to delete model configuration');
    } finally {
      setLoading(false);
    }
  };

  // Toggle model enabled state
  const handleToggleModelEnabled = async (id: string) => {
    try {
      const result = await sqliteService.toggleLLMModelConfigEnabled(id);
      if (result.success) {
        await loadLLMConfigs();
      }
    } catch (error) {
      showMessage('error', 'Failed to toggle model');
    }
  };


  const handleSaveLLMConfig = async () => {
    try {
      setLoading(true);

      // Save all provider configs
      for (const config of llmConfigs) {
        await sqliteService.saveLLMConfig(config);
      }

      // Set active provider
      await sqliteService.setActiveLLMProvider(activeProvider);

      // Save global settings
      await sqliteService.saveLLMGlobalSettings(llmGlobalSettings);

      showMessage('success', 'LLM configuration saved successfully');
      await loadLLMConfigs();
    } catch (error) {
      showMessage('error', 'Failed to save LLM configuration');
    } finally {
      setLoading(false);
    }
  };

  const updateLLMConfig = (provider: string, field: keyof LLMConfig, value: any) => {
    setLlmConfigs(prev => prev.map(config =>
      config.provider === provider ? { ...config, [field]: value } : config
    ));
  };

  const fetchOllamaModels = async () => {
    setOllamaLoading(true);
    setOllamaError(null);

    const currentConfig = getCurrentLLMConfig();
    const baseUrl = currentConfig?.base_url || 'http://localhost:11434';

    const result = await ollamaService.listModels(baseUrl);

    if (result.success && result.models) {
      setOllamaModels(result.models);
      setOllamaError(null);
    } else {
      setOllamaModels([]);
      setOllamaError(result.error || 'Failed to fetch models');
    }

    setOllamaLoading(false);
  };

  // Fetch Ollama models when switching to Ollama provider
  useEffect(() => {
    if (activeProvider === 'ollama' && activeSection === 'llm') {
      fetchOllamaModels();
    }
  }, [activeProvider, activeSection]);

  const togglePasswordVisibility = (id: number) => {
    setShowPasswords(prev => ({ ...prev, [id]: !prev[id] }));
  };

  const showMessage = (type: 'success' | 'error', text: string) => {
    setMessage({ type, text });
    setTimeout(() => setMessage(null), 5000);
  };

  const getCurrentLLMConfig = () => llmConfigs.find(c => c.provider === activeProvider);

  // Terminal appearance handlers
  const handleSaveTerminalAppearance = () => {
    const newTheme = {
      font: {
        family: fontFamily,
        baseSize: baseSize,
        weight: fontWeight,
        italic: fontItalic
      },
      colors: COLOR_THEMES[selectedTheme]
    };
    updateTheme(newTheme);

    // Save key mappings to localStorage
    localStorage.setItem('fincept_key_mappings', JSON.stringify(keyMappings));

    showMessage('success', 'Terminal appearance and key mappings saved successfully');
  };

  const handleResetTerminalAppearance = () => {
    resetTheme();
    const defaultTheme = terminalThemeService.getTheme();
    setFontFamily(defaultTheme.font.family);
    setBaseSize(defaultTheme.font.baseSize);
    setFontWeight(defaultTheme.font.weight);
    setFontItalic(defaultTheme.font.italic);
    setSelectedTheme('bloomberg-classic');

    // Reset key mappings to defaults
    setKeyMappings(DEFAULT_KEY_MAPPINGS);
    localStorage.removeItem('fincept_key_mappings');

    showMessage('success', 'Reset to default appearance and key mappings');
  };

  // Load saved key mappings on mount
  useEffect(() => {
    const savedMappings = localStorage.getItem('fincept_key_mappings');
    if (savedMappings) {
      try {
        setKeyMappings(JSON.parse(savedMappings));
      } catch (err) {
        console.error('Failed to load key mappings:', err);
      }
    }
  }, []);

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: colors.background }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: ${colors.panel}; }
        *::-webkit-scrollbar-thumb { background: #2a2a2a; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #3a3a3a; }

        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.5; }
        }

        @keyframes spin {
          from { transform: rotate(0deg); }
          to { transform: rotate(360deg); }
        }

        @keyframes slideIn {
          from { transform: translateX(-10px); opacity: 0; }
          to { transform: translateX(0); opacity: 1; }
        }
      `}</style>

      {/* Header */}
      <div style={{
        borderBottom: `2px solid ${colors.primary}`,
        padding: '12px 16px',
        background: `linear-gradient(180deg, #1a1a1a 0%, ${colors.panel} 100%)`,
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <SettingsIcon size={20} color={colors.primary} />
          <span style={{ color: colors.primary, fontSize: '16px', fontWeight: 'bold', letterSpacing: '1px' }}>
            {t('title')}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            padding: '4px 8px',
            background: dbInitialized ? '#0a3a0a' : '#3a0a0a',
            border: `1px solid ${dbInitialized ? '#00ff00' : '#ff0000'}`,
            borderRadius: '3px'
          }}>
            <span style={{ color: dbInitialized ? '#00ff00' : '#ff0000', fontSize: '9px', fontWeight: 'bold' }}>
              <Database size={10} style={{ display: 'inline', marginRight: '4px' }} />
              SQLite: {dbInitialized ? t('llm.connected') : t('llm.disconnected')}
            </span>
          </div>
        </div>
      </div>

      {/* Message Banner */}
      {message && (
        <div style={{
          padding: '8px 16px',
          background: message.type === 'success' ? '#0a3a0a' : '#3a0a0a',
          borderBottom: `1px solid ${message.type === 'success' ? '#00ff00' : '#ff0000'}`,
          color: message.type === 'success' ? '#00ff00' : '#ff0000',
          fontSize: '10px',
          flexShrink: 0
        }}>
          {message.text}
        </div>
      )}

      {/* Main Layout */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>

        {/* Sidebar Navigation */}
        <div style={{ width: '220px', borderRight: '1px solid #1a1a1a', background: colors.panel, flexShrink: 0 }}>
          <div style={{ padding: '16px 0' }}>
            {[
              { id: 'credentials', icon: Lock, label: t('sidebar.credentials') },
              { id: 'llm', icon: Bot, label: t('sidebar.llm') },
              { id: 'dataConnections', icon: Database, label: t('sidebar.dataSources') },
              { id: 'backtesting', icon: Activity, label: t('sidebar.backtesting') },
              { id: 'terminalConfig', icon: SettingsIcon, label: t('sidebar.tabLayout') },
              { id: 'terminal', icon: Terminal, label: t('sidebar.appearance') },
              { id: 'language', icon: Globe, label: t('sidebar.language') }
            ].map((item) => (
              <div
                key={item.id}
                onClick={() => setActiveSection(item.id as any)}
                style={{
                  padding: '12px 16px',
                  cursor: 'pointer',
                  background: activeSection === item.id ? '#1a1a1a' : 'transparent',
                  borderLeft: activeSection === item.id ? `3px solid ${colors.primary}` : '3px solid transparent',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                  transition: 'all 0.2s'
                }}
                onMouseEnter={(e) => {
                  if (activeSection !== item.id) e.currentTarget.style.background = '#151515';
                }}
                onMouseLeave={(e) => {
                  if (activeSection !== item.id) e.currentTarget.style.background = 'transparent';
                }}
              >
                <item.icon size={16} color={activeSection === item.id ? colors.primary : colors.text} />
                <span style={{ color: colors.text, fontSize: '11px', fontWeight: activeSection === item.id ? 'bold' : 'normal' }}>
                  {item.label}
                </span>
              </div>
            ))}
          </div>
        </div>

        {/* Main Content */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
          <div style={{ flex: 1, overflowY: 'auto', padding: '20px', minHeight: 0 }}>

            {/* Credentials Section - Predefined API Keys */}
            {activeSection === 'credentials' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    {t('credentials.title')}
                  </h2>
                  <p style={{ color: colors.text, fontSize: '10px' }}>
                    {t('credentials.description')}
                  </p>
                </div>

                {/* Predefined API Key Fields */}
                <div style={{ display: 'grid', gap: '16px' }}>
                  {PREDEFINED_API_KEYS.map(({ key, label, description }) => (
                    <div
                      key={key}
                      style={{
                        background: colors.panel,
                        border: '1px solid #1a1a1a',
                        padding: '16px',
                        borderRadius: '4px'
                      }}
                    >
                      <h3 style={{ color: colors.primary, fontSize: '12px', fontWeight: 'bold', marginBottom: '4px' }}>
                        {label}
                      </h3>
                      <p style={{ color: colors.text, fontSize: '9px', marginBottom: '12px', opacity: 0.7 }}>
                        {description}
                      </p>

                      <div style={{ display: 'grid', gridTemplateColumns: '1fr auto', gap: '12px', alignItems: 'end' }}>
                        <div>
                          <label style={{ color: colors.text, fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                            {t('credentials.apiKey')}
                          </label>
                          <input
                            type="password"
                            value={apiKeys[key] || ''}
                            onChange={(e) => setApiKeys({ ...apiKeys, [key]: e.target.value })}
                            placeholder={t('credentials.enterValue')}
                            style={{
                              width: '100%',
                              background: colors.background,
                              border: '1px solid #2a2a2a',
                              color: colors.text,
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px',
                              fontFamily: 'monospace'
                            }}
                          />
                        </div>
                        <button
                          onClick={() => handleSaveApiKeyField(key, apiKeys[key] || '')}
                          disabled={loading || !apiKeys[key]}
                          style={{
                            background: apiKeys[key] ? colors.primary : '#333',
                            color: colors.text,
                            border: 'none',
                            padding: '8px 16px',
                            fontSize: '10px',
                            fontWeight: 'bold',
                            cursor: (loading || !apiKeys[key]) ? 'not-allowed' : 'pointer',
                            borderRadius: '3px',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '6px',
                            opacity: (loading || !apiKeys[key]) ? 0.5 : 1,
                            whiteSpace: 'nowrap'
                          }}
                        >
                          <Save size={14} />
                          {t('buttons.save')}
                        </button>
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            )}

            {/* LLM Configuration Section */}
            {activeSection === 'llm' && (
              <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                {/* Top Bar */}
                <div style={{
                  backgroundColor: '#1A1A1A',
                  borderBottom: `2px solid ${colors.primary}`,
                  padding: '6px 12px',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'space-between',
                  boxShadow: `0 2px 8px ${colors.primary}20`
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                    <Bot size={16} color={colors.primary} style={{ filter: `drop-shadow(0 0 4px ${colors.primary})` }} />
                    <span style={{
                      color: colors.primary,
                      fontWeight: 700,
                      fontSize: '11px',
                      letterSpacing: '0.5px',
                      textShadow: `0 0 10px ${colors.primary}40`
                    }}>
                      {t('llm.title').toUpperCase()}
                    </span>
                  </div>
                  <div style={{ color: colors.text, fontSize: '9px' }}>
                    {t('llm.description')}
                  </div>
                </div>

                {/* Main Content Grid */}
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                  {/* Left Column */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                    {/* Provider Selection */}
                    <div style={{
                      backgroundColor: '#0F0F0F',
                      border: `1px solid #2A2A2A`,
                      padding: '12px'
                    }}>
                      <div style={{
                        color: colors.text,
                        fontSize: '10px',
                        fontWeight: 600,
                        marginBottom: '8px',
                        letterSpacing: '0.5px'
                      }}>
                        AI PROVIDER
                      </div>
                      <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
                        {llmConfigs.map(config => (
                          <button
                            key={config.provider}
                            onClick={() => setActiveProvider(config.provider)}
                            style={{
                              padding: '6px 12px',
                              backgroundColor: activeProvider === config.provider ? colors.primary : '#0F0F0F',
                              border: `1px solid ${activeProvider === config.provider ? colors.primary : '#2A2A2A'}`,
                              color: activeProvider === config.provider ? '#000' : colors.text,
                              cursor: 'pointer',
                              fontSize: '9px',
                              fontWeight: 600,
                              letterSpacing: '0.5px',
                              transition: 'all 0.2s'
                            }}
                            onMouseEnter={(e) => {
                              if (activeProvider !== config.provider) {
                                e.currentTarget.style.borderColor = colors.primary;
                              }
                            }}
                            onMouseLeave={(e) => {
                              if (activeProvider !== config.provider) {
                                e.currentTarget.style.borderColor = '#2A2A2A';
                              }
                            }}
                          >
                            {config.provider.toUpperCase()}
                          </button>
                        ))}
                      </div>
                    </div>

                    {/* Fincept Auto-Configured */}
                    {getCurrentLLMConfig() && getCurrentLLMConfig()!.provider === 'fincept' && (
                      <div style={{
                        backgroundColor: '#0a3a0a',
                        border: `1px solid ${colors.success}`,
                        padding: '8px 10px'
                      }}>
                        <div style={{ color: colors.success, fontSize: '9px', fontWeight: 600, marginBottom: '3px' }}>
                          ✓ FINCEPT AUTO-CONFIGURED
                        </div>
                        <div style={{ color: '#888', fontSize: '8px' }}>
                          Key: {session?.api_key?.substring(0, 8)}... • 5 credits/response
                        </div>
                      </div>
                    )}

                    {/* Provider Configuration */}
                    {getCurrentLLMConfig() && getCurrentLLMConfig()!.provider !== 'fincept' && (
                      <div style={{
                        backgroundColor: '#0F0F0F',
                        border: `1px solid #2A2A2A`,
                        padding: '12px'
                      }}>
                        <div style={{
                          color: colors.primary,
                          fontSize: '10px',
                          fontWeight: 700,
                          marginBottom: '10px',
                          letterSpacing: '0.5px',
                          borderBottom: `1px solid #2A2A2A`,
                          paddingBottom: '6px'
                        }}>
                          {getCurrentLLMConfig()!.provider.toUpperCase()} CONFIG
                        </div>

                        <div style={{ display: 'grid', gap: '8px' }}>
                          {/* API Key */}
                          {getCurrentLLMConfig()!.provider !== 'ollama' && (
                            <div>
                              <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                                API KEY *
                              </label>
                              <input
                                type="password"
                                value={getCurrentLLMConfig()!.api_key || ''}
                                onChange={(e) => updateLLMConfig(activeProvider, 'api_key', e.target.value)}
                                placeholder={`Enter ${activeProvider} API key`}
                                style={{
                                  width: '100%',
                                  background: '#000',
                                  border: `1px solid #2A2A2A`,
                                  color: colors.text,
                                  padding: '5px 6px',
                                  fontSize: '9px',
                                  fontFamily: 'monospace'
                                }}
                              />
                            </div>
                          )}

                          {/* Base URL */}
                          {(getCurrentLLMConfig()!.provider === 'ollama' || getCurrentLLMConfig()!.provider === 'deepseek' || getCurrentLLMConfig()!.provider === 'openrouter') && (
                            <div>
                              <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                                BASE URL
                              </label>
                              <input
                                type="text"
                                value={getCurrentLLMConfig()!.base_url || ''}
                                onChange={(e) => updateLLMConfig(activeProvider, 'base_url', e.target.value)}
                                placeholder={getCurrentLLMConfig()!.provider === 'ollama' ? 'http://localhost:11434' : 'Base URL'}
                                style={{
                                  width: '100%',
                                  background: '#000',
                                  border: `1px solid #2A2A2A`,
                                  color: colors.text,
                                  padding: '5px 6px',
                                  fontSize: '9px',
                                  fontFamily: 'monospace'
                                }}
                              />
                            </div>
                          )}

                          {/* Model */}
                          <div>
                            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '3px' }}>
                              <label style={{ color: colors.text, fontSize: '8px', fontWeight: 600 }}>
                                MODEL
                              </label>
                              {getCurrentLLMConfig()!.provider === 'ollama' && (
                                <div style={{ display: 'flex', gap: '4px' }}>
                                  <button
                                    onClick={fetchOllamaModels}
                                    disabled={ollamaLoading}
                                    style={{
                                      background: '#0F0F0F',
                                      border: `1px solid #2A2A2A`,
                                      color: colors.primary,
                                      padding: '2px 6px',
                                      fontSize: '8px',
                                      cursor: ollamaLoading ? 'not-allowed' : 'pointer',
                                      display: 'flex',
                                      alignItems: 'center',
                                      gap: '3px'
                                    }}
                                  >
                                    <RefreshCw size={8} />
                                    {ollamaLoading ? 'LOAD' : 'REFR'}
                                  </button>
                                  <button
                                    onClick={() => setUseManualEntry(!useManualEntry)}
                                    style={{
                                      background: useManualEntry ? colors.primary : '#0F0F0F',
                                      border: `1px solid ${useManualEntry ? colors.primary : '#2A2A2A'}`,
                                      color: useManualEntry ? '#000' : colors.text,
                                      padding: '2px 6px',
                                      fontSize: '8px',
                                      cursor: 'pointer',
                                      display: 'flex',
                                      alignItems: 'center',
                                      gap: '3px'
                                    }}
                                  >
                                    <Edit3 size={8} />
                                    {useManualEntry ? 'DROP' : 'MAN'}
                                  </button>
                                </div>
                              )}
                            </div>

                            {getCurrentLLMConfig()!.provider === 'ollama' && !useManualEntry ? (
                              <div>
                                <select
                                  value={getCurrentLLMConfig()!.model}
                                  onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                                  disabled={ollamaLoading || ollamaModels.length === 0}
                                  style={{
                                    width: '100%',
                                    background: '#000',
                                    border: `1px solid #2A2A2A`,
                                    color: colors.text,
                                    padding: '5px 6px',
                                    fontSize: '9px',
                                    cursor: ollamaLoading || ollamaModels.length === 0 ? 'not-allowed' : 'pointer'
                                  }}
                                >
                                  {ollamaModels.length === 0 && !ollamaLoading && <option value="">No models found</option>}
                                  {ollamaLoading && <option value="">Loading...</option>}
                                  {ollamaModels.map((model) => (
                                    <option key={model} value={model}>{model}</option>
                                  ))}
                                </select>
                                {ollamaError && (
                                  <div style={{
                                    marginTop: '3px',
                                    padding: '3px 5px',
                                    background: '#3a0a0a',
                                    border: `1px solid ${colors.alert}`,
                                    fontSize: '8px',
                                    color: colors.alert
                                  }}>
                                    {ollamaError}
                                  </div>
                                )}
                                {ollamaModels.length > 0 && (
                                  <div style={{ marginTop: '3px', fontSize: '8px', color: colors.success }}>
                                    ✓ {ollamaModels.length} model{ollamaModels.length !== 1 ? 's' : ''}
                                  </div>
                                )}
                              </div>
                            ) : (
                              <input
                                type="text"
                                value={getCurrentLLMConfig()!.model}
                                onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                                placeholder="Model name"
                                style={{
                                  width: '100%',
                                  background: '#000',
                                  border: `1px solid #2A2A2A`,
                                  color: colors.text,
                                  padding: '5px 6px',
                                  fontSize: '9px'
                                }}
                              />
                            )}
                          </div>
                        </div>
                      </div>
                    )}

                    {/* Save Button */}
                    <button
                      onClick={handleSaveLLMConfig}
                      disabled={loading}
                      style={{
                        background: loading ? '#2A2A2A' : colors.primary,
                        color: loading ? '#666' : '#000',
                        border: 'none',
                        padding: '6px 10px',
                        fontSize: '9px',
                        fontWeight: 700,
                        letterSpacing: '0.5px',
                        cursor: loading ? 'not-allowed' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        justifyContent: 'center',
                        gap: '5px'
                      }}
                    >
                      <Save size={10} />
                      {loading ? 'SAVING...' : 'SAVE CONFIG'}
                    </button>
                  </div>

                  {/* Right Column */}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
                    {/* Global Settings */}
                    <div style={{
                      backgroundColor: '#0F0F0F',
                      border: `1px solid #2A2A2A`,
                      padding: '12px'
                    }}>
                      <div style={{
                        color: colors.text,
                        fontSize: '10px',
                        fontWeight: 700,
                        marginBottom: '10px',
                        letterSpacing: '0.5px',
                        borderBottom: `1px solid #2A2A2A`,
                        paddingBottom: '6px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px'
                      }}>
                        <Zap size={12} color={colors.primary} />
                        GLOBAL AI SETTINGS
                      </div>

                      <div style={{ display: 'grid', gap: '8px' }}>
                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            TEMPERATURE
                          </label>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                            <input
                              type="range"
                              min="0"
                              max="2"
                              step="0.1"
                              value={llmGlobalSettings.temperature}
                              onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, temperature: parseFloat(e.target.value) })}
                              style={{ flex: 1, height: '4px', cursor: 'pointer' }}
                            />
                            <div style={{
                              minWidth: '35px',
                              padding: '2px 5px',
                              background: colors.primary + '20',
                              border: `1px solid ${colors.primary}`,
                              color: colors.primary,
                              fontSize: '8px',
                              fontWeight: 600,
                              textAlign: 'center'
                            }}>
                              {llmGlobalSettings.temperature.toFixed(1)}
                            </div>
                          </div>
                        </div>

                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            MAX TOKENS
                          </label>
                          <input
                            type="number"
                            min="1"
                            max="32000"
                            value={llmGlobalSettings.max_tokens}
                            onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, max_tokens: parseInt(e.target.value) })}
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '9px'
                            }}
                          />
                        </div>

                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            SYSTEM PROMPT
                          </label>
                          <textarea
                            value={llmGlobalSettings.system_prompt}
                            onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, system_prompt: e.target.value })}
                            rows={3}
                            placeholder="Custom system prompt (optional)"
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '8px',
                              resize: 'vertical',
                              fontFamily: 'monospace',
                              lineHeight: '1.4'
                            }}
                          />
                        </div>
                      </div>
                    </div>

                    {/* Model Library */}
                    <div style={{
                      backgroundColor: '#0F0F0F',
                      border: `1px solid #2A2A2A`,
                      padding: '12px'
                    }}>
                      <div style={{
                        display: 'flex',
                        justifyContent: 'space-between',
                        alignItems: 'center',
                        marginBottom: '10px',
                        paddingBottom: '6px',
                        borderBottom: `1px solid #2A2A2A`
                      }}>
                        <div>
                          <div style={{ color: colors.text, fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px', marginBottom: '2px' }}>
                            MODEL LIBRARY
                          </div>
                          <div style={{ color: '#666', fontSize: '8px' }}>
                            Multi-model configs
                          </div>
                        </div>
                        <button
                          onClick={() => setShowAddModel(!showAddModel)}
                          style={{
                            background: showAddModel ? '#3a0a0a' : colors.primary,
                            color: showAddModel ? colors.alert : '#000',
                            border: 'none',
                            padding: '3px 8px',
                            fontSize: '8px',
                            fontWeight: 600,
                            cursor: 'pointer',
                            display: 'flex',
                            alignItems: 'center',
                            gap: '3px'
                          }}
                        >
                          {showAddModel ? <><Trash2 size={8} /> CANCEL</> : <><Plus size={8} /> ADD</>}
                        </button>
                      </div>

                  {/* Add Model Form */}
                  {showAddModel && (
                    <div style={{
                      background: '#000',
                      border: `1px solid #2A2A2A`,
                      padding: '10px',
                      marginBottom: '10px'
                    }}>
                      <div style={{ color: colors.primary, fontSize: '9px', fontWeight: 600, marginBottom: '8px' }}>
                        ADD NEW MODEL
                      </div>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '8px' }}>
                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            PROVIDER
                          </label>
                          <select
                            value={newModelConfig.provider}
                            onChange={(e) => setNewModelConfig({ ...newModelConfig, provider: e.target.value })}
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '9px',
                              cursor: 'pointer'
                            }}
                          >
                            <option value="openai">OpenAI</option>
                            <option value="google">Google (Gemini)</option>
                            <option value="anthropic">Anthropic (Claude)</option>
                            <option value="deepseek">DeepSeek</option>
                            <option value="ollama">Ollama (Local)</option>
                            <option value="openrouter">OpenRouter</option>
                            <option value="groq">Groq</option>
                          </select>
                        </div>
                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            MODEL ID *
                          </label>
                          <input
                            type="text"
                            value={newModelConfig.model_id}
                            onChange={(e) => setNewModelConfig({ ...newModelConfig, model_id: e.target.value })}
                            placeholder="e.g. gpt-4o-mini"
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '9px'
                            }}
                          />
                        </div>
                      </div>

                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '8px' }}>
                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            DISPLAY NAME
                          </label>
                          <input
                            type="text"
                            value={newModelConfig.display_name}
                            onChange={(e) => setNewModelConfig({ ...newModelConfig, display_name: e.target.value })}
                            placeholder="e.g. GPT-4o Mini"
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '9px'
                            }}
                          />
                        </div>
                        <div>
                          <label style={{ color: colors.text, fontSize: '8px', display: 'block', marginBottom: '3px', fontWeight: 600 }}>
                            API KEY (Optional)
                          </label>
                          <input
                            type="password"
                            value={newModelConfig.api_key}
                            onChange={(e) => setNewModelConfig({ ...newModelConfig, api_key: e.target.value })}
                            placeholder="Uses provider key if empty"
                            style={{
                              width: '100%',
                              background: '#000',
                              border: `1px solid #2A2A2A`,
                              color: colors.text,
                              padding: '5px 6px',
                              fontSize: '9px',
                              fontFamily: 'monospace'
                            }}
                          />
                        </div>
                      </div>

                      <button
                        onClick={handleAddModelConfig}
                        disabled={loading || !newModelConfig.model_id}
                        style={{
                          background: loading || !newModelConfig.model_id ? '#2A2A2A' : colors.primary,
                          color: loading || !newModelConfig.model_id ? '#666' : '#000',
                          border: 'none',
                          padding: '5px 12px',
                          fontSize: '9px',
                          fontWeight: 600,
                          cursor: loading || !newModelConfig.model_id ? 'not-allowed' : 'pointer',
                          display: 'flex',
                          alignItems: 'center',
                          gap: '4px'
                        }}
                      >
                        <Plus size={10} />
                        ADD MODEL
                      </button>
                    </div>
                  )}

                  {/* Models List */}
                  {modelConfigs.length === 0 ? (
                    <div style={{
                      padding: '20px',
                      textAlign: 'center',
                      color: '#666',
                      fontSize: '9px',
                      border: `1px dashed #2A2A2A`,
                      background: '#000'
                    }}>
                      <Bot size={24} color="#2A2A2A" style={{ marginBottom: '8px' }} />
                      <div style={{ marginBottom: '4px', fontWeight: 600 }}>No models configured</div>
                      <div style={{ fontSize: '8px' }}>Click "ADD" to configure models</div>
                    </div>
                  ) : (
                    <div style={{ display: 'grid', gap: '6px' }}>
                      {modelConfigs.map((model) => (
                        <div
                          key={model.id}
                          style={{
                            display: 'grid',
                            gridTemplateColumns: 'auto 1fr auto',
                            alignItems: 'center',
                            gap: '10px',
                            padding: '8px',
                            background: model.is_enabled ? '#1A1A1A' : '#000',
                            border: `1px solid ${model.is_enabled ? '#2A2A2A' : '#1a1a1a'}`
                          }}
                        >
                          <button
                            onClick={() => handleToggleModelEnabled(model.id)}
                            style={{
                              background: 'transparent',
                              border: 'none',
                              cursor: 'pointer',
                              padding: 0
                            }}
                          >
                            {model.is_enabled ? (
                              <ToggleRight size={18} color={colors.success} />
                            ) : (
                              <ToggleLeft size={18} color="#666" />
                            )}
                          </button>
                          <div>
                            <div style={{
                              fontSize: '10px',
                              fontWeight: 600,
                              color: model.is_enabled ? colors.text : '#666',
                              marginBottom: '3px'
                            }}>
                              {model.display_name || model.model_id}
                            </div>
                            <div style={{ fontSize: '8px', color: '#666', display: 'flex', alignItems: 'center', gap: '6px' }}>
                              <span style={{
                                padding: '1px 4px',
                                background: colors.primary + '20',
                                border: `1px solid ${colors.primary}`,
                                color: colors.primary,
                                fontWeight: 600
                              }}>
                                {model.provider.toUpperCase()}
                              </span>
                              <span>{model.model_id}</span>
                              {model.api_key && (
                                <span style={{
                                  padding: '1px 4px',
                                  background: colors.success + '20',
                                  border: `1px solid ${colors.success}`,
                                  color: colors.success,
                                  fontWeight: 600
                                }}>
                                  KEY
                                </span>
                              )}
                            </div>
                          </div>
                          <div style={{ display: 'flex', gap: '4px' }}>
                            {model.api_key && (
                              <button
                                onClick={() => setShowApiKeys(prev => ({ ...prev, [model.id]: !prev[model.id] }))}
                                style={{
                                  background: '#0F0F0F',
                                  border: `1px solid #2A2A2A`,
                                  color: '#666',
                                  padding: '4px',
                                  cursor: 'pointer'
                                }}
                              >
                                {showApiKeys[model.id] ? <EyeOff size={10} /> : <Eye size={10} />}
                              </button>
                            )}
                            <button
                              onClick={() => handleDeleteModelConfig(model.id)}
                              style={{
                                background: '#0F0F0F',
                                border: `1px solid #2A2A2A`,
                                color: colors.alert,
                                padding: '4px 6px',
                                fontSize: '8px',
                                cursor: 'pointer',
                                display: 'flex',
                                alignItems: 'center',
                                gap: '3px'
                              }}
                            >
                              <Trash2 size={8} />
                              DEL
                            </button>
                          </div>
                        </div>
                      ))}
                    </div>
                  )}

                      {/* Summary */}
                      {modelConfigs.length > 0 && (
                        <div style={{
                          marginTop: '8px',
                          padding: '5px 6px',
                          background: '#0a3a0a',
                          border: `1px solid ${colors.success}`,
                          display: 'flex',
                          alignItems: 'center',
                          gap: '6px'
                        }}>
                          <Check size={10} color={colors.success} />
                          <div style={{ fontSize: '8px', color: colors.success, fontWeight: 600 }}>
                            {modelConfigs.filter(m => m.is_enabled).length} ENABLED · {modelConfigs.length} TOTAL
                          </div>
                        </div>
                      )}
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Terminal Tab Configuration Section */}
            {activeSection === 'terminalConfig' && (
              <TerminalConfigPanel />
            )}

            {/* Terminal Appearance Section */}
            {activeSection === 'terminal' && (
              <div style={{ fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
                {/* Header */}
                <div style={{
                  display: 'flex',
                  alignItems: 'center',
                  gap: '12px',
                  marginBottom: '24px',
                  paddingBottom: '12px',
                  borderBottom: `2px solid ${colors.primary}`
                }}>
                  <Terminal size={20} color={colors.primary} style={{ filter: `drop-shadow(0 0 4px ${colors.primary})` }} />
                  <h2 style={{
                    color: colors.primary,
                    fontSize: '16px',
                    fontWeight: 700,
                    letterSpacing: '1px',
                    margin: 0,
                    textShadow: `0 0 10px ${colors.primary}40`
                  }}>
                    {t('terminal.title')}
                  </h2>
                </div>

                {/* Description */}
                <div style={{
                  marginBottom: '20px',
                  padding: '12px',
                  backgroundColor: '#1A1A1A',
                  border: '1px solid #2A2A2A',
                  borderLeft: `3px solid ${colors.secondary}`
                }}>
                  <p style={{
                    color: colors.textMuted,
                    fontSize: '10px',
                    margin: 0,
                    lineHeight: '1.6'
                  }}>
                    {t('terminal.description')}
                  </p>
                </div>

                {/* Font Settings */}
                <div style={{
                  background: '#0F0F0F',
                  border: '1px solid #2A2A2A',
                  borderLeft: `3px solid ${colors.primary}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '20px'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                    <Type size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
                    <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>{t('terminal.currentSettings')}</span>
                  </div>

                  {/* Font Family */}
                  <div style={{ marginBottom: '16px' }}>
                    <label style={{ color: colors.text, fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>{t('terminal.fontFamily')}</label>
                    <select value={fontFamily} onChange={(e) => setFontFamily(e.target.value)} style={{
                      width: '100%',
                      padding: '10px',
                      background: colors.background,
                      border: `1px solid #2A2A2A`,
                      color: colors.text,
                      borderRadius: '3px',
                      fontSize: '11px',
                      fontWeight: 500,
                      cursor: 'pointer',
                      appearance: 'none',
                      backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                      backgroundRepeat: 'no-repeat',
                      backgroundPosition: 'right 10px center',
                      paddingRight: '32px'
                    }}>
                      {FONT_FAMILIES.map(f => <option key={f.value} value={f.value}>{f.label}</option>)}
                    </select>
                  </div>

                  {/* Base Font Size */}
                  <div style={{ marginBottom: '16px' }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: '8px' }}>
                      <label style={{ color: colors.text, fontSize: '10px', fontWeight: 600, letterSpacing: '0.5px' }}>BASE FONT SIZE</label>
                      <span style={{
                        color: colors.primary,
                        fontSize: '12px',
                        fontWeight: 'bold',
                        padding: '2px 8px',
                        background: `${colors.primary}20`,
                        borderRadius: '3px',
                        border: `1px solid ${colors.primary}40`
                      }}>{baseSize}px</span>
                    </div>
                    <input type="range" min="9" max="18" value={baseSize} onChange={(e) => setBaseSize(Number(e.target.value))} style={{
                      width: '100%',
                      height: '6px',
                      background: `linear-gradient(to right, ${colors.primary}40 0%, ${colors.primary}40 ${((baseSize - 9) / 9) * 100}%, #2A2A2A ${((baseSize - 9) / 9) * 100}%, #2A2A2A 100%)`,
                      borderRadius: '3px',
                      outline: 'none',
                      cursor: 'pointer'
                    }} />
                    <div style={{ display: 'flex', justifyContent: 'space-between', marginTop: '6px' }}>
                      <span style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 500 }}>Small (9px)</span>
                      <span style={{ color: colors.textMuted, fontSize: '9px', fontWeight: 500 }}>Large (18px)</span>
                    </div>
                  </div>

                  {/* Font Weight */}
                  <div style={{ marginBottom: '16px' }}>
                    <label style={{ color: colors.text, fontSize: '10px', display: 'block', marginBottom: '6px', fontWeight: 600, letterSpacing: '0.5px' }}>FONT WEIGHT</label>
                    <select value={fontWeight} onChange={(e) => setFontWeight(e.target.value as FontSettings['weight'])} style={{
                      width: '100%',
                      padding: '10px',
                      background: colors.background,
                      border: '1px solid #2A2A2A',
                      color: colors.text,
                      borderRadius: '3px',
                      fontSize: '11px',
                      fontWeight: 500,
                      cursor: 'pointer',
                      appearance: 'none',
                      backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                      backgroundRepeat: 'no-repeat',
                      backgroundPosition: 'right 10px center',
                      paddingRight: '32px'
                    }}>
                      <option value="normal">Normal (400)</option>
                      <option value="semibold">Semi-Bold (600)</option>
                      <option value="bold">Bold (700)</option>
                    </select>
                  </div>

                  {/* Font Style */}
                  <div>
                    <label style={{
                      display: 'flex',
                      alignItems: 'center',
                      gap: '10px',
                      cursor: 'pointer',
                      padding: '10px',
                      background: fontItalic ? `${colors.primary}10` : 'transparent',
                      border: `1px solid ${fontItalic ? colors.primary : '#2A2A2A'}`,
                      borderRadius: '3px',
                      transition: 'all 0.2s'
                    }}>
                      <input type="checkbox" checked={fontItalic} onChange={(e) => setFontItalic(e.target.checked)} style={{ width: '16px', height: '16px', cursor: 'pointer' }} />
                      <span style={{ color: colors.text, fontSize: '10px', fontWeight: 600, letterSpacing: '0.5px' }}>ENABLE ITALIC STYLE</span>
                    </label>
                  </div>
                </div>

                {/* Color Theme */}
                <div style={{
                  background: '#0F0F0F',
                  border: '1px solid #2A2A2A',
                  borderLeft: `3px solid ${colors.primary}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '20px'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                    <Palette size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
                    <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>COLOR THEME</span>
                  </div>

                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(300px, 1fr))', gap: '12px' }}>
                    {Object.entries(COLOR_THEMES).map(([key, themeObj]) => {
                      const isActive = selectedTheme === key;
                      return (
                        <button
                          key={key}
                          onClick={() => setSelectedTheme(key)}
                          style={{
                            display: 'flex',
                            alignItems: 'center',
                            gap: '12px',
                            padding: '12px',
                            background: isActive ? '#1A1A1A' : 'transparent',
                            border: `1px solid ${isActive ? colors.primary : '#2A2A2A'}`,
                            borderLeft: `3px solid ${isActive ? colors.primary : '#2A2A2A'}`,
                            borderRadius: '3px',
                            cursor: 'pointer',
                            transition: 'all 0.2s',
                            position: 'relative',
                            overflow: 'hidden'
                          }}
                          onMouseEnter={(e) => {
                            if (!isActive) {
                              e.currentTarget.style.background = '#151515';
                              e.currentTarget.style.borderColor = '#3A3A3A';
                            }
                          }}
                          onMouseLeave={(e) => {
                            if (!isActive) {
                              e.currentTarget.style.background = 'transparent';
                              e.currentTarget.style.borderColor = '#2A2A2A';
                            }
                          }}
                        >
                          {/* Color Swatches */}
                          <div style={{ display: 'flex', gap: '6px' }}>
                            <div style={{
                              width: '24px',
                              height: '24px',
                              background: themeObj.primary,
                              border: '1px solid #2A2A2A',
                              borderRadius: '3px',
                              boxShadow: isActive ? `0 0 8px ${themeObj.primary}60` : 'none'
                            }} title="Primary" />
                            <div style={{
                              width: '24px',
                              height: '24px',
                              background: themeObj.secondary,
                              border: '1px solid #2A2A2A',
                              borderRadius: '3px',
                              boxShadow: isActive ? `0 0 8px ${themeObj.secondary}60` : 'none'
                            }} title="Secondary" />
                            <div style={{
                              width: '24px',
                              height: '24px',
                              background: themeObj.alert,
                              border: '1px solid #2A2A2A',
                              borderRadius: '3px',
                              boxShadow: isActive ? `0 0 8px ${themeObj.alert}60` : 'none'
                            }} title="Alert" />
                          </div>

                          <span style={{
                            color: isActive ? colors.primary : colors.text,
                            fontSize: '11px',
                            flex: 1,
                            fontWeight: isActive ? 700 : 500,
                            letterSpacing: '0.5px'
                          }}>{themeObj.name}</span>

                          {isActive && (
                            <div style={{
                              display: 'flex',
                              alignItems: 'center',
                              gap: '4px'
                            }}>
                              <Check size={14} color={colors.success} />
                              <div style={{
                                width: '6px',
                                height: '6px',
                                borderRadius: '50%',
                                background: colors.success,
                                boxShadow: `0 0 6px ${colors.success}`
                              }} />
                            </div>
                          )}

                          {isActive && (
                            <div style={{
                              position: 'absolute',
                              top: 0,
                              left: 0,
                              right: 0,
                              height: '1px',
                              background: `linear-gradient(90deg, transparent, ${colors.primary}, transparent)`,
                              opacity: 0.5
                            }} />
                          )}
                        </button>
                      );
                    })}
                  </div>
                </div>

                {/* Default Timezone Selection */}
                <div style={{
                  background: '#0F0F0F',
                  border: '1px solid #2A2A2A',
                  borderLeft: `3px solid ${colors.primary}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '20px'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                    <Clock size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
                    <span style={{ color: colors.text, fontWeight: 'bold', fontSize: '12px' }}>
                      DEFAULT TIMEZONE
                    </span>
                  </div>
                  <p style={{ color: colors.textMuted, fontSize: '10px', marginBottom: '12px' }}>
                    Set the default timezone displayed in the navigation bar. Dashboard widgets can use their own timezone settings.
                  </p>
                  <div style={{
                    display: 'grid',
                    gridTemplateColumns: 'repeat(4, 1fr)',
                    gap: '8px',
                    maxHeight: '280px',
                    overflowY: 'auto',
                    paddingRight: '4px'
                  }}>
                    {timezoneOptions.map((tz) => (
                      <button
                        key={tz.id}
                        onClick={() => setDefaultTimezone(tz.id)}
                        style={{
                          background: defaultTimezone.id === tz.id ? `${colors.primary}20` : '#1A1A1A',
                          border: `1px solid ${defaultTimezone.id === tz.id ? colors.primary : '#2A2A2A'}`,
                          borderRadius: '4px',
                          padding: '12px 8px',
                          cursor: 'pointer',
                          textAlign: 'center',
                          transition: 'all 0.2s'
                        }}
                        onMouseEnter={(e) => {
                          if (defaultTimezone.id !== tz.id) {
                            e.currentTarget.style.borderColor = colors.primary;
                            e.currentTarget.style.background = '#2A2A2A';
                          }
                        }}
                        onMouseLeave={(e) => {
                          if (defaultTimezone.id !== tz.id) {
                            e.currentTarget.style.borderColor = '#2A2A2A';
                            e.currentTarget.style.background = '#1A1A1A';
                          }
                        }}
                      >
                        <div style={{
                          color: defaultTimezone.id === tz.id ? colors.primary : colors.text,
                          fontWeight: 'bold',
                          fontSize: '11px',
                          marginBottom: '4px'
                        }}>
                          {tz.shortLabel}
                        </div>
                        <div style={{
                          color: colors.textMuted,
                          fontSize: '9px'
                        }}>
                          {tz.label.split('(')[0].trim()}
                        </div>
                      </button>
                    ))}
                  </div>
                </div>

                {/* Key Mapping Configuration */}
                <div style={{
                  background: '#0F0F0F',
                  border: '1px solid #2A2A2A',
                  borderLeft: `3px solid ${colors.primary}`,
                  borderRadius: '4px',
                  padding: '16px',
                  marginBottom: '20px'
                }}>
                  <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
                    <Terminal size={18} color={colors.primary} style={{ filter: `drop-shadow(0 0 2px ${colors.primary})` }} />
                    <span style={{ color: colors.primary, fontSize: '14px', fontWeight: 'bold', letterSpacing: '0.5px' }}>FUNCTION KEY MAPPINGS</span>
                  </div>

                  <div style={{
                    marginBottom: '12px',
                    padding: '10px',
                    backgroundColor: '#1A1A1A',
                    border: '1px solid #2A2A2A',
                    borderRadius: '3px'
                  }}>
                    <p style={{
                      color: colors.textMuted,
                      fontSize: '9px',
                      margin: 0,
                      lineHeight: '1.5'
                    }}>
                      Configure which tab or action each function key (F1-F12) should trigger. Changes will be reflected in the status bar and keyboard shortcuts.
                    </p>
                  </div>

                  <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(280px, 1fr))', gap: '12px' }}>
                    {Object.entries(keyMappings).map(([key, action]) => (
                      <div key={key} style={{
                        background: '#1A1A1A',
                        border: '1px solid #2A2A2A',
                        borderRadius: '3px',
                        padding: '12px'
                      }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', marginBottom: '8px' }}>
                          <span style={{
                            backgroundColor: colors.primary,
                            color: '#000',
                            padding: '4px 8px',
                            fontSize: '9px',
                            fontWeight: 'bold',
                            borderRadius: '2px',
                            minWidth: '40px',
                            textAlign: 'center'
                          }}>
                            {key}
                          </span>
                          <span style={{ color: colors.textMuted, fontSize: '10px' }}>→</span>
                        </div>
                        <select
                          value={action}
                          onChange={(e) => setKeyMappings({ ...keyMappings, [key]: e.target.value })}
                          style={{
                            width: '100%',
                            padding: '8px',
                            background: colors.background,
                            border: '1px solid #2A2A2A',
                            color: colors.text,
                            borderRadius: '3px',
                            fontSize: '10px',
                            fontWeight: 500,
                            cursor: 'pointer',
                            appearance: 'none',
                            backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23FFA500' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                            backgroundRepeat: 'no-repeat',
                            backgroundPosition: 'right 8px center',
                            paddingRight: '28px'
                          }}
                        >
                          <option value="dashboard">Dashboard</option>
                          <option value="markets">Markets</option>
                          <option value="news">News</option>
                          <option value="portfolio">Portfolio</option>
                          <option value="analytics">Analytics</option>
                          <option value="watchlist">Watchlist</option>
                          <option value="equity-research">Equity Research</option>
                          <option value="screener">Screener</option>
                          <option value="trading">Trading</option>
                          <option value="chat">AI Chat</option>
                          <option value="fullscreen">Toggle Fullscreen</option>
                          <option value="profile">Profile</option>
                          <option value="settings">Settings</option>
                          <option value="forum">Forum</option>
                          <option value="marketplace">Marketplace</option>
                        </select>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Live Preview Panel */}
                <div style={{
                  background: theme.colors.background,
                  border: `2px solid ${theme.colors.primary}`,
                  borderRadius: '4px',
                  padding: '20px',
                  marginBottom: '20px',
                  position: 'relative',
                  overflow: 'hidden'
                }}>
                  <div style={{
                    position: 'absolute',
                    top: 0,
                    left: 0,
                    right: 0,
                    height: '2px',
                    background: `linear-gradient(90deg, transparent, ${theme.colors.primary}, transparent)`,
                    opacity: 0.5
                  }} />

                  <div style={{
                    color: theme.colors.textMuted,
                    fontSize: '9px',
                    fontWeight: 600,
                    letterSpacing: '1px',
                    marginBottom: '12px',
                    opacity: 0.7
                  }}>
                    LIVE PREVIEW
                  </div>

                  <div style={{
                    color: theme.colors.primary,
                    fontSize: `${baseSize + 2}px`,
                    fontFamily: `${fontFamily}, monospace`,
                    fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
                    fontStyle: fontItalic ? 'italic' : 'normal',
                    marginBottom: '12px',
                    letterSpacing: '0.5px'
                  }}>
                    FINCEPT TERMINAL PREVIEW
                  </div>

                  <div style={{
                    color: theme.colors.text,
                    fontSize: `${baseSize}px`,
                    fontFamily: `${fontFamily}, monospace`,
                    fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
                    fontStyle: fontItalic ? 'italic' : 'normal',
                    marginBottom: '8px',
                    lineHeight: '1.5'
                  }}>
                    Body Text: Real-time market analysis and portfolio tracking
                  </div>

                  <div style={{
                    color: theme.colors.textMuted,
                    fontSize: `${baseSize - 1}px`,
                    fontFamily: `${fontFamily}, monospace`,
                    fontWeight: fontWeight === 'normal' ? '400' : fontWeight === 'semibold' ? '600' : '700',
                    fontStyle: fontItalic ? 'italic' : 'normal',
                    marginBottom: '12px',
                    lineHeight: '1.5'
                  }}>
                    Small Text: Market data, timestamps, and metadata
                  </div>

                  <div style={{ display: 'flex', gap: '16px', marginTop: '12px', flexWrap: 'wrap' }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{
                        width: '8px',
                        height: '8px',
                        borderRadius: '50%',
                        background: theme.colors.success,
                        boxShadow: `0 0 8px ${theme.colors.success}`
                      }} />
                      <span style={{ color: theme.colors.success, fontSize: '10px', fontWeight: 600 }}>CONNECTED</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{
                        width: '8px',
                        height: '8px',
                        borderRadius: '50%',
                        background: theme.colors.alert,
                        boxShadow: `0 0 8px ${theme.colors.alert}`
                      }} />
                      <span style={{ color: theme.colors.alert, fontSize: '10px', fontWeight: 600 }}>ALERT</span>
                    </div>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      <div style={{
                        width: '8px',
                        height: '8px',
                        borderRadius: '50%',
                        background: theme.colors.warning,
                        boxShadow: `0 0 8px ${theme.colors.warning}`
                      }} />
                      <span style={{ color: theme.colors.warning, fontSize: '10px', fontWeight: 600 }}>WARNING</span>
                    </div>
                  </div>
                </div>

                {/* Action Buttons */}
                <div style={{ display: 'flex', gap: '12px' }}>
                  <button onClick={handleSaveTerminalAppearance} style={{
                    flex: 1,
                    background: colors.primary,
                    color: '#000000',
                    border: 'none',
                    padding: '14px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    letterSpacing: '0.5px',
                    borderRadius: '4px',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    gap: '8px',
                    transition: 'all 0.2s',
                    boxShadow: `0 0 12px ${colors.primary}40`
                  }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.boxShadow = `0 0 20px ${colors.primary}60`;
                      e.currentTarget.style.transform = 'translateY(-1px)';
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.boxShadow = `0 0 12px ${colors.primary}40`;
                      e.currentTarget.style.transform = 'translateY(0)';
                    }}>
                    <Save size={16} /> SAVE CONFIGURATION
                  </button>
                  <button onClick={handleResetTerminalAppearance} style={{
                    background: 'transparent',
                    color: colors.textMuted,
                    border: `1px solid #2A2A2A`,
                    padding: '14px 20px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    letterSpacing: '0.5px',
                    borderRadius: '4px',
                    cursor: 'pointer',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '8px',
                    transition: 'all 0.2s'
                  }}
                    onMouseEnter={(e) => {
                      e.currentTarget.style.borderColor = colors.alert;
                      e.currentTarget.style.color = colors.alert;
                    }}
                    onMouseLeave={(e) => {
                      e.currentTarget.style.borderColor = '#2A2A2A';
                      e.currentTarget.style.color = colors.textMuted;
                    }}>
                    <RefreshCw size={16} /> RESET
                  </button>
                </div>
              </div>
            )}

            {/* Data Sources Section (Unified WebSocket + REST API) */}
            {activeSection === 'dataConnections' && (
              <DataSourcesPanel colors={colors} />
            )}

            {/* Backtesting Providers Section */}
            {activeSection === 'backtesting' && (
              <BacktestingProvidersPanel colors={{
                background: colors.background,
                surface: colors.panel,
                text: colors.text,
                textSecondary: colors.textMuted,
                border: colors.textMuted,
                accent: colors.accent,
                success: colors.success,
                error: colors.alert
              }} />
            )}

            {/* Language Section */}
            {activeSection === 'language' && (
              <div style={{ padding: '20px' }}>
                <LanguageSelector />
              </div>
            )}

            {/* Other sections can be added here */}

          </div>
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="SETTINGS"
        leftInfo={[
          { label: 'Database: SQLite', color: colors.text },
          { label: 'Storage: File-based', color: colors.text },
        ]}
        statusInfo="All data stored securely in fincept_terminal.db"
        backgroundColor={colors.panel}
        borderColor={colors.primary}
      />
    </div>
  );
}
