import type { NotificationProvider, NotificationMessage } from '../types';

const COLOR: Record<string, string> = {
  success: '00d66f',
  error: 'ff3b3b',
  warning: 'ffd700',
  info: '00e5ff',
};

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const teamsProvider: NotificationProvider = {
  id: 'teams',
  name: 'Microsoft Teams',
  configFields: [
    {
      key: 'webhook_url',
      label: 'Webhook URL',
      placeholder: 'https://outlook.office.com/webhook/...',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { webhook_url } = config;
    if (!webhook_url) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    const color = COLOR[message.type] || COLOR.info;

    // Adaptive Card format for Teams workflows/Power Automate webhooks
    const card = {
      type: 'message',
      attachments: [
        {
          contentType: 'application/vnd.microsoft.card.adaptive',
          contentUrl: null,
          content: {
            $schema: 'http://adaptivecards.io/schemas/adaptive-card.json',
            type: 'AdaptiveCard',
            version: '1.4',
            body: [
              {
                type: 'TextBlock',
                text: `${emoji} ${message.title}`,
                weight: 'Bolder',
                size: 'Medium',
                color: message.type === 'error' ? 'Attention' : message.type === 'warning' ? 'Warning' : 'Good',
              },
              {
                type: 'TextBlock',
                text: message.body,
                wrap: true,
              },
              ...(message.metadata
                ? Object.entries(message.metadata).map(([key, value]) => ({
                    type: 'FactSet' as const,
                    facts: [{ title: key, value }],
                  }))
                : []),
            ],
          },
        },
      ],
    };

    const response = await fetch(webhook_url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(card),
    });
    return response.ok;
  },
};
