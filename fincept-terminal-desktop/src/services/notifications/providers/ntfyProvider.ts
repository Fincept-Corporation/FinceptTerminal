import type { NotificationProvider, NotificationMessage } from '../types';

const PRIORITY: Record<string, string> = {
  success: '2',  // low
  info: '3',     // default
  warning: '4',  // high
  error: '5',    // urgent
};

const TAGS: Record<string, string> = {
  success: 'white_check_mark',
  error: 'x',
  warning: 'warning',
  info: 'information_source',
};

export const ntfyProvider: NotificationProvider = {
  id: 'ntfy',
  name: 'Ntfy',
  configFields: [
    {
      key: 'server_url',
      label: 'Server URL',
      placeholder: 'https://ntfy.sh (default) or your self-hosted URL',
      type: 'text',
      required: false,
    },
    {
      key: 'topic',
      label: 'Topic',
      placeholder: 'e.g. fincept-alerts',
      type: 'text',
      required: true,
    },
    {
      key: 'access_token',
      label: 'Access Token (optional)',
      placeholder: 'Only if topic requires authentication',
      type: 'password',
      required: false,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { topic, access_token } = config;
    if (!topic) return false;

    const server = config.server_url?.trim() || 'https://ntfy.sh';
    const url = `${server.replace(/\/$/, '')}/${topic}`;

    const headers: Record<string, string> = {
      'Title': message.title,
      'Priority': PRIORITY[message.type] || '3',
      'Tags': TAGS[message.type] || 'information_source',
    };

    if (access_token) {
      headers['Authorization'] = `Bearer ${access_token}`;
    }

    const response = await fetch(url, {
      method: 'POST',
      headers,
      body: message.body,
    });
    return response.ok;
  },
};
