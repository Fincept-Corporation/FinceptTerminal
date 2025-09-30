// Data Source Types and Interfaces

export type DataSourceCategory = 'database' | 'api' | 'file' | 'streaming' | 'cloud' | 'timeseries' | 'market-data';

export interface DataSourceConfig {
  id: string;
  name: string;
  type: string;
  category: DataSourceCategory;
  icon: string;
  color: string;
  description: string;
  fields: DataSourceField[];
  testable: boolean;
  requiresAuth: boolean;
}

export interface DataSourceField {
  name: string;
  label: string;
  type: 'text' | 'password' | 'number' | 'url' | 'select' | 'textarea' | 'checkbox' | 'file';
  placeholder?: string;
  required: boolean;
  defaultValue?: any;
  options?: { label: string; value: string }[];
  validation?: {
    pattern?: string;
    min?: number;
    max?: number;
    message?: string;
  };
}

export interface DataSourceConnection {
  id: string;
  name: string;
  type: string;
  category: DataSourceCategory;
  config: Record<string, any>;
  status: 'connected' | 'disconnected' | 'error' | 'testing';
  createdAt: string;
  updatedAt: string;
  lastTested?: string;
  errorMessage?: string;
}

export interface TestConnectionResult {
  success: boolean;
  message: string;
  metadata?: Record<string, any>;
}
