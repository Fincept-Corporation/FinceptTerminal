export type NotificationEventType = 'success' | 'error' | 'warning' | 'info';

export interface NotificationMessage {
  type: NotificationEventType;
  title: string;
  body: string;
  timestamp: string;
  metadata?: Record<string, string>;
}

export interface ProviderConfigField {
  key: string;
  label: string;
  placeholder: string;
  type: 'text' | 'password';
  required: boolean;
  /** If set, this field can be auto-detected. The function receives the current config and returns the detected value. */
  autoDetect?: (config: Record<string, string>) => Promise<{ value: string; label: string } | null>;
  autoDetectLabel?: string;
  autoDetectHint?: string;
}

export interface NotificationFile {
  filename: string;
  content: Blob;
  caption?: string;
}

export interface NotificationProvider {
  id: string;
  name: string;
  configFields: ProviderConfigField[];
  send(config: Record<string, string>, message: NotificationMessage): Promise<boolean>;
  sendFile?(config: Record<string, string>, file: NotificationFile): Promise<boolean>;
}

export interface NotificationProviderConfig {
  id: string;
  name: string;
  enabled: boolean;
  config: Record<string, string>;
}

export interface NotificationSettings {
  providers: NotificationProviderConfig[];
  eventFilters: Record<NotificationEventType, boolean>;
}
