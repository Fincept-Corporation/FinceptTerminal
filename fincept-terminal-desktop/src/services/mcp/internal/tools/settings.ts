import { InternalTool } from '../types';

const THEMES = ['fincept-classic', 'matrix-green', 'blue-terminal', 'amber-retro', 'midnight-purple', 'crimson-dark'];
const LANGUAGES = ['en', 'es', 'zh', 'ja', 'fr', 'de', 'ko', 'hi', 'id', 'it', 'pt', 'ru', 'tr', 'uk', 'vi', 'ar', 'bn', 'pl', 'ro', 'th'];

export const settingsTools: InternalTool[] = [
  {
    name: 'set_theme',
    description: 'Change the terminal color theme',
    inputSchema: {
      type: 'object',
      properties: {
        theme: {
          type: 'string',
          description: `Theme to apply. Available: ${THEMES.join(', ')}`,
          enum: THEMES,
        },
      },
      required: ['theme'],
    },
    handler: async (args, contexts) => {
      if (!contexts.setTheme) {
        return { success: false, error: 'Theme context not available' };
      }
      contexts.setTheme(args.theme);
      return { success: true, message: `Theme changed to ${args.theme}` };
    },
  },
  {
    name: 'get_theme',
    description: 'Get the current terminal theme',
    inputSchema: {
      type: 'object',
      properties: {},
    },
    handler: async (_args, contexts) => {
      if (!contexts.getTheme) {
        return { success: false, error: 'Theme context not available' };
      }
      return { success: true, data: { theme: contexts.getTheme() } };
    },
  },
  {
    name: 'set_language',
    description: 'Change the terminal display language',
    inputSchema: {
      type: 'object',
      properties: {
        language: {
          type: 'string',
          description: `Language code. Available: ${LANGUAGES.join(', ')}`,
          enum: LANGUAGES,
        },
      },
      required: ['language'],
    },
    handler: async (args, contexts) => {
      if (!contexts.setLanguage) {
        return { success: false, error: 'Language context not available' };
      }
      contexts.setLanguage(args.language);
      return { success: true, message: `Language changed to ${args.language}` };
    },
  },
  {
    name: 'get_settings',
    description: 'Get current terminal settings (theme, language, etc.)',
    inputSchema: {
      type: 'object',
      properties: {
        key: { type: 'string', description: 'Specific setting key to retrieve. Omit to get all settings.' },
      },
    },
    handler: async (args, contexts) => {
      if (args.key && contexts.getSetting) {
        const value = await contexts.getSetting(args.key);
        return { success: true, data: { [args.key]: value } };
      }
      // Return what we can gather
      const data: any = {};
      if (contexts.getTheme) data.theme = contexts.getTheme();
      if (contexts.getLanguage) data.language = contexts.getLanguage();
      return { success: true, data };
    },
  },
  {
    name: 'set_setting',
    description: 'Set a terminal setting value',
    inputSchema: {
      type: 'object',
      properties: {
        key: { type: 'string', description: 'Setting key' },
        value: { type: 'string', description: 'Setting value (as string)' },
      },
      required: ['key', 'value'],
    },
    handler: async (args, contexts) => {
      if (!contexts.setSetting) {
        return { success: false, error: 'Settings context not available' };
      }
      await contexts.setSetting(args.key, args.value);
      return { success: true, message: `Setting "${args.key}" updated` };
    },
  },
];
