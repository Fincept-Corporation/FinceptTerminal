import type { NotificationProvider, NotificationMessage } from '../types';

export const emailProvider: NotificationProvider = {
  id: 'email',
  name: 'Email (SMTP)',
  configFields: [
    {
      key: 'smtp_host',
      label: 'SMTP Host',
      placeholder: 'e.g. smtp.gmail.com',
      type: 'text',
      required: true,
    },
    {
      key: 'smtp_port',
      label: 'SMTP Port',
      placeholder: 'e.g. 587',
      type: 'text',
      required: true,
    },
    {
      key: 'smtp_user',
      label: 'Username / Email',
      placeholder: 'your@email.com',
      type: 'text',
      required: true,
    },
    {
      key: 'smtp_pass',
      label: 'Password / App Password',
      placeholder: 'SMTP password or app-specific password',
      type: 'password',
      required: true,
    },
    {
      key: 'recipient',
      label: 'Recipient Email',
      placeholder: 'recipient@email.com',
      type: 'text',
      required: true,
    },
  ],
  async send(config: Record<string, string>, message: NotificationMessage): Promise<boolean> {
    const { smtp_host, smtp_port, smtp_user, smtp_pass, recipient } = config;
    if (!smtp_host || !smtp_port || !smtp_user || !smtp_pass || !recipient) return false;

    // SMTP requires a backend/Rust command since browsers cannot open raw TCP sockets.
    // This uses the Tauri invoke bridge to send email via the Rust backend.
    try {
      const { invoke } = await import('@tauri-apps/api/core');
      const result = await invoke<boolean>('send_smtp_email', {
        host: smtp_host,
        port: parseInt(smtp_port, 10),
        username: smtp_user,
        password: smtp_pass,
        to: recipient,
        subject: `[${message.type.toUpperCase()}] ${message.title}`,
        body: message.body,
      });
      return result;
    } catch (err) {
      // If the Rust command is not yet implemented, log and return false
      console.warn('[EmailProvider] send_smtp_email command not available:', err);
      return false;
    }
  },
};
