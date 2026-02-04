import type { NotificationProvider, NotificationMessage, NotificationFile } from '../types';

const EMOJI: Record<string, string> = {
  success: '\u2705',
  error: '\u274C',
  warning: '\u26A0\uFE0F',
  info: '\u2139\uFE0F',
};

export const telegramProvider: NotificationProvider = {
  id: 'telegram',
  name: 'Telegram',
  configFields: [
    {
      key: 'bot_token',
      label: 'Bot Token',
      placeholder: 'e.g. 123456:ABC-DEF1234ghIkl-zyx57W2v1u123ew11',
      type: 'password',
      required: true,
    },
    {
      key: 'chat_id',
      label: 'Chat ID',
      placeholder: 'Auto-detected or enter manually',
      type: 'text',
      required: true,
      autoDetectLabel: 'DETECT CHAT ID',
      autoDetectHint: 'Send any message to your bot first, then click detect.',
      autoDetect: async (config: Record<string, string>) => {
        const token = config.bot_token;
        if (!token) return null;

        const base = `https://api.telegram.org/bot${token}`;

        // Step 1: Remove any existing webhook so getUpdates works
        await fetch(`${base}/deleteWebhook`);

        // Step 2: Fetch recent updates (no offset, limit 100)
        const res = await fetch(`${base}/getUpdates?limit=100&timeout=1`);
        if (!res.ok) return null;

        const data = await res.json();
        if (!data.ok || !data.result || data.result.length === 0) return null;

        // Step 3: Extract chat from any update type (most recent first)
        for (let i = data.result.length - 1; i >= 0; i--) {
          const update = data.result[i];
          const chat =
            update.message?.chat ||
            update.edited_message?.chat ||
            update.channel_post?.chat ||
            update.edited_channel_post?.chat ||
            update.my_chat_member?.chat ||
            update.chat_member?.chat ||
            update.callback_query?.message?.chat;
          if (chat?.id) {
            const name = chat.title || chat.first_name || chat.username || String(chat.id);
            return { value: String(chat.id), label: name };
          }
        }
        return null;
      },
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { bot_token, chat_id } = config;
    if (!bot_token || !chat_id) return false;

    const emoji = EMOJI[message.type] || EMOJI.info;
    let text = `${emoji} *${message.title}*\n${message.body}`;

    if (message.metadata) {
      const metaLines = Object.entries(message.metadata)
        .map(([k, v]) => `  _${k}_: ${v}`)
        .join('\n');
      if (metaLines) text += `\n\n${metaLines}`;
    }

    const url = `https://api.telegram.org/bot${bot_token}/sendMessage`;
    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ chat_id, text, parse_mode: 'Markdown' }),
    });
    return response.ok;
  },

  async sendFile(config: Record<string, string>, file: NotificationFile): Promise<boolean> {
    const { bot_token, chat_id } = config;
    if (!bot_token || !chat_id) return false;

    const url = `https://api.telegram.org/bot${bot_token}/sendDocument`;

    // Build multipart/form-data body manually because Tauri webview's
    // FormData + File/Blob doesn't transmit binary data correctly.
    const boundary = '----FinceptBoundary' + Date.now().toString(36);
    const fileBytes = new Uint8Array(await file.content.arrayBuffer());

    const textEncoder = new TextEncoder();
    const parts: Uint8Array[] = [];

    // chat_id field
    parts.push(textEncoder.encode(
      `--${boundary}\r\nContent-Disposition: form-data; name="chat_id"\r\n\r\n${chat_id}\r\n`
    ));

    // caption field
    if (file.caption) {
      parts.push(textEncoder.encode(
        `--${boundary}\r\nContent-Disposition: form-data; name="caption"\r\n\r\n${file.caption.slice(0, 1024)}\r\n`
      ));
      parts.push(textEncoder.encode(
        `--${boundary}\r\nContent-Disposition: form-data; name="parse_mode"\r\n\r\nMarkdown\r\n`
      ));
    }

    // document field (binary file)
    const fileHeader = textEncoder.encode(
      `--${boundary}\r\nContent-Disposition: form-data; name="document"; filename="${file.filename}"\r\nContent-Type: ${file.content.type || 'application/octet-stream'}\r\n\r\n`
    );
    parts.push(fileHeader);
    parts.push(fileBytes);
    parts.push(textEncoder.encode('\r\n'));

    // closing boundary
    parts.push(textEncoder.encode(`--${boundary}--\r\n`));

    // Concatenate all parts into a single Uint8Array
    const totalLength = parts.reduce((sum, p) => sum + p.length, 0);
    const body = new Uint8Array(totalLength);
    let offset = 0;
    for (const part of parts) {
      body.set(part, offset);
      offset += part.length;
    }

    const response = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': `multipart/form-data; boundary=${boundary}` },
      body: body,
    });

    if (!response.ok) {
      const errBody = await response.text().catch(() => '');
      console.error(`[Telegram] sendDocument failed (${response.status}):`, errBody);
    }
    return response.ok;
  },
};
