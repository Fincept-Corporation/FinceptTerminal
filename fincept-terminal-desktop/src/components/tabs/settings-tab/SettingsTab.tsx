import React, { useState, useEffect } from 'react';
import { Settings as SettingsIcon, Lock, Bot, Database, Terminal, Activity, Globe, TrendingUp, Briefcase, HardDrive } from 'lucide-react';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { useTimezone } from '@/contexts/TimezoneContext';
import { useTranslation } from 'react-i18next';
import { useAuth } from '@/contexts/AuthContext';
import { TabFooter } from '@/components/common/TabFooter';

// External panels from settings folder
import { DataSourcesPanel } from '@/components/settings/DataSourcesPanel';
import { BacktestingProvidersPanel } from '@/components/settings/BacktestingProvidersPanel';
import { LanguageSelector } from '@/components/settings/LanguageSelector';
import { TerminalConfigPanel } from '@/components/settings/TerminalConfigPanel';
import { PolymarketCredentialsPanel } from '@/components/settings/PolymarketCredentialsPanel';
import { MasterContractManager } from '@/components/tabs/equity-trading/components';

// Section components
import {
  CredentialsSection,
  LLMConfigSection,
  TerminalAppearanceSection,
  StorageCacheSection,
} from './sections';

// Hooks and types
import { useSettingsData } from './hooks';
import type { SettingsSection, SettingsColors } from './types';

export default function SettingsTab() {
  const { t } = useTranslation('settings');
  const { session } = useAuth();
  const { theme, updateTheme, resetTheme, colors } = useTerminalTheme();
  const { defaultTimezone, setDefaultTimezone, options: timezoneOptions } = useTimezone();
  const [activeSection, setActiveSection] = useState<SettingsSection>('credentials');

  const {
    apiKeys,
    setApiKeys,
    loading,
    setLoading,
    message,
    setMessage,
    showMessage,
    dbInitialized,
    llmConfigs,
    setLlmConfigs,
    llmGlobalSettings,
    setLlmGlobalSettings,
    modelConfigs,
    keyMappings,
    setKeyMappings,
    checkAndLoadData,
    loadLLMConfigs,
  } = useSettingsData();

  useEffect(() => {
    checkAndLoadData(session?.api_key ?? undefined);
  }, []);

  const settingsColors: SettingsColors = {
    background: colors.background,
    panel: colors.panel,
    primary: colors.primary,
    secondary: colors.secondary,
    text: colors.text,
    textMuted: colors.textMuted,
    success: colors.success,
    alert: colors.alert,
    warning: colors.warning,
    accent: colors.accent,
  };

  const sidebarItems = [
    { id: 'credentials' as SettingsSection, icon: Lock, label: t('sidebar.credentials') },
    { id: 'polymarket' as SettingsSection, icon: TrendingUp, label: t('sidebar.polymarket') },
    { id: 'llm' as SettingsSection, icon: Bot, label: t('sidebar.llm') },
    { id: 'dataConnections' as SettingsSection, icon: Database, label: t('sidebar.dataSources') },
    { id: 'backtesting' as SettingsSection, icon: Activity, label: t('sidebar.backtesting') },
    { id: 'stockSymbols' as SettingsSection, icon: Briefcase, label: 'Stock Symbols' },
    { id: 'terminalConfig' as SettingsSection, icon: SettingsIcon, label: t('sidebar.tabLayout') },
    { id: 'terminal' as SettingsSection, icon: Terminal, label: t('sidebar.appearance') },
    { id: 'language' as SettingsSection, icon: Globe, label: t('sidebar.language') },
    { id: 'storage' as SettingsSection, icon: HardDrive, label: 'Storage & Cache' },
  ];

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
            {sidebarItems.map((item) => (
              <div
                key={item.id}
                onClick={() => setActiveSection(item.id)}
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
            {/* Credentials Section */}
            {activeSection === 'credentials' && (
              <CredentialsSection
                colors={settingsColors}
                apiKeys={apiKeys}
                setApiKeys={setApiKeys}
                loading={loading}
                setLoading={setLoading}
                showMessage={showMessage}
              />
            )}

            {/* Polymarket API Section */}
            {activeSection === 'polymarket' && (
              <PolymarketCredentialsPanel />
            )}

            {/* LLM Configuration Section */}
            {activeSection === 'llm' && (
              <LLMConfigSection
                colors={settingsColors}
                llmConfigs={llmConfigs}
                setLlmConfigs={setLlmConfigs}
                llmGlobalSettings={llmGlobalSettings}
                setLlmGlobalSettings={setLlmGlobalSettings}
                modelConfigs={modelConfigs}
                loading={loading}
                setLoading={setLoading}
                showMessage={showMessage}
                loadLLMConfigs={loadLLMConfigs}
                sessionApiKey={session?.api_key ?? undefined}
              />
            )}

            {/* Terminal Tab Configuration Section */}
            {activeSection === 'terminalConfig' && (
              <TerminalConfigPanel />
            )}

            {/* Terminal Appearance Section */}
            {activeSection === 'terminal' && (
              <TerminalAppearanceSection
                colors={settingsColors}
                theme={theme}
                updateTheme={updateTheme}
                resetTheme={resetTheme}
                showMessage={showMessage}
                keyMappings={keyMappings}
                setKeyMappings={setKeyMappings}
                defaultTimezone={defaultTimezone}
                setDefaultTimezone={setDefaultTimezone}
                timezoneOptions={timezoneOptions}
              />
            )}

            {/* Data Sources Section */}
            {activeSection === 'dataConnections' && (
              <DataSourcesPanel colors={settingsColors} />
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

            {/* Stock Symbols Section */}
            {activeSection === 'stockSymbols' && (
              <div style={{ maxWidth: '600px' }}>
                <div style={{ marginBottom: '16px' }}>
                  <h2 style={{ fontSize: '16px', fontWeight: 700, color: colors.text, margin: 0 }}>
                    Stock Symbol Database
                  </h2>
                  <p style={{ fontSize: '11px', color: colors.textMuted, marginTop: '8px', lineHeight: 1.5 }}>
                    Download and manage broker symbol data for fast offline search.
                    Symbol data includes stocks, F&O, commodities, and currency derivatives.
                    Update regularly to get latest instruments and expiries.
                  </p>
                </div>
                <MasterContractManager
                  brokers={['angelone', 'fyers']}
                  onUpdate={(brokerId, result) => {
                    if (result.success) {
                      setMessage({
                        type: 'success',
                        text: `${brokerId.toUpperCase()}: Downloaded ${result.symbol_count.toLocaleString()} symbols`
                      });
                    } else {
                      setMessage({
                        type: 'error',
                        text: `${brokerId.toUpperCase()}: ${result.message}`
                      });
                    }
                    setTimeout(() => setMessage(null), 5000);
                  }}
                />
              </div>
            )}

            {/* Language Section */}
            {activeSection === 'language' && (
              <div style={{ padding: '20px' }}>
                <LanguageSelector />
              </div>
            )}

            {/* Storage & Cache Section */}
            {activeSection === 'storage' && (
              <StorageCacheSection
                colors={settingsColors}
                showMessage={showMessage}
              />
            )}
          </div>
        </div>
      </div>

      {/* Footer */}
      <TabFooter
        tabName="SETTINGS"
        leftInfo={[
          { label: t('footer.database'), color: colors.text },
          { label: t('footer.storage'), color: colors.text },
        ]}
        statusInfo={t('footer.secureStorage')}
        backgroundColor={colors.panel}
        borderColor={colors.primary}
      />
    </div>
  );
}
