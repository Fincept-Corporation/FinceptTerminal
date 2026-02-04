import type { NotificationProvider, NotificationMessage } from '../types';

export const iftttProvider: NotificationProvider = {
  id: 'ifttt',
  name: 'IFTTT',
  configFields: [
    {
      key: 'webhook_key',
      label: 'Webhook Key',
      placeholder: 'From ifttt.com/services/maker_webhooks/settings',
      type: 'password',
      required: true,
    },
    {
      key: 'event_name',
      label: 'Event Name',
      placeholder: 'e.g. fincept_alert',
      type: 'text',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { webhook_key, event_name } = config;
    if (!webhook_key || !event_name) return false;

    const url = `https://maker.ifttt.com/trigger/${encodeURIComponent(event_name)}/with/key/${webhook_key}`;

    // IFTTT webhooks accept value1, value2, value3
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        value1: message.title,
        value2: message.body,
        value3: message.type,
      }),
    });
    return response.ok;
  },
};
