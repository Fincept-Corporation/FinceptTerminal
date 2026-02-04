import type { NotificationProvider } from './types';
// Tier 1 — Most popular
import { telegramProvider } from './providers/telegramProvider';
import { discordProvider } from './providers/discordProvider';
import { whatsappProvider } from './providers/whatsappProvider';
import { slackProvider } from './providers/slackProvider';
import { pushoverProvider } from './providers/pushoverProvider';
import { ntfyProvider } from './providers/ntfyProvider';
import { emailProvider } from './providers/emailProvider';
// Tier 2 — Power user
import { teamsProvider } from './providers/teamsProvider';
import { pushbulletProvider } from './providers/pushbulletProvider';
import { iftttProvider } from './providers/iftttProvider';
import { gotifyProvider } from './providers/gotifyProvider';
// Tier 3 — Specialized
import { pagerdutyProvider } from './providers/pagerdutyProvider';
import { opsgenieProvider } from './providers/opsgenieProvider';
import { twilioProvider } from './providers/twilioProvider';
import { lineProvider } from './providers/lineProvider';

const providers: Map<string, NotificationProvider> = new Map();

// Register built-in providers
const builtInProviders: NotificationProvider[] = [
  telegramProvider,
  discordProvider,
  whatsappProvider,
  slackProvider,
  pushoverProvider,
  ntfyProvider,
  emailProvider,
  teamsProvider,
  pushbulletProvider,
  iftttProvider,
  gotifyProvider,
  pagerdutyProvider,
  opsgenieProvider,
  twilioProvider,
  lineProvider,
];

for (const p of builtInProviders) {
  providers.set(p.id, p);
}

export function getProvider(id: string): NotificationProvider | undefined {
  return providers.get(id);
}

export function getAllProviders(): NotificationProvider[] {
  return Array.from(providers.values());
}

export function registerProvider(provider: NotificationProvider): void {
  providers.set(provider.id, provider);
}
