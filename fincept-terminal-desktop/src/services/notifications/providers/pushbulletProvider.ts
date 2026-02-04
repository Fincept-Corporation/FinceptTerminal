import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const pushbulletProvider: NotificationProvider = {
  id: 'pushbullet',
  name: 'Pushbullet',
  configFields: [
    {
      key: 'access_token',
      label: 'Access Token',
      placeholder: 'From pushbullet.com > Settings > Access Tokens',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { access_token } = config;
    if (!access_token) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;

    const response = await fetch('https://api.pushbullet.com/v2/pushes', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Access-Token': access_token,
      },
      body: JSON.stringify({
        type: 'note',
        title: `${emoji} ${message.title}`,
        body: message.body,
      }),
    });
    return response.ok;
  },
};
