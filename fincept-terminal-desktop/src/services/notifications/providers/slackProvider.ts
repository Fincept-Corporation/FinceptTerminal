import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: ':white_check_mark:',
  error: ':x:',
  warning: ':warning:',
  info: ':information_source:',
};

const COLOR: Record<string, string> = {
  success: '#00d66f',
  error: '#ff3b3b',
  warning: '#ffd700',
  info: '#00e5ff',
};

export const slackProvider: NotificationProvider = {
  id: 'slack',
  name: 'Slack',
  configFields: [
    {
      key: 'webhook_url',
      label: 'Webhook URL',
      placeholder: 'https://hooks.slack.com/services/T.../B.../...',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { webhook_url } = config;
    if (!webhook_url) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    const color = COLOR[message.type] || COLOR.info;

    const fields = message.metadata
      ? Object.entries(message.metadata).map(([title, value]) => ({ title, value, short: true }))
      : [];

    const payload = {
      username: 'Fincept Terminal',
      attachments: [
        {
          color,
          pretext: `${emoji} *${message.title}*`,
          text: message.body,
          fields,
          footer: 'Fincept Terminal',
          ts: Math.floor(new Date(message.timestamp).getTime() / 1000),
        },
      ],
    };

    const response = await fetch(webhook_url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
    return response.ok;
  },
};
