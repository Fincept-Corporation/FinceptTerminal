import { saveSetting, getSetting } from '@/services/core/sqliteService';
import { getProvider, getAllProviders } from './providerRegistry';
import type {
  NotificationMessage,
  NotificationFile,
  NotificationProviderConfig,
  NotificationEventType,
  NotificationSettings,
} from './types';

const SETTINGS_CATEGORY = 'notifications';
const EVENT_TYPES: NotificationEventType[] = ['success', 'error', 'warning', 'info'];

class NotificationService {
  private settings: NotificationSettings = {
    providers: [],
    eventFilters: { success: true, error: true, warning: true, info: true },
  };
  private initialized = false;

  async initialize(): Promise<void> {
    if (this.initialized) return;
    try {
      await this.loadSettings();
      this.initialized = true;
      console.log('[NotificationService] Initialized. Providers:', this.settings.providers.map(p => `${p.id}:${p.enabled}`).join(', '));
      console.log('[NotificationService] Event filters:', JSON.stringify(this.settings.eventFilters));
    } catch (err) {
      console.warn('[NotificationService] Failed to initialize:', err);
    }
  }

  async loadSettings(): Promise<NotificationSettings> {
    // Load event filters
    for (const type of EVENT_TYPES) {
      const val = await getSetting(`notification_event_filter_${type}`);
      this.settings.eventFilters[type] = val !== 'false'; // default true
    }

    // Load provider configs
    this.settings.providers = [];
    for (const provider of getAllProviders()) {
      const enabled = await getSetting(`notification_provider_${provider.id}_enabled`);
      const config: Record<string, string> = {};
      for (const field of provider.configFields) {
        const val = await getSetting(`notification_provider_${provider.id}_${field.key}`);
        config[field.key] = val || '';
      }
      this.settings.providers.push({
        id: provider.id,
        name: provider.name,
        enabled: enabled === 'true',
        config,
      });
    }

    return this.settings;
  }

  getSettings(): NotificationSettings {
    return this.settings;
  }

  isInitialized(): boolean {
    return this.initialized;
  }

  async saveProviderEnabled(providerId: string, enabled: boolean): Promise<void> {
    await saveSetting(`notification_provider_${providerId}_enabled`, String(enabled), SETTINGS_CATEGORY);
    const prov = this.settings.providers.find(p => p.id === providerId);
    if (prov) prov.enabled = enabled;
  }

  async saveProviderConfig(providerId: string, key: string, value: string): Promise<void> {
    await saveSetting(`notification_provider_${providerId}_${key}`, value, SETTINGS_CATEGORY);
    const prov = this.settings.providers.find(p => p.id === providerId);
    if (prov) prov.config[key] = value;
  }

  async saveEventFilter(type: NotificationEventType, enabled: boolean): Promise<void> {
    await saveSetting(`notification_event_filter_${type}`, String(enabled), SETTINGS_CATEGORY);
    this.settings.eventFilters[type] = enabled;
  }

  async notify(message: NotificationMessage): Promise<void> {
    if (!this.initialized) {
      console.warn('[NotificationService] Not initialized — skipping notification');
      return;
    }
    if (!this.settings.eventFilters[message.type]) {
      console.warn(`[NotificationService] Event type '${message.type}' is filtered out`);
      return;
    }

    const enabledProviders = this.settings.providers.filter(p => p.enabled);
    console.log(`[NotificationService] Sending '${message.type}' to ${enabledProviders.length} enabled provider(s):`,
      enabledProviders.map(p => p.id).join(', ') || '(none)');

    for (const provConfig of this.settings.providers) {
      if (!provConfig.enabled) continue;
      const provider = getProvider(provConfig.id);
      if (!provider) {
        console.warn(`[NotificationService] Provider '${provConfig.id}' not found in registry`);
        continue;
      }

      provider.send(provConfig.config, message)
        .then(ok => console.log(`[NotificationService] ${provConfig.id} send result:`, ok))
        .catch(err => console.warn(`[NotificationService] ${provConfig.id} send failed:`, err));
    }
  }

  async notifyWithFile(file: NotificationFile): Promise<void> {
    if (!this.initialized) {
      console.warn('[NotificationService] Not initialized — skipping file notification');
      return;
    }

    // Find the first enabled provider that supports sendFile
    const enabledProviders = this.settings.providers.filter(p => p.enabled);
    let sent = false;

    for (const provConfig of enabledProviders) {
      const provider = getProvider(provConfig.id);
      if (!provider?.sendFile) continue;

      console.log(`[NotificationService] Sending file '${file.filename}' via ${provConfig.id}`);
      try {
        const ok = await provider.sendFile(provConfig.config, file);
        console.log(`[NotificationService] ${provConfig.id} sendFile result:`, ok);
        if (ok) { sent = true; break; }
      } catch (err) {
        console.warn(`[NotificationService] ${provConfig.id} sendFile failed:`, err);
      }
    }

    if (!sent) {
      console.warn('[NotificationService] No provider could send the file');
    }
  }

  async testProvider(providerId: string, configOverride?: Record<string, string>): Promise<boolean> {
    const provider = getProvider(providerId);
    if (!provider) return false;

    const config = configOverride || this.settings.providers.find(p => p.id === providerId)?.config;
    if (!config) return false;

    const testMessage: NotificationMessage = {
      type: 'info',
      title: 'Fincept Terminal',
      body: 'Test notification — your provider is configured correctly!',
      timestamp: new Date().toISOString(),
    };

    return provider.send(config, testMessage);
  }
}

export const notificationService = new NotificationService();
