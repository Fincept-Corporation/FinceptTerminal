// Tab Configuration Service
import { TabConfiguration, DEFAULT_TAB_CONFIG, DEFAULT_TABS, TabDefinition } from '@/types/tabConfig';
import { saveSetting, getSetting } from './sqliteService';

const STORAGE_KEY = 'terminal-tab-configuration';
const STORAGE_EVENT = 'tab-config-changed';

class TabConfigService {
  private listeners: Set<(config: TabConfiguration) => void> = new Set();

  async getConfiguration(): Promise<TabConfiguration> {
    try {
      const stored = await getSetting(STORAGE_KEY);
      if (stored) {
        return JSON.parse(stored);
      }
    } catch (error) {
      console.error('Failed to load tab configuration:', error);
    }
    return DEFAULT_TAB_CONFIG;
  }

  async saveConfiguration(config: TabConfiguration): Promise<void> {
    try {
      await saveSetting(STORAGE_KEY, JSON.stringify(config), 'tab_config');
      // Notify all listeners
      this.notifyListeners(config);
      // Dispatch storage event for cross-window sync
      window.dispatchEvent(new CustomEvent(STORAGE_EVENT, { detail: config }));
    } catch (error) {
      console.error('Failed to save tab configuration:', error);
    }
  }

  async resetToDefault(): Promise<void> {
    await saveSetting(STORAGE_KEY, '', 'tab_config');
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
