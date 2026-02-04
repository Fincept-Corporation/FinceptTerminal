import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

const COLOR: Record<string, number> = {
  success: 0x00d66f,
  error: 0xff3b3b,
  warning: 0xffd700,
  info: 0x00e5ff,
};

export const discordProvider: NotificationProvider = {
  id: 'discord',
  name: 'Discord',
  configFields: [
    {
      key: 'webhook_url',
      label: 'Webhook URL',
      placeholder: 'https://discord.com/api/webhooks/...',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { webhook_url } = config;
    if (!webhook_url) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    const color = COLOR[message.type] ?? COLOR.info;

    const embed: Record<string, unknown> = {
      title: `${emoji} ${message.title}`,
      description: message.body,
      color,
      timestamp: message.timestamp,
      footer: { text: 'Fincept Terminal' },
    };

    if (message.metadata) {
      embed.fields = Object.entries(message.metadata).map(([name, value]) => ({
        name,
        value,
        inline: true,
      }));
    }

    const response = await fetch(webhook_url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        username: 'Fincept Terminal',
        embeds: [embed],
      }),
    });
    // Discord returns 204 No Content on success
    return response.ok;
  },
};
