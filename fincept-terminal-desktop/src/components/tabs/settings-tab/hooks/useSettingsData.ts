import { useState, useEffect } from 'react';
import { sqliteService, type LLMConfig, type LLMGlobalSettings, type LLMModelConfig, type ApiKeys } from '@/services/core/sqliteService';
import { getSetting } from '@/services/core/sqliteService';
import type { WSProviderConfig, SettingsMessage } from '../types';

export const DEFAULT_KEY_MAPPINGS = {
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

export function useSettingsData() {
  const [apiKeys, setApiKeys] = useState<ApiKeys>({});
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState<SettingsMessage | null>(null);
  const [dbInitialized, setDbInitialized] = useState(false);
  const [wsProviders, setWsProviders] = useState<WSProviderConfig[]>([]);
  const [llmConfigs, setLlmConfigs] = useState<LLMConfig[]>([]);
  const [llmGlobalSettings, setLlmGlobalSettings] = useState<LLMGlobalSettings>({
    temperature: 0.7,
    max_tokens: 2048,
    system_prompt: ''
  });
  const [modelConfigs, setModelConfigs] = useState<LLMModelConfig[]>([]);
  const [keyMappings, setKeyMappings] = useState<Record<string, string>>(DEFAULT_KEY_MAPPINGS);

  const showMessage = (type: 'success' | 'error', text: string) => {
    setMessage({ type, text });
    setTimeout(() => setMessage(null), 5000);
  };

  const loadApiKeys = async () => {
    try {
      const keys = await sqliteService.getAllApiKeys();
      setApiKeys(keys);
    } catch (error) {
      console.error('Failed to load API keys:', error);
    }
  };

  const loadWSProviders = async () => {
    try {
      const providers = await sqliteService.getWSProviderConfigs();
      setWsProviders(providers);
    } catch (error) {
      console.error('Failed to load WS providers:', error);
    }
  };

  const loadLLMConfigs = async (sessionApiKey?: string) => {
    try {
      let configs = await sqliteService.getLLMConfigs();

      if (configs.length === 0) {
        const userApiKey = sessionApiKey || '';
        const now = new Date().toISOString();
        const finceptConfig = {
          provider: 'fincept',
          api_key: userApiKey,
          base_url: 'https://api.fincept.in/research/llm',
          model: 'fincept-llm',
          is_active: true,
          created_at: now,
          updated_at: now,
        };
        await sqliteService.saveLLMConfig(finceptConfig);
        configs = await sqliteService.getLLMConfigs();
      } else if (sessionApiKey) {
        // Ensure the fincept config has the session API key populated
        const finceptConfig = configs.find(c => c.provider === 'fincept');
        if (finceptConfig && !finceptConfig.api_key) {
          finceptConfig.api_key = sessionApiKey;
          await sqliteService.saveLLMConfig(finceptConfig);
          configs = await sqliteService.getLLMConfigs();
        }
      }

      setLlmConfigs(configs);

      const globalSettings = await sqliteService.getLLMGlobalSettings();
      setLlmGlobalSettings(globalSettings);

      const modelCfgs = await sqliteService.getLLMModelConfigs();
      setModelConfigs(modelCfgs);

      return { configs, modelCfgs };
    } catch (error) {
      console.error('Failed to load LLM configs:', error);
      return { configs: [], modelCfgs: [] };
    }
  };

  const loadKeyMappings = async () => {
    const savedMappings = await getSetting('fincept_key_mappings');
    if (savedMappings) {
      try {
        setKeyMappings(JSON.parse(savedMappings));
      } catch (err) {
        console.error('Failed to load key mappings:', err);
      }
    }
  };

  const checkAndLoadData = async (sessionApiKey?: string) => {
    try {
      setLoading(true);
      const isReady = sqliteService.isReady();

      if (!isReady) {
        console.log('[SettingsTab] Database not initialized, initializing now...');
        await sqliteService.initialize();
      }

      const healthCheck = await sqliteService.healthCheck();
      if (!healthCheck.healthy) {
        throw new Error(healthCheck.message);
      }

      setDbInitialized(true);

      await loadApiKeys();
      await loadLLMConfigs(sessionApiKey);
      await loadWSProviders();
      await loadKeyMappings();

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

  return {
    apiKeys,
    setApiKeys,
    loading,
    setLoading,
    message,
    setMessage,
    showMessage,
    dbInitialized,
    wsProviders,
    setWsProviders,
    llmConfigs,
    setLlmConfigs,
    llmGlobalSettings,
    setLlmGlobalSettings,
    modelConfigs,
    setModelConfigs,
    keyMappings,
    setKeyMappings,
    loadApiKeys,
    loadWSProviders,
    loadLLMConfigs,
    checkAndLoadData,
  };
}
