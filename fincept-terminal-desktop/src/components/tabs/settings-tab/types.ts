// Settings Tab Types

export type SettingsSection =
  | 'credentials'
  | 'polymarket'
  | 'terminal'
  | 'terminalConfig'
  | 'llm'
  | 'dataConnections'
  | 'backtesting'
  | 'stockSymbols'
  | 'language'
  | 'storage';

export interface SettingsMessage {
  type: 'success' | 'error';
  text: string;
}

export interface WSProviderConfig {
  id: number;
  provider_name: string;
  enabled: boolean;
  api_key: string | null;
  api_secret: string | null;
  endpoint: string | null;
  config_data: string | null;
}

export interface SettingsColors {
  background: string;
  panel: string;
  primary: string;
  secondary: string;
  text: string;
  textMuted: string;
  success: string;
  alert: string;
  warning: string;
  accent: string;
}
