import type { NotificationProvider, NotificationMessage } from '../types';

const PRIORITY: Record<string, number> = {
  success: -1,  // low
  info: 0,      // normal
  warning: 1,   // high
  error: 2,     // emergency (requires retry/expire for emergency)
};

export const pushoverProvider: NotificationProvider = {
  id: 'pushover',
  name: 'Pushover',
  configFields: [
    {
      key: 'api_token',
      label: 'API Token (Application)',
      placeholder: 'Create app at pushover.net/apps/build',
      type: 'password',
      required: true,
    },
    {
      key: 'user_key',
      label: 'User Key',
      placeholder: 'Your Pushover user key',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { api_token, user_key } = config;
    if (!api_token || !user_key) return false;

    const priority = PRIORITY[message.type] ?? 0;

    const body: Record<string, string | number> = {
      token: api_token,
      user: user_key,
      title: message.title,
      message: message.body,
      priority,
    };

    // Emergency priority requires retry and expire params
    if (priority === 2) {
      body.retry = 60;
      body.expire = 300;
    }

    const response = await fetch('https://api.pushover.net/1/messages.json', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    return response.ok;
  },
};
