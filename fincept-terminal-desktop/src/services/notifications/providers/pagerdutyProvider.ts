import type { NotificationProvider, NotificationMessage } from '../types';

const SEVERITY: Record<string, string> = {
  success: 'info',
  info: 'info',
  warning: 'warning',
  error: 'critical',
};

export const pagerdutyProvider: NotificationProvider = {
  id: 'pagerduty',
  name: 'PagerDuty',
  configFields: [
    {
      key: 'routing_key',
      label: 'Integration / Routing Key',
      placeholder: 'Events API v2 integration key',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { routing_key } = config;
    if (!routing_key) return false;

    const severity = SEVERITY[message.type] || 'info';

    const payload = {
      routing_key,
      event_action: 'trigger',
      payload: {
        summary: `${message.title}: ${message.body}`,
        source: 'Fincept Terminal',
        severity,
        timestamp: message.timestamp,
        custom_details: message.metadata || {},
      },
    };

    const response = await fetch('https://events.pagerduty.com/v2/enqueue', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    return response.ok;
  },
};
