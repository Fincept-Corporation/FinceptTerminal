import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const twilioProvider: NotificationProvider = {
  id: 'twilio',
  name: 'Twilio SMS',
  configFields: [
    {
      key: 'account_sid',
      label: 'Account SID',
      placeholder: 'ACxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx',
      type: 'text',
      required: true,
    },
    {
      key: 'auth_token',
      label: 'Auth Token',
      placeholder: 'Your Twilio auth token',
      type: 'password',
      required: true,
    },
    {
      key: 'from_number',
      label: 'From Number',
      placeholder: '+1234567890 (Twilio number)',
      type: 'text',
      required: true,
    },
    {
      key: 'to_number',
      label: 'To Number',
      placeholder: '+1987654321 (recipient)',
      type: 'text',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { account_sid, auth_token, from_number, to_number } = config;
    if (!account_sid || !auth_token || !from_number || !to_number) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    const body = `${emoji} ${message.title}\n${message.body}`;

    const url = `https://api.twilio.com/2010-04-01/Accounts/${account_sid}/Messages.json`;
    const credentials = btoa(`${account_sid}:${auth_token}`);

    const formData = new URLSearchParams({
      From: from_number,
      To: to_number,
      Body: body,
    });

    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Authorization': `Basic ${credentials}`,
        'Content-Type': 'application/x-www-form-urlencoded',
      },
      body: formData.toString(),
    });
    return response.ok;
  },
};
