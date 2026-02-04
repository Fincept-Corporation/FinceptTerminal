import type { NotificationProvider, NotificationMessage } from '../types';

const PRIORITY: Record<string, number> = {
  success: 2,
  info: 4,
  warning: 7,
  error: 9,
};

export const gotifyProvider: NotificationProvider = {
  id: 'gotify',
  name: 'Gotify',
  configFields: [
    {
      key: 'server_url',
      label: 'Server URL',
      placeholder: 'e.g. https://gotify.yourdomain.com',
      type: 'text',
      required: true,
    },
    {
      key: 'app_token',
      label: 'Application Token',
      placeholder: 'From Gotify > Apps > Create Application',
      type: 'password',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { server_url, app_token } = config;
    if (!server_url || !app_token) return false;

    const url = `${server_url.replace(/\/$/, '')}/message?token=${app_token}`;

    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        title: message.title,
        message: message.body,
        priority: PRIORITY[message.type] ?? 4,
      }),
    });
    return response.ok;
  },
};
