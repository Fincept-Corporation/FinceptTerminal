import type { NotificationProvider, NotificationMessage } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const whatsappProvider: NotificationProvider = {
  id: 'whatsapp',
  name: 'WhatsApp',
  configFields: [
    {
      key: 'phone_number_id',
      label: 'Phone Number ID',
      placeholder: 'e.g. 106540352242922',
      type: 'text',
      required: true,
    },
    {
      key: 'access_token',
      label: 'Access Token',
      placeholder: 'Permanent token from Meta Business',
      type: 'password',
      required: true,
    },
    {
      key: 'recipient_number',
      label: 'Recipient Phone Number',
      placeholder: 'e.g. 919876543210 (with country code, no +)',
      type: 'text',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { phone_number_id, access_token, recipient_number } = config;
    if (!phone_number_id || !access_token || !recipient_number) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    let text = `${emoji} *${message.title}*\n${message.body}`;

    if (message.metadata) {
      const metaLines = Object.entries(message.metadata)
        .map(([k, v]) => `_${k}_: ${v}`)
        .join('\n');
      if (metaLines) text += `\n\n${metaLines}`;
    }

    const url = `https://graph.facebook.com/v21.0/${phone_number_id}/messages`;
    const response = await fetch(url, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': `Bearer ${access_token}`,
      },
      body: JSON.stringify({
        messaging_product: 'whatsapp',
        to: recipient_number,
        type: 'text',
        text: { body: text },
      }),
    });
    return response.ok;
  },
};
