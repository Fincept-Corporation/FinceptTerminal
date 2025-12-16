// Tab Configuration Service
import { TabConfiguration, DEFAULT_TAB_CONFIG, DEFAULT_TABS, TabDefinition } from '@/types/tabConfig';

const STORAGE_KEY = 'terminal-tab-configuration';

class TabConfigService {
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
    } catch (error) {
      console.error('Failed to save tab configuration:', error);
    }
  }

  resetToDefault(): void {
    localStorage.removeItem(STORAGE_KEY);
  }

  getAllTabs(): TabDefinition[] {
    return DEFAULT_TABS;
  }

  getTabById(id: string): TabDefinition | undefined {
    return DEFAULT_TABS.find(tab => tab.id === id);
  }
}

export const tabConfigService = new TabConfigService();
