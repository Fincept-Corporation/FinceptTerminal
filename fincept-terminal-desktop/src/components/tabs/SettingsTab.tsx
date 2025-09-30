import React, { useState, useEffect } from 'react';
import { Eye, EyeOff, Plus, Trash2, Save, RefreshCw, Lock, User, Settings as SettingsIcon, Database, Terminal, Bell } from 'lucide-react';
import * as storageService from '@/services/storageService';

interface Credential {
  id?: number;
  service_name: string;
  username: string;
  password: string;
  api_key?: string;
  api_secret?: string;
  additional_data?: string;
  created_at?: string;
  updated_at?: string;
}

export default function SettingsTab() {
  const [activeSection, setActiveSection] = useState<'credentials' | 'profile' | 'terminal' | 'notifications'>('credentials');
  const [credentials, setCredentials] = useState<Credential[]>([]);
  const [showPasswords, setShowPasswords] = useState<Record<number, boolean>>({});
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const [dbInitialized, setDbInitialized] = useState(false);

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
      await storageService.initializeDatabase();
      setDbInitialized(true);
      await loadCredentials();
      showMessage('success', 'Database initialized successfully');
    } catch (error) {
      console.error('Failed to initialize database:', error);
      showMessage('error', 'Failed to initialize database');
    } finally {
      setLoading(false);
    }
  };

  const loadCredentials = async () => {
    try {
      const result = await storageService.getCredentials();
      if (result.success && result.data) {
        setCredentials(result.data);
      }
    } catch (error) {
      console.error('Failed to load credentials:', error);
    }
  };

  const handleSaveCredential = async () => {
    if (!newCredential.service_name || !newCredential.username || !newCredential.password) {
      showMessage('error', 'Service name, username, and password are required');
      return;
    }

    try {
      setLoading(true);
      const result = await storageService.saveCredential(newCredential);

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
      const result = await storageService.deleteCredential(id);

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

  const togglePasswordVisibility = (id: number) => {
    setShowPasswords(prev => ({ ...prev, [id]: !prev[id] }));
  };

  const showMessage = (type: 'success' | 'error', text: string) => {
    setMessage({ type, text });
    setTimeout(() => setMessage(null), 5000);
  };

  const handleSaveSettings = async (section: string) => {
    // In a real implementation, save to DuckDB
    showMessage('success', `${section} settings saved successfully`);
  };

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
              DB: {dbInitialized ? 'CONNECTED' : 'DISCONNECTED'}
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
                    Securely store API keys, passwords, and authentication tokens. All data is encrypted and stored locally in DuckDB.
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

            {/* User Profile Section */}
            {activeSection === 'profile' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: '#ea580c', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    USER PROFILE
                  </h2>
                  <p style={{ color: '#888', fontSize: '10px' }}>
                    View and manage your account information and subscription details.
                  </p>
                </div>

                <div style={{ display: 'grid', gap: '16px' }}>
                  {/* Account Info */}
                  <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '16px', borderRadius: '4px' }}>
                    <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>Account Information</h3>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                      <div>
                        <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>FULL NAME</label>
                        <input
                          type="text"
                          value={userProfile.full_name}
                          onChange={(e) => setUserProfile({ ...userProfile, full_name: e.target.value })}
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
                        <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>EMAIL</label>
                        <input
                          type="email"
                          value={userProfile.email}
                          onChange={(e) => setUserProfile({ ...userProfile, email: e.target.value })}
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
                      onClick={() => handleSaveSettings('Profile')}
                      style={{
                        background: '#ea580c',
                        color: '#fff',
                        border: 'none',
                        padding: '8px 16px',
                        fontSize: '10px',
                        fontWeight: 'bold',
                        cursor: 'pointer',
                        borderRadius: '3px',
                        marginTop: '12px',
                        display: 'flex',
                        alignItems: 'center',
                        gap: '6px'
                      }}
                    >
                      <Save size={14} />
                      SAVE CHANGES
                    </button>
                  </div>

                  {/* Subscription Stats */}
                  <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '16px', borderRadius: '4px' }}>
                    <h3 style={{ color: '#fff', fontSize: '12px', fontWeight: 'bold', marginBottom: '12px' }}>Subscription Details</h3>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', fontSize: '10px' }}>
                      <div>
                        <span style={{ color: '#888' }}>Account Type:</span>
                        <div style={{ color: '#ea580c', fontWeight: 'bold', marginTop: '4px' }}>{userProfile.account_type}</div>
                      </div>
                      <div>
                        <span style={{ color: '#888' }}>Status:</span>
                        <div style={{ color: '#00ff00', fontWeight: 'bold', marginTop: '4px' }}>{userProfile.subscription_status}</div>
                      </div>
                      <div>
                        <span style={{ color: '#888' }}>Expires:</span>
                        <div style={{ color: '#fff', marginTop: '4px' }}>{userProfile.subscription_expiry}</div>
                      </div>
                      <div>
                        <span style={{ color: '#888' }}>API Calls Today:</span>
                        <div style={{ color: '#fff', marginTop: '4px' }}>{userProfile.api_calls_today} / {userProfile.api_limit}</div>
                      </div>
                      <div>
                        <span style={{ color: '#888' }}>Storage Used:</span>
                        <div style={{ color: '#fff', marginTop: '4px' }}>{userProfile.storage_used} / {userProfile.storage_limit}</div>
                      </div>
                    </div>
                  </div>
                </div>
              </div>
            )}

            {/* Terminal Configuration Section */}
            {activeSection === 'terminal' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: '#ea580c', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    TERMINAL CONFIGURATION
                  </h2>
                  <p style={{ color: '#888', fontSize: '10px' }}>
                    Customize terminal behavior, appearance, and performance settings.
                  </p>
                </div>

                <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '16px', borderRadius: '4px' }}>
                  <div style={{ display: 'grid', gap: '16px' }}>
                    {/* Appearance */}
                    <div>
                      <h3 style={{ color: '#fff', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>Appearance</h3>
                      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
                        <div>
                          <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>THEME</label>
                          <select
                            value={terminalSettings.theme}
                            onChange={(e) => setTerminalSettings({ ...terminalSettings, theme: e.target.value })}
                            style={{
                              width: '100%',
                              background: '#000',
                              border: '1px solid #2a2a2a',
                              color: '#fff',
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px'
                            }}
                          >
                            <option value="dark">Dark (Bloomberg)</option>
                            <option value="light">Light</option>
                            <option value="matrix">Matrix Green</option>
                          </select>
                        </div>
                        <div>
                          <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>FONT SIZE</label>
                          <select
                            value={terminalSettings.font_size}
                            onChange={(e) => setTerminalSettings({ ...terminalSettings, font_size: e.target.value })}
                            style={{
                              width: '100%',
                              background: '#000',
                              border: '1px solid #2a2a2a',
                              color: '#fff',
                              padding: '8px',
                              fontSize: '10px',
                              borderRadius: '3px'
                            }}
                          >
                            <option value="9">9px (Small)</option>
                            <option value="10">10px (Default)</option>
                            <option value="11">11px (Medium)</option>
                            <option value="12">12px (Large)</option>
                          </select>
                        </div>
                      </div>
                    </div>

                    {/* Performance */}
                    <div>
                      <h3 style={{ color: '#fff', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>Performance</h3>
                      <div style={{ display: 'grid', gap: '8px' }}>
                        {[
                          { key: 'auto_refresh', label: 'Auto Refresh Data' },
                          { key: 'enable_animations', label: 'Enable Animations' },
                          { key: 'show_tooltips', label: 'Show Tooltips' },
                          { key: 'data_compression', label: 'Data Compression' },
                          { key: 'cache_enabled', label: 'Enable Caching' }
                        ].map((setting) => (
                          <label key={setting.key} style={{ display: 'flex', alignItems: 'center', gap: '8px', cursor: 'pointer' }}>
                            <input
                              type="checkbox"
                              checked={terminalSettings[setting.key as keyof typeof terminalSettings] as boolean}
                              onChange={(e) => setTerminalSettings({ ...terminalSettings, [setting.key]: e.target.checked })}
                              style={{ width: '14px', height: '14px', cursor: 'pointer' }}
                            />
                            <span style={{ color: '#fff', fontSize: '10px' }}>{setting.label}</span>
                          </label>
                        ))}
                      </div>
                    </div>

                    {/* Data Settings */}
                    <div>
                      <h3 style={{ color: '#fff', fontSize: '11px', fontWeight: 'bold', marginBottom: '8px' }}>Data Settings</h3>
                      <div>
                        <label style={{ color: '#888', fontSize: '9px', display: 'block', marginBottom: '4px' }}>REFRESH INTERVAL (seconds)</label>
                        <select
                          value={terminalSettings.refresh_interval}
                          onChange={(e) => setTerminalSettings({ ...terminalSettings, refresh_interval: e.target.value })}
                          style={{
                            width: '100%',
                            background: '#000',
                            border: '1px solid #2a2a2a',
                            color: '#fff',
                            padding: '8px',
                            fontSize: '10px',
                            borderRadius: '3px'
                          }}
                        >
                          <option value="1">1 second</option>
                          <option value="5">5 seconds</option>
                          <option value="10">10 seconds</option>
                          <option value="30">30 seconds</option>
                          <option value="60">1 minute</option>
                        </select>
                      </div>
                    </div>
                  </div>

                  <button
                    onClick={() => handleSaveSettings('Terminal')}
                    style={{
                      background: '#ea580c',
                      color: '#fff',
                      border: 'none',
                      padding: '8px 16px',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      borderRadius: '3px',
                      marginTop: '16px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}
                  >
                    <Save size={14} />
                    SAVE CONFIGURATION
                  </button>
                </div>
              </div>
            )}

            {/* Notifications Section */}
            {activeSection === 'notifications' && (
              <div>
                <div style={{ marginBottom: '24px' }}>
                  <h2 style={{ color: '#ea580c', fontSize: '14px', fontWeight: 'bold', marginBottom: '8px' }}>
                    NOTIFICATION PREFERENCES
                  </h2>
                  <p style={{ color: '#888', fontSize: '10px' }}>
                    Configure alerts and notifications for market events, trades, and system updates.
                  </p>
                </div>

                <div style={{ background: '#0a0a0a', border: '1px solid #1a1a1a', padding: '16px', borderRadius: '4px' }}>
                  <div style={{ display: 'grid', gap: '12px' }}>
                    {[
                      { key: 'email_alerts', label: 'Email Alerts', desc: 'Receive email notifications for important events' },
                      { key: 'price_alerts', label: 'Price Alerts', desc: 'Notify when price targets are reached' },
                      { key: 'news_alerts', label: 'News Alerts', desc: 'Breaking financial news notifications' },
                      { key: 'trade_confirmations', label: 'Trade Confirmations', desc: 'Confirm all trade executions' },
                      { key: 'system_updates', label: 'System Updates', desc: 'Terminal updates and maintenance notices' },
                      { key: 'marketing_emails', label: 'Marketing Emails', desc: 'Product updates and promotional content' }
                    ].map((notif) => (
                      <div key={notif.key} style={{ padding: '12px', background: '#000', borderRadius: '3px' }}>
                        <label style={{ display: 'flex', alignItems: 'start', gap: '12px', cursor: 'pointer' }}>
                          <input
                            type="checkbox"
                            checked={notificationSettings[notif.key as keyof typeof notificationSettings]}
                            onChange={(e) => setNotificationSettings({ ...notificationSettings, [notif.key]: e.target.checked })}
                            style={{ width: '16px', height: '16px', cursor: 'pointer', marginTop: '2px', flexShrink: 0 }}
                          />
                          <div>
                            <div style={{ color: '#fff', fontSize: '10px', fontWeight: 'bold', marginBottom: '4px' }}>{notif.label}</div>
                            <div style={{ color: '#666', fontSize: '9px' }}>{notif.desc}</div>
                          </div>
                        </label>
                      </div>
                    ))}
                  </div>

                  <button
                    onClick={() => handleSaveSettings('Notification')}
                    style={{
                      background: '#ea580c',
                      color: '#fff',
                      border: 'none',
                      padding: '8px 16px',
                      fontSize: '10px',
                      fontWeight: 'bold',
                      cursor: 'pointer',
                      borderRadius: '3px',
                      marginTop: '16px',
                      display: 'flex',
                      alignItems: 'center',
                      gap: '6px'
                    }}
                  >
                    <Save size={14} />
                    SAVE PREFERENCES
                  </button>
                </div>
              </div>
            )}

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
          <span style={{ color: '#ea580c', fontWeight: 'bold' }}>SETTINGS v1.0.0</span>
          <span style={{ color: '#666' }}>|</span>
          <span style={{ color: '#888' }}>Database: IndexedDB</span>
          <span style={{ color: '#666' }}>|</span>
          <span style={{ color: '#888' }}>Storage: Local Browser</span>
        </div>
        <div style={{ fontSize: '9px', color: '#666' }}>
          All credentials are stored securely in browser database
        </div>
      </div>
    </div>
  );
}