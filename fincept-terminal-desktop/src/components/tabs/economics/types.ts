// Economics Tab Types

export interface DataPoint {
  date: string;
  value: number;
}

export interface IndicatorData {
  indicator: string;
  country: string;
  data: DataPoint[];
  metadata?: {
    indicator_name?: string;
    country_name?: string;
    source?: string;
  };
}

export type DataSource = 'worldbank' | 'bis' | 'imf' | 'fred' | 'oecd' | 'wto' | 'cftc' | 'eia' | 'adb' | 'fed' | 'bls' | 'unesco' | 'fiscaldata' | 'bea' | 'fincept';

export interface DataSourceConfig {
  id: DataSource;
  name: string;        // Short name for buttons (e.g., "BIS", "IMF")
  fullName: string;    // Full name for display (e.g., "Bank for International Settlements")
  description: string; // Brief description
  color: string;
  scriptName: string;
  requiresApiKey?: boolean;
  apiKeyName?: string;
}

export interface Indicator {
  id: string;
  name: string;
  category: string;
  code?: string; // For CFTC markets
}

export interface Country {
  code: string;
  name: string;
  bis?: string;
  imf?: string;
  fred?: string;
  oecd?: string;
  wto?: string;
  adb?: string; // ADB uses 3-letter codes like PHI, SGP, JPN
}

export interface SaveNotification {
  show: boolean;
  path: string;
  type: 'image' | 'csv';
}

export interface ChartStats {
  latest: number;
  change: number;
  min: number;
  max: number;
  avg: number;
  count: number;
}
