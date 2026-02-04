import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const lineProvider: NotificationProvider = {
  id: 'line',
  name: 'LINE Notify',
  configFields: [
    {
      key: 'access_token',
      label: 'Access Token',
      placeholder: 'From notify-bot.line.me > My Page',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { access_token } = config;
    if (!access_token) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    const text = `\n${emoji} ${message.title}\n${message.body}`;

    const formData = new URLSearchParams({ message: text });

    const response = await fetch('https://notify-api.line.me/api/notify', {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${access_token}`,
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: formData.toString(),
    });
    return response.ok;
  },
};
