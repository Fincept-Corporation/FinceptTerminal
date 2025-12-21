// Tab Configuration Service
import { TabConfiguration, DEFAULT_TAB_CONFIG, DEFAULT_TABS, TabDefinition } from '@/types/tabConfig';

const STORAGE_KEY = 'terminal-tab-configuration';
const STORAGE_EVENT = 'tab-config-changed';

class TabConfigService {
  private listeners: Set<(config: TabConfiguration) => void> = new Set();

  getConfiguration(): TabConfiguration {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        return JSON.parse(stored);
      }
    } catch (error) {
      console.error('Failed to load tab configuration:', error);
    }
    return DEFAULT_TAB_CONFIG;
  }

  saveConfiguration(config: TabConfiguration): void {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
      // Notify all listeners
      this.notifyListeners(config);
      // Dispatch storage event for cross-window sync
      window.dispatchEvent(new CustomEvent(STORAGE_EVENT, { detail: config }));
    } catch (error) {
      console.error('Failed to save tab configuration:', error);
    }
  }

  resetToDefault(): void {
    localStorage.removeItem(STORAGE_KEY);
    this.notifyListeners(DEFAULT_TAB_CONFIG);
    window.dispatchEvent(new CustomEvent(STORAGE_EVENT, { detail: DEFAULT_TAB_CONFIG }));
  }

  subscribe(listener: (config: TabConfiguration) => void): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  private notifyListeners(config: TabConfiguration): void {
    this.listeners.forEach(listener => listener(config));
  }

  getAllTabs(): TabDefinition[] {
    return DEFAULT_TABS;
  }

  getTabById(id: string): TabDefinition | undefined {
    return DEFAULT_TABS.find(tab => tab.id === id);
  }
}

export const tabConfigService = new TabConfigService();
