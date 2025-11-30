// File: src/contexts/ProviderContext.tsx
// Global provider selection context for multi-provider trading

import React, { createContext, useContext, useState, useEffect, useCallback, ReactNode } from 'react';
import { getWebSocketManager } from '../services/websocket';
import type { ProviderConfig } from '../services/websocket/types';
import { getAvailableProviders } from '../services/websocket/adapters';

interface ProviderContextType {
  activeProvider: string;
  availableProviders: string[];
  setActiveProvider: (provider: string) => Promise<void>;
  providerConfigs: Map<string, ProviderConfig>;
  updateProviderConfig: (provider: string, config: Partial<ProviderConfig>) => Promise<void>;
  isLoading: boolean;
}

const ProviderContext = createContext<ProviderContextType | null>(null);

interface ProviderProviderProps {
  children: ReactNode;
}

export function ProviderProvider({ children }: ProviderProviderProps) {
  const [activeProvider, setActiveProviderState] = useState<string>('kraken');
  const [providerConfigs, setProviderConfigs] = useState<Map<string, ProviderConfig>>(new Map());
  const [isLoading, setIsLoading] = useState(true);

  // Get available providers from adapter registry
  const availableProviders = getAvailableProviders();

  // Load provider configurations and preferences on mount
  useEffect(() => {
    const loadConfigurations = async () => {
      setIsLoading(true);

      try {
        const manager = getWebSocketManager();

        // Initialize default configurations for available providers
        const configs = new Map<string, ProviderConfig>();

        // Kraken config
        configs.set('kraken', {
          provider_name: 'kraken',
          enabled: true,
          endpoint: 'wss://ws.kraken.com/v2'
        });

        // HyperLiquid config
        configs.set('hyperliquid', {
          provider_name: 'hyperliquid',
          enabled: true,
          endpoint: 'wss://api.hyperliquid.xyz/ws',
          config_data: JSON.stringify({ testnet: false })
        });

        setProviderConfigs(configs);

        // Load saved provider preference from localStorage
        const savedProvider = localStorage.getItem('active_trading_provider');
        if (savedProvider && configs.has(savedProvider)) {
          setActiveProviderState(savedProvider);
        }

        // Apply configurations to WebSocket manager
        configs.forEach((config, provider) => {
          manager.setProviderConfig(provider, config);
        });
      } catch (error) {
        console.error('Failed to load provider configurations:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadConfigurations();
  }, []);

  const setActiveProvider = useCallback(async (provider: string) => {
    if (!availableProviders.includes(provider)) {
      throw new Error(`Provider ${provider} is not available`);
    }

    setActiveProviderState(provider);

    // Save preference to localStorage
    localStorage.setItem('active_trading_provider', provider);

    // Ensure the provider is connected
    const manager = getWebSocketManager();
    const status = manager.getStatus(provider);

    if (status !== 'connected') {
      try {
        await manager.connect(provider);
      } catch (error) {
        console.error(`Failed to connect to ${provider}:`, error);
      }
    }
  }, [availableProviders]);

  const updateProviderConfig = useCallback(async (
    provider: string,
    config: Partial<ProviderConfig>
  ) => {
    setProviderConfigs(prev => {
      const newConfigs = new Map(prev);
      const existingConfig = newConfigs.get(provider);

      const updatedConfig: ProviderConfig = {
        ...existingConfig,
        ...config,
        provider_name: provider
      } as ProviderConfig;

      newConfigs.set(provider, updatedConfig);

      // Update WebSocket manager
      const manager = getWebSocketManager();
      manager.setProviderConfig(provider, updatedConfig);

      return newConfigs;
    });
  }, []);

  return (
    <ProviderContext.Provider value={{
      activeProvider,
      availableProviders,
      setActiveProvider,
      providerConfigs,
      updateProviderConfig,
      isLoading
    }}>
      {children}
    </ProviderContext.Provider>
  );
}

export function useProviderContext(): ProviderContextType {
  const context = useContext(ProviderContext);

  if (!context) {
    throw new Error('useProviderContext must be used within a ProviderProvider');
  }

  return context;
}

/**
 * Hook for quick provider selection
 */
export function useProviderSelection() {
  const { activeProvider, availableProviders, setActiveProvider, isLoading } = useProviderContext();

  return {
    provider: activeProvider,
    providers: availableProviders,
    setProvider: setActiveProvider,
    isLoading
  };
}
