import type { NotificationProvider, NotificationMessage } from '../types';

const PRIORITY: Record<string, string> = {
  success: 'P5',
  info: 'P4',
  warning: 'P3',
  error: 'P1',
};

export const opsgenieProvider: NotificationProvider = {
  id: 'opsgenie',
  name: 'Opsgenie',
  configFields: [
    {
      key: 'api_key',
      label: 'API Key',
      placeholder: 'Opsgenie integration API key',
      type: 'password',
      required: true,
    },
    {
      key: 'api_url',
      label: 'API URL (optional)',
      placeholder: 'https://api.opsgenie.com (default) or EU: https://api.eu.opsgenie.com',
      type: 'text',
      required: false,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { api_key } = config;
    if (!api_key) return false;

    const baseUrl = config.api_url?.trim() || 'https://api.opsgenie.com';
    const url = `${baseUrl.replace(/\/$/, '')}/v2/alerts`;

    const payload = {
      message: message.title,
      description: message.body,
      priority: PRIORITY[message.type] || 'P4',
      source: 'Fincept Terminal',
      details: message.metadata || {},
    };

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `GenieKey ${api_key}`,
      },
      body: JSON.stringify(payload),
    });
    return response.ok;
  },
};
