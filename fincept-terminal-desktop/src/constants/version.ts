// Application version constants
// This file is the single source of truth for the app version displayed in the UI
// The build version comes from tauri.conf.json

export const APP_VERSION = '3.0.0';
export const APP_NAME = 'Fincept Terminal';
export const APP_BUILD_DATE = new Date().toISOString().split('T')[0];

// Full version string with build date
export const getFullVersionString = (): string => {
  return `${APP_NAME} v${APP_VERSION}`;
};

// Version info object for about screens
export const VERSION_INFO = {
  version: APP_VERSION,
  name: APP_NAME,
  buildDate: APP_BUILD_DATE,
  copyright: `Copyright © ${new Date().getFullYear()} Fincept Corporation. All rights reserved.`,
  description: 'Professional-grade financial analysis platform with AI-powered insights',
};
