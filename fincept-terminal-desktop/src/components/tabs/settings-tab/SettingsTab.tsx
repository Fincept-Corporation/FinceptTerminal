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

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
};

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
    { id: 'stockSymbols' as SettingsSection, icon: Briefcase, label: t('sidebar.stockSymbols') },
    { id: 'terminalConfig' as SettingsSection, icon: SettingsIcon, label: t('sidebar.tabLayout') },
    { id: 'terminal' as SettingsSection, icon: Terminal, label: t('sidebar.appearance') },
    { id: 'language' as SettingsSection, icon: Globe, label: t('sidebar.language') },
    { id: 'storage' as SettingsSection, icon: HardDrive, label: t('sidebar.storage') },
  ];

  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', backgroundColor: FINCEPT.DARK_BG, fontFamily: '"IBM Plex Mono", "Consolas", monospace' }}>
      <style>{`
        *::-webkit-scrollbar { width: 6px; height: 6px; }
        *::-webkit-scrollbar-track { background: ${FINCEPT.DARK_BG}; }
        *::-webkit-scrollbar-thumb { background: ${FINCEPT.BORDER}; border-radius: 3px; }
        *::-webkit-scrollbar-thumb:hover { background: ${FINCEPT.MUTED}; }
      `}</style>

      {/* Top Navigation Bar */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: `2px solid ${FINCEPT.ORANGE}`,
        padding: '8px 16px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        boxShadow: `0 2px 8px ${FINCEPT.ORANGE}20`,
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <SettingsIcon size={16} color={FINCEPT.ORANGE} />
          <span style={{
            fontSize: '11px',
            fontWeight: 700,
            color: FINCEPT.WHITE,
            letterSpacing: '0.5px',
            textTransform: 'uppercase'
          }}>
            {t('title')}
          </span>
        </div>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{
            padding: '2px 6px',
            backgroundColor: dbInitialized ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
            color: dbInitialized ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            display: 'flex',
            alignItems: 'center',
            gap: '4px'
          }}>
            <Database size={10} />
            <span style={{ textTransform: 'uppercase', letterSpacing: '0.5px' }}>
              {dbInitialized ? t('llm.connected') : t('llm.disconnected')}
            </span>
          </div>
        </div>
      </div>

      {/* Message Banner */}
      {message && (
        <div style={{
          padding: '6px 16px',
          backgroundColor: message.type === 'success' ? `${FINCEPT.GREEN}20` : `${FINCEPT.RED}20`,
          borderBottom: `1px solid ${message.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED}`,
          color: message.type === 'success' ? FINCEPT.GREEN : FINCEPT.RED,
          fontSize: '9px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          flexShrink: 0
        }}>
          {message.text}
        </div>
      )}

      {/* Main Layout */}
      <div style={{ flex: 1, display: 'flex', minHeight: 0, overflow: 'hidden' }}>
        {/* Left Panel - Sidebar Navigation */}
        <div style={{
          width: '280px',
          backgroundColor: FINCEPT.PANEL_BG,
          borderRight: `1px solid ${FINCEPT.BORDER}`,
          flexShrink: 0,
          overflowY: 'auto'
        }}>
          {/* Section Header */}
          <div style={{
            padding: '12px',
            backgroundColor: FINCEPT.HEADER_BG,
            borderBottom: `1px solid ${FINCEPT.BORDER}`
          }}>
            <span style={{
              fontSize: '9px',
              fontWeight: 700,
              color: FINCEPT.GRAY,
              letterSpacing: '0.5px',
              textTransform: 'uppercase'
            }}>
              SETTINGS MENU
            </span>
          </div>

          {/* Navigation Items */}
          <div style={{ padding: '4px 0' }}>
            {sidebarItems.map((item) => (
              <div
                key={item.id}
                onClick={() => setActiveSection(item.id)}
                style={{
                  padding: '10px 12px',
                  backgroundColor: activeSection === item.id ? `${FINCEPT.ORANGE}15` : 'transparent',
                  borderLeft: activeSection === item.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                  cursor: 'pointer',
                  transition: 'all 0.2s',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '10px'
                }}
                onMouseEnter={(e) => {
                  if (activeSection !== item.id) e.currentTarget.style.backgroundColor = FINCEPT.HOVER;
                }}
                onMouseLeave={(e) => {
                  if (activeSection !== item.id) e.currentTarget.style.backgroundColor = 'transparent';
                }}
              >
                <item.icon
                  size={12}
                  color={activeSection === item.id ? FINCEPT.ORANGE : FINCEPT.GRAY}
                />
                <span style={{
                  color: activeSection === item.id ? FINCEPT.WHITE : FINCEPT.GRAY,
                  fontSize: '10px',
                  fontWeight: activeSection === item.id ? 700 : 400,
                  letterSpacing: '0.3px',
                  textTransform: 'uppercase'
                }}>
                  {item.label}
                </span>
              </div>
            ))}
          </div>
        </div>

        {/* Center Content */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', minWidth: 0, overflow: 'hidden' }}>
          <div style={{ flex: 1, overflowY: 'auto', padding: '16px', minHeight: 0 }}>
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
              <div>
                <div style={{
                  marginBottom: '16px',
                  paddingBottom: '12px',
                  borderBottom: `1px solid ${FINCEPT.BORDER}`
                }}>
                  <h2 style={{
                    fontSize: '11px',
                    fontWeight: 700,
                    color: FINCEPT.WHITE,
                    margin: 0,
                    letterSpacing: '0.5px',
                    textTransform: 'uppercase'
                  }}>
                    {t('stockSymbols.title')}
                  </h2>
                  <p style={{
                    fontSize: '10px',
                    color: FINCEPT.GRAY,
                    marginTop: '8px',
                    lineHeight: 1.5
                  }}>
                    {t('stockSymbols.description')}
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
              <div>
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

      {/* Status Bar (Bottom) */}
      <div style={{
        backgroundColor: FINCEPT.HEADER_BG,
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        padding: '4px 16px',
        fontSize: '9px',
        color: FINCEPT.GRAY,
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        fontWeight: 700,
        letterSpacing: '0.5px',
        flexShrink: 0
      }}>
        <div style={{ display: 'flex', gap: '16px', alignItems: 'center' }}>
          <span style={{ textTransform: 'uppercase' }}>SETTINGS</span>
          <span style={{ color: FINCEPT.MUTED }}>|</span>
          <span style={{ textTransform: 'uppercase' }}>{t('footer.database')}</span>
          <span style={{ color: FINCEPT.MUTED }}>|</span>
          <span style={{ textTransform: 'uppercase' }}>{t('footer.storage')}</span>
        </div>
        <div style={{ textTransform: 'uppercase' }}>
          {t('footer.secureStorage')}
        </div>
      </div>
    </div>
  );
}
