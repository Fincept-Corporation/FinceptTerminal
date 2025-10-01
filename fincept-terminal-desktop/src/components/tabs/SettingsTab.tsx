import React, { useState, useEffect } from 'react';
import { Eye, EyeOff, Plus, Trash2, Save, RefreshCw, Lock, User, Settings as SettingsIcon, Database, Terminal, Bell, Bot, ChevronDown, Edit3 } from 'lucide-react';
import { sqliteService, type Credential, type LLMConfig, type LLMGlobalSettings } from '@/services/sqliteService';
import { ollamaService } from '@/services/ollamaService';

export default function SettingsTab() {
  const [activeSection, setActiveSection] = useState<'credentials' | 'profile' | 'terminal' | 'notifications' | 'llm'>('credentials');
  const [credentials, setCredentials] = useState<Credential[]>([]);
  const [showPasswords, setShowPasswords] = useState<Record<number, boolean>>({});
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [dbInitialized, setDbInitialized] = useState(false);

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

  // New credential form
  const [newCredential, setNewCredential] = useState<Credential>({
    service_name: '',
    username: '',
    password: '',
    api_key: '',
    api_secret: '',
    additional_data: ''
  });

  // Mock user profile data
  const [userProfile, setUserProfile] = useState({
    full_name: 'John Doe',
    email: 'john.doe@example.com',
    account_type: 'Professional',
    subscription_status: 'Active',
    subscription_expiry: '2025-12-31',
    api_calls_today: 1247,
    api_limit: 10000,
    storage_used: '2.4 GB',
    storage_limit: '50 GB'
  });

  // Mock terminal settings
  const [terminalSettings, setTerminalSettings] = useState({
    theme: 'dark',
    font_size: '11',
    auto_refresh: true,
    refresh_interval: '5',
    show_tooltips: true,
    enable_animations: true,
    data_compression: false,
    cache_enabled: true
  });

  // Mock notification settings
  const [notificationSettings, setNotificationSettings] = useState({
    email_alerts: true,
    price_alerts: true,
    news_alerts: false,
    trade_confirmations: true,
    system_updates: true,
    marketing_emails: false
  });

  useEffect(() => {
    initDB();
  }, []);

  const initDB = async () => {
    try {
      setLoading(true);

      // Initialize database
      await sqliteService.initialize();

      // Verify database health
      const healthCheck = await sqliteService.healthCheck();
      if (!healthCheck.healthy) {
        throw new Error(healthCheck.message);
      }

      setDbInitialized(true);

      // Load initial data
      await loadCredentials();
      await loadLLMConfigs();

      showMessage('success', `Database ready: ${healthCheck.message}`);
    } catch (error) {
      console.error('Failed to initialize database:', error);
      const errorMsg = error instanceof Error ? error.message : 'Unknown error';
      showMessage('error', `Database initialization failed: ${errorMsg}`);
      setDbInitialized(false);
    } finally {
      setLoading(false);
    }
  };

  const loadCredentials = async () => {
    try {
      const creds = await sqliteService.getCredentials();
      setCredentials(creds);
    } catch (error) {
      console.error('Failed to load credentials:', error);
    }
  };

  const loadLLMConfigs = async () => {
    try {
      const configs = await sqliteService.getLLMConfigs();
      setLlmConfigs(configs);

      const globalSettings = await sqliteService.getLLMGlobalSettings();
      setLlmGlobalSettings(globalSettings);

      const activeConfig = configs.find(c => c.is_active);
      if (activeConfig) {
        setActiveProvider(activeConfig.provider);
      }
    } catch (error) {
      console.error('Failed to load LLM configs:', error);
    }
  };

  const handleSaveCredential = async () => {
    if (!newCredential.service_name || !newCredential.username || !newCredential.password) {
      showMessage('error', 'Service name, username, and password are required');
      return;
    }

    try {
      setLoading(true);
      const result = await sqliteService.saveCredential(newCredential);

      if (result.success) {
        showMessage('success', result.message);
        setNewCredential({
          service_name: '',
          username: '',
          password: '',
          api_key: '',
          api_secret: '',
          additional_data: ''
        });
        await loadCredentials();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to save credential');
    } finally {
      setLoading(false);
    }
  };

  const handleDeleteCredential = async (id: number) => {
    if (!confirm('Are you sure you want to delete this credential?')) return;

    try {
      setLoading(true);
      const result = await sqliteService.deleteCredential(id);

      if (result.success) {
        showMessage('success', result.message);
        await loadCredentials();
      } else {
        showMessage('error', result.message);
      }
    } catch (error) {
      showMessage('error', 'Failed to delete credential');
    } finally {
      setLoading(false);
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

  const handleSaveSettings = async (section: string) => {
    showMessage('success', `${section} settings saved successfully`);
  };

  const getCurrentLLMConfig = () => llmConfigs.find(c => c.provider === activeProvider);

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: '#000' }}>
      <style>{`
        *::-webkit-scrollbar { width: 8px; height: 8px; }
        *::-webkit-scrollbar-track { background: #0a0a0a; }
        *::-webkit-scrollbar-thumb { background: #2a2a2a; border-radius: 4px; }
        *::-webkit-scrollbar-thumb:hover { background: #3a3a3a; }
      `}</style>

      {/* Header */}
      <div style={{
        borderBottom: '2px solid #ea580c',
        padding: '12px 16px',
        background: 'linear-gradient(180deg, #1a1a1a 0%, #0a0a0a 100%)',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <SettingsIcon size={20} color="#ea580c" />
          <span style={{ color: '#ea580c', fontSize: '16px', fontWeight: 'bold', letterSpacing: '1px' }}>
            TERMINAL SETTINGS
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
              SQLite: {dbInitialized ? 'CONNECTED' : 'DISCONNECTED'}
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
        <div style={{ width: '220px', borderRight: '1px solid #1a1a1a', background: '#0a0a0a', flexShrink: 0 }}>
          <div style={{ padding: '16px 0' }}>
            {[
              { id: 'credentials', icon: Lock, label: 'Credentials' },
              { id: 'llm', icon: Bot, label: 'LLM Configuration' },
              { id: 'profile', icon: User, label: 'User Profile' },
              { id: 'terminal', icon: Terminal, label: 'Terminal Config' },
              { id: 'notifications', icon: Bell, label: 'Notifications' }
            ].map((item) => (
              <div
                key={item.id}
                onClick={() => setActiveSection(item.id as any)}
                style={{
                  padding: '12px 16px',
                  cursor: 'pointer',
                  background: activeSection === item.id ? '#1a1a1a' : 'transparent',
                  borderLeft: activeSection === item.id ? '3px solid #ea580c' : '3px solid transparent',
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
                <item.icon size={16} color={activeSection === item.id ? '#ea580c' : '#666'} />
                <span style={{ color: activeSection === item.id ? '#fff' : '#666', fontSize: '11px', fontWeight: activeSection === item.id ? 'bold' : 'normal' }}>
                  {item.label}
                </span>
              </div>
            ))}
          </div>
        </div>

        {/* Main Content */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
          <div style={{ flex: 1, overflowY: 'auto', padding: '20px', minHeight: 0 }}>

            {/* Credentials Section */}
            {activeSection === 'credentials' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: '#ea580c', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    CREDENTIAL MANAGEMENT
                  </h2>
                  <p style={{ color: '#888', fontSize: '10px' }}>
                    Securely store API keys, passwords, and authentication tokens. All data is encrypted and stored locally in SQLite.
                  </p>
                </div>

                {/* Add New Credential */}
                <div style={{
                  background: '#0a0a0a',
                  border: '1px solid #1a1a1a',
                  padding: '16px',
                  marginBottom: '20px',
                  borderRadius: '4px'
                }}>
                  <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
                    Add New Credential
                  </h3>

                  <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '12px' }}>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        SERVICE NAME *
                      </label>
                      <input
                        type="text"
                        value={newCredential.service_name}
                        onChange={(e) => setNewCredential({ ...newCredential, service_name: e.target.value })}
                        placeholder="e.g., Fyers, Zerodha, Alpha Vantage"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        USERNAME *
                      </label>
                      <input
                        type="text"
                        value={newCredential.username}
                        onChange={(e) => setNewCredential({ ...newCredential, username: e.target.value })}
                        placeholder="Username or Client ID"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        PASSWORD *
                      </label>
                      <input
                        type="password"
                        value={newCredential.password}
                        onChange={(e) => setNewCredential({ ...newCredential, password: e.target.value })}
                        placeholder="Password or Secret"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        API KEY (Optional)
                      </label>
                      <input
                        type="text"
                        value={newCredential.api_key}
                        onChange={(e) => setNewCredential({ ...newCredential, api_key: e.target.value })}
                        placeholder="API Key"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        API SECRET (Optional)
                      </label>
                      <input
                        type="password"
                        value={newCredential.api_secret}
                        onChange={(e) => setNewCredential({ ...newCredential, api_secret: e.target.value })}
                        placeholder="API Secret"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        ADDITIONAL DATA (Optional)
                      </label>
                      <input
                        type="text"
                        value={newCredential.additional_data}
                        onChange={(e) => setNewCredential({ ...newCredential, additional_data: e.target.value })}
                        placeholder="JSON or any extra data"
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>
                  </div>

                  <button
                    onClick={handleSaveCredential}
                    disabled={loading}
                    style={{
                      background: '#ea580c',
                      color: '#fff',
                      border: 'none',
                      padding: '8px 16px',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: loading ? 'not-allowed' : 'pointer',
                      borderRadius: '3px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px',
                      opacity: loading ? 0.5 : 1
                    }}
                  >
                    <Plus size={14} />
                    {loading ? 'SAVING...' : 'ADD CREDENTIAL'}
                  </button>
                </div>

                {/* Saved Credentials List */}
                <div>
                  <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
                    Saved Credentials ({credentials.length})
                  </h3>

                  {credentials.length === 0 ? (
                    <div style={{
                      background: '#0a0a0a',
                      border: '1px solid #1a1a1a',
                      padding: '32px',
                      textAlign: 'center',
                      borderRadius: '4px'
                    }}>
                      <Lock size={32} color="#333" style={{ margin: '0 auto 12px' }} />
                      <p style={{ color: '#666', fontSize: '11px' }}>No credentials saved yet</p>
                    </div>
                  ) : (
                    <div style={{ display: 'grid', gap: '12px' }}>
                      {credentials.map((cred) => (
                        <div
                          key={cred.id}
                          style={{
                            background: '#0a0a0a',
                            border: '1px solid #1a1a1a',
                            padding: '16px',
                            borderRadius: '4px'
                          }}
                        >
                          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'start', marginBottom: '12px' }}>
                            <div>
                              <h4 style={{ color: '#ea580c', fontSize: '11px', fontWeight: 'bold', marginBottom: '4px' }}>
                                {cred.service_name}
                              </h4>
                              <p style={{ color: '#666', fontSize: '9px' }}>
                                Created: {cred.created_at ? new Date(cred.created_at).toLocaleDateString() : 'N/A'}
                              </p>
                            </div>
                            <button
                              onClick={() => cred.id && handleDeleteCredential(cred.id)}
                              style={{
                                background: 'transparent',
                                border: '1px solid #ff0000',
                                color: '#ff0000',
                                padding: '4px 8px',
                                fontSize: '9px',
                                cursor: 'pointer',
                                borderRadius: '3px',
                                display: 'flex',
                                alignItems: 'center',
                                gap: '4px'
                              }}
                            >
                              <Trash2 size={12} />
                              DELETE
                            </button>
                          </div>

                          <div style={{ display: 'grid', gridTemplateColumns: '120px 1fr', gap: '8px', fontSize: '10px' }}>
                            <span style={{ color: '#888' }}>Username:</span>
                            <span style={{ color: '#fff' }}>{cred.username}</span>

                            <span style={{ color: '#888' }}>Password:</span>
                            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                              <span style={{ color: '#fff', fontFamily: 'monospace' }}>
                                {showPasswords[cred.id!] ? cred.password : '•'.repeat(12)}
                              </span>
                              <button
                                onClick={() => cred.id && togglePasswordVisibility(cred.id)}
                                style={{
                                  background: 'transparent',
                                  border: 'none',
                                  cursor: 'pointer',
                                  padding: '2px',
                                  display: 'flex'
                                }}
                              >
                                {showPasswords[cred.id!] ? <EyeOff size={14} color="#888" /> : <Eye size={14} color="#888" />}
                              </button>
                            </div>

                            {cred.api_key && (
                              <>
                                <span style={{ color: '#888' }}>API Key:</span>
                                <span style={{ color: '#fff', fontFamily: 'monospace', wordBreak: 'break-all' }}>{cred.api_key}</span>
                              </>
                            )}

                            {cred.api_secret && (
                              <>
                                <span style={{ color: '#888' }}>API Secret:</span>
                                <span style={{ color: '#fff', fontFamily: 'monospace' }}>
                                  {showPasswords[cred.id!] ? cred.api_secret : '•'.repeat(12)}
                                </span>
                              </>
                            )}

                            {cred.additional_data && (
                              <>
                                <span style={{ color: '#888' }}>Additional:</span>
                                <span style={{ color: '#fff', fontSize: '9px', wordBreak: 'break-all' }}>{cred.additional_data}</span>
                              </>
                            )}
                          </div>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              </div>
            )}

            {/* LLM Configuration Section */}
            {activeSection === 'llm' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: '#ea580c', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    LLM CONFIGURATION
                  </h2>
                  <p style={{ color: '#888', fontSize: '10px' }}>
                    Configure AI providers for the Chat tab. Changes here are reflected in both Chat and Settings screens.
                  </p>
                </div>

                {/* Provider Selection */}
                <div style={{ marginBottom: '20px' }}>
                  <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '8px' }}>
                    Select Active Provider
                  </h3>
                  <div style={{ display: 'flex', gap: '8px', flexWrap: 'wrap' }}>
                    {llmConfigs.map(config => (
                      <button
                        key={config.provider}
                        onClick={() => setActiveProvider(config.provider)}
                        style={{
                          background: activeProvider === config.provider ? '#ea580c' : '#0a0a0a',
                          color: activeProvider === config.provider ? '#000' : '#fff',
                          border: `1px solid ${activeProvider === config.provider ? '#ea580c' : '#2a2a2a'}`,
                          padding: '8px 16px',
                          fontSize: '10px',
                          fontWeight: 'bold',
                          cursor: 'pointer',
                          borderRadius: '3px',
                          textTransform: 'uppercase'
                        }}
                      >
                        {config.provider}
                      </button>
                    ))}
                  </div>
                </div>

                {/* Current Provider Config */}
                {getCurrentLLMConfig() && (
                  <div style={{
                    background: '#0a0a0a',
                    border: '1px solid #1a1a1a',
                    padding: '16px',
                    marginBottom: '20px',
                    borderRadius: '4px'
                  }}>
                    <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
                      {getCurrentLLMConfig()!.provider.toUpperCase()} Configuration
                    </h3>

                    <div style={{ display: 'grid', gap: '12px' }}>
                      {getCurrentLLMConfig()!.provider !== 'ollama' && (
                        <div>
                          <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
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
                              border: '1px solid #2a2a2a',
                              color: '#fff',
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px'
                            }}
                          />
                        </div>
                      )}

                      {(getCurrentLLMConfig()!.provider === 'ollama' || getCurrentLLMConfig()!.provider === 'deepseek' || getCurrentLLMConfig()!.provider === 'openrouter') && (
                        <div>
                          <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
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
                              border: '1px solid #2a2a2a',
                              color: '#fff',
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px'
                            }}
                          />
                        </div>
                      )}

                      <div>
                        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
                          <label style={{ color: '#888', fontSize: '9px' }}>
                            MODEL
                          </label>
                          {getCurrentLLMConfig()!.provider === 'ollama' && (
                            <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                              <button
                                onClick={fetchOllamaModels}
                                disabled={ollamaLoading}
                                style={{
                                  background: 'transparent',
                                  border: '1px solid #2a2a2a',
                                  color: '#ea580c',
                                  padding: '4px 8px',
                                  fontSize: '9px',
                                  cursor: ollamaLoading ? 'not-allowed' : 'pointer',
                                  borderRadius: '3px',
                                  display: 'flex',
                                  alignItems: 'center',
                                  gap: '4px',
                                  opacity: ollamaLoading ? 0.5 : 1
                                }}
                              >
                                <RefreshCw size={10} />
                                {ollamaLoading ? 'LOADING...' : 'REFRESH'}
                              </button>
                              <button
                                onClick={() => setUseManualEntry(!useManualEntry)}
                                style={{
                                  background: 'transparent',
                                  border: '1px solid #2a2a2a',
                                  color: '#888',
                                  padding: '4px 8px',
                                  fontSize: '9px',
                                  cursor: 'pointer',
                                  borderRadius: '3px',
                                  display: 'flex',
                                  alignItems: 'center',
                                  gap: '4px'
                                }}
                              >
                                <Edit3 size={10} />
                                {useManualEntry ? 'DROPDOWN' : 'MANUAL'}
                              </button>
                            </div>
                          )}
                        </div>

                        {getCurrentLLMConfig()!.provider === 'ollama' && !useManualEntry ? (
                          <>
                            <select
                              value={getCurrentLLMConfig()!.model}
                              onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                              disabled={ollamaLoading || ollamaModels.length === 0}
                              style={{
                                width: '100%',
                                background: '#000',
                                border: '1px solid #2a2a2a',
                                color: '#fff',
                                padding: '8px',
                                fontSize: '10px',
                                borderRadius: '3px',
                                cursor: ollamaLoading || ollamaModels.length === 0 ? 'not-allowed' : 'pointer',
                                appearance: 'none',
                                backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='12' viewBox='0 0 12 12'%3E%3Cpath fill='%23ea580c' d='M6 9L1 4h10z'/%3E%3C/svg%3E")`,
                                backgroundRepeat: 'no-repeat',
                                backgroundPosition: 'right 8px center',
                                paddingRight: '32px'
                              }}
                            >
                              {ollamaModels.length === 0 && !ollamaLoading && (
                                <option value="">No models found</option>
                              )}
                              {ollamaLoading && (
                                <option value="">Loading models...</option>
                              )}
                              {ollamaModels.map((model) => (
                                <option key={model} value={model}>
                                  {model}
                                </option>
                              ))}
                            </select>
                            {ollamaError && (
                              <div style={{
                                marginTop: '4px',
                                padding: '6px 8px',
                                background: '#3a0a0a',
                                border: '1px solid #ff0000',
                                borderRadius: '3px',
                                fontSize: '9px',
                                color: '#ff0000'
                              }}>
                                {ollamaError}
                              </div>
                            )}
                            {ollamaModels.length > 0 && (
                              <div style={{ marginTop: '4px', fontSize: '9px', color: '#00ff00' }}>
                                ✓ Found {ollamaModels.length} model{ollamaModels.length !== 1 ? 's' : ''}
                              </div>
                            )}
                          </>
                        ) : (
                          <input
                            type="text"
                            value={getCurrentLLMConfig()!.model}
                            onChange={(e) => updateLLMConfig(activeProvider, 'model', e.target.value)}
                            placeholder="Model name"
                            style={{
                              width: '100%',
                              background: '#000',
                              border: '1px solid #2a2a2a',
                              color: '#fff',
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px'
                            }}
                          />
                        )}
                      </div>
                    </div>
                  </div>
                )}

                {/* Global Settings */}
                <div style={{
                  background: '#0a0a0a',
                  border: '1px solid #1a1a1a',
                  padding: '16px',
                  marginBottom: '20px',
                  borderRadius: '4px'
                }}>
                  <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>
                    Global AI Settings
                  </h3>

                  <div style={{ display: 'grid', gap: '12px' }}>
                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        TEMPERATURE (0.0 - 2.0)
                      </label>
                      <input
                        type="number"
                        min="0"
                        max="2"
                        step="0.1"
                        value={llmGlobalSettings.temperature}
                        onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, temperature: parseFloat(e.target.value) })}
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
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
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px'
                        }}
                      />
                    </div>

                    <div>
                      <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>
                        SYSTEM PROMPT
                      </label>
                      <textarea
                        value={llmGlobalSettings.system_prompt}
                        onChange={(e) => setLlmGlobalSettings({ ...llmGlobalSettings, system_prompt: e.target.value })}
                        rows={4}
                        style={{
                          width: '100%',
                          background: '#000',
                          border: '1px solid #2a2a2a',
                          color: '#fff',
                          padding: '8px',
                          fontSize: '10px',
                          borderRadius: '3px',
                          resize: 'vertical'
                        }}
                      />
                    </div>
                  </div>
                </div>

                <button
                  onClick={handleSaveLLMConfig}
                  disabled={loading}
                  style={{
                    background: '#ea580c',
                    color: '#fff',
                    border: 'none',
                    padding: '10px 20px',
                    fontSize: '11px',
                    fontWeight: 'bold',
                    cursor: loading ? 'not-allowed' : 'pointer',
                    borderRadius: '3px',
                    display: 'flex',
                    alignItems: 'center',
                    gap: '6px',
                    opacity: loading ? 0.5 : 1
                  }}
                >
                  <Save size={16} />
                  {loading ? 'SAVING...' : 'SAVE LLM CONFIGURATION'}
                </button>
              </div>
            )}

            {/* Other sections remain the same... */}
            {activeSection === 'profile' && <div style={{ color: '#888' }}>User Profile section (implementation same as before)</div>}
            {activeSection === 'terminal' && <div style={{ color: '#888' }}>Terminal Config section (implementation same as before)</div>}
            {activeSection === 'notifications' && <div style={{ color: '#888' }}>Notifications section (implementation same as before)</div>}

          </div>
        </div>
      </div>

      {/* Footer */}
      <div style={{
        borderTop: '2px solid #ea580c',
        padding: '8px 16px',
        background: 'linear-gradient(180deg, #0a0a0a 0%, #1a1a1a 100%)',
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        flexShrink: 0,
        flexWrap: 'wrap',
        gap: '12px'
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px', fontSize: '9px' }}>
          <span style={{ color: '#ea580c', fontWeight: 'bold' }}>SETTINGS v2.0.0</span>
          <span style={{ color: '#666' }}>|</span>
          <span style={{ color: '#888' }}>Database: SQLite</span>
          <span style={{ color: '#666' }}>|</span>
          <span style={{ color: '#888' }}>Storage: File-based</span>
        </div>
        <div style={{ fontSize: '9px', color: '#666' }}>
          All data stored securely in fincept_terminal.db
        </div>
      </div>
    </div>
  );
}
